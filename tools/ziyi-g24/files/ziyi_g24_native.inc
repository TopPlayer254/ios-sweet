/* Explicit kernel-only CP bring-up. Never returns into G8 reset/cleanup once
 * a new borrower can exist. No public user client or arbitrary command API. */
#include "ziyi_g24_record.h"
#include "ziyi_g7_sha256.h"
#include <pexpert/arm64/ziyi_log_strip.h>
static struct {
 struct g24_workspace w;struct hop pages;struct hop_xnu owner;
 struct zg_state*s;struct zg_mapping maps[3],zap;
 unsigned mode,prepared,checked_phase,metadata_verified,zap_backed,zap_copied,smc_inflight;
 uint32_t zap_crc,guard_error;
 uint8_t zap_backup[2][4096];
 uint32_t witness_sequence;uint8_t witness_bytes[64];
} g24n;
static int g24n_stop(unsigned error){g24n.w.r.failure_phase=g24n.w.r.phase;g24n.w.r.phase=G24_REFUSED;g24n.w.r.error=error;g24n.w.r.retained=1;return 0;}
static void*g24n_pointer(uint64_t at){
 if(at&3)return NULL;
 for(unsigned i=0;i<3;i++){struct zg_mapping*m=&g24n.maps[i];uint64_t off;if(m->page_va&&zg_region_offset(&m->region,at,4,&off))return(void*)(uintptr_t)(m->page_va+m->geometry.bias+off);}
 return NULL;
}
static int g24n_raw(uint64_t at,uint32_t*out){void*p=g24n_pointer(at);if(!p)return 0;*out=*(volatile uint32_t*)p;zg_barrier(NULL);return 1;}
static int g24n_lease(void){struct zg_state*s=g24n.s;return s==&zg&&g24n.mode==1&&g24n.prepared&&g24n.w.r.epoch&&s->lease_epoch==g24n.w.r.epoch&&__atomic_load_n(&s->lease,__ATOMIC_ACQUIRE)==1&&s->gmu_enabled==2&&zg_adt_check(s,0);}
static int g24n_tables(void){
 const struct gc_plan*p=&g24n.w.memory;const struct hop*h=&g24n.pages;
 if(!p->valid||p->epoch!=g24n.w.r.epoch||h->count!=8||p->translations.valid!=1||p->translations.table_count!=3||p->translations.mapped_pages!=24||p->ring_iova!=GC_IOVA||p->pixel_iova!=GC_IOVA+16384||p->fence_iova!=GC_IOVA+32768||p->sqe_iova!=GC_IOVA+49152||p->sqe_capacity!=49152)return 0;
 for(unsigned i=0;i<8;i++){
  if(!h->page[i].pa||(h->page[i].pa&16383)||h->page[i].pa>=(1ULL<<36)||h->page[i].pa==g24n.s->hfi_memory.page[0].pa)return 0;
  for(unsigned j=0;j<i;j++)if(h->page[j].pa==h->page[i].pa)return 0;
 }
 const volatile uint64_t*t=(volatile uint64_t*)(uintptr_t)h->va;
 for(unsigned level=0;level<3;level++){
  if(p->translations.table_physical[level]!=h->page[0].pa+level*4096)return 0;
  for(unsigned index=0;index<512;index++){
   uint64_t expected=0;
   if(level==0&&index==(GC_IOVA>>30))expected=(h->page[0].pa+4096)|3;
   if(level==1&&index==((GC_IOVA>>21)&511))expected=(h->page[0].pa+8192)|3;
   unsigned first=(GC_IOVA>>12)&511;
   if(level==2&&index>=first&&index<first+24){unsigned n=index-first;expected=(h->page[1+n/4].pa+(n%4)*4096)|GT_LEAF_ATTR|(n>=12?0xc0:0x40);}
   if(t[level*512+index]!=expected||p->tables[level][index]!=expected)return 0;
  }
 }
 for(unsigned i=3*4096;i<16384;i++)if(((volatile uint8_t*)t)[i]!=0xa5)return 0;
 return 1;
}
static int g24n_qualified(void*ctx,uint64_t epoch,unsigned phase){
 struct zg_state*s=ctx;uint32_t fw,hfi,reset,smr,s2cr,control;
 if(s!=g24n.s||!g24n_lease()||epoch!=g24n.w.r.epoch||phase!=g24n.w.r.phase||g23.record.state!=GS_COMPLETE||g23.record.error||g23.record.acks!=5||g23.record.epoch!=epoch)return 0;
 if(phase>=G24_ME_INIT&&phase<=G24_PRESENT&&!g29_cp_verified(&g24n.w))return 0;
 g24n.guard_error=1;
 if(g24n.checked_phase!=phase){if(!hop_xnu_validate(&g24n.pages,&g24n.owner)||!hop_xnu_validate(&s->hfi_memory,&s->hfi_owner)||!g24n_tables())return 0;g24n.checked_phase=phase;}
 g24n.guard_error=2;
 if(!g24n_raw(0x3d7e070,&fw)||!g24n_raw(0x3d7e600,&hfi)||!g24n_raw(0x3d7e000,&reset)||fw!=0x100||hfi!=1||reset)return 0;
 g24n.guard_error=3;
 if(!g24n_raw(0x3da0800,&smr)||!g24n_raw(0x3da0c00,&s2cr)||!g24n_raw(0x3db2000,&control)||smr!=0x84000005||s2cr!=0x302||control!=0xc00027)return 0;
 g24n.guard_error=4;
 if(g24n.w.r.route_live){
  const uint64_t at[]={0x3da0804,0x3da0c04,0x3da1000,0x3da1800,0x3db0000,0x3db0010,0x3db0030,0x3db0020,0x3db0024};
  const uint32_t want[]={0x84000000,0x300,0x1f300,1,0xc00127,0x38071,0x803519,(uint32_t)g24n.pages.page[0].pa,(uint32_t)(g24n.pages.page[0].pa>>32)};
  for(unsigned i=0;i<9;i++){uint32_t got;if(!g24n_raw(at[i],&got)||got!=want[i])return 0;}
 }
 g24n.guard_error=5;
 for(unsigned i=2;i<6;i++)if(!g24n_raw(0x3da0800+4*i,&smr)||!g24n_raw(0x3da0c00+4*i,&s2cr)||smr||s2cr!=0x20000)return 0;
 g24n.guard_error=6;
 for(unsigned i=0;i<8;i++)if(kvtophys((vm_offset_t)(g24n.pages.va+16384*i))!=g24n.pages.page[i].pa)return 0;
 g24n.guard_error=0;return 1;
}
/* GWT1 fields: seq,kind,space,index,addressLo/Hi,value,valueKind,phase,
 * cursor,epochLo/Hi. ENTRY is before owner validation; MMIO_INTENT follows the first original guards; after pacing
 * the original live owner/current predicates are repeated before access. ISSUED is not an exact readback. No new MMIO here. */
