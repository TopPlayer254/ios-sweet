/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_memory.h"
static int overlap(const void*a,size_t an,const void*b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;return x<y+bn&&y<x+an;}
static int physical_page(const struct gm_memory_io*io,const void*va,uint64_t*out){uint64_t pa=io->physical(io->ctx,va);if(!pa||(pa&4095)||pa>GT_OAS_END-4096)return 0;*out=pa;return 1;}
int gm_memory_prepare(struct gm_xnu_memory*s,const struct gn_plan*fw,const struct gm_memory_io*io){
 if(!s||!fw||!io||!io->physical||((uintptr_t)s&4095)||s->state||!fw->region_count||fw->region_count>GN_MAX_REGIONS||fw->backing_bytes>GM_BACKING_CAPACITY||overlap(s,sizeof(*s),fw,sizeof(*fw))||overlap(s,sizeof(*s),io,sizeof(*io)))return 0;
 s->state=4;unsigned used=0,n=0;
 for(unsigned i=0;i<fw->region_count;i++){
  const struct gn_region*r=&fw->regions[i];
  if(!r->size||(r->address|r->size)&4095||r->size>GM_BACKING_CAPACITY-used||r->kind<GN_CACHE||r->kind>GN_HFI)return 0;
  s->regions[i]=*r;s->region_offset[i]=used;
  for(unsigned off=0;off<r->size;off+=4096){
   if(n==GT_MAX_MAPPINGS||r->address>UINT32_MAX-off||!physical_page(io,s->backing+used+off,&s->mappings[n].physical))return 0;
   s->mappings[n].iova=r->address+off;s->mappings[n].reserved=0;n++;
  }
  used+=r->size;
 }
 if(!used||used!=fw->backing_bytes)return 0;
 for(unsigned i=0;i<GT_MAX_TABLES;i++)if(!physical_page(io,s->tables[i],&s->table_physical[i]))return 0;
 if(gt_build(&s->table_plan,s->tables,s->table_physical,GT_MAX_TABLES,s->mappings,n)!=GT_OK)return 0;
 for(unsigned i=0;i<used;i++)s->backing[i]=0;
 s->backing_bytes=used;s->region_count=fw->region_count;s->state=1;return 1;
}
void*gm_memory_owned_va(struct gm_xnu_memory*s,uint32_t address,size_t bytes){
 if(!s||s->state!=1||!bytes)return 0;
 for(unsigned i=0;i<s->region_count;i++){const struct gn_region*r=&s->regions[i];if(address>=r->address&&address-r->address<r->size&&bytes<=r->size-(address-r->address))return s->backing+s->region_offset[i]+address-r->address;}
 return 0;
}
int gm_memory_publish(struct gm_xnu_memory*s,const struct gm_memory_io*io){
 if(!s||!io||!io->flush||!io->clean||!io->barrier||s->state!=1||!s->table_plan.valid||!s->backing_bytes)return 0;
 s->state=4;
 if(!io->flush(io->ctx,s->backing,s->backing_bytes))return 0;
 for(unsigned i=0;i<s->table_plan.table_count;i++)if(!io->clean(io->ctx,s->tables[i],GT_PAGE))return 0;
 io->barrier(io->ctx);s->state=2;return 1;
}
