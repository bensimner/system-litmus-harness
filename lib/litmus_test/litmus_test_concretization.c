/**
 * concretization is the process of selecting virtual addresses for each variable
 * for each run of the harness.
 *
 * there are 2 stages of concretization:
 *  - selection of VAs
 *  - initialization of ptes/values
 *
 * there are roughly 3 ways to run a litmus test:
 *  - array:
 *    both phases of concretization happen at the beginning of time:
 *        SELECT VAs ITER #1
 *        INIT ITER #1
 *        SELECT VAs ITER #2
 *        INIT ITER #2
 *        ... then later ...
 *        RUN ITER #1
 *        RUN ITER #2
 *
 *  - ephemeral:
 *    both phases happen when the test is running:
 *      SELECT VAs ITER #1
 *      INIT ITER #1
 *      RUN ITER #1
 *      SELECT VAs ITER #2
 *      INIT ITER #2
 *      RUN ITER #2
 *
 *  - semi-array:
 *    selection of VAs and initialization happens upfront, like array.
 *    But PTEs need to be cleaned after each run since different runs may re-use the same page.
 */

#include "lib.h"

uint64_t LEVEL_SIZES[6] = {
  [REGION_VAR] = 0x1,
  [REGION_CACHE_LINE] = 0x0,  /* filled at runtime */
  [REGION_PAGE] = PAGE_SIZE,
  [REGION_PMD] = PMD_SIZE,
  [REGION_PUD] = PUD_SIZE,
  [REGION_PGD] = PGD_SIZE,
};

uint64_t LEVEL_SHIFTS[6] = {
  [REGION_VAR] = 0x1,
  [REGION_CACHE_LINE] = 0x0,  /* filled at runtime */
  [REGION_PAGE] = PAGE_SHIFT,
  [REGION_PMD] = PMD_SHIFT,
  [REGION_PUD] = PUD_SHIFT,
  [REGION_PGD] = PGD_SHIFT,
};

/* random */
extern void concretize_random_one(test_ctx_t* ctx, const litmus_test_t* cfg, void* st, run_idx_t run);
extern void concretize_random_all(test_ctx_t* ctx, const litmus_test_t* cfg, void* st, run_idx_t no_runs);

/* fixed */
extern void concretize_fixed_one(test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t run);
extern void concretize_fixed_all(test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t no_runs);

/* linear */
extern void concretize_linear_all(test_ctx_t* ctx, const litmus_test_t* cfg, void* st, run_idx_t no_runs);

/** given a var and an index perform the necessary initialization
 * to their pagetable entries
 *
 * assumes all the vars have their VAs assigned and the default
 * initial pagetable entries for all tests have been created
 */
void set_init_pte(test_ctx_t* ctx, var_idx_t varidx, run_idx_t run) {
  if (! ENABLE_PGTABLE)
    return;

  var_info_t* vinfo = &ctx->heap_vars[varidx];
  uint64_t* va = vinfo->values[run];
  uint64_t* ptable = ptable_from_run(ctx, run);
  vmm_ensure_level(ptable, 3, (uint64_t)va);

  uint64_t* pte = vmm_pte(ptable, (uint64_t)va);

  /* now if it was unmapped we can reset the last-level entry
  * to be invalid
  */
  if (vinfo->init_unmapped) {
    *pte = 0;
  } else {
  /* otherwise we write the level3 descriptor for this VA
   */
    uint64_t pg = SAFE_TESTDATA_PA((uint64_t)va) & ~BITMASK(PAGE_SHIFT);
    uint64_t default_desc = vmm_make_desc(pg, PROT_DEFAULT_HEAP, 3);
    *pte = default_desc;

    /* if the initial state specified access permissions
     * overwrite the default ones
     */
    if (vinfo->init_set_ap) {
      desc_t desc = read_desc(*pte, 3);
      desc.attrs.AP = vinfo->init_ap;
      *pte = write_desc(desc);
    }

    /* also set the AttrIdx if it was set */
    if (vinfo->init_set_attridx) {
      desc_t desc = read_desc(*pte, 3);
      desc.attrs.attr = vinfo->init_attridx;
      *pte = write_desc(desc);
    }
  }

  /* set alias by copying the last-level pte OA
  * this means we must ensure that all VAs for this idx are chosen
  * before attempting to set_init_var
  */
  if (vinfo->is_alias) {
    var_idx_t otheridx = vinfo->alias;
    uint64_t otherva = (uint64_t )ctx->heap_vars[otheridx].values[run];
    uint64_t otherpa = TESTDATA_MMAP_VIRT_TO_PHYS(otherva);

    /* do not copy attrs of otherpte */
    desc_t desc = read_desc(*pte, 3);
    desc.oa = PAGE(otherpa) << PAGE_SHIFT;
    *pte = write_desc(desc);
  }

  /* flush TLB now
   * so that the po-later writes/reads to VA succeed
   *
   * N.B. if we want to set MAIR_EL1 or other registers that
   * might get TLB cached we have to ensure we did so before this point.
   */
  if (LITMUS_SYNC_TYPE == SYNC_ALL) {
    vmm_flush_tlb();
  } else if (LITMUS_SYNC_TYPE == SYNC_ASID) {
    /* do nothing
     * ASIDs get cleaned up on batch creation */
  } else if (LITMUS_SYNC_TYPE == SYNC_VA) {
    vmm_flush_tlb_vaddr((uint64_t)va);
  }

  attrs_t attrs = vmm_read_attrs(ptable, (uint64_t)va);
  if (attrs.AP == PROT_AP_RW_RWX) {
    /* we need to clean caches now
    * since one of the mappings might require a NC mapping
    * we have to ensure that it gets flushed out
    *
    * so the value we write in set_init_var becomes the coherent one
    *
    * TODO BS: is this true ?
    */
    dc_civac((uint64_t)va);

    dsb();
  }
}

