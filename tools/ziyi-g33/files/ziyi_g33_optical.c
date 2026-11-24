/* CPU optical diagnostics, not GPU rendering. ZMP1RAW1 transport follows the
 * measured u24-s2 implementation. No GPU register/command API is present. */
#include "ziyi_g33_optical.h"

uint32_t g32_crc(const void *ptr, size_t n) {
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
int g32_validate_record(const uint8_t *r,size_t n) {
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
int g32_record(uint32_t sample,unsigned worker,unsigned observer,uint64_t epoch,
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
int g32_validate_packet(const uint8_t *p,size_t n) {
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
int g32_packet(const uint8_t *r,size_t n,uint32_t tag,unsigned page,uint8_t *out,size_t cap) {
 if(!out || cap!=128 || !tag || page>=2 || !g32_validate_record(r,n) || g32_overlap(r,n,out,cap)) return 0;
 for(unsigned i=0;i<128;i++) out[i]=0;
 const char magic[]="ZMP1RAW1"; for(unsigned i=0;i<8;i++) out[i]=(uint8_t)magic[i];
 g32_put16(out+8,1); g32_put16(out+10,32); g32_put32(out+12,tag); g32_put16(out+16,(uint16_t)page);
 g32_put16(out+18,2); g32_put32(out+20,G32_BYTES); g32_put32(out+24,page*92);
 unsigned count=page?68:92; g32_put16(out+28,(uint16_t)count);
 for(unsigned i=0;i<count;i++) out[32+i]=r[page*92+i];
 g32_put32(out+124,g32_crc(out,124)); return 1;
}
int g32_draw(volatile uint32_t *fb,size_t words,unsigned width,unsigned height,
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

int g33_stamp(uint8_t*r,size_t n,uint32_t worker,uint32_t observer_error,
 uint32_t route_stage,uint32_t route_error,uint64_t table_pa) {
 if(!g32_validate_record(r,n)||!((worker>=1&&worker<=8)||(worker>=32&&worker<=36))||
    observer_error>7||route_stage>6||route_error>6||!table_pa||
    (table_pa&16383)||table_pa>=(1ull<<36))return 0;
 g32_put32(r+144,worker);g32_put32(r+148,observer_error|(route_error<<8)|(route_stage<<16));
 g32_put32(r+152,(uint32_t)table_pa);g32_put32(r+156,(uint32_t)(table_pa>>32));
 g32_put32(r+12,g32_crc(r+16,144));return g32_validate_record(r,n);
}
