#include "ziyi_wifi_core.h"
#include "ziyi_wifi_sha.inc"
#include "ziyi_wifi_read_gate.inc"
#include "ziyi_wifi_smem.inc"
#include "ziyi_wifi_gate.inc"
#include "ziyi_wifi_survey.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"KWIFI survey line%u\n",__LINE__);return 1;}}while(0)
static void p16(uint8_t*p,uint16_t v){p[0]=v;p[1]=v>>8;}
static void p32(uint8_t*p,uint32_t v){p16(p,v);p16(p+2,v>>16);}
static uint32_t u32(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
struct model {uint8_t*smem,*db;uint8_t qmp[1024];struct zwifi_record*r;struct zwifi_cursor*c,*valid_cursor;unsigned calls,fail,change,denied,payloads,private_reads;};
static int model_read(void*ctx,uint64_t physical,void*to,uint32_t bytes){
 struct model*m=ctx;if(!zwifi_survey_allowed(m->r,m->c,physical,bytes)){m->r->denied++;m->denied++;return 0;}
 m->calls++;if(m->calls==m->fail)return 0;
 if(m->calls==m->change)m->smem[0x2000+32+4]^=8;
 if(m->c->kind==ZWIFI_TARGET_PAYLOAD){m->payloads++;if(m->payloads==1)*m->valid_cursor=*m->c;}
 if(physical>=ZW_SMEM_BASE&&physical<ZW_SMEM_BASE+ZW_SMEM_BYTES){
  uint32_t off=(uint32_t)(physical-ZW_SMEM_BASE);if(bytes>ZW_SMEM_BYTES-off)return 0;
  if(m->c->kind!=ZWIFI_PARTITION_HEADER&&m->c->kind!=ZWIFI_ALLOC_DESCRIPTOR&&m->c->kind!=ZWIFI_TARGET_DESCRIPTOR&&m->c->kind!=ZWIFI_TARGET_PAYLOAD){m->private_reads++;return 0;}
  memcpy(to,m->smem+off,bytes);return 1;
 }
 if(physical>=ZWIFI_QMP&&physical+bytes<=ZWIFI_QMP+1024){memcpy(to,m->qmp+(physical-ZWIFI_QMP),bytes);return 1;}
 if(physical>=ZWIFI_CMDDB&&physical+bytes<=ZWIFI_CMDDB+ZWIFI_CMDDB_BYTES){memcpy(to,m->db+(physical-ZWIFI_CMDDB),bytes);return 1;}
 if(physical==ZWIFI_IPCC_PENDING&&bytes==4){p32(to,0xffffffff);return 1;}return 0;
}
static int prefix_read(void*ctx,uint32_t off,void*to,uint32_t bytes){struct model*m=ctx;if(!zwifi_address_allowed(m->r,ZW_SMEM_BASE+off,bytes))return 0;if(off>ZW_SMEM_BYTES||bytes>ZW_SMEM_BYTES-off)return 0;memcpy(to,m->smem+off,bytes);return 1;}
static void desc(uint8_t*p,unsigned id,unsigned size,unsigned padding){memset(p,0,16);p16(p,0xa5a5);p16(p+2,id);p32(p+4,size);p16(p+8,padding);}
static void smp(uint8_t*p,unsigned local,unsigned remote){memset(p,0,340);p32(p,0x504d5324);p32(p+4,0x101);p16(p+8,local);p16(p+10,remote);p16(p+12,16);p16(p+14,2);memcpy(p+20,local==0?"master-kernel":"slave-kernel",local==0?13:12);p32(p+36,local==0?0:6);memcpy(p+40,"wlan",4);}
static void synth_base(struct zwifi_record*r){
 memset(r,0,sizeof(*r));r->version=1;r->bytes=sizeof(*r);r->stage=5;r->adt_facts=ZWIFI_ADT_FACTS;r->el=4;r->sctlr=0x1005;r->frequency=19200000;memcpy(&r->contract,zwifi_contract_bytes,160);r->smem_version[0]=r->smem_version[1]=12u<<16;
 struct zw_smem_record*x=&r->smem;x->count=5;x->wpss_entry=2;x->global_entry=3;
 uint8_t*t=x->toc[0];memcpy(t,"$TOC",4);p32(t+4,1);p32(t+8,5);
 uint8_t*e=x->entry[0][0];p32(e,ZW_SMEM_BYTES-4096);p32(e+4,4096);p32(e+8,1);p16(e+12,0xffff);p16(e+14,0xffff);
 for(unsigned part=0;part<2;part++){
  e=x->entry[0][part+1];unsigned off=part?0x6000:0x2000,size=part?0x8000:0x4000;uint8_t*h=x->partition[0][part+1];p32(e,off);p32(e+4,size);p16(e+12,part?0xfffe:0);p16(e+14,part?0xfffe:13);p32(e+16,32);memcpy(h,"$PRT",4);memcpy(h+4,e+12,4);p32(h+8,size);p32(h+12,part?0x3000:0x2000);p32(h+16,size);
 }
 e=x->entry[0][3];p32(e,0x10000);p32(e+4,0x2000);p16(e+12,1);p16(e+14,2);p32(e+16,32);
 e=x->entry[0][4];p32(e+4,4096);p32(e+8,3);p16(e+12,0xffff);p16(e+14,0xffff);
 memcpy(x->toc[1],x->toc[0],32);memcpy(x->entry[1],x->entry[0],32*48);memcpy(x->partition[1],x->partition[0],32*32);
}
static void setup(struct model*m,struct zwifi_record*r,struct zwifi_cursor*c,const uint8_t*fixture){
 (void)fixture;synth_base(r);m->r=r;m->c=c;m->calls=m->fail=m->change=m->denied=m->payloads=m->private_reads=0;memset(c,0,sizeof(*c));memset(m->smem,0xe7,ZW_SMEM_BYTES);memset(m->db,0,ZWIFI_CMDDB_BYTES);memset(m->qmp,0,1024);
 for(unsigned part=0;part<2;part++){
  unsigned id=part?r->smem.global_entry:r->smem.wpss_entry;uint32_t base=u32(r->smem.entry[0][id-1]),end=u32(r->smem.partition[0][id-1]+12),at=32;memcpy(m->smem+base,r->smem.partition[0][id-1],32);
  if(!part){desc(m->smem+base+at,616,344,4);smp(m->smem+base+at+16,0,13);at+=360;desc(m->smem+base+at,617,344,4);smp(m->smem+base+at+16,13,0);at+=360;desc(m->smem+base+at,626,256,0);memset(m->smem+base+at+16,0,256);memcpy(m->smem+base+at+16,"SYNTHETIC HOST CRASH REASON",27);at+=272;}
  else{desc(m->smem+base+at,616,344,4);smp(m->smem+base+at+16,0,13);at+=360;}
  desc(m->smem+base+at,900+part,end-at-16,0);
 }
 p32(m->smem+0x5c,12u<<16);memcpy(m->smem+ZW_SMEM_BYTES-4096,r->smem.toc[0],32);memcpy(m->smem+ZW_SMEM_BYTES-4096+32,r->smem.entry[0],32*48);
 p32(m->qmp,0x4d41494c);p32(m->qmp+4,1);for(unsigned off=12;off<=48;off+=4)p32(m->qmp+off,0xffff);p32(m->qmp+0x1c,128);p32(m->qmp+0x20,0x200);p32(m->qmp+0x34,128);p32(m->qmp+0x38,0x100);
 p32(m->db+4,0x0c0330db);p32(m->db+8,3);p32(m->db+12,(2u<<16)|128);
 for(unsigned t=0;t<2;t++){uint8_t*e=m->db+144+24*t;memcpy(e,t?"mx.lvl":"cx.lvl",7);p32(e+16,0x30000+16*t);p32(e+20,(4*t<<16)|4);p32(m->db+144+128+4*t,123+t);}
}
static void change_header(struct zwifi_record*r,struct model*m,unsigned part,unsigned cached){unsigned id=part?r->smem.global_entry:r->smem.wpss_entry;for(unsigned pass=0;pass<2;pass++)p32(r->smem.partition[pass][id-1]+16,cached);p32(m->smem+u32(r->smem.entry[0][id-1])+16,cached);}
int main(int argc,char**argv){
 CHECK(argc==2);FILE*f;uint8_t fixture[1]={0};
 struct zwifi_record*r=calloc(1,sizeof(*r));struct zwifi_cursor*c=calloc(1,sizeof(*c)),*saved=calloc(1,sizeof(*saved));struct model m={0};m.smem=malloc(ZW_SMEM_BYTES);m.db=malloc(ZWIFI_CMDDB_BYTES);m.valid_cursor=saved;CHECK(r&&c&&saved&&m.smem&&m.db);
 setup(&m,r,c,fixture);r->stage=3;CHECK(zw_smem_observe(prefix_read,&m,&r->smem));CHECK(r->smem.count==5&&r->smem.wpss_entry==2&&r->smem.global_entry==3&&r->smem.reads==18);r->stage=5;CHECK(zwifi_survey(r,c,model_read,&m));CHECK(!r->survey_error&&r->survey_complete&&!m.denied&&!m.private_reads&&m.payloads==6);CHECK(r->walk[0][0].count==4&&r->walk[0][1].count==2&&r->item[0][0].found&&r->item[0][1].found&&r->item[0][2].found);CHECK(r->db[0].matches==3&&r->ipcc_pending[0]==0xffffffff);unsigned calls=m.calls;
 f=fopen(argv[1],"wbx");CHECK(f);CHECK(fwrite(r,1,sizeof(*r),f)==sizeof(*r));CHECK(!fclose(f));
 for(unsigned i=1;i<=calls;i++){setup(&m,r,c,fixture);m.fail=i;CHECK(!zwifi_survey(r,c,model_read,&m));CHECK(r->survey_error&&!m.denied&&!m.private_reads);}
 for(unsigned mode=0;mode<12;mode++){
  setup(&m,r,c,fixture);uint8_t*d=m.smem+0x2000+32;
  switch(mode){case 0:d[0]^=1;break;case 1:p32(d+4,0);break;case 2:p32(d+4,0xfffffff8);break;case 3:p16(d+8,344);break;case 4:p16(d+10,65532);break;case 5:p16(d+10,1);break;case 6:p32(d+4,343);break;case 7:p16(d+360+2,616);break;case 8:p32(d+4,352);break;case 9:p16(d+8,0);break;case 10:m.smem[0x2000+12]^=8;break;case 11:m.change=7;break;}
  CHECK(!zwifi_survey(r,c,model_read,&m));CHECK(r->survey_error&&!m.private_reads);
 }
 for(unsigned mode=0;mode<6;mode++){setup(&m,r,c,fixture);uint8_t*p=m.smem+0x2000+48;if(mode==0)p[0]^=1;if(mode==1)p[4]=2;if(mode==2)p[8]=1;if(mode==3)p[10]=0;if(mode==4)p16(p+12,17);if(mode==5)p16(p+14,17);CHECK(!zwifi_survey(r,c,model_read,&m));CHECK(r->survey_error&&!m.private_reads);}
 /* Complete cached chain, then a selected cached crash item; unselected data is never read. */
 setup(&m,r,c,fixture);change_header(r,&m,0,0x4000-512);uint8_t*cache=m.smem+0x2000+0x4000-32;desc(cache,901,480,0);CHECK(zwifi_survey(r,c,model_read,&m));CHECK(r->walk[0][0].cached_count==1&&!m.denied);
 setup(&m,r,c,fixture);change_header(r,&m,0,0x4000-512);cache=m.smem+0x2000+0x4000-32;desc(cache,626,480,224);p16(m.smem+0x2000+32+720+2,902);memset(cache-480,0,256);memcpy(cache-480,"CACHED HOST FIXTURE",20);CHECK(zwifi_survey(r,c,model_read,&m));CHECK(r->item[0][2].offset==0x2000+0x4000-512&&!m.denied);
 setup(&m,r,c,fixture);change_header(r,&m,0,0x4000-512);cache=m.smem+0x2000+0x4000-32;desc(cache,901,488,0);CHECK(!zwifi_survey(r,c,model_read,&m));CHECK(r->survey_error&&!m.private_reads);
 /* Guard replays the actual descriptor prefix, not a guessed next-address range. */
 setup(&m,r,c,fixture);CHECK(zwifi_survey(r,c,model_read,&m));*c=*saved;c->kind=ZWIFI_TARGET_PAYLOAD;uint64_t payload=ZW_SMEM_BASE+0x2000+48;CHECK(zwifi_survey_allowed(r,c,payload,340));
 for(unsigned mode=0;mode<10;mode++){struct zwifi_cursor*v=malloc(sizeof(*v));CHECK(v);*v=*c;struct zwifi_record rr=*r;
  if(mode==0)v->count=ZWIFI_MAX_DESCRIPTORS+1;if(mode==1)v->history[0].offset++;if(mode==2)v->history[0].raw[0]^=1;if(mode==3)v->recheck[0]^=1;if(mode==4)v->pass=2;if(mode==5)v->part=2;if(mode==6)v->target=3;if(mode==7)rr.stage=4;if(mode==8)rr.adt_facts^=1;if(mode==9)rr.smem.partition[0][1][12]^=1;
  CHECK(!zwifi_survey_allowed(&rr,v,payload,340));free(v);
 }
 CHECK(!zwifi_survey_allowed(r,c,payload+4,340));CHECK(!zwifi_survey_allowed(r,c,payload,344));CHECK(!zwifi_survey_allowed(r,c,ZW_SMEM_BASE+0x2000+32+992+16,340));
 for(unsigned mode=0;mode<7;mode++){setup(&m,r,c,fixture);if(mode==0)p32(m.qmp,0);if(mode==1)p32(m.qmp+4,2);if(mode==2)p32(m.qmp+0x38,0xfffffff0);if(mode==3)p32(m.qmp+0x34,64);if(mode==4)p32(m.qmp+0x100,68);if(mode==5)p32(m.qmp+0x100,3);if(mode==6)p32(m.db+4,0);CHECK(!zwifi_survey(r,c,model_read,&m));CHECK(r->survey_error&&!m.private_reads);}
 setup(&m,r,c,fixture);p32(m.qmp+0x100,4);memcpy(m.qmp+0x104,"test",4);CHECK(zwifi_survey(r,c,model_read,&m));CHECK(r->qmp[0].length==4&&r->qmp[0].payload[0]==0x74736574);
 setup(&m,r,c,fixture);p32(m.db+144+20,2);CHECK(zwifi_survey(r,c,model_read,&m));CHECK(!m.denied);
 for(unsigned i=0;i<160;i++){struct zwifi_contract contract;memcpy(&contract,zwifi_contract_bytes,160);((uint8_t*)&contract)[i]^=1;CHECK(!zwifi_contract_valid(&contract));}
 setup(&m,r,c,fixture);CHECK(zwifi_contract_valid(&r->contract));CHECK(zwifi_context_valid(r));
 for(unsigned i=0;i<7;i++){struct zwifi_record copy=*r;if(i==0)copy.adt_facts=0;if(i==1)copy.el=0;if(i==2)copy.sctlr^=1;if(i==3)copy.sctlr^=4;if(i==4)copy.sctlr^=0x1000;if(i==5)copy.frequency++;if(i==6)copy.contract.flags=0;CHECK(!zwifi_context_valid(&copy));CHECK(!zwifi_survey_allowed(&copy,c,ZWIFI_IPCC_PENDING,4));}
 free(m.smem);free(m.db);free(saved);free(c);free(r);
 printf("KWIFI_ACTUAL_PUBLIC_SYNTHETIC_HOST_PASS checks=%u faultPositions=%u cachedAndUncached=1 targetOnlyPayloads=1 publicSyntheticOnly=1 noWriteAPI=1\n",checks,calls);
 printf("KWIFI_ABI bytes=%zu contract=%zu smem=%zu walk=%zu item=%zu qmp=%zu db=%zu versions=%zu crc=%zu\n",sizeof(struct zwifi_record),offsetof(struct zwifi_record,contract),offsetof(struct zwifi_record,smem),offsetof(struct zwifi_record,walk),offsetof(struct zwifi_record,item),offsetof(struct zwifi_record,qmp),offsetof(struct zwifi_record,db),offsetof(struct zwifi_record,smem_version),offsetof(struct zwifi_record,crc));return 0;
}
