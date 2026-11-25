"""Add a source-bound A662 noop producer and raw submission capture.

Only Linux host emulation changes: no kernel, device or vendor firmware.
"""
from pathlib import Path
import hashlib, json, sys

source=Path(sys.argv[1]); out=Path(sys.argv[2]); out.mkdir(exist_ok=False)
p=source/'src/freedreno/drm-shim/freedreno_noop.c'
b=p.read_bytes(); s=b.decode()
assert s.count('shim_device.version_minor = 9;')==1
# Classic softpin submit contains explicit BO/IB bindings. VM_BIND is deliberately
# not used by this offline capture; it says nothing about the live XNU ABI.
s=s.replace('shim_device.version_minor = 9;', 'shim_device.version_minor = 7;')
needle='static const struct msm_device_info device_infos[] = {'
assert s.count(needle)==1
s=s.replace(needle,needle+'''
   { .gpu_id = 644, .chip_id = 0x06060201, .gmem_size = 1024 * 1024 },
''')
needle='static ioctl_fn_t driver_ioctls[] = {'
assert s.count(needle)==1
capture=r'''
/* Explicitly synthetic: serialize Mesa output, never execute any GPU command. */
static int g34_offline_submit(int fd, unsigned long request, void *arg)
{
   (void)request;
   static unsigned serial;
   const char *dir = getenv("G34_CAPTURE_DIR");
   if (!dir || serial++) abort();
   struct drm_msm_gem_submit *submit = arg;
   struct drm_msm_gem_submit_bo *bos = (void *)(uintptr_t)submit->bos;
   struct drm_msm_gem_submit_cmd *cmds = (void *)(uintptr_t)submit->cmds;
   struct shim_fd *sf = drm_shim_fd_lookup(fd);
   if (!sf || !submit->nr_bos || submit->nr_bos > 256 || !submit->nr_cmds) abort();
   char name[4096];
   snprintf(name, sizeof(name), "%s/submit.json", dir);
   FILE *j = fopen(name, "wx"); if (!j) abort();
   fprintf(j,"{\"nativeExecution\":false,\"chipId\":101057025,\"bos\":[");
   for (unsigned i=0;i<submit->nr_bos;i++) {
      struct shim_bo *bo=drm_shim_bo_lookup(sf,bos[i].handle);
      if (!bo || !bo->size || bo->size > 64*1024*1024 || !bos[i].presumed) abort();
      void *map=mmap(NULL,bo->size,PROT_READ,MAP_SHARED,shim_device.mem_fd,bo->mem_addr);
      if (map==MAP_FAILED) abort();
      snprintf(name,sizeof(name),"%s/bo-%03u.bin",dir,i);
      FILE *f=fopen(name,"wbx"); if(!f)abort();
      if(fwrite(map,1,bo->size,f)!=bo->size || fclose(f))abort();
      fprintf(j,"%s{\"index\":%u,\"handle\":%u,\"iova\":%llu,\"bytes\":%u,\"file\":\"bo-%03u.bin\"}",
         i?",":"",i,bos[i].handle,(unsigned long long)bos[i].presumed,bo->size,i);
      munmap(map,bo->size); drm_shim_bo_put(bo);
   }
   fprintf(j,"],\"commands\":[");
   for(unsigned i=0;i<submit->nr_cmds;i++) {
      if(cmds[i].nr_relocs || cmds[i].submit_idx>=submit->nr_bos || !cmds[i].size)abort();
      fprintf(j,"%s{\"boIndex\":%u,\"offset\":%u,\"bytes\":%u,\"type\":%u}",
         i?",":"",cmds[i].submit_idx,cmds[i].submit_offset,cmds[i].size,cmds[i].type);
   }
   fprintf(j,"]}\n"); if(fclose(j))abort();
   return 0;
}
'''
s=s.replace(needle,capture+'\n'+needle)
s=s.replace('#include <sys/ioctl.h>','#include <sys/ioctl.h>\n#include <sys/mman.h>')
assert s.count('[DRM_MSM_GEM_SUBMIT] = msm_ioctl_noop,')==1
s=s.replace('[DRM_MSM_GEM_SUBMIT] = msm_ioctl_noop,','[DRM_MSM_GEM_SUBMIT] = g34_offline_submit,')
(out/'freedreno_noop.original.c').write_bytes(b)
(out/'freedreno_noop.producer.c').write_text(s)
p.write_text(s)
(out/'patch.json').write_text(json.dumps(dict(originalSha256=hashlib.sha256(b).hexdigest(),
    producerSha256=hashlib.sha256(s.encode()).hexdigest(),chipId='06060201',
    firmwareDownloaded=False,nativeExecution=False),indent=2)+'\n')
print('G34_MESA_OFFLINE_PRODUCER_PATCHED; no native execution')
