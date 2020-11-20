#include <stdint.h>

#include "lib.h"

/** lock for critical sections inside harness
 * during litmus test execution
 */
static lock_t __harness_lock;

static void go_cpus(int cpu, void* a);
static void run_thread(test_ctx_t* ctx, int cpu);
static void end_of_test(test_ctx_t* ctx);
static void start_of_test(test_ctx_t* ctx);
static void end_of_thread(test_ctx_t* ctx, int cpu);
static void start_of_thread(test_ctx_t* ctx, int cpu);
static void end_of_run(test_ctx_t* ctx, int cpu, int vcpu, run_idx_t i, run_count_t r);
static void start_of_run(test_ctx_t* ctx, int cpu, int vcpu, run_idx_t i, run_count_t r);

/* entry point */
void run_test(const litmus_test_t* cfg) {
  printf("\n");
  printf("Test %s:\n", cfg->name);

  /* create test context obj
   * make sure it's on the heap
   * if we're passing to another thread
   */
  test_ctx_t* ctx = ALLOC_ONE(test_ctx_t);

  /* use same seed for each test
   * this means we can re-run just 1 test from whole batch
   * and still get determinism (up to relaxation) */
  reset_seed();

  /* create the dynamic configuration (context) from the static information (cfg) */
  init_test_ctx(ctx, cfg, NUMBER_OF_RUNS);
  initialize_regions(&ctx->heap_memory);

  debug("done.  run the tests.\n");
  start_of_test(ctx);

  /* run it */
  trace("%s\n", "Running Tests ...");
  if (TRACE) {
    for(int i = 0; i < 4; i++) {
      printf("P%d\t\t\t", i);
    }
    printf("\n");
  }
  run_on_cpus((async_fn_t*)go_cpus, (void*)ctx);

  /* clean up and display results */
  end_of_test(ctx);
}

uint64_t* ctx_heap_var_va(test_ctx_t* ctx, uint64_t varidx, run_idx_t i) {
  return ctx->heap_vars[varidx].values[i];
}

uint64_t ctx_initial_heap_value(test_ctx_t* ctx, run_idx_t idx) {
  return ctx->heap_vars[idx].init_value;
}

static void go_cpus(int cpu, void* a) {
  test_ctx_t* ctx = (test_ctx_t*)a;
  start_of_thread(ctx, cpu);
  run_thread(ctx, cpu);
  end_of_thread(ctx, cpu);
}

static void reset_mair_attr7(test_ctx_t* ctx) {
  if (ctx->system_state->enable_mair) {
    uint64_t mair = read_sysreg(mair_el1);
    uint64_t bm = ~(BITMASK(8) << 56);
    write_sysreg((mair & bm) | (ctx->system_state->mair_attr7 << 56), mair_el1);
  }
}

/** reset the sysregister state after each run
 */
static void _init_sys_state(test_ctx_t* ctx) {
  reset_mair_attr7(ctx);
}

/** convert the top-level loop counter index into the
 * run offset into the tables
 */
static run_idx_t count_to_run_index(test_ctx_t* ctx, run_count_t i) {
  switch (LITMUS_SHUFFLE_TYPE) {
    case SHUF_NONE:
      return (run_idx_t)i;
    case SHUF_RAND:
      return ctx->shuffled_ixs[i];
    default:
      fail("! unknown LITMUS_SHUFFLE_TYPE: %d/%s\n", LITMUS_SHUFFLE_TYPE, shuff_type_to_str(LITMUS_SHUFFLE_TYPE));
      return 0;
  }
}

/* shuffle about the affinities,
 * giving threads new physical CPU assignments
 */
static void allocate_affinities(test_ctx_t* ctx) {
  if (LITMUS_AFF_TYPE != AFF_NONE) {
    shuffle((int*)ctx->affinity, sizeof(int), NO_CPUS);
    debug("set affinity = %Ad\n", ctx->affinity, NO_CPUS);
  }
}

/** run concretization and initialization for the runs of this batch
 */
