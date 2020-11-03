#include "lib.h"

/** here we define the translation table and virtual memory space
 *
 *
 * the address space on e.g. the rpi4 with the given
 * linker script (and 1 GiB of RAM) is given below
 *
 * addresses are of the form
 *  0xVA AT(0xPA)
 * if no PA is specified, the VA = PA
 *
 * the virtual address space is partitioned like so:
 *  the lower 1G region is IO-MMU mapped region
 *  the  next 1G is id-mapped code,data,bss,relocations,heap and physical stack space
 *  the  next 1G+ is id-mapped testdata
 *  the  next 1G is thread-specific mappings
 *  at 64G is a 64G id-map of all of physical memory
 *  at 128G is a 64G mapping of test data (only when running tests)
 *
 * since the 4k pages means levels of the pagetable maps regions of size:
 *  L0 - 512G
 *  L1 - 1G
 *  L2 - 2M
 *  L3 - 4K
 *
 * this mapping ensures *all* of the harness fits in 1 L0 entry,
 * the first L1 mapping can be shared between all tables
 * and the following L1 mappings can be allocated on a per-thread or per-test-run basis.
 *
 * remaining L0 entries can be used for IO-mapped highmem regions if needed later on,
 * but are not used by the harness.
 *
 *
 *  TOP OF MEM
 *
 *  (then we fill the next NO_CPUS*PAGE_SIZE bytes with the stack
 *   which is loaded here,  but whose virtual address is at the 2G+ region)
 *
 *  (we fill the rest of memory (up to the beginning of the stack space)
 *   with the test data section)
 *  -------------- 0x8000_0000  (RAM_END aka 2 * GiB)
 * |
 * |
 * |  TEST DATA
 * |
 * |
 *  --------------- 0x4880_0000
 *
 *  --------------- 0x48a00000 AT(0x8000_0000  (RAM_END))
 * |
 * |  STACK
 * |
 *  --------------- 0x4880_0000 AT(0x7fe0_0000)
 *
 *  (then fill the next 128M with a shared heap, that
 *   threads can use for shared data allocation)
 *  --------------- 0x4880_0000  (1 GiB + 8 MiB + 128 MiB)
 * |
 * |  HEAP
 * |
 *  --------------- 0x4060_0000
 *
 *  (we stick the data in a 2M region)
 *  --------------- 0x4060_0000
 * |
 * | DATA & BSS
 * |
 *  ~~~~~~~~~~~~~~~ 0x4040_0000  (1 GiB + 4 MiB)
 *
 *  (then a 2M region with relocations (expected mostly zero))
 *  ~~~~~~~~~~~~~~~
 * | Relocations
 *  ~~~~~~~~~~~~~~~
 *
 *  (we fit all of the code in the first 2M region above the IO region)
 *  ~~~~~~~~~~~~~~~ 0x4020_0000
 * |
 * | Text & Vector Table
 * |
 *  --------------- 0x4000_0000  (1 GiB)
 * |
 * | IO Region
 * |
 * ---------------- BOT OF MEM 0x0000_0000
 */

/* TODO: transfer this logic into a generic
 * debug(PTABLE_SET_RANGE, fmt, ...) func
 *
 * perhaps with a
 * START_DEBUG(PTABLE_SET_RANGE)
 *  { ... }
 * END_DEBUG(PTABLE_SET_RANGE)
 *
 * too
 */
#define DEBUG_PTABLE_SET_RANGE(fmt, ...) \
  if (ENABLE_DEBUG_PTABLE_SET_RANGE) { \
      debug(fmt, __VA_ARGS__); \
  }

uint64_t vmm_make_desc(uint64_t pa, uint64_t prot, int level) {
  desc_t final;
  final.type = Block;
  final.oa = pa;
  final.level = level;
  final.attrs = read_attrs(prot);
  final.attrs.nG = PROT_NG_NOT_GLOBAL;
  final.attrs.AF = PROT_AF_ACCESS_FLAG_DISABLE;
  final.attrs.SH = PROT_SH_ISH;
  final.attrs.NS = PROT_NS_NON_SECURE;
  return write_desc(final);
}

