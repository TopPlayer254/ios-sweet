#include "files/ziyi_g33_optical.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t word(const uint8_t*p,unsigned n){uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[4*n+i]<<(8*i);return v;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void save(const char*dir,const char*name,const void*p,size_t n){char path[2048];assert(snprintf(path,sizeof(path),"%s/%s",dir,name)>0);FILE*f=fopen(path,"wb");assert(f&&fwrite(p,1,n,f)==n&&fclose(f)==0);}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f);uint8_t fixture[384];assert(fread(fixture,1,384,f)==384&&fgetc(f)==EOF&&fclose(f)==0);
 uint64_t epoch=(uint64_t)word(fixture,7)|((uint64_t)word(fixture,8)<<32);
 unsigned worker=word(fixture,5),observer=word(fixture,6);uint8_t gsr[128],record[160],packet[128],bad[160];unsigned refusals=0;
 uint32_t*fb=malloc((1080u*2400u+2)*4);assert(fb);uint32_t*plane=fb+1;
 for(unsigned state=0;state<8;state++){
  memcpy(gsr,fixture,128);put(gsr+16,state>=1&&state<=5?(state==5?0:state-1):0);
  uint64_t table=(uint64_t)word(gsr,20)|((uint64_t)word(gsr,21)<<32);
  if(state==5){table=0x900000000ull;put(gsr+80,0);put(gsr+84,9);}
  put(gsr+12,g32_crc(gsr+16,112));
  const uint8_t*inner=state>=1&&state<=5?gsr:NULL;
  assert(g32_record(state+1,worker,observer,epoch,0x100000000ull+state,inner,record,160));
  unsigned ws=state==0?1:state==6?36:state==7?35:7;
  unsigned oe=state==6?6:0,rs=state==0?0:state==7?6:5,re=state==7?5:0;
  assert(g33_stamp(record,160,ws,oe,rs,re,table)&&g32_validate_record(record,160));
  char name[80];snprintf(name,sizeof(name),"record-%u.bin",state);save(argv[2],name,record,160);
  for(unsigned bit=0;bit<1280;bit++){memcpy(bad,record,160);bad[bit/8]^=1u<<(bit%8);assert(!g32_validate_record(bad,160));refusals++;}
  for(unsigned page=0;page<2;page++){
   assert(g32_packet(record,160,0x12abcdef,page,packet,128));assert(g32_validate_packet(packet,128));
   snprintf(name,sizeof(name),"packet-%u-%u.bin",state,page);save(argv[2],name,packet,128);
   for(unsigned bit=0;bit<1024;bit++){memcpy(bad,packet,128);bad[bit/8]^=1u<<(bit%8);assert(!g32_validate_packet(bad,128));refusals++;}
   for(size_t i=0;i<1080u*2400u+2;i++)fb[i]=0xa5a5a5a5;
   assert(g32_draw(plane,1080u*2400u,1080,2400,1080,packet,128));
   assert(fb[0]==0xa5a5a5a5&&fb[1080u*2400u+1]==0xa5a5a5a5);
   for(unsigned y=0;y<2400;y++)for(unsigned x=0;x<1080;x++)if(x<140||x>=940||y<1200||y>=2000)assert(plane[y*1080+x]==0xa5a5a5a5);
  }
 }
 /* Valid-CRC malformed diagnostics and a full-zero PA are refused. */
 for(unsigned which=0;which<5;which++){
  memcpy(bad,record,160);
  if(which==0)put(bad+144,31);
  if(which==1)put(bad+148,0x01000000);
  if(which==2)put(bad+152,1);
  if(which==3){put(bad+152,0);put(bad+156,0);}
  if(which==4)put(bad+156,16);
  put(bad+12,g32_crc(bad+16,144));assert(!g32_validate_record(bad,160));refusals++;
 }
 free(fb);printf("G33_OPTICAL_HOST_PASS states=8 corruptRefusals=%u zeroLowWordPA=PASS errorPackets=PASS native=0\n",refusals);return 0;
}