static void allocate_data_for_batch(test_ctx_t* ctx, uint64_t vcpu, run_count_t batch_start_idx, run_count_t batch_end_idx) {
  debug("vCPU%d allocating test data for batch starting %ld\n", vcpu, batch_start_idx);

  if (vcpu == 0) {
    /* first we have to allocate new pagetables for the whole batch
    * cleaning up any previous ones as we do
    */
    if (ENABLE_PGTABLE) {
      for (run_count_t r = batch_start_idx; r < batch_end_idx; r++) {
        uint64_t asid = asid_from_run_count(r);
        /* cleanup the previous if it exists */
        if (ctx->ptables[asid] != NULL) {
          vmm_free_test_pgtable(ctx->ptables[asid]);
        }

        debug("vCPU%d alloc pgtable for ASID=%ld\n", vcpu, asid);
        ctx->ptables[asid] = vmm_alloc_new_test_pgtable();
      }
    }

    if (LITMUS_RUNNER_TYPE != RUNNER_ARRAY) {
      for (run_count_t r = batch_start_idx; r < batch_end_idx; r++) {
        run_idx_t i = count_to_run_index(ctx, r);

        if (LITMUS_RUNNER_TYPE == RUNNER_EPHEMERAL) {
          concretize_one(LITMUS_CONCRETIZATION_TYPE, ctx, ctx->cfg, ctx->concretization_st, i);
        }

        if (LITMUS_RUNNER_TYPE == RUNNER_SEMI_ARRAY || LITMUS_RUNNER_TYPE == RUNNER_EPHEMERAL) {
          write_init_state(ctx, ctx->cfg, i);
        }
      }
    }
  }
}

/** initialise the litmus_test_run datas to pass as arguments
 */
static void setup_run_data(test_ctx_t* ctx, uint64_t vcpu, run_count_t batch_start_idx, run_count_t batch_end_idx, litmus_test_run* runs) {
  uint64_t* heaps[ctx->cfg->no_heap_vars];
  uint64_t pas[ctx->cfg->no_heap_vars];
  uint64_t* regs[ctx->cfg->no_regs];

  /* we save the entry and descriptor for each level
   * so we can recall them in a test
   */
  uint64_t* ptes[ctx->cfg->no_heap_vars][4];
  uint64_t descs[ctx->cfg->no_heap_vars][4];

  /* which gets mangled into the linked form the test data expects */
  uint64_t** tt_entries[ctx->cfg->no_heap_vars];
  uint64_t* tt_descs[ctx->cfg->no_heap_vars];

  for (run_count_t r = batch_start_idx; r < batch_end_idx; r++) {
    run_idx_t i = count_to_run_index(ctx, r);
    for (var_idx_t v = 0; v < ctx->cfg->no_heap_vars; v++) {
      uint64_t* p = ctx_heap_var_va(ctx, v, i);
      heaps[v] = p;

      if (ENABLE_PGTABLE) {
        for (int lvl = 0; lvl < 4; lvl++) {
          ptes[v][lvl] = vmm_pte_at_level(ctx->ptables[asid_from_run_count(r)], (uint64_t)p, lvl);
          descs[v][lvl] = *ptes[v][lvl];
        }

        pas[v] = ctx_pa(ctx, i, (uint64_t)p);
      }
    }

    for (reg_idx_t reg = 0; reg < ctx->cfg->no_regs; reg++) {
      regs[reg] = &ctx->out_regs[reg][i];
    }

    /* turn arrays into actual pointer structures */
    for (var_idx_t v = 0; v < ctx->cfg->no_heap_vars; v++) {
      tt_descs[v] = &descs[v][0];
      tt_entries[v] = &ptes[v][0];
    }

    runs[r] = (litmus_test_run){
      .ctx = ctx,
      .i = (uint64_t)i,
      .va = heaps,
      .pa = pas,
      .out_reg = regs,
      .tt_entries = &tt_entries[0],
      .tt_descs = &tt_descs[0],
    };
  }
}

/** invalidate all the ASIDs being used for the next batch
 */
static void clean_tlb_for_batch(run_count_t batch_start_idx, run_count_t batch_end_idx) {
  dsb();

  for (run_count_t r = batch_start_idx; r < batch_end_idx; r++) {
    uint64_t asid = asid_from_run_count(r);
    tlbi_asid(asid);
  }

  dsb();
}

