#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "lib.h"


/* global configuration options + default values */
uint64_t NUMBER_OF_RUNS = 100UL;
uint8_t ENABLE_PGTABLE = 0;

uint8_t DEBUG = 0;
uint8_t TRACE = 0;

static void read_arg(char* w) {
  char* word = w;

  switch (w[0]) {
    case '\0':
      return;

    case '-':
      switch (w[1]) {
        case 't':
          TRACE = 1;
          return;

        case 'd':
          DEBUG = 1;
          return;

        case 'p':
          ENABLE_PGTABLE = 1;
          return;

        case 'n':
          word += 2;
          NUMBER_OF_RUNS = atoi(word);
          return;

        default:
          goto read_arg_fail;
      }
      break;

    default:
      goto read_arg_fail;
  }

read_arg_fail:
  printf("! err: unknown argument \"%s\"\n", w);
  abort();
  return;
}

void read_args(int argc, char** argv) {
  for (int i = 0; i < argc; i++) {
    read_arg(argv[i]);
  }
}