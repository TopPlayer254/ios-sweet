/* CPU-only observer of ONE guarded SQE start. No observer MMIO or cleanup.
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
static void g31_observer(void*arg,wait_result_t wr){
 (void)arg;(void)wr;processor_t target=cpu_to_processor((int)g31.observer_cpu);
 if(!target){__atomic_store_n(&g31.error,1,__ATOMIC_RELEASE);thread_terminate_self();}
 (void)thread_bind(target);thread_block(THREAD_CONTINUE_NULL);
 if((unsigned)cpu_number()!=g31.observer_cpu){__atomic_store_n(&g31.error,2,__ATOMIC_RELEASE);thread_terminate_self();}
 __atomic_store_n(&g31.ready,1,__ATOMIC_RELEASE);
 uint64_t first=zg_ticks(NULL);
 for(unsigned i=0;i<1200&&zg_ticks(NULL)-first<19200000ULL*60;i++){
  uint8_t b[G31_BYTES];
  if((i%10)==0&&g31_snapshot(b)){
   printf("GSR1 CPU step%u worker%u observer%u CRC%08x\n",g30_word(b,4),g30_word(b,5),g30_word(b,6),g30_word(b,3));
   for(unsigned row=0;row<8;row++){unsigned at=row*4;printf("S%02u %08x %08x %08x %08x\n",row,g30_word(b,at),g30_word(b,at+1),g30_word(b,at+2),g30_word(b,at+3));}
  }
  zg_sleep(50);
 }
 thread_terminate_self();
}
__attribute__((noinline)) static int g31_observer_start(void*ctx){
 if(ctx!=g24n.s||g31.started)return 0;unsigned cpu=(unsigned)cpu_number();if(cpu>7)return 0;
 processor_t here=cpu_to_processor((int)cpu),other=cpu_to_processor((int)(cpu^1));if(!here||!other)return 0;
 (void)thread_bind(here);thread_block(THREAD_CONTINUE_NULL);if((unsigned)cpu_number()!=cpu)return 0;
 g31.worker_cpu=cpu;g31.observer_cpu=cpu^1;g31.started=1;thread_t observer=THREAD_NULL;
 if(kernel_thread_start(g31_observer,NULL,&observer)!=KERN_SUCCESS){g31.error=3;return 0;}thread_deallocate(observer);
 for(unsigned n=0;n<100;n++){if(__atomic_load_n(&g31.error,__ATOMIC_ACQUIRE))return 0;if(__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)==1)return 1;zg_sleep(10);}return 0;
}
#else
__attribute__((noinline)) static int g31_observer_start(void*ctx){if(ctx!=g24n.s)return 0;g31.started=1;g31.worker_cpu=0;g31.observer_cpu=1;g31.ready=1;return 1;}
#endif
__attribute__((noinline)) static int g31_arm(void*ctx,unsigned sp,unsigned id,uint64_t at,uint32_t value){
 if(sp!=G24_CTL||id!=G24_SQE_CONTROL)return 1;
 struct g24_workspace*w=&g24n.w;
 if(ctx!=g24n.s||!g31.started||g31.ready!=1||g31.error||g31.armed||g31.worker_cpu>7||g31.observer_cpu!=(g31.worker_cpu^1)||w->r.phase!=G24_ME_INIT||w->r.cp_cursor!=7||!g29_cp_verified(w)||at!=0x3d02020||value!=1||w->fw.sqe_bytes!=43068)return 0;
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
