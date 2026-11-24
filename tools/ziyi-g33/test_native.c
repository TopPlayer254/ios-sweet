/* Executes the real adapter against G31's CPU-only MMIO model. */
#define G33_NATIVE_HOST 1
static unsigned g33_cpu,g33_sleep_count,g33_sleep_mode;
static int cpu_number(void){return (int)g33_cpu;}
static void g33_sleep_hook(unsigned);
#define main g33_unused_legacy_main
#include "g31-host-support.inc"
#undef main

static void g33_sleep_hook(unsigned ms){
 assert(ms==10);g33_sleep_count++;
 if(g33_sleep_mode==1)g33n.armed_ack=1;
 if(g33_sleep_mode==2)g31.error=6;
 if(g33_sleep_mode==3)g31.ready=0;
 if(g33_sleep_mode==4){g33n.armed_ack=1;g24n.w.r.pending_value^=2;}
}
static void begin(void){
 memset(&g31,0,sizeof(g31));memset(&g33n,0,sizeof(g33n));
 g31_steps=g31_fault=g31_barriers=g30_count=g30_ms=0;
 g33_cpu=g33_sleep_count=g33_sleep_mode=0;
 g29_reset();assert(g31_observer_start(&zg));
 assert(g33n.table_pa==g24n.pages.page[0].pa&&g33n.worker_stage==1);
}
static int packet(void){struct g24_io o=io();o.read=g29_read;return g24_run(&g24n.w,&o);}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(capsule,sizeof(capsule),1,f)==1&&!fclose(f));
 for(unsigned i=0;i<3;i++){unsigned at=i==0?256:i==1?43328:46896,n=i==0?43068:i==1?3564:1976;g7_sha256(capsule+at,n,g24_own_sha[i]);}
 /* Full native adapter packet, including LPAC, fence and modeled pixels. */
 for(unsigned zero_low=0;zero_low<2;zero_low++){
  g33_table_base=zero_low?0xa00000000ull:0x900100000ull;
  begin();g33n.armed_ack=0;g33_sleep_mode=1;
  assert(packet());assert(g33_sleep_count==1&&g33n.worker_stage==8);
  assert(g33n.route.stage==G33_COMPLETE&&g33n.route.writes==2&&g33n.route.reads==32);
  assert(*(uint32_t*)physical(0x03da0808)==0x84000001&&*(uint32_t*)physical(0x03da0c08)==0x300);
  assert(*(uint32_t*)physical(0x03da0800)==0x84000005&&*(uint32_t*)physical(0x03da0c00)==0x302);
  for(unsigned i=3;i<6;i++)assert(!*(uint32_t*)physical(0x03da0800+4*i)&&*(uint32_t*)physical(0x03da0c00+4*i)==0x20000);
  assert(g31_steps==3&&g31.progress==3&&g24n.w.r.verified_pixels==4096&&g24n.w.r.presented);
  uint8_t record[320];struct g24_native_facts facts={g24n.zap_crc,g24n.zap_backed,g24n.zap_copied,g24n.guard_error};
  assert(g24_record_pack(&g24n.w,&facts,record,320));
  if(!zero_low){f=fopen(argv[2],"wb");assert(f&&fwrite(record,1,320,f)==320&&!fclose(f));}
 }
 unsigned refusals=0;g33_table_base=0x900100000ull;
 /* A missing CPU-publication ACK stops before the exact SQE store. */
 for(unsigned mode=0;mode<=4;mode++)if(mode!=1){
  begin();g33n.armed_ack=0;g33_sleep_mode=mode;
  assert(!packet()&&!g31_steps&&!value(G24_SQE_CONTROL)&&!g24n.w.r.cp_started&&!smc_calls&&!g24n.w.r.presented);
  assert(g33_sleep_count==(mode==0?300:1));assert(g33n.route.stage==G33_COMPLETE&&g33n.route.retained);
  if(mode==4)assert(g33n.worker_stage==8);else assert(g33n.worker_stage==7);
  refusals++;
 }
 /* Every valid-bit perturbation of a foreign LPAC slot is rejected before SQE. */
 for(unsigned slot=2;slot<6;slot++)for(unsigned bit=0;bit<32;bit++){
  begin();*(uint32_t*)physical(0x03da0800+4*slot)^=1u<<bit;
  assert(!packet()&&!g31_steps&&!value(G24_SQE_CONTROL)&&!g24n.w.r.presented&&!g33n.route.writes);refusals++;
 }
 /* The adapter's typed callback cannot address any neighboring register. */
 begin();assert(!g33_write_native(&zg,G33_GMU_MATCH,0x84000001));
 assert(!g33_write_native(&zg,G33_CORE_CONTROL,0x300));
 assert(!g33_write_native(&zg,G33_LPAC_CONTROL,0x301));refusals+=3;
 free(raw_allocation);raw_allocation=NULL;
 printf("G33_NATIVE_ADAPTER_HOST_PASS fullPackets2 modeledPixels4096 zeroLowWordPA1 LPACStores2 publicationTimeout300x10ms freshGuardAfterACK1 refusals%u native0\n",refusals);return 0;
}
