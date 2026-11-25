/* CPU-only fault model of the actual G34 native adapter. Not native rendering. */
#define G34_NATIVE_HOST 1
static void g34_model_before_publish(void);
static void g34_model_after_publish(void);
#include "g33-native-support.inc"
static unsigned g34_mode,g34_stores;
static void g34_model_before_publish(void){
 if(g34_mode==6)zg.lease=0;
 if(g34_mode==7)((volatile uint8_t*)g24n.w.fence)[G34_VS_OFFSET]^=1;
 if(g34_mode==8)g33_cpu=1;
 if(g34_mode==9)set(G24_RPTR,104);
}
static void g34_model_after_publish(void){
 g34_stores++;assert(g34_stores==1&&value(G24_WPTR)==303&&g34n.submitted);
 assert(g34n_exact_pages(0));
 /* Independent, explicitly fabricated GPU-result fixture. The production
  * adapter has no target writes or CPU fallback, and no model hook in XNU. */
 for(unsigned y=16;y<48;y++)for(unsigned x=0;x<64;x++){
  if((int)(4*x)>=31+2*(int)y&&(int)(4*x)<=221-2*(int)y){
   uint32_t c=0xffff00ffu;for(unsigned b=0;b<4;b++)g24n.w.pixels[(y*64+x)*4+b]=(uint8_t)(c>>(b*8));
  }
 }
 g24n.w.fence[3]=g34n.r.tokens[0];g24n.w.fence[4]=g34n.r.tokens[1];g24n.w.fence[16]=303;
 set(G24_RPTR,303);
 if(g34_mode==1)g24n.w.fence[3]=0;
 if(g34_mode==2)set(G24_CPFAULT,1);
 if(g34_mode==3)g24n.w.pixels[8193]^=1;
 if(g34_mode==4)((volatile uint8_t*)g24n.w.fence)[200]^=1;
 if(g34_mode==5)set(G24_RPTR,103);
}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(capsule,sizeof(capsule),1,f)==1&&!fclose(f));
 for(unsigned i=0;i<3;i++){unsigned at=i==0?256:i==1?43328:46896,n=i==0?43068:i==1?3564:1976;g7_sha256(capsule+at,n,g24_own_sha[i]);}
 unsigned passed=0;
 for(unsigned mode=0;mode<11;mode++){
  begin();g33n.armed_ack=1;assert(packet());
  memset(&g34n,0,sizeof(g34n));g34_mode=mode;g34_stores=0;
  uint32_t before=g34_crc(framebuffer,sizeof(framebuffer));
  int ok=g34n_run(&zg,mode!=10);
  if(!mode){
   assert(ok&&g34_stores==1&&g34n.r.stage==G34_DONE&&g34n.r.proofs==511);
   assert(g34n.r.triangle_pixels==512&&g34n.r.background_pixels==3584);
   assert(g34_crc(framebuffer,sizeof(framebuffer))!=before);
   assert(g34n_finish_record(&zg)&&g34_final_valid(g34n.final,sizeof(g34n.final)));
   f=fopen(argv[2],"wb");assert(f&&fwrite(g34n.final,1,sizeof(g34n.final),f)==sizeof(g34n.final)&&!fclose(f));
  }else{
   assert(!ok&&g34n.r.stage==G34_REFUSED&&!(g34n.r.proofs&G34_PROOF_PRESENTED));
   assert(g34_stores==(mode<=5?1u:0u));
   assert(g34_crc(framebuffer,sizeof(framebuffer))==before);
   assert((mode!=1&&mode!=5)||g34n.r.error==G34_TIMEOUT);
   assert(mode!=2||g34n.r.error==G34_FAULT);
   assert(mode!=3||g34n.r.error==G34_PIXEL_BAD);
   assert(mode!=4||g34n.r.error==G34_GUARD_BAD);
   uint8_t receipt[256];assert(g34_result_pack(&g34n.r,receipt,sizeof(receipt)));
  }
  assert(g24n.w.r.phase==G24_DONE&&g24n.w.r.rptr==103&&g24n.w.r.wptr==103&&g24n.w.r.presented);
  passed++;
 }
 free(raw_allocation);raw_allocation=NULL;
 printf("G34_ACTUAL_NATIVE_MODEL_PASS cases%u realG33Prelude11 oneTypedPublish timeout/fence/fault/pixel/canary/lateOwner/code/CPU/active/baseline refusal; syntheticOnly1 native0\n",passed);
 return 0;
}
