/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
enum {G24P_NEW=0,G24P_PROBED=1,G24P_INIT=2,G24P_MEMORY=3,G24P_AUTH=4,G24P_REFUSED=5};
enum {G24P_INFO=1,G24P_METADATA,G24P_RANGE,G24P_START};
struct g24_pas {uint64_t epoch,last[8],metadata_pa;uint32_t state,error,calls,interrupts,metadata_retained,image_retained;};
struct g24_pas_io {void*ctx;int(*qualified)(void*,uint64_t,unsigned);int(*smc)(void*,uint64_t[8]);};
int g24_pas_probe(struct g24_pas*,uint64_t,const struct g24_pas_io*);
int g24_pas_init(struct g24_pas*,uint64_t,const struct g24_pas_io*);
int g24_pas_memory(struct g24_pas*,const struct g24_pas_io*);
int g24_pas_auth(struct g24_pas*,const struct g24_pas_io*);
