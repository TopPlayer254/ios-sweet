/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#define GSR_REGS 37u
#define GSR_PAGES 8u
#define GSR_PARTS 4u
#define GSR_CHARS 32u
struct gsr_page {uint32_t page,snapshot_crc;uint64_t invalid,values[5];};
int gsr_encode(const uint64_t old[GSR_REGS],uint64_t invalid,char out[GSR_PAGES][GSR_PARTS][GSR_CHARS+1]);
int gsr_decode_page(const char in[GSR_PARTS][GSR_CHARS+1],struct gsr_page*out);
int gsr_assemble(const struct gsr_page in[GSR_PAGES],uint64_t out[GSR_REGS],uint64_t*invalid);

/* SPDX-License-Identifier: GPL-2.0-only */
static uint32_t gsr_crc(const uint8_t*p,unsigned n){uint32_t c=~0u;for(unsigned i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
static void gsr_put(uint8_t*p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t gsr_get(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t gsr_u64(const uint8_t*p){return gsr_get(p)|(uint64_t)gsr_get(p+4)<<32;}
static unsigned gsr_nibble(char c){const char*a="ABCDEFGHJKLMNPQR";for(unsigned i=0;i<16;i++)if(a[i]==c)return i;return 16;}
static uint32_t gsr_snapshot_crc(const uint64_t old[GSR_REGS]){uint8_t raw[GSR_REGS*8];for(unsigned i=0;i<GSR_REGS;i++){gsr_put(raw+8*i,(uint32_t)old[i]);gsr_put(raw+8*i+4,(uint32_t)(old[i]>>32));}return gsr_crc(raw,sizeof(raw));}
int gsr_encode(const uint64_t old[GSR_REGS],uint64_t invalid,char out[GSR_PAGES][GSR_PARTS][GSR_CHARS+1]){
 if(!old||!out||invalid>>GSR_REGS)return 0;uint32_t crc=gsr_snapshot_crc(old);const char*a="ABCDEFGHJKLMNPQR";
 for(unsigned page=0;page<GSR_PAGES;page++){uint8_t raw[64]={0};gsr_put(raw,0x3153475a);gsr_put(raw+4,page);gsr_put(raw+8,crc);gsr_put(raw+12,(uint32_t)invalid);gsr_put(raw+16,(uint32_t)(invalid>>32));
  for(unsigned j=0;j<5;j++){unsigned i=page*5+j;uint64_t v=i<GSR_REGS?old[i]:0;gsr_put(raw+20+8*j,(uint32_t)v);gsr_put(raw+24+8*j,(uint32_t)(v>>32));}gsr_put(raw+60,gsr_crc(raw,60));
  for(unsigned p=0;p<GSR_PARTS;p++){for(unsigned i=0;i<16;i++){unsigned v=raw[p*16+i];out[page][p][2*i]=a[v>>4];out[page][p][2*i+1]=a[v&15];}out[page][p][GSR_CHARS]=0;}
 }return 1;
}
int gsr_decode_page(const char in[GSR_PARTS][GSR_CHARS+1],struct gsr_page*out){
 if(!in||!out)return 0;uint8_t raw[64];for(unsigned p=0;p<GSR_PARTS;p++){if(in[p][GSR_CHARS])return 0;for(unsigned i=0;i<16;i++){unsigned a=gsr_nibble(in[p][2*i]),b=gsr_nibble(in[p][2*i+1]);if(a>15||b>15)return 0;raw[p*16+i]=(uint8_t)(a*16+b);}}
 if(gsr_get(raw)!=0x3153475a||gsr_get(raw+4)>=GSR_PAGES||gsr_get(raw+60)!=gsr_crc(raw,60))return 0;
 struct gsr_page result={.page=gsr_get(raw+4),.snapshot_crc=gsr_get(raw+8),.invalid=gsr_u64(raw+12)};if(result.invalid>>GSR_REGS)return 0;
 for(unsigned i=0;i<5;i++){result.values[i]=gsr_u64(raw+20+8*i);if(result.page*5+i>=GSR_REGS&&result.values[i])return 0;}*out=result;return 1;
}
int gsr_assemble(const struct gsr_page in[GSR_PAGES],uint64_t out[GSR_REGS],uint64_t*invalid){
 if(!in||!out||!invalid||in[0].invalid>>GSR_REGS)return 0;uint64_t old[GSR_REGS];
 for(unsigned p=0;p<GSR_PAGES;p++){if(in[p].page!=p||in[p].snapshot_crc!=in[0].snapshot_crc||in[p].invalid!=in[0].invalid)return 0;for(unsigned j=0;j<5;j++){unsigned i=p*5+j;if(i<GSR_REGS)old[i]=in[p].values[j];else if(in[p].values[j])return 0;}}
 if(gsr_snapshot_crc(old)!=in[0].snapshot_crc)return 0;for(unsigned i=0;i<GSR_REGS;i++)out[i]=old[i];*invalid=in[0].invalid;return 1;
}

/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){unsigned checks=0;uint64_t old[GSR_REGS],out[GSR_REGS],invalid;char lines[GSR_PAGES][GSR_PARTS][GSR_CHARS+1];struct gsr_page pages[GSR_PAGES];
 for(unsigned v=0;v<64;v++){
  for(unsigned i=0;i<GSR_REGS;i++)old[i]=((uint64_t)(v^0xa13f57bc)<<32)^((uint64_t)i*0x9e3779b97f4a7c15ULL);
  uint64_t mask=1ULL<<(v%GSR_REGS);assert(gsr_encode(old,mask,lines));checks++;
  for(unsigned p=0;p<GSR_PAGES;p++){assert(gsr_decode_page(lines[p],&pages[p]));checks++;
   for(unsigned part=0;part<GSR_PARTS;part++)for(unsigned j=0;j<GSR_CHARS;j++){
    struct gsr_page untouched;memset(&untouched,0xa5,sizeof(untouched));struct gsr_page before=untouched;char original=lines[p][part][j];lines[p][part][j]=original=='A'?'B':'A';
    assert(!gsr_decode_page(lines[p],&untouched));assert(!memcmp(&untouched,&before,sizeof(before)));checks+=2;lines[p][part][j]=original;
   }
  }
  assert(gsr_assemble(pages,out,&invalid));assert(invalid==mask&&!memcmp(old,out,sizeof(old)));checks+=2;
  pages[4].values[2]^=1;memset(out,0xa5,sizeof(out));uint64_t before[GSR_REGS];memcpy(before,out,sizeof(out));invalid=0xdeadbeef;
  assert(!gsr_assemble(pages,out,&invalid));assert(!memcmp(before,out,sizeof(out))&&invalid==0xdeadbeef);checks+=2;pages[4].values[2]^=1;
  pages[7].values[4]=1;assert(!gsr_assemble(pages,out,&invalid));checks++;pages[7].values[4]=0;
  pages[7].invalid^=1;assert(!gsr_assemble(pages,out,&invalid));checks++;pages[7].invalid^=1;
 }
 assert(!gsr_encode(old,1ULL<<37,lines));assert(!gsr_encode(0,0,lines));assert(!gsr_assemble(0,out,&invalid));checks+=3;
 printf("G13_SNAPSHOT_CODEC_PASS checks=%u all37Values=1 perPageAndWholeCRC=1 corruptMixedPaddingReject=1 noOutputMutationOnError=1\n",checks);return 0;
}
