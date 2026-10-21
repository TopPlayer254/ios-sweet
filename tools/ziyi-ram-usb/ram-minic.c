/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Own, single-threaded AArch64 syscall runtime for the bounded RAM receiver.
 * No allocator, TLS, vDSO, clock queries, threads, dynamic loader or libc.
 * Headers supply ABI types/constants only. No external command or path RPC.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <asm/unistd.h>
#ifndef __aarch64__
#error This runtime implements the AArch64 Linux syscall ABI only
#endif

static int mini_errno;
int *__errno_location(void) { return &mini_errno; }
static long sc(long number,long a,long b,long c,long d,long e,long f) {
 register long x0 __asm__("x0")=a,x1 __asm__("x1")=b,x2 __asm__("x2")=c;
 register long x3 __asm__("x3")=d,x4 __asm__("x4")=e,x5 __asm__("x5")=f;
 register long x8 __asm__("x8")=number;
 __asm__ volatile("svc #0" : "+r"(x0) : "r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x8) : "memory","cc");
 if((unsigned long)x0>=(unsigned long)-4095){mini_errno=(int)-x0;return -1;}
 return x0;
}
#define SC(n,a,b,c,d,e,f) sc(__NR_##n,(long)(a),(long)(b),(long)(c),(long)(d),(long)(e),(long)(f))
ssize_t read(int f,void *p,size_t n) { return SC(read,f,p,n,0,0,0); }
ssize_t write(int f,const void *p,size_t n) { return SC(write,f,p,n,0,0,0); }
int close(int f) { return SC(close,f,0,0,0,0,0); }
int open(const char *p,int flags,...) {
 if(flags&O_CREAT){errno=EINVAL;return -1;}
 return SC(openat,AT_FDCWD,p,flags,0,0,0);
}
pid_t getpid(void) { return SC(getpid,0,0,0,0,0,0); }
int mkdir(const char *p,mode_t mode) { return SC(mkdirat,AT_FDCWD,p,mode,0,0,0); }
int mknod(const char *p,mode_t mode,dev_t dev) {
 if(dev>UINT32_MAX){errno=EINVAL;return -1;}
 return SC(mknodat,AT_FDCWD,p,mode,dev,0,0);
}
int chdir(const char *p) { return SC(chdir,p,0,0,0,0,0); }
int symlink(const char *target,const char *p) { return SC(symlinkat,target,AT_FDCWD,p,0,0,0); }
int dup2(int oldfd,int newfd) { return oldfd==newfd?oldfd:SC(dup3,oldfd,newfd,0,0,0,0); }
int mount(const char *source,const char *target,const char *type,unsigned long flags,const void *data) {
 return SC(mount,source,target,type,flags,data,0);
}
void *mmap(void *p,size_t n,int prot,int flags,int fd,off_t offset) { return (void *)SC(mmap,p,n,prot,flags,fd,offset); }
int munmap(void *p,size_t n) { return SC(munmap,p,n,0,0,0,0); }
int mprotect(void *p,size_t n,int prot) { return SC(mprotect,p,n,prot,0,0,0); }
unsigned int sleep(unsigned int seconds) {
 struct timespec request={(time_t)seconds,0},remaining={0,0};
 while(SC(nanosleep,&request,&remaining,0,0,0,0)<0){
  if(errno!=EINTR)return (unsigned int)request.tv_sec;
  request=remaining;
 }
 return 0;
}
int pause(void) { uint64_t mask=0;return SC(rt_sigsuspend,&mask,sizeof(mask),0,0,0,0); }
int prctl(int option,...) {
 va_list ap;va_start(ap,option);int value=va_arg(ap,int);va_end(ap);
 if(option!=PR_SET_NO_NEW_PRIVS||value!=1){errno=EINVAL;return -1;}
 return SC(prctl,option,1,0,0,0,0);
}
int tcgetattr(int fd,struct termios *t) { return SC(ioctl,fd,0x5401,t,0,0,0); }
int tcsetattr(int fd,int action,const struct termios *t) {
 if(action!=TCSANOW){errno=EINVAL;return -1;}
 return SC(ioctl,fd,0x5402,t,0,0,0);
}
void cfmakeraw(struct termios *t) {
 t->c_iflag&=~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
 t->c_oflag&=~OPOST;t->c_lflag&=~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
 t->c_cflag=(t->c_cflag&~(CSIZE|PARENB))|CS8;
 t->c_cc[VMIN]=1;t->c_cc[VTIME]=0;
}
void *memcpy(void *to,const void *from,size_t n) {
 unsigned char *d=to;const unsigned char *s=from;while(n--)*d++=*s++;return to;
}
void *memset(void *p,int value,size_t n) { unsigned char *d=p;while(n--)*d++=(unsigned char)value;return p; }
int memcmp(const void *a,const void *b,size_t n) {
 const unsigned char *x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;x++;y++;}return 0;
}
size_t strlen(const char *s) { const char *p=s;while(*p)p++;return (size_t)(p-s); }
char *strchr(const char *s,int c) { do{if(*s==(char)c)return (char *)s;}while(*s++);return NULL; }
char *strstr(const char *s,const char *needle) {
 size_t n=strlen(needle);if(!n)return (char *)s;
 for(;*s;s++){size_t i=0;while(i<n&&s[i]&&s[i]==needle[i])i++;if(i==n)return (char *)s;}
 return NULL;
}

