/** an ephemeral compatible concretization algorithm
 *
 * always picks the same addresses for the variables.
 */

#include "lib.h"

void concretize_random_one(test_ctx_t* ctx, const litmus_test_t* cfg, int run);

void __set_var(test_ctx_t* ctx, const litmus_test_t* cfg, char* var, char* val) {
  uint64_t value = atoi(val);
  uint64_t varidx = idx_from_varname(ctx, var);

  ctx->heap_vars[varidx].values[0] = (uint64_t*)value;
}

void __read_from_argv(test_ctx_t* ctx, const litmus_test_t* cfg, char* arg) {
  char* cur = arg;
  char curVar[1024];
  char curVal[1024];
  char* curSink = &curVar[0];

  while (*cur) {
    if (*cur == '=') {
      *curSink = '\0';
      curSink = &curVal[0];
      cur++;
    } else if (*cur == ',') {
      *curSink = '\0';
      __set_var(ctx, cfg, &curVar[0], &curVal[0]);
      curSink = &curVar[0];
      cur++;
    } else {
      *curSink++ = *cur++;
    }
  }

  *curSink = '\0';
  __set_var(ctx, cfg, &curVar[0], &curVal[0]);
}

void concretized_fixed_init(test_ctx_t* ctx, const litmus_test_t* cfg) {
  if (strcmp(LITMUS_CONCRETIZATION_CFG, "")) {
    concretize_random_one(ctx, cfg, 0);
  } else {
    __read_from_argv(ctx, cfg, (char*)LITMUS_CONCRETIZATION_CFG);

    var_info_t* var;
    FOREACH_HEAP_VAR(ctx, var) {
      if (var->values[0] == NULL) {
        fail("! concretized_fixed_init: cannot run test, --concretization=fixed"
             " with --config-concretization passed, but missing value for variable %s"
             " in test %s\n",
             var->name,
             cfg->name
        );
      }
    }
  }
}

void concretize_fixed_one(test_ctx_t* ctx, const litmus_test_t* cfg, int run) {
  var_info_t* var;
  FOREACH_HEAP_VAR(ctx, var) {
    var->values[run] = var->values[0];
  }
}

void concretize_fixed_all(test_ctx_t* ctx, const litmus_test_t* cfg, int no_runs) {
  concretized_fixed_init(ctx, cfg);

  for (uint64_t i = 0; i < ctx->no_runs; i++) {
    concretize_fixed_one(ctx, cfg, i);
  }
}