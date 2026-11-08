/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g24_memory.h"
#include <stdint.h>
static int gc_overlap(const void*a,size_t an,const void*b,size_t bn){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y||(x<y+bn&&y<x+an);
}
static int gc_page(uint64_t pa){return pa&&!(pa&(GC_CPU_PAGE-1u))&&pa<=GT_OAS_END-GC_CPU_PAGE;}
int gc_build(struct gc_plan*out,const struct gc_request*r){
 if(!out||!r)return GC_ARGUMENT;
 if(gc_overlap(out,sizeof(*out),r,sizeof(*r)))return GC_OVERLAP;
 if(!r->epoch||r->pages!=GC_PAGES||r->normal_nc!=1||r->private_owned!=1||!gc_page(r->hfi_pa))return GC_OWNERSHIP;
 for(unsigned i=0;i<GC_PAGES;i++){
  if(!gc_page(r->pa[i])||r->pa[i]==r->hfi_pa)return GC_OWNERSHIP;
  for(unsigned j=0;j<i;j++)if(r->pa[i]==r->pa[j])return GC_OVERLAP;
 }
 uint64_t table_pa[GC_TABLES];struct gt_mapping maps[GC_MAPS];
 for(unsigned i=0;i<GC_TABLES;i++)table_pa[i]=r->pa[0]+i*GT_PAGE;
 for(unsigned i=0;i<GC_MAPS;i++)maps[i]=(struct gt_mapping){GC_IOVA+i*GT_PAGE,0,r->pa[1+i/4]+(i%4)*GT_PAGE};
 *out=(struct gc_plan){0};
 if(gt_build(&out->translations,out->tables,table_pa,GC_TABLES,maps,GC_MAPS)!=GT_OK)return GC_TABLE;
 if(out->translations.table_count!=GC_TABLES||out->translations.mapped_pages!=GC_MAPS)return GC_TABLE;
 for(unsigned i=0;i<GC_MAPS;i++){
  uint64_t pa=0;
  if(!gt_lookup(&out->translations,out->tables,maps[i].iova,&pa)||pa!=maps[i].physical)return GC_TABLE;
 }
 out->epoch=r->epoch;out->ring_iova=GC_IOVA;out->pixel_iova=GC_IOVA+GC_CPU_PAGE;
 out->fence_iova=GC_IOVA+2*GC_CPU_PAGE;out->sqe_iova=GC_IOVA+3*GC_CPU_PAGE;
 out->sqe_capacity=3*GC_CPU_PAGE;out->valid=1;return GC_OK;
}
