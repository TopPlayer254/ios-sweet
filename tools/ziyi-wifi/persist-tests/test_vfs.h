/* SPDX-License-Identifier: MIT -- synthetic API surface for fault injection */
#pragma once
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifndef ENOTSUP
#define ENOTSUP 129
#endif
#define FREAD 1
#define FWRITE 2
#define O_DIRECTORY 0x10000
#define O_NOFOLLOW_ANY 0x20000000
#define O_CREAT 0x200
#define O_EXCL 0x800
#define UIO_READ 0
#define UIO_WRITE 1
#define UIO_SYSSPACE 0
#define IO_SYNC 4
#define MNT_WAIT 1
#define NULLVP NULL
#define bzero(p,n) memset(p,0,n)
typedef char*caddr_t;
typedef void*vfs_context_t;
struct vfsstatfs{char f_fstypename[16],f_mntfromname[64];};
struct test_mount{struct vfsstatfs stat;unsigned readonly;};
typedef struct test_mount*mount_t;
struct test_vnode{unsigned kind;};
typedef struct test_vnode*vnode_t;
struct test_time{int64_t tv_sec,tv_nsec;};
struct vnode_attr{uint64_t va_active,va_supported,va_data_size,va_nlink,va_fileid;unsigned va_uid,va_gid,va_mode;struct test_time va_modify_time,va_change_time;};
enum{VNODE_ATTR_va_uid=1,VNODE_ATTR_va_gid=2,VNODE_ATTR_va_mode=4,VNODE_ATTR_va_data_size=8,VNODE_ATTR_va_nlink=16,VNODE_ATTR_va_fileid=32,VNODE_ATTR_va_modify_time=64,VNODE_ATTR_va_change_time=128};
#define VATTR_INIT(a) memset(a,0,sizeof(*(a)))
#define VATTR_WANTED(a,n) ((a)->va_active|=VNODE_ATTR_##n)
static struct {
 struct test_mount mount;
 struct test_vnode root,parent,file;
 uint8_t data[ZNP_MAX];size_t bytes;
 unsigned exists,persist,creates,writes,syncs,reads,opens,closes,rootputs;
 unsigned badroot,badparent,badfile,attr_unsupported,create_race,symlink,shortwrite,shortread,writefail,syncfail,closefail,reopen_id,readcorrupt;
} f;
static vfs_context_t vfs_context_kernel(void){return &f;}
static vnode_t vfs_rootvnode(void){return &f.root;}
static mount_t vnode_mount(vnode_t v){(void)v;return &f.mount;}
static struct vfsstatfs*vfs_statfs(mount_t m){return &m->stat;}
static int vfs_isrdonly(mount_t m){return m->readonly;}
static int vnode_isreg(vnode_t v){return v->kind==3;}
static int vnode_isdir(vnode_t v){return v->kind==2;}
static void vnode_put(vnode_t v){(void)v;f.rootputs++;}
static void* vfs_context_ucred(vfs_context_t c){return c;}
static void* vfs_context_proc(vfs_context_t c){return c;}
static int PE_parse_boot_argn(const char*n,void*out,size_t size){(void)n;if(size!=sizeof(unsigned))return 0;*(unsigned*)out=f.persist;return 1;}
static int vnode_getattr(vnode_t v,struct vnode_attr*a,vfs_context_t c){
 (void)c;a->va_supported=f.attr_unsupported?0:a->va_active;a->va_uid=a->va_gid=0;
 a->va_mode=v==&f.file?0600:0700;a->va_nlink=1;a->va_fileid=v==&f.file?3:2;
 a->va_data_size=v==&f.file?f.bytes:0;
 if(v==&f.file&&f.badfile)a->va_mode=0644;
 if(v==&f.parent&&f.badparent)a->va_uid=501;
 if(v==&f.file&&f.reopen_id&&f.opens>=4)a->va_fileid=4;
 return 0;
}
static int vnode_open(const char*path,int flags,int mode,int unused,vnode_t*out,vfs_context_t c){
 (void)unused;(void)c;f.opens++;
 if(!(flags&O_NOFOLLOW_ANY))return EACCES;
 if(!strcmp(path,"/private/var/root")){if(!(flags&O_DIRECTORY))return EINVAL;*out=&f.parent;return 0;}
 if(strcmp(path,"/private/var/root/.ziyi-wifi-network"))return ENOENT;
 if(f.symlink)return ELOOP;
 if(flags&O_CREAT){
  if(f.exists||f.create_race)return EEXIST;
  if(!(flags&O_EXCL)||mode!=0600||!(flags&FWRITE))return EACCES;
  f.exists=1;f.creates++;
 }else if(!f.exists)return ENOENT;
 *out=&f.file;return 0;
}
static int vnode_close(vnode_t v,int flags,vfs_context_t c){(void)v;(void)flags;(void)c;f.closes++;return f.closefail?EIO:0;}
static int vn_rdwr(int rw,vnode_t v,caddr_t b,int n,int64_t off,int seg,int io,void*cred,int*resid,void*proc){
 (void)seg;(void)cred;(void)proc;if(v!=&f.file||off||n<0||n>(int)ZNP_MAX)return EINVAL;
 if(rw==UIO_WRITE){f.writes++;if(f.writefail)return EIO;if(io!=IO_SYNC)return EINVAL;f.bytes=(size_t)(n-(f.shortwrite?1:0));memcpy(f.data,b,f.bytes);*resid=f.shortwrite?1:0;}
 else {f.reads++;if((size_t)n>f.bytes)return EIO;memcpy(b,f.data,(size_t)n);*resid=f.shortread?1:0;if(f.readcorrupt&&f.reads==2)b[0]^=1;}
 return 0;
}
static int VNOP_FSYNC(vnode_t v,int wait,vfs_context_t c){(void)v;(void)c;f.syncs++;return wait!=MNT_WAIT||f.syncfail==f.syncs?EIO:0;}