static void set_block_or_page(uint64_t* root, uint64_t va, uint64_t pa, uint8_t unmap, uint64_t prot, uint64_t desired_level) {
  vmm_ensure_level(root, desired_level, va);

  desc_t desc = vmm_translation_walk(root, va);

  if (! unmap)
    *desc.src = vmm_make_desc(pa, prot, desc.level);
  else
    *desc.src = 0;
}


static void __ptable_set_range(uint64_t* root,
                      uint64_t pa_start,
                      uint64_t va_start, uint64_t va_end,
                      uint8_t unmap,
                      uint64_t prot) {
  uint64_t level1 = 30, level2 = 21, level3 = 12;

  uint64_t c = 0;

  if (unmap) {
    DEBUG_PTABLE_SET_RANGE("unmap from %p -> %p\n", va_start, va_end);
  } else {
    DEBUG_PTABLE_SET_RANGE("map %p -> %p, prot=%p\n", va_start, va_end, prot);
  }


  if (!IS_ALIGNED(va_start, level3)) {
    puts("! error: __ptable_set_range: got unaligned va_start\n");
    abort();
  }

  if (!IS_ALIGNED(va_end, level3)) {
    puts("! error: __ptable_set_range: got unaligned va_end\n");
    abort();
  }

  uint64_t va = va_start; /* see above: must be aligned on a page */
  uint64_t pa = pa_start;

  for (c=0; !IS_ALIGNED(va, level2) && va+(1UL << level3) <= va_end;
        va += (1UL << level3), pa += (1UL << level3), c++)
    set_block_or_page(
        root, va, pa, unmap, prot,
        3);  // allocate 4k regions up to the first 2M region

  DEBUG_PTABLE_SET_RANGE("allocated %ld lvl3 entries up to %p\n", c, va);

  for (c=0; !IS_ALIGNED(va, level1) && va+(1UL<<level2) <= va_end;
        va += (1UL << level2), pa += (1UL << level2), c++)
    set_block_or_page(
        root, va, pa, unmap, prot,
        2);  // allocate 2M regions up to the first 1G region

  DEBUG_PTABLE_SET_RANGE("allocated %ld lvl2 entries up to %p\n", c, va);

  for (c=0; va < ALIGN_TO(va_end, level1) && va+(1UL << level1) <= va_end;
        va += (1UL << level1), pa += (1UL << level1), c++)
    set_block_or_page(root, va, pa, unmap, prot,
                                1);  // Alloc as many 1G regions as possible

  DEBUG_PTABLE_SET_RANGE("allocated %ld lvl1 entries up to %p\n", c, va);

  for (c=0; va < ALIGN_TO(va_end, level2) && va+(1UL << level2) <= va_end;
        va += (1UL << level2), pa += (1UL << level2), c++)
    set_block_or_page(
        root, va, pa, unmap, prot,
        2);  // allocate as much of what's left as 2MB regions

  DEBUG_PTABLE_SET_RANGE("allocated %ld lvl2 entries up to %p\n", c, va);

  for (c=0; va < va_end;
        va += (1UL << level3), pa += (1UL << level3), c++)
    set_block_or_page(root, va, pa, unmap, prot,
                                3);  // allocate whatever remains as 4k pages.

  DEBUG_PTABLE_SET_RANGE("allocated %ld lvl3 entries up to %p\n", c, va);
}

static void ptable_map_range(uint64_t* root,
                      uint64_t pa_start,
                      uint64_t va_start, uint64_t va_end,
                      uint64_t prot) {
    DEBUG_PTABLE_SET_RANGE("mapping from %p -> %p with translation to %p\n", va_start, va_end, pa_start);
    __ptable_set_range(root, pa_start, va_start, va_end, 0, prot);
}

