/* SPDX-License-Identifier: GPL-2.0-only
 * Exact command-DB ABI: Xiaomi 24856283 cmd-db.c; dependency/vote formulas:
 * Qualcomm KGSL d70a6765 adreno_rpmh.c and adreno_a6xx_rpmh.c.
 * This CPU-only preparation is NOT linked to a kernel or hardware-qualified.
 */
#include "ziyi_g23_resources.h"
#include <string.h>
static uint16_t gr_private_u16(const uint8_t*p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t gr_private_u32(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int gr_private_span(size_t at,size_t n,size_t cap){return at<=cap&&n<=cap-at;}
static const uint8_t names[GR_COUNT][8]={"gfx.lvl","cx.lvl","mx.lvl","SH0","MC0","ACV","CN0"};
__attribute__((noinline)) int gr_parse(const uint8_t*d,size_t bytes,struct gr_resources*out){
 if(!d||!out||bytes<144||bytes>0x20000)return GR_ARGUMENT;
 if(gr_private_u32(d)!=1||gr_private_u32(d+4)!=0x0c0330db)return GR_HEADER;
 struct gr_resources r={0};
 for(unsigned slot=0;slot<8;slot++){
  const uint8_t*h=d+8+16*slot;uint32_t slave=gr_private_u16(h),count=gr_private_u16(h+6),entry=144u+gr_private_u16(h+2),aux=144u+gr_private_u16(h+4);
  if(!slave)break;
  /* Native G1 has two distinct ARC groups (slave 3). Group IDs may
    * repeat; matching resource keys must still be unique across groups. */
   if(slave>7)return GR_HEADER;
  if(count>512||r.entries>1024-count||!gr_private_span(entry,24u*count,bytes)||!gr_private_span(aux,0,bytes))return GR_BOUNDS;
  r.entries+=count;
  for(unsigned i=0;i<count;i++){
   const uint8_t*e=d+entry+24*i;
   for(unsigned key=0;key<GR_COUNT;key++)if(!memcmp(e,names[key],8)){
    if(r.found&(1u<<key))return GR_DUPLICATE;
    uint32_t address=gr_private_u32(e+16),n=gr_private_u16(e+20),at=aux+gr_private_u16(e+22);
    if(n>32||!gr_private_span(at,n,bytes))return GR_BOUNDS;
    if((address>>16&7)!=slave||slave!=(key<3?3u:5u)||!address||(address&3))return GR_HEADER;
    struct gr_resource*q=&r.resource[key];q->address=address;q->slave=slave;q->version=gr_private_u16(h+8);q->aux_bytes=n;
    if(n)memcpy(q->aux,d+at,n);r.found|=1u<<key;
   }
  }
 }
 *out=r;return r.found==GR_ALL?GR_OK:GR_MISSING;
}
int gr_stable(const uint8_t*a,size_t an,const uint8_t*b,size_t bn,struct gr_resources*out){
 if(!out)return GR_ARGUMENT;struct gr_resources x={0},y={0};int e=gr_parse(a,an,&x);if(e)return e;
 e=gr_parse(b,bn,&y);if(e)return e;
 if(memcmp(&x,&y,sizeof(x)))return GR_CHANGED;*out=x;return GR_OK;
}
static int gr_private_levels(const struct gr_resource*r,uint16_t v[16],unsigned*n){
 if(!r||r->slave!=3||r->aux_bytes<4||r->aux_bytes>32||(r->aux_bytes&1))return GR_AUX;
 unsigned count=0;int padding=0;
 for(unsigned i=0;i<r->aux_bytes/2;i++){
  uint16_t x=gr_private_u16(r->aux+2*i);
  if(i&&!x){padding=1;continue;}
  if(padding||(i==0&&x)||(i&&x<=v[count-1]))return GR_AUX;
  v[count++]=x;
 }
 if(count<2)return GR_AUX;*n=count;return GR_OK;
}
int gr_arc_vote(const struct gr_resource*pri,const struct gr_resource*sec,uint16_t requested,uint32_t*out){
 if(!out)return GR_ARGUMENT;uint16_t p[16],s[16];unsigned pn=0,sn=0;
 int e=gr_private_levels(pri,p,&pn);if(e)return e;e=gr_private_levels(sec,s,&sn);if(e)return e;
 for(unsigned i=0;i<pn;i++)if(p[i]>=requested)for(unsigned j=0;j<sn;j++)if(s[j]>=p[i]){
  *out=((uint32_t)p[i]<<16)|(j<<8)|i;return GR_OK;
 }
 /* Refuse unsupported dependencies, rather than voting a donor fallback. */
 return GR_LEVEL;
}
static int gr_private_header(uint32_t id,uint32_t words,uint32_t sequence,uint32_t*out){
 if(!sequence||sequence>4095||!words||words>255)return GR_ARGUMENT;
 *out=(sequence<<20)|(words<<8)|id;return GR_OK;
}
int gr_perf_lowest(const struct gr_resources*r,uint32_t sequence,uint32_t out[59]){
 if(!r||!out||r->found!=GR_ALL)return GR_ARGUMENT;
 uint32_t table[59]={0};int e=gr_private_header(4,59,sequence,table);if(e)return e;
 /* Own DT: lowest OPP common to all four bins, NOT an A650 table. */
 table[1]=2;table[2]=2;
 e=gr_arc_vote(&r->resource[0],&r->resource[2],56,&table[6]);if(e)return e;
 uint16_t cx[16];unsigned count=0;e=gr_private_levels(&r->resource[1],cx,&count);if(e)return e;
 unsigned dep=0;while(dep<count&&cx[dep]<64)dep++;if(dep==count)return GR_LEVEL;
 table[7]=dep;table[8]=230000;
 e=gr_arc_vote(&r->resource[1],&r->resource[2],64,&table[53]);if(e)return e;table[54]=200000;
 memcpy(out,table,sizeof(table));return GR_OK;
}
static int gr_private_bcm(const struct gr_resource*r,uint32_t*unit,uint32_t*width,uint32_t*vcd,int fixed){
 if(r->slave!=5||r->aux_bytes!=8)return GR_AUX;
 *unit=gr_private_u32(r->aux);*width=gr_private_u16(r->aux+4);*vcd=r->aux[6];
 if(*vcd>31||(!fixed&&(!*unit||!*width)))return GR_AUX;return GR_OK;
}
static uint32_t gr_private_vote(uint32_t kbps,uint32_t unit,uint32_t width,uint32_t buswidth,int fixed,int commit){
 uint64_t peak=fixed?(kbps?8u:0u):((uint64_t)kbps*width/buswidth)*1000/unit;
 if(kbps&&!peak)peak=1;if(peak>0x3fff)peak=0x3fff;
 return ((uint32_t)commit<<30)|((uint32_t)(peak!=0)<<29)|(uint32_t)peak;
}
int gr_bandwidth(const struct gr_resources*r,uint32_t sequence,uint32_t kbps,uint32_t out[160]){
 if(!r||!out||r->found!=GR_ALL||!kbps||kbps>0x7f22ff)return GR_ARGUMENT;
 uint32_t t[160]={0};int e=gr_private_header(3,160,sequence,t);if(e)return e;
 uint32_t units[4],widths[4],vcds[4];
 for(unsigned i=0;i<4;i++){e=gr_private_bcm(&r->resource[3+i],&units[i],&widths[i],&vcds[i],i==2);if(e)return e;}
 /* Off + one source-derived bandwidth. No average bandwidth feature enabled. */
 t[1]=2;t[2]=1;t[3]=3;t[4]=1;t[6]=r->resource[6].address;
 t[12]=gr_private_vote(0,units[3],widths[3],4,0,1);t[18]=gr_private_vote(100,units[3],widths[3],4,0,1);
 for(unsigned i=0;i<3;i++){
  int commit=i==2||vcds[i]!=vcds[i+1];t[5]|=(uint32_t)commit<<i;t[24+i]=r->resource[3+i].address;
  t[32+i]=gr_private_vote(0,units[i],widths[i],i==0?16u:4u,i==2,commit);
  t[40+i]=gr_private_vote(kbps,units[i],widths[i],i==0?16u:4u,i==2,commit);
 }
 memcpy(out,t,sizeof(t));return GR_OK;
}
