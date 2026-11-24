/* CPU diagnostic output only. No GPU register or command API. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#define G32_BYTES 160u
uint32_t g32_crc(const void*,size_t);
int g32_validate_record(const uint8_t*,size_t);
int g32_validate_packet(const uint8_t*,size_t);
/* gsr128, if non-NULL, must reference exactly 128 readable bytes. Output and
 * input ranges must not overlap. All failures occur before output writes. */
int g32_record(uint32_t sample,unsigned worker,unsigned observer,uint64_t epoch,
 uint64_t counter,const uint8_t *gsr128,uint8_t *out,size_t capacity);
int g32_packet(const uint8_t *record,size_t bytes,uint32_t tag,unsigned page,uint8_t *out,size_t capacity);
int g32_draw(volatile uint32_t *frame,size_t words,unsigned width,unsigned height,
 unsigned stride,const uint8_t *packet,size_t bytes);
