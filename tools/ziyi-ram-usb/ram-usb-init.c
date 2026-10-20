/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Own RAM-only Linux PID1. No shell, exec, block files, physical-memory access,
 * reboot or kexec RPC. The first candidate receives assets; handoff is separate.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include "sha256.h"

#define MAX_PACKET 65536U
#define MAGIC "ZYRMUSB1"
enum { HELLO=1, ECHO=2, BEGIN=3, DATA=4, END=5, INFO=6 };
enum { OK=0, BAD_FRAME=1, BAD_SESSION=2, BAD_ARGUMENT=3, BAD_STATE=4,
       BAD_HASH=5, NO_MEMORY=6 };
struct asset { uint8_t *p, expected[32]; uint64_t size, used; int sealed; };
static struct asset assets[3];
static const uint64_t limits[3]={3ULL<<30,128ULL<<20,2ULL<<20};
static uint8_t packet[MAX_PACKET], response[MAX_PACKET+64];
static int outfd;
static uint32_t le32(const void *p) { const uint8_t *q=p; return (uint32_t)q[0]|(uint32_t)q[1]<<8|(uint32_t)q[2]<<16|(uint32_t)q[3]<<24; }
static uint64_t le64(const void *p) { const uint8_t *q=p; return le32(q)|(uint64_t)le32(q+4)<<32; }
static void put32(void *p,uint32_t v) { uint8_t *q=p; for(int i=0;i<4;i++)q[i]=v>>(8*i); }
static void put64(void *p,uint64_t v) { put32(p,v); put32((uint8_t *)p+4,v>>32); }
static uint32_t crc32(const void *p,size_t n) { const uint8_t *q=p; uint32_t c=~0U; while(n--){ c^=*q++; for(int i=0;i<8;i++)c=(c>>1)^(0xedb88320U&-(c&1)); } return ~c; }
static int fullread(int fd,void *p,size_t n) { uint8_t *q=p; while(n){ ssize_t r=read(fd,q,n); if(r<0&&errno==EINTR)continue; if(r<=0)return -1; q+=r;n-=r; } return 0; }
static int fullwrite(int fd,const void *p,size_t n) { const uint8_t *q=p; while(n){ ssize_t r=write(fd,q,n); if(r<0&&errno==EINTR)continue; if(r<=0)return -1; q+=r;n-=r; } return 0; }
static int reply(uint32_t op,uint32_t seq,uint32_t status,const void *data,size_t n) {
 uint8_t h[24]; memcpy(h,MAGIC,8); put32(h+8,op|0x80000000U); put32(h+12,seq);
 put32(response,status); if(n)memcpy(response+4,data,n); put32(h+16,n+4);put32(h+20,crc32(response,n+4));
 return fullwrite(outfd,h,24)||fullwrite(outfd,response,n+4);
}
static void discard(void) { for(int i=0;i<3;i++){ if(assets[i].p)munmap(assets[i].p,assets[i].size); memset(&assets[i],0,sizeof(assets[i])); } }
static int serve(int fd) {
 uint32_t next=1; int hello=0; discard();
 for(;;){
  uint8_t h[24], extra[64]; size_t extra_n=0; uint32_t status=OK;
  if(fullread(fd,h,24))break;
  uint32_t op=le32(h+8),seq=le32(h+12),n=le32(h+16);
  if(memcmp(h,MAGIC,8)||n>MAX_PACKET||seq!=next||next==UINT32_MAX){reply(op,seq,BAD_FRAME,NULL,0);break;}
  if(fullread(fd,packet,n))break;
  if(crc32(packet,n)!=le32(h+20)){reply(op,seq,BAD_FRAME,NULL,0);break;}
  next++;
  if(!hello&&op!=HELLO)status=BAD_SESSION;
  else if(op==HELLO){
   if(hello||n!=16)status=BAD_SESSION;
   else { hello=1; memcpy(extra,packet,16);put32(extra+16,getpid());put32(extra+20,1);
    put64(extra+24,limits[0]);put64(extra+32,limits[1]);put64(extra+40,limits[2]);extra_n=48; }
  }else if(op==ECHO){ if(reply(op,seq,OK,packet,n))break;continue;
  }else if(op==BEGIN){
   uint32_t id=n>=4?le32(packet):3; uint64_t size=n>=12?le64(packet+4):0;
   if(n!=44||id>=3||size==0||size>limits[id])status=BAD_ARGUMENT;
   else if(assets[id].p)status=BAD_STATE;
   else { void *p=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED)status=NO_MEMORY;
    else {assets[id].p=p;assets[id].size=size;memcpy(assets[id].expected,packet+12,32);} }
  }else if(op==DATA){
   uint32_t id=n>=4?le32(packet):3;uint64_t offset=n>=12?le64(packet+4):0;
   if(n<=12||id>=3)status=BAD_ARGUMENT;
   else { struct asset *a=&assets[id]; uint64_t bytes=n-12;
    if(!a->p||a->sealed||offset!=a->used||offset>a->size||bytes>a->size-offset)status=BAD_STATE;
    else {memcpy(a->p+offset,packet+12,bytes);a->used+=bytes;} }
  }else if(op==END){
   uint32_t id=n==4?le32(packet):3;
   if(id>=3)status=BAD_ARGUMENT;
   else { struct asset *a=&assets[id];
    if(!a->p||a->sealed||a->used!=a->size)status=BAD_STATE;
    else { s2_sha256(a->p,a->size,extra);extra_n=32;
     if(memcmp(extra,a->expected,32))status=BAD_HASH;
     else if(mprotect(a->p,a->size,PROT_READ))status=BAD_STATE;
     else a->sealed=1;
    }
   }
  }else if(op==INFO){
   uint32_t id=n==4?le32(packet):3;
   if(id>=3)status=BAD_ARGUMENT;
   else { struct asset *a=&assets[id];put64(extra,a->size);put64(extra+8,a->used);put32(extra+16,a->sealed);extra_n=20; }
  }else status=BAD_ARGUMENT;
  if(reply(op,seq,status,extra,extra_n))break;
 }
 discard(); return 0;
}

