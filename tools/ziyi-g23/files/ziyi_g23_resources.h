/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define GR_COUNT 7u
#define GR_ALL 127u
enum {GR_OK=0,GR_ARGUMENT,GR_HEADER,GR_BOUNDS,GR_DUPLICATE,GR_MISSING,GR_CHANGED,GR_LEVEL,GR_AUX};
/* Canonical order: gfx.lvl, cx.lvl, mx.lvl, SH0, MC0, ACV, CN0. */
struct gr_resource {uint32_t address,slave,version,aux_bytes;uint8_t aux[32];};
struct gr_resources {uint32_t found,entries;struct gr_resource resource[GR_COUNT];};
/* CPU snapshots only: no MMIO, memory mapping, power votes or HFI submission. */
int gr_parse(const uint8_t*,size_t,struct gr_resources*);
int gr_stable(const uint8_t*,size_t,const uint8_t*,size_t,struct gr_resources*);
int gr_arc_vote(const struct gr_resource*,const struct gr_resource*,uint16_t,uint32_t*);
int gr_perf_lowest(const struct gr_resources*,uint32_t sequence,uint32_t out[59]);
int gr_bandwidth(const struct gr_resources*,uint32_t sequence,uint32_t peak_kbps,uint32_t out[160]);
