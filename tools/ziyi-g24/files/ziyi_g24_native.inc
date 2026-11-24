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

/* G33 adds only the stock LPAC stream, to already owned core context 0. */
/* PC-qualified candidate. No native adapter is connected to this module yet. */

enum g33_register { G33_GMU_MATCH, G33_GMU_CONTROL, G33_CORE_MATCH,
 G33_CORE_CONTROL, G33_LPAC_MATCH, G33_LPAC_CONTROL, G33_REGISTERS };
enum g33_stage { G33_NEW, G33_SNAPSHOT, G33_CONTROL_ISSUED,
 G33_CONTROL_VERIFIED, G33_MATCH_ISSUED, G33_COMPLETE, G33_REFUSED };
enum g33_error { G33_OK, G33_ARGUMENT, G33_OWNER, G33_IO, G33_OCCUPIED, G33_READBACK };
struct g33_record {
 uint64_t epoch, table_pa;
 uint32_t stage, failed_stage, error, retained, reads, writes;
 uint32_t before[G33_REGISTERS], after[G33_REGISTERS];
 uint32_t before_valid, after_valid;
};
struct g33_io {
 void *ctx;
 /* Must freshly qualify the original lease, epoch, live GMU/GX/core tables,
  * NC backing and CP cohort. The two owned LPAC words are checked here. */
 int (*guard)(void *, uint64_t epoch, uint64_t core_table_pa);
 int (*read)(void *, enum g33_register, uint32_t *);
 int (*write)(void *, enum g33_register, uint32_t);
 int (*order)(void *);
};
uint64_t g33_address(enum g33_register);
int g33_install(struct g33_record *, const struct g33_io *, uint64_t epoch,
 uint64_t core_table_pa);

uint64_t g33_address(enum g33_register id) {
 static const uint64_t addresses[G33_REGISTERS] = {
  0x03da0800,0x03da0c00,0x03da0804,0x03da0c04,0x03da0808,0x03da0c08
 };
 return (unsigned)id<G33_REGISTERS?addresses[id]:0;
}
static int g33_lpac_refuse(struct g33_record *r, unsigned error) {
 r->failed_stage=r->stage; __atomic_store_n(&r->error,error,__ATOMIC_RELEASE); __atomic_store_n(&r->stage,G33_REFUSED,__ATOMIC_RELEASE); return 0;
}
static int g33_lpac_guard(struct g33_record *r,const struct g33_io *io) {
 return io->guard(io->ctx,r->epoch,r->table_pa) || g33_lpac_refuse(r,G33_OWNER);
}
static int g33_lpac_read_word(struct g33_record *r,const struct g33_io *io,
 enum g33_register id,uint32_t *value) {
 if(!g33_lpac_guard(r,io))return 0;
 if(!io->read(io->ctx,id,value))return g33_lpac_refuse(r,G33_IO);
 r->reads++;
 return g33_lpac_guard(r,io);
}
static int g33_lpac_exact(struct g33_record *r,const struct g33_io *io,
 enum g33_register id,uint32_t expected) {
 uint32_t value=0;
 if(!g33_lpac_read_word(r,io,id,&value))return 0;
 r->after[id]=value;r->after_valid|=1u<<id;
 return value==expected || g33_lpac_refuse(r,G33_READBACK);
}
static int g33_lpac_issue(struct g33_record *r,const struct g33_io *io,
 enum g33_register id,uint32_t value,unsigned stage) {
 if(!((id==G33_LPAC_CONTROL&&value==0x300&&stage==G33_CONTROL_ISSUED)||
      (id==G33_LPAC_MATCH&&value==0x84000001&&stage==G33_MATCH_ISSUED)))
  return g33_lpac_refuse(r,G33_ARGUMENT);
 if(!g33_lpac_guard(r,io))return 0;
 /* No rollback after an issued transaction: a match may already admit DMA. */
 __atomic_store_n(&r->stage,stage,__ATOMIC_RELEASE);r->retained=1;r->writes++;
 if(!io->write(io->ctx,id,value)||!io->order(io->ctx))return g33_lpac_refuse(r,G33_IO);
 return g33_lpac_exact(r,io,id,value);
}
int g33_install(struct g33_record *r,const struct g33_io *io,uint64_t epoch,
 uint64_t core_table_pa) {
 static const uint32_t empty[G33_REGISTERS]={0x84000005,0x302,0x84000000,0x300,0,0x20000};
 static const uint32_t live[G33_REGISTERS]={0x84000005,0x302,0x84000000,0x300,0x84000001,0x300};
 if(!r||!io||!io->guard||!io->read||!io->write||!io->order||
    !epoch||!core_table_pa||(core_table_pa&16383)||core_table_pa>=(1ull<<36))return 0;
 if(r->stage!=G33_NEW||r->epoch||r->table_pa||r->before_valid||r->after_valid||r->retained||r->reads||r->writes)return 0;
 r->epoch=epoch;r->table_pa=core_table_pa;__atomic_store_n(&r->stage,G33_SNAPSHOT,__ATOMIC_RELEASE);
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<G33_REGISTERS;i++) {
  uint32_t value=0;if(!g33_lpac_read_word(r,io,(enum g33_register)i,&value))return 0;
  if(!pass){r->before[i]=value;r->before_valid|=1u<<i;}
  if(value!=empty[i])return g33_lpac_refuse(r,G33_OCCUPIED);
 }
 /* Bind only SID 1 / 0x401 to the already owned core context. SID 2 and 5
  * remain outside this match. No combined mask or context-bank write. */
 if(!g33_lpac_issue(r,io,G33_LPAC_CONTROL,0x300,G33_CONTROL_ISSUED))return 0;
 __atomic_store_n(&r->stage,G33_CONTROL_VERIFIED,__ATOMIC_RELEASE);
 for(unsigned i=0;i<G33_LPAC_CONTROL;i++)
  if(!g33_lpac_exact(r,io,(enum g33_register)i,empty[i]))return 0;
 if(!g33_lpac_exact(r,io,G33_LPAC_CONTROL,0x300))return 0;
 if(!g33_lpac_issue(r,io,G33_LPAC_MATCH,0x84000001,G33_MATCH_ISSUED))return 0;
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<G33_REGISTERS;i++)
  if(!g33_lpac_exact(r,io,(enum g33_register)i,live[i]))return 0;
 if(!g33_lpac_guard(r,io))return 0;
 __atomic_store_n(&r->stage,G33_COMPLETE,__ATOMIC_RELEASE);return 1;
}

