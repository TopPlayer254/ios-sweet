/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g19_owned_pages.h"

static int ziyi_g19_owned_pages_fail(struct hop *h,uint32_t error)
{
 h->error=error;
 h->state=h->va||h->state==HOP_OWNED||h->state==HOP_READY||h->state==HOP_LIVE
          ?HOP_QUARANTINE:HOP_REFUSED;
 return 0;
}
static int ziyi_g19_owned_pages_complete(const struct hop_ops *o)
{
 return o&&o->policy&&o->evidence&&o->allocate&&o->backing&&o->managed&&
  o->aliases_closed&&o->get_wimg&&o->set_wimg&&o->observe&&o->free_allocation;
}
static int ziyi_g19_owned_pages_facts(struct hop *h,const struct hop_ops *o,uint32_t required)
{
 uint32_t got=0;
 return o->evidence(o->ctx,h->epoch,&got)&&(got&required)==required;
}
static int ziyi_g19_owned_pages_observation(const struct hop_ops *o,uint64_t va,uint64_t pa,
                       uint8_t *attr)
{
 struct gh_cpu_page_observation v={0};
 if(!o->observe(o->ctx,va,pa,&v)||v.va!=va||v.expected_pa!=pa||v.el!=4||
    !(v.sctlr&1u)||!gh_par_identity(v.par,va,pa))return 0;
 *attr=(uint8_t)(v.par>>56);
 return 1;
}
static int ziyi_g19_owned_pages_verify_page(struct hop *h,const struct hop_ops *o,uint32_t i,
                       uint8_t expected)
{
 struct hop_page *p=&h->page[i];
 uint64_t va=h->va+(uint64_t)i*HOP_PAGE_SIZE,pa=0,aperture=0;
 if(!ziyi_g19_owned_pages_facts(h,o,HOP_LEASE_HELD)||!o->backing(o->ctx,va,&pa,&aperture)||
    pa!=p->pa||aperture!=p->aperture||!o->managed(o->ctx,pa)||
    !o->aliases_closed(o->ctx,h->epoch,va,pa))return 0;
 /* 4KiB is the minimum qualified CPU translation leaf. Check both ends of
  * every GMU subpage, including aliases of the unused part of each CPU page. */
 for(uint32_t off=0;off<HOP_PAGE_SIZE;off+=4096u){
  for(uint32_t end=0;end<2;end++){
   uint32_t d=off+(end?4095u:0u);uint8_t a=0,b=0;
   if(!ziyi_g19_owned_pages_facts(h,o,HOP_LEASE_HELD)||
      !ziyi_g19_owned_pages_observation(o,va+d,pa+d,&a)||
      !ziyi_g19_owned_pages_observation(o,aperture+d,pa+d,&b)||a!=expected||b!=expected)return 0;
  }
 }
 return ziyi_g19_owned_pages_facts(h,o,HOP_LEASE_HELD);
}
int hop_prepare(struct hop *h,const struct hop_ops *o,uint64_t epoch,uint32_t n)
{
 if(!h)return 0;
 if(h->state!=HOP_FRESH)return 0; /* Never destroy retained ownership. */
 if(!ziyi_g19_owned_pages_complete(o)||!epoch||!n||n>HOP_MAX_PAGES)return ziyi_g19_owned_pages_fail(h,HOP_ARGUMENT);
 h->epoch=epoch;h->count=n;h->bytes=n*HOP_PAGE_SIZE;
 uint32_t granule=0;
 if(!o->policy(o->ctx,&granule,&h->nc_wimg)||granule!=HOP_PAGE_SIZE)
  return ziyi_g19_owned_pages_fail(h,HOP_POLICY);
 if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM))return ziyi_g19_owned_pages_fail(h,HOP_LIFECYCLE);
 if(!o->allocate(o->ctx,epoch,h->bytes,&h->va))return ziyi_g19_owned_pages_fail(h,HOP_ALLOCATE);
 h->state=HOP_OWNED;
 if(!h->va||(h->va&(HOP_PAGE_SIZE-1u))||h->va>UINT64_MAX-h->bytes)
  return ziyi_g19_owned_pages_fail(h,HOP_BACKING);
 /* Snapshot and qualify ALL pages before changing the first one's attrs. */
 for(uint32_t i=0;i<n;i++){
  struct hop_page *p=&h->page[i];uint64_t va=h->va+(uint64_t)i*HOP_PAGE_SIZE;
  if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM)||!o->backing(o->ctx,va,&p->pa,&p->aperture)||
     !p->pa||(p->pa&(HOP_PAGE_SIZE-1u))||p->pa>=(1ULL<<36)||
     !p->aperture||(p->aperture&(HOP_PAGE_SIZE-1u))||
     p->aperture>UINT64_MAX-HOP_PAGE_SIZE||!o->managed(o->ctx,p->pa))
   return ziyi_g19_owned_pages_fail(h,HOP_BACKING);
  for(uint32_t j=0;j<i;j++)if(h->page[j].pa==p->pa)
   return ziyi_g19_owned_pages_fail(h,HOP_BACKING);
  if(!o->aliases_closed(o->ctx,epoch,va,p->pa))return ziyi_g19_owned_pages_fail(h,HOP_ALIAS);
  if(!o->get_wimg(o->ctx,p->pa,&p->old_wimg))return ziyi_g19_owned_pages_fail(h,HOP_ATTRIBUTES);
  uint8_t initial=0;
  if(!ziyi_g19_owned_pages_observation(o,va,p->pa,&initial))return ziyi_g19_owned_pages_fail(h,HOP_TRANSLATION);
  /* Refuse Device and initially NC allocations: candidate contract owns
   * ordinary cached allocator pages and restores that exact prior attr. */
  if(!(initial&0xf0u)||!(initial&0x0fu)||initial==HOP_NORMAL_NC)
   return ziyi_g19_owned_pages_fail(h,HOP_ATTRIBUTES);
  p->old_attr=initial;
  if(!ziyi_g19_owned_pages_verify_page(h,o,i,initial))return ziyi_g19_owned_pages_fail(h,HOP_TRANSLATION);
 }
 for(uint32_t i=0;i<n;i++){
  struct hop_page *p=&h->page[i];uint32_t got=0;
  if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM))return ziyi_g19_owned_pages_fail(h,HOP_LIFECYCLE);
  p->touched=1; /* Conversion may occur even when callback reports failure. */
  if(!o->set_wimg(o->ctx,p->pa,h->nc_wimg)||
     !o->get_wimg(o->ctx,p->pa,&got)||got!=h->nc_wimg)
   return ziyi_g19_owned_pages_fail(h,HOP_ATTRIBUTES);
  if(!ziyi_g19_owned_pages_verify_page(h,o,i,HOP_NORMAL_NC))return ziyi_g19_owned_pages_fail(h,HOP_TRANSLATION);
 }
 h->state=HOP_READY;
 if(!hop_validate(h,o))return 0;
 if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM))return ziyi_g19_owned_pages_fail(h,HOP_LIFECYCLE);
 return 1;
}
int hop_validate(struct hop *h,const struct hop_ops *o)
{
 if(!h||(h->state!=HOP_READY&&h->state!=HOP_LIVE))return 0;
 if(!ziyi_g19_owned_pages_complete(o))return ziyi_g19_owned_pages_fail(h,HOP_ARGUMENT);
 uint32_t granule=0,nc=0;
 if(!o->policy(o->ctx,&granule,&nc)||granule!=HOP_PAGE_SIZE||nc!=h->nc_wimg)
  return ziyi_g19_owned_pages_fail(h,HOP_POLICY);
 for(uint32_t i=0;i<h->count;i++){
  uint32_t got=0;
  if(!o->get_wimg(o->ctx,h->page[i].pa,&got)||got!=nc)
   return ziyi_g19_owned_pages_fail(h,HOP_ATTRIBUTES);
  if(!ziyi_g19_owned_pages_verify_page(h,o,i,HOP_NORMAL_NC))return ziyi_g19_owned_pages_fail(h,HOP_TRANSLATION);
 }
 return 1;
}
int hop_mark_live(struct hop *h,const struct hop_ops *o)
{
 if(!h||h->state!=HOP_READY)return 0;
 if(!hop_validate(h,o))return 0;
 if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM))return ziyi_g19_owned_pages_fail(h,HOP_LIFECYCLE);
 h->state=HOP_LIVE;
 return 1;
}
int hop_release(struct hop *h,const struct hop_ops *o)
{
 if(!h||(h->state!=HOP_READY&&h->state!=HOP_LIVE))return 0;
 if(!ziyi_g19_owned_pages_complete(o))return ziyi_g19_owned_pages_fail(h,HOP_ARGUMENT);
 if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM))return ziyi_g19_owned_pages_fail(h,HOP_LIFECYCLE);
 if(!hop_validate(h,o))return 0;
 for(uint32_t i=0;i<h->count;i++){
  struct hop_page *p=&h->page[i];uint32_t got=0;
  if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM))return ziyi_g19_owned_pages_fail(h,HOP_LIFECYCLE);
  if(!o->set_wimg(o->ctx,p->pa,p->old_wimg)||
     !o->get_wimg(o->ctx,p->pa,&got)||got!=p->old_wimg||
     !ziyi_g19_owned_pages_verify_page(h,o,i,p->old_attr))return ziyi_g19_owned_pages_fail(h,HOP_ATTRIBUTES);
  p->touched=0;
 }
 if(!ziyi_g19_owned_pages_facts(h,o,HOP_RECLAIM)||
    !o->free_allocation(o->ctx,h->epoch,h->va,h->bytes))
  return ziyi_g19_owned_pages_fail(h,HOP_LIFECYCLE);
 h->state=HOP_RELEASED;h->error=HOP_OK;
 return 1;
}
