/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define G35_FINAL_BYTES 880u
#define G35_TRI_BYTES 256u
#define G35_PAGES 10u
uint32_t g35_crc(const void *,size_t);
int g35_final_pack(uint8_t *,size_t,uint64_t,unsigned,unsigned,
 const uint8_t *,const uint8_t *,const uint8_t *,const uint8_t *);
int g35_final_valid(const uint8_t *,size_t);
int g35_final_packet(const uint8_t *,size_t,unsigned,uint8_t *,size_t);
int g35_final_packet_valid(const uint8_t *,size_t);
int g35_final_draw(volatile uint32_t *,size_t,const uint8_t *,size_t);