static struct {struct g33_record route;uint64_t table_pa;uint32_t worker_cpu,worker_stage,armed_ack;} g33n;
static void g33_worker_stage(uint32_t stage){__atomic_store_n(&g33n.worker_stage,stage,__ATOMIC_RELEASE);}
static inline int g33_initial(void*ctx,unsigned cpu){
 if(ctx!=g24n.s||cpu>7||g33n.table_pa||!g24n.pages.page[0].pa||g33n.route.stage)return 0;
 g33n.table_pa=g24n.pages.page[0].pa;g33n.worker_cpu=cpu;g33_worker_stage(1);return 1;
}
static int g33_slot_permitted(uint32_t match,uint32_t control){
 unsigned stage=__atomic_load_n(&g33n.route.stage,__ATOMIC_ACQUIRE);
 if(stage==G33_NEW||stage==G33_SNAPSHOT)return !match&&control==0x20000;
 if(stage==G33_CONTROL_ISSUED)return !match&&(control==0x20000||control==0x300);
 if(stage==G33_CONTROL_VERIFIED)return !match&&control==0x300;
 if(stage==G33_MATCH_ISSUED)return (match==0||match==0x84000001)&&control==0x300;
 return stage==G33_COMPLETE&&match==0x84000001&&control==0x300;
}
#if !defined(G24_HOST_TEST) || defined(G33_NATIVE_HOST)
static int g33_prepare_lpac(void*);
static int g33_wait_publication(void*,unsigned,unsigned);
#else
static int g33_wait_publication(void*c,unsigned sp,unsigned id){(void)c;(void)sp;(void)id;return 1;}
#endif

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
 for(unsigned i=2;i<6;i++){if(!g24n_raw(0x3da0800+4*i,&smr)||!g24n_raw(0x3da0c00+4*i,&s2cr))return 0;if(i==2){if(!g33_slot_permitted(smr,s2cr))return 0;}else if(smr||s2cr!=0x20000)return 0;}
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
/* G32 CPU optical observer of ONE guarded SQE start. No GPU-register access or cleanup.
 * Progress2 means the CPU reached the instruction after STR, not device ACK.
 * Progress3 means the original DSB returned, not an exact register readback. */
