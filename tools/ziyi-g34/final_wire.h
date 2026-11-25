/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define G34_FINAL_BYTES 880u
#define G34_TRI_BYTES 256u
#define G34_PAGES 10u
uint32_t g34_crc(const void *,size_t);
int g34_final_pack(uint8_t *,size_t,uint64_t,unsigned,unsigned,
 const uint8_t *,const uint8_t *,const uint8_t *,const uint8_t *);
int g34_final_valid(const uint8_t *,size_t);
int g34_final_packet(const uint8_t *,size_t,unsigned,uint8_t *,size_t);
int g34_final_packet_valid(const uint8_t *,size_t);
int g34_final_draw(volatile uint32_t *,size_t,const uint8_t *,size_t);
