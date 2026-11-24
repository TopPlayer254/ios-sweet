/* Actual-C synthetic host controls. No hardware claims. */
#include "files/ziyi_g32_optical.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t word(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void save(const char *dir,const char *name,const void *p,size_t n) {
 char path[2048]; assert(snprintf(path,sizeof(path),"%s/%s",dir,name)>0); FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(p,1,n,f)==n); assert(fclose(f)==0);
}
int main(int argc,char **argv) {
 assert(argc==3); FILE *f=fopen(argv[1],"rb"); assert(f); uint8_t source[384]; assert(fread(source,1,384,f)==384); assert(fgetc(f)==EOF); fclose(f);
 uint8_t gsr[128], record[160], packet[128], corrupt[160], untouched[160];
 const unsigned w=1080,h=2400; size_t count=(size_t)w*h; uint32_t *allocation=malloc((count+2)*4); assert(allocation);
 uint32_t *fb=allocation+1; uint8_t *crop=malloc(800*800); assert(crop);
 uint64_t epoch=(uint64_t)word(source+28)|((uint64_t)word(source+32)<<32); unsigned worker=word(source+20),observer=word(source+24);
 const uint32_t tag=0x12abcdefu;
 unsigned bit_refusals=0,draws=0;
 for(unsigned state=0;state<5;state++) {
  memcpy(gsr,source,128); put(gsr+16,state?state-1:0); put(gsr+12,g32_crc(gsr+16,112));
  assert(g32_record(state+1,worker,observer,epoch,1000+state,state?gsr:NULL,record,160)); assert(g32_validate_record(record,160));
  char name[128]; snprintf(name,sizeof(name),"record-%u.bin",state); save(argv[2],name,record,160);
  for(unsigned bit=0;bit<1280;bit++) { memcpy(corrupt,record,160); corrupt[bit/8]^=(uint8_t)(1u<<(bit%8)); assert(!g32_validate_record(corrupt,160)); bit_refusals++; }
  for(unsigned page=0;page<2;page++) {
   assert(g32_packet(record,160,tag,page,packet,128)); assert(g32_validate_packet(packet,128));
   snprintf(name,sizeof(name),"packet-%u-%u.bin",state,page); save(argv[2],name,packet,128);
   for(unsigned bit=0;bit<1024;bit++) { memcpy(corrupt,packet,128); corrupt[bit/8]^=(uint8_t)(1u<<(bit%8)); assert(!g32_validate_packet(corrupt,128)); bit_refusals++; }
   for(size_t i=0;i<count+2;i++) allocation[i]=0xa5a5a5a5u;
   assert(g32_draw(fb,count,w,h,w,packet,128)); draws++;
   assert(allocation[0]==0xa5a5a5a5u && allocation[count+1]==0xa5a5a5a5u);
   for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++) {
    uint32_t pixel=fb[y*w+x];
    if(x>=140 && x<940 && y>=1200 && y<2000) { assert(pixel==0 || pixel==0xffffffffu); crop[(y-1200)*800+x-140]=(uint8_t)pixel; }
    else assert(pixel==0xa5a5a5a5u);
   }
   snprintf(name,sizeof(name),"crop-%u-%u.gray",state,page); save(argv[2],name,crop,800*800);
  }
 }
 /* Refusal must leave output untouched, including corrupt-but-valid-CRC schema. */
 memset(untouched,0xa5,sizeof(untouched)); memcpy(corrupt,untouched,160);
 assert(!g32_record(0,worker,observer,epoch,1,gsr,corrupt,160)); assert(!memcmp(corrupt,untouched,160));
 assert(!g32_record(1,worker,worker,epoch,1,gsr,corrupt,160)); assert(!g32_record(1,worker,observer,0,1,gsr,corrupt,160));
 assert(!g32_record(1,worker,observer,epoch,0,gsr,corrupt,160)); assert(!g32_record(1,worker,observer,epoch,1,gsr,corrupt,159));
 assert(!memcmp(corrupt,untouched,160));
 uint8_t alias[320]; memset(alias,0x5a,sizeof(alias)); memcpy(alias,gsr,128); uint8_t before[320]; memcpy(before,alias,sizeof(alias));
 assert(!g32_record(1,worker,observer,epoch,1,alias,alias+64,160)); assert(!memcmp(alias,before,sizeof(alias)));
 memcpy(alias,record,160); memcpy(before,alias,sizeof(alias)); assert(!g32_packet(alias,160,tag,0,alias+80,128)); assert(!memcmp(alias,before,sizeof(alias)));
 assert(!g32_packet(record,160,tag,2,corrupt,128)); assert(!g32_packet(record,160,0,0,corrupt,128));
 for(unsigned index=13;index<40;index++) {
  assert(g32_record(1,worker,observer,epoch,1,NULL,record,160)); put(record+index*4,1); put(record+12,g32_crc(record+16,144)); assert(!g32_validate_record(record,160));
 }
 for(unsigned index=9;index<32;index++) {
  memcpy(gsr,source,128); put(gsr+index*4,index==20?1:word(gsr+index*4)^0x80000000u); put(gsr+12,g32_crc(gsr+16,112));
  assert(!g32_record(1,worker,observer,epoch,1,gsr,corrupt,160));
 }
 assert(g32_packet(record,160,tag,0,packet,128)==0); /* invalid schema retained */
 memcpy(gsr,source,128); assert(g32_record(1,worker,observer,epoch,1,gsr,record,160)); assert(g32_packet(record,160,tag,0,packet,128));
 for(size_t i=0;i<count+2;i++) allocation[i]=0xa5a5a5a5u;
 assert(!g32_draw(fb,count,w-1,h,w,packet,128)); assert(!g32_draw(fb,count,w,h-1,w,packet,128)); assert(!g32_draw(fb,count,w,h,w-1,packet,128));
 assert(!g32_draw(fb,count-1,w,h,w,packet,128)); assert(!g32_draw(fb,count,w,h,w,packet,127));
 assert(!g32_draw((uint32_t*)((uint8_t*)fb+1),count,w,h,w,packet,128));
 packet[0]^=1; assert(!g32_draw(fb,count,w,h,w,packet,128)); packet[0]^=1;
 for(size_t i=0;i<count+2;i++) assert(allocation[i]==0xa5a5a5a5u);
 memcpy(fb,packet,128); assert(!g32_draw(fb,count,w,h,w,(uint8_t*)fb,128)); assert(!memcmp(fb,packet,128));
 free(crop); free(allocation); printf("G32_OPTICAL_HOST_PASS syntheticOnly=1 framebufferDraws=%u outsidePixelsPreserved=1 bitRefusals=%u\n",draws,bit_refusals); return 0;
}
