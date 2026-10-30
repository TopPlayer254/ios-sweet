/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g8_fwinit.h"
#include "ziyi_g7_sha256.h"
#define SYSRESET 0x03d7e000ULL
#define HFI_CTRL 0x03d7e600ULL
#define FW_RESULT 0x03d7e070ULL
#define FSR 0x03db2058ULL
#define TLB_STATUS 0x03db27f4ULL
#define TLBI 0x03db2610ULL
#define TLBSYNC 0x03db27f0ULL
static const uint64_t irq_status[4]={0x03d7e648,0x03d8ec14,0x03d8ec4c,FSR};
static const uint64_t irq_clear[4]={0x03d7e644,0x03d8ec10,0x03d8ec50,FSR};
static const uint32_t irq_mask[4]={0x8006,0x3b,7,0xc00001fe};
static uint32_t word(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void bytes(uint8_t*p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static int same(const void*a,const void*b,size_t n){const uint8_t*x=a,*y=b;unsigned different=0;for(size_t i=0;i<n;i++)different|=x[i]^y[i];return !different;}
/* ARM-SMMU v2 FSR.Format is read-only, not a W1C fault. CBA2R.VA64 must
 * already have an exact, verified snapshot/readback before accepting 0x400. */
static int fsr_faults(const struct g8_workspace*w,uint32_t raw,uint32_t*out){
 const uint32_t faults=0xc00001feu,format=0x600u;
 if(!w||!out||raw&~(faults|format)||w->record.expected[25]>1)return 0;
 if((raw&format)!=(w->record.expected[25]?0x400u:0u))return 0;
 *out=raw&faults;return 1;
}
/* G9 native measured HALT0/ACK3 with CM3 held in reset. Vendor boot writes
 * HALT0 unconditionally, even when its control already reads zero. Accept
 * this exact sticky ACK tuple only; partial/unknown ACK states still stop. */
static int inherited_bus(uint32_t halt,uint32_t ack){return (!halt&&!ack)||((halt==0||halt==2||halt==3)&&ack==3);}
static int fail(struct g8_record*r,unsigned code,unsigned poison){if(!r->error)r->error=code;r->poison|=poison;return 0;}
static uint32_t old_crc(const struct g8_record*r){uint32_t h=2166136261u;for(unsigned i=0;i<G8_REGS;i++)for(unsigned j=0;j<8;j++){h^=(uint8_t)(r->old[i]>>(8*j));h*=16777619u;}return h;}
static int index_of(uint64_t at,unsigned width){for(unsigned i=0;i<G8_REGS;i++)if(g8_regs[i].address==at&&g8_regs[i].width==width)return (int)i;return -1;}
static int tcm_offset(uint64_t at){if(!(at&3)&&at>=0x03d6d000&&at<0x03d75000)return (int)(at-0x03d6d000);return -1;}
static uint64_t tcm_address(unsigned off){return 0x03d6d000ULL+off;}
static uint64_t target(const struct g8_workspace*w,unsigned i){if(i==5)return w->firmware_plan.hfi_address;if(i==11)return w->firmware_plan.log_address|3u;if(i==30)return w->memory.table_plan.table_physical[0]|(2ULL<<48);return g8_regs[i].literal;}
static int payload_word(const struct g8_workspace*w,unsigned off,uint32_t*value){
 uint32_t va=off<0x4000?off:0x10004000u+off-0x4000;
 for(unsigned i=0;i<w->firmware_plan.block_count;i++){const struct gn_block*b=&w->firmware_plan.blocks[i];if(b->size&&va>=b->address&&va-b->address<=b->size-4&&(b->kind==GN_ITCM||b->kind==GN_DTCM)){*value=word(w->firmware+b->data_offset+va-b->address);return 1;}}
 return 0;
}
int g8_observation_address(uint64_t at,unsigned width,uint64_t*out){
 if(!out)return -1;if(width==4&&(at==TLBI||at==TLBSYNC)){*out=0;return 0;}
 if(width==4)for(unsigned i=0;i<4;i++)if(at==irq_clear[i]){*out=irq_status[i];return 1;}
 if(index_of(at,width)>=0||(width==4&&tcm_offset(at)>=0)){*out=at;return 1;}return -1;
}
int g8_read_allowed(const struct g8_workspace*w,uint64_t at,unsigned width){
 if(!w)return 0;const struct g8_record*r=&w->record;if(!(r->flags&G8_LEASE)||r->lease_released||r->poison||!r->epoch||(width!=4&&width!=8)||(at&(width-1)))return 0;
 if(width==4&&(at==0x03d9953c||at==0x03d9905c||at==SYSRESET||at==HFI_CTRL||at==FW_RESULT))return 1;
 if(width==4&&(at==0x03da0000||at==0x03da0020||at==0x03da0024||at==0x03da0028||(at>=0x03da0800&&at<0x03da0818)||(at>=0x03da0c00&&at<0x03da0c18)))return 1;
 if(!(r->flags&G8_SCOPED))return 0;
 if(index_of(at,width)>=0)return 1;
 if(width==4){for(unsigned i=0;i<4;i++)if(at==irq_status[i])return 1;if(at==TLB_STATUS||at==GD_ACK)return 1;int off=tcm_offset(at);if(off>=0&&(r->flags&G8_HELD_RESET)&&(r->phase==G8_SAVE||r->phase==G8_LOAD||r->phase==G8_TCM_RESTORE))return 1;}
 return 0;
}
int g8_store_guard(const struct g8_workspace*w,uint64_t at,unsigned width,uint64_t value,uint64_t now,uint32_t reset,uint32_t pending){
 if(!w)return 0;const struct g8_record*r=&w->record;
 if(r->poison||r->lease_released||!r->epoch||(r->flags&(G8_LEASE|G8_SAVED|G8_SCOPED))!=(G8_LEASE|G8_SAVED|G8_SCOPED)||r->saved_crc!=old_crc(r)||r->pending_address!=at||r->pending_value!=value||r->pending_width!=width)return 0;
 if(at==SYSRESET){if(width!=4||now!=r->expected[19]||now>1)return 0;return (r->phase==G8_START&&value==0&&reset==1&&(r->flags&(G8_PUBLISHED|G8_MAPPED|G8_LOADED))==(G8_PUBLISHED|G8_MAPPED|G8_LOADED)&&r->bus_released&&r->expected[23]==0&&r->final_ack==0)||(r->phase==G8_STOP&&value==1&&reset==0&&(r->flags&G8_RELEASED_RESET));}
 if(reset!=1)return 0;
 int off=tcm_offset(at);if(width==4&&off>=0){if((unsigned)off!=r->tcm_cursor)return 0;if(r->phase==G8_LOAD){uint32_t want;return (r->flags&G8_MAPPED)&&payload_word(w,(unsigned)off,&want)&&value==want;}return r->phase==G8_TCM_RESTORE&&(r->flags&G8_BUS_DRAINED)&&word(w->backup[0]+off)==word(w->backup[1]+off)&&value==word(w->backup[0]+off);}
 if(width==4&&(at==TLBI||at==TLBSYNC))return (r->phase==G8_INSTALL||r->phase==G8_CONTEXT_RESTORE)&&value==(at==TLBI?2u:0u)&&(r->flags&G8_PUBLISHED);
 if(width==4&&r->phase==G8_CLEAR)for(unsigned i=0;i<4;i++)if(at==irq_clear[i]){uint32_t actual=(uint32_t)now;if(i==3&&!fsr_faults(w,actual,&actual))return 0;return (r->flags&G8_BUS_DRAINED)&&value&&value==actual&&!(value&~irq_mask[i])&&!(i==3&&(value&0x40000000u));}
 int idx=index_of(at,width);if(idx<0||now!=r->expected[idx])return 0;unsigned i=(unsigned)idx;const struct g8_reg*d=&g8_regs[i];
 if(d->kind==G8_GBIF){if(r->phase==G8_DRAIN)return gd_store_guard(&r->drain,at,(uint32_t)value,(uint32_t)now);if(r->phase==G9_RELEASE_BUS)return value==0&&now==r->initial_halt&&r->old[i]==now&&pending==r->initial_ack&&r->bus_observations==2&&inherited_bus(r->initial_halt,r->initial_ack)&&(r->flags&(G8_PUBLISHED|G8_MAPPED|G8_LOADED))==(G8_PUBLISHED|G8_MAPPED|G8_LOADED)&&!(r->flags&G8_RELEASED_RESET)&&!r->bus_released;return r->phase==G8_CONFIG_RESTORE&&(r->flags&G8_BUS_DRAINED)&&!(r->flags&G8_MAPPED)&&value==r->old[i];}
 if(d->kind==G8_FWSTATUS)return value==0&&((r->phase==G8_CONFIG&&now==0)||(r->phase==G8_CONFIG_RESTORE&&(now==0||now==0x100)));
 if(r->phase==G8_CONFIG_RESTORE){if(i>=24||value!=r->old[i]||(i==20||i==21?pending!=0:0))return 0;return d->kind!=G8_MASK||!((value^now)&~d->mask);}
 if(r->phase==G8_CONTEXT_RESTORE){if(i<24||i==36)return 0;if(i==26&&value==0)return 1;return value==r->old[i]&&(!(r->flags&G8_MAPPED));}
 if(r->phase==G8_UNROUTE)return (i==34&&value==0)||(i==35&&value==0x20000&&r->expected[34]==0);
 if(r->phase==G8_INSTALL){if(i<24||i==36)return 0;if(i==26&&value==0)return 1;if(i==34)return value==0x84000005&&r->expected[26]==0xc01027&&r->expected[35]==0x302;return value==target(w,i);}
 if(r->phase!=G8_CONFIG||i>=24)return 0;
 if(d->kind==G8_MASK)return value==(r->old[i]|d->mask)&&!((value^now)&~d->mask);
 if(d->kind==G8_INTERFRAME)return value==0x9c40400||(value==0x9c40403&&now==0x9c40400)||(value==0x9c40407&&now==0x9c40403);
 return value==target(w,i);
}
static uint64_t read_value(struct g8_workspace*w,const struct g8_io*io,uint64_t at,unsigned width){
 struct g8_record*r=&w->record;if(r->poison)return 0;r->last_address=at;
 if(!g8_read_allowed(w,at,width)){fail(r,G8_OWNER,1);return 0;}int ok=0;uint64_t v=io->read(io->ctx,at,width,&ok);r->reads++;r->last_value=v;if(!ok)fail(r,G8_IO,1);return v;
}
static int put(struct g8_workspace*w,const struct g8_io*io,uint64_t at,unsigned width,uint64_t value){
 struct g8_record*r=&w->record;if(r->poison)return 0;uint64_t observe;int mode=g8_observation_address(at,width,&observe);if(mode<0)return fail(r,G8_OWNER,1);
 uint64_t now=mode?read_value(w,io,observe,width):0;uint32_t reset=(uint32_t)read_value(w,io,SYSRESET,4);if(r->poison)return 0;
 uint32_t pending=0;if(r->phase==G8_CONFIG_RESTORE&&(at==0x03d7e64c||at==0x03d8ec18))pending=(uint32_t)read_value(w,io,at==0x03d7e64c?irq_status[0]:irq_status[1],4);
 if(r->phase==G9_RELEASE_BUS&&at==GD_HALT)pending=(uint32_t)read_value(w,io,GD_ACK,4);
 r->pending_address=at;r->pending_width=width;r->pending_value=value;
 if(!g8_store_guard(w,at,width,value,now,reset,pending))return fail(r,G8_OWNER,1);
 int ok=0;io->write(io->ctx,at,width,value,&ok);r->writes++;if(tcm_offset(at)>=0)r->tcmmodified=1;if(!ok)return fail(r,G8_IO,1);io->barrier(io->ctx);
 if(mode){uint64_t after=read_value(w,io,observe,width);if(r->poison)return 0;uint64_t want=r->phase==G8_CLEAR?0:value;if(r->phase==G8_CLEAR&&at==FSR){uint32_t bits;if(!fsr_faults(w,(uint32_t)after,&bits))return fail(r,G8_OWNER,1);after=bits;}if(after!=want)return fail(r,G8_OWNER,1);}
 int i=index_of(at,width);if(i>=0)r->expected[i]=r->phase==G8_CLEAR?(at==FSR&&r->expected[25]?0x400u:0u):value;return 1;
}
static int set_reg(struct g8_workspace*w,const struct g8_io*io,unsigned i,uint64_t value){if(w->record.expected[i]==value)return 1;return put(w,io,g8_regs[i].address,g8_regs[i].width,value);}
static int bus_wait(struct g8_workspace*w,const struct g8_io*io,uint32_t wanted){
 for(unsigned n=0;n<100000;n++){
  if(read_value(w,io,SYSRESET,4)!=1||read_value(w,io,GD_HALT,4)!=w->record.expected[23]||w->record.poison)return fail(&w->record,G8_OWNER,1);
  uint32_t ack=(uint32_t)read_value(w,io,GD_ACK,4);w->record.final_ack=ack;
  if(w->record.poison||ack&~3u)return fail(&w->record,G8_OWNER,1);
  if(ack==wanted)return 1;io->delay(io->ctx,1);
 }
 return fail(&w->record,G8_TIMEOUT,1);
}
static int bus_release(struct g8_workspace*w,const struct g8_io*io){
 if(!inherited_bus(w->record.initial_halt,w->record.initial_ack)||w->record.old[23]!=w->record.initial_halt)return fail(&w->record,G8_OWNER,1);
 w->record.phase=G9_RELEASE_BUS;
 if(!put(w,io,GD_HALT,4,0)||!bus_wait(w,io,0))return 0;
 w->record.bus_released=1;return 1;
}
static int quiet(struct g8_workspace*w,const struct g8_io*io){uint64_t cx=read_value(w,io,0x03d9953c,4),gx=read_value(w,io,0x03d9905c,4),reset=read_value(w,io,SYSRESET,4),hfi=read_value(w,io,HFI_CTRL,4);return !w->record.poison&&((cx&0x80000000u)&&!(gx&0x80000000u)&&reset==1&&!hfi)?1:fail(&w->record,G8_PREREQ,1);}
int g8_kernel_plan(const struct g8_prereq*q,const struct ga_snapshot*s,struct ga_plan*out){
 if(!q||!s||!out)return 0;*out=(struct ga_plan){0};
 if(q->board!=0x06060201||q->current_el!=4||(q->sctlr&0x1005)!=0x1005||(q->sctlr&0x3000000)||q->hlos_cb!=6||q->hlos_smr!=6||!q->epoch||s->epoch!=q->epoch||s->dt_cb!=6||s->dt_smr!=6||s->dt_handoff_pairs||s->facts)return 0;
 for(unsigned i=0;i<4;i++)if(s->gr0[0][i]!=s->gr0[1][i])return 0;
 uint32_t id0=s->gr0[0][1],id1=s->gr0[0][2],id2=s->gr0[0][3];
 if(!(id0&(1u<<30))||!(id0&(1u<<27))||(id0&(1u<<8))||(id0&255)<6||(id1&255)<6||((id1>>16)&255)||!(id2&(1u<<12))||(id1&(1u<<31))||((id1>>28)&7)!=3)return 0;
 for(unsigned i=0;i<6;i++)if(s->smr[0][i]!=s->smr[1][i]||(s->smr[0][i]&(1u<<31))||s->s2cr[0][i]!=0x20000||s->s2cr[1][i]!=0x20000)return 0;
 out->cb=2;out->smr=0;out->software_context_map=7;out->epoch=s->epoch;return 1;
}
static int snapshot(struct g8_workspace*w,const struct g8_prereq*q,const struct g8_io*io){
 struct ga_snapshot*s=&w->snapshot;s->facts=0;s->dt_cb=s->dt_smr=6;s->epoch=w->record.epoch;const uint64_t ids[]={0x3da0000,0x3da0020,0x3da0024,0x3da0028};
 for(unsigned pass=0;pass<2;pass++){for(unsigned i=0;i<4;i++)s->gr0[pass][i]=(uint32_t)read_value(w,io,ids[i],4);for(unsigned i=0;i<6;i++){s->smr[pass][i]=(uint32_t)read_value(w,io,0x3da0800+4*i,4);s->s2cr[pass][i]=(uint32_t)read_value(w,io,0x3da0c00+4*i,4);if(s->s2cr[pass][i]!=0x20000)return fail(&w->record,G8_OWNER,1);}}
 if(w->record.poison||!g8_kernel_plan(q,s,&w->lease_plan)||w->lease_plan.cb!=2||w->lease_plan.smr!=0||w->lease_plan.software_context_map!=7||s->smr[0][0])return fail(&w->record,G8_OWNER,1);return 1;
}
static int old_valid(struct g8_workspace*w,unsigned i){uint64_t old=w->record.old[i];const struct g8_reg*d=&g8_regs[i];if(d->kind==G8_RESET)return old==1;if(d->kind==G8_GBIF)return old==w->record.initial_halt&&inherited_bus(w->record.initial_halt,w->record.initial_ack);if(i==36){uint32_t faults;return fsr_faults(w,(uint32_t)old,&faults)&&!faults;}if(d->kind==G8_FWSTATUS)return old==0;if(i==34)return old==0;if(i==35)return old==0x20000;if(d->kind==G8_ARCH)return !(old&~d->mask)&&!(i==26&&(old&1));if(d->kind==G8_MASK)return old<=0xffffffffu;if(d->kind==G8_INTERFRAME)return old==0||old==0x9c40400||old==0x9c40403||old==0x9c40407;return old==0||old==target(w,i);}
static int hashes_valid(struct g8_workspace*w){uint8_t h[32];for(unsigned i=0;i<2;i++){g7_sha256(w->backup[i],G8_TCM_BYTES,h);if(!same(h,w->record.tcm_sha[i],32))return 0;}return same(w->backup[0],w->backup[1],G8_TCM_BYTES);}
static int tlbi(struct g8_workspace*w,const struct g8_io*io){io->barrier(io->ctx);if(!put(w,io,TLBI,4,2)||!put(w,io,TLBSYNC,4,0))return 0;for(unsigned i=0;i<10000;i++){uint64_t v=read_value(w,io,TLB_STATUS,4);if(w->record.poison)return 0;if(v&~1u)return fail(&w->record,G8_OWNER,1);if(!v)return 1;io->delay(io->ctx,1);}return fail(&w->record,G8_TIMEOUT,1);}
struct g8_drain_ctx {struct g8_workspace*w;const struct g8_io*io;};
static uint32_t drain_read(void*arg,uint64_t at,int*ok){struct g8_drain_ctx*c=arg;*ok=0;if(at!=GD_HALT&&at!=GD_RESET&&at!=GD_ACK)return 0;uint32_t v=(uint32_t)read_value(c->w,c->io,at,4);*ok=!c->w->record.poison;return v;}
static void drain_write(void*arg,uint64_t at,uint32_t value,int*ok){struct g8_drain_ctx*c=arg;*ok=put(c->w,c->io,at,4,value);}
static void drain_delay(void*arg,uint32_t us){struct g8_drain_ctx*c=arg;c->io->delay(c->io->ctx,us);}
static int cleanup(struct g8_workspace*w,const struct g8_io*io){
 struct g8_record*r=&w->record;if(r->poison)return 0;
 if(r->flags&G8_RELEASED_RESET){r->phase=G8_STOP;if(!set_reg(w,io,19,1))return 0;r->flags|=G8_HELD_RESET;if(!quiet(w,io))return 0;r->phase=G8_DRAIN;struct g8_drain_ctx ctx={w,io};struct gd_io d={&ctx,drain_read,drain_write,drain_delay};if(!gd_drain(&r->drain,r->epoch,100000,&d))return fail(r,G8_RESTORE,1);r->flags|=G8_BUS_DRAINED;}
 else r->flags|=G8_BUS_DRAINED;
 r->phase=G8_CLEAR;
 for(unsigned i=0;i<4;i++){uint32_t pending=(uint32_t)read_value(w,io,irq_status[i],4);if(i==0)r->last_host=pending;if(i==1)r->last_ao=pending;if(i==2)r->last_fence=pending;if(i==3)r->last_fsr=pending;if(r->poison)return 0;if((i==3&&!fsr_faults(w,pending,&pending))||pending&~irq_mask[i]||(i==3&&(pending&0x40000000u)))return fail(r,G8_OWNER,1);if(pending){if(!r->error)r->error=G8_DEVICE_FAULT;if(!put(w,io,irq_clear[i],4,pending))return 0;}}
 if(r->flags&G8_MAPPED){r->phase=G8_UNROUTE;if(!set_reg(w,io,34,0)||!set_reg(w,io,35,0x20000))return 0;r->flags&=~G8_MAPPED;}
 r->phase=G8_CONTEXT_RESTORE;if(!set_reg(w,io,26,0)||!tlbi(w,io))return 0;
 const unsigned order[]={25,24,28,29,30,31,32,33,27,26};for(unsigned n=0;n<sizeof(order)/sizeof(order[0]);n++)if(!set_reg(w,io,order[n],r->old[order[n]]))return 0;if(!tlbi(w,io))return 0;
 if(r->tcmmodified){if(!hashes_valid(w))return fail(r,G8_BACKUP,1);r->phase=G8_TCM_RESTORE;for(unsigned off=0;off<G8_TCM_BYTES;off+=4){r->tcm_cursor=off;if(!put(w,io,tcm_address(off),4,word(w->backup[0]+off)))return 0;}for(unsigned off=0;off<G8_TCM_BYTES;off+=4)if(read_value(w,io,tcm_address(off),4)!=word(w->backup[1]+off)||r->poison)return fail(r,G8_RESTORE,1);}
 r->phase=G8_CONFIG_RESTORE;
 for(unsigned i=0;i<24;i++){if(i==19||i==23)continue;if(!set_reg(w,io,i,r->old[i]))return 0;}
 if(!set_reg(w,io,23,r->old[23]))return 0;
 for(unsigned i=0;i<G8_REGS;i++)if(read_value(w,io,g8_regs[i].address,g8_regs[i].width)!=r->old[i]||r->poison)return fail(r,G8_RESTORE,1);
 /* ACK is read-only status: initial HALT0/ACK3 is deliberately normalized
  * to idle0 by the vendor release. Restore controls/TCM exactly, not stale ACK3. */
 if(!hashes_valid(w)||!bus_wait(w,io,r->initial_halt?3u:0u)||!quiet(w,io))return fail(r,G8_RESTORE,1);r->restored=1;r->flags|=G8_RESTORED;return 1;
}
int g8_run(struct g8_workspace*w,const struct g8_prereq*q,const struct g8_io*io){
 if(!w||!q||!io||!io->read||!io->write||!io->ticks||!io->delay||!io->barrier||!io->lease||!io->proof||!io->owned||(w->record.flags&G8_LEASE&&!w->record.lease_released)||w->memory.state)return 0;
 struct g8_record*r=&w->record;*r=(struct g8_record){.epoch=q->epoch,.phase=G8_ACQUIRE};
 if(!q->epoch||q->board!=0x06060201||q->hlos_cb!=6||q->hlos_smr!=6||q->current_el!=4||(q->sctlr&0x1005)!=0x1005||(q->sctlr&0x3000000)||q->acd_off!=1||q->qdss_off!=1||q->lm_bcl_off!=1||q->hfi_off!=1||q->inherited_ddr_probe!=1||!io->proof(io->ctx,q,&w->blob)||!io->owned(io->ctx,w,sizeof(*w)))return fail(r,G8_PREREQ,0);
 const uint32_t lo[]={18600000,1000000,180000000,135000000},hi[]={19800000,300000000,220000000,165000000};for(unsigned i=0;i<4;i++)if(q->rates[i]<lo[i]||q->rates[i]>hi[i])return fail(r,G8_PREREQ,0);
 uint8_t hash[32];g7_sha256(w->firmware,G8_FW_BYTES,hash);if(w->blob.bytes!=G8_FW_BYTES||w->blob.board!=q->board||!same(hash,w->blob.firmware_sha256,32)||gn_firmware_plan(w->firmware,G8_FW_BYTES,&w->firmware_plan)!=GN_OK||w->firmware_plan.block_count!=15||w->firmware_plan.data_blocks!=4||w->firmware_plan.metadata_blocks!=11)return fail(r,G8_FIRMWARE,0);
 r->start=io->ticks(io->ctx);if(r->start<q->epoch||r->start-q->epoch>19200000||!io->lease(io->ctx,q->epoch,1))return fail(r,G8_PREREQ,0);r->flags=G8_LEASE|G8_HELD_RESET;
 if(!quiet(w,io)||!snapshot(w,q,io))goto done;r->flags|=G8_SCOPED;r->phase=G8_SAVE;
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<G8_REGS;i++){uint64_t value=read_value(w,io,g8_regs[i].address,g8_regs[i].width);if(r->poison)goto done;if(pass&&r->old[i]!=value){fail(r,G8_OWNER,1);goto done;}r->old[i]=r->expected[i]=value;}
 for(unsigned i=0;i<4;i++){uint32_t pending=(uint32_t)read_value(w,io,irq_status[i],4);if((i==3&&!fsr_faults(w,pending,&pending))||pending||r->poison){fail(r,G8_OWNER,1);goto done;}}
 for(unsigned pass=0;pass<2;pass++){
  uint32_t halt=(uint32_t)read_value(w,io,GD_HALT,4),ack=(uint32_t)read_value(w,io,GD_ACK,4);
  if(!pass){r->initial_halt=halt;r->initial_ack=ack;}
  if(halt!=r->old[23]||halt!=r->initial_halt||ack!=r->initial_ack||!inherited_bus(halt,ack)||read_value(w,io,SYSRESET,4)!=1||r->poison){fail(r,G8_OWNER,1);goto done;}
  r->bus_observations++;io->barrier(io->ctx);
 }
 if(read_value(w,io,TLB_STATUS,4)||r->poison){fail(r,G8_OWNER,1);goto done;}
 for(unsigned pass=0;pass<2;pass++){for(unsigned off=0;off<G8_TCM_BYTES;off+=4){if(!(off&0xfff)&&!quiet(w,io))goto done;bytes(w->backup[pass]+off,(uint32_t)read_value(w,io,tcm_address(off),4));if(r->poison)goto done;}g7_sha256(w->backup[pass],G8_TCM_BYTES,r->tcm_sha[pass]);}
 if(!hashes_valid(w)){fail(r,G8_BACKUP,1);goto done;}r->saved_crc=old_crc(r);r->flags|=G8_SAVED;r->phase=G8_MEMORY;
 if(!gm_memory_prepare(&w->memory,&w->firmware_plan,&io->memory_io)){fail(r,G8_MEMORY_ERROR,0);goto done;}r->flags|=G8_MEMORY_READY;
 for(unsigned i=0;i<G8_REGS;i++)if(!old_valid(w,i)){fail(r,G8_PREREQ,0);goto done;}
 for(unsigned i=0;i<w->firmware_plan.block_count;i++){const struct gn_block*b=&w->firmware_plan.blocks[i];if(b->size&&b->kind!=GN_ITCM&&b->kind!=GN_DTCM){uint8_t*dst=gm_memory_owned_va(&w->memory,b->address,b->size);if(!dst){fail(r,G8_MEMORY_ERROR,0);goto done;}for(unsigned j=0;j<b->size;j++)dst[j]=w->firmware[b->data_offset+j];if(!same(dst,w->firmware+b->data_offset,b->size)){fail(r,G8_BACKUP,1);goto done;}}}
 if(gn_hfi_init(gm_memory_owned_va(&w->memory,w->firmware_plan.hfi_address,GN_HFI_BYTES),GN_HFI_BYTES,w->firmware_plan.hfi_address)!=GN_OK||!gm_memory_publish(&w->memory,&io->memory_io)){fail(r,G8_MEMORY_ERROR,0);goto done;}r->flags|=G8_PUBLISHED;r->phase=G8_INSTALL;
 if(!set_reg(w,io,26,0)||!set_reg(w,io,27,target(w,27))||!tlbi(w,io))goto done;
 const unsigned setup[]={25,24,28,29,30,31,32,33,26,35,34};for(unsigned n=0;n<sizeof(setup)/sizeof(setup[0]);n++)if(!set_reg(w,io,setup[n],target(w,setup[n])))goto done;r->flags|=G8_MAPPED;
 if(!hashes_valid(w)){fail(r,G8_BACKUP,1);goto done;}r->phase=G8_LOAD;for(unsigned off=0;off<G8_TCM_BYTES;off+=4){uint32_t value;if(payload_word(w,off,&value)){r->tcm_cursor=off;if(!put(w,io,tcm_address(off),4,value))goto done;}}
 const uint32_t version[]={0x30020004,0xffffffff,0x30010002,0,0x20010000};for(unsigned i=0;i<5;i++)if(read_value(w,io,0x3d74fe0+4*i,4)!=version[i]||r->poison){fail(r,G8_FIRMWARE,0);goto restore;}r->flags|=G8_LOADED;r->phase=G8_CONFIG;
 for(unsigned i=0;i<23;i++){if(i==19)continue;uint64_t value=g8_regs[i].kind==G8_MASK?r->old[i]|g8_regs[i].mask:target(w,i);if(!set_reg(w,io,i,value))goto done;if(i==15&&(!set_reg(w,io,15,0x9c40403)||!set_reg(w,io,15,0x9c40407)))goto done;}
 g7_sha256(w->firmware,G8_FW_BYTES,hash);if(!same(hash,w->blob.firmware_sha256,32)){fail(r,G8_FIRMWARE,0);goto restore;}
 if(read_value(w,io,FW_RESULT,4)||r->poison){fail(r,G8_FIRMWARE,0);goto restore;}
 if(!bus_release(w,io))goto done;
 r->phase=G8_START;if(!set_reg(w,io,19,0))goto done;r->flags&=~G8_HELD_RESET;r->flags|=G8_RELEASED_RESET;r->phase=G8_WAIT;
 for(unsigned i=0;i<100000;i++){uint32_t value=(uint32_t)read_value(w,io,FW_RESULT,4);if(r->poison)goto done;r->expected[3]=value;if((value&0x1ff)==0x100){r->flags|=G8_FW100;break;}if(i==99999)fail(r,G8_TIMEOUT,0);io->delay(io->ctx,1);}
restore:if(!r->poison)cleanup(w,io);
done:r->end=io->ticks(io->ctx);if(!r->writes||r->restored){if(io->lease(io->ctx,q->epoch,0)){r->lease_released=1;r->flags|=G8_LEASE_RELEASED;}else fail(r,G8_OWNER,1);}r->retain_allocations=!r->restored&&r->writes;return g8_transaction_complete(w);
}
int g8_transaction_complete(const struct g8_workspace*w){return w&&!w->record.error&&!w->record.poison&&w->record.restored&&w->record.lease_released&&(w->record.flags&G8_FW100);}