void vmm_ptable_map(uint64_t* pgtable, VMRegion reg) {
  uint64_t size = reg.va_end - reg.va_start;
  uint64_t pa_start = reg.pa_start != 0 ? reg.pa_start : reg.va_start;
  debug("map 0x%lx bytes for region %p -> %p with translation at %p\n", size, reg.va_start, reg.va_end, pa_start);
  ptable_map_range(pgtable, pa_start, reg.va_start, reg.va_end, reg.memattr | reg.prot);
}

void vmm_ptable_unmap(uint64_t* pgtable, VMRegion reg) {
  uint64_t level;
  uint64_t size = reg.va_end - reg.va_start;
  debug("unmap 0x%lx bytes at %p\n", size, reg.va_start);
  switch (size) {
    case PAGE_SIZE:
      level = 3;
      break;
    case PMD_SIZE:
      level = 2;
      break;
    case PUD_SIZE:
      level = 1;
      break;
    default:
      fail(
        "! vmm_ptable_unmap can only unmap PAGE/PMD/PUD size chunk (0x%lx, 0x%lx, 0x%lx respectively)\n",
        PAGE_SIZE,
        PMD_SIZE,
        PUD_SIZE
      );
  }
  set_block_or_page(pgtable, reg.va_start, 0, 1, 0, level);
}

static void update_table_from_vmregion_map(uint64_t* table, VMRegions regs) {
  VMRegion* map = regs.regions;

  VMRegion r_prev = {0};
  for (int i = VM_MMAP_IO; i <= VM_MMAP_HARNESS; i++) {
    VMRegion r = map[i];

    if (! r.valid)
      continue;

    printf("update_Table_from_vmregion %d => %p\n", i, r.va_start);

    /* sanity check for overlaps */
    if (r.va_start < r_prev.va_end)
      fail("! %o and %o virtual regions overlap\n", r, r_prev);
    else if (r.va_start < r_prev.va_start)
      fail("! %o's region comes after a later region %o\n", r, r_prev);

    vmm_ptable_map(table, r);
  }
}

/* have to ensure that if two threads try allocate pagetables at the same time
 * the shared portion is created once
 */
static lock_t __vmm_shared_lock;

/** allocate (or return the previously-allocated) entry that is shared for all translation tables
 * which is the 1G block that maps the IO region 0x0000'0000 -> 0x4000'0000
 * and the 1G table that maps 0x4000'0000 -> 0x8000'0000
 *
 * although the high region of that last 1G is left invalid
 * to be filled by individuals.
 */
