/* SPDX-License-Identifier: GPL-2.0-only */
#include "ziyi_g24_firmware.h"
#include "ziyi_g24_pas.h"
#include "ziyi_g7_sha256.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(G24_PUBLIC_FIXTURE)
static uint8_t g24_own_sha[3][32];
#else
#include "ziyi_g24_pins.inc"
#endif
static uint8_t blob[G24_SLOT_BYTES],work[G24_SLOT_BYTES];
struct pm {unsigned calls,refuse,interrupt,busy,waitq,broken;uint64_t epoch,session;unsigned last_op;};
static int authority(void*c,uint64_t e,unsigned op){struct pm*m=c;m->last_op=op;return e==m->epoch&&!m->refuse;}
static int smc(void*c,uint64_t r[8]){struct pm*m=c;m->calls++;unsigned op=m->last_op;
 if(r[0]==1){assert(r[6]==m->session&&r[1]==130&&r[2]==13&&r[3]==0xc8000000&&r[4]==0);r[0]=0;r[1]=0;return 1;}
 if(op==G24P_INFO){assert(r[0]==(m->calls==1?0xc2000601ULL:0x42000601ULL)&&r[1]==1&&(r[2]&0xff000000)==0x02000000);r[0]=0;r[1]=1;return 1;}
 assert(!(r[0]&0x80000000)&&r[2]==13);
 if(op==G24P_METADATA){assert(r[0]==0x42000201&&r[1]==130&&r[3]==0xc8000000&&!r[4]);if(m->interrupt){m->session=0x456789;r[6]=m->session;r[0]=1;return 1;}}
 if(op==G24P_RANGE)assert(r[0]==0x42000202&&r[1]==3&&r[3]==G24_ZAP_PA&&r[4]==4096);
 if(op==G24P_START)assert(r[0]==0x42000205&&r[1]==1&&r[3]==0&&r[4]==0);
 r[0]=m->waitq?2:m->busy?(uint64_t)(int64_t)-12:0;r[1]=m->broken?1:0;return 1;
}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(blob,sizeof(blob),1,f)==1&&fgetc(f)==EOF);assert(!fclose(f));
#if defined(G24_PUBLIC_FIXTURE)
 for(unsigned i=0;i<3;i++){unsigned at=i==0?256:i==1?43328:46896,n=i==0?43068:i==1?3564:1976;g7_sha256(blob+at,n,g24_own_sha[i]);}
#endif
struct g24_firmware fw={0};assert(g24_capsule_view(blob,sizeof(blob),g24_own_sha,&fw));assert(fw.sqe_bytes==43068&&fw.mdt_bytes==3564&&fw.code_bytes==1976&&fw.load_bytes==4096&&fw.relocation==4096&&fw.sqe_version==0x016dc109);assert(!g24_slot_view(&fw));
 unsigned bad=0;
 for(unsigned bit=0;bit<2048;bit++){memcpy(work,blob,sizeof(blob));work[bit/8]^=1u<<(bit%8);assert(!g24_capsule_view(work,sizeof(work),g24_own_sha,&fw));bad++;}
 for(unsigned n=0;n<4096;n++){memcpy(work,blob,sizeof(blob));unsigned at=256+(n*7919u)%(sizeof(work)-256);work[at]^=1u<<(n%8);assert(!g24_capsule_view(work,sizeof(work),g24_own_sha,&fw));bad++;}
 unsigned elf_controls=0;for(unsigned n=0;n<148;n++)if(n<7||(n>=16&&n<20)||(n>=28&&n<32)||(n>=40&&n<46)||n>=52){memcpy(work,blob+43328,3564);work[n]^=1;assert(!g24_mdt_plan(work,3564,1976,&fw));elf_controls++;}
 uint8_t reversed[3][32];memcpy(reversed,g24_own_sha,sizeof(reversed));memcpy(reversed[1],g24_own_sha[2],32);assert(!g24_capsule_view(blob,sizeof(blob),reversed,&fw));
 for(unsigned fault=0;fault<6;fault++){struct pm m={.epoch=77};struct g24_pas p={0};struct g24_pas_io o={&m,authority,smc};assert(g24_pas_probe(&p,77,&o)&&p.state==G24P_PROBED&&m.calls==4);if(fault==1)m.interrupt=1;if(fault==2)m.busy=1;if(fault==3)m.waitq=1;if(fault==4)m.broken=1;if(fault==5)m.refuse=1;int ok=g24_pas_init(&p,0xc8000000,&o);if(fault>=2){assert(!ok&&p.metadata_retained&&p.state==G24P_REFUSED);continue;}assert(ok&&p.metadata_retained&&p.state==G24P_INIT);assert(!g24_pas_auth(&p,&o));assert(g24_pas_memory(&p,&o)&&p.image_retained);assert(g24_pas_auth(&p,&o)&&p.state==G24P_AUTH);assert(!g24_pas_memory(&p,&o));}
 printf("G24_OWN_CAPSULE_PAS_HOST_PASS corruptions%u ELFcontrols%u PAScases6 syntheticSMC1 native0\n",bad,elf_controls);return 0;
}
