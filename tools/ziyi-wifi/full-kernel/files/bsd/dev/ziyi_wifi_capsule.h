/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stddef.h>
#define ZWF_CAPACITY 0xc00000u
#define ZWF_HEADER 256u
#define ZWF_ENTRY 160u
#define ZWF_MAX_FILES 64u
#define ZWF_BOARD 0x5a495949u
struct zwf_view {const uint8_t *base;uint32_t count,data,used;uint8_t header[ZWF_HEADER];};
int zwf_validate(const uint8_t *,size_t,struct zwf_view *);
const uint8_t *zwf_find(const struct zwf_view *,const char *,uint64_t *);
const uint8_t *ziyi_wifi_image_file(const char *,const char *,uint64_t *);
const uint8_t *redwood_image_file(const char *,const char *,uint64_t *,int *);
const uint8_t *hakios_image_file(const char *,const char *,uint64_t *,int *);
