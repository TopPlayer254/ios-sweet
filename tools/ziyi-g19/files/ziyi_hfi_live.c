/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_live.h"
#define Q0 ((uint32_t)offsetof(struct gn_hfi_table,queue[0]))
#define Q1 ((uint32_t)offsetof(struct gn_hfi_table,queue[1]))
#define QR ((uint32_t)offsetof(struct gn_hfi_queue,read))
#define QW ((uint32_t)offsetof(struct gn_hfi_queue,write))
_Static_assert(Q0==24&&Q1==72&&QR==40&&QW==44,"pinned HFIv2 ABI");
static int ziyi_hfi_live_poison(struct gh_live*s,int error){s->state=GH_LIVE_POISON;return error;}
static int ziyi_hfi_live_valid(const struct gh_live*s,const struct gh_live_io*io){return s&&io&&s->epoch&&s->gmu_address>=0x60000000&&s->gmu_address<=0x80000000-GN_HFI_BYTES&&!(s->gmu_address&4095)&&io->qualified&&io->read32&&io->write32&&io->order&&io->doorbell&&io->qualified(io->ctx,s->epoch);}
static int ziyi_hfi_live_load(struct gh_live*s,const struct gh_live_io*io,uint32_t at,uint32_t*out){return ziyi_hfi_live_valid(s,io)&&at<=GN_HFI_BYTES-4&&!(at&3)&&io->read32(io->ctx,at,out);}
static int ziyi_hfi_live_store(struct gh_live*s,const struct gh_live_io*io,uint32_t at,uint32_t v){return ziyi_hfi_live_valid(s,io)&&at<=GN_HFI_BYTES-4&&!(at&3)&&io->write32(io->ctx,at,v);}
static int ziyi_hfi_live_queue(struct gh_live*s,const struct gh_live_io*io,unsigned i,struct gn_hfi_queue*out){
 uint32_t at=i?Q1:Q0;*out=(struct gn_hfi_queue){0};
 static const uint32_t table[6]={0,GN_HFI_TABLE_BYTES/4,6,12,3,3};
 for(unsigned k=0;k<6;k++){uint32_t v;if(!ziyi_hfi_live_load(s,io,k*4,&v))return GH_LIVE_IO;if(v!=table[k])return GN_QUEUE;}
 if(!ziyi_hfi_live_load(s,io,at,&out->status)||!ziyi_hfi_live_load(s,io,at+4,&out->start)||!ziyi_hfi_live_load(s,io,at+8,&out->type)||!ziyi_hfi_live_load(s,io,at+12,&out->words)||!ziyi_hfi_live_load(s,io,at+QR,&out->read)||!ziyi_hfi_live_load(s,io,at+QW,&out->write))return GH_LIVE_IO;
 if(!io->order(io->ctx,GH_LIVE_ACQUIRE)||!ziyi_hfi_live_valid(s,io))return GH_LIVE_OWNER;
 if(out->status!=1||out->start!=s->gmu_address+GN_HFI_QUEUE0+4096*i||out->type!=(0xa00u+i)||out->words!=GN_HFI_WORDS||out->read>=GN_HFI_WORDS||out->write>=GN_HFI_WORDS||(out->read|out->write)&3)return GN_QUEUE;
 return GN_OK;
}
int gh_live_request(struct gh_live*s,const struct gh_live_io*io,uint32_t sequence){
 if(!s)return GN_ARG;if(s->state!=GH_LIVE_FRESH)return GH_LIVE_STATE;
 if(!ziyi_hfi_live_valid(s,io))return ziyi_hfi_live_poison(s,GH_LIVE_OWNER);
 uint32_t message[2];int e=gh_version_command(sequence,message);if(e)return e;
 struct gn_hfi_queue q;e=ziyi_hfi_live_queue(s,io,0,&q);if(e)return ziyi_hfi_live_poison(s,e);
 uint32_t empty=(q.read+GN_HFI_WORDS-q.write)%GN_HFI_WORDS;if(!empty)empty=GN_HFI_WORDS;
 if(empty<=4)return GN_FULL;
 for(unsigned i=0;i<4;i++)if(!ziyi_hfi_live_store(s,io,GN_HFI_QUEUE0+4*((q.write+i)%GN_HFI_WORDS),i<2?message[i]:0xfafafafa))return ziyi_hfi_live_poison(s,GH_LIVE_IO);
 if(!io->order(io->ctx,GH_LIVE_RELEASE)||!ziyi_hfi_live_store(s,io,Q0+QW,(q.write+4)%GN_HFI_WORDS))return ziyi_hfi_live_poison(s,GH_LIVE_IO);
 s->sent=message[0];s->state=GH_LIVE_PENDING;
 if(!ziyi_hfi_live_valid(s,io)||!io->doorbell(io->ctx))return ziyi_hfi_live_poison(s,GH_LIVE_IO);
 return GN_OK;
}
int gh_live_poll(struct gh_live*s,const struct gh_live_io*io){
 if(!s)return GN_ARG;if(s->state!=GH_LIVE_PENDING)return GH_LIVE_STATE;
 if(!ziyi_hfi_live_valid(s,io))return ziyi_hfi_live_poison(s,GH_LIVE_OWNER);
 struct gn_hfi_queue q;int e=ziyi_hfi_live_queue(s,io,1,&q);if(e)return ziyi_hfi_live_poison(s,e);
 uint32_t used=(q.write+GN_HFI_WORDS-q.read)%GN_HFI_WORDS;if(!used)return GN_EMPTY;
 uint32_t header;if(!ziyi_hfi_live_load(s,io,GN_HFI_QUEUE0+4096+4*q.read,&header))return ziyi_hfi_live_poison(s,GH_LIVE_IO);
 uint32_t words=(header>>8)&255,aligned=(words+3)&~3u;
 if(words<3||words>19||aligned>used)return ziyi_hfi_live_poison(s,GN_PACKET);
 s->scratch[q.read]=header;
 for(unsigned i=1;i<words;i++)if(!ziyi_hfi_live_load(s,io,GN_HFI_QUEUE0+4096+4*((q.read+i)%GN_HFI_WORDS),&s->scratch[(q.read+i)%GN_HFI_WORDS]))return ziyi_hfi_live_poison(s,GH_LIVE_IO);
 struct gh_version_reply reply;e=gh_version_reply(s->scratch,&q,s->sent,&reply);
 if(e!=GN_OK&&e!=GH_REMOTE_ERROR)return ziyi_hfi_live_poison(s,e);
 if(!io->order(io->ctx,GH_LIVE_RELEASE)||!ziyi_hfi_live_store(s,io,Q1+QR,reply.next_read))return ziyi_hfi_live_poison(s,GH_LIVE_IO);
 s->next_read=reply.next_read;s->version=reply.version;s->remote_status=reply.status;s->state=GH_LIVE_COMPLETE;
 return e;
}
