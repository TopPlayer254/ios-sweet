/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ziyi_g24_engine.h"
#define G24_RECORD_BYTES 320u
struct g24_native_facts {uint32_t zap_crc,zap_backed,zap_copied,guard_error;};
int g24_record_pack(const struct g24_workspace*,const struct g24_native_facts*,uint8_t*,size_t);
int g24_record_valid(const uint8_t*,size_t);
