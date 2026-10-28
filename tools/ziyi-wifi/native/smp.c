/* SPDX-License-Identifier: GPL-2.0-only
 * Bounded SMP2P parsing and one-bit preparation. Publication/doorbells belong
 * to the native single-writer APSS item owner; this file never writes hardware.
 */
#include "native.h"
static uint32_t le32(const uint8_t*p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint16_t le16(const uint8_t*p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static int equal_name(const uint8_t*p,const char*name,size_t n)
{
 for(size_t i=0;i<n;i++)if(p[i]!=(uint8_t)name[i])return 0;
 return n==16||p[n]==0;
}
int zwn_smp_parse(const uint8_t*p,size_t size,uint16_t local,uint16_t remote,
 const char*entry,struct zwn_smp_view*out)
{
 if(!p||!out||!entry||size<340||!((local==0&&remote==13)||(local==13&&remote==0)))return 0;
 size_t n=0;while(n<17&&entry[n])n++;
 if(n==0||n>16||le32(p)!=0x504d5324u||p[4]!=1||le16(p+8)!=local||le16(p+10)!=remote)return 0;
 unsigned capacity=le16(p+12),valid=le16(p+14);
 if(capacity!=16||valid>capacity)return 0;
 struct zwn_smp_view v={.features=le32(p+4)>>8,.flags=le32(p+16),.capacity=capacity,.valid=valid};
 unsigned matches=0;
 for(unsigned i=0;i<valid;i++)if(equal_name(p+20+20*i,entry,n)){
  v.stop_offset=36+20*i;v.state=le32(p+v.stop_offset);matches++;
 }
 if(matches!=1)return 0;
 if(local==13){v.fatal=!!(v.state&1);v.ready=!!(v.state&2);v.handover=!!(v.state&4);v.stop_ack=!!(v.state&8);}
 *out=v;return 1;
}
int zwn_smp_prepare_stop(const uint8_t*in,size_t size,uint8_t*out,size_t capacity,
 int request,struct zwn_smp_view*view)
{
 if(!out||!view||capacity<size||(request!=0&&request!=1))return 0;
 struct zwn_smp_view v;
 if(!zwn_smp_parse(in,size,0,13,"master-kernel",&v))return 0;
 if(request&&(v.state&1))return 0; /* an old request cannot prove a fresh ACK */
 uintptr_t a=(uintptr_t)in,b=(uintptr_t)out;
 if(in!=out&&((a<b&&b-a<size)||(b<a&&a-b<size)))return 0;
 for(size_t i=0;i<size;i++)out[i]=in[i];
 uint32_t next=(v.state&~1u)|(unsigned)request;
 for(unsigned i=0;i<4;i++)out[v.stop_offset+i]=(uint8_t)(next>>(8*i));
 v.state=next;*view=v;return 1;
}
