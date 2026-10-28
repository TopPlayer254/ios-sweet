/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_wifi_smmu.h"
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
