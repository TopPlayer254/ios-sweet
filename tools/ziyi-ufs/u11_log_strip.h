/* Diagnostic console geometry only; full iOS display dimensions are unchanged. */
#ifndef ZIYI_LOG_STRIP_H
#define ZIYI_LOG_STRIP_H
#include <stdint.h>
#define ZS_ROWS 11u
#define ZS_WIDTH 1080u
#define ZS_HEIGHT 2400u
#define ZS_STRIDE 4320u
#define ZS_PIXELS (ZS_ROWS*32u)
#define ZS_TOP (ZS_HEIGHT-ZS_PIXELS)
#define ZS_PHYS 0xb8000000ull
#define ZS_BASE (ZS_PHYS+(uint64_t)ZS_TOP*ZS_STRIDE)
#define ZS_BYTES ((uint64_t)ZS_PIXELS*ZS_STRIDE)
static inline uint32_t zs_strip_pixels(uint32_t rows,uint64_t phys,uint32_t w,uint32_t h,uint32_t stride) {
 return rows==ZS_ROWS&&phys==ZS_PHYS&&w==ZS_WIDTH&&h==ZS_HEIGHT&&stride==ZS_STRIDE?ZS_PIXELS:0;
}
static inline int zs_console_region(uint32_t rows,uint64_t base,uint64_t size,uint32_t w,uint32_t h,uint32_t stride) {
 return rows==ZS_ROWS&&base==ZS_BASE&&size==ZS_BYTES&&w==ZS_WIDTH&&h==ZS_PIXELS&&stride==ZS_STRIDE;
}
#endif
