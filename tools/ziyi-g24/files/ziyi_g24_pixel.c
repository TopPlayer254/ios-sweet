/* SPDX-License-Identifier: GPL-2.0-only
 * CPU-only bounded A6xx solid-fill packet builder. No register access, DMA,
 * ring publication, cache conversion, or fake rendering fallback.
 * Mesa 25.0.7 fd6_blitter + a6xx.xml; Umi ring_fill (source pins in README).
 */
#include "ziyi_g24_pixel.h"
static uint32_t g24pix_parity(uint32_t n){
 /* A6xx headers have odd parity over the field and its parity bit. */
 uint32_t p=1;while(n){p^=n&1u;n>>=1;}return p;
}
static uint32_t g24pix_pkt7(uint32_t op,uint32_t n){return 0x70000000u|n|(g24pix_parity(n)<<15)|(op<<16)|(g24pix_parity(op)<<23);}
static uint32_t g24pix_pkt4(uint32_t reg){return 0x40000001u|(g24pix_parity(1)<<7)|(reg<<8)|(g24pix_parity(reg)<<27);}
static void g24pix_reg(uint32_t*b,size_t*n,uint32_t r,uint32_t v){b[(*n)++]=g24pix_pkt4(r);b[(*n)++]=v;}
static int g24pix_valid_page(const struct gp_page*p,uint32_t epoch){
 return p->iova&&!(p->iova&(GP_PAGE-1))&&p->iova<=UINT32_MAX-GP_PAGE&&
  p->bytes==GP_PAGE&&p->epoch==epoch&&p->private_owned==1;
}
static int g24pix_valid(const struct gp_request*r){
 return r&&r->epoch&&r->color_token&&r->flush_token&&r->color_token!=r->flush_token&&
  g24pix_valid_page(&r->ring,r->epoch)&&g24pix_valid_page(&r->target,r->epoch)&&g24pix_valid_page(&r->fence,r->epoch)&&
  r->ring.iova!=r->target.iova&&r->ring.iova!=r->fence.iova&&r->target.iova!=r->fence.iova;
}
static void g24pix_band(uint32_t*b,size_t*n,const struct gp_request*r,uint32_t top,uint32_t bottom,uint32_t red,uint32_t blue){
 g24pix_reg(b,n,0x8405,top<<16);g24pix_reg(b,n,0x8406,(GP_WIDTH-1)|((bottom-1)<<16));
 g24pix_reg(b,n,0x8c2c,red);g24pix_reg(b,n,0x8c2d,128);g24pix_reg(b,n,0x8c2e,blue);g24pix_reg(b,n,0x8c2f,255);
 g24pix_reg(b,n,0x8c00,0x10f03080);g24pix_reg(b,n,0x8400,0x10f03080);
 g24pix_reg(b,n,0xacc0,0xf181);g24pix_reg(b,n,0x8c01,0);g24pix_reg(b,n,0x8c17,0x430);
 g24pix_reg(b,n,0x8c18,r->target.iova);g24pix_reg(b,n,0x8c19,0);g24pix_reg(b,n,0x8c1a,GP_PITCH>>6);
 b[(*n)++]=g24pix_pkt7(0x26,0);g24pix_reg(b,n,0x8e04,0x04100000);
 b[(*n)++]=g24pix_pkt7(0x2c,1);b[(*n)++]=3;
 b[(*n)++]=g24pix_pkt7(0x26,0);g24pix_reg(b,n,0x8e04,0);
}
static void g24pix_event(uint32_t*b,size_t*n,uint32_t id,uint32_t address,uint32_t token){
 b[(*n)++]=g24pix_pkt7(0x46,4);b[(*n)++]=id;b[(*n)++]=address;b[(*n)++]=0;b[(*n)++]=token;
}
int gp_build(uint32_t*out,size_t capacity,const struct gp_request*r,size_t*words){
 if(!out||!words||capacity<GP_WORDS)return GP_BAD_ARGUMENT;
 if(!g24pix_valid(r))return GP_OWNERSHIP;
 /* Preflight is complete. Invalid inputs leave both outputs untouched. */
 size_t n=0;out[n++]=g24pix_pkt7(0x65,1);out[n++]=12;
 g24pix_reg(out,&n,0x8e07,0x18000000); /* FD644/A662 has 3 CCUs, 64 KiB each. */
 g24pix_band(out,&n,r,0,32,255,0);g24pix_band(out,&n,r,32,64,0,255);
 g24pix_event(out,&n,0x1d,r->fence.iova,r->color_token);
 g24pix_event(out,&n,4,r->fence.iova+4,r->flush_token);
 out[n++]=g24pix_pkt7(0x26,0);*words=n;return GP_OK;
}
int gp_submit_allowed(const struct gp_request*r,const struct gp_observation*o){
 if(!g24pix_valid(r)||!o)return GP_BAD_ARGUMENT;
 if(o->epoch!=r->epoch||o->proof_flags!=GP_NATIVE_ALL)return GP_STALE;
 if(o->cp_fault||o->smmu_fault||o->gpu_busy)return GP_FAULT;
 return GP_OK;
}
int gp_verify(const struct gp_request*r,const struct gp_observation*o,
 const volatile uint8_t*p,size_t bytes,const uint8_t*before,
 const volatile uint8_t*after,size_t guards,uint32_t*bad){
 if(!p||bytes!=GP_PAGE||!before||!after||guards!=64||!bad)return GP_BAD_ARGUMENT;
 int result=gp_submit_allowed(r,o);if(result)return result;
 if(o->rptr!=GP_WORDS||o->wptr!=GP_WORDS||o->color_token!=r->color_token||o->flush_token!=r->flush_token)return GP_STALE;
 for(size_t i=0;i<guards;i++)if(before[i]!=after[i])return GP_GUARD;
 for(uint32_t i=0;i<GP_PAGE;i++){
  uint32_t c=i&3u;uint8_t expected=c==1?128:c==3?255:c==0?(i<GP_PAGE/2?0:255):(i<GP_PAGE/2?255:0);
  if(p[i]!=expected){*bad=i;return GP_PIXEL;}
 }
 *bad=UINT32_MAX;return GP_OK;
}
