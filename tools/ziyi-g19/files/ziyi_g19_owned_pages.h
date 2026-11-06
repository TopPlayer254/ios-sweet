/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#include "ziyi_hfi_page_observation.h"

/* Candidate only. Caller serializes every operation and retains ctx/ops for
 * the allocation lifetime. No GMU mapping, queue publication or MMIO here. */
#define HOP_PAGE_SIZE 16384u
#define HOP_MAX_PAGES 8u
#define HOP_NORMAL_NC 0x44u
enum hop_state { HOP_FRESH, HOP_OWNED, HOP_READY, HOP_LIVE,
                 HOP_RELEASED, HOP_REFUSED, HOP_QUARANTINE };
enum hop_error { HOP_OK, HOP_ARGUMENT, HOP_STATE, HOP_POLICY, HOP_LEASE,
                 HOP_ALLOCATE, HOP_BACKING, HOP_ALIAS, HOP_ATTRIBUTES,
                 HOP_TRANSLATION, HOP_IO, HOP_LIFECYCLE };
enum hop_evidence { HOP_LEASE_HELD=1u, HOP_RESET_HELD=2u,
                    HOP_DRAINED=4u, HOP_GMU_UNMAPPED=8u };
#define HOP_RECLAIM (HOP_LEASE_HELD|HOP_RESET_HELD|HOP_DRAINED|HOP_GMU_UNMAPPED)
struct hop_page {
 uint64_t pa, aperture;
 uint32_t old_wimg, touched;
 uint8_t old_attr;
};
struct hop {
 uint64_t epoch, va;
 uint32_t count, bytes, state, error, nc_wimg;
 struct hop_page page[HOP_MAX_PAGES];
};
struct hop_ops {
 void *ctx;
 /* Policy is a separately audited pmap/allocator alias-closure invariant.
  * An AT result, valid managed PA, or a successful void pmap call is not it. */
 int (*policy)(void*,uint32_t *runtime_page_size,uint32_t *normal_nc_wimg);
 int (*evidence)(void*,uint64_t epoch,uint32_t *facts);
 /* Allocation success transfers one whole wired allocation; failure must
  * transfer none. Every uncertain callback after this point quarantines it. */
 int (*allocate)(void*,uint64_t epoch,uint32_t bytes,uint64_t *va);
 int (*backing)(void*,uint64_t va,uint64_t *pa,uint64_t *aperture);
 int (*managed)(void*,uint64_t pa);
 /* Must establish allocator provenance, no external borrowers, complete
  * tracked-PV + physmap alias set, and exclusion of future new aliases. */
 int (*aliases_closed)(void*,uint64_t epoch,uint64_t va,uint64_t pa);
 int (*get_wimg)(void*,uint64_t pa,uint32_t*);
 int (*set_wimg)(void*,uint64_t pa,uint32_t);
 int (*observe)(void*,uint64_t va,uint64_t expected_pa,
                struct gh_cpu_page_observation*);
 int (*free_allocation)(void*,uint64_t epoch,uint64_t va,uint32_t bytes);
};
int hop_prepare(struct hop*,const struct hop_ops*,uint64_t epoch,uint32_t pages);
/* Requalifies every 4KiB subpage at both aliases. On uncertainty, retains all. */
int hop_validate(struct hop*,const struct hop_ops*);
/* Call BEFORE making the allocation reachable by the GMU. A failed/partial
 * device publication still requires the full reset/drain/unmap release path. */
int hop_mark_live(struct hop*,const struct hop_ops*);
int hop_release(struct hop*,const struct hop_ops*);
