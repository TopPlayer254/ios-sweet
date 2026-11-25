/* G34: immutable VS/FS packet on the existing G33-owned NC pages.
 * No new SMMU/MMIO address, firmware transaction, allocation or reset.
 */
#if !defined(G24_HOST_TEST) || defined(G34_NATIVE_HOST)
static struct {
 struct g34_result r;
 uint8_t ring[16384],fence[16384],baseline[560],final[G34_FINAL_BYTES];
 unsigned baseline_valid,final_valid,submitted;
} g34n;
static int g34n_fail(unsigned error){
 g34n.r.failure_stage=g34n.r.stage;g34n.r.stage=G34_REFUSED;g34n.r.error=error;
 g34n.r.retained=1;g34n.r.end=zg_ticks(g24n.s);return 0;
}
static volatile uint32_t*g34n_frame(void){
#if defined(G34_NATIVE_HOST)
 return (volatile uint32_t*)(void*)framebuffer; /* explicitly synthetic host */
#else
 return g32_fb.frame;
#endif
}
__attribute__((noinline)) static int g34n_guard(void*ctx){
 struct g24_workspace*w=&g24n.w;uint32_t sp,gx;
 return ctx==g24n.s&&(unsigned)cpu_number()==g31.worker_cpu&&
  g34n.r.epoch==w->r.epoch&&w->r.phase==G24_DONE&&!w->r.error&&
  w->r.proofs==GP_NATIVE_ALL&&w->r.verified_pixels==4096&&w->r.presented&&
  w->r.rptr==103&&w->r.wptr==103&&g29_cp_verified(w)&&
  g33n.route.stage==G33_COMPLETE&&!g33n.route.error&&g31.progress==3&&
  hop_xnu_validate(&g24n.pages,&g24n.owner)&&g24n_tables()&&
  g24n_qualified(ctx,w->r.epoch,G24_DONE)&&
  g24n_raw(0x03d7e340,&sp)&&!(sp&0xc0)&&
  g24n_raw(g24_register_address(G24_CTL,G24_GDSC),&gx)&&(gx&0x80000000);
}
__attribute__((noinline)) static int g34n_sample(void*ctx){
 if(!g34n_guard(ctx))return 0;
 const unsigned ids[]={G24_RPTR,G24_WPTR,G24_STATUS,G24_INT,G24_CPFAULT,G24_CPPROTECT,G24_FSR,G24_GLOBAL_FSR};
 uint32_t v[8];for(unsigned i=0;i<8;i++)if(!g24n_raw(g24_register_address(G24_CTL,ids[i]),&v[i]))return 0;
 g34n.r.rptr=v[0];g34n.r.wptr=v[1];g34n.r.status=v[2];g34n.r.intr=v[3];
 g34n.r.cp_fault=v[4];g34n.r.cp_protect=v[5];g34n.r.smmu_fsr=v[6];g34n.r.global_fsr=v[7];
 return 1;
}
static int g34n_faults(void){
 return g34n.r.cp_fault||g34n.r.cp_protect||(g34n.r.smmu_fsr&0xc00001feu)||
  g34n.r.global_fsr||(g34n.r.intr&0x13000282u);
}
__attribute__((noinline)) static int g34n_exact_pages(unsigned completed){
 struct g24_workspace*w=&g24n.w;
 const volatile uint8_t*ring=(const volatile uint8_t*)w->ring;
 const volatile uint8_t*fence=(const volatile uint8_t*)w->fence;
 for(unsigned i=0;i<16384;i++){
  if(ring[i]!=g34n.ring[i])return 0;
  /* Only two fresh completion words and the pre-existing CP RPTR shadow may
   * change. The original G33 event/fence words and its 64-byte guard remain. */
  if(completed&&((i>=12&&i<20)||(i>=64&&i<68)))continue;
  if(fence[i]!=g34n.fence[i])return 0;
 }
 if(g34_crc((const void*)(fence+G34_VS_OFFSET),256)!=G34_VS_CRC||
    g34_crc((const void*)(fence+G34_FS_OFFSET),128)!=G34_FS_CRC)return 0;
 if(completed){
  if(w->fence[3]!=g34n.r.tokens[0]||w->fence[4]!=g34n.r.tokens[1]||
     w->fence[16]<103||w->fence[16]>G34_RING_END)return 0;
 }
 return 1;
}
__attribute__((noinline)) static int g34n_publish(void*ctx){
#if defined(G34_NATIVE_HOST)
 g34_model_before_publish();
#endif
 if(g34n.submitted||g34n.r.stage!=G34_READY||!g34n_guard(ctx)||!g34n_exact_pages(0)||
    !g34n_sample(ctx)||g34n_faults()||g34n.r.rptr!=103||g34n.r.wptr!=103||
    (g34n.r.status&(1u<<23)))return 0;
 void*p=g24n_pointer(g24_register_address(G24_CTL,G24_WPTR));if(!p)return 0;
 /* Last owner/packet/current checks occur after the publication barrier. */
 zg_barrier(ctx);
 if(!g34n_guard(ctx)||!g34n_exact_pages(0)||!g34n_sample(ctx)||g34n_faults()||
    g34n.r.rptr!=103||g34n.r.wptr!=103||(g34n.r.status&(1u<<23)))return 0;
 g34n.submitted=1;g34n.r.stage=G34_SUBMITTED;g34n.r.retained=1;
 *(volatile uint32_t*)p=G34_RING_END;zg_barrier(ctx);
#if defined(G34_NATIVE_HOST)
 g34_model_after_publish();
#endif
 g34n.r.proofs|=G34_PROOF_SUBMIT;return 1;
}
__attribute__((noinline)) static int g34n_present(void*ctx){
 if(!g34n_guard(ctx)||g34n.r.proofs!=255||!g34n_exact_pages(1)||
    !g34n_sample(ctx)||g34n_faults()||g34n.r.rptr!=G34_RING_END||g34n.r.wptr!=G34_RING_END||
    (g34n.r.status&(1u<<23))||g34n.r.triangle_pixels!=512||g34n.r.background_pixels!=3584)return 0;
 volatile uint32_t*frame=g34n_frame();if(!frame)return 0;
 const volatile uint32_t*src=(const volatile uint32_t*)g24n.w.pixels;
 for(unsigned y=0;y<512;y++)for(unsigned x=0;x<512;x++)frame[(512+y)*1080+284+x]=src[(y/8)*64+x/8];
 zg_barrier(ctx);return 1;
}
__attribute__((noinline)) static int g34n_run(void*ctx,int baseline_ok){
 struct g24_workspace*w=&g24n.w;struct g34_result*r=&g34n.r;
 if(r->epoch)return 0;
 r->epoch=w->r.epoch;r->stage=G34_BASELINE;r->retained=1;r->start=zg_ticks(ctx);r->end=r->start;
 r->ring_begin=G34_RING_BEGIN;r->ring_end=G34_RING_END;r->first_bad_byte=UINT32_MAX;
 r->vertex_count=3;r->fragment_outputs=1;r->shader_bytes=640;r->vs_crc=G34_VS_CRC;r->fs_crc=G34_FS_CRC;
 for(unsigned i=0;i<32;i++){r->source_identity[i]=g34_source_identity[i];r->asset_identity[i]=g34_asset_identity[i];}
 const struct g24_native_facts facts={g24n.zap_crc,g24n.zap_backed,g24n.zap_copied,g24n.guard_error};
 g34n.baseline_valid=g24_record_pack(w,&facts,g34n.baseline,320)&&
  g28_clock_pack(w,g34n.baseline+320,80)&&g29_cp_pack(w,g34n.baseline+400,160);
 if(!baseline_ok||!g34n.baseline_valid||!g34n_guard(ctx))return g34n_fail(G34_BASELINE_BAD);
 if(!g34n_sample(ctx)||g34n_faults()||r->rptr!=103||r->wptr!=103||(r->status&(1u<<23)))return g34n_fail(G34_ACTIVE);
 r->proofs=G34_PROOF_OWNER;r->owner_flags=15;r->stage=G34_MEMORY;
 const volatile uint8_t*oldring=(const volatile uint8_t*)w->ring;
 const volatile uint8_t*oldfence=(const volatile uint8_t*)w->fence;
 for(unsigned i=0;i<16384;i++){g34n.ring[i]=i<412?oldring[i]:g34_ring_template[i];g34n.fence[i]=i<256?oldfence[i]:g34_fence_template[i];}
 r->tokens[0]=0xc0340001u^((uint32_t)r->epoch&0x0000ff00u);
 r->tokens[1]=0xc0340002u^((uint32_t)r->epoch&0x0000ff00u);
 g34_put(g34n.ring,G34_TOKEN0_WORD,r->tokens[0]);g34_put(g34n.ring,G34_TOKEN1_WORD,r->tokens[1]);
 g34_put(g34n.fence,3,0);g34_put(g34n.fence,4,0);
 if(!g34n_guard(ctx))return g34n_fail(G34_OWNER);
 volatile uint8_t*ring=(volatile uint8_t*)w->ring;volatile uint8_t*fence=(volatile uint8_t*)w->fence;
 for(unsigned i=412;i<16384;i++)ring[i]=g34n.ring[i];
 for(unsigned i=256;i<16384;i++)fence[i]=g34n.fence[i];
 w->fence[3]=0;w->fence[4]=0;zg_barrier(ctx);
 r->stage=G34_PROGRAM;
 if(!g34n_guard(ctx)||!g34n_exact_pages(0))return g34n_fail(G34_ASSET_BAD);
 r->program_crc=g34_crc(g34n.ring+412,(G34_RING_END-G34_RING_BEGIN)*4);
 r->proofs|=G34_PROOF_PROGRAM|G34_PROOF_SHADERS;r->stage=G34_READY;
 if(!g34n_publish(ctx))return g34n_fail(G34_OWNER);
 r->stage=G34_WAITING;
 for(unsigned n=0;n<20000;n++){
  if(!g34n_sample(ctx))return g34n_fail(G34_OWNER);
  if(g34n_faults())return g34n_fail(G34_FAULT);
  if(r->wptr!=G34_RING_END||r->rptr<103||r->rptr>G34_RING_END)return g34n_fail(G34_ACTIVE);
  r->observed[0]=w->fence[3];r->observed[1]=w->fence[4];
  if(r->rptr==G34_RING_END&&!(r->status&(1u<<23))&&
     r->observed[0]==r->tokens[0]&&r->observed[1]==r->tokens[1])break;
  if(zg_ticks(ctx)-r->start>=19200000ull*3||n==19999)return g34n_fail(G34_TIMEOUT);
  zg_delay(ctx,100);
 }
 r->proofs|=G34_PROOF_CONSUMED;zg_barrier(ctx);
 r->observed[0]=w->fence[3];r->observed[1]=w->fence[4];
 if(r->observed[0]!=r->tokens[0]||r->observed[1]!=r->tokens[1])return g34n_fail(G34_FENCE_BAD);
 r->proofs|=G34_PROOF_FENCES;r->stage=G34_PIXELS;
 if(!g34_check_pixels(r,w->pixels,16384))return g34n_fail(G34_PIXEL_BAD);
 r->proofs|=G34_PROOF_PIXELS;
 if(!g34n_exact_pages(1))return g34n_fail(G34_GUARD_BAD);
 r->guard_count=32768-12;r->guard_crc=g34_crc(g34n.ring,16384)^g34_crc(g34n.fence,16384);
 r->proofs|=G34_PROOF_GUARDS;
 if(!g34n_present(ctx))return g34n_fail(G34_DISPLAY_BAD);
 r->proofs|=G34_PROOF_PRESENTED;r->stage=G34_DONE;r->end=zg_ticks(ctx);return 1;
}
__attribute__((noinline)) static int g34n_finish_record(void*ctx){
 uint8_t tri[256];
 if(ctx!=g24n.s||!g34n.baseline_valid||!g34_result_pack(&g34n.r,tri,sizeof(tri)))return 0;
 g34n.final_valid=g34_final_pack(g34n.final,sizeof(g34n.final),g34n.r.epoch,
  g31.worker_cpu,g31.observer_cpu,g34n.baseline,g34n.baseline+320,g34n.baseline+400,tri);
 return g34n.final_valid;
}
__attribute__((noinline)) static int g34n_emit_final(void*ctx){
 if(ctx!=g24n.s||!g34n.final_valid||__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)||
    (unsigned)cpu_number()!=g31.worker_cpu)return 0;
 volatile uint32_t*frame=g34n_frame();if(!frame)return 0;
 uint8_t packets[10][128];
 for(unsigned p=0;p<10;p++)if(!g34_final_packet(g34n.final,sizeof(g34n.final),p,packets[p],128))return 0;
 for(unsigned p=0;p<10;p++){
  if(!g34_final_draw(frame,1080u*2400u,packets[p],128))return 0;
  zg_barrier(ctx);zg_sleep(200);
 }
 return 1;
}
#else
static int g34n_run(void*c,int ok){(void)c;(void)ok;return 0;}
static int g34n_finish_record(void*c){(void)c;return 0;}
static int g34n_emit_final(void*c){(void)c;return 0;}
#endif