#if !defined(G24_HOST_TEST) || defined(G31_OBSERVER_HOST)
#define G31_BYTES 128u
static struct {
 uint32_t progress __attribute__((aligned(4)));
 uint32_t armed,ready,error,started,worker_cpu,observer_cpu;
 uint32_t binding[27];
} g31;
__attribute__((noinline)) static int g31_record_pack(const uint32_t*p,uint8_t*b){
 if(!p||!b||p[0]>3||p[1]>7||p[2]>7||p[1]==p[2]||!(p[3]||p[4])||p[5]!=0x3d02020||p[6]||p[7]!=1||p[8]!=G24_ME_INIT||p[9]!=7||p[10]!=150||!p[11]||p[11]>4096||p[12]!=GC_IOVA||p[13]!=GC_IOVA+49152||p[14]!=GC_IOVA+32768||p[15]!=43064||!(p[16]||p[17])||(p[16]&16383)||p[17]>15||p[18]!=3||p[19]!=15||p[20]!=0xffffff||p[21]||p[22]!=1||p[23]||p[24]||p[25]||p[26]||p[27])return 0;
 uint32_t a[32]={0x31525347u,1,G31_BYTES,0};for(unsigned i=0;i<28;i++)a[4+i]=p[i];
 for(unsigned i=0;i<32;i++)for(unsigned j=0;j<4;j++)b[4*i+j]=(uint8_t)(a[i]>>(8*j));
 uint32_t crc=g28_clock_crc(b+16,112);for(unsigned j=0;j<4;j++)b[12+j]=(uint8_t)(crc>>(8*j));return 1;
}
static inline int g31_record_unpack(const uint8_t*b,size_t n,uint32_t*p){
 if(!b||!p||n!=G31_BYTES||g30_word(b,0)!=0x31525347u||g30_word(b,1)!=1||g30_word(b,2)!=G31_BYTES||g30_word(b,3)!=g28_clock_crc(b+16,112))return 0;
 for(unsigned i=0;i<28;i++)p[i]=g30_word(b,4+i);uint8_t again[G31_BYTES];return g31_record_pack(p,again)&&!memcmp(b,again,G31_BYTES);
}
__attribute__((noinline)) static int g31_snapshot(uint8_t*b){
 if(!__atomic_load_n(&g31.armed,__ATOMIC_ACQUIRE)||__atomic_load_n(&g31.error,__ATOMIC_RELAXED)||__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)!=1)return 0;
 uint32_t p[28];p[0]=__atomic_load_n(&g31.progress,__ATOMIC_RELAXED);for(unsigned i=0;i<27;i++)p[i+1]=g31.binding[i];return g31_record_pack(p,b);
}
#if !defined(G24_HOST_TEST)
#include <kern/processor.h>
#include <arm/cpu_number.h>
/* Exact XNU-private declarations; linked definitions independently checked. */
extern processor_t thread_bind(processor_t);
extern void thread_terminate_self(void);
/* CPU diagnostic output only. No GPU register or command API. */
#define G32_BYTES 160u
uint32_t g32_crc(const void*,size_t);
int g32_validate_record(const uint8_t*,size_t);
int g32_validate_packet(const uint8_t*,size_t);
/* gsr128, if non-NULL, must reference exactly 128 readable bytes. Output and
 * input ranges must not overlap. All failures occur before output writes. */
int g32_record(uint32_t sample,unsigned worker,unsigned observer,uint64_t epoch,
 uint64_t counter,const uint8_t *gsr128,uint8_t *out,size_t capacity);
int g32_packet(const uint8_t *record,size_t bytes,uint32_t tag,unsigned page,uint8_t *out,size_t capacity);
int g32_draw(volatile uint32_t *frame,size_t words,unsigned width,unsigned height,
 unsigned stride,const uint8_t *packet,size_t bytes);

int g33_stamp(uint8_t*,size_t,uint32_t,uint32_t,uint32_t,uint32_t,uint64_t);
/* CPU optical diagnostics, not GPU rendering. ZMP1RAW1 transport follows the
 * measured u24-s2 implementation. No GPU register/command API is present. */

