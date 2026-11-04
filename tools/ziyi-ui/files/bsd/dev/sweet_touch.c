/* U23: bounded GPIO SPI, native TCM identity and stock HID. No DMA. */
#if defined(ARM64_BOARD_CONFIG_SWEET)
#include <sys/param.h>
#include <sys/systm.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include "ziyi_u12_input.h"
#include "ziyi_u23_native_touch.h"
#include "ziyi_u18_gpio_spi.h"
#include "ziyi_u21_power.h"
#include "ziyi_u14_gpi.h"
extern vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
extern int sweet_hid_touch_frame(const uint32_t*,const uint32_t*,uint32_t);
_Static_assert(ZI_CONTACTS==10,"HID full-frame ABI");
extern kern_return_t thread_terminate(thread_t);
void sweet_touch_init(void);
static vm_offset_t zt_se,zt_gpio,zt_gpi,zt_db,zt_rsc;
static unsigned zt_dead;
static uint16_t zt_maxwrite;
static uint32_t zt_max_x,zt_max_y;
static uint8_t zt_message[4100],zt_config[512],zt_oldconfig[512];
static size_t zt_config_len,zt_oldconfig_len;
static struct zi_contact zt_previous[ZI_CONTACTS];
static uint32_t zt_rd(vm_offset_t b,uint32_t o) {
 uint32_t v=*(volatile uint32_t *)(b+o);__asm__ volatile("dmb oshld":::"memory");return v;
}
static uint32_t zt_board_read(void *ctx,unsigned b,uint32_t o) {
 (void)ctx;return zt_rd(b==BB_SE?zt_se:zt_gpio,o);
}
static void zt_board_write(void *ctx,unsigned b,uint32_t o,uint32_t v) {
 (void)ctx;vm_offset_t base=b==BB_SE?zt_se:zt_gpio;
 __asm__ volatile("dmb oshst":::"memory");*(volatile uint32_t *)(base+o)=v;__asm__ volatile("dsb osh":::"memory");
}
static void zt_board_wait(void *ctx,unsigned us){(void)ctx;delay(us);}
static uint32_t zt_gpi_read(void *ctx,uint32_t o){(void)ctx;return zt_rd(zt_gpi,o);}
static int zt_gpi_quiet(void *ctx) {
 (void)ctx;if(!zt_gpi)return 0;
 struct zg_io io={NULL,zt_gpi_read,zt_board_wait};struct zg_snapshot s;
 int e=zg_quiet(&io,&s);printf("ZIYI U23 GPI quiet=%d signature=%x\n",e,s.signature);return !e;
}
static struct bb_state zt_board;
static struct bb_io zt_board_io={NULL,zt_board_read,zt_board_write,zt_board_wait,zt_gpi_quiet};
static void zt_sleep(unsigned ms) {
 static int chan;uint64_t end;clock_interval_to_deadline(ms,NSEC_PER_MSEC,&end);
 (void)assert_wait_deadline(&chan,THREAD_UNINT,end);thread_block(THREAD_CONTINUE_NULL);
}
static uint32_t zt_power_read(void *ctx,unsigned b,uint32_t o) {
 (void)ctx;if((o&3)||(b==ZP_DB?o>=0x20000:o>=0x10000))return ~0u;
 return zt_rd(b==ZP_DB?zt_db:zt_rsc,o);
}
static void zt_power_write(void *ctx,unsigned b,uint32_t o,uint32_t v) {
 (void)ctx;if(b!=ZP_RSC)return;
 int allowed=(o==0xd08&&v==2)||
  (o==ZP_SLOT+0x14&&(v==0||v==0x10000||v==0x1010000))||
  (o==ZP_SLOT+0x1c&&v<=1)||(o==ZP_SLOT+0x30&&(v==0||v==0x10108))||
  (o==ZP_SLOT+0x34&&(v==0||v==0x42100||v==0x42104||v==0x42108||v==0x41300||v==0x41304||v==0x41308))||
  (o==ZP_SLOT+0x38&&(v==0||v==1||v==7||v==1800||v==2800));
 if(!allowed)return;
 __asm__ volatile("dmb oshst":::"memory");*(volatile uint32_t *)(zt_rsc+o)=v;__asm__ volatile("dsb osh":::"memory");
}
static struct zp_state zt_power;
static struct zp_io zt_power_io={NULL,zt_power_read,zt_power_write,zt_board_wait};
static int zt_transfer(const uint8_t *tx,uint8_t *rx,size_t bytes) {
 int e=bb_transfer(&zt_board_io,&zt_board,tx,rx,bytes);
 if(e){zt_dead=zt_board.poison;printf("ZIYI U23 GPIO SPI STOP transfer=%d poison=%u\n",e,zt_dead);}
 return e;
}
static int zt_read_message(uint8_t *code,uint16_t *length) {
 uint8_t h[4];int e=zt_transfer(NULL,h,4);if(e)return e;
 e=zi_tcm_header(h,4,code,length);if(e){static unsigned bad; if(bad++<4)printf("ZIYI U23 TCM header %x %x %x %x\n",h[0],h[1],h[2],h[3]);return 4;}
 if(!*length)return 0;
 if(zt_transfer(NULL,zt_message,(size_t)*length+3))return 5;
 if(zt_message[0]!=0xa5||zt_message[1]!=3||zt_message[*length+2]!=0x5a)return 6;
 memmove(zt_message,zt_message+2,*length);return 0;
}
static int zt_command(uint8_t op,const uint8_t *body,size_t n,uint16_t *reply) {
 /* Exactly identify/app-info/config-read/volatile-report-config/enable-report.
  * Flash, firmware upload, reset and charger commands cannot be sent. */
 if(!((op==2&&n==0)||(op==0x20&&n==0)||(op==0x25&&n==0)||
      (op==0x26&&n&&n<=512)||(op==5&&n==1&&body&&body[0]==0x11)))return 1;
 if(zt_maxwrite&&n+3>zt_maxwrite)return 1;
 uint8_t command[515];command[0]=op;command[1]=(uint8_t)n;command[2]=(uint8_t)(n>>8);
 if(n)memcpy(command+3,body,n);
 int transfer_error=zt_transfer(command,NULL,n+3);
 if(transfer_error){printf("ZIYI U23 TCM op=%x write_error=%d\n",op,transfer_error);return 2;}
 for(unsigned i=0;i<100;i++) {
  uint8_t code;uint16_t length;zt_sleep(5);
  int read_error=zt_read_message(&code,&length);
  if(read_error){printf("ZIYI U23 TCM op=%x read_error=%d poll=%u\n",op,read_error,i);return 3;}
  if(code==1){*reply=length;return 0;}
  if(code==0||code==2||code==0x11)continue;
  if(code==0xfe){printf("ZIYI U23 TCM STOP host firmware download required\n");return 4;}
  if(code==0x10&&op==2&&length>=24){*reply=length;return 0;}
  if(code<0x10){printf("ZIYI U23 TCM command %x status=%x\n",op,code);return 5;}
 }
 return 6;
}
static int zt_send(const struct zi_contact *c) {
 uint32_t xs[ZI_CONTACTS],ys[ZI_CONTACTS],mask=0;
 for(unsigned i=0;i<ZI_CONTACTS;i++){xs[i]=c[i].x;ys[i]=c[i].y;if(c[i].down)mask|=1u<<i;}
 return sweet_hid_touch_frame(xs,ys,mask);
}
static void zt_release(void) {
 struct zi_contact released[ZI_CONTACTS];
 for(unsigned i=0;i<ZI_CONTACTS;i++){released[i]=zt_previous[i];released[i].down=0;}
 if(zt_send(released))for(unsigned i=0;i<ZI_CONTACTS;i++)zt_previous[i].down=0;
}
static int zt_setup(void) {
 printf("ZIYI U23 GPIO SPI fifoDisable=%x fw=%x state=%x mode0 MISO0 MOSI1 CLK2 CS3\n",zt_rd(zt_se,0x64),zt_rd(zt_se,0x68),zt_rd(zt_se,0x40));
 if(zt_rd(zt_se,0x68)!=0x102 || !(zt_rd(zt_se,0x64)&1))return 11;
 if(bb_idle(&zt_board_io))return 60;
 for(unsigned i=0;i<4;i++){uint32_t c=zt_rd(zt_gpio,i*0x1000);if(((c>>2)&15)>1||(c&0x200))return 61;}
 if((zt_rd(zt_gpio,126*0x1000)&0x23c)||(zt_rd(zt_gpio,51*0x1000)&0x23c)||
    (zt_rd(zt_gpio,126*0x1000+4)&1))return 62;
 int power=zp_start(&zt_power_io,&zt_power);
 printf("ZIYI U23 RPMh power=%d qualified=%u steps=%u poison=%u IRQ=%x/%x\n",power,zt_power.qualified,zt_power.steps,zt_power.poison,zt_rd(zt_rsc,0xd00),zt_rd(zt_rsc,0xd04));
 printf("ZIYI U23 RPMh phase=%u error=%u polls=%u fresh=%u before=%x\n",zt_power.phase,zt_power.error,zt_power.polls,zt_power.fresh,zt_power.before);
 printf("ZIYI U23 RPMh raw ctl=%x idle=%x en=%x cmd=%x resp=%x irq=%x addr=%x data=%x\n",zt_power.control,zt_power.tcs_status,zt_power.enable,zt_power.command,zt_power.response_word,zt_power.irq,zt_power.address_word,zt_power.data_word);
 for(unsigned i=0;i<zt_power.steps;i++)printf("ZIYI U23 RPMh ACK %u status=%x response=%x\n",i,zt_power.status[i],zt_power.response[i]);
 if(power)return 100+power;
 int take=bb_take(&zt_board_io,&zt_board);
 printf("ZIYI U23 GPIO SPI take=%d pins=%u reset=%u IRQ=%x\n",take,zt_board.pins,zt_board.reset,zt_rd(zt_gpio,51*0x1000+4));
 if(take)return 30+take;
 uint16_t reply=0;
 int identify_error=zt_command(2,NULL,0,&reply);
 printf("ZIYI U23 TCM identify error=%d bytes=%u mode=%x\n",identify_error,reply,reply>=2?zt_message[1]:0xff);
 if(identify_error||reply<24||zt_message[1]!=1)return 15;
 zt_maxwrite=(uint16_t)(zt_message[22]|zt_message[23]<<8);
 if(zt_maxwrite<16)return 23;
 printf("ZIYI U23 TCM application build=%02x%02x%02x%02x maxwrite=%u\n",zt_message[21],zt_message[20],zt_message[19],zt_message[18],zt_message[22]|zt_message[23]<<8);
 if(zt_command(0x20,NULL,0,&reply)||reply<38||zt_message[2]||zt_message[3])return 16;
 unsigned mx=zt_message[32]|zt_message[33]<<8,my=zt_message[34]|zt_message[35]<<8,objects=zt_message[36]|zt_message[37]<<8;
 printf("ZIYI U23 TCM max=%u,%u objects=%u\n",mx,my,objects);
 if(!zi_tcm_geometry(mx,my)||!objects||objects>ZI_CONTACTS)return 17;
 zt_max_x=mx;zt_max_y=my;
 printf("ZIYI U23 TCM native max=%u,%u -> HID max=1079,2399\n",mx,my);
 if(zt_command(0x25,NULL,0,&reply)||!reply||reply>sizeof(zt_oldconfig)||reply+3u>zt_maxwrite)return 18;
 memcpy(zt_oldconfig,zt_message,reply);zt_oldconfig_len=reply;
 printf("ZIYI U23 TCM saved volatile config bytes=%u\n",reply);
 static const uint8_t config[]={24,8,1,6,4,7,4,8,16,9,16,3,0};
 if(zt_command(0x26,config,sizeof(config),&reply))return 19;
 /* Read the controller's accepted configuration back before any HID event. */
 if(zt_command(0x25,NULL,0,&reply)||reply<sizeof(config)||reply>sizeof(zt_config)||memcmp(zt_message,config,sizeof(config)))return 20;
 memcpy(zt_config,zt_message,reply);zt_config_len=reply;
 uint8_t touch=0x11;if(zt_command(5,&touch,1,&reply))return 21;
 printf("ZIYI U23 TCM READY native->HID1080x2400 contacts=%u\n",objects);return 0;
}
static void zt_thread(void *arg,wait_result_t wr) {
 (void)arg;(void)wr;int result=zt_setup();
 if(result){printf("ZIYI U23 TCM STOP setup=%d\n",result);goto out;}
 unsigned errors=0,ticks=0,age=0;
 for(;;) {
  if(!(zt_rd(zt_gpio,51*0x1000+4)&1)||++ticks%10==0) {
   uint8_t code=0;uint16_t n=0;struct zi_contact contacts[ZI_CONTACTS];
   int error=zt_read_message(&code,&n);
   if(!error&&code==0x11) {
    error=zi_tcm_report_native(zt_config,zt_config_len,zt_message,n,contacts,zt_max_x,zt_max_y);
    if(!error&&!zt_send(contacts))error=30;
    if(!error){age=0;for(unsigned i=0;i<ZI_CONTACTS;i++)zt_previous[i]=contacts[i];}
   }
   if(error){if(++errors==10)zt_release();if(errors==1||errors%100==0)printf("ZIYI U23 TCM report error=%d count=%u\n",error,errors);}
   else errors=0;
   if(zt_dead||errors>=100)break;
  }
  if(++age>=50){zt_release();age=0;}
  zt_sleep(10);
 }
out:
 zt_release();
 if(!zt_dead&&zt_oldconfig_len){uint16_t reply;int e=zt_command(0x26,zt_oldconfig,zt_oldconfig_len,&reply);printf("ZIYI U23 TCM restore config=%d\n",e);}
 int restore=bb_release(&zt_board_io,&zt_board);
 printf("ZIYI U23 TCM board restore=%d\n",restore);
 (void)thread_terminate(current_thread());thread_block(THREAD_CONTINUE_NULL);
}
void sweet_touch_init(void) {
 static int started;int enable=0;if(started)return;started=1;
 PE_parse_boot_argn("ziyi_touch",&enable,sizeof(enable));if(enable!=1)return;
 zt_se=ml_io_map(0x00980000,0x4000);
 /* Diwali PINGROUP uses TLMM+0x100000+GPIO*0x1000. */
 zt_gpio=ml_io_map(0x0f100000,0x100000);
 zt_gpi=ml_io_map(0x00900000,0x60000);
 zt_db=ml_io_map(0x80860000,0x20000);zt_rsc=ml_io_map(0x17a20000,0x10000);
 if(!zt_se||!zt_gpio||!zt_gpi||!zt_db||!zt_rsc){printf("ZIYI U23 TCM STOP map\n");return;}
 thread_t t;kern_return_t e=kernel_thread_start(zt_thread,NULL,&t);
 if(e==KERN_SUCCESS)thread_deallocate(t);else printf("ZIYI U23 TCM thread=%d\n",e);
}
#endif
