/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#define ZG_REGIONS 4u
struct zg_region {uint64_t base,bytes;};
struct zg_contract {uint32_t board,cb,smr,sid,sid_mask,handoff;struct zg_region regions[ZG_REGIONS];};
struct zg_geometry {uint64_t page_base,page_span,bias;};
typedef int (*zg_get_property)(void *,const char *,const void **,unsigned *);
/* Strict normalized SecureDT schema; no Android FDT/flat-MMU assertions. */
int zg_contract_read(struct zg_contract *,zg_get_property,void *);
int zg_kernel_context(uint64_t current_el,uint64_t sctlr,uint64_t counter_hz);
int zg_map_geometry(uint64_t base,uint64_t bytes,uint64_t page,struct zg_geometry *);
int zg_region_offset(const struct zg_region *,uint64_t address,uint64_t bytes,uint64_t *offset);