__attribute__((noinline)) uint32_t g32_crc(const void *ptr, size_t n) {
 const uint8_t *p=ptr; uint32_t c=~0u;
 for(size_t i=0;i<n;i++) { c^=p[i]; for(unsigned b=0;b<8;b++) c=(c>>1)^(0xedb88320u&(0u-(c&1))); }
 return ~c;
}
static uint32_t g32_word(const uint8_t *p,unsigned n) {
 uint32_t v=0; for(unsigned j=0;j<4;j++) v|=(uint32_t)p[4*n+j]<<(8*j); return v;
}
static void g32_put32(uint8_t *p,uint32_t v) { for(unsigned j=0;j<4;j++) p[j]=(uint8_t)(v>>(8*j)); }
static void g32_put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static int g32_overlap(const volatile void *a,size_t an,const volatile void *b,size_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x || bn>UINTPTR_MAX-y) return 1;
 return x<y+bn && y<x+an;
}
/* Same immutable binding contract as the G31 actual-C/Python wire schema. */
static int g32_binding(const uint32_t *p) {
 if(p[0]>7 || p[1]>7 || p[0]==p[1] || !(p[2]|p[3]) ||
    p[4]!=0x03d02020u || p[5] || p[6]!=1 || p[7]!=6 || p[8]!=7 || p[9]!=150 ||
    !p[10] || p[10]>4096 || p[11]!=0x61000000u || p[12]!=0x6100c000u ||
    p[13]!=0x61008000u || p[14]!=43064 || !(p[15]|p[16]) || (p[15]&16383) || p[16]>15 ||
    p[17]!=3 || p[18]!=15 || p[19]!=0x00ffffffu || p[20] || p[21]!=1) return 0;
 for(unsigned i=22;i<27;i++) if(p[i]) return 0;
 return 1;
}
static int g33_diag_valid(const uint32_t*a) {
 if(!(a[36]|a[37]|a[38]|a[39]))return 1; /* internal base record, before stamp */
 if(!((a[36]>=1&&a[36]<=8)||(a[36]>=32&&a[36]<=36))||a[37]>>24||
    (a[37]&255)>7||((a[37]>>8)&255)>6||((a[37]>>16)&255)>6||
    !(a[38]|a[39])||(a[38]&16383)||a[39]>15)return 0;
 if(a[5]==3&&(a[24]!=a[38]||a[25]!=a[39]))return 0;
 return 1;
}
__attribute__((noinline)) int g32_validate_record(const uint8_t *r,size_t n) {
 if(!r || n!=G32_BYTES) return 0;
 uint32_t a[40]; for(unsigned i=0;i<40;i++) a[i]=g32_word(r,i);
 if(a[0]!=0x32424f47u || a[1]!=2 || a[2]!=G32_BYTES || a[3]!=g32_crc(r+16,144) ||
    !a[4] || !(a[7]|a[8]) || a[9]>7 || a[10]>7 || a[9]==a[10] || !(a[11]|a[12])) return 0;
 if(!g33_diag_valid(a)) return 0;
 if(a[5]==1) { if(a[6]) return 0; for(unsigned i=13;i<36;i++) if(a[i]) return 0; return 1; }
 if(a[5]!=3 || a[6]>3 || !g32_binding(a+9)) return 0;
 if(!g33_diag_valid(a)) return 0;
 return 1;
}
__attribute__((noinline)) int g32_record(uint32_t sample,unsigned worker,unsigned observer,uint64_t epoch,
 uint64_t counter,const uint8_t *gsr,uint8_t *out,size_t n) {
 if(!out || n!=G32_BYTES || !sample || worker>7 || observer>7 || worker==observer || !epoch || !counter ||
    (gsr && g32_overlap(gsr,128,out,n))) return 0;
 uint32_t a[40]={0x32424f47u,2,G32_BYTES,0,sample,1,0,(uint32_t)counter,(uint32_t)(counter>>32),worker,observer,(uint32_t)epoch,(uint32_t)(epoch>>32)};
 if(gsr) {
  if(g32_word(gsr,0)!=0x31525347u || g32_word(gsr,1)!=1 || g32_word(gsr,2)!=128 ||
     g32_word(gsr,3)!=g32_crc(gsr+16,112) || g32_word(gsr,4)>3 ||
     g32_word(gsr,5)!=worker || g32_word(gsr,6)!=observer ||
     g32_word(gsr,7)!=(uint32_t)epoch || g32_word(gsr,8)!=(uint32_t)(epoch>>32)) return 0;
  a[5]=3; a[6]=g32_word(gsr,4); for(unsigned i=0;i<27;i++) a[9+i]=g32_word(gsr,5+i);
  if(!g32_binding(a+9)) return 0;
 }
 for(unsigned i=0;i<40;i++) g32_put32(out+4*i,a[i]);
 g32_put32(out+12,g32_crc(out+16,144)); return 1;
}
__attribute__((noinline)) int g32_validate_packet(const uint8_t *p,size_t n) {
 static const uint8_t magic[8]={'Z','M','P','1','R','A','W','1'};
 if(!p || n!=128) return 0;
 for(unsigned i=0;i<8;i++) if(p[i]!=magic[i]) return 0;
 unsigned page=(unsigned)p[16]|((unsigned)p[17]<<8);
 if(g32_word(p,2)!=0x00200001u || !g32_word(p,3) || page>1 ||
    p[18]!=2 || p[19] || g32_word(p,5)!=G32_BYTES || g32_word(p,6)!=page*92 ||
    g32_word(p,7)!=(page?68u:92u) || g32_word(p,31)!=g32_crc(p,124)) return 0;
 for(unsigned i=32+(page?68u:92u);i<124;i++) if(p[i]) return 0;
 return 1;
}
__attribute__((noinline)) int g32_packet(const uint8_t *r,size_t n,uint32_t tag,unsigned page,uint8_t *out,size_t cap) {
 if(!out || cap!=128 || !tag || page>=2 || !g32_validate_record(r,n) || g32_overlap(r,n,out,cap)) return 0;
 for(unsigned i=0;i<128;i++) out[i]=0;
 const char magic[]="ZMP1RAW1"; for(unsigned i=0;i<8;i++) out[i]=(uint8_t)magic[i];
 g32_put16(out+8,1); g32_put16(out+10,32); g32_put32(out+12,tag); g32_put16(out+16,(uint16_t)page);
 g32_put16(out+18,2); g32_put32(out+20,G32_BYTES); g32_put32(out+24,page*92);
 unsigned count=page?68:92; g32_put16(out+28,(uint16_t)count);
 for(unsigned i=0;i<count;i++) out[32+i]=r[page*92+i];
 g32_put32(out+124,g32_crc(out,124)); return 1;
}
__attribute__((noinline)) int g32_draw(volatile uint32_t *fb,size_t words,unsigned width,unsigned height,
 unsigned stride,const uint8_t *p,size_t bytes) {
 if(!fb || width!=1080 || height!=2400 || stride!=1080 || words<(size_t)stride*height ||
    words>SIZE_MAX/sizeof(uint32_t) || ((uintptr_t)fb&3) || !g32_validate_packet(p,bytes) ||
    g32_overlap(fb,words*sizeof(uint32_t),p,bytes)) return 0;
 enum {SIDE=800,CELL=20,OX=140,OY=1200};
 for(unsigned y=0;y<SIDE;y++) for(unsigned x=0;x<SIDE;x++) {
  unsigned cx=x/CELL,cy=y/CELL; uint32_t c=0xffffffff;
  if(cx>=2 && cx<=37 && cy>=2 && cy<=37 && (cx==2 || cx==37 || cy==2 || cy==37)) c=0;
  if(cx>=4 && cx<36 && cy>=4 && cy<36) { unsigned b=(cy-4)*32+cx-4; c=(p[b/8]&(1u<<(7-b%8)))?0xffffffff:0; }
  fb[(OY+y)*stride+OX+x]=c;
 }
 return 1;
}

