/* Read-only HLOS GPI snapshot. Pinned DT: top0x900000,size0x60000,
 * EE offset -0x10000, static mask1,dynamic mask0x7e. No GPI command,
 * doorbell, interrupt ACK, ring memory, DMA pointer or SMMU write. */
#ifndef ZIYI_U14_GPI_H
#define ZIYI_U14_GPI_H
#include <stdint.h>
struct zg_io {void *context;uint32_t (*read)(void *,uint32_t);void (*wait)(void *,unsigned);};
struct zg_snapshot {uint32_t word[14][7];uint32_t signature;};
static inline int zg_quiet(struct zg_io *io,struct zg_snapshot *out) {
 static const uint32_t fields[]={0,4,0x10,0x14,0x18,0x1c,0x60};
 struct zg_snapshot second;unsigned error=0;
 if(!io||!out||!io->read||!io->wait)return 1;
 for(unsigned phase=0;phase<2;phase++) {
  struct zg_snapshot *s=phase?&second:out;
  for(unsigned n=0;n<7;n++)for(unsigned ch=0;ch<2;ch++)for(unsigned k=0;k<7;k++)
   s->word[n*2+ch][k]=io->read(io->context,0x10000+n*0x4000+ch*0x80+fields[k]);
  if(!phase)io->wait(io->context,50);
 }
 uint32_t tag=2166136261u;
 for(unsigned i=0;i<14;i++) {
  uint32_t cfg=out->word[i][0],state=(cfg>>20)&15;
  if(state!=0&&state!=1&&state!=3)error|=2;
  if(state&&((cfg>>24)!=16||(cfg&7)!=2))error|=4;
  for(unsigned k=0;k<7;k++) {
   uint32_t a=out->word[i][k];if(a!=second.word[i][k])error|=8;
   tag=(tag^a)*16777619u;
  }
 }
 out->signature=tag;return (int)error;
}
#endif
