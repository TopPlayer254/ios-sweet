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
 hop_last_error=1;
#if XNU_MONITOR
 (void)allocation_va;(void)pa;(void)old_wimg;(void)new_wimg;
 (void)convert;(void)aperture;
 return 0;
#else
 if (ziyi_hop_pmap_abi() != HOP_PMAP_ABI || aperture == NULL || pa == 0 ||
     pa >= (1ULL << 36) || (pa & PAGE_MASK) ||
     allocation_va == 0 || (allocation_va & PAGE_MASK) || !pa_valid(pa) ||
     (old_wimg != VM_WIMG_DEFAULT && old_wimg != VM_WIMG_WCOMB) ||
     (new_wimg != VM_WIMG_DEFAULT && new_wimg != VM_WIMG_WCOMB)) {
  return 0;
 }
 unsigned int pai = pa_index(pa);
 unsigned int observed;
 bool ok = false;
 vm_offset_t physical_va = phystokv(pa);
 if (physical_va == allocation_va) return 0;
 pvh_lock(pai);
 pv_entry_t **head = pai_to_pvh(pai);
 pt_entry_t *owned = hop_pmap_leaf_no_demote(allocation_va);
 pt_entry_t *physical = hop_pmap_leaf_no_demote(physical_va);
 hop_last_error=2;
 /* PVH_TYPE_PTEP is the singleton representation; refusing PVEP is safe
  * even if its list happens to contain one PTE. No new alias is tolerated. */
 if (owned == NULL || physical == NULL ||
     !pvh_test_type(head, PVH_TYPE_PTEP) || pvh_ptep(head) != owned) goto out;
 hop_last_error=3;
#ifdef PVH_FLAG_IOMMU
 if (pvh_ptep_is_iommu(owned)) goto out;
#endif
 if (ptep_get_pmap(owned) != kernel_pmap ||
     ptep_get_va(owned) != allocation_va ||
     (*owned & ARM_PTE_TYPE_MASK) != ARM_PTE_TYPE_VALID ||
     (*physical & ARM_PTE_TYPE_MASK) != ARM_PTE_TYPE_VALID ||
     pte_to_pa(*owned) != pa || pte_to_pa(*physical) != pa ||
     ((*owned | *physical) & ARM_PTE_HINT_MASK)) goto out;
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
 pvh_unlock(pai);
 if (ok && convert && old_wimg != new_wimg)
  pmap_sync_wimg((ppnum_t)atop(pa), old_wimg, new_wimg);
 return ok ? 1 : 0;
#endif
}