typedef struct {
  uint32_t* el0;
  uint32_t* el1_sp0;
  uint32_t* el1_spx;
} exception_handlers_refs_t;

static void set_new_sync_exception_handlers(test_ctx_t* ctx, uint64_t vcpu, exception_handlers_refs_t* handlers) {
  if (ctx->cfg->thread_sync_handlers) {
    if (ctx->cfg->thread_sync_handlers[vcpu][0] != NULL) {
      handlers->el0 = hotswap_exception(0x400, (uint32_t*)ctx->cfg->thread_sync_handlers[vcpu][0]);
    }
    if (ctx->cfg->thread_sync_handlers[vcpu][1] != NULL) {
      handlers->el1_sp0 = hotswap_exception(0x000, (uint32_t*)ctx->cfg->thread_sync_handlers[vcpu][1]);
      handlers->el1_spx = hotswap_exception(0x200, (uint32_t*)ctx->cfg->thread_sync_handlers[vcpu][1]);
    }
  }
}

static void restore_old_sync_exception_handlers(test_ctx_t* ctx, uint64_t vcpu, exception_handlers_refs_t* handlers) {
  if (ctx->cfg->thread_sync_handlers) {
    if (handlers->el0 != NULL) {
      restore_hotswapped_exception(0x400, handlers->el0);
    }
    if (handlers->el1_sp0 != NULL) {
      restore_hotswapped_exception(0x000, handlers->el1_sp0);
      restore_hotswapped_exception(0x200, handlers->el1_spx);
    }
  }

  handlers->el0 = NULL;
  handlers->el1_sp0 = NULL;
  handlers->el1_spx = NULL;
}

/** prepare the state for the following tests
 * e.g. clean TLBs and install exception handler
 */
static void prepare_test_contexts(test_ctx_t* ctx, uint64_t vcpu, run_count_t batch_start_idx, run_count_t batch_end_idx, exception_handlers_refs_t* handlers) {
  clean_tlb_for_batch(batch_start_idx, batch_end_idx);
  set_new_sync_exception_handlers(ctx, vcpu, handlers);
}

/** switch to a particular run's ASID
 */
static void switch_to_test_context(test_ctx_t* ctx, run_count_t r) {
  debug("switching to test context for run %ld\n", r);

  /* set sysregs to what the test needs
   * we do this before write_init_state to ensure
   * changes to translation regime are picked up
   */
  _init_sys_state(ctx);

  if (LITMUS_SYNC_TYPE == SYNC_ASID) {
    uint64_t asid = asid_from_run_count(r);
    vmm_switch_ttable_asid(ctx->ptables[asid], asid);
  }
}

static void return_to_harness_context(test_ctx_t* ctx, uint64_t cpu, uint64_t vcpu, exception_handlers_refs_t* handlers) {
  if (LITMUS_SYNC_TYPE == SYNC_ASID) {
    vmm_switch_ttable_asid(vmm_pgtables[cpu], 0);
  }

  restore_old_sync_exception_handlers(ctx, vcpu, handlers);
}

/** get this physical CPU's allocation to a vCPU (aka test thread)
 */
static uint64_t get_affinity(test_ctx_t* ctx, uint64_t cpu) {
  if (LITMUS_AFF_TYPE == AFF_RAND) {
    return ctx->affinity[cpu];
  } else if (LITMUS_AFF_TYPE == AFF_NONE) {
    return cpu;
  }

  fail("! get_affinity: unknown LITMUS_AFF_TYPE\n");
  return 0;
}

/** run the tests in a loop
 */
