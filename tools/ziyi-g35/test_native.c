/* CPU-only fault model of the actual G34 native adapter. Not native rendering. */
#define G34_NATIVE_HOST 1
#define G35_NATIVE_HOST 1
static void g35_model_before_publish(void);
static void g35_model_after_publish(void);
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

static unsigned g35_mode,g35_stores;
static void g35_model_before_publish(void){
 if(g35_mode==10)zg.lease=0;
 if(g35_mode==11)((volatile uint8_t*)g24n.w.fence)[g35_shader_ranges[0][0]]^=1;
 if(g35_mode==12)g33_cpu=1;
 if(g35_mode==13)set(G24_RPTR,304);
}
static void g35_model_after_publish(void){
 g35_stores++;assert(g35_stores==1&&value(G24_WPTR)==512&&g35n.submitted&&g35n_exact_pages(0));
 /* Explicit synthetic scene, independent strip construction. The production
  * adapter reads images only; it has no CPU raster fallback. */
 const unsigned char tex[2][2][4]={{{0,0,255,255},{0,255,0,255}},{{255,0,0,255},{255,255,255,255}}};
 for(unsigned row=0;row<64;row++){
  for(unsigned col=0;col<16;col++)memcpy((void*)(g24n.w.pixels+row*256+col*4),tex[row%2][col%2],4);
  for(unsigned col=16;col<32;col++){const unsigned char b[4]={32,128,96,255};memcpy((void*)(g24n.w.pixels+row*256+col*4),b,4);}
  for(unsigned col=32;col<64;col++){const unsigned char green[4]={0,255,0,255};memcpy((void*)(g24n.w.pixels+row*256+col*4),green,4);}
 }
 volatile uint8_t*depth=(volatile uint8_t*)g24n.w.ring+4096;
 for(unsigned i=0;i<4096;i++){
  unsigned v=(i%64)<32?65535:16384;depth[i*2]=(uint8_t)v;depth[i*2+1]=(uint8_t)(v>>8);
 }
 g24n.w.fence[5]=g35n.r.w[G35_TOKEN0];g24n.w.fence[6]=g35n.r.w[G35_TOKEN1];g24n.w.fence[16]=512;set(G24_RPTR,512);
 if(g35_mode==1)g24n.w.fence[5]=0;
 if(g35_mode==2)set(G24_CPFAULT,1);
 if(g35_mode==3)g24n.w.pixels[0]^=1;
 if(g35_mode==4)g24n.w.pixels[16*4]^=1;
 if(g35_mode==5)g24n.w.pixels[32*4+1]=0;
 if(g35_mode==6)depth[0]=0;
 if(g35_mode==7)depth[32*2+1]=0;
 if(g35_mode==8)((volatile uint8_t*)g24n.w.fence)[200]^=1;
 if(g35_mode==9)set(G24_RPTR,303);
}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(capsule,sizeof(capsule),1,f)==1&&!fclose(f));
 for(unsigned i=0;i<3;i++){unsigned at=i==0?256:i==1?43328:46896,n=i==0?43068:i==1?3564:1976;g7_sha256(capsule+at,n,g24_own_sha[i]);}
 for(unsigned mode=0;mode<15;mode++){
  begin();g33n.armed_ack=1;assert(packet());memset(&g34n,0,sizeof(g34n));g34_mode=0;g34_stores=0;
  assert(g34n_run(&zg,1)&&g34n_finish_record(&zg));
  memset(&g35n,0,sizeof(g35n));g35_mode=mode;g35_stores=0;uint32_t before=g34_crc(framebuffer,sizeof(framebuffer));
  if(mode==14)g34n.r.stage=G34_REFUSED;
  int ok=g35n_run(&zg);uint8_t raw[256];assert(g35_result_pack(&g35n.r,raw,256));
  if(!mode){
   assert(ok&&g35_stores==1&&g35n.r.w[G35_STAGE]==G35_DONE&&g35n.r.w[G35_PROOFS]==1023);
   assert(g35n.r.w[G35_TEX_PIXELS]==1024&&g35n.r.w[G35_BLEND_PIXELS]==1024&&g35n.r.w[G35_DEPTH_PIXELS]==2048);
   assert(g35n_finish_record(&zg)&&g35_final_valid(g35n.final,sizeof(g35n.final)));
   f=fopen(argv[2],"wb");assert(f&&fwrite(g35n.final,1,sizeof(g35n.final),f)==sizeof(g35n.final)&&!fclose(f));
   assert(g34_crc(framebuffer,sizeof(framebuffer))!=before);
  }else{
   assert(!ok&&g35n.r.w[G35_STAGE]==G35_REFUSED&&!(g35n.r.w[G35_PROOFS]&G35_PRESENT_PROOF));
   assert(g35_stores==(mode<10?1u:0u));assert(g34_crc(framebuffer,sizeof(framebuffer))==before);
   assert((mode!=1&&mode!=9)||g35n.r.w[G35_ERROR]==G35_TIMEOUT);
   assert(mode!=2||g35n.r.w[G35_ERROR]==G35_FAULT);
   assert((mode<3||mode>5)||g35n.r.w[G35_ERROR]==G35_PIXEL_BAD);
   assert((mode!=6&&mode!=7)||g35n.r.w[G35_ERROR]==G35_DEPTH_BAD);
   assert(mode!=8||g35n.r.w[G35_ERROR]==G35_GUARD_BAD);
   assert(mode!=14||g35n.r.w[G35_ERROR]==G35_BASELINE_BAD);
  }
 }
 free(raw_allocation);raw_allocation=NULL;
 puts("G35_ACTUAL_NATIVE_MODEL_PASS cases15 realG34Prelude15 texture/blend/depth/fence/guard/owner/CPU/active/timeouts; syntheticOnly1 native0");return 0;
}
