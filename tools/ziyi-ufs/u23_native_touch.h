/* U23: native U22 IDENTIFY/application reply measured max10799,23999.
 * Same transactional TCM wire policy as U12, normalize discrete coordinates
 * into stock HID1080x2400 only after validating the entire report. Include
 * ziyi_u12_input.h first. No synthetic contact or arbitrary geometry. */
#ifndef ZIYI_U23_NATIVE_TOUCH_H
#define ZIYI_U23_NATIVE_TOUCH_H
static inline int zi_tcm_geometry(uint32_t x,uint32_t y) {
 return (x==1079&&y==2399)||(x==10799&&y==23999);
}
static inline int zi_tcm_report_native(const uint8_t *cfg,size_t cn,const uint8_t *p,size_t n,
 struct zi_contact out[ZI_CONTACTS],uint32_t max_x,uint32_t max_y) {
 struct zi_contact tmp[ZI_CONTACTS]={{0,0,0}};uint32_t at=0,obj=0,active=0;
 size_t pc=0,loop=0;unsigned loops=0,limit=0,have_active=0,seen=0,ended=0,xs=0,ys=0;
 if(!cfg||!p||!out||cn>512||n>4096)return 1;
 if(!zi_tcm_geometry(max_x,max_y))return 20;
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
   else if(op==8){if(v>max_x)return 15;tmp[obj].x=(uint32_t)((uint64_t)v*1080/(max_x+1u));xs|=1u<<obj;}
   else {if(v>max_y)return 16;tmp[obj].y=(uint32_t)((uint64_t)v*2400/(max_y+1u));ys|=1u<<obj;}
  }else if(!((op>=5&&op<=25)||(op>=0x80&&op<=0x82)))return 17;
 }
 if(!ended||loop)return 18;
 for(unsigned i=0;i<ZI_CONTACTS;i++)if(tmp[i].down&&(!(xs&(1u<<i))||!(ys&(1u<<i))))return 19;
 for(unsigned i=0;i<ZI_CONTACTS;i++)out[i]=tmp[i];
 return 0;
}
#endif
