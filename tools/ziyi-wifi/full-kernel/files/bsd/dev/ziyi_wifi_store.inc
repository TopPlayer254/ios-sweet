/* SPDX-License-Identifier: MIT
 * Journaled filesystem access only; never raw UFS/HFS metadata writes.
 * Existing different or insecure files are not modified or removed.
 */
#include "ziyi_wifi_network.h"
#ifdef ZNP_HOST_TEST
#include "test_vfs.h"
#else
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>
#include <sys/uio.h>
#include <pexpert/pexpert.h>
#endif

static const char znp_parent[]="/private/var/root";
static const char znp_target[]="/private/var/root/.ziyi-wifi-network";
static uint8_t znp_network[ZNP_MAX];
static uint64_t znp_size;
static unsigned znp_state;

static int znp_attrs(vnode_t vp,struct vnode_attr*a,vfs_context_t ctx){
 VATTR_INIT(a);
 VATTR_WANTED(a,va_uid);VATTR_WANTED(a,va_gid);VATTR_WANTED(a,va_mode);
 VATTR_WANTED(a,va_data_size);VATTR_WANTED(a,va_nlink);VATTR_WANTED(a,va_fileid);
 VATTR_WANTED(a,va_modify_time);VATTR_WANTED(a,va_change_time);
 int error=vnode_getattr(vp,a,ctx);
 if(error)return error;
 if((a->va_supported&a->va_active)!=a->va_active)return ENOTSUP;
 return 0;
}
static int znp_root(vnode_t vp,mount_t root){
 if(vnode_mount(vp)!=root)return 0;
 struct vfsstatfs*s=vfs_statfs(root);
 return s&&!strcmp(s->f_fstypename,"hfs")&&
  (!strcmp(s->f_mntfromname,"/dev/ufsroot0")||!strcmp(s->f_mntfromname,"ufsroot0"));
}
static int znp_read(vnode_t vp,mount_t root,uint8_t*out,size_t*n,vfs_context_t ctx){
 struct vnode_attr a,b;
 int error=znp_attrs(vp,&a,ctx);
 if(error)return error;
 if(!znp_root(vp,root)||!vnode_isreg(vp)||a.va_uid||a.va_gid||
    (a.va_mode&07777)!=0600||a.va_nlink!=1||a.va_data_size<11||a.va_data_size>ZNP_MAX)return EINVAL;
 int residual=(int)a.va_data_size;
 error=vn_rdwr(UIO_READ,vp,(caddr_t)out,(int)a.va_data_size,0,UIO_SYSSPACE,0,
               vfs_context_ucred(ctx),&residual,vfs_context_proc(ctx));
 if(error||residual)return error?error:EIO;
 error=znp_attrs(vp,&b,ctx);
 if(error)return error;
 if(a.va_fileid!=b.va_fileid||a.va_data_size!=b.va_data_size||a.va_uid!=b.va_uid||
    a.va_gid!=b.va_gid||a.va_mode!=b.va_mode||a.va_nlink!=b.va_nlink||
    a.va_modify_time.tv_sec!=b.va_modify_time.tv_sec||a.va_modify_time.tv_nsec!=b.va_modify_time.tv_nsec||
    a.va_change_time.tv_sec!=b.va_change_time.tv_sec||a.va_change_time.tv_nsec!=b.va_change_time.tv_nsec)return EBUSY;
 if(!znp_valid(out,(size_t)a.va_data_size))return EINVAL;
 *n=(size_t)a.va_data_size;return 0;
}
static int znp_load(const uint8_t*capsule,uint64_t bytes){
 vfs_context_t ctx=vfs_context_kernel();
 vnode_t rootvp=vfs_rootvnode(),parent=NULLVP,file=NULLVP;
 int error=EINVAL,close_error,flags=FREAD;size_t count=0;
 uint8_t verify[ZNP_MAX]={0};unsigned persist=0;
 PE_parse_boot_argn("ziyi_wifi_persist",&persist,sizeof(persist));
 if(!rootvp)return ENOENT;
 mount_t root=vnode_mount(rootvp);
 if(!znp_root(rootvp,root))goto done;
 if(capsule&&!znp_valid(capsule,(size_t)bytes))goto done;
 error=vnode_open(znp_parent,FREAD|O_DIRECTORY|O_NOFOLLOW_ANY,0,0,&parent,ctx);
 if(error)goto done;
 struct vnode_attr pa;
 error=znp_attrs(parent,&pa,ctx);
 if(error)goto done;
 if(!znp_root(parent,root)||!vnode_isdir(parent)||pa.va_uid||pa.va_gid||(pa.va_mode&0022)){
  error=EPERM;goto done;
 }
 error=vnode_open(znp_target,FREAD|O_NOFOLLOW_ANY,0,0,&file,ctx);
 if(error==ENOENT){
  if(persist!=1||!capsule){error=ENOENT;goto done;}
  if(vfs_isrdonly(root)){error=EROFS;goto done;}
  flags=FREAD|FWRITE;
  error=vnode_open(znp_target,flags|O_CREAT|O_EXCL|O_NOFOLLOW_ANY,0600,0,&file,ctx);
  if(error)goto done; /* EEXIST race is never followed by a replacement. */
  struct vnode_attr fresh;
  error=znp_attrs(file,&fresh,ctx);
  if(error)goto done;
  if(!znp_root(file,root)||!vnode_isreg(file)||fresh.va_uid||fresh.va_gid||
     (fresh.va_mode&07777)!=0600||fresh.va_nlink!=1||fresh.va_data_size){error=EPERM;goto done;}
  int residual=(int)bytes;
  error=vn_rdwr(UIO_WRITE,file,(caddr_t)capsule,(int)bytes,0,UIO_SYSSPACE,IO_SYNC,
                vfs_context_ucred(ctx),&residual,vfs_context_proc(ctx));
  if(error||residual){if(!error)error=EIO;goto done;}
  error=VNOP_FSYNC(file,MNT_WAIT,ctx);if(error)goto done;
  error=VNOP_FSYNC(parent,MNT_WAIT,ctx);if(error)goto done;
 }
 if(error)goto done;
 error=znp_read(file,root,verify,&count,ctx);if(error)goto done;
 if(capsule&&(bytes!=count||memcmp(capsule,verify,count))){error=EEXIST;goto done;}
 /* Reopen the pathname after close, then read it back from the same fileid. */
 struct vnode_attr original;
 error=znp_attrs(file,&original,ctx);if(error)goto done;
 if(original.va_data_size!=count){error=EBUSY;goto done;}
 close_error=vnode_close(file,flags,ctx);file=NULLVP;flags=FREAD;
 if(close_error){error=close_error;goto done;}
 error=vnode_open(znp_target,FREAD|O_NOFOLLOW_ANY,0,0,&file,ctx);if(error)goto done;
 error=znp_read(file,root,znp_network,&count,ctx);if(error)goto done;
 struct vnode_attr reopened;
 error=znp_attrs(file,&reopened,ctx);if(error)goto done;
 if(original.va_fileid!=reopened.va_fileid||original.va_data_size!=count||
    memcmp(verify,znp_network,count)){error=EBUSY;goto done;}
 znp_size=count;
done:
 if(file){close_error=vnode_close(file,flags,ctx);if(!error)error=close_error;}
 if(parent){close_error=vnode_close(parent,FREAD,ctx);if(!error)error=close_error;}
 vnode_put(rootvp);
 bzero(verify,sizeof(verify));
 if(error){bzero(znp_network,sizeof(znp_network));znp_size=0;}
 return error;
}
static const uint8_t*znp_file(const uint8_t*capsule,uint64_t bytes,uint64_t*size){
 unsigned state=__atomic_load_n(&znp_state,__ATOMIC_ACQUIRE);
 if(!state){unsigned expect=0;if(__atomic_compare_exchange_n(&znp_state,&expect,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
   int error=znp_load(capsule,bytes);
   /* No values, byte counts, hashes or credentials are emitted. */
   printf("ZWIFI network persistence status=%d verified=%u\n",error,!error);
   state=error?3:2;__atomic_store_n(&znp_state,state,__ATOMIC_RELEASE);
  }else state=expect;}
 if(state!=2)return NULL;
 *size=znp_size;return znp_network;
}