struct mini_dir { int fd;size_t used,pos;_Alignas(8) unsigned char data[4096]; };
static struct mini_dir directory={.fd=-1};
_Static_assert(offsetof(struct dirent,d_name)==19,"getdents64 ABI");
DIR *opendir(const char *path) {
 if(directory.fd>=0){errno=EMFILE;return NULL;}
 int fd=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0)return NULL;
 directory.fd=fd;directory.used=directory.pos=0;return (DIR *)&directory;
}
struct dirent *readdir(DIR *stream) {
 if(stream!=(DIR *)&directory||directory.fd<0){errno=EINVAL;return NULL;}
 if(directory.pos==directory.used){
  long n=SC(getdents64,directory.fd,directory.data,sizeof(directory.data),0,0,0);
  if(n<=0)return NULL;directory.used=(size_t)n;directory.pos=0;
 }
 if(directory.used-directory.pos<20){errno=EIO;return NULL;}
 struct dirent *entry=(struct dirent *)(directory.data+directory.pos);
 size_t n=entry->d_reclen;if(n<20||n>directory.used-directory.pos){errno=EIO;return NULL;}
 size_t i=19;while(i<n&&directory.data[directory.pos+i])i++;
 if(i==n){errno=EIO;return NULL;}
 directory.pos+=n;return entry;
}
int closedir(DIR *stream) {
 if(stream!=(DIR *)&directory||directory.fd<0){errno=EINVAL;return -1;}
 int r=close(directory.fd);directory.fd=-1;return r;
}

/* Formats used by this receiver only: %s, %d, %u, %zu, %zd and %%. */
static void emit(char *out,size_t cap,size_t *count,char c) {
 if(*count+1<cap)out[*count]=c;(*count)++;
}
int vsnprintf(char *out,size_t cap,const char *fmt,va_list ap) {
 size_t count=0;
 while(*fmt){
  if(*fmt!='%'){emit(out,cap,&count,*fmt++);continue;}
  fmt++;int wide=0;if(*fmt=='z'){wide=1;fmt++;}
  char kind=*fmt;if(!kind)break;fmt++;
  if(kind=='s'){
   const char *s=va_arg(ap,const char *);if(!s)s="(null)";while(*s)emit(out,cap,&count,*s++);
  }else if(kind=='d'||kind=='u'){
   uint64_t value;char digits[24];size_t n=0;
   if(kind=='d'){
    int64_t v=wide?va_arg(ap,ssize_t):va_arg(ap,int);
    if(v<0){emit(out,cap,&count,'-');value=0-(uint64_t)v;}else value=(uint64_t)v;
   }else value=wide?va_arg(ap,size_t):va_arg(ap,unsigned int);
   do{digits[n++]=(char)('0'+value%10);value/=10;}while(value);
   while(n)emit(out,cap,&count,digits[--n]);
  }else if(kind=='%')emit(out,cap,&count,'%');
  else {errno=EINVAL;if(cap)out[0]=0;return -1;}
 }
 if(cap)out[count<cap?count:cap-1]=0;
 return count>INT32_MAX?-1:(int)count;
}
int snprintf(char *out,size_t cap,const char *fmt,...) {
 va_list ap;va_start(ap,fmt);int n=vsnprintf(out,cap,fmt,ap);va_end(ap);return n;
}
static int write_all(int fd,const char *p,size_t n) {
 while(n){ssize_t r=write(fd,p,n);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;p+=r;n-=(size_t)r;}return 0;
}
int dprintf(int fd,const char *fmt,...) {
 char buf[1024];va_list ap;va_start(ap,fmt);int n=vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);
 if(n<0)return -1;size_t size=(size_t)n<sizeof(buf)?(size_t)n:sizeof(buf)-1;
 return write_all(fd,buf,size)<0?-1:(int)size;
}
FILE *stdout;
int puts(const char *s) { return write_all(1,s,strlen(s))||write_all(1,"\n",1)?-1:0; }
int fflush(FILE *stream) { (void)stream;return 0; }
