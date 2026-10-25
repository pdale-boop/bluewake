#ifndef BLUEWAKE_INLINE_GPR_H
#define BLUEWAKE_INLINE_GPR_H

#include "direct_calls.h"

/* Bulk charging cannot expose a different PC/downcount to an MMIO or journal
 * callback. Validate the complete stack span before using the inline routine.
 * Actual stores still use the maintained memory helpers, including reservation
 * invalidation. */
static inline bool bw_inline_gpr_memory_ready(CPUState* cpu, unsigned first) {
    if (first < 14 || first > 31 || cpu->ram == NULL || g_mem_write_journal != NULL)
        return false;
    const u32 size = 4u * (32u - first);
    const u32 address = cpu->gpr[11] - size;
    const u32 canonical = address & ~0x40000000u;
    if ((address & 3u) != 0u || canonical < GC_RAM_BASE ||
        size > cpu->ram_size)
        return false;
    const u32 offset = canonical - GC_RAM_BASE;
    if (offset > cpu->ram_size - size ||
        ppc_guest_alias_range_overlaps(address, size) ||
        (canonical != address &&
         ppc_guest_alias_range_overlaps(canonical, size)))
        return false;
    /* Bounds and both accepted address forms are now proven to select plain
     * MEM1. Calling get_ram_ptr() here would repeat the same alias search;
     * the actual loads and stores retain their normal memory helpers. */
    return true;
}

/* After bw_inline_gpr_memory_ready(): the frame's MEM1 bytes, lowest address
 * first (r`first` at offset 0). Proven plain MEM1 with no journal, each
 * mem_write32 would only clear a reservation on its 32-byte granule and store,
 * and nothing can observe the stores in between; so a save clears a
 * reservation on any granule the frame covers once, here, and the stores go
 * straight to memory. A restore's loads need nothing. */
static inline u8* bw_inline_gpr_frame(CPUState* cpu, unsigned first, bool save) {
    const u32 size = 4u * (32u - first);
    const u32 canonical = (cpu->gpr[11] - size) & ~0x40000000u;
    if (save && cpu->reserve_valid) {
        const u32 granule = cpu->reserve_addr & ~0x40000000u & ~31u;
        if (granule >= (canonical & ~31u) && granule <= ((canonical + size - 1u) & ~31u))
            cpu->reserve_valid = false;
    }
#if defined(BW_GUEST_MEM1)
    return BW_GUEST_MEM1 + (canonical - GC_RAM_BASE);
#else
    return cpu->ram + (canonical - GC_RAM_BASE);
#endif
}

/* The same routine as an out-of-line call, for call sites that are many and
 * mostly cold (the REL modules' and the return-keyed entries'): one copy per
 * source file instead of the stores at every site. 0: declined, nothing
 * changed, the call goes out as before; 1: the routine ran and the turn stops
 * at its return; 2: the routine ran and the caller continues at its return.
 * Each step is the inline form's (inline_save_restore_gpr.py's inline_body).
 * Only a translated chunk has the cycle budget (generated.h); the parameter is
 * named ctx because DOLRECOMP_C_LOOP_CYCLE_BUDGET reads ctx. */
#if defined(DOLRECOMP_C_LOOP_CYCLE_BUDGET)
static __attribute__((noinline, unused)) int bw_gpr_run(CPUState* ctx, u32 target, unsigned first,
                                               bool save, u32 ret) {
    if (!bw_direct_call_ready(ctx, target) || !bw_inline_gpr_memory_ready(ctx, first))
        return 0;
    const unsigned count = 32u - first + 1u;  /* the stores or loads, and the blr */
    u32 suffix;
    if (first == 14u) {
        /* The routine's first instruction leads a block that prepays it all. */
        if (!(ctx->downcount > -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET &&
              (ctx->cycle_deadline_budget <= 0 ||
               (ctx->cycle_deadline_budget >= 18 && dolrecomp_block_can_precharge(ctx, count)))))
            return 0;
        suffix = 1u;
    } else {
        if (!(ctx->downcount - (s64)(count - 1u) > -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET))
            return 0;
        suffix = 0u;
    }
    ctx->downcount -= count;
    u8* const slots = bw_inline_gpr_frame(ctx, first, save);
    for (unsigned reg = first; reg < 32u; ++reg) {
        if (save)
            write_be32(slots + 4u * (reg - first), (u32)ctx->gpr[reg]);
        else
            ctx->gpr[reg] = read_be32(slots + 4u * (reg - first));
    }
    ctx->cycle_observation_suffix = suffix;
    ctx->pc = ret;
    /* The routine's return dispatch, then the chassis's checks at the boundary. */
    if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET || ctx->exception != 0u ||
        (ctx->cycle_budget > 0 && ctx->downcount <= -ctx->cycle_budget) ||
        !bw_direct_call_ready(ctx, ret))
        return 1;
    return 2;
}
#endif

#endif
