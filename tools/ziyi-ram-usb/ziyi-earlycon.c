// SPDX-License-Identifier: GPL-2.0-only
/* Fixed ziyi RAM framebuffer console. No firmware, storage or hardware MMIO.
 * Mapping lifecycle follows Linux drivers/firmware/efi/earlycon.c, but this
 * console has no EFI dependency and uses only the measured ziyi geometry.
 */
#include <linux/console.h>
#include <linux/font.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/serial_core.h>
#include <asm/early_ioremap.h>

#define FB_BASE 0xb8000000ULL
#define FB_STRIDE 4320U
#define FB_HEIGHT 2400U
#define FB_BYTES (FB_STRIDE * FB_HEIGHT)
#define CELL_W 16U
#define CELL_H 32U
#define COLS (1080U / CELL_W)
#define ROWS (FB_HEIGHT / CELL_H)
static unsigned int column, row;
static void *mapped_fb;
static const struct console *boot_console;

static __ref void *map_row(unsigned int y)
{
    if (mapped_fb)
        return mapped_fb + y * CELL_H * FB_STRIDE;
    return early_memremap_prot(FB_BASE + y * CELL_H * FB_STRIDE,
        CELL_H * FB_STRIDE, pgprot_val(pgprot_writecombine(PAGE_KERNEL)));
}

static __ref void unmap_row(void *p)
{
    if (!mapped_fb)
        early_memunmap(p, CELL_H * FB_STRIDE);
}

static void write_fb(struct console *con, const char *s, unsigned int n)
{
    (void)con;
    while (n) {
        unsigned int count = 0, y, x, bit;
        u8 *p = map_row(row);
        if (!p)
            return;
        if (!column)
            memset(p, 0, CELL_H * FB_STRIDE);
        while (count < n && s[count] != '\n' && count < COLS - column)
            count++;
        for (y = 0; y < CELL_H; y++) {
            u32 *dst = (u32 *)(p + y * FB_STRIDE) + column * CELL_W;
            for (x = 0; x < count; x++) {
                u8 glyph = ((const u8 *)font_vga_8x16.data)[(unsigned char)s[x] * 16 + y / 2];
                for (bit = 0; bit < CELL_W; bit++)
                    *dst++ = glyph & (0x80 >> (bit / 2)) ? 0x00ffffff : 0;
            }
        }
        unmap_row(p);
        s += count;
        n -= count;
        column += count;
        if (n && *s == '\n') {
            s++;
            n--;
            column = 0;
            row = (row + 1) % ROWS;
        } else if (column == COLS) {
            column = 0;
            row = (row + 1) % ROWS;
        }
    }
}

static int __init remap_fb(void)
{
    if (!boot_console || !(boot_console->flags & CON_ENABLED))
        return 0;
    mapped_fb = memremap(FB_BASE, FB_BYTES, MEMREMAP_WC);
    return mapped_fb ? 0 : -ENOMEM;
}
early_initcall(remap_fb);

static int __init setup_fb(struct earlycon_device *device, const char *options)
{
    (void)options;
    device->con->write = write_fb;
    boot_console = device->con;
    return 0;
}
EARLYCON_DECLARE(ziyi_fb, setup_fb);
