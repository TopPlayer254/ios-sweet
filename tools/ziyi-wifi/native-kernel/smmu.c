/* SPDX-License-Identifier: GPL-2.0-only */
#include "smmu.h"
#include <stddef.h>
#ifdef ZWS_HOST
#include "u3_policy.h"
#else
#include "ziyi_u3_policy.h"
#endif
int zws_select(const struct zws_snapshot*a,const struct zws_snapshot*b,struct zws_choice*c)
{
 if(!a||!b||!c||a->scr0!=b->scr0||a->idr0!=b->idr0||a->idr1!=b->idr1||
    (a->scr0&9u)||(a->idr0&255u)<ZWS_SMRS||(a->idr1&255u)<ZWS_CBS)return 0;
 uint32_t page=a->idr1>>31?0x10000u:0x1000u,pages=2u<<((a->idr1>>28)&7u);
 if(2ull*pages*page!=0x100000u||ZWS_CBS>pages)return 0;
 uint32_t chosen=ZWS_SMRS;uint8_t used[ZWS_CBS]={0};
 for(unsigned n=0;n<ZWS_SMRS;n++){
  uint32_t s=a->smr[n],r=a->s2cr[n];
  if(s!=b->smr[n]||r!=b->s2cr[n])return 0;
  if(!(s>>31)){if(chosen==ZWS_SMRS)chosen=n;continue;}
  uint32_t id=s&0x7fffu,mask=(s>>16)&0x7fffu;
  if(!((id^0x1c00u)&~(mask|1u)))return 0;
  unsigned kind=(r>>16)&3u;
  if(kind==3u)return 0;
  /* Bypass/fault S2CR low bits do not reference a translation context. */
  if(kind==0u&&(r&255u)<ZWS_CBS)used[r&255u]=1;
 }
 uint32_t cb=ZWS_CBS;
 for(unsigned n=0;n<ZWS_CBS;n++){
  if(a->sctlr[n]!=b->sctlr[n])return 0;
  if(cb==ZWS_CBS&&!used[n]&&!(a->sctlr[n]&1u))cb=n;
 }
 if(chosen==ZWS_SMRS||cb==ZWS_CBS)return 0;
 *c=(struct zws_choice){chosen,cb,page,pages};return 1;
}

/* Publish one immutable lease before the first SMMU store. UFS can then
 * distinguish this exact disjoint stream from changes to its own mapping.
 * No callbacks, allocations, or storage locks occur under the Wi-Fi mutex. */
static struct {unsigned match,context;uint32_t old_smr,old_s2cr;} storage_lease;
static unsigned storage_lease_state;
/* ARM IHI0062D.c16-320: AFE/TRE[2:1] are UNKNOWN/SBOP for effective EAE1.
 * Prove AArch64 VA64 first; every other written control bit remains exact. */
int zws_context_matches(const uint32_t a[ZWS_CB_WORDS],const uint32_t w[ZWS_CB_WORDS])
{
 if(!a||!w||w[10]!=1||a[10]!=1||w[0]!=0x1127u||w[6]!=0x803519u||w[9]!=0x1f300u)return 0;
 for(unsigned i=0;i<ZWS_CB_WORDS;i++){
  /* IHI0062D.c16-333: reserved TG1=00 may read an implementation-selected
   * granule. TTBR1 stays disabled by exact EPD1=1, all other TCR bits exact.
   * CBAR.BPSHCFG is optionally RAZ/WI; accept written3 or RAZ0 only. */
  if(i==9&&(a[i]&0x300u)!=0&&(a[i]&0x300u)!=0x300u)return 0;
  uint32_t mask=i==0?~6u:i==6?~(3u<<30):i==9?~0x300u:UINT32_MAX;
  if((a[i]&mask)!=(w[i]&mask))return 0;
 }
 return 1;
}
static uint64_t storage_generation;
uint64_t zws_storage_epoch(void){return __atomic_load_n(&storage_generation,__ATOMIC_ACQUIRE);}
uint64_t zws_storage_write_begin(void)
{
 uint64_t old=zws_storage_epoch();
 if((old&1u)||old>=UINT64_MAX-1)return 0;
 return __atomic_compare_exchange_n(&storage_generation,&old,old+1,0,
  __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)?old+1:0;
}
int zws_storage_write_end(uint64_t token)
{
 if(!(token&1u))return 0;
 return __atomic_compare_exchange_n(&storage_generation,&token,token+1,0,
  __ATOMIC_RELEASE,__ATOMIC_RELAXED);
}
int zws_storage_lease(unsigned match,unsigned context,uint32_t old_smr,uint32_t old_s2cr)
{
 if(!match||match>=ZWS_SMRS||!context||context>=ZWS_CBS||(old_smr&0x80000000u))return 0;
 unsigned state=__atomic_load_n(&storage_lease_state,__ATOMIC_ACQUIRE);
 if(state==2)return storage_lease.match==match&&storage_lease.context==context&&
  storage_lease.old_smr==old_smr&&storage_lease.old_s2cr==old_s2cr;
 unsigned zero=0;
 if(!__atomic_compare_exchange_n(&storage_lease_state,&zero,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
 storage_lease.match=match;storage_lease.context=context;
 storage_lease.old_smr=old_smr;storage_lease.old_s2cr=old_s2cr;
 __atomic_store_n(&storage_lease_state,2,__ATOMIC_RELEASE);return 1;
}
static int storage_pair(const struct u3_smmu*s)
{
 unsigned n=storage_lease.match;
 /* Stream-last installation and stream-first restoration have safe prefixes.
  * Never accept an enabled Wi-Fi stream routed to the original context. */
 return (s->smr[n]==storage_lease.old_smr&&
   (s->s2cr[n]==storage_lease.old_s2cr||s->s2cr[n]==storage_lease.context))||
  (s->smr[n]==0x80011c00u&&s->s2cr[n]==storage_lease.context);
}
int zws_storage_coexists(const struct u3_smmu*a,const struct u3_smmu*b,const struct u3_smmu*base)
{
 if(!a||!b||!base||!u3_identity(a)||!u3_identity(b)||!u3_identity(base))return 0;
 if(u1_equal(a,b,sizeof(*a))&&u1_equal(a,base,sizeof(*a)))return 1;
 if(__atomic_load_n(&storage_lease_state,__ATOMIC_ACQUIRE)!=2)return 0;
 if(!storage_pair(a)||!storage_pair(b)||!storage_pair(base))return 0;
 const uint8_t *av=(const uint8_t*)a,*bv=(const uint8_t*)b,*cv=(const uint8_t*)base;
 size_t sm=offsetof(struct u3_smmu,smr)+4*storage_lease.match;
 size_t sc=offsetof(struct u3_smmu,s2cr)+4*storage_lease.match;
 for(size_t i=0;i<sizeof(*a);i++){
  if((i>=sm&&i<sm+4)||(i>=sc&&i<sc+4))continue;
  if(av[i]!=cv[i]||bv[i]!=cv[i])return 0;
 }
 return 1;
}
