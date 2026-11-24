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
 if(!p||!b||p[0]>3||p[1]>7||p[2]>7||p[1]==p[2]||!(p[3]||p[4])||p[5]!=0x3d02020||p[6]||p[7]!=1||p[8]!=G24_ME_INIT||p[9]!=7||p[10]!=150||!p[11]||p[11]>4096||p[12]!=GC_IOVA||p[13]!=GC_IOVA+49152||p[14]!=GC_IOVA+32768||p[15]!=43064||!p[16]||(p[16]&16383)||p[17]>15||p[18]!=3||p[19]!=15||p[20]!=0xffffff||p[21]||p[22]!=1||p[23]||p[24]||p[25]||p[26]||p[27])return 0;
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
    p[13]!=0x61008000u || p[14]!=43064 || !p[15] || (p[15]&16383) || p[16]>15 ||
    p[17]!=3 || p[18]!=15 || p[19]!=0x00ffffffu || p[20] || p[21]!=1) return 0;
 for(unsigned i=22;i<27;i++) if(p[i]) return 0;
 return 1;
}
__attribute__((noinline)) int g32_validate_record(const uint8_t *r,size_t n) {
 if(!r || n!=G32_BYTES) return 0;
 uint32_t a[40]; for(unsigned i=0;i<40;i++) a[i]=g32_word(r,i);
 if(a[0]!=0x32424f47u || a[1]!=1 || a[2]!=G32_BYTES || a[3]!=g32_crc(r+16,144) ||
    !a[4] || !(a[7]|a[8]) || a[9]>7 || a[10]>7 || a[9]==a[10] || !(a[11]|a[12])) return 0;
 if(a[5]==1) { if(a[6]) return 0; for(unsigned i=13;i<40;i++) if(a[i]) return 0; return 1; }
 if(a[5]!=3 || a[6]>3 || !g32_binding(a+9)) return 0;
 for(unsigned i=36;i<40;i++) if(a[i]) return 0;
 return 1;
}
__attribute__((noinline)) int g32_record(uint32_t sample,unsigned worker,unsigned observer,uint64_t epoch,
 uint64_t counter,const uint8_t *gsr,uint8_t *out,size_t n) {
 if(!out || n!=G32_BYTES || !sample || worker>7 || observer>7 || worker==observer || !epoch || !counter ||
    (gsr && g32_overlap(gsr,128,out,n))) return 0;
 uint32_t a[40]={0x32424f47u,1,G32_BYTES,0,sample,1,0,(uint32_t)counter,(uint32_t)(counter>>32),worker,observer,(uint32_t)epoch,(uint32_t)(epoch>>32)};
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
   if(!g31_snapshot(b)){__atomic_store_n(&g31.error,6,__ATOMIC_RELEASE);break;}
   snapshot=b;
  }
  if(!g32_emit_record(i+2,snapshot)){__atomic_store_n(&g31.error,7,__ATOMIC_RELEASE);break;}
  zg_sleep(50);
 }
 __atomic_store_n(&g31.ready,0,__ATOMIC_RELEASE);
 thread_terminate_self();
}
__attribute__((noinline)) static int g31_observer_start(void*ctx){
 if(ctx!=g24n.s||g31.started)return 0;unsigned cpu=(unsigned)cpu_number();if(cpu>7)return 0;
 processor_t here=cpu_to_processor((int)cpu),other=cpu_to_processor((int)(cpu^1));if(!here||!other)return 0;
 (void)thread_bind(here);thread_block(THREAD_CONTINUE_NULL);if((unsigned)cpu_number()!=cpu)return 0;
 if(!g32_fb_prepare(ctx))return 0;
 g31.worker_cpu=cpu;g31.observer_cpu=cpu^1;g31.started=1;thread_t observer=THREAD_NULL;
 if(kernel_thread_start(g31_observer,NULL,&observer)!=KERN_SUCCESS){g31.error=3;return 0;}thread_deallocate(observer);
 for(unsigned n=0;n<300;n++){if(__atomic_load_n(&g31.error,__ATOMIC_ACQUIRE))return 0;if(__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)==1)return 1;zg_sleep(10);}return 0;
}
#else
__attribute__((noinline)) static int g31_observer_start(void*ctx){if(ctx!=g24n.s)return 0;g31.started=1;g31.worker_cpu=0;g31.observer_cpu=1;g31.ready=1;return 1;}
#endif
__attribute__((noinline)) static int g31_arm(void*ctx,unsigned sp,unsigned id,uint64_t at,uint32_t value){
 if(sp!=G24_CTL||id!=G24_SQE_CONTROL)return 1;
 struct g24_workspace*w=&g24n.w;
 if(ctx!=g24n.s||!g31.started||__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)!=1||__atomic_load_n(&g31.error,__ATOMIC_ACQUIRE)||g31.armed||g31.worker_cpu>7||g31.observer_cpu!=(g31.worker_cpu^1)||w->r.phase!=G24_ME_INIT||w->r.cp_cursor!=7||!g29_cp_verified(w)||at!=0x3d02020||value!=1||w->fw.sqe_bytes!=43068)return 0;
#if !defined(G24_HOST_TEST)
 if((unsigned)cpu_number()!=g31.worker_cpu)return 0;
#endif
 const volatile uint8_t*sqe=(const volatile uint8_t*)(uintptr_t)(g24n.pages.va+4*16384);
 for(unsigned i=4;i<w->fw.sqe_bytes;i++)if(sqe[i-4]!=w->fw.sqe[i])return 0;
 uint32_t p[28]={0,g31.worker_cpu,g31.observer_cpu,(uint32_t)w->r.epoch,(uint32_t)(w->r.epoch>>32),(uint32_t)at,(uint32_t)(at>>32),value,w->r.phase,w->r.cp_cursor,w->r.profile_cursor,g24n.witness_sequence,w->memory.ring_iova,w->memory.sqe_iova,w->memory.fence_iova,w->fw.sqe_bytes-4,(uint32_t)g24n.pages.page[0].pa,(uint32_t)(g24n.pages.page[0].pa>>32),w->cp_config.stage,w->cp_config.exact,w->cp_config.valid,w->cp_config.aux[15],1,0,0,0,0,0};
 uint8_t test[G31_BYTES];if(!g31_record_pack(p,test))return 0;
 for(unsigned i=0;i<27;i++)g31.binding[i]=p[i+1];__atomic_store_n(&g31.progress,0,__ATOMIC_RELAXED);__atomic_store_n(&g31.armed,1,__ATOMIC_RELEASE);return 1;
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
