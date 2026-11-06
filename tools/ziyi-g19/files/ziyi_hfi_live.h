/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_hfi_version.h"
/* FUTURE PC-qualified transport, not linked to G17. Callbacks require an
 * exclusively retained GMU/HFI owner and independently qualified Normal-NC
 * CPU aliases, with one serialized host caller. No cache-line clean/invalidate
 * operation is provided. This module cannot qualify a native mapping itself. */
enum gh_live_order {GH_LIVE_ACQUIRE=1,GH_LIVE_RELEASE};
struct gh_live_io {
 void *ctx;
 int (*qualified)(void*,uint64_t epoch);
 int (*read32)(void*,uint32_t offset,uint32_t*);
 int (*write32)(void*,uint32_t offset,uint32_t);
 int (*order)(void*,enum gh_live_order);
 int (*doorbell)(void*);
};
enum gh_live_state {GH_LIVE_FRESH,GH_LIVE_PENDING,GH_LIVE_COMPLETE,GH_LIVE_POISON};
enum gh_live_error {GH_LIVE_IO=120,GH_LIVE_OWNER,GH_LIVE_STATE};
struct gh_live {
 uint64_t epoch;uint32_t gmu_address,state,sent,next_read,version,remote_status;
 uint32_t scratch[GN_HFI_WORDS]; /* CPU-private, never a shared alias. */
};
int gh_live_request(struct gh_live*,const struct gh_live_io*,uint32_t sequence);
/* Nonblocking. GN_EMPTY retains the one pending request. Malformed/stale
 * responses poison and retain ownership; no consumer index is advanced. */
int gh_live_poll(struct gh_live*,const struct gh_live_io*);
