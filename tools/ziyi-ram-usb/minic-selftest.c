/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include "boot-usb-log.h"
static int equal(const char *a,const char *b) { return strlen(a)==strlen(b)&&!memcmp(a,b,strlen(a)+1); }
int main(void) {
 char buf[128],tiny[4];
 int n=snprintf(buf,sizeof(buf),"%s %d %u %zu %zd %%","ok",-7,42u,(size_t)UINT64_MAX,(ssize_t)INT64_MIN);
 if(!equal(buf,"ok -7 42 18446744073709551615 -9223372036854775808 %")||n!=(int)strlen(buf))return 1;
 if(snprintf(tiny,sizeof(tiny),"%s",buf)!=n||!equal(tiny,"ok "))return 2;
 if(snprintf(NULL,0,"%zu",(size_t)123)!=3)return 3;
 if(getpid()<=0)return 4;
 if(open("/ziyi-nonexistent-selftest-directory/file",O_RDONLY)!=-1||errno!=ENOENT)return 5;
 unsigned char *p=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if(p==MAP_FAILED)return 6;
 memset(p,0x5a,4096);memcpy(p+2048,p,2048);
 if(p[0]!=0x5a||p[4095]!=0x5a||memcmp(p,p+2048,2048))return 7;
 if(mprotect(p,4096,PROT_READ)||munmap(p,4096))return 8;
 DIR *d=opendir("/proc/self/fd");if(!d)return 9;
 if(opendir("/proc/self/fd")||errno!=EMFILE)return 10;
 int entries=0;struct dirent *e;while((e=readdir(d))){if(!e->d_name[0])return 11;entries++;}
 if(closedir(d)||entries<3)return 12;
 if(!strstr("raw-runtime-entry","runtime")||strstr("raw","runtime"))return 13;
 struct ziyi_boot_usb_log usb={0};
 ziyi_boot_usb_add(&usb,"invalid");ziyi_boot_usb_add(&usb,"6,1,2,-;ZIYI RAM USB waiting");
 ziyi_boot_usb_add(&usb,"6,1,2,-;calling dwc3_init");ziyi_boot_usb_add(&usb,"6,1,2,-;initcall dwc3 returned 0");
 ziyi_boot_usb_add(&usb,"6,1,2,-;unrelated event");
 if(usb.kept||usb.seen)return 15;
 for(unsigned int i=0;i<ZIYI_LOG_SLOTS+3;i++){
  snprintf(buf,sizeof(buf),"6,1,2,-;dwc3 test %u\n",i);ziyi_boot_usb_add(&usb,buf);
 }
 if(usb.kept!=ZIYI_LOG_SLOTS||usb.seen!=ZIYI_LOG_SLOTS+3||!equal(usb.line[usb.next],"dwc3 test 3"))return 16;
 char longlog[512];memset(longlog,'x',sizeof(longlog));memcpy(longlog,"6,1,2,-;usb ",11);longlog[sizeof(longlog)-1]=0;
 unsigned int slot=usb.next;ziyi_boot_usb_add(&usb,longlog);
 if(strlen(usb.line[slot])!=ZIYI_LOG_WIDTH-1||usb.line[slot][ZIYI_LOG_WIDTH-1]!=0)return 17;
 if(dprintf(1,"OWN_MINIC_SELFTEST_PASS pid=%d\n",getpid())<0)return 14;
 return 0;
}