/** given a var and an index perform the necessary initialization
 *
 * assumes all vars have their VAs assigned and the default initial pagetable
 * for the test has been created.
 *
 * this will, for each run, for each VA, set the PTE entry for that VA
 * and then write the initial value (if applicable).
 */
void set_init_var(test_ctx_t* ctx, var_idx_t varidx, run_idx_t run) {
  var_info_t* vinfo = &ctx->heap_vars[varidx];

  uint64_t* va = vinfo->values[run];
  set_init_pte(ctx, varidx, run);

  if (ENABLE_PGTABLE) {
    /* convert the VA to the PA
    * then convert the PA to its location
    * in the HARNESS MMAP to get reliable R/W mapping
    */
    uint64_t* safe_va = (uint64_t*)SAFE_TESTDATA_VA((uint64_t)va);
    *safe_va = vinfo->init_value;
  } else {
    *va = vinfo->init_value;
  }
}

void write_init_state(test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t run) {
  for (int v = 0; v < cfg->no_heap_vars; v++) {
    set_init_var(ctx, v, run);
  }

  /* check that the concretization was successful before continuing */
  concretization_postcheck(ctx, cfg, ctx->heap_vars, run);
}

void write_init_states(test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t no_runs) {
  for (int i = 0; i < no_runs; i++) {
    write_init_state(ctx, cfg, i);
  }
}

void pick_concrete_addrs(test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t run);

void concretize_one(concretize_type_t type, test_ctx_t* ctx, const litmus_test_t* cfg, void* st, run_idx_t run) {
  concretization_precheck(ctx, cfg, ctx->heap_vars);

  switch (type) {
    case CONCRETE_LINEAR:
      fail("! concretize: cannot concretize_one with concretization=linear\n");
      break;
    case CONCRETE_RANDOM:
      concretize_random_one(ctx, cfg, st, run);
      break;
    case CONCRETE_FIXED:
      concretize_fixed_one(ctx, cfg, run);
      break;
    default:
      fail("! concretize_one: got unexpected concretization type: %s (%s)\n", LITMUS_CONCRETIZATION_TYPE, (LITMUS_CONCRETIZATION_TYPE));
      break;
  }
}

void concretize(concretize_type_t type, test_ctx_t* ctx, const litmus_test_t* cfg, void* st, run_idx_t no_runs) {
  /* check that the test _can_ be concretized correctly
   * or fail if there's some error */
  concretization_precheck(ctx, cfg, ctx->heap_vars);

  switch (type) {
    case CONCRETE_LINEAR:
      concretize_linear_all(ctx, cfg, st, no_runs);
      break;
    case CONCRETE_RANDOM:
      concretize_random_all(ctx, cfg, st, no_runs);
      break;
    case CONCRETE_FIXED:
      concretize_fixed_all(ctx, cfg, no_runs);
      break;
    default:
      fail("! concretize: got unexpected concretization type: %s (%s)\n", LITMUS_CONCRETIZATION_TYPE, (LITMUS_CONCRETIZATION_TYPE));
      break;
  }
}

void* concretize_random_init(test_ctx_t* ctx, const litmus_test_t* cfg);
void concretized_fixed_init(test_ctx_t* ctx, const litmus_test_t* cfg);
void* concretize_linear_init(test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t no_runs);

/** initialize the concretizer
 * and optionally return any state to be passed along
 */
void* concretize_init(concretize_type_t type, test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t no_runs) {
  switch (type) {
    case CONCRETE_LINEAR:
      return concretize_linear_init(ctx, cfg, no_runs);
    case CONCRETE_RANDOM:
      return concretize_random_init(ctx, cfg);
    case CONCRETE_FIXED:
      concretized_fixed_init(ctx, cfg);
      break;
    default:
      fail("! concretize_allocate_st: got unexpected concretization type: %s (%s)\n", LITMUS_CONCRETIZATION_TYPE, (LITMUS_CONCRETIZATION_TYPE));
      break;
  }

  return NULL;
}

void concretize_linear_finalize(test_ctx_t* ctx, const litmus_test_t* cfg, void* st);
void concretize_random_finalize(test_ctx_t* ctx, const litmus_test_t* cfg, void* st);

/** cleanup after a run
 *
 * and optionally free any allocated state
 */
void concretize_finalize(concretize_type_t type, test_ctx_t* ctx, const litmus_test_t* cfg, run_idx_t no_runs, void* st) {
  switch (type) {
    case CONCRETE_LINEAR:
      concretize_linear_finalize(ctx, cfg, st);
      break;
    case CONCRETE_RANDOM:
      concretize_random_finalize(ctx, cfg, st);
      break;
    case CONCRETE_FIXED:
      break;
    default:
      fail("! concretize_free_st: got unexpected concretization type: %s (%s)\n", LITMUS_CONCRETIZATION_TYPE, (LITMUS_CONCRETIZATION_TYPE));
      break;
  }
}