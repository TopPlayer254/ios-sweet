/* Candidate addition at END of captured osfmk/arm/pmap/pmap.c.
 * Uses that file's private PVH primitives; never compile as a separate TU.
 * This is not an ownership oracle: the caller must possess a fresh private
 * wired kmem allocation and must not export/create aliases of that allocation.
 * Under that construction, require precisely its one tracked PV PTE plus the
 * permanent physical aperture, then convert both through the existing updater.
 * Deliberately rejects PV lists, IOMMU aliases, hints, PPL and other granules.
 * Non-PPL SWEET lacks __ARM_PTE_PHYSMAP__: its stock updater omits physmap.
 * This addition explicitly updates the verified physical leaf in that case.
 */
/* G20 CPU-only pmap diagnostic. No MMIO or memory-attribute authority. */
#ifndef ZIYI_G20_TRACE_H
#define ZIYI_G20_TRACE_H
#include <stdint.h>
#include <stddef.h>
#define G20_TRACE_BYTES 192u
#define G20_TRACE_VALUES 20u
enum g20_value {G20_VA,G20_PA,G20_APERTURE,G20_KERNEL_PMAP,G20_OWNED_PTR,
 G20_PHYSICAL_PTR,G20_TRACKED_PTR,G20_OWNER_PMAP,G20_OWNER_VA,G20_OWNED_PTE,
 G20_PHYSICAL_PTE,G20_OWNED_PA,G20_PHYSICAL_PA,G20_OLD_WIMG,G20_NEW_WIMG,
 G20_CONVERT,G20_OWNED_TYPE,G20_PHYSICAL_TYPE,G20_IOMMU,G20_HINT_MASK};
enum g20_reason {G20_BAD_IOMMU=1,G20_BAD_PMAP=2,G20_BAD_VA=4,
 G20_BAD_OWNED_TYPE=8,G20_BAD_PHYSICAL_TYPE=16,G20_BAD_OWNED_PA=32,
 G20_BAD_PHYSICAL_PA=64,G20_BAD_HINT=128};
struct g20_pmap_trace {uint32_t call,guard,valid,reason;uint64_t value[20];};
_Static_assert(sizeof(struct g20_pmap_trace)==176,"G20 in-memory trace");
/* Only the existing serialized HOP worker may write; read after its return.
 * Snapshot publication uses atomic fields and refuses concurrent/partial reads.
 * First refusal is latched; later cleanup cannot replace it with success. */
int ziyi_hop_pmap_trace(struct g20_pmap_trace *);
int g20_trace_encode(const struct g20_pmap_trace *,uint8_t[G20_TRACE_BYTES]);
int g20_trace_decode(const uint8_t *,size_t,struct g20_pmap_trace *);

#endif


/* CPU-only trace: one serialized private HOP writer, first refusal retained. */
static struct g20_pmap_trace g20_slot;
static uint32_t g20_sequence,g20_latched,g20_call;
static void g20_observe(struct g20_pmap_trace*t,unsigned i,uint64_t v){t->value[i]=v;t->valid|=1u<<i;}
static void g20_publish(struct g20_pmap_trace*t){
 if(g20_latched)return;
 uint32_t q=__atomic_load_n(&g20_sequence,__ATOMIC_RELAXED);
 __atomic_store_n(&g20_sequence,q+1,__ATOMIC_RELEASE);
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
 __atomic_store_n(&g20_slot.call,t->call,__ATOMIC_RELAXED);
 __atomic_store_n(&g20_slot.guard,t->guard,__ATOMIC_RELAXED);
 __atomic_store_n(&g20_slot.valid,t->valid,__ATOMIC_RELAXED);
 __atomic_store_n(&g20_slot.reason,t->reason,__ATOMIC_RELAXED);
 for(unsigned i=0;i<20;i++)__atomic_store_n(&g20_slot.value[i],t->value[i],__ATOMIC_RELAXED);
 if(t->guard)g20_latched=1;
 __atomic_store_n(&g20_sequence,q+2,__ATOMIC_RELEASE);
}
int ziyi_hop_pmap_trace(struct g20_pmap_trace*out){
 if(!out)return 0;
 uint32_t a=__atomic_load_n(&g20_sequence,__ATOMIC_ACQUIRE);if(!a||(a&1))return 0;
 struct g20_pmap_trace t={0};
 t.call=__atomic_load_n(&g20_slot.call,__ATOMIC_RELAXED);t.guard=__atomic_load_n(&g20_slot.guard,__ATOMIC_RELAXED);
 t.valid=__atomic_load_n(&g20_slot.valid,__ATOMIC_RELAXED);t.reason=__atomic_load_n(&g20_slot.reason,__ATOMIC_RELAXED);
 for(unsigned i=0;i<20;i++)t.value[i]=__atomic_load_n(&g20_slot.value[i],__ATOMIC_RELAXED);
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
 if(a!=__atomic_load_n(&g20_sequence,__ATOMIC_ACQUIRE))return 0;*out=t;return 1;
}

