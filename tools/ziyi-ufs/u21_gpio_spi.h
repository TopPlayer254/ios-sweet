/* Mode0 GPIO SPI: MISO0, MOSI1, SCLK2, CS3, reset126; native TCM identity
 * remains mandatory. Uses no FIFO, DMA, clock-controller or SMMU command. */
#ifndef ZIYI_U18_GPIO_SPI_H
#define ZIYI_U18_GPIO_SPI_H
#include <stdint.h>
#include <stddef.h>
enum { BB_SE, BB_GPIO };
struct bb_io {
 void *ctx;
 uint32_t (*read)(void *,unsigned,uint32_t);
 void (*write)(void *,unsigned,uint32_t,uint32_t);
 void (*wait)(void *,unsigned);
 int (*quiet)(void *);
};
struct bb_state {
 uint32_t cfg[4],out[4],reset_cfg,reset_out,gsi,dma;
 unsigned saved,closed,latches,pins,reset,poison;
};
static inline uint32_t bb_rd(struct bb_io *io,unsigned b,uint32_t o){return io->read(io->ctx,b,o);}
static inline void bb_wr(struct bb_io *io,unsigned b,uint32_t o,uint32_t v){io->write(io->ctx,b,o,v);}
static inline int bb_idle(struct bb_io *io) {
 if(bb_rd(io,BB_SE,0x40)&0x1001)return 1;
 if(bb_rd(io,BB_SE,0x258)&~1u)return 2;
 if(bb_rd(io,BB_SE,0xe18)&~15u)return 3;
 if(!io->quiet(io->ctx))return 4;
 return 0;
}
static inline int bb_level(struct bb_io *io,struct bb_state *s,unsigned pin,unsigned high) {
 if(pin<1||pin>3||high>1)return 1;
 uint32_t v=(s->out[pin]&~2u)|(high<<1);
 bb_wr(io,BB_GPIO,pin*0x1000+4,v);
 return (bb_rd(io,BB_GPIO,pin*0x1000+4)&2)!=(high<<1);
}
static inline int bb_take(struct bb_io *io,struct bb_state *s) {
 if(!io||!s||!io->read||!io->write||!io->wait||!io->quiet||s->saved)return 1;
 if(bb_idle(io))return 2;
 for(unsigned i=0;i<4;i++) {
  uint32_t c=bb_rd(io,BB_GPIO,i*0x1000);
  if(((c>>2)&15)>1)return 3;
  s->cfg[i]=c;s->out[i]=bb_rd(io,BB_GPIO,i*0x1000+4);
 }
 s->reset_cfg=bb_rd(io,BB_GPIO,126*0x1000);
 s->reset_out=bb_rd(io,BB_GPIO,126*0x1000+4);
 if((s->reset_cfg&0x3c)||(bb_rd(io,BB_GPIO,51*0x1000)&0x23c))return 4;
 s->gsi=bb_rd(io,BB_SE,0xe18);s->dma=bb_rd(io,BB_SE,0x258);s->saved=1;
 s->closed=1;bb_wr(io,BB_SE,0xe18,0);bb_wr(io,BB_SE,0x258,0);
 if(bb_rd(io,BB_SE,0xe18)||bb_rd(io,BB_SE,0x258)||(bb_rd(io,BB_SE,0x40)&0x1001))return 5;
 /* Preload output latches before switching their mux/direction. MISO never
  * becomes an output, and its output latch is never written. */
 s->latches=1;
 if(bb_level(io,s,3,1)||bb_level(io,s,2,0)||bb_level(io,s,1,1))return 6;
 s->pins=1;
 for(unsigned i=0;i<4;i++) {
  uint32_t c=(s->cfg[i]&~0x3ffu)|(i?0x200u:0);
  bb_wr(io,BB_GPIO,i*0x1000,c);
  if(bb_rd(io,BB_GPIO,i*0x1000)!=c)return 7;
 }
 s->reset=1;bb_wr(io,BB_GPIO,126*0x1000+4,s->reset_out|2u);
 bb_wr(io,BB_GPIO,126*0x1000,(s->reset_cfg&~0x3ffu)|0x200u);
 if(!(bb_rd(io,BB_GPIO,126*0x1000+4)&2)||(bb_rd(io,BB_GPIO,126*0x1000)&0x3ff)!=0x200)return 8;
 io->wait(io->ctx,200000);return 0;
}
static inline int bb_release(struct bb_io *io,struct bb_state *s) {
 if(!s->saved)return 0;
 if(s->poison||bb_idle(io))return 1;
 if(s->pins&&(bb_level(io,s,3,1)||bb_level(io,s,2,0))){s->poison=1;return 2;}
 if(s->reset) {
  bb_wr(io,BB_GPIO,126*0x1000+4,s->reset_out);
  bb_wr(io,BB_GPIO,126*0x1000,s->reset_cfg);
 }
 if(s->pins) {
  /* Keep CS high while relinquishing clock/data. Restore inactive output
   * latches as inputs; release CS mux last, before its inherited latch. */
  for(unsigned i=0;i<3;i++) {
   if(i){bb_wr(io,BB_GPIO,i*0x1000,s->cfg[i]&~0x23cu);bb_wr(io,BB_GPIO,i*0x1000+4,s->out[i]);}
   bb_wr(io,BB_GPIO,i*0x1000,s->cfg[i]);
  }
  bb_wr(io,BB_GPIO,3*0x1000,s->cfg[3]);
  bb_wr(io,BB_GPIO,3*0x1000+4,s->out[3]);
 } else if(s->latches)for(unsigned i=1;i<4;i++)bb_wr(io,BB_GPIO,i*0x1000+4,s->out[i]);
 for(unsigned i=0;i<4;i++)if(bb_rd(io,BB_GPIO,i*0x1000)!=s->cfg[i] ||
   (i&&(bb_rd(io,BB_GPIO,i*0x1000+4)&2)!=(s->out[i]&2))){s->poison=1;return 4;}
 if(s->reset&&(bb_rd(io,BB_GPIO,126*0x1000)!=s->reset_cfg ||
    (bb_rd(io,BB_GPIO,126*0x1000+4)&2)!=(s->reset_out&2))){s->poison=1;return 5;}
 /* Recheck quiescence immediately before restoring the inherited route. */
 if(bb_idle(io)){s->poison=1;return 3;}
 bb_wr(io,BB_SE,0x258,s->dma);bb_wr(io,BB_SE,0xe18,s->gsi);
 if(bb_rd(io,BB_SE,0x258)!=s->dma||bb_rd(io,BB_SE,0xe18)!=s->gsi){s->poison=1;return 6;}
 s->saved=s->closed=s->latches=s->pins=s->reset=0;return 0;
}
static inline int bb_transfer(struct bb_io *io,struct bb_state *s,const uint8_t *tx,uint8_t *rx,size_t n) {
 if(!s->pins||s->poison||!n||n>4099)return 1;
 if(bb_level(io,s,2,0)||bb_level(io,s,3,0))goto bad;
 io->wait(io->ctx,2);
 for(size_t i=0;i<n;i++) {
  uint8_t send=tx?tx[i]:255,read=0;
  for(unsigned b=0;b<8;b++) {
   if(bb_level(io,s,1,(send>>(7-b))&1))goto bad;
   io->wait(io->ctx,2);
   if(bb_level(io,s,2,1))goto bad;
   io->wait(io->ctx,2);
   read=(uint8_t)((read<<1)|(bb_rd(io,BB_GPIO,4)&1));
   if(bb_level(io,s,2,0))goto bad;
  }
  if(rx)rx[i]=read;
 }
 io->wait(io->ctx,2);
 if(bb_level(io,s,3,1))goto bad;
 io->wait(io->ctx,2);return 0;
bad:
 /* No further clock or data pulses after a failed latch readback. */
 s->poison=1;(void)bb_level(io,s,3,1);return 2;
}
#endif
