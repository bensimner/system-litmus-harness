#include "lib.h"

void init_test_ctx(test_ctx_t* ctx, const litmus_test_t* cfg, int no_runs) {
  var_info_t* var_infos = ALLOC_MANY(var_info_t, cfg->no_heap_vars);
  uint64_t** out_regs = ALLOC_MANY(uint64_t*, cfg->no_regs);
  init_system_state_t* sys_st = ALLOC_ONE(init_system_state_t);
  bar_t* generic_cpu_bar = ALLOC_ONE(bar_t);
  bar_t* generic_vcpu_bar = ALLOC_ONE(bar_t);
  bar_t* bars = ALLOC_MANY(bar_t, 1 << ASID_SIZE);
  run_idx_t* shuffled = ALLOC_MANY(run_idx_t, no_runs);
  run_count_t* rev_lookup = ALLOC_MANY(run_count_t, no_runs);
  int* affinity = ALLOC_MANY(int, NO_CPUS);
  uint64_t** ptables = ALLOC_MANY(uint64_t*, 1 << ASID_SIZE);

  for (int v = 0; v < cfg->no_heap_vars; v++) {
    valloc_memset(&var_infos[v], 0, sizeof(var_info_t));
    var_infos[v].values = ALLOC_MANY(uint64_t*, no_runs);
  }

  sys_st->enable_mair = 0;
  read_var_infos(cfg, sys_st, var_infos, no_runs);

  debug("thread_sync_handlers = {\n");
  if (cfg->thread_sync_handlers != NULL) {
    for (int i = 0; i < cfg->no_threads; i++) {
      debug(" [%d] = {%p, %p}\n", i, cfg->thread_sync_handlers[i][0], cfg->thread_sync_handlers[i][1]);
    }
  }
  debug("}\n");

  for (reg_idx_t r = 0; r < cfg->no_regs; r++) {
    uint64_t* out_reg = ALLOC_MANY(uint64_t, no_runs);
    out_regs[r] = out_reg;
  }

  for (run_idx_t i = 0; i < no_runs; i++) {
    for (reg_idx_t r = 0; r < cfg->no_regs; r++) {
      out_regs[r][i] = 0;
    }

    shuffled[i] = i;
  }

  shuffle(shuffled, sizeof(run_idx_t), no_runs);
  for (run_count_t i = 0; i < no_runs; i++) {
    rev_lookup[shuffled[i]] = i;
  }

  for (int i = 0; i < (1 << ASID_SIZE); i++) {
    bars[i] = EMPTY_BAR;
  }
  *generic_cpu_bar = EMPTY_BAR;
  *generic_vcpu_bar = EMPTY_BAR;

  for (int i = 0; i < NO_CPUS; i++) {
    affinity[i] = i;
  }

  test_hist_t* hist = alloc(sizeof(test_hist_t) + sizeof(test_result_t) * 200);
  hist->allocated = 0;
  hist->limit = 200;
  test_result_t** lut = ALLOC_MANY(test_result_t*, hist->limit);
  hist->lut = lut;

  for (int t = 0; t < hist->limit; t++) {
    test_result_t* new_res =
        alloc(sizeof(test_result_t) + sizeof(uint64_t) * cfg->no_regs);
    hist->results[t] = new_res;
    lut[t] = NULL;
  }

  ctx->no_runs = no_runs;
  ctx->heap_vars = var_infos;
  ctx->system_state = sys_st;
  ctx->out_regs = out_regs;
  ctx->start_barriers = bars;
  ctx->shuffled_ixs = shuffled;
  ctx->shuffled_ixs_inverse = rev_lookup;
  ctx->affinity = affinity;
  ctx->last_tick = 0;
  ctx->hist = hist;
  ctx->ptables = ptables;
  ctx->current_run = 0;
  ctx->privileged_harness = 0;
  ctx->cfg = cfg;
  ctx->concretization_st = NULL;
}

uint64_t ctx_pa(test_ctx_t* ctx, run_idx_t run, uint64_t va) {
  /* return the PA associated with the given va in a particular iteration */
  return (uint64_t)vmm_pa(ptable_from_run(ctx, run), va);
}

uint64_t* ctx_pte(test_ctx_t* ctx, run_idx_t run, uint64_t va) {
  /* return the VA at which the pte lives for the given va in a particular
   * iteration */
  return vmm_pte(ptable_from_run(ctx, run), va);
}


const char* varname_from_idx(test_ctx_t* ctx, var_idx_t idx) {
  for (var_idx_t i = 0; i < ctx->cfg->no_heap_vars; i++) {
    if (idx == i)
      return ctx->cfg->heap_var_names[i];
  }

  fail("! err: no such variable with index %ld.\n", idx);
  return 0;
}

var_idx_t idx_from_varname_infos(const litmus_test_t* cfg, var_info_t* infos, const char* varname) {
  for (var_idx_t i = 0; i < cfg->no_heap_vars; i++) {
    if (strcmp(infos[i].name, varname)) {
      return i;
    }
  }

  return cfg->no_heap_vars;
}

var_idx_t idx_from_varname(test_ctx_t* ctx, const char* varname) {
  for (var_idx_t i = 0; i < ctx->cfg->no_heap_vars; i++) {
    if (strcmp(ctx->heap_vars[i].name, varname)) {
      return i;
    }
  }

  fail("! err: no such variable \"%s\".\n", varname);
  return 0;
}

const char* regname_from_idx(test_ctx_t* ctx, var_idx_t idx) {
  for (int i = 0; i < ctx->cfg->no_regs; i++) {
    if (idx == i)
      return ctx->cfg->reg_names[i];
  }

  fail("! err: no such register with index %ld.\n", idx);
  return 0;
}

reg_idx_t idx_from_regname(test_ctx_t* ctx, const char* varname) {
  for (reg_idx_t i = 0; i < ctx->cfg->no_regs; i++) {
    if (strcmp(ctx->cfg->reg_names[i], varname)) {
      return i;
    }
  }

  fail("! err: no such register \"%s\".\n", varname);
  return 0;
}

/** return the loop counter that links to this run index offset
 */
run_count_t run_count_from_idx(test_ctx_t* ctx, run_idx_t idx) {
  return ctx->shuffled_ixs_inverse[idx];
}

uint64_t asid_from_run_count(run_count_t r) {
  /* reserve ASID 0 for harness */
  return 1 + (r % 254);
}

uint64_t* ptable_from_run(test_ctx_t* ctx, run_idx_t i) {
  run_count_t r = run_count_from_idx(ctx, i);
  uint64_t asid = asid_from_run_count(r);
  return ctx->ptables[asid];
}

void free_test_ctx(test_ctx_t* ctx) {
  for (int t = 0; t < ctx->hist->limit; t++) {
    free(ctx->hist->results[t]);
  }

  free(ctx->hist->lut);
  free(ctx->hist);

  for (int r = 0; r < ctx->cfg->no_regs; r++) {
    free(ctx->out_regs[r]);
  }

  for (int v = 0; v < ctx->cfg->no_heap_vars; v++) {
    free(ctx->heap_vars[v].values);
  }

  free((int*)ctx->affinity);
  free(ctx->shuffled_ixs_inverse);
  free(ctx->shuffled_ixs);
  free((bar_t*)ctx->start_barriers);
  free((bar_t*)ctx->generic_cpu_barrier);
  free((bar_t*)ctx->generic_vcpu_barrier);
  free(ctx->ptables);
  free(ctx->out_regs);
  free(ctx->system_state);
  free(ctx->heap_vars);
}