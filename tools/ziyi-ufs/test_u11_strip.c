#include <stdint.h>
#include <stddef.h>
#include "u11_log_strip.h"
unsigned char iso_font[4096];
#define FBCON_SCALE 2

#define FBCON_FG        0xffffffffu
#define FBCON_BG        0xff000000u

static volatile uint32_t *fbcon_base;
static int ziyi_strip_enabled;
static uint32_t fbcon_width, fbcon_height, fbcon_stride;
static uint32_t fbcon_cols, fbcon_rows, fbcon_col, fbcon_row;

static void
fbcon_clear_row(uint32_t row)
{
	for (uint32_t y = row * 16 * FBCON_SCALE; y < (row + 1) * 16 * FBCON_SCALE; y++) {
		volatile uint32_t *line = fbcon_base + y * (fbcon_stride / 4);
		for (uint32_t x = 0; x < fbcon_width; x++) {
			line[x] = FBCON_BG;
		}
	}
}

static void
fbcon_newline(void)
{
	fbcon_col = 0;
	fbcon_row = (fbcon_row + 1) % fbcon_rows;
	fbcon_clear_row(fbcon_row);
	fbcon_clear_row((fbcon_row + 1) % fbcon_rows);
}

static void
fbcon_init(void)
{
	fbcon_cols = fbcon_width / (8 * FBCON_SCALE);
	fbcon_rows = fbcon_height / (16 * FBCON_SCALE);
	fbcon_col = fbcon_row = 0;
	for (uint32_t r = 0; r < fbcon_rows; r++) {
		fbcon_clear_row(r);
	}
}

static unsigned int
fbcon_transmit_ready(void)
{
	return 1;
}

static void
fbcon_transmit_data(uint8_t c)
{
	if (c == '\r') {
		fbcon_col = 0;
		return;
	}
	if (c == '\n') {
		fbcon_newline();
		return;
	}
	if (fbcon_col >= fbcon_cols) {
		fbcon_newline();
	}
	const unsigned char *glyph = iso_font + c * 16;
	uint32_t x0 = fbcon_col * 8 * FBCON_SCALE, y0 = fbcon_row * 16 * FBCON_SCALE;
	for (uint32_t gy = 0; gy < 16; gy++) {
		for (uint32_t sy = 0; sy < FBCON_SCALE; sy++) {
			volatile uint32_t *line = fbcon_base + (y0 + gy * FBCON_SCALE + sy) * (fbcon_stride / 4) + x0;
			for (uint32_t gx = 0; gx < 8; gx++) {
				uint32_t px = (glyph[gy] & (1u << gx)) ? FBCON_FG : FBCON_BG;
				for (uint32_t sx = 0; sx < FBCON_SCALE; sx++) {
					line[gx * FBCON_SCALE + sx] = px;
				}
			}
		}
	}
	fbcon_col++;
}

/* _cnputc routes normal console writes here; direct kprintf already uses
 * this same serial renderer. No console selector is changed after lockdown. */
int
sweet_fbcon_strip_putchar(uint8_t c)
{
    if(!ziyi_strip_enabled || fbcon_base==NULL)return 0;
    fbcon_transmit_data(c);
    return 1;
}


void attach(uint32_t *pixels){
 fbcon_base=pixels+(ZS_TOP*ZS_STRIDE/4);fbcon_width=ZS_WIDTH;fbcon_height=ZS_PIXELS;fbcon_stride=ZS_STRIDE;
 ziyi_strip_enabled=1;for(unsigned i=0;i<4096;i++)iso_font[i]=(uint8_t)i;fbcon_init();(void)fbcon_transmit_ready();
}
uint32_t geometry(uint32_t r,uint64_t p,uint32_t w,uint32_t h,uint32_t s){return zs_strip_pixels(r,p,w,h,s);}
int region(uint32_t r,uint64_t p,uint64_t z,uint32_t w,uint32_t h,uint32_t s){return zs_console_region(r,p,z,w,h,s);}

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void){
 const unsigned n=1080*2400;uint32_t *p=malloc((n+32)*4);assert(p);memset(p,0x5a,(n+32)*4);
 assert(geometry(11,ZS_PHYS,1080,2400,4320)==352);
 assert(!geometry(10,ZS_PHYS,1080,2400,4320));assert(!geometry(11,ZS_PHYS+4,1080,2400,4320));
 assert(region(11,ZS_BASE,ZS_BYTES,1080,352,4320));assert(!region(11,ZS_BASE,ZS_BYTES-1,1080,352,4320));
 attach(p+16);for(unsigned i=0;i<20000;i++)assert(sweet_fbcon_strip_putchar(i%131==130?10:(33+i%90)));
 for(unsigned i=0;i<16+1080*2048;i++)assert(p[i]==0x5a5a5a5a);
 for(unsigned i=n+16;i<n+32;i++)assert(p[i]==0x5a5a5a5a);
 free(p);puts("U11_NATIVE_CONSOLE_STRIP_PASS chars=20000 upperRows=2048 guards=PASS");return 0;
}
