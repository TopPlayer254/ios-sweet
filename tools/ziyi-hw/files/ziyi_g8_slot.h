/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#define G8_SLOT_BYTES 65536u
#define G8_SLOT_HEADER 128u
#define G8_FW_BYTES 55888u
struct g8_blob {uint32_t bytes,board;uint8_t firmware_sha256[32],header_sha256[32];};
int g8_slot_decode(const volatile uint8_t *,size_t,uint8_t *,size_t,struct g8_blob *);
int g8_slot_copy(uint8_t *,size_t,struct g8_blob *);
