#include <stdint.h>

#include "lib.h"
#include "argdef.h"

/* global configuration options + default values */
uint64_t NUMBER_OF_RUNS = 10000UL;
uint8_t ENABLE_PGTABLE = 1;  /* start enabled */
uint8_t ENABLE_PERF_COUNTS = 0;

uint8_t ENABLE_RESULTS_HIST = 1;

uint8_t VERBOSE = 1;  /* start verbose */
uint8_t TRACE = 0;
uint8_t DEBUG = 0;

uint8_t ONLY_SHOW_MATCHES = 0;

char* collected_tests[100];
int   collected_tests_count;

sync_type_t LITMUS_SYNC_TYPE = SYNC_ALL;
aff_type_t LITMUS_AFF_TYPE = AFF_RAND;
shuffle_type_t LITMUS_SHUFFLE_TYPE = SHUF_RAND;
concretize_type_t LITMUS_CONCRETIZATION_TYPE = CONCRETE_LINEAR;
char LITMUS_CONCRETIZATION_CFG[1024];
litmus_runner_type_t LITMUS_RUNNER_TYPE = RUNNER_SEMI_ARRAY;

char* sync_type_to_str(sync_type_t ty) {
  switch (ty) {
    case SYNC_NONE:
      return "none";
    case SYNC_ALL:
      return "all";
    case SYNC_ASID:
      return "asid";
    case SYNC_VA:
      return "va";
    default:
      return "unknown";
  }
}

char* aff_type_to_str(aff_type_t ty) {
  switch (ty) {
    case AFF_NONE:
      return "none";
    case AFF_RAND:
      return "rand";
    default:
      return "unknown";
  }
}

char* shuff_type_to_str(shuffle_type_t ty) {
  switch (ty) {
    case SHUF_NONE:
      return "none";
    case SHUF_RAND:
      return "rand";
    default:
      return "unknown";
  }
}

char* concretize_type_to_str(concretize_type_t ty) {
  switch (ty) {
    case CONCRETE_LINEAR:
      return "linear";
    case CONCRETE_RANDOM:
      return "random";
    case CONCRETE_FIXED:
      return "fixed";
    default:
      return "unknown";
  }
}

char* runner_type_to_str(litmus_runner_type_t ty) {
  switch (ty) {
    case RUNNER_ARRAY:
      return "array";
    case RUNNER_SEMI_ARRAY:
      return "semi";
    case RUNNER_EPHEMERAL:
      return "ephemeral";
    default:
      return "unknown";
  }
}

static void help(char* opt) {
  if (opt == NULL || *opt == '\0')
    display_help_and_quit(&ARGS);
  else
    display_help_for_and_quit(&ARGS, opt);
}

static void version(char* opt) {
  printf("%s\n", version_string());
  abort();
}

static void n(char* x) {
  int Xn = atoi(x);
  NUMBER_OF_RUNS = Xn;
}

static void s(char* x) {
  int Xn = atoi(x);
  INITIAL_SEED = Xn;
}

static void show(char* x) {
  display_help_show_tests();
}

static void q(char* x) {
  if (x != NULL && *x != '\0') {
    fail("-q/--quiet did not expect an argument.\n");
  }

  VERBOSE = 0;
  DEBUG = 0;
  TRACE = 0;
}

static void conc_cfg(char* x) {
  valloc_memcpy(LITMUS_CONCRETIZATION_CFG, x, strlen(x));
}

