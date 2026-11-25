/* Readback verifier only: never writes the GPU color or depth images. */
#include "primitives_result.h"
#include "final_wire.h"
int g35_check_pixels(struct g35_result*r,const volatile uint8_t*p,size_t n){
 if(!r||!p||n!=16384||r->w[G35_STAGE]!=G35_PIXELS||r->w[G35_TEX_PIXELS]||r->w[G35_BLEND_PIXELS]||r->w[G35_DEPTH_PIXELS])return 0;
 r->w[G35_BAD_PIXEL]=UINT32_MAX;
 const uint32_t checker[4]={0xffff0000u,0xff00ff00u,0xff0000ffu,0xffffffffu};
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
  uint32_t expected=x<16?checker[(y&1)*2+(x&1)]:x<32?0xff608020u:0xff00ff00u;
  for(unsigned b=0;b<4;b++)if(p[(y*64+x)*4+b]!=(uint8_t)(expected>>(8*b))){r->w[G35_BAD_PIXEL]=(y*64+x)*4+b;return 0;}
  r->w[x<16?G35_TEX_PIXELS:x<32?G35_BLEND_PIXELS:G35_DEPTH_PIXELS]++;
 }
 if(r->w[G35_TEX_PIXELS]!=1024||r->w[G35_BLEND_PIXELS]!=1024||r->w[G35_DEPTH_PIXELS]!=2048)return 0;
 r->w[G35_PIXEL_CRC]=g35_crc((const void*)p,n);return 1;
}
int g35_check_depth(struct g35_result*r,const volatile uint8_t*p,size_t n){
 if(!r||!p||n!=8192||r->w[G35_STAGE]!=G35_PIXELS||r->w[G35_NEAR_SAMPLES]||r->w[G35_CLEAR_SAMPLES])return 0;
 r->w[G35_BAD_DEPTH]=UINT32_MAX;r->w[G35_NEAR_MIN]=65535;r->w[G35_NEAR_MAX]=0;
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
  unsigned at=y*64+x;uint32_t d=p[at*2]|(uint32_t)p[at*2+1]<<8;
  /* D16 UNORM accepts either adjacent conversion value at depth0.25.
   * Color occlusion is exact: the later depth0.75 red draw must lose. */
  if(x<32){if(d!=65535){r->w[G35_BAD_DEPTH]=at;return 0;}r->w[G35_CLEAR_SAMPLES]++;}
  else{
   if(d<16383||d>16384){r->w[G35_BAD_DEPTH]=at;return 0;}r->w[G35_NEAR_SAMPLES]++;
   if(d<r->w[G35_NEAR_MIN])r->w[G35_NEAR_MIN]=d;if(d>r->w[G35_NEAR_MAX])r->w[G35_NEAR_MAX]=d;
  }
 }
 if(r->w[G35_NEAR_SAMPLES]!=2048||r->w[G35_CLEAR_SAMPLES]!=2048)return 0;
 r->w[G35_DEPTH_CRC]=g35_crc((const void*)p,n);return 1;
}
static uint32_t g35r_get(const uint8_t*p,unsigned n){uint32_t v=0;for(unsigned j=0;j<4;j++)v|=(uint32_t)p[n*4+j]<<(8*j);return v;}
int g35_result_valid(const uint8_t*p,size_t n){
 if(!p||n!=256||g35r_get(p,0)!=0x31525047u||g35r_get(p,1)!=1||g35r_get(p,2)!=256||g35r_get(p,3)!=g35_crc(p+16,240))return 0;
 if(!(g35r_get(p,8)|g35r_get(p,9))||g35r_get(p,4)>15||(g35r_get(p,4)>9&&g35r_get(p,4)!=15)||g35r_get(p,5)>9||g35r_get(p,6)>13||g35r_get(p,7)>1023||g35r_get(p,55)>1)return 0;
 for(unsigned i=58;i<64;i++)if(g35r_get(p,i))return 0;
 if(g35r_get(p,4)==G35_DONE){
  const unsigned zero[]={5,6,15,16,17,19};for(unsigned i=0;i<6;i++)if(g35r_get(p,zero[i]))return 0;
  if(g35r_get(p,7)!=1023||g35r_get(p,10)!=303||g35r_get(p,11)!=512||g35r_get(p,12)!=512||g35r_get(p,13)!=512||
   (g35r_get(p,14)&(1u<<23))||(g35r_get(p,18)&0xc00001feu)||!g35r_get(p,22)||!g35r_get(p,23)||g35r_get(p,22)==g35r_get(p,23)||
   g35r_get(p,20)!=g35r_get(p,22)||g35r_get(p,21)!=g35r_get(p,23)||g35r_get(p,31)!=1024||g35r_get(p,32)!=1024||g35r_get(p,33)!=2048||
   g35r_get(p,34)!=2048||g35r_get(p,35)!=2048||g35r_get(p,36)!=UINT32_MAX||g35r_get(p,37)!=UINT32_MAX||
   g35r_get(p,38)!=1920||g35r_get(p,39)!=4||g35r_get(p,52)!=15||g35r_get(p,54)!=24564||g35r_get(p,55)!=1||
   g35r_get(p,56)<16383||g35r_get(p,57)>16384||g35r_get(p,56)>g35r_get(p,57))return 0;
 }
 return 1;
}
int g35_result_pack(const struct g35_result*r,uint8_t*out,size_t n){
 if(!r||!out||n!=256)return 0;uintptr_t a=(uintptr_t)r,b=(uintptr_t)out;
 if(sizeof(*r)>UINTPTR_MAX-a||n>UINTPTR_MAX-b||(a<b+n&&b<a+sizeof(*r)))return 0;
 uint8_t tmp[256];for(unsigned i=0;i<64;i++)for(unsigned j=0;j<4;j++)tmp[i*4+j]=(uint8_t)(r->w[i]>>(8*j));
 uint32_t crc=g35_crc(tmp+16,240);for(unsigned j=0;j<4;j++)tmp[12+j]=(uint8_t)(crc>>(8*j));
 if(!g35_result_valid(tmp,256))return 0;for(unsigned i=0;i<256;i++)out[i]=tmp[i];return 1;
}
