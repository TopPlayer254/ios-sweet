/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g24_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct g24_workspace w;
static uint8_t ram[8*16384] __attribute__((aligned(64)));
struct model {uint32_t ctl[G24_CTL_COUNT],profile[G24_MAX_PROFILE];unsigned calls,fail,phase_fail,drop_grant,bad_pixel,bad_fence,bad_token,bad_guard,stale_grant,smc_fail,smc_interrupt,smc_count,zap,presents;uint64_t ticks;};
static struct model m;
static int step(struct model*x){return ++x->calls!=x->fail;}
static int qualified(void*c,uint64_t epoch,unsigned phase){struct model*x=c;return step(x)&&epoch==w.r.epoch&&phase!=x->phase_fail;}
static int rd(void*c,unsigned space,unsigned id,uint32_t*out){struct model*x=c;if(!step(x))return 0;assert(g24_register_address(space,id));*out=space==G24_CTL?x->ctl[id]:x->profile[id];return 1;}
static unsigned odd(uint32_t n){unsigned p=1;while(n){p^=n&1;n>>=1;}return p;}
static void consume(struct model*x,uint32_t end){
 uint32_t reg[0xc000]={0};uint32_t at=x->ctl[G24_RPTR];
 while(at<end){uint32_t h=w.ring[at++];unsigned type=h>>28;
  if(type==4){unsigned count=h&0x7f,addr=h>>8&0x3ffff;assert(count==1&&at<end&&addr<0xc000);assert((h>>7&1)==odd(count)&&(h>>27&1)==odd(addr));reg[addr]=w.ring[at++];continue;}
  assert(type==7);unsigned op=h>>16&0x7f,count=h&0x7fff;assert((h>>15&1)==odd(count)&&(h>>23&1)==odd(op)&&count<=end-at);
  if(op==0x48){assert(count==8&&w.ring[at]==0x2f&&w.ring[at+1]==3&&w.ring[at+2]==0x20000000);}
  else if(op==0x66){assert(count==1&&w.ring[at]==0&&w.r.pas_authenticated);}
  else if(op==0x3d){assert(count==3&&w.ring[at]==w.memory.fence_iova+8&&!w.ring[at+1]);if(!x->bad_fence)w.fence[2]=w.ring[at+2];}
  else if(op==0x46){assert(count==4&&w.ring[at+2]==0);unsigned offset=w.ring[at+1]-w.memory.fence_iova;assert((offset==0||offset==4)&&(w.ring[at]==0x1d||w.ring[at]==4));if(!x->bad_token)w.fence[offset/4]=w.ring[at+3];}
  else if(op==0x2c){assert(count==1&&w.ring[at]==3&&reg[0x8c18]==w.memory.pixel_iova&&reg[0x8c19]==0&&(reg[0x8c1a]<<6)==256);unsigned x1=reg[0x8405]&65535,y1=reg[0x8405]>>16,x2=(reg[0x8406]&65535)+1,y2=(reg[0x8406]>>16)+1;assert(x1<x2&&x2<=64&&y1<y2&&y2<=64);for(unsigned y=y1;y<y2;y++)for(unsigned k=x1;k<x2;k++){unsigned off=y*256+4*k;w.pixels[off]=(uint8_t)reg[0x8c2e];w.pixels[off+1]=(uint8_t)reg[0x8c2d];w.pixels[off+2]=(uint8_t)reg[0x8c2c];w.pixels[off+3]=(uint8_t)reg[0x8c2f];}}
  else assert(op==0x26||op==0x65);
  at+=count;
 }
 assert(at==end);if(x->bad_pixel&&w.r.phase==G24_BLIT)w.pixels[8191]^=1;if(x->bad_guard&&w.r.phase==G24_BLIT)((volatile uint8_t*)w.fence)[150]^=1;x->ctl[G24_RPTR]=end;
}
static int wr(void*c,unsigned space,unsigned id,uint32_t value){
 struct model*x=c;if(!step(x))return 0;uint32_t current=space==G24_CTL?x->ctl[id]:x->profile[id];assert(g24_store_allowed(&w,space,id,value,current));
 if(space==G24_PROFILE){uint64_t address=g24_register_address(space,id);for(unsigned i=0;i<g24_profile_count();i++)if(g24_register_address(space,i)==address)x->profile[i]=value;return 1;}
 if(id==G24_REQUEST){if(!x->drop_grant){x->ctl[G24_INFO]|=0x80000000;x->ctl[G24_GDSC]=0x80000000;x->ctl[G24_SPTPRAC]=0;}return 1;}
 if(id==G24_ACK){x->ctl[G24_INFO]&=~value;return 1;}
 x->ctl[id]=value;if(id==G24_WPTR)consume(x,value);return 1;
}
static int order(void*c){return step(c);}
static int delay(void*c,uint32_t us){struct model*x=c;if(!step(x))return 0;assert(us<=100);x->ticks+=(uint64_t)us*192/10;return 1;}
static int now(void*c,uint64_t*out){struct model*x=c;if(!step(x))return 0;x->ticks+=100;*out=x->ticks;return 1;}
static int pas_qualified(void*c,uint64_t epoch,unsigned op){struct model*x=c;return step(x)&&epoch==w.r.epoch&&op<=G24P_START&&w.r.phase==G24_ZAP&&w.r.phase!=x->phase_fail;}
static int smc(void*c,uint64_t regs[8]){struct model*x=c;if(!step(x))return 0;x->smc_count++;if(x->smc_fail){regs[0]=x->smc_fail;return 1;}if(x->smc_interrupt&&(x->smc_count%2)){regs[0]=1;regs[6]=0x1234;return 1;}uint32_t fn=(uint32_t)regs[0]&65535;assert(fn==0x601||fn==0x201||fn==0x202||fn==0x205||regs[0]==1);if(fn!=0x601&&regs[0]!=1)assert(regs[2]==13);if(fn==0x201)assert(regs[3]==w.pas.metadata_pa&&regs[1]==130);if(fn==0x202)assert(regs[3]==G24_ZAP_PA&&regs[4]==4096);regs[0]=0;regs[1]=fn==0x601?1:0;return 1;}
static int zap(void*c){struct model*x=c;if(!step(x))return 0;assert(w.pas.state==G24P_MEMORY&&w.pas.image_retained);x->zap=1;return 1;}
static int present(void*c){struct model*x=c;if(!step(x))return 0;assert(w.r.phase==G24_PRESENT&&w.r.verified_pixels==4096&&w.r.proofs==GP_NATIVE_ALL&&x->zap&&w.r.pas_authenticated);x->presents++;return 1;}
static void reset(void){memset(&w,0,sizeof(w));memset(&m,0,sizeof(m));memset(ram,0,sizeof(ram));m.phase_fail=99;m.ticks=1000000;w.r.epoch=0x0123456789abcdefULL;w.r.phase=G24_READY;struct gc_request q={.epoch=w.r.epoch,.hfi_pa=0xd0000000,.pages=7,.normal_nc=1,.private_owned=1};for(unsigned i=0;i<7;i++)q.pa[i]=0xc0000000+0x4000*i;assert(gc_build(&w.memory,&q)==GC_OK);w.ring=(volatile uint32_t*)(ram+16384);w.pixels=ram+2*16384;w.fence=(volatile uint32_t*)(ram+3*16384);w.pas.metadata_pa=0xc001c000;for(unsigned i=0;i<64;i++)w.guard[i]=((volatile uint8_t*)w.fence)[128+i]=(uint8_t)(0x55^i);memset((void*)w.pixels,0x55,16384);w.pixel_request=(struct gp_request){.epoch=0x89abcdef,.color_token=0x12345678,.flush_token=0x87654321,.ring={w.memory.ring_iova,16384,0x89abcdef,1},.target={w.memory.pixel_iova,16384,0x89abcdef,1},.fence={w.memory.fence_iova,16384,0x89abcdef,1}};m.ctl[G24_S2CR]=0x20000;m.ctl[G24_FSR]=0x400;m.ctl[G24_GMU_SMR]=0x84000005;m.ctl[G24_GMU_S2CR]=0x302;m.ctl[G24_GMU_SCTLR]=0xc00027;m.ctl[G24_FW]=0x100;m.ctl[G24_HFI]=1;m.ctl[G24_SPTPRAC]=0xc0;}
static struct g24_io io(void){return (struct g24_io){&m,qualified,rd,wr,order,delay,now,zap,present,{&m,pas_qualified,smc}};}
int main(void){reset();struct g24_io o=io();assert(g24_run(&w,&o));assert(w.r.phase==G24_DONE&&!w.r.error&&w.r.presented&&w.r.verified_pixels==4096&&w.r.proofs==127&&w.r.ring_words==103&&m.presents==1);unsigned calls=m.calls;printf("MODEL_SUCCESS calls%u stores%u reads%u\n",calls,w.r.writes,w.r.reads);
 for(unsigned i=1;i<=calls;i++){reset();m.fail=i;o=io();int ok=g24_run(&w,&o);assert(!ok||i==calls);if(!ok)assert(!m.presents||w.r.verified_pixels==4096);}
 for(unsigned phase=G24_POWER;phase<=G24_VERIFY;phase++){reset();m.phase_fail=phase;o=io();assert(!g24_run(&w,&o)&&!w.r.presented);}
 for(unsigned fault=0;fault<7;fault++){reset();if(fault==0)m.drop_grant=1;if(fault==1)m.bad_pixel=1;if(fault==2)m.bad_fence=1;if(fault==3)m.bad_token=1;if(fault==4)m.bad_guard=1;if(fault==5)m.smc_fail=2;if(fault==6)m.ctl[G24_INFO]=0x80000000;o=io();assert(!g24_run(&w,&o)&&!w.r.presented);}
 printf("G24_ACTUAL_ENGINE_MODEL_PASS failures%u phases%u guards7 syntheticOnly1 native0\n",calls,G24_VERIFY-G24_POWER+1);return 0;}
