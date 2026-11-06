/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_diagnostic.h"

static void ziyi_hfi_diagnostic_clear(void *p,size_t n){uint8_t *b=p;for(size_t i=0;i<n;i++)b[i]=0;}
static int ziyi_hfi_diagnostic_qualified(struct g19_hfi_runtime *s){
 const struct gh_live_io *io=&s->ops->live;
 int ok=io->qualified(io->ctx,s->live.epoch);
 if(!ok)s->record->owner_failed=1;
 return ok;
}
static int ziyi_hfi_diagnostic_qindex(uint32_t offset){
 if(offset==64)return 0;if(offset==68)return 1;
 if(offset==112)return 2;if(offset==116)return 3;return -1;
}
static void ziyi_hfi_diagnostic_observed(struct g19_hfi_record *r,unsigned i,uint32_t value){
 if(!(r->first_mask&(1u<<i))){r->queue_first[i]=value;r->first_mask|=1u<<i;}
 r->queue_last[i]=value;r->last_mask|=1u<<i;
}
static int ziyi_hfi_diagnostic_owner(void *ctx,uint64_t epoch){
 struct g19_hfi_runtime *s=ctx;
 return epoch==s->live.epoch&&ziyi_hfi_diagnostic_qualified(s);
}
static int ziyi_hfi_diagnostic_read_word(void *ctx,uint32_t offset,uint32_t *value){
 struct g19_hfi_runtime *s=ctx;struct g19_hfi_record *r=s->record;
 if(!s->ops->live.read32(s->ops->live.ctx,offset,value))return 0;
 int i=ziyi_hfi_diagnostic_qindex(offset);if(i>=0)ziyi_hfi_diagnostic_observed(r,(unsigned)i,*value);
 if(s->capture_reply){
  if(offset==112)s->reply_read=*value;
  uint32_t base=GN_HFI_QUEUE0+4096u;
  if(offset>=base&&offset<base+4096u&&s->reply_read<GN_HFI_WORDS){
   uint32_t at=(offset-base)/4u,delta=(at+GN_HFI_WORDS-s->reply_read)%GN_HFI_WORDS;
   if(delta<19&&delta==r->raw_count){
    r->raw[r->raw_count++]=*value;
    if(!delta)r->raw_declared=(*value>>8)&255u;
   }
  }
 }
 return 1;
}
static int ziyi_hfi_diagnostic_write_word(void *ctx,uint32_t offset,uint32_t value){
 struct g19_hfi_runtime *s=ctx;struct g19_hfi_record *r=s->record;
 /* Never write the GMU-owned q0 read or q1 write indexes, nor any q1 data. */
 if(offset!=68&&offset!=112&&!(offset>=GN_HFI_QUEUE0&&offset<GN_HFI_QUEUE0+4096u))return 0;
 if(!s->ops->live.write32(s->ops->live.ctx,offset,value))return 0;
 r->stores++;
 if(offset==68){r->q0_write_stores++;r->flags|=G19_HFI_PUBLISHED;ziyi_hfi_diagnostic_observed(r,1,value);}
 if(offset==112){r->q1_read_stores++;r->flags|=G19_HFI_CONSUMED;ziyi_hfi_diagnostic_observed(r,2,value);}
 return 1;
}
static int ziyi_hfi_diagnostic_order(void *ctx,enum gh_live_order o){
 struct g19_hfi_runtime *s=ctx;return s->ops->live.order(s->ops->live.ctx,o);
}
static int ziyi_hfi_diagnostic_bell(void *ctx){
 struct g19_hfi_runtime *s=ctx;s->record->doorbells++;s->record->flags|=G19_HFI_BELL_CALLED;
 return s->ops->live.doorbell(s->ops->live.ctx);
}
static int ziyi_hfi_diagnostic_clock_check(struct g19_hfi_runtime *s){
 struct g19_hfi_record *r=s->record;uint64_t now=0;
 if(!s->ops->ticks(s->ops->ctx,&now))return G19_HFI_CLOCK;
 if(!(r->flags&G19_HFI_TICKS)){
  r->start_tick=r->last_tick=now;r->flags|=G19_HFI_TICKS;return 0;
 }
 if(now<r->last_tick)return G19_HFI_CLOCK;
 r->last_tick=now;
 return now-r->start_tick>=r->timeout_ticks?G19_HFI_TIMEOUT:0;
}
static int ziyi_hfi_diagnostic_health(struct g19_hfi_runtime *s,int ready){
 struct g19_hfi_record *r=s->record;struct g19_hfi_health h={0};
 if(!ziyi_hfi_diagnostic_qualified(s))return G19_HFI_OWNERSHIP;
 int ok=s->ops->health(s->ops->ctx,&h);
 if(!(r->flags&G19_HFI_FIRST_HEALTH))r->first_health=h;
 r->last_health=h;
 if(!ok)return G19_HFI_HEALTH;
 r->flags|=G19_HFI_FIRST_HEALTH|G19_HFI_LAST_HEALTH;
 if(!ziyi_hfi_diagnostic_qualified(s))return G19_HFI_OWNERSHIP;
 if(h.reset!=0||(h.fw_result&0x1ffu)!=0x100u||(ready&&!(h.hfi_status&1u)))return G19_HFI_HEALTH;
 return 0;
}
static int ziyi_hfi_diagnostic_snapshot(struct g19_hfi_runtime *s){
 static const uint32_t offsets[4]={64,68,112,116};
 if(!ziyi_hfi_diagnostic_qualified(s))return G19_HFI_OWNERSHIP;
 for(unsigned i=0;i<4;i++){
  uint32_t value;
  if(!ziyi_hfi_diagnostic_qualified(s)||!ziyi_hfi_diagnostic_read_word(s,offsets[i],&value))return G19_HFI_SNAPSHOT;
 }
 if(!s->ops->live.order(s->ops->live.ctx,GH_LIVE_ACQUIRE)||!ziyi_hfi_diagnostic_qualified(s))return G19_HFI_SNAPSHOT;
 return 0;
}
static int ziyi_hfi_diagnostic_budget(struct g19_hfi_runtime *s,int ack){
 int e=ziyi_hfi_diagnostic_clock_check(s);if(e)return e;
 if(s->record->polls>=s->record->poll_cap)return G19_HFI_POLL_CAP;
 s->record->polls++;
 if(ack)s->record->ack_polls++;else s->record->ready_polls++;
 return 0;
}
static int ziyi_hfi_diagnostic_pause_poll(struct g19_hfi_runtime *s){
 int e=ziyi_hfi_diagnostic_clock_check(s);if(e)return e;
 if(!s->ops->delay(s->ops->ctx,s->record->delay_us))return G19_HFI_DELAY;
 return ziyi_hfi_diagnostic_clock_check(s);
}
static int ziyi_hfi_diagnostic_finish(struct g19_hfi_runtime *s,int error){
 struct g19_hfi_record *r=s->record;
 /* Best-effort read-only final observation; it never conceals the first error. */
 if(ziyi_hfi_diagnostic_qualified(s)){int e=ziyi_hfi_diagnostic_snapshot(s);if(!error&&e)error=e;}
 else if(!error)error=G19_HFI_OWNERSHIP;
 if(error&&s->live.state!=GH_LIVE_COMPLETE)s->live.state=GH_LIVE_POISON;
 r->live_state=s->live.state;r->remote_status=s->live.remote_status;r->version=s->live.version;
 r->error=(uint32_t)error;
 if(!error)r->stage=G19_HFI_COMPLETE;
 return error;
}
int g19_hfi_run(struct g19_hfi_runtime *s,const struct g19_hfi_config *c,
 const struct g19_hfi_ops *ops,struct g19_hfi_record *r){
 if(!s||!r)return G19_HFI_ARGUMENT;
 if(s->attempted)return GH_LIVE_STATE;
 ziyi_hfi_diagnostic_clear(r,sizeof(*r));r->stage=G19_HFI_VALIDATE;r->flags=G19_HFI_RETAIN;
 if(!c||!ops||!ops->live.qualified||!ops->live.read32||!ops->live.write32||!ops->live.order||
    !ops->live.doorbell||!ops->health||!ops->start_hfi||!ops->ticks||!ops->delay){r->error=G19_HFI_ARGUMENT;return G19_HFI_ARGUMENT;}
 r->epoch=c->epoch;r->gmu_address=c->gmu_address;r->sequence=c->sequence;r->poll_cap=c->poll_cap;
 r->timeout_ticks=c->timeout_ticks;r->delay_us=c->delay_us;
 if(!c->epoch||!c->timeout_ticks||c->timeout_ticks>UINT64_MAX/2||!c->poll_cap||c->poll_cap>G19_HFI_MAX_POLLS||
    c->delay_us>1000000u||c->gmu_address<0x60000000u||c->gmu_address>0x80000000u-GN_HFI_BYTES||
    (c->gmu_address&4095u)||gh_version_command(c->sequence,r->sent)){
  r->error=G19_HFI_ARGUMENT;return G19_HFI_ARGUMENT;
 }
 ziyi_hfi_diagnostic_clear(&s->live,sizeof(s->live));s->live.epoch=c->epoch;s->live.gmu_address=c->gmu_address;
 s->ops=ops;s->record=r;s->attempted=1;s->capture_reply=0;s->reply_read=0;
 r->flags|=G19_HFI_PLANNED;
 struct gh_live_io wrapped={s,ziyi_hfi_diagnostic_owner,ziyi_hfi_diagnostic_read_word,ziyi_hfi_diagnostic_write_word,ziyi_hfi_diagnostic_order,ziyi_hfi_diagnostic_bell};
 int e=ziyi_hfi_diagnostic_clock_check(s);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 r->stage=G19_HFI_OWNER;if(!ziyi_hfi_diagnostic_qualified(s))return ziyi_hfi_diagnostic_finish(s,G19_HFI_OWNERSHIP);
 r->stage=G19_HFI_PREHEALTH;e=ziyi_hfi_diagnostic_health(s,0);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 /* HFI_STATUS=1 after fresh FW100 is an observation, not stale-start proof.
  * The pre-CM3 fresh-zero check belongs to the native startup adapter. */
 e=ziyi_hfi_diagnostic_clock_check(s);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 r->stage=G19_HFI_INITIALIZE;
 if(!ziyi_hfi_diagnostic_qualified(s))return ziyi_hfi_diagnostic_finish(s,G19_HFI_OWNERSHIP);
 r->start_calls=1;r->flags|=G19_HFI_START_CALLED;
 if(!ops->start_hfi(ops->ctx))return ziyi_hfi_diagnostic_finish(s,G19_HFI_START);
 r->flags|=G19_HFI_START_RETURNED;
 r->stage=G19_HFI_READY;
 for(;;){
  e=ziyi_hfi_diagnostic_budget(s,0);if(e)return ziyi_hfi_diagnostic_finish(s,e);
  e=ziyi_hfi_diagnostic_health(s,0);if(e)return ziyi_hfi_diagnostic_finish(s,e);
  if(r->last_health.hfi_status&1u)break;
  e=ziyi_hfi_diagnostic_pause_poll(s);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 }
 r->stage=G19_HFI_PUBLISH;
 e=ziyi_hfi_diagnostic_snapshot(s);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 for(unsigned i=0;i<4;i++)if(r->queue_last[i]>=GN_HFI_WORDS||(r->queue_last[i]&3u))return ziyi_hfi_diagnostic_finish(s,GN_QUEUE);
 if(r->queue_last[0]!=r->queue_last[1])return ziyi_hfi_diagnostic_finish(s,GH_LIVE_STATE);
 if(r->queue_last[2]!=r->queue_last[3])return ziyi_hfi_diagnostic_finish(s,GN_STALE);
 e=ziyi_hfi_diagnostic_clock_check(s);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 e=gh_live_request(&s->live,&wrapped,c->sequence);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 r->stage=G19_HFI_ACK;
 for(;;){
  e=ziyi_hfi_diagnostic_budget(s,1);if(e)return ziyi_hfi_diagnostic_finish(s,e);
  e=ziyi_hfi_diagnostic_health(s,1);if(e)return ziyi_hfi_diagnostic_finish(s,e);
  s->capture_reply=1;e=gh_live_poll(&s->live,&wrapped);s->capture_reply=0;
  if(e!=GN_EMPTY){
   if(e==GN_OK||e==GH_REMOTE_ERROR){int time_error=ziyi_hfi_diagnostic_clock_check(s);if(time_error)e=time_error;}
   return ziyi_hfi_diagnostic_finish(s,e);
  }
  e=ziyi_hfi_diagnostic_pause_poll(s);if(e)return ziyi_hfi_diagnostic_finish(s,e);
 }
}
