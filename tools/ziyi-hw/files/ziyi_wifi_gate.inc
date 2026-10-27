/* No load/store primitive: exact next-read authorization, independently replayed. */
static const struct zwifi_fixed_read zwifi_survey_fixed[3]={{ZWIFI_QMP,60,0},{ZWIFI_CMDDB,144,0},{ZWIFI_IPCC_PENDING,4,0}};
static uint16_t zg_u16(const uint8_t*p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t zg_u32(const uint8_t*p){return zg_u16(p)|(uint32_t)zg_u16(p+2)<<16;}
static int zg_equal(const void*aa,const void*bb,unsigned n){const uint8_t*a=aa,*b=bb;for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static int zg_target(uint16_t id){return id==616?0:id==617?1:id==626?2:-1;}
static int zg_partition(const struct zwifi_record*r,unsigned part,uint32_t*base,uint32_t*size,const uint8_t**header){
 if(part>1||zwifi_table_validate(&r->smem)||r->smem.error)return 0;uint32_t id=part?r->smem.global_entry:r->smem.wpss_entry;
 if(!id||id>r->smem.count)return 0;const uint8_t*e=r->smem.entry[0][id-1],*p=r->smem.partition[0][id-1];
 if(zwifi_entry_kind(e)!=2||!zg_equal(p,r->smem.partition[1][id-1],32)||!zg_equal(p,"$PRT",4)||!zg_equal(p+4,e+12,4)||zg_u32(p+8)!=zg_u32(e+4)||zg_u32(p+12)<32||zg_u32(p+12)>zg_u32(p+16)||zg_u32(p+16)>zg_u32(e+4))return 0;
 if(part?(zg_u16(e+12)!=0xfffe||zg_u16(e+14)!=0xfffe):!((zg_u16(e+12)==0&&zg_u16(e+14)==13)||(zg_u16(e+12)==13&&zg_u16(e+14)==0)))return 0;
 *base=zg_u32(e);*size=zg_u32(e+4);*header=p;return 1;
}
static int zwifi_replay(const struct zwifi_record*r,struct zwifi_cursor*c,uint32_t*lane_out,uint32_t*at_out,struct zwifi_locator out[3]){
 if(!r||!c||c->pass>1||c->part>1||c->count>ZWIFI_MAX_DESCRIPTORS||!lane_out||!at_out||!out)return ZW_ARGUMENT;
 for(unsigned k=0;k<3;k++)out[k]=(struct zwifi_locator){0};uint32_t base,size;const uint8_t*header;
 if(!zg_partition(r,c->part,&base,&size,&header)||!zg_equal(header,r->walk[c->pass][c->part].before,32))return ZW_FORMAT;
 uint32_t id=c->part?r->smem.global_entry:r->smem.wpss_entry,line=zg_u32(r->smem.entry[0][id-1]+16),uncached=zg_u32(header+12),cached=zg_u32(header+16);
 if((uncached&3)||(cached&3)||!line||line>4096||(line&(line-1)))return ZW_FORMAT;
 uint32_t stride=(16+line-1)&~(line-1),lane=0,at=32;uint8_t*seen=c->scratch_seen;for(unsigned z=0;z<8192;z++)seen[z]=0;
 for(unsigned i=0;i<=c->count;i++){
  if(lane==0&&at==uncached){lane=1;if(stride>size)return ZW_BOUNDS;at=size-stride;}
  if(lane==1&&((uint64_t)at+stride==cached)){lane=2;}
  if(i==c->count){*lane_out=lane;*at_out=at;return ZW_OK;}
  if(lane==2||c->history[i].offset!=at||at>size||16>size-at||(at&3))return ZW_BOUNDS;
  const uint8_t*d=c->history[i].raw;uint32_t n=zg_u32(d+4),pad=zg_u16(d+8),ph=zg_u16(d+10);uint16_t item=zg_u16(d+2);
  if(zg_u16(d)!=0xa5a5||!n||(n&7)||pad>=n||(ph&3))return ZW_FORMAT;
  if(seen[item>>3]&(1u<<(item&7)))return ZW_FORMAT;seen[item>>3]|=(uint8_t)(1u<<(item&7));
  uint32_t data,next;
  if(lane==0){if(at>uncached||16>uncached-at||ph>uncached-at-16||n>uncached-at-16-ph)return ZW_BOUNDS;data=at+16+ph;next=data+n;}
  else{if(at<=cached||n>at||at-n<cached||n+stride>at)return ZW_BOUNDS;data=at-n;next=at-n-stride;}
  int t=zg_target(item);if(t>=0){out[t].found=1;out[t].descriptor=base+at;out[t].data=base+data;out[t].allocated=n;out[t].logical=n-pad;for(unsigned k=0;k<16;k++)out[t].raw[k]=d[k];}
  at=next;
 }
 return ZW_BOUNDS;
}
static int zg_valid_qmp(const struct zwifi_qmp*q){
 uint32_t off=q->descriptor[14],size=q->descriptor[13];return q->descriptor[0]==0x4d41494c&&q->descriptor[1]==1&&off>=0x3c&&!(off&3)&&size>=68&&off<=1024&&size<=1024-off;
}
static int zg_db_headers(const struct zwifi_db*d){
 if(d->header[1]!=0x0c0330db)return 0;
 for(unsigned j=0;j<8;j++){uint32_t a=d->header[2+4*j],b=d->header[3+4*j];unsigned id=a&0xffff,start=a>>16,count=b>>16;
  if(id&&(id>8||count>512||(start&3)||144u+start>ZWIFI_CMDDB_BYTES||count*24u>ZWIFI_CMDDB_BYTES-144u-start))return 0;
 }
 return 1;
}
static int zwifi_survey_allowed(const struct zwifi_record*r,struct zwifi_cursor*c,uint64_t physical,uint32_t bytes){
 if(!r||!c||r->stage!=5||r->status||!zwifi_context_valid(r)||c->pass>1||!bytes||(bytes&3)||(physical&3))return 0;
 if(c->kind==ZWIFI_IPCC_ID)return physical==zwifi_survey_fixed[2].physical&&bytes==zwifi_survey_fixed[2].bytes;
 if(c->kind==ZWIFI_QMP_DESCRIPTOR)return physical==zwifi_survey_fixed[0].physical&&bytes==zwifi_survey_fixed[0].bytes;
 if(c->kind==ZWIFI_QMP_LENGTH||c->kind==ZWIFI_QMP_PAYLOAD){
  const struct zwifi_qmp*q=&r->qmp[c->pass];if(!zg_valid_qmp(q))return 0;uint64_t box=ZWIFI_QMP+q->descriptor[14];
  return c->kind==ZWIFI_QMP_LENGTH?(physical==box&&bytes==4):(q->length>0&&q->length<=64&&!(q->length&3)&&physical==box+4&&bytes==q->length);
 }
 if(c->kind==ZWIFI_DB_HEADER)return physical==zwifi_survey_fixed[1].physical&&bytes==zwifi_survey_fixed[1].bytes;
 if(c->kind==ZWIFI_DB_ENTRY||c->kind==ZWIFI_DB_AUX){
  const struct zwifi_db*d=&r->db[c->pass];if(!zg_db_headers(d)||c->db_slave>=8)return 0;
  uint32_t a=d->header[2+4*c->db_slave],b=d->header[3+4*c->db_slave];unsigned id=a&0xffff,start=a>>16,count=b>>16,data=b&0xffff;
  if(!id||c->db_index>=count)return 0;
  if(c->kind==ZWIFI_DB_ENTRY)return physical==ZWIFI_CMDDB+144u+start+c->db_index*24u&&bytes==24;
  static const char names[2][8]={"cx.lvl","mx.lvl"};int match=-1;for(unsigned t=0;t<2;t++)if(zg_equal(c->db_entry,names[t],8))match=(int)t;
  unsigned len=c->db_entry[5]&0xffff,off=c->db_entry[5]>>16;uint64_t at=144u+data+off;
  uint64_t aligned=at&~3ULL;unsigned cover=(unsigned)(((at&3)+len+3)&~3ULL);return match>=0&&len&&len<=64&&!(len&1)&&aligned<=ZWIFI_CMDDB_BYTES&&cover<=ZWIFI_CMDDB_BYTES-aligned&&physical==ZWIFI_CMDDB+aligned&&bytes==cover;
 }
 uint32_t base,size;const uint8_t*header;if(!zg_partition(r,c->part,&base,&size,&header))return 0;
 if(c->kind==ZWIFI_PARTITION_HEADER)return physical==ZW_SMEM_BASE+base&&bytes==32;
 struct zwifi_locator found[3];uint32_t lane,at;if(zwifi_replay(r,c,&lane,&at,found))return 0;
 if(c->kind==ZWIFI_ALLOC_DESCRIPTOR){
  uint32_t limit=lane==0?zg_u32(header+12):size;return lane<2&&c->count<ZWIFI_MAX_DESCRIPTORS&&at<=limit&&16<=limit-at&&physical==ZW_SMEM_BASE+base+at&&bytes==16;
 }
 if(lane!=2||c->target>=3||!found[c->target].found)return 0;const struct zwifi_locator*t=&found[c->target];
 if(c->kind==ZWIFI_TARGET_DESCRIPTOR)return physical==ZW_SMEM_BASE+t->descriptor&&bytes==16;
 if(c->kind!=ZWIFI_TARGET_PAYLOAD||!zg_equal(c->recheck,t->raw,16))return 0;
 uint32_t want=c->target==2?(t->logical>256?256:t->logical):340;
 if(c->target<2&&t->logical!=340)return 0;uint32_t rounded=(want+3)&~3u;
 return want&&rounded<=t->allocated&&physical==ZW_SMEM_BASE+t->data&&bytes==rounded;
}
