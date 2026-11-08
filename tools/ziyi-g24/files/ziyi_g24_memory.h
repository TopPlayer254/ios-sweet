/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g8_translation.h"
#define GC_CPU_PAGE 16384u
#define GC_PAGES 7u
#define GC_TABLES 3u
#define GC_MAPS 24u
#define GC_IOVA 0x61000000u
enum { GC_OK=0, GC_ARGUMENT=1, GC_OWNERSHIP=2, GC_OVERLAP=3, GC_TABLE=4 };
/* CPU-only inputs. The native owner must independently validate its complete
 * hop allocation and all aliases before passing these values. This structure
 * is never proof of native ownership or permission to publish a route. */
struct gc_request {
 uint64_t epoch,hfi_pa,pa[GC_PAGES];
 uint32_t pages,normal_nc,private_owned;
};
struct gc_plan {
 uint64_t epoch;
 uint32_t valid,ring_iova,pixel_iova,fence_iova,sqe_iova,sqe_capacity;
 struct gt_plan translations;
 uint64_t tables[GC_TABLES][GT_ENTRIES];
};
/* No physical dereference, device write, alias conversion or publication.
 * Output is valid only on GC_OK. The table page's fourth 4-KiB quarter is not
 * used; the native adapter must retain/check it as a guard. Table CPU output
 * must be copied into the separately owned table backing before publication. */
int gc_build(struct gc_plan*,const struct gc_request*);
