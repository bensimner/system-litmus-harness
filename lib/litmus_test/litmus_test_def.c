#include "lib.h"

/** helper file for defining litmus tests */

uint64_t* var_va(litmus_test_run* data, const char* name) {
    return data->va[idx_from_varname(data->ctx, name)];
}

uint64_t var_pa(litmus_test_run* data, const char* name) {
    return data->pa[idx_from_varname(data->ctx, name)];
}

uint64_t* var_pte(litmus_test_run* data, const char* name) {
    return data->pte_descs[3][idx_from_varname(data->ctx, name)];
}

uint64_t* var_pte_level(litmus_test_run* data, const char* name, int level) {
    return data->pte_descs[level][idx_from_varname(data->ctx, name)];
}

uint64_t* var_pte_level0(litmus_test_run* data, const char* name) {
    return var_pte_level(data, name, 0);
}

uint64_t* var_pte_level1(litmus_test_run* data, const char* name) {
    return var_pte_level(data, name, 1);
}

uint64_t* var_pte_level2(litmus_test_run* data, const char* name) {
    return var_pte_level(data, name, 2);
}

uint64_t* var_pte_level3(litmus_test_run* data, const char* name) {
    return var_pte_level(data, name, 3);
}

uint64_t var_desc(litmus_test_run* data, const char* name) {
    return data->desc[idx_from_varname(data->ctx, name)];
}

uint64_t var_page(litmus_test_run* data, const char* name) {
    return PAGE(data->va[idx_from_varname(data->ctx, name)]);
}

uint64_t* out_reg(litmus_test_run* data, const char* name) {
    return data->out_reg[idx_from_regname(data->ctx, name)];
}

/** different levels */
char* pin_level_to_str(pin_level_t lvl) {
    switch (lvl) {
        case REGION_SAME_VAR_OFFSET:
            return "REGION_SAME_VAR_OFFSET";
        case REGION_SAME_CACHE_LINE_OFFSET:
            return "REGION_SAME_CACHE_LINE_OFFSET";
        case REGION_SAME_PAGE_OFFSET:
            return "REGION_SAME_PAGE_OFFSET";
        case REGION_SAME_PMD_OFFSET:
            return "REGION_SAME_PMD_OFFSET";
        case REGION_SAME_PUD_OFFSET:
            return "REGION_SAME_PUD_OFFSET";
        case REGION_SAME_PGD_OFFSET:
            return "REGION_SAME_PGD_OFFSET";
        default:
            return "<unknown pin_level_t>";
    }
}

char* rel_offset_to_str(rel_offset_t lvl) {
    switch (lvl) {
        case REGION_SAME_VAR_OFFSET:
            return "REGION_SAME_VAR_OFFSET";
        case REGION_SAME_CACHE_LINE_OFFSET:
            return "REGION_SAME_CACHE_LINE_OFFSET";
        case REGION_SAME_PAGE_OFFSET:
            return "REGION_SAME_PAGE_OFFSET";
        case REGION_SAME_PMD_OFFSET:
            return "REGION_SAME_PMD_OFFSET";
        case REGION_SAME_PUD_OFFSET:
            return "REGION_SAME_PUD_OFFSET";
        case REGION_SAME_PGD_OFFSET:
            return "REGION_SAME_PGD_OFFSET";
        default:
            return "<unknown rel_offset_t>";
    }
}

char* own_level_to_str(own_level_t lvl) {
    switch (lvl) {
        case REGION_OWN_VAR:
            return "REGION_OWN_VAR";
        case REGION_OWN_CACHE_LINE:
            return "REGION_OWN_CACHE_LINE";
        case REGION_OWN_PAGE:
            return "REGION_OWN_PAGE";
        case REGION_OWN_PMD:
            return "REGION_OWN_PMD";
        case REGION_OWN_PUD:
            return "REGION_OWN_PUD";
        case REGION_OWN_PGD:
            return "REGION_OWN_PGD";
        default: {
            char* s = alloc(1024);
            sprintf(s, "<unknown own_level_t: %d>", lvl);
            return s;
        }
    }
}