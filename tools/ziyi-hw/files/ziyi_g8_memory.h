/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g8_translation.h"
#include "ziyi_g8_contract.h"
#define GM_BACKING_CAPACITY 0x40000u
struct gm_xnu_memory {
 uint32_t state,backing_bytes,region_count,reserved;
 struct gt_plan table_plan;
 struct gt_mapping mappings[GT_MAX_MAPPINGS];
 struct gn_region regions[GN_MAX_REGIONS];
 uint32_t region_offset[GN_MAX_REGIONS];
 uint64_t table_physical[GT_MAX_TABLES];
 uint8_t backing[GM_BACKING_CAPACITY] __attribute__((aligned(4096)));
 uint64_t tables[GT_MAX_TABLES][GT_ENTRIES] __attribute__((aligned(4096)));
};
struct gm_memory_io {void*ctx;uint64_t(*physical)(void*,const void*);int(*flush)(void*,void*,size_t);int(*clean)(void*,void*,size_t);void(*barrier)(void*);};
int gm_memory_prepare(struct gm_xnu_memory *,const struct gn_plan *,const struct gm_memory_io *);
void *gm_memory_owned_va(struct gm_xnu_memory *,uint32_t iova,size_t bytes);
int gm_memory_publish(struct gm_xnu_memory *,const struct gm_memory_io *);
/* Caller supplies a freshly zeroed, exclusively owned object. Prepare and
 * publish must run while CM3 remains held in reset; they grant no MMIO/DMA
 * authority. No teardown/free API is provided: retain allocation on uncertainty. */
int gm_xnu_memory_prepare(struct gm_xnu_memory *,const struct gn_plan *);
void *gm_xnu_memory_owned_va(struct gm_xnu_memory *,uint32_t iova,size_t bytes);
int gm_xnu_memory_publish(struct gm_xnu_memory *);
