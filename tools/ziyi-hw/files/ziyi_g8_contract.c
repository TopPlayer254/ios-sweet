/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_contract.h"
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void zero(void *p,size_t n){uint8_t *b=p;for(size_t i=0;i<n;i++)b[i]=0;}
static int contains(uint32_t base,uint32_t extent,uint32_t address,uint32_t size) {
 return size&&address>=base&&address-base<extent&&size<=extent-(address-base);
}
static unsigned kind(uint32_t address,uint32_t bytes) {
 if(contains(0,0x4000,address,bytes))return GN_ITCM;
 if(contains(0x10004000,0x4000,address,bytes))return GN_DTCM;
 if(contains(0x4000,0x1000000-0x4000,address,bytes))return GN_CACHE;
 if(contains(0x60000000,0x20000000,address,bytes))return GN_NC;
 return 0;
}
static int add_region(struct gn_plan *p,uint32_t address,uint32_t bytes,unsigned k) {
 if(p->region_count==GN_MAX_REGIONS||!bytes||bytes>0x100000||address&(GN_PAGE-1))return GN_LIMIT;
 uint32_t size=(bytes+GN_PAGE-1)&~(GN_PAGE-1);
 if(!kind(address,size)||size>0xffffffffu-address)return GN_ADDRESS;
 for(unsigned i=0;i<p->region_count;i++) {
  const struct gn_region *r=&p->regions[i];
  if(address<r->address+r->size&&r->address<address+size)return GN_OVERLAP;
 }
 if(p->backing_bytes>0x1000000-size)return GN_LIMIT;
 p->regions[p->region_count++]=(struct gn_region){address,size,k};p->backing_bytes+=size;return GN_OK;
}
int gn_firmware_plan(const void *source,size_t bytes,struct gn_plan *out) {
 if(!source||!out||!bytes||bytes>0x1000000)return GN_ARG;
 const uint8_t *data=source;struct gn_plan p;zero(&p,sizeof(p));size_t at=0;unsigned payload_seen=0;
 while(at<bytes) {
  if(bytes-at<16)return GN_TRUNCATED;
  if(p.block_count==GN_MAX_BLOCKS)return GN_LIMIT;
  struct gn_block b={u32(data+at),u32(data+at+4),u32(data+at+8),u32(data+at+12),(uint32_t)at+16,0};at+=16;
  if(b.size>bytes-at)return GN_TRUNCATED;
  if((b.address|b.size)&3)return GN_ADDRESS;
  uint32_t span=b.size?b.size:b.value;b.kind=kind(b.address,span);
  if(!b.size) {
   if(payload_seen||!b.kind||(b.type!=1&&b.type!=7)||!b.value)return GN_METADATA;
   p.metadata_blocks++;
   if(b.kind!=GN_ITCM&&b.kind!=GN_DTCM) {int e=add_region(&p,b.address,b.value,b.kind);if(e)return e;}
  } else {
   payload_seen=1;p.data_blocks++;if(!b.kind)return GN_ADDRESS;
   /* Payload type/value are ignored by the vendor loader: a662 uses sentinels.
    * They are kept as observations, never interpreted as metadata or commands. */
   if(b.kind!=GN_ITCM&&b.kind!=GN_DTCM) {
    unsigned found=0;for(unsigned i=0;i<p.region_count;i++)found+=contains(p.regions[i].address,p.regions[i].size,b.address,b.size);
    if(found!=1)return GN_BACKING;
   }
   for(unsigned i=0;i<p.block_count;i++) {const struct gn_block *old=&p.blocks[i];
    if(old->size&&b.address<old->address+old->size&&old->address<b.address+b.size)return GN_OVERLAP;
   }
  }
  p.blocks[p.block_count++]=b;at+=b.size;
 }
 if(!p.metadata_blocks||!p.data_blocks)return GN_METADATA;
 uint32_t after=0x60000000;
 for(unsigned i=0;i<p.region_count;i++)if(p.regions[i].kind==GN_NC&&p.regions[i].address+p.regions[i].size>after)after=p.regions[i].address+p.regions[i].size;
 if(after>0x80000000-0x8000)return GN_ADDRESS;
 p.log_address=after;p.hfi_address=after+0x4000;
 int error=add_region(&p,p.log_address,0x4000,GN_LOG);if(error)return error;
 error=add_region(&p,p.hfi_address,GN_HFI_BYTES,GN_HFI);if(error)return error;
 volatile uint8_t *dst=(volatile uint8_t*)out;const uint8_t *src=(const uint8_t*)&p;
 for(size_t i=0;i<sizeof(p);i++)dst[i]=src[i];return GN_OK;
}
int gn_hfi_init(void *memory,size_t bytes,uint32_t address) {
 if(!memory||bytes<GN_HFI_BYTES||(uintptr_t)memory%4||address%GN_PAGE||!contains(0x60000000,0x20000000,address,GN_HFI_BYTES))return GN_ARG;
 zero(memory,GN_HFI_BYTES);struct gn_hfi_table *t=memory;
 t->words=sizeof(*t)/4;t->first_queue=6;t->queue_words=12;t->queues=t->active_queues=GN_HFI_QUEUES;
 static const uint8_t priority[]={10,10,40};
 for(unsigned i=0;i<GN_HFI_QUEUES;i++)t->queue[i]=(struct gn_hfi_queue){.status=1,.start=address+GN_HFI_QUEUE0+4096*i,.type=i|(uint32_t)priority[i]<<8,.words=GN_HFI_WORDS};
 return GN_OK;
}
int gn_hfi_sequence(uint32_t *sequence,uint32_t *next) {
 if(!sequence||!next)return GN_ARG;if(*sequence>=4095)return GN_WRAP;
 *next=++*sequence;return GN_OK;
}
int gn_hfi_header(uint32_t id,uint32_t words,uint32_t sequence,uint32_t *header) {
 if(!header||id>255||!words||words>255||!sequence||sequence>4095)return GN_ARG;
 *header=id|words<<8|sequence<<20;return GN_OK;
}
static int queue_valid(const struct gn_hfi_queue *q) {
 return q&&q->status==1&&q->words==GN_HFI_WORDS&&q->read<GN_HFI_WORDS&&q->write<GN_HFI_WORDS&&!(q->read&3)&&!(q->write&3);
}
int gn_hfi_enqueue(uint32_t ring[GN_HFI_WORDS],const struct gn_hfi_queue *q,const uint32_t *msg,uint32_t words,uint32_t *next) {
 if(!ring||!msg||!next||!queue_valid(q)||q->type!=0xa00)return GN_QUEUE;
 if(!words||words>255||((msg[0]>>8)&255)!=words||((msg[0]>>16)&15)||!(msg[0]>>20))return GN_PACKET;
 uint32_t aligned=(words+3)&~3u,empty=q->write>=q->read?GN_HFI_WORDS-(q->write-q->read):q->read-q->write;
 if(empty<=aligned)return GN_FULL;
 uint32_t staged[255];for(unsigned i=0;i<words;i++)staged[i]=msg[i];
 uint32_t at=q->write;for(unsigned i=0;i<aligned;i++){ring[at]=i<words?staged[i]:0xfafafafa;at=(at+1)%GN_HFI_WORDS;}
 *next=at;return GN_OK;
}
int gn_hfi_ack(const uint32_t ring[GN_HFI_WORDS],const struct gn_hfi_queue *q,uint32_t sent,uint32_t *verdict,uint32_t *next) {
 if(!ring||!verdict||!next||!queue_valid(q)||q->type!=0xa01)return GN_QUEUE;
 if(!((sent>>8)&255)||((sent>>16)&15)||!(sent>>20))return GN_ARG;
 uint32_t used=(q->write+GN_HFI_WORDS-q->read)%GN_HFI_WORDS;if(!used)return GN_EMPTY;
 uint32_t h=ring[q->read],words=(h>>8)&255,aligned=(words+3)&~3u;
 if(words<3||words>19||aligned>used||((h>>16)&15)!=1)return GN_PACKET;
 if(ring[(q->read+1)%GN_HFI_WORDS]!=sent)return GN_STALE;
 *verdict=ring[(q->read+2)%GN_HFI_WORDS];*next=(q->read+aligned)%GN_HFI_WORDS;return GN_OK;
}
