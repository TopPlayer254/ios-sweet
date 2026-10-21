// SPDX-License-Identifier: GPL-2.0-only
/* Fixed ziyi RAM framebuffer console. No firmware, storage or hardware MMIO.
 * Mapping lifecycle follows Linux drivers/firmware/efi/earlycon.c, but this
 * console has no EFI dependency and uses only the measured ziyi geometry.
 */
#include <linux/console.h>
#include <linux/font.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/serial_core.h>
#include <linux/string.h>
#include <asm/early_ioremap.h>

/* ABL may select vendor_boot's DTB. This explicit, opt-in kernel parameter
 * selects our RAM profile independently of appended-DTB selection. Keep
 * state outside initdata: regulator late work runs after free_initmem(). */
static bool ram_usb_profile;
static int __init setup_ram_usb_profile(char *value)
{
    if (!value || strcmp(value, "1"))
        return 0;
    ram_usb_profile = true;
    pr_info("ZIYI RAM PROFILE: explicit command-line selector enabled\n");
    return 1;
}
__setup("ziyi_ram_usb=", setup_ram_usb_profile);

bool ziyi_ram_usb_mode(void)
{
    return ram_usb_profile;
}

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
static struct console *boot_console;
static bool early_mapping_live = true;

static __ref void *map_row(unsigned int y)
{
    if (mapped_fb)
        return mapped_fb + y * CELL_H * FB_STRIDE;
    if (!early_mapping_live)
        return NULL;
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
    pr_info("ZIYI RAM FB permanent WC mapping: %s\n", mapped_fb ? "READY" : "FAILED");
    return mapped_fb ? 0 : -ENOMEM;
}
early_initcall(remap_fb);

/* Never call __init early_memremap code after free_initmem(), even if the
 * persistent map failed. A plain memreserve does not exclude RAM from the
 * linear map; the boot DTB must also declare our framebuffer no-map.
 */
static int __init finish_early_mapping(void)
{
    if (mapped_fb) {
        pr_info("ZIYI RAM FB persistent console ready before initmem release\n");
    } else {
        pr_err("ZIYI RAM FB STOP: no persistent map; disabling boot console safely\n");
        early_mapping_live = false;
        if (boot_console)
            unregister_console(boot_console);
    }
    return 0;
}
late_initcall_sync(finish_early_mapping);

static int __init setup_fb(struct earlycon_device *device, const char *options)
{
    (void)options;
    device->con->write = write_fb;
    boot_console = device->con;
    return 0;
}
EARLYCON_DECLARE(ziyi_fb, setup_fb);
