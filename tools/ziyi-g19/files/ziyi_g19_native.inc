/* G19 one-command HFI adapter. No CoreAnimation/Metal/client publication. */
#define G19_INIT 0x03d7e618ULL
#define G19_BELL 0x03d7e650ULL
#define G19_STATUS 0x03d7e600ULL
#define G19_FW 0x03d7e070ULL
extern unsigned int ziyi_hop_pmap_last_error(void);

static int zg_hfi_lease(struct zg_state*s,uint64_t epoch){
 return s->gmu_enabled==2 && epoch &&
  __atomic_load_n(&s->lease,__ATOMIC_ACQUIRE)==1 &&
  s->lease_epoch==epoch && s->firmware.record.epoch==epoch;
}
static int zg_hfi_evidence(void*ctx,uint64_t epoch,uint32_t*facts){
 struct zg_state*s=ctx;struct g8_record*r=&s->firmware.record;*facts=0;
 if(!zg_hfi_lease(s,epoch)||r->poison||!zg_adt_check(s,0))return 0;
 *facts=HOP_LEASE_HELD;
 int ok;uint32_t reset=(uint32_t)zg_g8_load(s,GD_RESET,4,&ok);if(!ok)return 0;
 if(reset!=1)return 1;
 *facts|=HOP_RESET_HELD;
 /* Preparation is before this GMU's own route/publication, while the
  * qualified inherited halt/ACK tuple and reset keep its bus quiescent. */
 if((r->flags&(G8_SAVED|G8_SCOPED))!=(G8_SAVED|G8_SCOPED)||
    r->flags&(G8_MAPPED|G8_PUBLISHED|G8_RELEASED_RESET))return 1;
 uint32_t halt=(uint32_t)zg_g8_load(s,GD_HALT,4,&ok);if(!ok)return 0;
 uint32_t ack=(uint32_t)zg_g8_load(s,GD_ACK,4,&ok);if(!ok)return 0;
 if(r->bus_observations!=2||halt!=r->initial_halt||ack!=r->initial_ack||
    !((halt==8&&ack==3)||(!halt&&!ack)))return 1;
 *facts|=HOP_DRAINED|HOP_GMU_UNMAPPED;return 1;
}
static void zg_hfi_memory_failure(struct zg_state*s){
 struct g19_hfi_record*d=&s->hfi_record;
 d->stage=G19_HFI_OWNER;d->error=G19_HFI_OWNERSHIP;
 d->last_health.helper_phase=s->hfi_memory.state;
 d->last_health.helper_error=(s->hfi_memory.error<<8)|ziyi_hop_pmap_last_error();
 printf("ZIYI HFI memory STOP state%u err%u pmap%u va%llx pa%llx alias%llx retained1\n",
  s->hfi_memory.state,s->hfi_memory.error,ziyi_hop_pmap_last_error(),
  s->hfi_memory.va,s->hfi_memory.page[0].pa,s->hfi_memory.page[0].aperture);
}
static int zg_hfi_prepare(void*ctx,struct g8_workspace*w,const struct g8_io*io){
 (void)io;struct zg_state*s=ctx;struct g19_hfi_record*d=&s->hfi_record;
 bzero(d,sizeof(*d));d->stage=G19_HFI_OWNER;d->flags=G19_HFI_RETAIN|G19_HFI_PLANNED;
 d->error=G19_HFI_OWNERSHIP;
 d->epoch=w->record.epoch;d->gmu_address=w->firmware_plan.hfi_address;
 d->sequence=1;d->poll_cap=50000;d->delay_us=100;d->timeout_ticks=5*zg_frequency();
 (void)gh_version_command(1,d->sent);
 s->hfi_owner.owner_ctx=s;s->hfi_owner.owner_evidence=zg_hfi_evidence;
 if(w!=&s->firmware||!hop_xnu_prepare(&s->hfi_memory,&s->hfi_owner,w->record.epoch,1)||
    !gm_memory_bind_hfi(&w->memory,(void*)(uintptr_t)s->hfi_memory.va,
                        GN_HFI_BYTES,w->firmware_plan.hfi_address)){
  zg_hfi_memory_failure(s);return 0;
 }
 if(!ziyi_gpu_cp_prepare(s))return 0;
 printf("ZIYI HFI page NormalNC16K pa%llx aperture%llx, queueIOVA%x bytes%u\n",
  s->hfi_memory.page[0].pa,s->hfi_memory.page[0].aperture,
  w->firmware_plan.hfi_address,GN_HFI_BYTES);
 return 1;
}
static int zg_hfi_publish(void*ctx,struct g8_workspace*w,const struct g8_io*io){
 (void)io;struct zg_state*s=ctx;
 if(w!=&s->firmware||w->memory.external_hfi!=(void*)(uintptr_t)s->hfi_memory.va||
    !hop_xnu_mark_live(&s->hfi_memory,&s->hfi_owner)){
  zg_hfi_memory_failure(s);return 0;
 }
 /* All four GMU pages must map the qualified allocation, never its former
  * unused static placeholder. Publication follows this exact table check. */
 for(unsigned off=0;off<GN_HFI_BYTES;off+=GT_PAGE){
  uint64_t pa=0;
  if(gt_lookup(&w->memory.table_plan,w->memory.tables,
     w->firmware_plan.hfi_address+off,&pa)!=1||
     pa!=s->hfi_memory.page[0].pa+off){zg_hfi_memory_failure(s);return 0;}
 }
 return 1;
}
static int zg_hfi_qualified(void*ctx,uint64_t epoch){
 struct zg_state*s=ctx;struct g8_record*r=&s->firmware.record;
 return zg_hfi_lease(s,epoch)&&!r->poison&&r->phase==G8_WAIT&&
  (r->flags&(G8_FW100|G8_PUBLISHED|G8_MAPPED|G8_LOADED|G8_RELEASED_RESET))==
   (G8_FW100|G8_PUBLISHED|G8_MAPPED|G8_LOADED|G8_RELEASED_RESET)&&
  s->hfi_memory.state==HOP_LIVE&&s->firmware.memory.state==2&&
  s->firmware.memory.external_hfi==(void*)(uintptr_t)s->hfi_memory.va&&
  hop_xnu_validate(&s->hfi_memory,&s->hfi_owner);
}
static int zg_hfi_read32(void*ctx,uint32_t offset,uint32_t*out){
 struct zg_state*s=ctx;if(!out||offset>GN_HFI_BYTES-4||(offset&3)||
  !zg_hfi_qualified(s,s->lease_epoch))return 0;
 *out=*(volatile uint32_t*)(uintptr_t)(s->hfi_memory.va+offset);return 1;
}
static int zg_hfi_write32(void*ctx,uint32_t offset,uint32_t value){
 struct zg_state*s=ctx;
 if(offset>GN_HFI_BYTES-4||(offset&3)||
    (offset!=68&&offset!=112&&!(offset>=GN_HFI_QUEUE0&&offset<GN_HFI_QUEUE0+4096))||
    !zg_hfi_qualified(s,s->lease_epoch))return 0;
 *(volatile uint32_t*)(uintptr_t)(s->hfi_memory.va+offset)=value;return 1;
}
static int zg_hfi_order(void*ctx,enum gh_live_order order){
 struct zg_state*s=ctx;if(!zg_hfi_qualified(s,s->lease_epoch))return 0;
 if(order==GH_LIVE_RELEASE)__asm__ volatile("dmb oshst":::"memory");
 else if(order==GH_LIVE_ACQUIRE)__asm__ volatile("dmb oshld":::"memory");
 else return 0;
 return 1;
}
static int zg_hfi_health(void*ctx,struct g19_hfi_health*h){
 struct zg_state*s=ctx;h->helper_phase=s->hfi_memory.state;
 h->helper_error=(s->hfi_memory.error<<8)|ziyi_hop_pmap_last_error();
 if(!zg_hfi_qualified(s,s->lease_epoch))return 0;
 int good;h->fw_result=(uint32_t)zg_g8_load(s,G19_FW,4,&good);if(!good)return 0;
 h->hfi_status=(uint32_t)zg_g8_load(s,G19_STATUS,4,&good);if(!good)return 0;
 h->reset=(uint32_t)zg_g8_load(s,GD_RESET,4,&good);if(!good)return 0;
 if(h->reset||h->hfi_status&~1u||h->fw_result!=0x100u)return 0;
 uint32_t fsr=(uint32_t)zg_g8_load(s,0x03db2058,4,&good);if(!good||fsr!=0x400)return 0;
 uint32_t halt=(uint32_t)zg_g8_load(s,GD_HALT,4,&good);if(!good||halt)return 0;
 uint32_t ack=(uint32_t)zg_g8_load(s,GD_ACK,4,&good);if(!good||(ack!=0&&ack!=3))return 0;
 return 1;
}
static int zg_hfi_store(void*ctx,uint64_t address){
 struct zg_state*s=ctx;struct g19_hfi_health health={0};
 if((address!=G19_INIT&&address!=G19_BELL)||!zg_adt_check(s,0)||
    !zg_hfi_health(s,&health))return 0;
 if(address==G19_INIT){
  if(s->hfi_record.stage!=G19_HFI_INITIALIZE||s->hfi_record.start_calls!=1)return 0;
  int good;uint32_t before=(uint32_t)zg_g8_load(s,G19_INIT,4,&good);
  if(!good||before>1)return 0;
 }else{
  if(s->hfi_record.stage!=G19_HFI_PUBLISH||s->hfi_record.doorbells!=1||
     s->hfi_record.q0_write_stores!=1||s->hfi_record.stores!=5)return 0;
  volatile uint32_t*q=(volatile uint32_t*)(uintptr_t)s->hfi_memory.va;
  if(q[68/4]!=4||q[GN_HFI_QUEUE0/4]!=0x00100201||
     q[GN_HFI_QUEUE0/4+1]!=GH_A662_SUPPORTED_VERSION)return 0;
 }
 /* Exact finite source-qualified stores. Doorbell is write-only: no
  * fictitious readback or general bit mask is used as authorization. */
 void*p=zg_g8_pointer(s,address,4,1);if(!p)return 0;
 zg_barrier(s);*(volatile uint32_t*)p=1;zg_barrier(s);return 1;
}
static int zg_hfi_start(void*ctx){return zg_hfi_store(ctx,G19_INIT);}
static int zg_hfi_bell(void*ctx){return zg_hfi_store(ctx,G19_BELL);}
static int zg_hfi_now(void*ctx,uint64_t*out){*out=zg_ticks(ctx);return 1;}
static int zg_hfi_delay(void*ctx,uint32_t us){if(us>100)return 0;zg_delay(ctx,us);return 1;}
static void zg_hfi_ready(void*ctx,struct g8_workspace*w,const struct g8_io*io){
 (void)io;struct zg_state*s=ctx;if(w!=&s->firmware)return;
 const struct g19_hfi_config config={w->record.epoch,5*zg_frequency(),
  w->firmware_plan.hfi_address,1,50000,100};
 const struct g19_hfi_ops ops={s,{s,zg_hfi_qualified,zg_hfi_read32,zg_hfi_write32,
  zg_hfi_order,zg_hfi_bell},zg_hfi_health,zg_hfi_start,zg_hfi_now,zg_hfi_delay};
 int error=g19_hfi_run(&s->hfi_runtime,&config,&ops,&s->hfi_record);
 printf("ZIYI HFI result stage%u err%d sent%08x/%08x raw%u status%x version%x polls%u retained1\n",
  s->hfi_record.stage,error,s->hfi_record.sent[0],s->hfi_record.sent[1],
  s->hfi_record.raw_count,s->hfi_record.remote_status,s->hfi_record.version,s->hfi_record.polls);
 if(!error&&ziyi_gpu_startup_continue(s))(void)ziyi_gpu_cp_continue(s);
}
static void zg_hfi_print(struct zg_state*s){
 if(s->gmu_enabled!=2)return;
 uint8_t bytes[G19_HFI_RECORD_BYTES];
 if(g19_hfi_record_encode(&s->hfi_record,bytes)){
  printf("ZIYI HFI record invalid stage%u error%u\n",s->hfi_record.stage,s->hfi_record.error);return;
 }
 const char*alphabet="ABCDEFGHJKLMNPQR";char lines[20][33];
 for(unsigned i=0;i<sizeof(bytes);i++){
  lines[i/16][2*(i%16)]=alphabet[bytes[i]>>4];
  lines[i/16][2*(i%16)+1]=alphabet[bytes[i]&15];
 }
 for(unsigned i=0;i<20;i++)lines[i][32]=0;
 for(unsigned repeat=0;repeat<3;repeat++)for(unsigned page=0;page<5;page++){
  for(unsigned part=0;part<4;part++)printf("ZGHF%u%u %s\n",page+1,part+1,lines[page*4+part]);
  zg_sleep(1500);
 }
}
