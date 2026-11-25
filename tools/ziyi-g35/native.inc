/* G35 appends real texture/blend/D16 work only after native G34 DONE.
 * No new allocation, page table, register address, firmware or reset. */
#if !defined(G24_HOST_TEST) || defined(G35_NATIVE_HOST)
static struct {
 struct g35_result r;
 uint8_t ring[16384],fence[16384],final[G35_FINAL_BYTES];
 unsigned submitted,final_valid;
} g35n;
static int g35n_fail(unsigned error){
 g35n.r.w[G35_FAILURE_STAGE]=g35n.r.w[G35_STAGE];g35n.r.w[G35_STAGE]=G35_REFUSED;g35n.r.w[G35_ERROR]=error;
 uint64_t t=zg_ticks(g24n.s);g35n.r.w[G35_END_LO]=(uint32_t)t;g35n.r.w[G35_END_HI]=(uint32_t)(t>>32);return 0;
}
__attribute__((noinline)) static int g35n_guard(void*ctx){
 return g34n_guard(ctx)&&g34n.r.stage==G34_DONE&&!g34n.r.error&&g34n.r.proofs==511&&
  g34n.r.rptr==303&&g34n.r.wptr==303&&
  ((uint64_t)g35n.r.w[G35_EPOCH_LO]|(uint64_t)g35n.r.w[G35_EPOCH_HI]<<32)==g34n.r.epoch;
}
__attribute__((noinline)) static int g35n_sample(void*ctx){
 if(!g35n_guard(ctx))return 0;
 const unsigned ids[]={G24_RPTR,G24_WPTR,G24_STATUS,G24_INT,G24_CPFAULT,G24_CPPROTECT,G24_FSR,G24_GLOBAL_FSR};
 for(unsigned i=0;i<8;i++)if(!g24n_raw(g24_register_address(G24_CTL,ids[i]),&g35n.r.w[G35_RPTR+i]))return 0;
 return 1;
}
static int g35n_faults(void){
 uint32_t*w=g35n.r.w;return w[G35_CPFAULT]||w[G35_CPPROTECT]||(w[G35_FSR]&0xc00001feu)||w[G35_GLOBAL_FSR]||(w[G35_INT]&0x13000282u);
}
__attribute__((noinline)) static int g35n_exact_pages(unsigned completed){
 const volatile uint8_t*ring=(const volatile uint8_t*)g24n.w.ring;
 const volatile uint8_t*fence=(const volatile uint8_t*)g24n.w.fence;
 for(unsigned i=0;i<16384;i++){
  if(!(completed&&i>=4096&&i<12288)&&ring[i]!=g35n.ring[i])return 0;
  if(completed&&((i>=20&&i<28)||(i>=64&&i<68)))continue;
  if(fence[i]!=g35n.fence[i])return 0;
 }
 for(unsigned i=0;i<9;i++)if(g35_crc((const void*)(fence+g35_shader_ranges[i][0]),g35_shader_ranges[i][1])!=g35_shader_ranges[i][2])return 0;
 if(completed&&(g24n.w.fence[5]!=g35n.r.w[G35_TOKEN0]||g24n.w.fence[6]!=g35n.r.w[G35_TOKEN1]||
  g24n.w.fence[16]<303||g24n.w.fence[16]>512))return 0;
 return 1;
}
__attribute__((noinline)) static int g35n_publish(void*ctx){
#if defined(G35_NATIVE_HOST)
 g35_model_before_publish();
#endif
 if(g35n.submitted||g35n.r.w[G35_STAGE]!=G35_READY||!g35n_guard(ctx)||!g35n_exact_pages(0)||
  !g35n_sample(ctx)||g35n_faults()||g35n.r.w[G35_RPTR]!=303||g35n.r.w[G35_WPTR]!=303||(g35n.r.w[G35_STATUS]&(1u<<23)))return 0;
 void*p=g24n_pointer(g24_register_address(G24_CTL,G24_WPTR));if(!p)return 0;zg_barrier(ctx);
 if(!g35n_guard(ctx)||!g35n_exact_pages(0)||!g35n_sample(ctx)||g35n_faults()||g35n.r.w[G35_RPTR]!=303||g35n.r.w[G35_WPTR]!=303||(g35n.r.w[G35_STATUS]&(1u<<23)))return 0;
 g35n.submitted=1;g35n.r.w[G35_STAGE]=G35_SUBMITTED;
 *(volatile uint32_t*)p=G35_RING_END;zg_barrier(ctx);
#if defined(G35_NATIVE_HOST)
 g35_model_after_publish();
#endif
 g35n.r.w[G35_PROOFS]|=G35_SUBMIT_PROOF;return 1;
}
__attribute__((noinline)) static int g35n_present(void*ctx){
 if(!g35n_guard(ctx)||g35n.r.w[G35_PROOFS]!=511||!g35n_exact_pages(1)||!g35n_sample(ctx)||g35n_faults()||
  g35n.r.w[G35_RPTR]!=512||g35n.r.w[G35_WPTR]!=512||(g35n.r.w[G35_STATUS]&(1u<<23)))return 0;
 volatile uint32_t*fb=g34n_frame();if(!fb)return 0;const volatile uint32_t*src=(const volatile uint32_t*)g24n.w.pixels;
 for(unsigned y=0;y<512;y++)for(unsigned x=0;x<512;x++)fb[(512+y)*1080+284+x]=src[(y/8)*64+x/8];
 zg_barrier(ctx);return 1;
}
__attribute__((noinline)) static int g35n_run(void*ctx){
 uint32_t*r=g35n.r.w;if(r[G35_EPOCH_LO]||r[G35_EPOCH_HI])return 0;
 r[G35_MAGIC]=0x31525047;r[G35_VERSION]=1;r[G35_BYTES]=256;r[G35_STAGE]=G35_BASELINE;
 r[G35_EPOCH_LO]=(uint32_t)g34n.r.epoch;r[G35_EPOCH_HI]=(uint32_t)(g34n.r.epoch>>32);
 uint64_t start=zg_ticks(ctx);r[G35_START_LO]=(uint32_t)start;r[G35_START_HI]=(uint32_t)(start>>32);
 r[G35_END_LO]=r[G35_START_LO];r[G35_END_HI]=r[G35_START_HI];r[G35_RETAINED]=1;
 r[G35_BEGIN]=303;r[G35_END]=512;r[G35_BAD_PIXEL]=UINT32_MAX;r[G35_BAD_DEPTH]=UINT32_MAX;r[G35_SHADER_BYTES]=1920;r[G35_DRAWS]=4;
 for(unsigned i=0;i<32;i++)r[G35_ASSET_WORD+i/4]|=(uint32_t)g35_asset_identity[i]<<((i%4)*8);
 r[G35_VS_CRC]=g35_shader_ranges[0][2];r[G35_TEX_FS_CRC]=g35_shader_ranges[2][2];r[G35_BLEND_FS_CRC]=g35_shader_ranges[5][2];r[G35_DEPTH_FS_CRC]=g35_shader_ranges[8][2];
 if(!g35n_guard(ctx))return g35n_fail(G35_BASELINE_BAD);
 if(!g35n_sample(ctx)||g35n_faults()||r[G35_RPTR]!=303||r[G35_WPTR]!=303||(r[G35_STATUS]&(1u<<23)))return g35n_fail(G35_ACTIVE);
 r[G35_PROOFS]=G35_OWNER_PROOF;r[G35_OWNER_FLAGS]=15;r[G35_STAGE]=G35_MEMORY;
 volatile uint8_t*ring=(volatile uint8_t*)g24n.w.ring;volatile uint8_t*fence=(volatile uint8_t*)g24n.w.fence;
 for(unsigned i=0;i<16384;i++){g35n.ring[i]=i<1212?ring[i]:g35_ring_template[i];g35n.fence[i]=i<256?fence[i]:g35_fence_template[i];}
 r[G35_TOKEN0]=0xc0350001u^(r[G35_EPOCH_LO]&0x0000ff00u);r[G35_TOKEN1]=0xc0350002u^(r[G35_EPOCH_LO]&0x0000ff00u);
 g35_put(g35n.ring,G35_TOKEN0_WORD,r[G35_TOKEN0]);g35_put(g35n.ring,G35_TOKEN1_WORD,r[G35_TOKEN1]);g35_put(g35n.fence,5,0);g35_put(g35n.fence,6,0);
 if(!g35n_guard(ctx))return g35n_fail(G35_OWNER);
 for(unsigned i=1212;i<16384;i++)ring[i]=g35n.ring[i];for(unsigned i=256;i<16384;i++)fence[i]=g35n.fence[i];
 g24n.w.fence[5]=0;g24n.w.fence[6]=0;zg_barrier(ctx);r[G35_STAGE]=G35_PROGRAM;
 if(!g35n_guard(ctx)||!g35n_exact_pages(0))return g35n_fail(G35_ASSET_BAD);
 r[G35_PROGRAM_CRC]=g35_crc(g35n.ring+1212,836);r[G35_PROOFS]|=G35_PROGRAM_PROOF|G35_SHADER_PROOF;r[G35_STAGE]=G35_READY;
 if(!g35n_publish(ctx))return g35n_fail(G35_OWNER);r[G35_STAGE]=G35_WAITING;
 for(unsigned n=0;n<20000;n++){
  if(!g35n_sample(ctx))return g35n_fail(G35_OWNER);if(g35n_faults())return g35n_fail(G35_FAULT);
  if(r[G35_WPTR]!=512||r[G35_RPTR]<303||r[G35_RPTR]>512)return g35n_fail(G35_ACTIVE);
  r[G35_OBS0]=g24n.w.fence[5];r[G35_OBS1]=g24n.w.fence[6];
  if(r[G35_RPTR]==512&&!(r[G35_STATUS]&(1u<<23))&&r[G35_OBS0]==r[G35_TOKEN0]&&r[G35_OBS1]==r[G35_TOKEN1])break;
  if(zg_ticks(ctx)-start>=19200000ull*3||n==19999)return g35n_fail(G35_TIMEOUT);zg_delay(ctx,100);
 }
 r[G35_PROOFS]|=G35_CONSUMED_PROOF;zg_barrier(ctx);r[G35_OBS0]=g24n.w.fence[5];r[G35_OBS1]=g24n.w.fence[6];
 if(r[G35_OBS0]!=r[G35_TOKEN0]||r[G35_OBS1]!=r[G35_TOKEN1])return g35n_fail(G35_FENCE_BAD);
 r[G35_PROOFS]|=G35_FENCE_PROOF;r[G35_STAGE]=G35_PIXELS;
 if(!g35_check_pixels(&g35n.r,g24n.w.pixels,16384))return g35n_fail(G35_PIXEL_BAD);r[G35_PROOFS]|=G35_PIXEL_PROOF;
 if(!g35_check_depth(&g35n.r,ring+4096,8192))return g35n_fail(G35_DEPTH_BAD);r[G35_PROOFS]|=G35_DEPTH_PROOF;
 if(!g35n_exact_pages(1))return g35n_fail(G35_GUARD_BAD);r[G35_GUARD_COUNT]=24564;r[G35_GUARD_CRC]=g35_crc(g35n.ring,16384)^g35_crc(g35n.fence,16384);r[G35_PROOFS]|=G35_GUARD_PROOF;
 if(!g35n_present(ctx))return g35n_fail(G35_DISPLAY_BAD);r[G35_PROOFS]|=G35_PRESENT_PROOF;r[G35_STAGE]=G35_DONE;
 uint64_t end=zg_ticks(ctx);r[G35_END_LO]=(uint32_t)end;r[G35_END_HI]=(uint32_t)(end>>32);return 1;
}
__attribute__((noinline)) static int g35n_finish_record(void*ctx){
 uint8_t p[256];if(ctx!=g24n.s||!g34n.baseline_valid||!g35_result_pack(&g35n.r,p,256))return 0;
 uint64_t epoch=(uint64_t)g35n.r.w[G35_EPOCH_LO]|(uint64_t)g35n.r.w[G35_EPOCH_HI]<<32;
 g35n.final_valid=g35_final_pack(g35n.final,sizeof(g35n.final),epoch,g31.worker_cpu,g31.observer_cpu,g34n.baseline,g34n.baseline+320,g34n.baseline+400,p);return g35n.final_valid;
}
__attribute__((noinline)) static int g35n_emit_final(void*ctx){
 if(ctx!=g24n.s||!g35n.final_valid||__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)||(unsigned)cpu_number()!=g31.worker_cpu)return 0;
 volatile uint32_t*frame=g34n_frame();if(!frame)return 0;uint8_t p[10][128];
 for(unsigned i=0;i<10;i++)if(!g35_final_packet(g35n.final,sizeof(g35n.final),i,p[i],128))return 0;
 for(unsigned i=0;i<10;i++){if(!g35_final_draw(frame,1080u*2400u,p[i],128))return 0;zg_barrier(ctx);zg_sleep(200);}return 1;
}
#else
static int g35n_run(void*c){(void)c;return 0;}
static int g35n_finish_record(void*c){(void)c;return 0;}
static int g35n_emit_final(void*c){(void)c;return 0;}
#endif
