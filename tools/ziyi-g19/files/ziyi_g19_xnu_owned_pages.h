/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g19_owned_pages.h"
/* Retain this context privately for the entire lifetime. No VA exports,
 * mach memory entries, vm_map copies, extra pmap_enter, or IOMMU mappings.
 * The future GMU mapping is released only after reset+drain are proven.
 * Existing G18 bounded gm_memory API is not modified or accepted here. */
struct hop_xnu {
 uint64_t va,epoch;
 uint32_t bytes;
 void *owner_ctx;
 int (*owner_evidence)(void*,uint64_t,uint32_t*);
};
int hop_xnu_prepare(struct hop*,struct hop_xnu*,uint64_t,uint32_t);
int hop_xnu_validate(struct hop*,struct hop_xnu*);
int hop_xnu_mark_live(struct hop*,struct hop_xnu*);
int hop_xnu_release(struct hop*,struct hop_xnu*);