argdef_t ARGS = (argdef_t){
  .args=(const argdef_arg_t*[]){
    OPT(
      "-h",
      "--help",
      help,
      "display this help text and quit\n"
      "\n"
      "displays help text.\n"
      "--help=foo will display detailed help for foo.",
      .show_help_both=1
    ),
    OPT(
      "-V",
      "--version",
      version,
      "display version information and quit\n"
      "\n"
      "displays full version info\n"
    ),
    OPT(
      NULL,
      "--show",
      show,
      "show list of tests and quit\n"
      "\n"
      "displays the complete list of the compiled tests and their groups, then quits.",
      .only_action=1
    ),
    OPT(
      "-n",
      NULL,
      n,
      "number of runs per test\n"
      "\n"
      "sets the number of runs per test\n"
      "X must be an integer (default: 10k).\n"
      "Examples: \n"
      " ./litmus.exe -n300\n"
      " ./litmus.exe -n10k\n"
      " ./litmus.exe -n1M\n"
      "\n"
      "Note that currently 1M is likely to fail due to over-allocation of results.\n"
    ),
    FLAG(
      "-p",
      "--pgtable",
      ENABLE_PGTABLE,
      "enable/disable pagetable tests\n"
    ),
    FLAG(
      NULL,
      "--perf",
      ENABLE_PERF_COUNTS,
      "enable/disable performance tests\n"
    ),
    FLAG(
      "-t",
      "--trace",
      TRACE,
      "enable/disable tracing\n"
    ),
    FLAG(
      "-d",
      "--debug",
      DEBUG,
      "enable/disable debugging\n"
    ),
    FLAG(
      NULL,
      "--verbose",
      VERBOSE,
      "enable/disable verbose output\n"
    ),
    OPT(
      "-q",
      "--quiet",
      q,
      "quiet mode\n"
      "\n"
      "disables verbose/trace and debug output.\n",
      .only_action=1
    ),
    FLAG(
      NULL,
      "--hist",
      ENABLE_RESULTS_HIST,
      "enable/disable results histogram collection\n"
    ),
    OPT(
      "-s",
      "--seed",
      s,
      "initial seed\n"
      "\n"
      "at the beginning of each test, a random seed is selected.\n"
      "this seed can be forced with --seed=12345 which ensures some level of determinism."
    ),
    ENUMERATE(
      "--sync",
      LITMUS_SYNC_TYPE,
      sync_type_t,
      4,
      ARR((const char*[]){"none", "asid", "va", "all"}),
      ARR((sync_type_t[]){SYNC_NONE, SYNC_ASID, SYNC_VA, SYNC_ALL}),
      "type of tlb synchronization\n"
      "\n"
      "none:  no synchronization of the TLB (incompatible with --pgtable).\n"
      "all: always flush the entire TLB in-between tests.\n"
      "va: (EXPERIMENTAL) only flush test data VAs\n"
      "asid: (EXPERIMENTAL) assign each test run an ASID and only flush that \n"
    ),
    ENUMERATE(
      "--aff",
      LITMUS_AFF_TYPE,
      aff_type_t,
      2,
      ARR((const char*[]){"none", "rand"}),
      ARR((aff_type_t[]){AFF_NONE, AFF_RAND}),
      "type of affinity control\n"
      "\n"
      "none: Thread 0 is pinned to CPU0, Thread 1 to CPU1 etc.\n"
      "rand: Thread 0 is pinned to vCPU0, etc.  vCPUs may migrate between CPUs"
      " in-between tests.\n"
    ),
    ENUMERATE(
      "--shuffle",
      LITMUS_SHUFFLE_TYPE,
      shuffle_type_t,
      2,
      ARR((const char*[]){"none", "rand"}),
      ARR((shuffle_type_t[]){SHUF_NONE, SHUF_RAND}),
      "type of shuffle control\n"
      "\n"
      "controls the order of access to allocated pages\n"
      "shuffling the indexes more should lead to more interesting caching results.\n"
      "\n"
      "none: access pages in-order\n"
      "rand: access pages in random order\n"
    ),
    ENUMERATE(
      "--concretization",
      LITMUS_CONCRETIZATION_TYPE,
      concretize_type_t,
      3,
      ARR((const char*[]){"linear", "random", "fixed"}),
      ARR((concretize_type_t[]){CONCRETE_LINEAR, CONCRETE_RANDOM, CONCRETE_FIXED}),
      "test concretization algorithml\n"
      "\n"
      "controls the memory layout of generated concrete tests\n"
      "\n"
      "linear: allocate each var as a fixed shape and walk linearly over memory\n"
      "random: allocate randomly\n"
      "fixed: always use the same address, random or can be manually picked via --config-concretize\n"
    ),
    OPT(
      NULL,
      "--config-concretize",
      conc_cfg,
      "concretization-specific configuration\n"
      "\n"
      "the format differs depending on the value of --concretization:\n"
      "for random, linear: do nothing.\n"
      "for fixed:\n"
      "format:  [<var>=<value]*\n"
      "example: \n"
      "  --config-concretize=\"x=0x1234,y=0x5678,c=0x9abc\"\n"
      "  places x at 0x1234, y at 0x5678 and z at 0x9abc.\n"
    ),
    ENUMERATE(
      "--runner",
      LITMUS_RUNNER_TYPE,
      litmus_runner_type_t,
      3,
      ARR((const char*[]){"array", "semi", "ephemeral"}),
      ARR((litmus_runner_type_t[]){RUNNER_ARRAY, RUNNER_SEMI_ARRAY, RUNNER_EPHEMERAL}),
      "test runner algorithml\n"
      "\n"
      "controls when concretization occurs and how the test is run\n"
      "\n"
      "array: full array-ization, all alloation done upfront\n"
      "stanard: semi-arrayization, VA selection upfront, synchronization at runtime\n"
      "ephemeral: allocate at runtime\n"
    ),
    NULL,  /* nul terminated list */
  }
};

void read_args(int argc, char** argv) {
  argparse_read_args(&ARGS, argc, argv);
}