static void __vm_alloc_shared_2g_region(uint64_t* root_pgtable) {
  static uint64_t first_1g_entry = 0;
  static uint64_t second_1g_entries[512] = {0};

  LOCK(&__vmm_shared_lock);

  if (first_1g_entry != 0) {
    /* ensure there's a level1 table we can use,
     * this table must not be shared */
    vmm_ensure_level(root_pgtable, 1, 0x0);

    /* ensure that for the second 1g region, we have
     * a non-shared level2 table */
    vmm_ensure_level(root_pgtable, 2, 0x40000000UL);

    /* copy the descriptors over to the new pgtable
     * this is a 0-cost share of the shared PA space
     * since the L1 table for the first 512G must exist anyway
     * so must the L2 of the 1-2G VA space
     * due to the test changing the mappings in those spaces
     *
     * the first 1G block
     * and the tables+blocks of the 1G->TOP_OF_HEAP space
     * are shared between all tables and are freely copied here.
     */
    *vmm_desc_at_level(root_pgtable, 0, 1, 1) = first_1g_entry;

    for (int i = 0; i < 512; i++) {
      *vmm_desc_at_level(root_pgtable, 1*GiB + (i << PMD_SHIFT), 2, 2) = second_1g_entries[i];
    }

    goto __vm_alloc_shared_2g_region_end;
  }

  VMRegions map = {{
    /* QEMU Memory Mapped I/O
     * 0x00000000 -> 0x08000000  == Boot ROM
     * 0x08000000 -> 0x09000000  == GIC
     * 0x09000000 -> 0x09001000  == UART
     * ....       -> 0x40000000  == MMIO/PCIE/etc
     *
     * see https://github.com/qemu/qemu/blob/master/hw/arm/virt.c
     *
     * Actual RAM is from
     * 0x40000000 -> RAM_END
     *  where RAM_END is defined by the dtb
     */
    [VM_MMAP_IO] = {VMREGION_VALID, 0x0, GiB, PROT_MEMTYPE_DEVICE, PROT_RW_RWX},
    /* linker .text section
     * this contains the harness, litmus and unittest code segments as well
     * as the initial boot segment that occurs before BOT_OF_TEXT
     * executable code must be non-writable at EL0 if executable at EL1
     */
    [VM_TEXT] =  {VMREGION_VALID, GiB, TOP_OF_TEXT, PROT_MEMTYPE_NORMAL, PROT_RX_RX},
    /* data sections contain all static storage duration constants
     */
    [VM_DATA] = {VMREGION_VALID, BOT_OF_DATA, TOP_OF_DATA, PROT_MEMTYPE_NORMAL, PROT_RW_RWX},
    /* physical stack space is mapped by all threads -- but at runtime should use the virtual addr instead
     * if we're allocating a pagetable to be shared between test threads then we map the entire 2M stack space
     */
    [VM_STACK] = {VMREGION_VALID, BOT_OF_STACK_PA, TOP_OF_STACK_PA, PROT_MEMTYPE_NORMAL, PROT_RW_RWX},
    /* for various run-time allocations
     * we have a heap that ALLOC() makes use of
     */
    [VM_HEAP] = {VMREGION_VALID, BOT_OF_HEAP, TOP_OF_HEAP, PROT_MEMTYPE_NORMAL, PROT_RW_RWX},
  }};

  update_table_from_vmregion_map(root_pgtable, map);

  /* save the first 1G mapping in the level1 table off
   * these are the PA mappings that all threads will share, even duirng tests
   */
  first_1g_entry = *vmm_desc_at_level(root_pgtable, 0, 1, 1);

  for (int i = 0; i < 512; i++) {
    second_1g_entries[i] = *vmm_desc_at_level(root_pgtable, 1*GiB + (i << PMD_SHIFT), 2, 2);
  }

__vm_alloc_shared_2g_region_end:
  UNLOCK(&__vmm_shared_lock);
}

static uint64_t* __vm_alloc_base_map(void) {
  uint64_t* root_ptable = alloc(4096);
  valloc_memset(root_ptable, 0, 4096);
  __vm_alloc_shared_2g_region(root_ptable);

  VMRegions extra_universal = {{
    /* each map also contains the testdata physical address space itself is mapped in the virtual space
    * this is for tests that might want to have a INIT_IDENTITY_MAP(x)
    */
    [VM_TESTDATA] = {VMREGION_VALID, BOT_OF_TESTDATA, TOP_OF_TESTDATA, PROT_MEMTYPE_NORMAL, PROT_RW_RWX},
  }};

  update_table_from_vmregion_map(root_ptable, extra_universal);

  return root_ptable;
}