#define HOP_PMAP_ABI 0x484f5031u
unsigned int ziyi_hop_pmap_abi(void);
unsigned int ziyi_hop_pmap_last_error(void);
static unsigned int hop_last_error;
unsigned int ziyi_hop_pmap_last_error(void) { return hop_last_error; }
int ziyi_hop_pmap_page(uint64_t, uint64_t, unsigned int, unsigned int,
                      bool, uint64_t *);

#if !XNU_MONITOR
/* pmap_pte()/pmap_ttne() panics on an inherited block with MACH_ASSERT.
 * This read-only walk refuses blocks at every ancestor without demotion. */
static pt_entry_t *
hop_pmap_leaf_no_demote(vm_map_address_t va)
{
 const pt_attr_t *attr = pmap_get_pt_attr(kernel_pmap);
 if (va < kernel_pmap->min || va >= kernel_pmap->max ||
     attr->pta_max_level != PMAP_TT_L3_LEVEL) return NULL;
 tt_entry_t *table = kernel_pmap->tte;
 for (unsigned int level=attr->pta_root_level; level<PMAP_TT_L3_LEVEL; level++) {
  tt_entry_t entry = table[ttn_index(attr,va,level)];
  if ((entry & (ARM_TTE_TYPE_MASK|ARM_TTE_VALID)) !=
      (ARM_TTE_TYPE_TABLE|ARM_TTE_VALID)) return NULL;
  table = (tt_entry_t*)phystokv(entry & ARM_TTE_TABLE_MASK);
 }
 return (pt_entry_t*)&table[ttn_index(attr,va,PMAP_TT_L3_LEVEL)];
}
/* Reclaimed BOOT/static pages can retain their original linear image alias.
 * Inspect it without a pmap_pte block panic. Refuse an extra alias instead
 * of silently converting only the allocator VA and physical aperture. A
 * mapped original VA targeting a DIFFERENT PA is not an alias of this page.
 * This is source-scoped to non-PPL SWEET's linear kernel-image mapping. */
static int
hop_original_alias(vm_map_address_t va, uint64_t pa)
{
 const pt_attr_t *attr = pmap_get_pt_attr(kernel_pmap);
 if (va < kernel_pmap->min || va >= kernel_pmap->max) return 0;
 tt_entry_t *table = kernel_pmap->tte;
 for (unsigned int level=attr->pta_root_level; level<=PMAP_TT_L3_LEVEL; level++) {
  tt_entry_t entry = table[ttn_index(attr,va,level)];
  if (!(entry & ARM_TTE_VALID)) return 0;
  if (level==PMAP_TT_L3_LEVEL)
   return (entry & ARM_PTE_TYPE_MASK)==ARM_PTE_TYPE_VALID ? pte_to_pa(entry)==pa : -1;
  if ((entry & ARM_TTE_TYPE_MASK)==ARM_TTE_TYPE_TABLE) {
   table=(tt_entry_t*)phystokv(entry & ARM_TTE_TABLE_MASK);
  } else {
   const uint64_t off=attr->pta_level_info[level].offmask;
   return ((pte_to_pa(entry)&~off)|(va&off))==pa;
  }
 }
 return -1;
}
#endif

unsigned int
ziyi_hop_pmap_abi(void)
{
#if XNU_MONITOR
 return 0;
#else
 return PAGE_SIZE == 16384 && PAGE_RATIO == 1 ? HOP_PMAP_ABI : 0;
#endif
}