__attribute__((noinline)) int g33_stamp(uint8_t*r,size_t n,uint32_t worker,uint32_t observer_error,
 uint32_t route_stage,uint32_t route_error,uint64_t table_pa) {
 if(!g32_validate_record(r,n)||!((worker>=1&&worker<=8)||(worker>=32&&worker<=36))||
    observer_error>7||route_stage>6||route_error>6||!table_pa||
    (table_pa&16383)||table_pa>=(1ull<<36))return 0;
 g32_put32(r+144,worker);g32_put32(r+148,observer_error|(route_error<<8)|(route_stage<<16));
 g32_put32(r+152,(uint32_t)table_pa);g32_put32(r+156,(uint32_t)(table_pa>>32));
 g32_put32(r+12,g32_crc(r+16,144));return g32_validate_record(r,n);
}
/* G32 observer writes only the measured CPU framebuffer rectangle. It never
 * calls printf/console or reads GPU registers. Mapping is prepared by worker
 * before thread creation and retained throughout the bounded diagnostic. */
static struct {
 volatile uint32_t *frame;
 size_t words;
 uint64_t epoch;
} g32_fb;
__attribute__((noinline)) static int g32_fb_prepare(void*ctx){
 if(ctx!=g24n.s||g32_fb.frame||!g24n.w.r.epoch)return 0;
 PE_Video video;PE_current_console(&video);
 if(!zs_strip_pixels(11,video.v_baseAddr,(uint32_t)video.v_width,(uint32_t)video.v_height,(uint32_t)video.v_rowBytes))return 0;
 if(video.v_width!=1080||video.v_height!=2400||video.v_rowBytes!=4320)return 0;
 struct zg_geometry geometry;
 if(!zg_map_geometry(ZS_PHYS,2400ull*ZS_STRIDE,PAGE_SIZE,&geometry)||geometry.bias)return 0;
 vm_offset_t va=io_map(geometry.page_base,geometry.page_span,VM_WIMG_WCOMB,VM_PROT_READ|VM_PROT_WRITE,true);
 if(!va)return 0;
 g32_fb.frame=(volatile uint32_t*)(uintptr_t)va;g32_fb.words=2400u*1080u;g32_fb.epoch=g24n.w.r.epoch;return 1;
}
__attribute__((noinline)) static int g32_emit_record(uint32_t sample,const uint8_t*gsr){
 uint8_t record[G32_BYTES],packets[2][128];
 static const char tag[]="ZIYI-20261113-G32X-CPU-OPTICAL";
 if(!g32_fb.frame||(unsigned)cpu_number()!=g31.observer_cpu||
    !g32_record(sample,g31.worker_cpu,g31.observer_cpu,g32_fb.epoch,zg_ticks(NULL),gsr,record,sizeof(record)))return 0;
 if(!g33_stamp(record,sizeof(record),__atomic_load_n(&g33n.worker_stage,__ATOMIC_ACQUIRE),
 __atomic_load_n(&g31.error,__ATOMIC_ACQUIRE),__atomic_load_n(&g33n.route.stage,__ATOMIC_ACQUIRE),
 __atomic_load_n(&g33n.route.error,__ATOMIC_ACQUIRE),g33n.table_pa))return 0;
 uint32_t run=g32_crc(tag,sizeof(tag)-1);
 /* Freeze and validate BOTH pages of one snapshot before drawing either. */
 if(!g32_packet(record,sizeof(record),run,0,packets[0],128)||
    !g32_packet(record,sizeof(record),run,1,packets[1],128))return 0;
 for(unsigned page=0;page<2;page++){
  if(!g32_draw(g32_fb.frame,g32_fb.words,1080,2400,1080,packets[page],128))return 0;
  /* Observer-only publication of CPU WC pixels; never on the SQE worker. */
  __asm__ volatile("dsb st":::"memory");
  zg_sleep(150);
 }
 return 1;
}
static void g31_observer(void*arg,wait_result_t wr){
 (void)arg;(void)wr;processor_t target=cpu_to_processor((int)g31.observer_cpu);
 if(!target){__atomic_store_n(&g31.error,1,__ATOMIC_RELEASE);thread_terminate_self();}
 (void)thread_bind(target);thread_block(THREAD_CONTINUE_NULL);
 if((unsigned)cpu_number()!=g31.observer_cpu){__atomic_store_n(&g31.error,2,__ATOMIC_RELEASE);thread_terminate_self();}
 uint64_t first=zg_ticks(NULL);
 /* A complete unarmed heartbeat pair is displayed BEFORE opening worker gate. */
 if(!g32_emit_record(1,NULL)){__atomic_store_n(&g31.error,4,__ATOMIC_RELEASE);thread_terminate_self();}
 __atomic_store_n(&g31.ready,1,__ATOMIC_RELEASE);
 for(unsigned i=0;i<160&&zg_ticks(NULL)-first<19200000ULL*60;i++){
  uint8_t b[G31_BYTES];const uint8_t*snapshot=NULL;
  if((unsigned)cpu_number()!=g31.observer_cpu){__atomic_store_n(&g31.error,5,__ATOMIC_RELEASE);break;}
  if(__atomic_load_n(&g31.armed,__ATOMIC_ACQUIRE)){
   if(!g31_snapshot(b)){__atomic_store_n(&g31.error,6,__ATOMIC_RELEASE);(void)g32_emit_record(i+2,NULL);break;}
   snapshot=b;
  }
  if(!g32_emit_record(i+2,snapshot)){__atomic_store_n(&g31.error,7,__ATOMIC_RELEASE);(void)g32_emit_record(i+2,NULL);break;}
  if(snapshot)__atomic_store_n(&g33n.armed_ack,1,__ATOMIC_RELEASE);
  zg_sleep(50);
 }
 __atomic_store_n(&g31.ready,0,__ATOMIC_RELEASE);
 thread_terminate_self();
}
__attribute__((noinline)) static int g31_observer_start(void*ctx){
 if(ctx!=g24n.s||g31.started)return 0;unsigned cpu=(unsigned)cpu_number();if(cpu>7)return 0;
 processor_t here=cpu_to_processor((int)cpu),other=cpu_to_processor((int)(cpu^1));if(!here||!other)return 0;
 (void)thread_bind(here);thread_block(THREAD_CONTINUE_NULL);if((unsigned)cpu_number()!=cpu)return 0;
 if(!g33_initial(ctx,cpu)||!g32_fb_prepare(ctx))return 0;
 g31.worker_cpu=cpu;g31.observer_cpu=cpu^1;g31.started=1;thread_t observer=THREAD_NULL;
 if(kernel_thread_start(g31_observer,NULL,&observer)!=KERN_SUCCESS){g31.error=3;return 0;}thread_deallocate(observer);
 for(unsigned n=0;n<300;n++){if(__atomic_load_n(&g31.error,__ATOMIC_ACQUIRE))return 0;if(__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)==1)return 1;zg_sleep(10);}return 0;
}
#else
__attribute__((noinline)) static int g31_observer_start(void*ctx){if(ctx!=g24n.s)return 0;g31.started=1;g31.worker_cpu=0;g31.observer_cpu=1;g31.ready=1;
#if defined(G33_NATIVE_HOST)
 if(!g33_initial(ctx,0))return 0;g33n.armed_ack=1; /* synthetic observer stand-in */
#endif
 return 1;}
