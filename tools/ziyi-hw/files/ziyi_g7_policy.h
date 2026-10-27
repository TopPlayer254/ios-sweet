/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
/* Pure source/reference planner. NO MMIO/SMC, no hardware ownership grant. */
#define GA_CB_LIMIT 6u
#define GA_SMR_LIMIT 6u
#define GA_GMU_FIRST_CB 2u
#define GA_GMU_SID 5u
#define GA_GMU_MASK 0x400u
#define GA_REQUIRED_FACTS 0xffu
#define GA_ARENA_BASE 0xa7400000ULL
#define GA_ARENA_END 0xa7700000ULL
struct ga_snapshot {
 uint32_t facts,dt_cb,dt_smr,dt_handoff_pairs,gr0[2][4],smr[2][6],s2cr[2][6];
 uint64_t epoch;
};
struct ga_plan {uint32_t cb,smr,software_context_map,hardware_write_authority;uint64_t epoch;};
struct ga_layout {uint64_t firmware,backup0,backup1,backing,tables,end;uint32_t table_bytes;};
enum ga_error {GA_OK=0,GA_ARG,GA_FACTS,GA_DT,GA_ID,GA_CHANGED,GA_ACTIVE,GA_FULL,GA_RANGE};
/* Facts must be independently measured by the caller in THIS native session:
 * FDT qualified, sole HLOS GPU-SMMU manager, reset-held CM3, HFI stopped,
 * GX off, rates/counter proof, exact device-IO mapping, bounded context.
 * G7 is read-only: this does not authorize DMA/page-table/cache operations.
 * A bitmask alone never establishes those facts. The result is PLAN ONLY. */
int ga_plan_fresh(const struct ga_snapshot *,uint32_t software_busy,struct ga_plan *);
/* RO global/stream/GR1 metadata subset; NEVER CB registers or writes. */
int ga_ro_offset(const struct ga_snapshot *,uint32_t offset);
int ga_layout_plan(uint64_t anchor,uint32_t firmware_bytes,uint32_t backing_bytes,struct ga_layout *);
