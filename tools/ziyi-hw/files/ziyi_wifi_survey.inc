/* Cohesive READ-only lifecycle survey. No hardware write/ACK/allocation API. */
static uint16_t zc_u16(const uint8_t*p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t zc_u32(const uint8_t*p){return zc_u16(p)|(uint32_t)zc_u16(p+2)<<16;}
static int zc_same(const void*aa,const void*bb,unsigned n){const uint8_t*a=aa,*b=bb;for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void zc_copy(void*aa,const void*bb,unsigned n){uint8_t*a=aa;const uint8_t*b=bb;for(unsigned i=0;i<n;i++)a[i]=b[i];}
static int zc_rd(struct zwifi_record*r,struct zwifi_cursor*c,zwifi_reader read,void*ctx,unsigned kind,uint64_t phys,void*to,unsigned bytes){
 c->kind=kind;r->extra_reads++;return read(ctx,phys,to,bytes);
}
static void zc_walk(struct zwifi_record*r,struct zwifi_cursor*c,zwifi_reader read,void*ctx,unsigned pass,unsigned part){
 c->pass=pass;c->part=part;c->count=0;c->kind=ZWIFI_NONE;struct zwifi_walk*w=&r->walk[pass][part];
 uint32_t id=part?r->smem.global_entry:r->smem.wpss_entry;
 if(!id||id>r->smem.count){w->error=ZW_FORMAT;return;}uint32_t base=zc_u32(r->smem.entry[0][id-1]);
 if(!zc_rd(r,c,read,ctx,ZWIFI_PARTITION_HEADER,ZW_SMEM_BASE+base,w->before,32)){w->error=ZW_IO;return;}
 uint32_t lane=0,at=32;struct zwifi_locator found[3]={0};
 for(;;){
  w->error=(uint32_t)zwifi_replay(r,c,&lane,&at,found);w->end_lane=lane;w->end_at=at;if(w->error||lane==2)break;
  if(c->count==ZWIFI_MAX_DESCRIPTORS){w->error=ZW_BOUNDS;break;}
  struct zwifi_step*s=&c->history[c->count];s->offset=at;
  if(!zc_rd(r,c,read,ctx,ZWIFI_ALLOC_DESCRIPTOR,ZW_SMEM_BASE+base+at,s->raw,16)){w->error=ZW_IO;break;}
  if(lane==0)w->uncached_count++;else w->cached_count++;c->count++;
 }
 w->count=c->count;zwifi_sha256(c->history,c->count*sizeof(c->history[0]),w->sha256);zc_copy(w->target,found,sizeof(found));
 if(w->error)return;
 if(!zc_rd(r,c,read,ctx,ZWIFI_PARTITION_HEADER,ZW_SMEM_BASE+base,w->after,32)){w->error=ZW_IO;return;}
 if(!zc_same(w->before,w->after,32)){w->error=ZW_CHANGED;return;}
 for(unsigned t=0;t<3;t++)if(found[t].found&&!r->item[pass][t].found){
  c->target=t;
  if(!zc_rd(r,c,read,ctx,ZWIFI_TARGET_DESCRIPTOR,ZW_SMEM_BASE+found[t].descriptor,c->recheck,16)){w->error=ZW_IO;return;}
  if(!zc_same(c->recheck,found[t].raw,16)){w->error=ZW_CHANGED;return;}
  struct zwifi_item*item=&r->item[pass][t];uint32_t n=t==2?(found[t].logical>256?256:found[t].logical):340;
  if((t<2&&found[t].logical!=340)||!n){w->error=ZW_FORMAT;return;}
  item->part=part;item->offset=found[t].data;item->logical=found[t].logical;item->captured=n;item->physical_bytes=(n+3)&~3u;
  if(!zc_rd(r,c,read,ctx,ZWIFI_TARGET_PAYLOAD,ZW_SMEM_BASE+item->offset,item->data,item->physical_bytes)){w->error=ZW_IO;return;}
  /* bytes beyond the logical capped payload are padding, never interpreted. */
  for(unsigned j=n;j<item->physical_bytes;j++)item->data[j]=0;
  if(t<2){uint8_t*p=item->data;unsigned total=zc_u16(p+12),valid=zc_u16(p+14);if(zc_u32(p)!=0x504d5324||(zc_u32(p+4)&255)!=1||zc_u16(p+8)!=(t?13:0)||zc_u16(p+10)!=(t?0:13)||!total||total>16||valid>total){w->error=ZW_FORMAT;return;}}
  item->found=1;
 }
 if(!zc_rd(r,c,read,ctx,ZWIFI_PARTITION_HEADER,ZW_SMEM_BASE+base,w->after,32)){w->error=ZW_IO;return;}
 if(!zc_same(w->before,w->after,32))w->error=ZW_CHANGED;
}
static void zc_qmp(struct zwifi_record*r,struct zwifi_cursor*c,zwifi_reader read,void*ctx,unsigned pass){
 c->pass=pass;struct zwifi_qmp*q=&r->qmp[pass];
 if(!zc_rd(r,c,read,ctx,ZWIFI_QMP_DESCRIPTOR,ZWIFI_QMP,q->descriptor,60)){q->error=ZW_IO;return;}
 uint32_t off=q->descriptor[14],size=q->descriptor[13];
 if(q->descriptor[0]!=0x4d41494c||q->descriptor[1]!=1||off<0x3c||(off&3)||size<68||off>1024||size>1024-off){q->error=ZW_FORMAT;return;}
 if(!zc_rd(r,c,read,ctx,ZWIFI_QMP_LENGTH,ZWIFI_QMP+off,&q->length,4)){q->error=ZW_IO;return;}
 if(q->length>64||(q->length&3)){q->error=ZW_BOUNDS;return;}
 if(q->length&&!zc_rd(r,c,read,ctx,ZWIFI_QMP_PAYLOAD,ZWIFI_QMP+off+4,q->payload,q->length))q->error=ZW_IO;
}
static void zc_db(struct zwifi_record*r,struct zwifi_cursor*c,zwifi_reader read,void*ctx,unsigned pass){
 c->pass=pass;struct zwifi_db*d=&r->db[pass];static const char names[2][8]={"cx.lvl","mx.lvl"};
 if(!zc_rd(r,c,read,ctx,ZWIFI_DB_HEADER,ZWIFI_CMDDB,d->header,144)){d->error=ZW_IO;return;}d->reads++;
 if(d->header[1]!=0x0c0330db){d->error=ZW_FORMAT;return;}
 for(unsigned slave=0;slave<8;slave++){
  uint32_t a=d->header[2+4*slave],b=d->header[3+4*slave];unsigned id=a&0xffff,start=a>>16,data=b&0xffff,count=b>>16;
  if(!id)continue;if(id>8||count>512||(start&3)||144u+start>ZWIFI_CMDDB_BYTES||count*24u>ZWIFI_CMDDB_BYTES-144u-start){d->error=ZW_BOUNDS;return;}
  c->db_slave=slave;
  for(unsigned n=0;n<count;n++){
   c->db_index=n;
   if(!zc_rd(r,c,read,ctx,ZWIFI_DB_ENTRY,ZWIFI_CMDDB+144u+start+24u*n,c->db_entry,24)){d->error=ZW_IO;return;}d->reads++;d->scanned++;
   for(unsigned t=0;t<2;t++)if(zc_same(c->db_entry,names[t],8)){
    if(d->matches&(1u<<t)){d->error=ZW_FORMAT;return;}unsigned len=c->db_entry[5]&0xffff,aux=c->db_entry[5]>>16;uint64_t at=144u+data+aux;
    if(len>64||(len&1)||at>ZWIFI_CMDDB_BYTES||len>ZWIFI_CMDDB_BYTES-at){d->error=ZW_BOUNDS;return;}
    zc_copy(d->entry[t],c->db_entry,24);
    if(len){uint8_t temp[68]={0};uint64_t aligned=at&~3ULL;unsigned cover=(unsigned)(((at&3)+len+3)&~3ULL);if(!zc_rd(r,c,read,ctx,ZWIFI_DB_AUX,ZWIFI_CMDDB+aligned,temp,cover)){d->error=ZW_IO;return;}zc_copy(d->aux[t],temp+(at&3),len);d->reads++;}d->matches|=1u<<t;
   }
  }
 }
 if(d->matches!=3)d->error=ZW_FORMAT;
}
static int zwifi_survey(struct zwifi_record*r,struct zwifi_cursor*c,zwifi_reader read,void*ctx){
 if(!r||!c||!read)return 0;
 for(unsigned pass=0;pass<2;pass++){
  for(unsigned part=0;part<2;part++)zc_walk(r,c,read,ctx,pass,part);
  zc_qmp(r,c,read,ctx,pass);zc_db(r,c,read,ctx,pass);c->pass=pass;
  if(!zc_rd(r,c,read,ctx,ZWIFI_IPCC_ID,ZWIFI_IPCC_PENDING,&r->ipcc_pending[pass],4))r->survey_error|=1u<<8;
 }
 for(unsigned part=0;part<2;part++){
  const struct zwifi_walk*a=&r->walk[0][part],*b=&r->walk[1][part];
  if(a->error||b->error)r->survey_error|=1u<<part;
  else if(!zc_same(a,b,sizeof(*a)))r->survey_error|=1u<<4;
 }
 for(unsigned t=0;t<3;t++)if(!zc_same(&r->item[0][t],&r->item[1][t],sizeof(struct zwifi_item)))r->survey_error|=1u<<5;
 for(unsigned pass=0;pass<2;pass++){if(r->qmp[pass].error)r->survey_error|=1u<<2;if(r->db[pass].error)r->survey_error|=1u<<3;}
 if(!r->db[0].error&&!r->db[1].error&&!zc_same(&r->db[0],&r->db[1],sizeof(struct zwifi_db)))r->survey_error|=1u<<6;
 r->survey_complete=1;return !r->survey_error;
}
