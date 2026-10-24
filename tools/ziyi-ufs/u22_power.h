/* Ziyi RPMh active votes. Exact CMDDB identities and inherited idle DRV2
 * are checked before any write. No arbitrary resource/voltage API, PMIC
 * direct command, sleep/wake TCS modification, IRQ mask or media change. */
#ifndef ZIYI_U22_POWER_H
#define ZIYI_U22_POWER_H
#include <stdint.h>
enum { ZP_DB,ZP_RSC };
struct zp_io {void *ctx;uint32_t (*read)(void *,unsigned,uint32_t);void (*write)(void *,unsigned,uint32_t,uint32_t);void (*wait)(void *,unsigned);};
struct zp_state {uint32_t saved[16][3],status[6],response[6];unsigned qualified,steps,poison,phase,error,polls,fresh;uint32_t before,irq,control,tcs_status,enable,command,response_word,address_word,data_word;};
#define ZP_SLOT 0xfa0u /* actual DT d00 + active TCS1 stride2a0 */
static inline uint32_t zp_rd(struct zp_io *io,unsigned b,uint32_t o){return io->read(io->ctx,b,o);}
static inline void zp_wr(struct zp_io *io,uint32_t o,uint32_t v){io->write(io->ctx,ZP_RSC,o,v);}
static inline int zp_sync(struct zp_io *io,uint32_t o,uint32_t v){zp_wr(io,o,v);return zp_rd(io,ZP_RSC,o)!=v;}
static inline int zp_database(struct zp_io *io) {
 uint32_t header[36],second[36],found[2][7]={{0}};unsigned mask=0;
 for(unsigned pass=0;pass<2;pass++) {
  uint32_t *h=pass?second:header;for(unsigned i=0;i<36;i++)h[i]=zp_rd(io,ZP_DB,4*i);
  if(h[0]!=1||h[1]!=0x0c0330db)return 1;
  unsigned resource=0,local=0;
  for(unsigned slave=0;slave<8;slave++) {
   uint32_t a=h[2+4*slave],b=h[3+4*slave];unsigned id=a&65535,headers=a>>16,data=b&65535,count=b>>16;
   if(id!=4)continue;
   if(++resource!=1||h[4+4*slave]!=0x100||count>512||(headers&3)||144u+headers>0x20000||count*24u>0x20000u-144u-headers)return 2;
   for(unsigned i=0;i<count;i++) {
    uint32_t w[6];for(unsigned k=0;k<6;k++)w[k]=zp_rd(io,ZP_DB,144u+headers+i*24u+k*4u);
    if(w[0]!=0x636f646c||(w[1]!=0x32&&w[1]!=0x3131))continue;
    unsigned which=w[1]==0x32?0:1;
    if(local&(1u<<which))return 3;
    local|=1u<<which;
    unsigned len=w[5]&65535,offset=w[5]>>16,at=144u+data+offset;
    if(len!=4||at>0x20000-4||(at&3)||w[2]||w[3]||w[4]!=(which?0x41300u:0x42100u))return 4;
    uint32_t aux=zp_rd(io,ZP_DB,at);
    if(aux!=(which?0x3001u:0x2701u))return 5;
    for(unsigned k=0;k<7;k++) {
     uint32_t value=k==6?aux:w[k];
     if(pass&&found[which][k]!=value)return 6;
     found[which][k]=value;
    }
   }
  }
  if(resource!=1||local!=3)return 7;
  if(pass)for(unsigned i=0;i<36;i++)if(header[i]!=second[i])return 8;
  mask|=local;
 }
 return mask==3?0:9;
}
static inline int zp_idle(struct zp_io *io) {
 if(zp_rd(io,ZP_RSC,4)!=0x04010100||zp_rd(io,ZP_RSC,0xc)!=0x800c8104)return 1;
 if(zp_rd(io,ZP_RSC,0xd00)!=1||zp_rd(io,ZP_RSC,0xd04))return 2;
 for(unsigned t=0;t<8;t++) {
  uint32_t at=0xd00+t*0x2a0;
  if(zp_rd(io,ZP_RSC,at+0x10)||zp_rd(io,ZP_RSC,at+0x14)||zp_rd(io,ZP_RSC,at+0x18)!=1||zp_rd(io,ZP_RSC,at+0x1c))return 3;
 }
 return 0;
}
static inline int zp_qualify(struct zp_io *io,struct zp_state *s) {
 if(!io||!s||!io->read||!io->write||!io->wait||s->qualified||s->poison)return 1;
 if(zp_database(io))return 2;
 for(unsigned pass=0;pass<2;pass++) {
  if(zp_idle(io))return 3;
  for(unsigned i=0;i<16;i++)for(unsigned k=0;k<3;k++) {
   uint32_t v=zp_rd(io,ZP_RSC,ZP_SLOT+0x30+i*0x14+k*4);
   /* Blank, disabled active slot only. Never replace an unknown old vote. */
   if(v)return 4;
   s->saved[i][k]=v;
  }
  if(!pass)io->wait(io->ctx,50);
 }
 s->qualified=1;return 0;
}
/* Read-only terminal evidence, including failures before any accepted ACK. */
static inline void zp_observe(struct zp_io *io,struct zp_state *s) {
 s->irq=zp_rd(io,ZP_RSC,0xd04);s->control=zp_rd(io,ZP_RSC,ZP_SLOT+0x14);
 s->tcs_status=zp_rd(io,ZP_RSC,ZP_SLOT+0x18);s->enable=zp_rd(io,ZP_RSC,ZP_SLOT+0x1c);
 s->command=zp_rd(io,ZP_RSC,ZP_SLOT+0x3c);s->response_word=zp_rd(io,ZP_RSC,ZP_SLOT+0x40);
 s->address_word=zp_rd(io,ZP_RSC,ZP_SLOT+0x34);s->data_word=zp_rd(io,ZP_RSC,ZP_SLOT+0x38);
}
static inline int zp_vote(struct zp_io *io,struct zp_state *s,unsigned step) {
 static const uint32_t address[]={0x42100,0x42108,0x42104,0x41300,0x41308,0x41304};
 static const uint32_t value[]={1800,7,1,2800,7,1};
 if(step>=6||!s->qualified||s->poison||s->steps!=step)return 1;
 s->phase=1;s->error=0;s->polls=0;s->fresh=0;
 if(zp_idle(io)){s->error=2;goto bad;}
 s->phase=2;
 if(zp_sync(io,ZP_SLOT+0x30,0x10108)||zp_sync(io,ZP_SLOT+0x34,address[step])||
    zp_sync(io,ZP_SLOT+0x38,value[step])||zp_sync(io,ZP_SLOT+0x1c,1)){s->error=3;goto bad;}
 /* Baseline is after staging, before trigger. A stale complete from the last
  * vote cannot qualify a new completion unless busy/complete-clear is seen. */
 s->before=zp_rd(io,ZP_RSC,ZP_SLOT+0x3c);
 s->fresh=(s->before&0x10100)==0;
 s->phase=3;
 if(zp_sync(io,ZP_SLOT+0x14,0)||zp_sync(io,ZP_SLOT+0x14,0x10000)){s->error=4;goto bad;}
 zp_wr(io,ZP_SLOT+0x14,0x1010000);
 s->phase=4;
 for(unsigned n=0;n<200000;n++) {
  uint32_t irq=zp_rd(io,ZP_RSC,0xd04),status=zp_rd(io,ZP_RSC,ZP_SLOT+0x3c);
  uint32_t idle=zp_rd(io,ZP_RSC,ZP_SLOT+0x18);s->polls=n+1;
  if(irq&~2u){s->error=5;goto bad;}
  if(idle==0||!(status&0x10000))s->fresh=1;
  /* IRQ_STATUS is not required for this interrupt-disabled slot. Pinned
   * rpmh-rsc fast-path likewise uses controller status with its IRQ disabled.
   * Require a fresh request and BOTH issued+complete, plus controller idle. */
  if(s->fresh&&(status&0x10100)==0x10100&&idle==1) {
   s->status[step]=status;s->response[step]=zp_rd(io,ZP_RSC,ZP_SLOT+0x40);
   zp_observe(io,s);s->phase=5;
   if(zp_sync(io,ZP_SLOT+0x14,0x10000)||zp_sync(io,ZP_SLOT+0x14,0)||zp_sync(io,ZP_SLOT+0x1c,0)){s->error=7;goto bad;}
   if(irq&2)zp_wr(io,0xd08,2); /* Only an observed own completion bit. */
   for(unsigned k=0;k<3;k++)if(zp_sync(io,ZP_SLOT+0x30+k*4,s->saved[0][k])){s->error=7;goto bad;}
   if(zp_idle(io)){s->error=7;goto bad;}
   s->steps++;return 0;
  }
  io->wait(io->ctx,1);
 }
 s->error=6;
bad:
 zp_observe(io,s);s->poison=1;return 3;
}
static inline int zp_start(struct zp_io *io,struct zp_state *s) {
 int e=zp_qualify(io,s);if(e)return e;
 for(unsigned step=0;step<6;step++) {
  e=zp_vote(io,s,step);if(e)return 10+(int)step*4+e;
  if(step==2)io->wait(io->ctx,20000);
 }
 io->wait(io->ctx,200000);return 0;
}
#endif