static void run_thread(test_ctx_t* ctx, int cpu) {
  /* assuming 8-bit ASIDs we get 256 ASIDs
   * for 16-bit ASIDs we get ~65k ASIDs
  */
  uint64_t nr_asids = 1 << ASID_SIZE;

  /* we reserve ASID 0 for the harness itself
   */
  uint64_t batch_size = 1; //(nr_asids - 1) / 2;

  /**
   * we run the tests in a batched way
   * running $N tests in a row before cleaning up
   *
   * to do this we have to ensure that each test that gets run is given a separate ASID
   * and each test has its own pagetable to manipulate (so they do not interfere with other tests).
   *
   * at the start of a batch we allocate $N pagetables for each test, using ASIDS 0..$N
   * if we have available ASIDs 0..255 (aka 8-bit ASIDs) then we reserve ASID 0 for the test harness itself
   * and allocate ASIDs 1...127 for the tests (aka 127 runs)
   * and reserve ASIDs 128..255 as spare ASIDs for tests.
   *
   * Hence the max batch size is entirely determined by the number of bits available for ASIDs that we have.
   *
   * then at the end of the batch, we clean all the ASIDS, free all the pagetables, flush any caches that may need flushing.
   */
  for (run_count_t j = 0; j < ctx->no_runs;) {
    run_idx_t i;    /* the offset into the va lookup table */
    uint64_t vcpu;  /* the test thread this physical core will execute */

    i = count_to_run_index(ctx, j);
    run_count_t batch_start_idx = j;
    run_count_t batch_end_idx = MIN(batch_start_idx+batch_size, ctx->no_runs);
    allocate_affinities(ctx);
    /* since some vCPUs will skip over the tests
     * it's possible for the test to finish before they get their affinity assignment
     * but thinks it's for the *old* run
     *
     * this bwait ensures that does not happen and that all affinity assignments are per-run
     */
    BWAIT(cpu, ctx->generic_cpu_barrier, NO_CPUS);

    vcpu = get_affinity(ctx, cpu);
    set_vcpu(vcpu);

    allocate_data_for_batch(ctx, vcpu, batch_start_idx, batch_end_idx);
    /* since only 1 vCPU will allocate pagetables to ensure consistency
     * we wait for them to have finished before continuing and trying to read
     * the PTEs
     */
    BWAIT(cpu, ctx->generic_cpu_barrier, NO_CPUS);

    litmus_test_run runs[batch_size];
    setup_run_data(ctx, vcpu, batch_start_idx, batch_end_idx, runs);

    exception_handlers_refs_t handlers = {NULL, NULL, NULL};

    prepare_test_contexts(ctx, vcpu, batch_start_idx, batch_end_idx, &handlers);
    for (int bi = 0; j < batch_end_idx; bi++, j++) {
      th_f* pre = ctx->cfg->setup_fns == NULL ? NULL : ctx->cfg->setup_fns[vcpu];
      th_f* func = ctx->cfg->threads[vcpu];
      th_f* post = ctx->cfg->teardown_fns == NULL ? NULL : ctx->cfg->teardown_fns[vcpu];

      if (vcpu >= ctx->cfg->no_threads) {
        goto run_thread_after_execution;
      }

      start_of_run(ctx, cpu, vcpu, i, j);

      pre(&runs[bi]);
      switch_to_test_context(ctx, vcpu);

      /* this barrier must be last thing before running function */
      BWAIT(vcpu, &ctx->start_barriers[i % 512], ctx->cfg->no_threads);
      func(&runs[bi]);

      if (post != NULL) {
        restore_old_sync_exception_handlers(ctx, vcpu, &handlers);
        post(&runs[bi]);
        set_new_sync_exception_handlers(ctx, vcpu, &handlers);
      }

      end_of_run(ctx, cpu, vcpu, i, j);
run_thread_after_execution:
      BWAIT(cpu, ctx->generic_cpu_barrier, NO_CPUS);
    }
    return_to_harness_context(ctx, cpu, vcpu, &handlers);
  }
}

static void prefetch(test_ctx_t* ctx, run_idx_t i, run_count_t r) {
  for (var_idx_t v = 0; v < ctx->cfg->no_heap_vars; v++) {
    LOCK(&__harness_lock);
    uint64_t* va = ctx_heap_var_va(ctx, v, i);
    uint64_t is_valid = vmm_pte_valid(ptable_from_run(ctx, i), va);
    uint64_t* safe_va = (uint64_t*)SAFE_TESTDATA_VA((uint64_t)va);
    UNLOCK(&__harness_lock);
    if (randn() % 2 && is_valid && *safe_va != ctx_initial_heap_value(ctx, v)) {
      fail(
          "! fatal: initial state for heap var \"%s\" on run %d was %ld not %ld\n",
          varname_from_idx(ctx, v), r, *safe_va, ctx_initial_heap_value(ctx, v));
    }
  }
}

