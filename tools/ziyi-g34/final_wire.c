/* SPDX-License-Identifier: GPL-2.0-only
 * CPU transport only. An optical square is not a GPU-rendered triangle.
 */
#include "final_wire.h"
uint32_t g34_crc(const void *ptr,size_t n){
 const uint8_t*p=ptr;uint32_t c=~0u;
 for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}
 return ~c;
}
static uint32_t g34_get(const uint8_t*p,unsigned n){uint32_t v=0;for(unsigned j=0;j<4;j++)v|=(uint32_t)p[n*4+j]<<(j*8);return v;}
static void g34_put(uint8_t*p,unsigned n,uint32_t v){for(unsigned j=0;j<4;j++)p[n*4+j]=(uint8_t)(v>>(j*8));}
static int g34_overlap(const volatile void*a,size_t an,const volatile void*b,size_t bn){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y||(x<y+bn&&y<x+an);
}
static int g34_inner(const uint8_t*p,unsigned n,uint32_t magic){
 return p&&g34_get(p,0)==magic&&g34_get(p,1)==1&&g34_get(p,2)==n&&g34_get(p,3)==g34_crc(p+16,n-16);
}
int g34_final_valid(const uint8_t*p,size_t n){
 if(!p||n!=G34_FINAL_BYTES||g34_get(p,0)!=0x46343347u||g34_get(p,1)!=1||
    g34_get(p,2)!=n||g34_get(p,3)!=g34_crc(p+16,n-16)||
    !(g34_get(p,4)|g34_get(p,5))||g34_get(p,6)!=0x06060201u||g34_get(p,7)!=4||
    g34_get(p,8)!=64||g34_get(p,9)!=384||g34_get(p,10)!=464||g34_get(p,11)!=624||
    g34_get(p,12)>7||g34_get(p,13)>7||g34_get(p,12)==g34_get(p,13)||g34_get(p,14)||g34_get(p,15))return 0;
 return g34_inner(p+64,320,0x31504347u)&&g34_inner(p+384,80,0x314b4347u)&&
        g34_inner(p+464,160,0x31525043u)&&g34_inner(p+624,256,0x31525447u);
}
int g34_final_pack(uint8_t*out,size_t n,uint64_t epoch,unsigned worker,unsigned observer,
 const uint8_t*gcp,const uint8_t*gck,const uint8_t*cpr,const uint8_t*tri){
 if(!out||n!=G34_FINAL_BYTES||!epoch||worker>7||observer>7||worker==observer||
    !g34_inner(gcp,320,0x31504347u)||!g34_inner(gck,80,0x314b4347u)||
    !g34_inner(cpr,160,0x31525043u)||!g34_inner(tri,256,0x31525447u)||
    g34_overlap(out,n,gcp,320)||g34_overlap(out,n,gck,80)||g34_overlap(out,n,cpr,160)||g34_overlap(out,n,tri,256))return 0;
 for(unsigned i=0;i<n;i++)out[i]=0;
 g34_put(out,0,0x46343347u);g34_put(out,1,1);g34_put(out,2,(uint32_t)n);
 g34_put(out,4,(uint32_t)epoch);g34_put(out,5,(uint32_t)(epoch>>32));g34_put(out,6,0x06060201u);g34_put(out,7,4);
 g34_put(out,8,64);g34_put(out,9,384);g34_put(out,10,464);g34_put(out,11,624);
 g34_put(out,12,worker);g34_put(out,13,observer);
 for(unsigned i=0;i<320;i++)out[64+i]=gcp[i];for(unsigned i=0;i<80;i++)out[384+i]=gck[i];
 for(unsigned i=0;i<160;i++)out[464+i]=cpr[i];for(unsigned i=0;i<256;i++)out[624+i]=tri[i];
 g34_put(out,3,g34_crc(out+16,n-16));return g34_final_valid(out,n);
}
int g34_final_packet_valid(const uint8_t*p,size_t n){
 const char magic[]="ZMP1RAW1";if(!p||n!=128)return 0;for(unsigned i=0;i<8;i++)if(p[i]!=(uint8_t)magic[i])return 0;
 unsigned page=(unsigned)p[16]|(unsigned)p[17]<<8,count=page==9?52:92;
 if(g34_get(p,2)!=0x00200001u||g34_get(p,3)!=0x34525447u||page>=10||p[18]!=10||p[19]||
    g34_get(p,5)!=G34_FINAL_BYTES||g34_get(p,6)!=page*92||g34_get(p,7)!=count||g34_get(p,31)!=g34_crc(p,124))return 0;
 for(unsigned i=32+count;i<124;i++)if(p[i])return 0;return 1;
}
int g34_final_packet(const uint8_t*r,size_t n,unsigned page,uint8_t*out,size_t cap){
 if(!out||cap!=128||page>=G34_PAGES||!g34_final_valid(r,n)||g34_overlap(r,n,out,cap))return 0;
 for(unsigned i=0;i<128;i++)out[i]=0;
 const char magic[]="ZMP1RAW1";for(unsigned i=0;i<8;i++)out[i]=(uint8_t)magic[i];
 g34_put(out,2,0x00200001u);g34_put(out,3,0x34525447u);out[16]=(uint8_t)page;out[18]=10;
 g34_put(out,5,G34_FINAL_BYTES);g34_put(out,6,page*92);unsigned count=page==9?52:92;g34_put(out,7,count);
 for(unsigned i=0;i<count;i++)out[32+i]=r[page*92+i];g34_put(out,31,g34_crc(out,124));return g34_final_packet_valid(out,cap);
}
int g34_final_draw(volatile uint32_t*fb,size_t words,const uint8_t*p,size_t n){
 if(!fb||((uintptr_t)fb&3)||words!=1080u*2400u||!g34_final_packet_valid(p,n)||g34_overlap(fb,words*4,p,n))return 0;
 /* Same camera-qualified rectangle as G33; does not cover the upper picture. */
 for(unsigned y=0;y<800;y++)for(unsigned x=0;x<800;x++){
  unsigned cx=x/20,cy=y/20;uint32_t c=0xffffffffu;
  if(cx>=2&&cx<=37&&cy>=2&&cy<=37&&(cx==2||cx==37||cy==2||cy==37))c=0;
  if(cx>=4&&cx<36&&cy>=4&&cy<36){unsigned b=(cy-4)*32+cx-4;c=(p[b/8]&(1u<<(7-b%8)))?0xffffffffu:0;}
  fb[(1200+y)*1080+140+x]=c;
 }
 return 1;
}
