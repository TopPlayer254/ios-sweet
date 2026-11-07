/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_hfi_diagnostic.h"

static uint32_t ziyi_hfi_record_update(uint32_t crc,uint8_t b){
 crc^=b;for(unsigned k=0;k<8;k++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));return crc;
}
uint32_t g19_hfi_crc32(const uint8_t *data,size_t bytes){
 if(!data&&bytes)return 0;uint32_t crc=~0u;
 for(size_t i=0;i<bytes;i++)crc=ziyi_hfi_record_update(crc,data[i]);return ~crc;
}
static uint32_t ziyi_hfi_record_checksum(const uint8_t *data){
 uint32_t crc=~0u;for(unsigned i=0;i<G19_HFI_RECORD_BYTES;i++)crc=ziyi_hfi_record_update(crc,i>=12&&i<16?0:data[i]);return ~crc;
}
static uint32_t ziyi_hfi_record_get(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void ziyi_hfi_record_put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void ziyi_hfi_record_health_put(uint32_t *w,const struct g19_hfi_health *h){
 w[0]=h->fw_result;w[1]=h->hfi_status;w[2]=h->reset;w[3]=h->helper_phase;w[4]=h->helper_error;
}
static void ziyi_hfi_record_health_get(struct g19_hfi_health *h,const uint32_t *w){
 *h=(struct g19_hfi_health){w[0],w[1],w[2],w[3],w[4]};
}
static void ziyi_hfi_record_wide_put(uint32_t *w,uint64_t v){w[0]=(uint32_t)v;w[1]=(uint32_t)(v>>32);}
static uint64_t ziyi_hfi_record_wide_get(const uint32_t *w){return w[0]|(uint64_t)w[1]<<32;}
static int ziyi_hfi_record_valid(const struct g19_hfi_record *r){
 if(!r||r->stage<G19_HFI_VALIDATE||r->stage>G19_HFI_COMPLETE||r->live_state>GH_LIVE_POISON||
    (r->flags&~1023u)||!(r->flags&G19_HFI_RETAIN)||r->raw_count>19||r->raw_declared>255||
    r->owner_failed>1||r->first_mask>15||r->last_mask>15||r->start_calls>1||r->doorbells>1||
    r->q0_write_stores>1||r->q1_read_stores>1||r->stores>6)return 0;
 if(r->polls!=(uint64_t)r->ready_polls+r->ack_polls)return 0;
 if((r->flags&G19_HFI_PLANNED)){
  uint32_t planned[2];if(gh_version_command(r->sequence,planned)||r->sent[0]!=planned[0]||r->sent[1]!=planned[1])return 0;
 }
 if(!!(r->flags&G19_HFI_START_CALLED)!=!!r->start_calls||
    !!(r->flags&G19_HFI_BELL_CALLED)!=!!r->doorbells||
    !!(r->flags&G19_HFI_PUBLISHED)!=!!r->q0_write_stores||
    !!(r->flags&G19_HFI_CONSUMED)!=!!r->q1_read_stores)return 0;
 if((r->flags&G19_HFI_START_RETURNED)&&!(r->flags&G19_HFI_START_CALLED))return 0;
 if((r->flags&G19_HFI_PUBLISHED)&&(!(r->flags&G19_HFI_PLANNED)||r->stores<5))return 0;
 if((r->flags&G19_HFI_BELL_CALLED)&&!(r->flags&G19_HFI_PUBLISHED))return 0;
 if((r->flags&G19_HFI_CONSUMED)&&(!(r->flags&G19_HFI_PUBLISHED)||r->raw_count<3))return 0;
 if(r->raw_count&&r->raw_declared!=((r->raw[0]>>8)&255u))return 0;
 for(unsigned i=r->raw_count;i<19;i++)if(r->raw[i])return 0;
 if(!r->error&&(r->stage!=G19_HFI_COMPLETE||r->live_state!=GH_LIVE_COMPLETE||
    !(r->flags&G19_HFI_CONSUMED)||r->remote_status||r->raw_count<4||!gh_version_response_supported(r->version)))return 0;
 if((r->flags&G19_HFI_TICKS)&&r->last_tick<r->start_tick)return 0;
 return 1;
}
int g19_hfi_record_encode(const struct g19_hfi_record *r,uint8_t out[G19_HFI_RECORD_BYTES]){
 if(!out||!ziyi_hfi_record_valid(r))return G19_HFI_RECORD;
 uint32_t w[80]={G19_HFI_RECORD_MAGIC,1,G19_HFI_RECORD_BYTES,0};
 w[4]=r->stage;w[5]=r->error;w[6]=r->flags;w[7]=r->live_state;w[8]=r->sequence;
 w[9]=r->sent[0];w[10]=r->sent[1];w[11]=r->poll_cap;w[12]=r->polls;w[13]=r->ready_polls;w[14]=r->ack_polls;
 w[15]=r->remote_status;w[16]=r->version;w[17]=r->raw_declared;w[18]=r->raw_count;w[19]=r->owner_failed;
 w[20]=r->first_mask;w[21]=r->last_mask;
 for(unsigned i=0;i<4;i++){w[22+i]=r->queue_first[i];w[26+i]=r->queue_last[i];}
 for(unsigned i=0;i<19;i++)w[30+i]=r->raw[i];
 ziyi_hfi_record_health_put(w+49,&r->first_health);ziyi_hfi_record_health_put(w+54,&r->last_health);
 w[59]=r->gmu_address;w[60]=r->stores;w[61]=r->q0_write_stores;w[62]=r->q1_read_stores;
 ziyi_hfi_record_wide_put(w+63,r->epoch);ziyi_hfi_record_wide_put(w+65,r->start_tick);ziyi_hfi_record_wide_put(w+67,r->last_tick);ziyi_hfi_record_wide_put(w+69,r->timeout_ticks);
 w[71]=r->delay_us;w[72]=r->start_calls;w[73]=r->doorbells;
 for(unsigned i=0;i<80;i++)ziyi_hfi_record_put(out+4*i,w[i]);ziyi_hfi_record_put(out+12,ziyi_hfi_record_checksum(out));return 0;
}
int g19_hfi_record_decode(const uint8_t *data,size_t bytes,struct g19_hfi_record *out){
 if(!data||!out||bytes!=G19_HFI_RECORD_BYTES||ziyi_hfi_record_get(data)!=G19_HFI_RECORD_MAGIC||ziyi_hfi_record_get(data+4)!=1||
    ziyi_hfi_record_get(data+8)!=G19_HFI_RECORD_BYTES||ziyi_hfi_record_get(data+12)!=ziyi_hfi_record_checksum(data))return G19_HFI_RECORD;
 uint32_t w[80];for(unsigned i=0;i<80;i++)w[i]=ziyi_hfi_record_get(data+4*i);
 for(unsigned i=74;i<80;i++)if(w[i])return G19_HFI_RECORD;
 struct g19_hfi_record r={0};
 r.stage=w[4];r.error=w[5];r.flags=w[6];r.live_state=w[7];r.sequence=w[8];r.sent[0]=w[9];r.sent[1]=w[10];
 r.poll_cap=w[11];r.polls=w[12];r.ready_polls=w[13];r.ack_polls=w[14];r.remote_status=w[15];r.version=w[16];
 r.raw_declared=w[17];r.raw_count=w[18];r.owner_failed=w[19];r.first_mask=w[20];r.last_mask=w[21];
 for(unsigned i=0;i<4;i++){r.queue_first[i]=w[22+i];r.queue_last[i]=w[26+i];}
 for(unsigned i=0;i<19;i++)r.raw[i]=w[30+i];
 ziyi_hfi_record_health_get(&r.first_health,w+49);ziyi_hfi_record_health_get(&r.last_health,w+54);
 r.gmu_address=w[59];r.stores=w[60];r.q0_write_stores=w[61];r.q1_read_stores=w[62];
 r.epoch=ziyi_hfi_record_wide_get(w+63);r.start_tick=ziyi_hfi_record_wide_get(w+65);r.last_tick=ziyi_hfi_record_wide_get(w+67);r.timeout_ticks=ziyi_hfi_record_wide_get(w+69);
 r.delay_us=w[71];r.start_calls=w[72];r.doorbells=w[73];
 if(!ziyi_hfi_record_valid(&r))return G19_HFI_RECORD;*out=r;return 0;
}
