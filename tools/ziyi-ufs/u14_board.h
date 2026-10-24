/* ziyi/Diwali SPI0 bring-up, based on the pinned Xiaomi clock/pinctrl/GENI
 * drivers and U12's actual idle handoff. No DMA addresses or GPI writes, PMIC writes,
 * firmware upload, shared clock reset, or persistent storage operation.
 * The callbacks make the exact hardware sequence testable on a host. */
#ifndef ZIYI_U13_BOARD_H
#define ZIYI_U13_BOARD_H
#include <stdint.h>
#include <stddef.h>
enum { ZB_SE, ZB_GCC, ZB_GPIO };
struct zb_io {
 void *context;
 uint32_t (*read)(void *,unsigned,uint32_t);
 void (*write)(void *,unsigned,uint32_t,uint32_t);
 void (*wait)(void *,unsigned);
 int (*quiescent_events)(void *);
};
struct zb_state {
 uint32_t vote,root,config,clock,select,pin[4],reset_cfg,reset_io;
 uint32_t gsi,dma;
 unsigned saved,clocks,pins,reset,closed;
};
#define ZB_VOTES 0x7c0u /* wrap0 m/s AHB, core/core2x, SE0 only */
static inline uint32_t zb_read(struct zb_io *io,unsigned b,uint32_t o) {
 return io->read(io->context,b,o);
}
static inline void zb_write(struct zb_io *io,unsigned b,uint32_t o,uint32_t v) {
 io->write(io->context,b,o,v);
}
static inline int zb_poll(struct zb_io *io,unsigned b,uint32_t o,uint32_t mask,uint32_t value) {
 for(unsigned n=0;n<10000;n++) {
  if((zb_read(io,b,o)&mask)==value)return 0;
  io->wait(io->context,1);
 }
 return 1;
}
/* Reject a live engine or foreign pin owner before the first write. DMA-mode
 * enabled is not an active transfer: it can be changed only with both GENIs
 * idle, no GSI event route and empty FIFOs. U12 measured this exact state. */
static inline int zb_qualify(struct zb_io *io) {
 if(((zb_read(io,ZB_SE,0x68)>>8)&255)!=1)return 1;
 if(zb_read(io,ZB_SE,0x40)&0x1001)return 2;
 if(zb_read(io,ZB_SE,0xe18)&&(!io->quiescent_events||!io->quiescent_events(io->context)))return 3;
 if(zb_read(io,ZB_SE,0x800)||(zb_read(io,ZB_SE,0x804)&0x1ffffff))return 4;
 if(zb_read(io,ZB_SE,0x258)&~1u)return 5;
 if(zb_read(io,ZB_GCC,0x27014)&1)return 6;
 for(unsigned i=0;i<4;i++)if(((zb_read(io,ZB_GPIO,i*0x1000)>>2)&15)>1)return 7;
 if(zb_read(io,ZB_GPIO,126*0x1000)&0x3c)return 8;
 if(zb_read(io,ZB_GPIO,51*0x1000)&0x23c)return 9;
 return 0;
}
/* The pinned frequency table begins7.3728,14.7456,19.2MHz. Linux
 * programs the matching table index into GENI DFS even in FIFO mode. */
