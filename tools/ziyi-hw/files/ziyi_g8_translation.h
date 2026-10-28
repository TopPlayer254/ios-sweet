/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define GT_PAGE 4096u
#define GT_ENTRIES 512u
#define GT_MAX_TABLES 32u
#define GT_MAX_MAPPINGS 128u
#define GT_OAS_END (1ULL<<36)
#define GT_LEAF_ATTR 0xe03ULL /* page,nG,AF,OSH,MAIR0=NormalNC,privilegedRW */
enum gt_error {GT_OK=0,GT_ARGUMENT,GT_OVERLAP,GT_PHYSICAL,GT_IOVA,GT_CAPACITY};
struct gt_mapping {uint32_t iova,reserved;uint64_t physical;};
struct gt_plan {uint32_t valid,table_count,mapped_pages,reserved;uint64_t table_physical[GT_MAX_TABLES];};
/* Caller owns all arrays. These functions write ONLY caller-provided CPU RAM.
 * No register, physical address dereference, publication, TLB or DMA operation. */
int gt_build(struct gt_plan *,uint64_t (*)[GT_ENTRIES],const uint64_t *table_physical,unsigned table_count,const struct gt_mapping *,unsigned mapping_count);
int gt_lookup(const struct gt_plan *,const uint64_t (*)[GT_ENTRIES],uint32_t iova,uint64_t *physical);