int
ziyi_hop_pmap_page(uint64_t allocation_va, uint64_t pa, unsigned int old_wimg,
                   unsigned int new_wimg, bool convert, uint64_t *aperture)
{
 struct g20_pmap_trace trace={0};
 if(g20_call!=UINT32_MAX)g20_call++;trace.call=g20_call;
 g20_observe(&trace,G20_VA,allocation_va);g20_observe(&trace,G20_PA,pa);
 g20_observe(&trace,G20_OLD_WIMG,old_wimg);g20_observe(&trace,G20_NEW_WIMG,new_wimg);g20_observe(&trace,G20_CONVERT,convert);
 hop_last_error=1;
#if XNU_MONITOR
 (void)allocation_va;(void)pa;(void)old_wimg;(void)new_wimg;
 (void)convert;(void)aperture;
 trace.guard=hop_last_error;g20_publish(&trace);return 0;
#else
 if (ziyi_hop_pmap_abi() != HOP_PMAP_ABI || aperture == NULL || pa == 0 ||
     pa >= (1ULL << 36) || (pa & PAGE_MASK) ||
     allocation_va == 0 || (allocation_va & PAGE_MASK) || !pa_valid(pa) ||
     (old_wimg != VM_WIMG_DEFAULT && old_wimg != VM_WIMG_WCOMB) ||
     (new_wimg != VM_WIMG_DEFAULT && new_wimg != VM_WIMG_WCOMB)) {
  trace.guard=hop_last_error;g20_publish(&trace);return 0;
 }
 unsigned int pai = pa_index(pa);
 unsigned int observed;
 bool ok = false;
 vm_offset_t physical_va = phystokv(pa);
 g20_observe(&trace,G20_APERTURE,physical_va);g20_observe(&trace,G20_KERNEL_PMAP,(uint64_t)(uintptr_t)kernel_pmap);
 if (physical_va == allocation_va){trace.guard=hop_last_error;g20_publish(&trace);return 0;}
 pvh_lock(pai);
 pv_entry_t **head = pai_to_pvh(pai);
 pt_entry_t *owned = hop_pmap_leaf_no_demote(allocation_va);
 pt_entry_t *physical = hop_pmap_leaf_no_demote(physical_va);
 hop_last_error=2;
 /* PVH_TYPE_PTEP is the singleton representation; refusing PVEP is safe
  * even if its list happens to contain one PTE. No new alias is tolerated. */
 g20_observe(&trace,G20_OWNED_PTR,(uint64_t)(uintptr_t)owned);g20_observe(&trace,G20_PHYSICAL_PTR,(uint64_t)(uintptr_t)physical);
 if(owned==NULL||physical==NULL||!pvh_test_type(head,PVH_TYPE_PTEP))goto out;
 pt_entry_t*tracked=pvh_ptep(head);g20_observe(&trace,G20_TRACKED_PTR,(uint64_t)(uintptr_t)tracked);if(tracked!=owned)goto out;
 hop_last_error=3;
 /* Raw CPU leaf reads only after both non-demoting walks and singleton PV. */
 g20_observe(&trace,G20_OWNED_PTE,*owned);g20_observe(&trace,G20_PHYSICAL_PTE,*physical);
 g20_observe(&trace,G20_OWNED_PA,pte_to_pa(*owned));g20_observe(&trace,G20_PHYSICAL_PA,pte_to_pa(*physical));
 g20_observe(&trace,G20_OWNED_TYPE,*owned&ARM_PTE_TYPE_MASK);g20_observe(&trace,G20_PHYSICAL_TYPE,*physical&ARM_PTE_TYPE_MASK);
 g20_observe(&trace,G20_HINT_MASK,ARM_PTE_HINT_MASK);
#ifdef PVH_FLAG_IOMMU
 g20_observe(&trace,G20_IOMMU,pvh_ptep_is_iommu(owned));
 if(trace.value[G20_IOMMU]){trace.reason=G20_BAD_IOMMU;goto out;}
#else
 g20_observe(&trace,G20_IOMMU,0);
#endif
 g20_observe(&trace,G20_OWNER_PMAP,(uint64_t)(uintptr_t)ptep_get_pmap(owned));
 if(trace.value[G20_OWNER_PMAP]!=(uint64_t)(uintptr_t)kernel_pmap){trace.reason=G20_BAD_PMAP;goto out;}
 g20_observe(&trace,G20_OWNER_VA,ptep_get_va(owned));
 if(trace.value[G20_OWNER_VA]!=allocation_va){trace.reason=G20_BAD_VA;goto out;}
 if(trace.value[G20_OWNED_TYPE]!=ARM_PTE_TYPE_VALID){trace.reason=G20_BAD_OWNED_TYPE;goto out;}
 if(trace.value[G20_PHYSICAL_TYPE]!=ARM_PTE_TYPE_VALID){trace.reason=G20_BAD_PHYSICAL_TYPE;goto out;}
 if(trace.value[G20_OWNED_PA]!=pa){trace.reason=G20_BAD_OWNED_PA;goto out;}
 if(trace.value[G20_PHYSICAL_PA]!=pa){trace.reason=G20_BAD_PHYSICAL_PA;goto out;}
 if((trace.value[G20_OWNED_PTE]|trace.value[G20_PHYSICAL_PTE])&ARM_PTE_HINT_MASK){trace.reason=G20_BAD_HINT;goto out;}
 /* The original BOOT/PRELINK alias is untracked by the PV list. Never
  * normalize or ignore it. An allocator-selected such page is a precise
  * diagnostic refusal; no random-PA retry or ownership fabrication. */
 hop_last_error=4;
 if (pa < gPhysBase || gVirtBase > UINT64_MAX-(pa-gPhysBase)) goto out;
 const uint64_t original_va=gVirtBase+(pa-gPhysBase);
 if (original_va!=physical_va && original_va!=allocation_va &&
     hop_original_alias(original_va,pa)!=0) goto out;
 hop_last_error=5;
 observed = pp_attr_table[pai] & PP_ATTR_WIMG_MASK;
 if (observed == 0) observed = VM_WIMG_DEFAULT;
 if (observed != old_wimg ||
     (*owned & (ARM_PTE_ATTRINDXMASK | ARM_PTE_SHMASK)) !=
     (wimg_to_pte(old_wimg, pa) & (ARM_PTE_ATTRINDXMASK | ARM_PTE_SHMASK)) ||
     (*physical & (ARM_PTE_ATTRINDXMASK | ARM_PTE_SHMASK)) !=
     (wimg_to_pte(old_wimg, pa) & (ARM_PTE_ATTRINDXMASK | ARM_PTE_SHMASK)))
  goto out;
 if (convert && new_wimg != old_wimg) {
  pp_attr_t before, after;
  do {
   before = pp_attr_table[pai];
   after = (before & ~PP_ATTR_WIMG_MASK) | PP_ATTR_WIMG(new_wimg);
  } while (!OSCompareAndSwap16(before, after, &pp_attr_table[pai]));
  /* Non-PPL SWEET's stock updater has no physical-aperture branch. Mirror
   * its existing PTE+TLBI sequence explicitly, after refusing block/hint
   * mappings above. No demotion, raw-reserved PTE replay or Device attrs. */
#if !__ARM_PTE_PHYSMAP__
  pt_entry_t physical_new = *physical;
  physical_new &= ~(ARM_PTE_ATTRINDXMASK|ARM_PTE_SHMASK);
  physical_new |= wimg_to_pte(new_wimg,pa);
  write_pte_strong(physical,physical_new);
  flush_mmu_tlb_region_asid_async(physical_va,PAGE_SIZE,kernel_pmap,true);
#endif
  /* Existing managed-PV update and TLB synchronization. These pages cannot
   * be published or accessed until the caller's actual AT pass. */
  (void)pmap_update_cache_attributes_locked((ppnum_t)atop(pa), new_wimg, true);
#if !__ARM_PTE_PHYSMAP__
  arm64_sync_tlb(false);
#endif
 }
 *aperture = physical_va;
 hop_last_error=0;
 ok = true;
out:
 trace.guard=hop_last_error;g20_publish(&trace);
 pvh_unlock(pai);
 if (ok && convert && old_wimg != new_wimg)
  pmap_sync_wimg((ppnum_t)atop(pa), old_wimg, new_wimg);
 return ok ? 1 : 0;
#endif
}
