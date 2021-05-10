#ifndef ASM_AARCH64_SYSREGS_H
#define ASM_AARCH64_SYSREGS_H

#include "arch/registers.h"

#define FIELD_SCTLR_EL1_NOTRAP_CACHE_MAINTENANCE (1 << 26)
#define FIELD_SCTLR_EL1_LITTLE_ENDIAN (0 << 25)  /* bits [25:24] are 0 for little endian */
#define FIELD_SCTLR_EL1_NOPAN (1 << 23)  /* No Privileged Access Never */
#define FIELD_SCTLR_EL1_EXCEPTION_ISB (1 << 22)  /* Exceptions are context synchronizing */
#define FIELD_SCTLR_EL1_NOIESB (0 << 21)  /* No Explicit Error Synchronization */
#define FIELD_SCTLR_EL1_NOWXR (0 << 19)  /* No Write-Execute-Never */
#define FIELD_SCTLR_EL1_NOTRAP_WFE (1 << 18) /* No Trap WFE */
#define FIELD_SCTLR_EL1_NOTRAP_WFI (1 << 16) /* No Trap WFI */
#define FIELD_SCTLR_EL1_NOTRAP_CTR (1 << 15) /* No Trap access CTR_EL0 */
#define FIELD_SCTLR_EL1_NOTRAP_DCZ (1 << 14) /* No Trap DC ZVA */
#define FIELD_SCTLR_EL1_I (1 << 12) /* Instructon accesses are not non-cacheable */
#define FIELD_SCTLR_EL1_ERET_ISB (1 << 11) /* Exception exit are context synchronizing */
#define FIELD_SCTLR_EL1_DATA_ALIGN (0 << 6)  /* Alignment Check on Data Accesses */
#define FIELD_SCTLR_EL1_SP_ALIGN (3 << 3)  /* SP Alignment Check */
#define FIELD_SCTLR_EL1_C (1 << 2)  /* Data Accesess not non-cacheable */
#define FIELD_SCTLR_EL1_ALIGN_CHECK (1 << 1)  /* Alignment checks */
#define FIELD_SCTLR_EL1_MMU_ON (1)
#define FIELD_SCTLR_EL1_MMU_OFF (0)

#define SCTLR_EL1_DEFAULT ( \
    FIELD_SCTLR_EL1_NOTRAP_CACHE_MAINTENANCE \
    | FIELD_SCTLR_EL1_LITTLE_ENDIAN  \
    | FIELD_SCTLR_EL1_NOPAN  \
    | FIELD_SCTLR_EL1_EXCEPTION_ISB  \
    | FIELD_SCTLR_EL1_NOIESB  \
    | FIELD_SCTLR_EL1_NOWXR  \
    | FIELD_SCTLR_EL1_NOTRAP_WFE  \
    | FIELD_SCTLR_EL1_NOTRAP_WFI  \
    | FIELD_SCTLR_EL1_NOTRAP_DCZ  \
    | FIELD_SCTLR_EL1_NOTRAP_CTR  \
    | FIELD_SCTLR_EL1_I  \
    | FIELD_SCTLR_EL1_ERET_ISB \
    | FIELD_SCTLR_EL1_DATA_ALIGN \
    | FIELD_SCTLR_EL1_SP_ALIGN \
    | FIELD_SCTLR_EL1_C \
    | FIELD_SCTLR_EL1_ALIGN_CHECK \
    | FIELD_SCTLR_EL1_MMU_OFF\
    ) \


#endif /* ASM_AARCH64_SYSREGS_H */
