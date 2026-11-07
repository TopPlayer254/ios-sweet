/* SPDX-License-Identifier: GPL-2.0-only
 * One finite legacy-A6xx startup batch after the qualified G22 version ACK.
 * PERF -> BW -> CORE_FW_START -> START -> lowest GX/BW vote. No GX OOB,
 * core MMIO, SMMU modification, CP, ZAP, rendering or user client here.
 */
#include "ziyi_g23_startup.h"
#include <string.h>
uint32_t gs_crc(const uint8_t*d,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=d[i];for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
static uint32_t gs_private_get(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void gs_private_put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
int gs_prepare(struct gs_program*p,const struct gr_resources*r,uint64_t epoch,uint32_t address,uint32_t peak){
 if(!p||!r||!epoch||address<0x60000000||address>0x80000000-GN_HFI_BYTES||(address&4095))return GS_ARGUMENT;
 uint32_t perf[59],bandwidth[160];
 if(gr_perf_lowest(r,2,perf)||gr_bandwidth(r,3,peak,bandwidth))return GS_ARGUMENT;
 memset(p,0,sizeof(*p));p->epoch=epoch;p->gmu_address=address;p->peak_kbps=peak;p->resources=*r;
 p->packet[0].count=59;p->packet[1].count=160;p->packet[2].count=2;p->packet[3].count=1;p->packet[4].count=4;
 memcpy(p->packet[0].words,perf,sizeof(perf));memcpy(p->packet[1].words,bandwidth,sizeof(bandwidth));
 p->packet[2].words[0]=0x0040020e;p->packet[3].words[0]=0x0050010a;
 p->packet[4].words[0]=0x0060041e;p->packet[4].words[1]=1;p->packet[4].words[2]=1;p->packet[4].words[3]=1;
 return GS_OK;
}
static int gs_private_program_valid(const struct gs_program*p){
 static const uint32_t ids[5]={4,3,14,10,30},counts[5]={59,160,2,1,4};
 if(!p||!p->epoch||p->gmu_address<0x60000000||p->gmu_address>0x80000000-GN_HFI_BYTES||(p->gmu_address&4095)||!p->peak_kbps)return 0;
 for(unsigned i=0;i<5;i++)if(p->packet[i].count!=counts[i]||p->packet[i].words[0]!=((i+2)<<20|counts[i]<<8|ids[i]))return 0;
 uint32_t perf[59],bandwidth[160];
 if(gr_perf_lowest(&p->resources,2,perf)||gr_bandwidth(&p->resources,3,p->peak_kbps,bandwidth)||memcmp(perf,p->packet[0].words,sizeof(perf))||memcmp(bandwidth,p->packet[1].words,sizeof(bandwidth)))return 0;
 for(unsigned i=0;i<5;i++)for(unsigned n=counts[i];n<GS_PACKET_WORDS;n++)if(p->packet[i].words[n])return 0;
 return p->packet[2].words[1]==0&&p->packet[4].words[1]==1&&p->packet[4].words[2]==1&&p->packet[4].words[3]==1;
}
static int gs_private_poison(struct gs_record*r,uint32_t e){r->state=GS_POISON;r->error=e;return (int)e;}
static int gs_private_owner(const struct gs_program*p,const struct gs_io*o){return o->qualified(o->ctx,p->epoch)&&o->healthy(o->ctx);}
static int gs_private_load(const struct gs_program*p,const struct gs_io*o,uint32_t at,uint32_t*v){return !(at&3)&&at<=GN_HFI_BYTES-4&&gs_private_owner(p,o)&&o->read32(o->ctx,at,v);}
static int gs_private_store(const struct gs_program*p,const struct gs_io*o,uint32_t at,uint32_t v,struct gs_step*s){
 if(!gs_private_owner(p,o)||!o->write32(o->ctx,at,v))return 0;s->stores++;return 1;
}
static int gs_private_queue(const struct gs_program*p,const struct gs_io*o,unsigned i,struct gn_hfi_queue*q,struct gs_record*r){
 static const uint32_t table[6]={0,210,6,12,3,3};uint32_t v;
 r->queue_key=2*i;r->queue_seen=0;memset(r->queue_values,0,sizeof(r->queue_values));
 for(unsigned n=0;n<6;n++){
  if(!gs_private_load(p,o,4*n,&v))return 0;r->queue_values[n]=v;r->queue_seen|=1u<<n;if(v!=table[n])return 0;
 }
 uint32_t at=24+48*i;memset(q,0,sizeof(*q));r->queue_key=2*i+1;r->queue_seen=0;memset(r->queue_values,0,sizeof(r->queue_values));
 uint32_t*dst[6]={&q->status,&q->start,&q->type,&q->words,&q->read,&q->write};static const uint32_t offsets[6]={0,4,8,12,40,44};
 for(unsigned n=0;n<6;n++){
  if(!gs_private_load(p,o,at+offsets[n],dst[n]))return 0;r->queue_values[n]=*dst[n];r->queue_seen|=1u<<n;
 }
 return o->order(o->ctx,GS_ACQUIRE)&&gs_private_owner(p,o)&&q->status==1&&q->start==p->gmu_address+GN_HFI_QUEUE0+4096*i&&q->type==0xa00+i&&q->words==1024&&q->read<1024&&q->write<1024&&!((q->read|q->write)&3);
}

__attribute__((noinline)) int gs_run(const struct gs_program*p,const struct gs_io*o,uint64_t frequency,struct gs_record*r){
 if(!r)return GS_ARGUMENT;if(r->state!=GS_PREPARED)return GS_STATE;
 if(!gs_private_program_valid(p)||!o||!o->qualified||!o->read32||!o->write32||!o->order||!o->doorbell||!o->healthy||!o->now||!o->delay||frequency!=19200000||r->epoch!=p->epoch||r->found!=127||r->resource_crc!=gs_crc((const uint8_t*)&p->resources,sizeof(p->resources))||!r->header_equal||!r->resources_equal)return gs_private_poison(r,GS_ARGUMENT);
 if(!gs_private_owner(p,o))return gs_private_poison(r,GS_OWNER);
 uint64_t now;if(!o->now(o->ctx,&now))return gs_private_poison(r,GS_CLOCK);r->start=r->end=now;r->frequency=frequency;r->peak_kbps=p->peak_kbps;r->state=GS_RUNNING;
 for(unsigned index=0;index<5;index++){
  if(!gs_private_program_valid(p)||r->resource_crc!=gs_crc((const uint8_t*)&p->resources,sizeof(p->resources)))return gs_private_poison(r,GS_PACKET);
  r->current=index;struct gs_step*s=&r->step[index];const struct gs_packet*m=&p->packet[index];struct gn_hfi_queue q,a;
  s->header=m->words[0];s->crc=gs_crc((const uint8_t*)m->words,4*m->count);
  if(!gs_private_queue(p,o,0,&q,r)||!gs_private_queue(p,o,1,&a,r)||q.read!=q.write||a.read!=a.write)return gs_private_poison(r,GS_QUEUE);
  uint64_t start=now;uint32_t aligned=(m->count+3)&~3u;if(aligned>=1024)return gs_private_poison(r,GS_PACKET);
  for(unsigned n=0;n<aligned;n++)if(!gs_private_store(p,o,GN_HFI_QUEUE0+4*((q.write+n)%1024),n<m->count?m->words[n]:0xfafafafa,s))return gs_private_poison(r,GS_IO);
  s->q0_write=(q.write+aligned)%1024;
  if(!o->order(o->ctx,GS_RELEASE)||!gs_private_store(p,o,68,s->q0_write,s))return gs_private_poison(r,GS_IO);
  if(!gs_private_owner(p,o))return gs_private_poison(r,GS_OWNER);
  r->calls++;
  if(!o->doorbell(o->ctx,index,m))return gs_private_poison(r,GS_IO);
  r->bell_returned|=1u<<index;
  for(;;){
   if(!o->now(o->ctx,&now)||now<r->end)return gs_private_poison(r,GS_CLOCK);r->end=now;
   if(now-start>=5*frequency||now-r->start>=25*frequency||s->polls>=50000)return gs_private_poison(r,GS_TIMEOUT);
   s->polls++;
   if(!gs_private_queue(p,o,1,&a,r))return gs_private_poison(r,GS_QUEUE);
   uint32_t used=(a.write+1024-a.read)%1024;
   if(!used){if(!o->delay(o->ctx,100))return gs_private_poison(r,GS_IO);continue;}
   uint32_t h;if(!gs_private_load(p,o,GN_HFI_QUEUE0+4096+4*a.read,&h))return gs_private_poison(r,GS_IO);
   uint32_t words=h>>8&255,consume=(words+3)&~3u;s->q1_write=a.write;s->raw_declared=words;s->raw_count=1;s->raw[0]=h;
   if(words<3||words>19||consume>used)return gs_private_poison(r,GS_PACKET);
   for(unsigned n=1;n<words;n++){if(!gs_private_load(p,o,GN_HFI_QUEUE0+4096+4*((a.read+n)%1024),&s->raw[n]))return gs_private_poison(r,GS_IO);s->raw_count++;}
   if(((h>>16)&15)!=1||s->raw[1]!=s->header)return gs_private_poison(r,GS_PACKET);
   s->remote=s->raw[2];s->q1_read=(a.read+consume)%1024;
   if(!o->order(o->ctx,GS_RELEASE)||!gs_private_store(p,o,112,s->q1_read,s))return gs_private_poison(r,GS_IO);
   if(s->remote)return gs_private_poison(r,GS_REMOTE);
   if(!gs_private_queue(p,o,0,&q,r)||q.read!=s->q0_write||q.write!=s->q0_write)return gs_private_poison(r,GS_QUEUE);
   s->complete=1;r->acks++;break;
  }
 }
 r->current=5;r->state=GS_COMPLETE;return GS_OK;
}
static int gs_private_record_valid(const struct gs_record*r){
 if(!r||r->state>GS_POISON||r->current>5||r->queue_key>3||(r->queue_seen&~63u)||r->calls>5||r->acks>r->calls||(r->bell_returned&~31u)||r->found&~127u||r->header_equal>1||r->resources_equal>1)return 0;
 if(r->state==GS_COMPLETE&&(r->error||r->acks!=5||r->calls!=5||r->current!=5||r->bell_returned!=31||r->found!=127||!r->epoch||!r->resources_equal||!r->header_equal))return 0;
 for(unsigned n=0;n<6;n++)if(!(r->queue_seen&(1u<<n))&&r->queue_values[n])return 0;
 if(r->frequency&&r->frequency!=19200000)return 0;if(r->end<r->start)return 0;
 for(unsigned i=0;i<5;i++){const struct gs_step*s=&r->step[i];if(s->raw_count>19||s->raw_declared>255||s->polls>50000||s->stores>162||s->q0_write>=1024||s->q1_read>=1024||s->q1_write>=1024||s->complete>1)return 0;if(s->complete&&(s->raw_count<3||s->raw_count!=s->raw_declared||!(r->bell_returned&(1u<<i))||s->remote||s->raw[1]!=s->header))return 0;if(s->raw_count&&s->raw_declared!=((s->raw[0]>>8)&255))return 0;for(unsigned n=s->raw_count;n<19;n++)if(s->raw[n])return 0;}
 return 1;
}
__attribute__((noinline)) int gs_encode(const struct gs_record*r,uint8_t*out){
 if(!out||!gs_private_record_valid(r))return GS_ARGUMENT;uint32_t w[192]={0x31545347,1,768,0,r->state,r->error,r->current,r->calls,r->acks,r->resource_crc,r->header_crc,r->found,r->entries,r->peak_kbps,r->header_equal,r->resources_equal,r->read_words,(uint32_t)r->epoch,(uint32_t)(r->epoch>>32)};
 for(unsigned i=0;i<5;i++){const struct gs_step*s=&r->step[i];w[24+i]=s->header;w[29+i]=s->crc;w[34+i]=s->remote;w[39+i]=s->raw_count;for(unsigned n=0;n<19;n++)w[44+19*i+n]=s->raw[n];w[139+i]=s->q0_write;w[144+i]=s->q1_read;w[149+i]=s->q1_write;w[154+i]=s->polls;w[159+i]=s->stores;w[164+i]=s->complete;}
 for(unsigned i=0;i<5;i++){w[169+i]=r->health[i];w[174+i]=r->step[i].raw_declared;}w[179]=r->bell_returned;
 w[19]=r->queue_key;w[20]=r->queue_seen;for(unsigned i=0;i<3;i++){w[21+i]=r->queue_values[i];w[186+i]=r->queue_values[i+3];}
 w[180]=(uint32_t)r->start;w[181]=(uint32_t)(r->start>>32);w[182]=(uint32_t)r->end;w[183]=(uint32_t)(r->end>>32);w[184]=(uint32_t)r->frequency;w[185]=(uint32_t)(r->frequency>>32);
 for(unsigned i=0;i<192;i++)gs_private_put(out+4*i,w[i]);gs_private_put(out+12,gs_crc(out,768));return GS_OK;
}
int gs_decode(const uint8_t*d,size_t bytes,struct gs_record*out){
 if(!d||!out||bytes!=768||gs_private_get(d)!=0x31545347||gs_private_get(d+4)!=1||gs_private_get(d+8)!=768)return GS_ARGUMENT;
 uint8_t temp[768];memcpy(temp,d,768);memset(temp+12,0,4);if(gs_crc(temp,768)!=gs_private_get(d+12))return GS_PACKET;
 uint32_t w[192];for(unsigned i=0;i<192;i++)w[i]=gs_private_get(d+4*i);
 for(unsigned i=189;i<192;i++)if(w[i])return GS_PACKET;
 struct gs_record r={0};r.state=w[4];r.error=w[5];r.current=w[6];r.calls=w[7];r.acks=w[8];r.resource_crc=w[9];r.header_crc=w[10];r.found=w[11];r.entries=w[12];r.peak_kbps=w[13];r.header_equal=w[14];r.resources_equal=w[15];r.read_words=w[16];r.epoch=w[17]|(uint64_t)w[18]<<32;
 for(unsigned i=0;i<5;i++){struct gs_step*s=&r.step[i];s->header=w[24+i];s->crc=w[29+i];s->remote=w[34+i];s->raw_count=w[39+i];for(unsigned n=0;n<19;n++)s->raw[n]=w[44+19*i+n];s->q0_write=w[139+i];s->q1_read=w[144+i];s->q1_write=w[149+i];s->polls=w[154+i];s->stores=w[159+i];s->complete=w[164+i];}
 for(unsigned i=0;i<5;i++){r.health[i]=w[169+i];r.step[i].raw_declared=w[174+i];}r.bell_returned=w[179];
 r.queue_key=w[19];r.queue_seen=w[20];for(unsigned i=0;i<3;i++){r.queue_values[i]=w[21+i];r.queue_values[i+3]=w[186+i];}
 r.start=w[180]|(uint64_t)w[181]<<32;r.end=w[182]|(uint64_t)w[183]<<32;r.frequency=w[184]|(uint64_t)w[185]<<32;
 if(!gs_private_record_valid(&r))return GS_PACKET;*out=r;return GS_OK;
}
