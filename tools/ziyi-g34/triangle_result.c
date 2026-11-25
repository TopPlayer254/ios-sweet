/* SPDX-License-Identifier: GPL-2.0-only
 * Readback verification. This file never writes the GPU target or framebuffer.
 */
#include "triangle_result.h"
#include "final_wire.h"
int g34_triangle_covered(unsigned x,unsigned y){
 if(x>=64||y>=64)return 0;
 /* Pixel centres and clip-space vertices in doubled screen coordinates.
  * None of the three edges passes through a sample centre, so the oracle is
  * independent of top-left tie-breaking. No floating point in the kernel.
  */
 int px=(int)(2*x+1),py=(int)(2*y+1);
 return 64*(py-32)>0 && -32*(py-32)-64*(px-96)>0 && -32*(py-96)+64*(px-64)>0;
}
int g34_check_pixels(struct g34_result*r,const volatile uint8_t*p,size_t n){
 if(!r||!p||n!=16384||r->stage!=G34_PIXELS||r->triangle_pixels||r->background_pixels)return 0;
 r->first_bad_byte=UINT32_MAX;
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
  int covered=g34_triangle_covered(x,y);
  uint32_t expected=covered?0xffff00ffu:y<32?0xffff8000u:0xff0080ffu;
  for(unsigned b=0;b<4;b++)if(p[(y*64+x)*4+b]!=(uint8_t)(expected>>(8*b))){r->first_bad_byte=(y*64+x)*4+b;return 0;}
  if(covered)r->triangle_pixels++;else r->background_pixels++;
 }
 if(r->triangle_pixels!=512||r->background_pixels!=3584)return 0;
 r->pixel_crc=g34_crc((const void*)p,n);return 1;
}
static void g34r_put(uint8_t*p,unsigned n,uint32_t v){for(unsigned j=0;j<4;j++)p[n*4+j]=(uint8_t)(v>>(8*j));}
static uint32_t g34r_get(const uint8_t*p,unsigned n){uint32_t v=0;for(unsigned j=0;j<4;j++)v|=(uint32_t)p[n*4+j]<<(8*j);return v;}
int g34_result_valid(const uint8_t*p,size_t n){
 if(!p||n!=256||g34r_get(p,0)!=0x31525447u||g34r_get(p,1)!=1||g34r_get(p,2)!=n||
    g34r_get(p,3)!=g34_crc(p+16,n-16)||!(g34r_get(p,8)|g34r_get(p,9)))return 0;
 unsigned stage=g34r_get(p,4),proofs=g34r_get(p,7);
 if((stage>9&&stage!=15)||g34r_get(p,5)>9||g34r_get(p,6)>12||proofs>511||
    g34r_get(p,28)>512||g34r_get(p,29)>3584||g34r_get(p,58)>1)return 0;
 for(unsigned i=59;i<64;i++)if(g34r_get(p,i))return 0;
 if(stage==G34_DONE&&(!g34r_get(p,58)||g34r_get(p,6)||proofs!=511||g34r_get(p,28)!=512||
    g34r_get(p,29)!=3584||g34r_get(p,30)!=UINT32_MAX||g34r_get(p,56)!=3||g34r_get(p,57)!=1||
    g34r_get(p,10)!=103||g34r_get(p,11)!=303||g34r_get(p,12)!=303||g34r_get(p,13)!=303||
    (g34r_get(p,14)&(1u<<23))||(g34r_get(p,15)&0x13000282u)||g34r_get(p,16)||g34r_get(p,17)||
    (g34r_get(p,18)&0xc00001feu)||g34r_get(p,19)||!g34r_get(p,22)||!g34r_get(p,23)||
    g34r_get(p,22)==g34r_get(p,23)||g34r_get(p,20)!=g34r_get(p,22)||g34r_get(p,21)!=g34r_get(p,23)||
    !g34r_get(p,24)||!g34r_get(p,25)||!g34r_get(p,26)||!g34r_get(p,27)||
    g34r_get(p,31)!=640||g34r_get(p,52)!=15||g34r_get(p,54)!=32756))return 0;
 return 1;
}
int g34_result_pack(const struct g34_result*r,uint8_t*out,size_t n){
 if(!r||!out||n!=256)return 0;
 uintptr_t a=(uintptr_t)r,b=(uintptr_t)out;
 if(sizeof(*r)>UINTPTR_MAX-a||n>UINTPTR_MAX-b||(a<b+n&&b<a+sizeof(*r)))return 0;
 uint32_t w[64]={0x31525447u,1,256,0,r->stage,r->failure_stage,r->error,r->proofs,
  (uint32_t)r->epoch,(uint32_t)(r->epoch>>32),r->ring_begin,r->ring_end,r->rptr,r->wptr,r->status,r->intr,
  r->cp_fault,r->cp_protect,r->smmu_fsr,r->global_fsr,r->observed[0],r->observed[1],r->tokens[0],r->tokens[1],
  r->vs_crc,r->fs_crc,r->program_crc,r->pixel_crc,r->triangle_pixels,r->background_pixels,r->first_bad_byte,r->shader_bytes};
 uint8_t tmp[256];for(unsigned i=0;i<64;i++)g34r_put(tmp,i,w[i]);
 for(unsigned i=0;i<32;i++){tmp[128+i]=r->source_identity[i];tmp[160+i]=r->asset_identity[i];}
 g34r_put(tmp,48,(uint32_t)r->start);g34r_put(tmp,49,(uint32_t)(r->start>>32));
 g34r_put(tmp,50,(uint32_t)r->end);g34r_put(tmp,51,(uint32_t)(r->end>>32));
 g34r_put(tmp,52,r->owner_flags);g34r_put(tmp,53,r->guard_crc);g34r_put(tmp,54,r->guard_count);
 g34r_put(tmp,56,r->vertex_count);g34r_put(tmp,57,r->fragment_outputs);g34r_put(tmp,58,r->retained);
 g34r_put(tmp,3,g34_crc(tmp+16,240));if(!g34_result_valid(tmp,n))return 0;
 for(unsigned i=0;i<n;i++)out[i]=tmp[i];return 1;
}