#ifndef ZIYI_HOST_TEST
static void halt(const char *s) { dprintf(2,"ZIYI RAM USB STOP: %s errno=%d\n",s,errno); for(;;)pause(); }
static void dir(const char *p) { if(mkdir(p,0755)&&errno!=EEXIST)halt(p); }
static void value(const char *p,const char *s) { int f=open(p,O_WRONLY|O_CLOEXEC);if(f<0||fullwrite(f,s,strlen(s)))halt(p);close(f); }
static void gadget(void) {
 const char *root="/sys/kernel/config/usb_gadget/ziyi";
 dir(root);if(chdir(root))halt("gadget cwd");
 value("idVendor","0x1d6b"); value("idProduct","0x0104");value("bcdUSB","0x0200");
 dir("strings/0x409");value("strings/0x409/serialnumber","ZIYI_RAM_USB");
 value("strings/0x409/manufacturer","Ziyi bring-up");value("strings/0x409/product","RAM USB loader v1");
 dir("configs/c.1");dir("configs/c.1/strings/0x409");value("configs/c.1/strings/0x409/configuration","RAM only");
 value("configs/c.1/MaxPower","250");dir("functions/acm.usb0");
 if(symlink("../../functions/acm.usb0","configs/c.1/acm.usb0"))halt("acm link");
 char udc[256]={0};
 for(int t=0;t<120&&!udc[0];t++){ DIR *d=opendir("/sys/class/udc");struct dirent *e;
  if(d){while((e=readdir(d)))if(e->d_name[0]!='.') {if(udc[0])halt("multiple UDC");snprintf(udc,sizeof(udc),"%s",e->d_name);}closedir(d);}
  if(!udc[0])sleep(1);
 }
 if(!udc[0])halt("no UDC after 120 seconds");value("UDC",udc);chdir("/");
}
int main(void) {
 if(getpid()!=1)return 90;
 dir("/dev");if(mount("devtmpfs","/dev","devtmpfs",MS_NOSUID,"mode=0755"))halt("devtmpfs");
 int c=open("/dev/console",O_RDWR|O_NOCTTY);if(c>=0){dup2(c,0);dup2(c,1);dup2(c,2);if(c>2)close(c);}
 puts("ZIYI RAM USB PID1 v1: no storage, no shell, receive-only");fflush(stdout);
 dir("/proc");dir("/sys");if(mount("proc","/proc","proc",MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL))halt("proc");
 if(mount("sysfs","/sys","sysfs",MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL))halt("sysfs");
 if(mount("configfs","/sys/kernel/config","configfs",MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL))halt("configfs");
 gadget();
 /* Init creates no descendants and exposes no control socket or command RPC. */
 if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))halt("no_new_privs");
 for(;;){int f=open("/dev/ttyGS0",O_RDWR|O_NOCTTY);if(f<0){sleep(1);continue;}
  struct termios t;if(tcgetattr(f,&t))halt("tty termios");cfmakeraw(&t);t.c_cflag|=CLOCAL|CREAD;
  t.c_cc[VMIN]=1;t.c_cc[VTIME]=0;if(tcsetattr(f,TCSANOW,&t))halt("tty raw");
  outfd=f;serve(f);close(f);sleep(1);
 }
}
#else
int main(void) { outfd=STDOUT_FILENO;return serve(STDIN_FILENO); }
#endif