static void resetsp(void) {
  /* check and reset to SP_EL0 */
  uint64_t currentsp;
  asm volatile("mrs %[currentsp], SPSel\n" : [currentsp] "=r"(currentsp));

  if (currentsp == 0b1) { /* if not already using SP_EL0 */
    asm volatile(
        "mov x18, sp\n"
        "msr spsel, x18\n"
        "dsb nsh\n"
        "isb\n"
        "mov sp, x18\n"
        :
        :
        : "x18");
  }
}

static void start_of_run(test_ctx_t* ctx, int cpu, int vcpu, run_idx_t i, run_count_t r) {
  prefetch(ctx, i, r);
  if (! ctx->cfg->start_els || ctx->cfg->start_els[vcpu] == 0) {
    drop_to_el0();
  }
}

static void end_of_run(test_ctx_t* ctx, int cpu, int vcpu, run_idx_t i, run_count_t r) {
  if (! ctx->cfg->start_els || ctx->cfg->start_els[vcpu] == 0)
    raise_to_el1();

  BWAIT(vcpu, ctx->generic_vcpu_barrier, ctx->cfg->no_threads);

  /* only 1 thread should collect the results, else they will be duplicated */
  if (vcpu == 0) {
    uint64_t time = read_clk();
    if (time - ctx->last_tick > 10*TICKS_PER_SEC || ctx->last_tick == 0) {
      char time_str[100];
      sprint_time((char*)&time_str, time, SPRINT_TIME_HHMMSS);
      verbose("  [%s] %d/%d\n", time_str, r, ctx->no_runs);
      ctx->last_tick = time;
    }

    handle_new_result(ctx, i, r);

    /* progress indicator */
    uint64_t step = (ctx->no_runs / 10);
    if (r % step == 0) {
      trace("[%d/%d]\n", r, ctx->no_runs);
    } else if (r == ctx->no_runs - 1) {
      trace("[%d/%d]\n", r + 1, ctx->no_runs);
    }
  }
}

static void start_of_thread(test_ctx_t* ctx, int cpu) {
  /* ensure initial state gets propagated to all cores before continuing ...
  */
  BWAIT(cpu, ctx->generic_cpu_barrier, NO_CPUS);

  /* before can drop to EL0, ensure EL0 has a valid mapped stack space
   */
  resetsp();

  trace("CPU%d: starting test\n", cpu);
}

static void end_of_thread(test_ctx_t* ctx, int cpu) {
  if (ENABLE_PGTABLE) {
    /* restore global non-test pgtable */
    vmm_switch_ttable(vmm_pgtables[cpu]);
  }

  BWAIT(cpu, ctx->generic_cpu_barrier, NO_CPUS);
  trace("CPU%d: end of test\n", cpu);
}

static void start_of_test(test_ctx_t* ctx) {
  ctx->concretization_st = concretize_init(LITMUS_CONCRETIZATION_TYPE, ctx, ctx->cfg, ctx->no_runs);
  if (LITMUS_RUNNER_TYPE != RUNNER_EPHEMERAL) {
    concretize(LITMUS_CONCRETIZATION_TYPE, ctx, ctx->cfg, ctx->concretization_st, ctx->no_runs);
    write_init_states(ctx, ctx->cfg, ctx->no_runs);
  }

  trace("====== %s ======\n", ctx->cfg->name);
}

static void end_of_test(test_ctx_t* ctx) {
  if (ENABLE_RESULTS_HIST) {
    trace("%s\n", "Printing Results...");
    print_results(ctx->hist, ctx);
  }

  trace("Finished test %s\n", ctx->cfg->name);

  concretize_finalize(LITMUS_CONCRETIZATION_TYPE, ctx, ctx->cfg, ctx->no_runs, ctx->concretization_st);
  free_test_ctx(ctx);
  free(ctx);
}