static uint64_t* __vmm_alloc_table(uint8_t is_test) {
  uint64_t* root_ptable = __vm_alloc_base_map();
  int cpu = get_cpu();

  uint64_t stack_bot, stack_top;
  uint64_t vtable_bot, vtable_top;

  if (is_test) {
    /* if in a test, then allocate the whole 1G entry to the stack */
    stack_bot = STACK_MMAP_BASE;
    stack_top = STACK_MMAP_BASE + 1*GiB;

    vtable_bot = VTABLE_MMAP_BASE;
    vtable_top = VTABLE_MMAP_BASE + 1*GiB;
  } else {
    stack_bot = STACK_MMAP_BASE + cpu*PAGE_SIZE;
    stack_top = stack_bot + PAGE_SIZE;

    vtable_bot = VTABLE_MMAP_BASE + cpu*PAGE_SIZE;;
    vtable_top = vtable_bot + PAGE_SIZE;
  }

  VMRegions map = {{
    /* the harness itself maps all of memory starting @ 64G
     */
    [VM_MMAP_HARNESS] = {VMREGION_VALID, HARNESS_MMAP_BASE, HARNESS_MMAP_BASE+TOTAL_MEM, PROT_MEMTYPE_NORMAL, PROT_RW_RWX, GiB},

    /* each thread has its stack mapped
     * at a high VA @ 16G
     */
    [VM_MMAP_STACK] = {VMREGION_VALID, stack_bot, stack_top, PROT_MEMTYPE_NORMAL, PROT_RW_RWX, BOT_OF_STACK_PA},

    /* re-map vector_base_addr to some non-executable mapping of the vector table
     * which can be used by the test harness to update the vtable at runtime
     *
     * if the pagetable is shared between threads, we have to map the entire range
     */
    [VM_MMAP_VTABLE] = {VMREGION_VALID, vtable_bot, vtable_top, PROT_MEMTYPE_NORMAL, PROT_RW_RWX, vector_base_pa},
  }};

  update_table_from_vmregion_map(root_ptable, map);

  if (! is_test) {
    if (DEBUG && ENABLE_DEBUG_PTABLE_SET_RANGE) {
      debug("table:\n");
      vmm_dump_table(root_ptable);
    }
    return root_ptable;
  }

  /* finally, the test data (variables etc) are placed into
   * physical address space TOP_OF_HEAP -> TOP_OF_MEM but with
   * a virtual address space placed after RAM_END
   *
   * we allocate 16 x 8M contiguous regions
   * but spread evenly throughout a 64G VA space located at 128G
   */
  for (int i = 0 ; i < NR_REGIONS; i++) {
    uint64_t start_va = TESTDATA_MMAP_8M_VA_FROM_INDEX(i);
    uint64_t start_pa = TESTDATA_MMAP_8M_PA_FROM_INDEX(i);
    ptable_map_range(root_ptable, start_pa, start_va, start_va + REGION_SIZE, PROT_MEMTYPE_NORMAL | PROT_RW_RWX);
  }

  return root_ptable;
}

uint64_t* vmm_alloc_new_4k_pgtable(void) {
  uint64_t* ptable = __vmm_alloc_table(0);
  debug("allocated new generic 4k pgtable rooted at %p\n", ptable);
  return ptable;
}

uint64_t* vmm_alloc_new_test_pgtable(void) {
  uint64_t* ptable = __vmm_alloc_table(1);
  debug("allocated new test pgtable rooted at %p\n", ptable);
  return ptable;
}

static void set_new_ttable(uint64_t ttbr, uint64_t tcr, uint64_t mair) {
  /* no printf happens here, so need to worry about disabling locking during them */
  if ((read_sysreg(sctlr_el1) & 1) == 1) {
    printf("! err: set_new_ttable:  MMU already on!\n");
    abort();
  }
  write_sysreg(ttbr, ttbr0_el1);
  write_sysreg(tcr, tcr_el1);
  write_sysreg(mair, mair_el1);
  dsb();
  isb();
  vmm_mmu_on();
}

