#include "lib.h"

static const char* indents[4] = {
  "",           /* L0 */
  "  ",         /* L1 */
  "    ",       /* L2 */
  "      ",     /* L3 */
};

static void _vmm_dump_table(uint64_t* ptable, int level, int depth) {
  const char* sindent = indents[depth];
  debug("%s[%d] %p\n", sindent, level, ptable);

  for (int i = 0; i < 512; i++) {
    uint64_t d = ptable[i];
    desc_t desc = read_desc(d, level);
    debug("%s[%d:%d]", sindent, level, i);
    /* TODO: make show_desc take char buffer to write into */
    if (DEBUG) {
      show_desc(desc); printf("\n");
    }
    if (desc.type == Table) {
      _vmm_dump_table((uint64_t*)desc.table_addr, level+1, depth+1);
    }
  }
}

void vmm_dump_table(uint64_t* ptable) {
  _vmm_dump_table(ptable, 0, 0);
}