#endif
__attribute__((noinline)) static int g31_arm(void*ctx,unsigned sp,unsigned id,uint64_t at,uint32_t value){
 if(sp!=G24_CTL||id!=G24_SQE_CONTROL)return 1;
 g33_worker_stage(3);
 struct g24_workspace*w=&g24n.w;
 if(ctx!=g24n.s||!g31.started||__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)!=1||__atomic_load_n(&g31.error,__ATOMIC_ACQUIRE)||g31.armed||g31.worker_cpu>7||g31.observer_cpu!=(g31.worker_cpu^1)||w->r.phase!=G24_ME_INIT||w->r.cp_cursor!=7||!g29_cp_verified(w)||at!=0x3d02020||value!=1||w->fw.sqe_bytes!=43068){g33_worker_stage(32);return 0;}
#if !defined(G24_HOST_TEST)
 if((unsigned)cpu_number()!=g31.worker_cpu){g33_worker_stage(33);return 0;}
#endif
 const volatile uint8_t*sqe=(const volatile uint8_t*)(uintptr_t)(g24n.pages.va+4*16384);
 for(unsigned i=4;i<w->fw.sqe_bytes;i++)if(sqe[i-4]!=w->fw.sqe[i]){g33_worker_stage(34);return 0;}
 g33_worker_stage(4);
