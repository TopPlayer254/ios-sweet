/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_translation.h"
static int overlap(const void*a,size_t an,const void*b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;return x<y+bn&&y<x+an;}
static int pa_valid(uint64_t pa){return pa&&!(pa&(GT_PAGE-1))&&pa<=GT_OAS_END-GT_PAGE;}
static int iova_valid(uint32_t va){return !(va&(GT_PAGE-1))&&((va>=0x4000&&va<0x1000000)||(va>=0x60000000&&va<0x80000000));}
static int table_index(const struct gt_plan*p,uint64_t value){if((value&0xfff)!=3||value>=GT_OAS_END)return -1;for(unsigned i=0;i<p->table_count;i++)if(p->table_physical[i]==(value&~0xfffULL))return (int)i;return -1;}
int gt_build(struct gt_plan*out,uint64_t (*tables)[GT_ENTRIES],const uint64_t*physical,unsigned count,const struct gt_mapping*maps,unsigned n){
 if(!out||!tables||!physical||!maps||!count||count>GT_MAX_TABLES||!n||n>GT_MAX_MAPPINGS)return GT_ARGUMENT;
 size_t tb=(size_t)count*GT_PAGE,pb=(size_t)count*sizeof(*physical),mb=(size_t)n*sizeof(*maps);
 if(overlap(out,sizeof(*out),tables,tb)||overlap(out,sizeof(*out),physical,pb)||overlap(out,sizeof(*out),maps,mb)||overlap(tables,tb,physical,pb)||overlap(tables,tb,maps,mb))return GT_OVERLAP;
 *out=(struct gt_plan){0};
 for(unsigned i=0;i<count;i++){
  if(!pa_valid(physical[i]))return GT_PHYSICAL;
  for(unsigned j=0;j<i;j++)if(physical[i]==physical[j])return GT_OVERLAP;
 }
 unsigned needed=1;
 for(unsigned i=0;i<n;i++){
  if(maps[i].reserved||!iova_valid(maps[i].iova))return GT_IOVA;
  if(!pa_valid(maps[i].physical))return GT_PHYSICAL;
  for(unsigned j=0;j<count;j++)if(maps[i].physical==physical[j])return GT_OVERLAP;
  unsigned same_l1=0,same_l2=0;
  for(unsigned j=0;j<i;j++){
   if(maps[i].iova==maps[j].iova||maps[i].physical==maps[j].physical)return GT_OVERLAP;
   same_l1|=(maps[i].iova>>30)==(maps[j].iova>>30);
   same_l2|=(maps[i].iova>>21)==(maps[j].iova>>21);
  }
  needed+=!same_l1;needed+=!same_l2;
 }
 if(needed>count)return GT_CAPACITY;
 for(unsigned i=0;i<count;i++)for(unsigned j=0;j<GT_ENTRIES;j++)tables[i][j]=0;
 out->table_count=1;out->mapped_pages=n;
 for(unsigned i=0;i<count;i++)out->table_physical[i]=physical[i];
 for(unsigned i=0;i<n;i++){
  unsigned current=0;const unsigned shifts[]={30,21};
  for(unsigned level=0;level<2;level++){
   unsigned index=(maps[i].iova>>shifts[level])&511;uint64_t*entry=&tables[current][index];
   if(!*entry){unsigned child=out->table_count++;*entry=physical[child]|3;current=child;}
   else {int child=table_index(out,*entry);if(child<0)return GT_ARGUMENT;current=(unsigned)child;}
  }
  tables[current][(maps[i].iova>>12)&511]=maps[i].physical|GT_LEAF_ATTR;
 }
 if(out->table_count!=needed)return GT_ARGUMENT;
 out->valid=1;return GT_OK;
}
int gt_lookup(const struct gt_plan*p,const uint64_t (*tables)[GT_ENTRIES],uint32_t va,uint64_t*physical){
 if(!p||!tables||!physical||p->valid!=1||!p->table_count||p->table_count>GT_MAX_TABLES||p->mapped_pages>GT_MAX_MAPPINGS)return 0;
 unsigned current=0;const unsigned shifts[]={30,21};
 for(unsigned level=0;level<2;level++){int index=table_index(p,tables[current][(va>>shifts[level])&511]);if(index<0)return 0;current=(unsigned)index;}
 uint64_t value=tables[current][(va>>12)&511];
 if((value&0xfff)!=GT_LEAF_ATTR||value>=GT_OAS_END)return 0;
 *physical=(value&~0xfffULL)|(va&0xfff);return 1;
}