void vmm_set_id_translation(uint64_t* pgtable) {
  if (pgtable == NULL) {
    vmm_mmu_off();
    return;
  }

  /* now set the new TTBR and TCR */
  uint64_t asid = 0;
  uint64_t ttbr = TTBR0(pgtable, asid);

  uint64_t tcr = \
    0          |
    (0L << 39) |  /* HA, software access flag */
    (1L << 37) |  /* TBI, top byte ignored. */
    (5L << 32) |  /* IPS, 48-bit (I)PA. */
    (0 << 14)  |  /* TG0, granule size, 4K. */
    (3 << 12)  |  /* SH0, inner shareable. */
    (1 << 10)  |  /* ORGN0, normal mem, WB RA WA Cacheable. */
    (1 << 8)   |  /* IRGN0, normal mem, WB RA WA Cacheable. */
    (16 << 0)  ;  /* T0SZ, input address is 48 bits => VA[47:12] are
                      used for  translation starting at level 0. */


  #define MAIR_ATTR(idx, attr)  ((attr) << (idx*8))
  uint64_t mair = \
    MAIR_ATTR(PROT_ATTR_DEVICE_nGnRnE, MAIR_DEVICE_nGnRnE)  |
    MAIR_ATTR(PROT_ATTR_DEVICE_GRE, MAIR_DEVICE_GRE)        |
    MAIR_ATTR(PROT_ATTR_NORMAL_NC, MAIR_NORMAL_NC)          |
    MAIR_ATTR(PROT_ATTR_NORMAL_RA_WA, MAIR_NORMAL_RA_WA)    |
    0;

  set_new_ttable(ttbr, tcr, mair);
}

void vmm_switch_ttable(uint64_t* new_table) {
  write_sysreg(TTBR0(new_table, 0), ttbr0_el1);
  dsb();
  isb();
  vmm_flush_tlb();
}

void vmm_switch_asid(uint64_t asid) {
  uint64_t ttbr = read_sysreg(ttbr0_el1);
  write_sysreg(TTBR0(ttbr, asid), ttbr0_el1);
  isb();  /* is this needed? */
}

void vmm_switch_ttable_asid(uint64_t* new_table, uint64_t asid) {
  write_sysreg(TTBR0(new_table, asid), ttbr0_el1);
  isb();
}

static void _vmm_walk_table(uint64_t* pgtable, int level, uint64_t va_start, walker_cb_t* cb_f) {
  for (int i = 0; i < 512; i++) {
    uint64_t va_start_i = va_start+i*LEVEL_SIZES[level];
    desc_t d = read_desc(pgtable[i], level);
    if (d.type == Table) {
      _vmm_walk_table((uint64_t*)d.table_addr, level+1, va_start_i, cb_f);
    }

    int is_leaf = d.type == Table ? 0 : 1;
    va_range r = {va_start_i,va_start_i+LEVEL_SIZES[level]};
    cb_f(level, is_leaf, pgtable, d, r);
  }
}

void vmm_walk_table(uint64_t* root, walker_cb_t* cb_f) {
  _vmm_walk_table(root, 0, 0, cb_f);
}

static void __vmm_free_pgtable(uint64_t* pgtable, int level) {
  for (int i = 0; i < 512; i++) {
    desc_t d = read_desc(pgtable[i], level);
    if (d.type == Table) {
      __vmm_free_pgtable((uint64_t*)d.table_addr, level+1);
    }
  }

  free(pgtable);
}

static void _free_test_pgtable_entry(int level, int is_leaf, uint64_t* parent, desc_t desc, va_range r) {
  /* entries between 0 -> TOP_OF_HEAP are shared and so do not free
   */
  if (TOP_OF_HEAP <= r.end) {
    return;
  }

  if (r.start <= TOP_OF_HEAP) {
    fail("! table contained entry {%p, %p} which overlaps with HEAP\n", r.start, r.end);
  }

  if (! is_leaf) {
    free((uint64_t*)desc.table_addr);
  }
}

void vmm_free_generic_pgtable(uint64_t* pgtable) {
  vmm_walk_table(pgtable, _free_test_pgtable_entry);
  free(pgtable);
}

void vmm_free_test_pgtable(uint64_t* pgtable) {
  __vmm_free_pgtable(pgtable, 0);
}