#if !defined(G24_HOST_TEST) || defined(G33_NATIVE_HOST)
 if(!g33_prepare_lpac(ctx)){g33_worker_stage(35);return 0;}
#endif
 g33_worker_stage(5);
 uint32_t p[28]={0,g31.worker_cpu,g31.observer_cpu,(uint32_t)w->r.epoch,(uint32_t)(w->r.epoch>>32),(uint32_t)at,(uint32_t)(at>>32),value,w->r.phase,w->r.cp_cursor,w->r.profile_cursor,g24n.witness_sequence,w->memory.ring_iova,w->memory.sqe_iova,w->memory.fence_iova,w->fw.sqe_bytes-4,(uint32_t)g24n.pages.page[0].pa,(uint32_t)(g24n.pages.page[0].pa>>32),w->cp_config.stage,w->cp_config.exact,w->cp_config.valid,w->cp_config.aux[15],1,0,0,0,0,0};
 uint8_t test[G31_BYTES];if(!g31_record_pack(p,test)){g33_worker_stage(36);return 0;}
 for(unsigned i=0;i<27;i++)g31.binding[i]=p[i+1];__atomic_store_n(&g31.progress,0,__ATOMIC_RELAXED);__atomic_store_n(&g31.armed,1,__ATOMIC_RELEASE);g33_worker_stage(6);return 1;
}
__attribute__((noinline)) static void g31_sqe_store(void*p,uint32_t value){
#if !defined(G24_HOST_TEST)
 /* No call, print, sleep or extra barrier between the final guards and STR.
  * Plain aligned STR implements the CPU progress atomic stores. */
 __asm__ volatile("mov w9,#1\nstr w9,[%1]\nstr %w2,[%0]\nmov w9,#2\nstr w9,[%1]\ndsb sy\nmov w9,#3\nstr w9,[%1]"::"r"(p),"r"(&g31.progress),"r"(value):"x9","memory");
#else
 __atomic_store_n(&g31.progress,1,__ATOMIC_RELAXED);g31_test_step(1,p,value);
 *(volatile uint32_t*)p=value;__atomic_store_n(&g31.progress,2,__ATOMIC_RELAXED);g31_test_step(2,p,value);
 zg_barrier(g24n.s);__atomic_store_n(&g31.progress,3,__ATOMIC_RELAXED);g31_test_step(3,p,value);
#endif
}
#else
/* Inherited fixtures exercise their original target packet; G31's own tests
 * compile the real CPU binding/codec/step path with G31_OBSERVER_HOST. */