static inline uint32_t zb_spi_rate(uint32_t cmd,uint32_t cfg,uint32_t sel,uint32_t clk) {
 return !(cmd&0x80000001u)&&(cfg&0x371fu)==1u&&sel==2&&clk==0x41?4800000u:0;
}
static inline int zb_start(struct zb_io *io,struct zb_state *s) {
 if(!io||!s||!io->read||!io->write||!io->wait||s->saved)return 20;
 int e=zb_qualify(io);if(e)return e;
 s->vote=zb_read(io,ZB_GCC,0x62008);s->root=zb_read(io,ZB_GCC,0x27014);
 s->config=zb_read(io,ZB_GCC,0x27018);s->clock=zb_read(io,ZB_SE,0x48);
 s->select=zb_read(io,ZB_SE,0x7c);
 for(unsigned i=0;i<4;i++)s->pin[i]=zb_read(io,ZB_GPIO,i*0x1000);
 s->reset_cfg=zb_read(io,ZB_GPIO,126*0x1000);s->reset_io=zb_read(io,ZB_GPIO,126*0x1000+4);
 s->saved=1;s->gsi=zb_read(io,ZB_SE,0xe18);s->dma=zb_read(io,ZB_SE,0x258);
 s->closed=1;
 /* Primary GENI FIFO transition, only after repeated idle GPI proof. */
 zb_write(io,ZB_SE,0xe18,0);zb_write(io,ZB_SE,0x258,0);
 if(zb_read(io,ZB_SE,0xe18)||zb_read(io,ZB_SE,0x258)||(zb_read(io,ZB_SE,0x40)&0x1001))return 28;
 s->clocks=1;
 /* RMW the HLOS vote register; no clock used by another wrapper is removed. */
 zb_write(io,ZB_GCC,0x62008,zb_read(io,ZB_GCC,0x62008)|ZB_VOTES);
 if(zb_poll(io,ZB_GCC,0x27004,0x80000000,0)||
    zb_poll(io,ZB_GCC,0x27008,0x80000000,0)||
    zb_poll(io,ZB_GCC,0x33000,0x80000000,0)||
    zb_poll(io,ZB_GCC,0x3300c,0x80000000,0))return 21;
 /* Exact 19.2MHz TCXO entry: HID=1, source=0, MND bypass. No PLL changes.
  * GENI divide-by4 gives <=4.8MHz, below the DT 5MHz limit. */
 zb_write(io,ZB_GCC,0x27018,(s->config&~0x371fu)|1u);
 zb_write(io,ZB_GCC,0x27014,(zb_read(io,ZB_GCC,0x27014)&~1u)|3u);
 if(zb_poll(io,ZB_GCC,0x27014,0x80000001,0)||
    zb_poll(io,ZB_GCC,0x2700c,0x80000000,0))return 22;
 zb_write(io,ZB_SE,0x7c,2);zb_write(io,ZB_SE,0x48,0x41);
 if(zb_read(io,ZB_SE,0x7c)!=2||zb_read(io,ZB_SE,0x48)!=0x41)return 23;
 return 0;
}
/* Called only after the serial engine has FIFO mode, packing, mode0/CS0 and
 * default outputs set. Pins: DT function1,6mA,no bias. Reset126 is released
 * once, not pulsed; callers wait the DT200ms before reading TCM. */
static inline int zb_connect(struct zb_io *io,struct zb_state *s) {
 if(!s->saved||!s->clocks||s->pins)return 24;
 if(zb_read(io,ZB_SE,0x40)&0x1001||zb_read(io,ZB_SE,0x258)||zb_read(io,ZB_SE,0xe18))return 25;
 s->pins=1;
 for(unsigned i=0;i<4;i++) {
  uint32_t v=(s->pin[i]&~0x3ffu)|0x84u;
  zb_write(io,ZB_GPIO,i*0x1000,v);
  if(zb_read(io,ZB_GPIO,i*0x1000)!=v)return 26;
 }
 s->reset=1;
 zb_write(io,ZB_GPIO,126*0x1000+4,s->reset_io|2u);
 zb_write(io,ZB_GPIO,126*0x1000,(s->reset_cfg&~0x3ffu)|0x200u);
 if(!(zb_read(io,ZB_GPIO,126*0x1000+4)&2)||
    (zb_read(io,ZB_GPIO,126*0x1000)&0x3ff)!=0x200)return 27;
 return 0;
}
/* Restore only if the caller has proved its SPI command stopped. Keep
 * clocks/pins intact after failed abort; never turn off a live peripheral.
 * Shared vote restoration preserves all concurrently added external bits. */
static inline int zb_restore(struct zb_io *io,struct zb_state *s,int idle) {
 if(!s->saved)return 0;
 if(!idle||(zb_read(io,ZB_SE,0x40)&0x1001))return 30;
 if(s->reset) {
  zb_write(io,ZB_GPIO,126*0x1000,s->reset_cfg);
  zb_write(io,ZB_GPIO,126*0x1000+4,s->reset_io);
 }
 if(s->pins)for(unsigned i=0;i<4;i++)zb_write(io,ZB_GPIO,i*0x1000,s->pin[i]);
 if(s->clocks) {
  zb_write(io,ZB_SE,0x48,s->clock);zb_write(io,ZB_SE,0x7c,s->select);
  zb_write(io,ZB_GCC,0x27018,s->config);
  zb_write(io,ZB_GCC,0x27014,(zb_read(io,ZB_GCC,0x27014)&~1u)|3u);
  if(zb_poll(io,ZB_GCC,0x27014,1,0))return 31;
  uint32_t root=zb_read(io,ZB_GCC,0x27014);
  zb_write(io,ZB_GCC,0x27014,(root&~2u)|(s->root&2u));
  uint32_t added=ZB_VOTES&~s->vote;
  zb_write(io,ZB_GCC,0x62008,zb_read(io,ZB_GCC,0x62008)&~added);
 }
 if(s->closed) {
  if(s->gsi&&(!io->quiescent_events||!io->quiescent_events(io->context)))return 32;
  zb_write(io,ZB_SE,0x258,s->dma);zb_write(io,ZB_SE,0xe18,s->gsi);
 }
 s->saved=s->clocks=s->pins=s->reset=s->closed=0;
 return 0;
}
#endif