static int g30_target(unsigned kind,unsigned space,unsigned id,unsigned phase,unsigned cursor){
 if(space==3)return (kind==7||kind==8)&&phase==G24_ZAP&&id>=G24P_INFO&&id<=G24P_START;
 if(kind<1||kind>6)return 0;
 if(phase==G24_CONFIG&&cursor>=76&&cursor<151)return (space==G29_CPAUX&&id<8&&cursor<=79)||(space==G24_PROFILE&&id>=76&&id<151);
 return space==G24_CTL&&phase>=G24_ME_INIT&&phase<=G24_BLIT&&id<G24_CTL_COUNT&&(kind>=4||id==G24_SQE_CONTROL);
}
__attribute__((noinline)) static int g30_witness_pack(const uint32_t*p,uint8_t*b){
 if(!p||!b||!p[0]||p[0]>4096||!g30_target(p[1],p[2],p[3],p[8],p[9])||!(p[10]||p[11]))return 0;
 uint64_t address=(uint64_t)p[4]|(uint64_t)p[5]<<32;
 if(p[2]!=3&&address!=g24_register_address(p[2],p[3]))return 0;
 unsigned value_kind=(p[1]==3||p[1]==8)?1:p[1]>=4?2:0;
 if(p[7]!=value_kind||(!value_kind&&p[6]))return 0;
 uint32_t a[16]={0x31545747u,1,64,0};for(unsigned i=0;i<12;i++)a[4+i]=p[i];
 for(unsigned i=0;i<16;i++)for(unsigned j=0;j<4;j++)b[4*i+j]=(uint8_t)(a[i]>>(8*j));
 uint32_t crc=g28_clock_crc(b+16,48);for(unsigned j=0;j<4;j++)b[12+j]=(uint8_t)(crc>>(8*j));return 1;
}
static uint32_t g30_word(const uint8_t*b,unsigned n){uint32_t v=0;for(unsigned j=0;j<4;j++)v|=(uint32_t)b[4*n+j]<<(8*j);return v;}
static inline int g30_witness_unpack(const uint8_t*b,size_t n,uint32_t*p){
 if(!b||!p||n!=64||g30_word(b,0)!=0x31545747u||g30_word(b,1)!=1||g30_word(b,2)!=64||g30_word(b,3)!=g28_clock_crc(b+16,48))return 0;
 for(unsigned i=0;i<12;i++)p[i]=g30_word(b,4+i);uint8_t again[64];return g30_witness_pack(p,again)&&!memcmp(b,again,64);
}
static unsigned g30_pace(unsigned kind,unsigned space,unsigned id,unsigned seq){
 if(seq==1)return 160;
 if(kind==7)return 80;
 if(kind!=2&&kind!=5)return 0;
 if(space==G24_PROFILE&&id>=80)return kind==2?20:40;
 return 80;
}
__attribute__((noinline)) static int g30_cp_witness(void*ctx,unsigned kind,unsigned space,unsigned id,uint64_t address,uint32_t value){
 struct g24_workspace*w=&g24n.w;
 if(!g30_target(kind,space,id,w->r.phase,w->r.profile_cursor))return 1;
 /* Memory-only validation. Original native owner checks still follow. */
 if(ctx!=g24n.s||g24n.s!=&zg||!g24n.mode||!g24n.prepared||!w->r.epoch||w->r.epoch!=g24n.s->lease_epoch||__atomic_load_n(&g24n.s->lease,__ATOMIC_ACQUIRE)!=1||g24n.witness_sequence>=4096)return 0;
 unsigned seq=++g24n.witness_sequence,vk=(kind==3||kind==8)?1:kind>=4?2:0;
 uint32_t p[12]={seq,kind,space,id,(uint32_t)address,(uint32_t)(address>>32),value,vk,w->r.phase,w->r.profile_cursor,(uint32_t)w->r.epoch,(uint32_t)(w->r.epoch>>32)};
 if(!g30_witness_pack(p,g24n.witness_bytes))return 0;
#if defined(G30_WITNESS_HOST)
 g30_test_witness(g24n.witness_bytes,64,g30_pace(kind,space,id,seq));
#elif !defined(G24_HOST_TEST)
 printf("GWT1 seq%u kind%u sp%u id%u CRC%08x\n",seq,kind,space,id,g30_word(g24n.witness_bytes,3));
 for(unsigned row=0;row<4;row++){unsigned at=row*4;printf("W%02u %08x %08x %08x %08x\n",row,g30_word(g24n.witness_bytes,at),g30_word(g24n.witness_bytes,at+1),g30_word(g24n.witness_bytes,at+2),g30_word(g24n.witness_bytes,at+3));}
 zg_barrier(ctx);unsigned ms=g30_pace(kind,space,id,seq);if(ms)zg_sleep(ms);
#else
 (void)g30_pace;
#endif
 return 1;
}
static int g24n_read(void*ctx,unsigned space,unsigned id,uint32_t*out){
 if(!g30_cp_witness(ctx,1,space,id,g24_register_address(space,id),0))return 0;
 if(ctx!=g24n.s||!out||!g24n_qualified(ctx,g24n.w.r.epoch,g24n.w.r.phase))return 0;
 uint64_t at=g24_register_address(space,id);if(!at)return 0;
 /* No GX register load before grant/power proof. A map alone is not access. */
 if(at<0x3d40000&&at>=0x3d00000&&(!g24n.w.r.power_grant||(g24n.w.r.sptprac&0xc0)))return 0;
 if(!g30_cp_witness(ctx,2,space,id,at,0))return 0;
 if(g30_target(2,space,id,g24n.w.r.phase,g24n.w.r.profile_cursor)&&!g24n_qualified(ctx,g24n.w.r.epoch,g24n.w.r.phase))return 0;
 if(!g24n_raw(at,out))return 0;
 return g30_cp_witness(ctx,3,space,id,at,*out);
}
static int g24n_packet(void){
 struct g24_workspace*w=&g24n.w;uint32_t expected[GP_WORDS];unsigned from=0,count=0;
 if(w->r.phase==G24_ME_INIT){uint32_t init[9]={0x70c80008,0x2f,3,0x20000000,0,0,0,0,0};memcpy(expected,init,sizeof(init));count=9;}
 else if(w->r.phase==G24_UNSECURE){expected[0]=0x70e60001;expected[1]=0;from=9;count=2;}
 else if(w->r.phase==G24_FENCE){expected[0]=0x703d8003;expected[1]=w->memory.fence_iova+8;expected[2]=0;expected[3]=0xc0240000u|(uint32_t)(w->r.epoch&0xffffu);expected[4]=0x70268000;from=11;count=5;}
 else if(w->r.phase==G24_BLIT){size_t words=0;if(gp_build(expected,GP_WORDS,&w->pixel_request,&words)!=GP_OK||words!=GP_WORDS)return 0;from=16;count=GP_WORDS;}else return 0;
 if(w->r.ring_words!=from+count||w->r.program_crc!=gs_crc((uint8_t*)expected,count*4))return 0;
 uint32_t previous;if(!g24n_raw(g24_register_address(G24_CTL,G24_WPTR),&previous)||previous!=from)return 0;
 for(unsigned i=0;i<count;i++)if(w->ring[from+i]!=expected[i])return 0;
 return 1;
}
static int g24n_write(void*ctx,unsigned space,unsigned id,uint32_t value){
 if(!g30_cp_witness(ctx,4,space,id,g24_register_address(space,id),value))return 0;
 struct g24_workspace*w=&g24n.w;if(ctx!=g24n.s||!g24n_qualified(ctx,w->r.epoch,w->r.phase)||w->r.pending_space!=space||w->r.pending_index!=id||w->r.pending_value!=value)return 0;
 uint64_t at=g24_register_address(space,id);uint32_t current=0;
 if(!at||((space!=G24_CTL||id!=G24_REQUEST)&&!(space==G24_CTL&&id==G24_ACK)&&!g24n_raw(at,&current))||!g24_store_allowed(w,space,id,value,current))return 0;
 if(space==G24_CTL){
  if(id==G24_REQUEST){uint32_t fresh;if(!g24n_raw(0x3d7e648,&fresh)||(fresh&0x80000000))return 0;}
  if(id==G24_ACK){uint32_t fresh;if(!g24n_raw(0x3d7e648,&fresh)||!(fresh&0x80000000))return 0;}
  if(id>=G24_SMR&&id<=G24_MAIR1){uint32_t smr,ctl;if(!g24n_raw(0x3da0804,&smr)||smr||!g24n_raw(0x3db0000,&ctl))return 0;if(id!=G24_SCTLR&&id!=G24_S2CR&&id!=G24_SMR&&(ctl&1))return 0;}
  if(id==G24_WPTR&&!g24n_packet())return 0;
 }
 void*p=g24n_pointer(at);if(!p)return 0;
 /* Retain before a store whose completion can be uncertain. */
 if(!g30_cp_witness(ctx,5,space,id,at,value))return 0;
 /* Pacing yields; repeat the original full owner/current/store guards. */
 if(g30_target(5,space,id,w->r.phase,w->r.profile_cursor)){
 struct g24_workspace*w=&g24n.w;if(ctx!=g24n.s||!g24n_qualified(ctx,w->r.epoch,w->r.phase)||w->r.pending_space!=space||w->r.pending_index!=id||w->r.pending_value!=value)return 0;
 uint64_t at=g24_register_address(space,id);uint32_t current=0;
 if(!at||((space!=G24_CTL||id!=G24_REQUEST)&&!(space==G24_CTL&&id==G24_ACK)&&!g24n_raw(at,&current))||!g24_store_allowed(w,space,id,value,current))return 0;
 if(space==G24_CTL){
  if(id==G24_REQUEST){uint32_t fresh;if(!g24n_raw(0x3d7e648,&fresh)||(fresh&0x80000000))return 0;}
  if(id==G24_ACK){uint32_t fresh;if(!g24n_raw(0x3d7e648,&fresh)||!(fresh&0x80000000))return 0;}
  if(id>=G24_SMR&&id<=G24_MAIR1){uint32_t smr,ctl;if(!g24n_raw(0x3da0804,&smr)||smr||!g24n_raw(0x3db0000,&ctl))return 0;if(id!=G24_SCTLR&&id!=G24_S2CR&&id!=G24_SMR&&(ctl&1))return 0;}
  if(id==G24_WPTR&&!g24n_packet())return 0;
 }
 }
 p=g24n_pointer(at);if(!p)return 0;
 w->r.hardware_attempted=1;*(volatile uint32_t*)p=value;zg_barrier(ctx);
#if defined(G24_HOST_TEST)
 g24_test_after_store(at,value);
#endif
 return g30_cp_witness(ctx,6,space,id,at,value);
}
static int g24n_order(void*ctx){if(ctx!=g24n.s)return 0;zg_barrier(ctx);return 1;}
static int g24n_delay(void*ctx,uint32_t us){if(ctx!=g24n.s||us>100)return 0;zg_delay(ctx,us);return 1;}
static int g24n_now(void*ctx,uint64_t*out){if(ctx!=g24n.s||!out||zg_frequency()!=19200000)return 0;*out=zg_ticks(ctx);return 1;}
static int g24n_pas_qualified(void*ctx,uint64_t epoch,unsigned op){
 struct g24_workspace*w=&g24n.w;if(!g24n_qualified(ctx,epoch,w->r.phase)||w->r.phase!=G24_ZAP||!w->r.cp_started||!g24n.metadata_verified)return 0;
 if(op==G24P_INFO)return w->pas.state==G24P_NEW;
 if(op==G24P_METADATA)return w->pas.state==G24P_PROBED&&w->pas.metadata_pa==g24n.pages.page[7].pa;
 if(op==G24P_RANGE)return w->pas.state==G24P_INIT;
 if(op==G24P_START)return w->pas.state==G24P_MEMORY&&g24n.zap_copied;
 return 0;
}
static int g24n_smc(void*ctx,uint64_t r[8]){
 struct g24_workspace*w=&g24n.w;unsigned op=w->pas.state==G24P_NEW?G24P_INFO:w->pas.state==G24P_PROBED?G24P_METADATA:w->pas.state==G24P_INIT?G24P_RANGE:w->pas.state==G24P_MEMORY?G24P_START:0;
 if(!op||!g24n_pas_qualified(ctx,w->r.epoch,op))return 0;
 /* Revalidate the original typed arguments even for interrupted calls.
  * A continuation may carry only the token returned by this PAS session. */
 if(!r||r[5]||r[7])return 0;
 if(r[0]==1){if(w->pas.last[0]!=1||r[6]!=w->pas.last[6])return 0;}
 else if(r[6])return 0;
 if(op==G24P_INFO){if((r[0]!=1&&r[0]!=0xc2000601ULL&&r[0]!=0x42000601ULL)||r[1]!=1||(r[2]!=0x02000601&&r[2]!=0x02000201&&r[2]!=0x02000202&&r[2]!=0x02000205)||r[3]||r[4])return 0;}
 else if(op==G24P_METADATA){if((r[0]!=1&&r[0]!=0x42000201)||r[1]!=130||r[2]!=13||r[3]!=g24n.pages.page[7].pa||r[4])return 0;}
 else if(op==G24P_RANGE){if((r[0]!=1&&r[0]!=0x42000202)||r[1]!=3||r[2]!=13||r[3]!=G24_ZAP_PA||r[4]!=4096)return 0;}
 else if((r[0]!=1&&r[0]!=0x42000205)||r[1]!=1||r[2]!=13||r[3]||r[4])return 0;
 uint64_t witness_fn=r[0],witness_args[8];memcpy(witness_args,r,sizeof(witness_args));if(!g30_cp_witness(ctx,7,3,op,witness_fn,(uint32_t)r[1])||memcmp(witness_args,r,sizeof(witness_args)))return 0;
 {
 struct g24_workspace*w=&g24n.w;unsigned op=w->pas.state==G24P_NEW?G24P_INFO:w->pas.state==G24P_PROBED?G24P_METADATA:w->pas.state==G24P_INIT?G24P_RANGE:w->pas.state==G24P_MEMORY?G24P_START:0;
 if(!op||!g24n_pas_qualified(ctx,w->r.epoch,op))return 0;
 /* Revalidate the original typed arguments even for interrupted calls.
  * A continuation may carry only the token returned by this PAS session. */
 if(!r||r[5]||r[7])return 0;
 if(r[0]==1){if(w->pas.last[0]!=1||r[6]!=w->pas.last[6])return 0;}
 else if(r[6])return 0;
 if(op==G24P_INFO){if((r[0]!=1&&r[0]!=0xc2000601ULL&&r[0]!=0x42000601ULL)||r[1]!=1||(r[2]!=0x02000601&&r[2]!=0x02000201&&r[2]!=0x02000202&&r[2]!=0x02000205)||r[3]||r[4])return 0;}
 else if(op==G24P_METADATA){if((r[0]!=1&&r[0]!=0x42000201)||r[1]!=130||r[2]!=13||r[3]!=g24n.pages.page[7].pa||r[4])return 0;}
 else if(op==G24P_RANGE){if((r[0]!=1&&r[0]!=0x42000202)||r[1]!=3||r[2]!=13||r[3]!=G24_ZAP_PA||r[4]!=4096)return 0;}
 else if((r[0]!=1&&r[0]!=0x42000205)||r[1]!=1||r[2]!=13||r[3]||r[4])return 0;
 }
 g24n.smc_inflight=1;
#if defined(G24_HOST_TEST)
 g24_test_smc(r);
#else
 register uint64_t x0 __asm__("x0")=r[0],x1 __asm__("x1")=r[1],x2 __asm__("x2")=r[2],x3 __asm__("x3")=r[3];
 register uint64_t x4 __asm__("x4")=r[4],x5 __asm__("x5")=r[5],x6 __asm__("x6")=r[6],x7 __asm__("x7")=r[7];
 __asm__ volatile("smc #0":"+r"(x0),"+r"(x1),"+r"(x2),"+r"(x3),"+r"(x4),"+r"(x5),"+r"(x6),"+r"(x7)::"x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","memory");
 r[0]=x0;r[1]=x1;r[2]=x2;r[3]=x3;r[4]=x4;r[5]=x5;r[6]=x6;r[7]=x7;
#endif
 g24n.smc_inflight=0;return g30_cp_witness(ctx,8,3,op,witness_fn,(uint32_t)r[0]);
}
static int g24n_zap_copy(void*ctx){
 struct g24_workspace*w=&g24n.w;if(ctx!=g24n.s||!g24n_qualified(ctx,w->r.epoch,G24_ZAP)||w->pas.state!=G24P_MEMORY||!w->pas.image_retained||g24n.zap_copied||w->fw.code_bytes!=1976||w->fw.load_bytes!=4096)return 0;
 struct zg_geometry geometry;if(!zg_map_geometry(G24_ZAP_PA,4096,PAGE_SIZE,&geometry)||geometry.page_base!=0x8bb18000||geometry.page_span!=16384||geometry.bias!=8192)return 0;
 vm_offset_t va=io_map(geometry.page_base,geometry.page_span,VM_WIMG_WCOMB,VM_PROT_READ|VM_PROT_WRITE,true);if(!va)return 0;
 g24n.zap.region=(struct zg_region){G24_ZAP_PA,4096};g24n.zap.geometry=geometry;g24n.zap.page_va=va;
 /* This is a partial reserved page, never an owned allocator page. The
  * lower shared 8KiB is not read or written. CPU accesses stay inside the
  * 4KiB image authorized by successful PAS_MEM_SETUP. */
 volatile uint8_t*dst=(volatile uint8_t*)(uintptr_t)(g24n.zap.page_va+g24n.zap.geometry.bias);
 for(unsigned pass=0;pass<2;pass++){for(unsigned i=0;i<4096;i++)g24n.zap_backup[pass][i]=dst[i];zg_barrier(ctx);}
 if(memcmp(g24n.zap_backup[0],g24n.zap_backup[1],4096))return 0;
 uint8_t a[32],b[32];g7_sha256(g24n.zap_backup[0],4096,a);g7_sha256(g24n.zap_backup[1],4096,b);if(memcmp(a,b,32))return 0;
 g24n.zap_crc=gs_crc(g24n.zap_backup[0],4096);g24n.zap_backed=1;
 for(unsigned i=0;i<1976;i++)dst[i]=w->fw.code[i];zg_barrier(ctx);
 for(unsigned i=0;i<4096;i++)if(dst[i]!=(i<1976?w->fw.code[i]:g24n.zap_backup[1][i]))return 0;
 if(gs_crc(g24n.zap_backup[0],4096)!=g24n.zap_crc||memcmp(g24n.zap_backup[0],g24n.zap_backup[1],4096))return 0;
 g24n.zap_copied=1;printf("ZIYI G24 ZAP b01 code1976 verified, original4KiB backupCRC%x; shared neighbour untouched\n",g24n.zap_crc);return 1;
}
static int g24n_present(void*ctx){
 struct g24_workspace*w=&g24n.w;if(ctx!=g24n.s||!g24n_qualified(ctx,w->r.epoch,G24_PRESENT)||w->r.verified_pixels!=4096||w->r.proofs!=GP_NATIVE_ALL||!w->r.pas_authenticated||w->r.rptr!=103||w->r.wptr!=103)return 0;
 uint32_t rptr,wptr,status,intr,cp,protect,fsr,global;
 if(!g24n_raw(g24_register_address(G24_CTL,G24_RPTR),&rptr)||!g24n_raw(g24_register_address(G24_CTL,G24_WPTR),&wptr)||!g24n_raw(g24_register_address(G24_CTL,G24_STATUS),&status)||!g24n_raw(g24_register_address(G24_CTL,G24_INT),&intr)||!g24n_raw(g24_register_address(G24_CTL,G24_CPFAULT),&cp)||!g24n_raw(g24_register_address(G24_CTL,G24_CPPROTECT),&protect)||!g24n_raw(g24_register_address(G24_CTL,G24_FSR),&fsr)||!g24n_raw(g24_register_address(G24_CTL,G24_GLOBAL_FSR),&global)||rptr!=103||wptr!=103||protect||global||(intr&0x13000282))return 0;
 struct gp_observation observation={.epoch=w->pixel_request.epoch,.proof_flags=w->r.proofs,.rptr=rptr-16,.wptr=wptr-16,.color_token=w->fence[0],.flush_token=w->fence[1],.cp_fault=cp,.smmu_fault=fsr&0xc00001fe,.gpu_busy=status&(1u<<23)};
 uint32_t bad=0;if(gp_verify(&w->pixel_request,&observation,w->pixels,GP_PAGE,w->guard,(volatile uint8_t*)w->fence+128,64,&bad)!=GP_OK)return 0;
 PE_Video video;PE_current_console(&video);
 if(!zs_strip_pixels(11,video.v_baseAddr,(uint32_t)video.v_width,(uint32_t)video.v_height,(uint32_t)video.v_rowBytes))return 0;
 const uint64_t first=ZS_PHYS+512ull*ZS_STRIDE+284*4,bytes=511ull*ZS_STRIDE+512*4;
 struct zg_geometry geometry;if(!zg_map_geometry(first,bytes,PAGE_SIZE,&geometry))return 0;
 vm_offset_t va=io_map(geometry.page_base,geometry.page_span,VM_WIMG_WCOMB,VM_PROT_READ|VM_PROT_WRITE,true);if(!va)return 0;
 for(unsigned y=0;y<512;y++){volatile uint32_t*dst=(volatile uint32_t*)(uintptr_t)(va+geometry.bias+(uint64_t)y*ZS_STRIDE);const volatile uint32_t*src=(const volatile uint32_t*)(w->pixels+(y/8)*256);for(unsigned x=0;x<512;x++)dst[x]=src[x/8];}
 zg_barrier(ctx);ml_io_unmap(va,geometry.page_span);return 1;
}
__attribute__((noinline)) int ziyi_gpu_cp_prepare(void*ctx){
 unsigned enabled=0,preuser=0,startup=0;PE_parse_boot_argn("ziyi_gpu_cp",&enabled,sizeof(enabled));if(enabled!=1)return 1;
 if(g24n.mode)return 0;g24n.mode=1;g24n.s=ctx;g24n.w.r.phase=G24_NEW;
 PE_parse_boot_argn("ziyi_gpu_preuser",&preuser,sizeof(preuser));PE_parse_boot_argn("ziyi_gpu_startup",&startup,sizeof(startup));
 struct zg_state*s=ctx;if(s!=&zg||s->gmu_enabled!=2||preuser!=1||startup!=1||!zg_adt_check(s,0)||!g24_slot_view(&g24n.w.fw))return g24n_stop(G24_E_ARGUMENT);
 g24n.w.r.epoch=s->lease_epoch;g24n.owner.owner_ctx=s;g24n.owner.owner_evidence=zg_hfi_evidence;
 if(!hop_xnu_prepare(&g24n.pages,&g24n.owner,s->lease_epoch,8))return g24n_stop(G24_E_OWNER);
 struct gc_request request={.epoch=s->lease_epoch,.hfi_pa=s->hfi_memory.page[0].pa,.pages=7,.normal_nc=1,.private_owned=1};for(unsigned i=0;i<7;i++)request.pa[i]=g24n.pages.page[i].pa;
 struct g24_workspace*w=&g24n.w;if(gc_build(&w->memory,&request)!=GC_OK)return g24n_stop(G24_E_SMMU);
 /* Core unprivileged data; SQE leaves additionally read-only. Tables and
  * PAS metadata are never exposed as data through the core's IOVA space. */
 for(unsigned i=0;i<24;i++){unsigned index=((GC_IOVA+i*4096)>>12)&511;w->memory.tables[2][index]|=i>=12?0xc0:0x40;}
 volatile uint8_t*base=(volatile uint8_t*)(uintptr_t)g24n.pages.va;for(unsigned i=0;i<3*4096;i++)base[i]=((uint8_t*)w->memory.tables)[i];for(unsigned i=3*4096;i<16384;i++)base[i]=0xa5;
 w->ring=(volatile uint32_t*)(base+16384);w->pixels=base+2*16384;w->fence=(volatile uint32_t*)(base+3*16384);
 for(unsigned i=0;i<16384;i++)w->pixels[i]=0x55;
 for(unsigned i=0;i<64;i++)w->guard[i]=((volatile uint8_t*)w->fence)[128+i]=(uint8_t)(0xa5^i);
 for(unsigned i=4;i<w->fw.sqe_bytes;i++)base[4*16384+i-4]=w->fw.sqe[i];
 for(unsigned i=0;i<w->fw.mdt_bytes;i++)base[7*16384+i]=w->fw.mdt[i];zg_barrier(s);
 for(unsigned i=4;i<w->fw.sqe_bytes;i++)if(base[4*16384+i-4]!=w->fw.sqe[i])return g24n_stop(G24_E_GUARD);
 for(unsigned i=0;i<w->fw.mdt_bytes;i++)if(base[7*16384+i]!=w->fw.mdt[i])return g24n_stop(G24_E_GUARD);g24n.metadata_verified=1;w->pas.metadata_pa=g24n.pages.page[7].pa;
 if(!hop_xnu_mark_live(&g24n.pages,&g24n.owner))return g24n_stop(G24_E_OWNER);
 const struct zg_region regions[3]={{0x3d00000,0x40000},{0x3d68000,0x38000},{0x3da0000,0x20000}};
 for(unsigned i=0;i<3;i++)if(!zg_map_one(&g24n.maps[i],regions[i],VM_PROT_READ|VM_PROT_WRITE))return g24n_stop(G24_E_IO);
 uint32_t epoch=(uint32_t)s->lease_epoch;if(!epoch)epoch=1;
 w->pixel_request=(struct gp_request){.epoch=epoch,.color_token=0xc0241001,.flush_token=0xc0241002,.ring={w->memory.ring_iova,16384,epoch,1},.target={w->memory.pixel_iova,16384,epoch,1},.fence={w->memory.fence_iova,16384,epoch,1}};
 w->r.phase=G24_READY;w->r.retained=1;g24n.prepared=1;
 printf("ZIYI G24 private NC pages8 prepared: core tables/ring/SQE/PAS metadata; no route or command yet\n");return 1;
}
static uint32_t g24n_record_word(const uint8_t*b,unsigned n){uint32_t x=0;for(unsigned j=0;j<4;j++)x|=(uint32_t)b[4*n+j]<<(8*j);return x;}
__attribute__((noinline)) void ziyi_gpu_cp_print(void*ctx){
 if(ctx!=g24n.s||g24n.mode!=1)return;
 static unsigned page;uint8_t bytes[G24_RECORD_BYTES];
 const struct g24_native_facts facts={g24n.zap_crc,g24n.zap_backed,g24n.zap_copied,g24n.guard_error};
 if(!g24_record_pack(&g24n.w,&facts,bytes,sizeof(bytes))){printf("G24 record refused encoding\n");return;}
 unsigned p=page++%8;
 if(p>=6){uint8_t cp[G29_CP_BYTES];if(!g29_cp_pack(&g24n.w,cp,sizeof(cp))){printf("CPR1 encoding refused\n");return;}
  unsigned part=p-6;printf("CPR1 page%u/2 stage%u staged%x exact%x bad%x CRC%08x\n",part,g24n.w.cp_config.stage,g24n.w.cp_config.staged,g24n.w.cp_config.exact,g24n.w.cp_config.mismatch,g24n_record_word(cp,3));
  for(unsigned row=0;row<5;row++){unsigned at=part*20+row*4;printf("P%02u %08x %08x %08x %08x\n",at/4,g24n_record_word(cp,at),g24n_record_word(cp,at+1),g24n_record_word(cp,at+2),g24n_record_word(cp,at+3));}return;
 }
 if(p==5){uint8_t clock[G28_CLOCK_BYTES];if(!g28_clock_pack(&g24n.w,clock,sizeof(clock))){printf("GCK1 encoding refused\n");return;}
  printf("GCK1 clock mode%u valid%02x CRC%08x\n",g24n.w.clock.mode,g24n.w.clock.valid,g24n_record_word(clock,3));
  for(unsigned row=0;row<5;row++){unsigned n=row*4;printf("K%02u %08x %08x %08x %08x\n",row,g24n_record_word(clock,n),g24n_record_word(clock,n+1),g24n_record_word(clock,n+2),g24n_record_word(clock,n+3));}return;
 }
 printf("GCP1 page%u/5 phase%u failedAt%u err%u CRC%08x\n",p,g24n.w.r.phase,g24n.w.r.failure_phase,g24n.w.r.error,g24n_record_word(bytes,3));
 for(unsigned row=0;row<4;row++){unsigned n=p*16+row*4;printf("C%02u %08x %08x %08x %08x\n",n/4,g24n_record_word(bytes,n),g24n_record_word(bytes,n+1),g24n_record_word(bytes,n+2),g24n_record_word(bytes,n+3));}
}
__attribute__((noinline)) int ziyi_gpu_cp_continue(void*ctx){
 if(g24n.mode!=1)return 0;if(ctx!=g24n.s||!g24n.prepared||g24n.w.r.phase!=G24_READY)return 0;
 const struct g24_io io={ctx,g24n_qualified,g24n_read,g24n_write,g24n_order,g24n_delay,g24n_now,g24n_zap_copy,g24n_present,{ctx,g24n_pas_qualified,g24n_smc}};
 int ok=g24_run(&g24n.w,&io);ziyi_gpu_cp_print(ctx);
 if(g24n.w.r.hardware_attempted||g24n.w.pas.metadata_retained){
  printf("ZIYI G24 diagnostic parked with all borrowers retained; no return into GMU reset\n");
  for(;;){zg_sleep(2000);ziyi_gpu_cp_print(ctx);}
 }
 return ok;
}
