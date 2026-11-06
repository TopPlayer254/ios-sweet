/* SPDX-License-Identifier: GPL-2.0-only */
#include <pexpert/arm64/board_config.h>
#if !SWEET
#error "Candidate requires qualified SWEET source profile"
#endif
#include <sys/systm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <mach/vm_statistics.h>
#include <arm/machine_routines.h>
#include "ziyi_g19_xnu_owned_pages.h"
extern uint64_t kvtophys(vm_offset_t);
/* These exact private signatures are checked against captured vm/pmap.h and
 * arm/pmap/pmap.h; BSD_KERNEL_PRIVATE does not expose their declarations. */
extern boolean_t pmap_valid_page(ppnum_t);
extern unsigned int pmap_cache_attributes(ppnum_t);
extern int ziyi_hop_boot_leaf_ready(void);
extern unsigned int ziyi_hop_pmap_abi(void);
extern int ziyi_hop_pmap_page(uint64_t,uint64_t,unsigned int,unsigned int,
                              bool,uint64_t*);

static int ziyi_g19_xnu_owned_pages_policy(void *ctx,uint32_t *granule,uint32_t *nc)
{
 (void)ctx;*granule=(uint32_t)PAGE_SIZE;*nc=VM_WIMG_WCOMB;
 return ziyi_hop_boot_leaf_ready()&&PAGE_SHIFT==14&&PAGE_SIZE==HOP_PAGE_SIZE&&
        ziyi_hop_pmap_abi()==0x484f5031u;
}
static int ziyi_g19_xnu_owned_pages_evidence(void *ctx,uint64_t epoch,uint32_t *facts)
{
 struct hop_xnu *x=ctx;
 return x&&x->owner_evidence&&x->owner_evidence(x->owner_ctx,epoch,facts);
}
static int ziyi_g19_xnu_owned_pages_allocate(void *ctx,uint64_t epoch,uint32_t bytes,uint64_t *va)
{
 struct hop_xnu *x=ctx;vm_offset_t address=0;
 if(x->va||!epoch||!bytes||bytes%HOP_PAGE_SIZE)return 0;
 /* No KOBJECT/PAGEABLE/VAONLY: fresh private VM object and wired page list.
  * ZERO applies to the entire runtime pages, before NC conversion. */
 kern_return_t kr=kernel_memory_allocate(kernel_map,&address,bytes,
                    HOP_PAGE_SIZE-1u,KMA_DATA|KMA_ZERO,VM_KERN_MEMORY_IOKIT);
 if(kr!=KERN_SUCCESS)return 0;
 x->va=address;x->epoch=epoch;x->bytes=bytes;*va=address;
 return 1;
}
static int ziyi_g19_xnu_owned_pages_within(const struct hop_xnu *x,uint64_t va)
{
 return x->va&&va>=x->va&&va-x->va<x->bytes;
}
static int ziyi_g19_xnu_owned_pages_backing(void *ctx,uint64_t va,uint64_t *pa,uint64_t *aperture)
{
 struct hop_xnu *x=ctx;
 if(!ziyi_g19_xnu_owned_pages_within(x,va)||(va&(HOP_PAGE_SIZE-1u)))return 0;
 *pa=kvtophys((vm_offset_t)va);
 if(!*pa||*pa>=(1ULL<<36))return 0;
 *aperture=(uint64_t)ml_static_ptovirt(*pa);
 return *aperture!=0;
}
static int ziyi_g19_xnu_owned_pages_managed(void *ctx,uint64_t pa)
{
 (void)ctx;
 return pa&&pa<(1ULL<<36)&&!(pa&(HOP_PAGE_SIZE-1u))&&
        pmap_valid_page((ppnum_t)atop(pa));
}
static int ziyi_g19_xnu_owned_pages_aliases(void *ctx,uint64_t epoch,uint64_t va,uint64_t pa)
{
 struct hop_xnu *x=ctx;uint64_t aperture=0;
 if(x->epoch!=epoch||!ziyi_g19_xnu_owned_pages_within(x,va)||kvtophys((vm_offset_t)va)!=pa)return 0;
 unsigned int attr=pmap_cache_attributes((ppnum_t)atop(pa));
 return ziyi_hop_pmap_page(va,pa,attr,attr,false,&aperture);
}
static int ziyi_g19_xnu_owned_pages_get(void *ctx,uint64_t pa,uint32_t *attr)
{
 (void)ctx;if(!ziyi_g19_xnu_owned_pages_managed(ctx,pa))return 0;
 *attr=pmap_cache_attributes((ppnum_t)atop(pa));return 1;
}
static int ziyi_g19_xnu_owned_pages_set(void *ctx,uint64_t pa,uint32_t attr)
{
 struct hop_xnu *x=ctx;uint64_t aperture=0;
 if(!ziyi_g19_xnu_owned_pages_managed(ctx,pa))return 0;
 for(uint32_t off=0;off<x->bytes;off+=HOP_PAGE_SIZE){
  uint64_t va=x->va+off;
  if(kvtophys((vm_offset_t)va)==pa)
   return ziyi_hop_pmap_page(va,pa,pmap_cache_attributes((ppnum_t)atop(pa)),
                              attr,true,&aperture);
 }
 return 0;
}
static int ziyi_g19_xnu_owned_pages_observe(void *ctx,uint64_t va,uint64_t pa,
                    struct gh_cpu_page_observation *v)
{
 (void)ctx;return gh_cpu_observe(va,pa,v);
}
static int ziyi_g19_xnu_owned_pages_release(void *ctx,uint64_t epoch,uint64_t va,uint32_t bytes)
{
 struct hop_xnu *x=ctx;
 if(x->epoch!=epoch||x->va!=va||x->bytes!=bytes)return 0;
 kmem_free(kernel_map,(vm_offset_t)va,bytes);
 x->va=0;x->bytes=0;return 1;
}
static struct hop_ops ziyi_g19_xnu_owned_pages_ops(struct hop_xnu *x)
{
 return (struct hop_ops){x,ziyi_g19_xnu_owned_pages_policy,ziyi_g19_xnu_owned_pages_evidence,ziyi_g19_xnu_owned_pages_allocate,ziyi_g19_xnu_owned_pages_backing,ziyi_g19_xnu_owned_pages_managed,ziyi_g19_xnu_owned_pages_aliases,
                         ziyi_g19_xnu_owned_pages_get,ziyi_g19_xnu_owned_pages_set,ziyi_g19_xnu_owned_pages_observe,ziyi_g19_xnu_owned_pages_release};
}
int hop_xnu_prepare(struct hop *h,struct hop_xnu *x,uint64_t e,uint32_t n)
{ if(!x)return 0;struct hop_ops o=ziyi_g19_xnu_owned_pages_ops(x);return hop_prepare(h,&o,e,n); }
int hop_xnu_validate(struct hop *h,struct hop_xnu *x)
{ if(!x)return 0;struct hop_ops o=ziyi_g19_xnu_owned_pages_ops(x);return hop_validate(h,&o); }
int hop_xnu_mark_live(struct hop *h,struct hop_xnu *x)
{ if(!x)return 0;struct hop_ops o=ziyi_g19_xnu_owned_pages_ops(x);return hop_mark_live(h,&o); }
int hop_xnu_release(struct hop *h,struct hop_xnu *x)
{ if(!x)return 0;struct hop_ops o=ziyi_g19_xnu_owned_pages_ops(x);return hop_release(h,&o); }
