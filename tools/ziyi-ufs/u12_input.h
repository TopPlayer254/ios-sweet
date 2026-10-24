/* ziyi input wire policy. Register facts: pinned Xiaomi ziyi-s-oss drivers
 * and actual runtime DT; TCM framing: Synaptics' msm-modules/touch driver. */
#ifndef ZIYI_U12_INPUT_H
#define ZIYI_U12_INPUT_H
#include <stdint.h>
#include <stddef.h>
#define ZI_CONTACTS 10u
struct zi_contact { uint32_t x,y; uint8_t down; };
/* First SPI handoff profile accepts a directly measurable TCXO root only.
 * Unknown PLL/MND parents need a separate clock qualification. Rate is an
 * upper bound (assume oversampling1); it cannot exceed the DT5MHz limit. */
static inline uint32_t zi_spi_rate(uint32_t root_cmd,uint32_t root_cfg,uint32_t sel,uint32_t clk) {
 if((root_cmd&0x80000001u)||(root_cfg&0x3700u)||sel||!(clk&1))return 0;
 unsigned hid=root_cfg&31,div=(clk>>4)&4095;if(!div)return 0;
 uint64_t root=hid?(38400000ull+hid)/(hid+1):19200000ull;
 uint32_t hz=(uint32_t)((root+div-1)/div);
 return hz>=1000000&&hz<=5000000?hz:0;
}
static inline int zi_spmi_layout(uint32_t version,uint32_t *map,uint32_t *step) {
 if(version>=0x50000000u&&version<0x70000000u){*map=0x900;*step=0x80;return 0;}
 if(version>=0x70000000u&&version<0x80000000u){*map=0x2000;*step=0x20;return 0;}
 return 1;
}
static inline int zi_key_state(unsigned k,uint8_t pon,uint8_t gpio) {
 return k==0?!!(pon&0x80):(k==2?!!(pon&0x40):!(gpio&1));
}
/* Returns events only after two consecutive valid observations. Failed reads
 * release held keys after250ms; recovery must debounce again. */
struct zi_key { uint8_t state,candidate,count;uint16_t misses; };
static inline int zi_key_update(struct zi_key *k,int valid,int down) {
 if(!valid){k->count=0;if(k->misses<65535)k->misses++;if(k->misses>=13&&k->state){k->state=0;return -1;}return 0;}
 k->misses=0;
 if(k->candidate!=(uint8_t)down){k->candidate=(uint8_t)down;k->count=1;return 0;}
 if(k->count<2)k->count++;
 if(k->count==2&&k->state!=(uint8_t)down){k->state=(uint8_t)down;return down?1:-1;}
 return 0;
}
static inline int zi_tcm_header(const uint8_t *p,size_t bytes,uint8_t *code,uint16_t *length) {
 if(bytes<4||p[0]!=0xa5)return 1;
 *code=p[1];*length=(uint16_t)(p[2]|((uint16_t)p[3]<<8));
 return *length>4096?2:0;
}
static inline int zi_bits(const uint8_t *p,size_t n,uint32_t *at,unsigned bits,uint32_t *v) {
 if(!bits||bits>32||*at>(uint64_t)n*8||bits>(uint64_t)n*8-*at)return 1;
 *v=0;for(unsigned i=0;i<bits;i++)*v|=(uint32_t)((p[(*at+i)/8]>>((*at+i)%8))&1)<<i;
 *at+=bits;return 0;
}
/* Interpret the controller's GET_TOUCH_REPORT_CONFIG. No guessed report
 * layout, out-of-range contact, nested loop or unbounded program is accepted.
 * Output is transactional: malformed reports never partially update HID. */
static inline int zi_tcm_report(const uint8_t *cfg,size_t cn,const uint8_t *p,size_t n,
 struct zi_contact out[ZI_CONTACTS]) {
 struct zi_contact tmp[ZI_CONTACTS]={{0,0,0}};uint32_t at=0,obj=0,active=0;
 size_t pc=0,loop=0;unsigned loops=0,limit=0,have_active=0,seen=0,ended=0,xs=0,ys=0;
 if(!cfg||!p||!out||cn>512||n>4096)return 1;
 for(unsigned steps=0;steps<4096&&pc<cn;steps++) {
  unsigned op=cfg[pc++];uint32_t v=0;
  if(op==0){if(loop)return 2;ended=1;break;}
  if(op==1||op==2) {
   if(loop)return 3;
   /* Active-object loops require the explicit count. Payload padding is not
    * evidence of another finger. The caller can request a known RAM config. */
   if(op==1&&!have_active)return 4;
   limit=op==1?active:ZI_CONTACTS;loops=0;obj=0;loop=pc;
   if(!limit){while(pc<cn&&cfg[pc]!=3){unsigned q=cfg[pc++];if(q==1||q==2||q==0)return 5;if(q!=4){if(pc==cn)return 5;pc++;}}if(pc==cn)return 5;pc++;loop=0;}
   continue;
  }
  if(op==3){if(!loop)return 6;if(++loops<limit){obj=loops;pc=loop;}else loop=0;continue;}
  if(op==4){at=(at+7)&~7u;if(at>(uint64_t)n*8)return 7;continue;}
  if(pc>=cn)return 8;
  unsigned bits=cfg[pc++];if(zi_bits(p,n,&at,bits,&v))return 9;
  if(op==24){if(v>ZI_CONTACTS||loop)return 10;active=v;have_active=1;}
  else if(op==6){if(!loop||v>=ZI_CONTACTS)return 11;obj=v;}
  else if(op>=7&&op<=9){
   if(!loop||obj>=ZI_CONTACTS)return 12;
   if(op==7){if(seen&(1u<<obj))return 13;seen|=1u<<obj;if(v>2)return 14;tmp[obj].down=v!=0;}
   else if(op==8){if(v>1079)return 15;tmp[obj].x=v;xs|=1u<<obj;}
   else {if(v>2399)return 16;tmp[obj].y=v;ys|=1u<<obj;}
  }else if(!((op>=5&&op<=25)||(op>=0x80&&op<=0x82)))return 17;
 }
 if(!ended||loop)return 18;
 for(unsigned i=0;i<ZI_CONTACTS;i++)if(tmp[i].down&&(!(xs&(1u<<i))||!(ys&(1u<<i))))return 19;
 for(unsigned i=0;i<ZI_CONTACTS;i++)out[i]=tmp[i];
 return 0;
}
#endif