static inline int g31_observer_start(void*ctx){return ctx==g24n.s;}
static inline int g31_arm(void*ctx,unsigned sp,unsigned id,uint64_t at,uint32_t value){(void)sp;(void)id;(void)at;(void)value;return ctx==g24n.s;}
static inline void g31_sqe_store(void*p,uint32_t value){*(volatile uint32_t*)p=value;zg_barrier(g24n.s);}
#endif

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

#if !defined(G24_HOST_TEST) || defined(G33_NATIVE_HOST)
static int g33_live(void*ctx,uint64_t epoch,uint64_t pa){
 return ctx==g24n.s&&pa==g33n.table_pa&&pa==g24n.pages.page[0].pa&&
  (unsigned)cpu_number()==g33n.worker_cpu&&g24n.w.r.phase==G24_ME_INIT&&
  g24n.w.r.cp_cursor==7&&epoch==g24n.w.r.epoch&&g24n_qualified(ctx,epoch,G24_ME_INIT);
}
static int g33_read_native(void*ctx,enum g33_register id,uint32_t*out){
 return g33_live(ctx,g24n.w.r.epoch,g33n.table_pa)&&g24n_raw(g33_address(id),out);
}
static int g33_write_native(void*ctx,enum g33_register id,uint32_t value){
 if(!g33_live(ctx,g24n.w.r.epoch,g33n.table_pa)||
  !((id==G33_LPAC_CONTROL&&value==0x300&&g33n.route.stage==G33_CONTROL_ISSUED)||
    (id==G33_LPAC_MATCH&&value==0x84000001&&g33n.route.stage==G33_MATCH_ISSUED)))return 0;
 void*p=g24n_pointer(g33_address(id));if(!p)return 0;*(volatile uint32_t*)p=value;return 1;
}
static int g33_order_native(void*ctx){if(ctx!=g24n.s)return 0;zg_barrier(ctx);return 1;}
__attribute__((noinline)) static int g33_prepare_lpac(void*ctx){
 const struct g33_io io={ctx,g33_live,g33_read_native,g33_write_native,g33_order_native};
 return g33_install(&g33n.route,&io,g24n.w.r.epoch,g33n.table_pa);
}
__attribute__((noinline)) static int g33_wait_publication(void*ctx,unsigned sp,unsigned id){
 if(sp!=G24_CTL||id!=G24_SQE_CONTROL)return 1;
 if(ctx!=g24n.s||!g31.armed||g33n.route.stage!=G33_COMPLETE)return 0;
 g33_worker_stage(7);
 for(unsigned n=0;n<300;n++){
  if(__atomic_load_n(&g31.error,__ATOMIC_ACQUIRE)||!__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE))return 0;
  if(__atomic_load_n(&g33n.armed_ack,__ATOMIC_ACQUIRE)){g33_worker_stage(8);return 1;}
  zg_sleep(10);
 }
 return 0;
}
#endif

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
 if(!g31_arm(ctx,space,id,at,value))return 0;
 if(!g33_wait_publication(ctx,space,id))return 0;
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
 w->r.hardware_attempted=1;
 if(space==G24_CTL&&id==G24_SQE_CONTROL)g31_sqe_store(p,value);
 else{*(volatile uint32_t*)p=value;zg_barrier(ctx);}
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
 if(!g31_observer_start(ctx)){g24n_stop(G24_E_OWNER);for(;;){zg_sleep(2000);ziyi_gpu_cp_print(ctx);}}
 const struct g24_io io={ctx,g24n_qualified,g24n_read,g24n_write,g24n_order,g24n_delay,g24n_now,g24n_zap_copy,g24n_present,{ctx,g24n_pas_qualified,g24n_smc}};
 int ok=g24_run(&g24n.w,&io);ziyi_gpu_cp_print(ctx);
 if(g24n.w.r.hardware_attempted||g24n.w.pas.metadata_retained){
  printf("ZIYI G24 diagnostic parked with all borrowers retained; no return into GMU reset\n");
  for(;;){zg_sleep(2000);ziyi_gpu_cp_print(ctx);}
 }
 return ok;
}
