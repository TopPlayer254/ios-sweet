/* Apple XNU source under APSL 2.0, https://opensource.apple.com/apsl/ .
 * Exact production function bodies; platform primitives are modeled. */
static void
update_or_defer_tte(tt_entry_t *ttep, tt_entry_t entry, pmap_paddr_t pa, vm_map_address_t ttebase,
    unsigned granule __unused, tt_entry_t **deferred_ttep_pair, tt_entry_t *deferred_tte_pair)
{
	/*
	 * If we're trying to assign an entry that maps the current TTE region (identified by ttebase),
	 * and the pagetable is already live (indicated by kva_active), defer assignment of the current
	 * entry and possibly the entry after it until all other mappings in the segment have been
	 * updated.  Otherwise we may end up immediately marking the pagetable region read-only
	 * leading to a fault later on a later assignment if we manage to outrun the TLB.  This can
	 * happen on KTRR/CTRR-enabled devices when marking segDATACONST read-only, as the pagetables
	 * that map that segment must come from the segment itself.  We therefore store the initial
	 * recursive TTE in deferred_ttep_pair[0] and its value in deferred_tte_pair[0].  We may also
	 * defer assignment of the TTE following that recursive TTE and store its value in
	 * deferred_tte_pair[1], because the TTE region following the current one may also contain
	 * pagetables and we must avoid marking that region read-only before updating those tables.
	 *
	 * We require that such recursive mappings must exist in regions that can be mapped with L2
	 * block entries if they are sufficiently large.  This is what allows us to assume that no
	 * more than 2 deferred TTEs will be required, because:
	 * 	--If more than 2 adjacent L3 PTEs were required to map our pagetables, that would mean
	 * 	  we would have at least one full L3 pagetable page and would instead use an L2 block.
	 *	--If more than 2 adjacent L2 blocks were required to map our pagetables, that would
	 * 	  mean we would have at least one full L2-block-sized region of TTEs and something
	 *	  is very wrong because no segment should be that large.
	 */
	if ((deferred_ttep_pair != NULL) && (deferred_ttep_pair[0] != NULL) && (ttep == (deferred_ttep_pair[0] + 1))) {
		assert(deferred_tte_pair[1] == 0);
		deferred_ttep_pair[1] = ttep;
		deferred_tte_pair[1] = entry;
	} else if (kva_active && (phystokv(pa) == ttebase)) {
		assert(deferred_ttep_pair != NULL);
		assert(granule & ARM64_GRANULE_ALLOW_BLOCK);
		if (deferred_ttep_pair[0] == NULL) {
			deferred_ttep_pair[0] = ttep;
			deferred_tte_pair[0] = entry;
		} else {
			assert(deferred_ttep_pair[1] == NULL);
			deferred_ttep_pair[1] = ttep;
			deferred_tte_pair[1] = entry;
		}
	} else {
		*ttep = entry;
	}
}


static void
arm_vm_page_granular_helper(vm_offset_t start, vm_offset_t _end, vm_offset_t va, pmap_paddr_t pa_offset,
    int pte_prot_APX, int pte_prot_XN, unsigned granule,
    tt_entry_t **deferred_ttep_pair, tt_entry_t *deferred_tte_pair)
{
	if (va & ARM_TT_L2_OFFMASK) { /* ragged edge hanging over a ARM_TT_L2_SIZE  boundary */
		tt_entry_t *tte2;
		tt_entry_t tmplate;
		pmap_paddr_t pa;
		pt_entry_t *ppte, ptmp;
		addr64_t ppte_phys;
		unsigned i;

		va &= ~ARM_TT_L2_OFFMASK;
		pa = va - gVirtBase + gPhysBase - pa_offset;

		if (pa >= real_avail_end) {
			return;
		}

		tte2 = arm_kva_to_tte(va);

		assert(_end >= va);
		tmplate = *tte2;

		if (ARM_TTE_TYPE_TABLE == (tmplate & ARM_TTE_TYPE_MASK)) {
			/* pick up the existing page table. */
			ppte = (pt_entry_t *)phystokv((tmplate & ARM_TTE_TABLE_MASK));
		} else {
			// TTE must be reincarnated with page level mappings.

			// ... but we don't want to break up blocks on live
			// translation tables.
			assert(!kva_active);

			ppte = (pt_entry_t*)alloc_ptpage(pa_offset == 0);
			bzero(ppte, ARM_PGBYTES);
			ppte_phys = kvtophys((vm_offset_t)ppte);

			*tte2 = pa_to_tte(ppte_phys) | ARM_TTE_TYPE_TABLE | ARM_TTE_VALID;
		}

		vm_offset_t len = _end - va;
		if ((pa + len) > real_avail_end) {
			_end -= (pa + len - real_avail_end);
		}
		assert((start - gVirtBase + gPhysBase - pa_offset) >= gPhysBase);

		/* Round up to the nearest PAGE_SIZE boundary when creating mappings:
		 * PAGE_SIZE may be a multiple of ARM_PGBYTES, and we don't want to leave
		 * a ragged non-PAGE_SIZE-aligned edge. */
		vm_offset_t rounded_end = round_page(_end);
		/* Apply the desired protections to the specified page range */
		for (i = 0; i <= (ARM_TT_L3_INDEX_MASK >> ARM_TT_L3_SHIFT); i++) {
			if ((start <= va) && (va < rounded_end)) {
				ptmp = pa | ARM_PTE_AF | ARM_PTE_SH(SH_OUTER_MEMORY) | ARM_PTE_TYPE;
				ptmp = ptmp | ARM_PTE_ATTRINDX(CACHE_ATTRINDX_DEFAULT);
				ptmp = ptmp | ARM_PTE_AP(pte_prot_APX);
				ptmp = ptmp | ARM_PTE_NX;
#if __ARM_KERNEL_PROTECT__
				ptmp = ptmp | ARM_PTE_NG;
#endif /* __ARM_KERNEL_PROTECT__ */

				if (pte_prot_XN) {
					ptmp = ptmp | ARM_PTE_PNX;
				}

				/*
				 * If we can, apply the contiguous hint to this range.  The hint is
				 * applicable if the current address falls within a hint-sized range that will
				 * be fully covered by this mapping request.
				 */
				if ((va >= round_up_pte_hint_address(start)) && (round_up_pte_hint_address(va + 1) <= _end) &&
				    (granule & ARM64_GRANULE_ALLOW_HINT) && use_contiguous_hint) {
					assert((va & ((1 << ARM_PTE_HINT_ADDR_SHIFT) - 1)) == ((pa & ((1 << ARM_PTE_HINT_ADDR_SHIFT) - 1))));
					ptmp |= ARM_PTE_HINT;
					/* Do not attempt to reapply the hint bit to an already-active mapping.
					 * This very likely means we're attempting to change attributes on an already-active mapping,
					 * which violates the requirement of the hint bit.*/
					assert(!kva_active || (ppte[i] == ARM_PTE_TYPE_FAULT));
				}
				/*
				 * Do not change the contiguous bit on an active mapping.  Even in a single-threaded
				 * environment, it's possible for prefetch to produce a TLB conflict by trying to pull in
				 * a hint-sized entry on top of one or more existing page-sized entries.  It's also useful
				 * to make sure we're not trying to unhint a sub-range of a larger hinted range, which
				 * could produce a later TLB conflict.
				 */
				assert(!kva_active || (ppte[i] == ARM_PTE_TYPE_FAULT) || ((ppte[i] & ARM_PTE_HINT) == (ptmp & ARM_PTE_HINT)));

				update_or_defer_tte(&ppte[i], ptmp, pa, (vm_map_address_t)ppte, granule, deferred_ttep_pair, deferred_tte_pair);
			}

			va += ARM_PGBYTES;
			pa += ARM_PGBYTES;
		}
	}
}


static void
arm_vm_page_granular_prot(vm_offset_t start, unsigned long size, pmap_paddr_t pa_offset,
    int tte_prot_XN, int pte_prot_APX, int pte_prot_XN,
    unsigned granule)
{
	tt_entry_t *deferred_ttep_pair[2] = {NULL};
	tt_entry_t deferred_tte_pair[2] = {0};
	vm_offset_t _end = start + size;
	vm_offset_t align_start = (start + ARM_TT_L2_OFFMASK) & ~ARM_TT_L2_OFFMASK;

	if (size == 0x0UL) {
		return;
	}

	if (align_start > _end) {
		align_start = _end;
	}

	arm_vm_page_granular_helper(start, align_start, start, pa_offset, pte_prot_APX, pte_prot_XN, granule, deferred_ttep_pair, deferred_tte_pair);

	while ((_end - align_start) >= ARM_TT_L2_SIZE) {
		if (!(granule & ARM64_GRANULE_ALLOW_BLOCK)) {
			arm_vm_page_granular_helper(align_start, align_start + ARM_TT_L2_SIZE, align_start + 1, pa_offset,
			    pte_prot_APX, pte_prot_XN, granule, deferred_ttep_pair, deferred_tte_pair);
		} else {
			pmap_paddr_t pa = align_start - gVirtBase + gPhysBase - pa_offset;
			assert((pa & ARM_TT_L2_OFFMASK) == 0);
			tt_entry_t *tte2;
			tt_entry_t tmplate;

			tte2 = arm_kva_to_tte(align_start);

			if ((pa >= gPhysBase) && (pa < real_avail_end)) {
				tmplate = (pa & ARM_TTE_BLOCK_L2_MASK) | ARM_TTE_TYPE_BLOCK
				    | ARM_TTE_VALID | ARM_TTE_BLOCK_AF | ARM_TTE_BLOCK_NX
				    | ARM_TTE_BLOCK_AP(pte_prot_APX) | ARM_TTE_BLOCK_SH(SH_OUTER_MEMORY)
				    | ARM_TTE_BLOCK_ATTRINDX(CACHE_ATTRINDX_WRITEBACK);

#if __ARM_KERNEL_PROTECT__
				tmplate = tmplate | ARM_TTE_BLOCK_NG;
#endif /* __ARM_KERNEL_PROTECT__ */
				if (tte_prot_XN) {
					tmplate = tmplate | ARM_TTE_BLOCK_PNX;
				}

				update_or_defer_tte(tte2, tmplate, pa, (vm_map_address_t)tte2 & ~ARM_TT_L2_OFFMASK,
				    granule, deferred_ttep_pair, deferred_tte_pair);
			}
		}
		align_start += ARM_TT_L2_SIZE;
	}

	if (align_start < _end) {
		arm_vm_page_granular_helper(align_start, _end, _end, pa_offset, pte_prot_APX, pte_prot_XN, granule, deferred_ttep_pair, deferred_tte_pair);
	}

	if (deferred_ttep_pair[0] != NULL) {
#if DEBUG || DEVELOPMENT
		/*
		 * Flush the TLB to catch bugs that might cause us to prematurely revoke write access from the pagetable page.
		 * These bugs may otherwise be hidden by TLB entries in most cases, resulting in very rare panics.
		 * Note that we always flush the TLB at the end of arm_vm_prot_finalize().
		 */
		flush_mmu_tlb();
#endif
		/*
		 * The first TTE in the pair is a recursive mapping of the pagetable region, so we must update it last
		 * to avoid potentially marking deferred_pte_pair[1] read-only.
		 */
		if (deferred_tte_pair[1] != 0) {
			os_atomic_store(deferred_ttep_pair[1], deferred_tte_pair[1], release);
		}
		os_atomic_store(deferred_ttep_pair[0], deferred_tte_pair[0], release);
	}
}


static void
arm_vm_physmap_slide(ptov_table_entry *temp_ptov_table, vm_map_address_t orig_va, vm_size_t len, int pte_prot_APX, unsigned granule)
{
	pmap_paddr_t pa_offset;

	if (__improbable(ptov_index >= PTOV_TABLE_SIZE)) {
		panic("%s: PTOV table limit exceeded; segment va = 0x%llx, size = 0x%llx", __func__,
		    (unsigned long long)orig_va, (unsigned long long)len);
	}
	assert((orig_va & ARM_PGMASK) == 0);
	temp_ptov_table[ptov_index].pa = orig_va - gVirtBase + gPhysBase;
	if (ptov_index == 0) {
		temp_ptov_table[ptov_index].va = physmap_base;
	} else {
		temp_ptov_table[ptov_index].va = temp_ptov_table[ptov_index - 1].va + temp_ptov_table[ptov_index - 1].len;
	}
	if (granule & ARM64_GRANULE_ALLOW_BLOCK) {
		vm_map_address_t orig_offset = temp_ptov_table[ptov_index].pa & ARM_TT_TWIG_OFFMASK;
		vm_map_address_t new_offset = temp_ptov_table[ptov_index].va & ARM_TT_TWIG_OFFMASK;
		if (new_offset < orig_offset) {
			temp_ptov_table[ptov_index].va += (orig_offset - new_offset);
		} else if (new_offset > orig_offset) {
			temp_ptov_table[ptov_index].va = ROUND_TWIG(temp_ptov_table[ptov_index].va) + orig_offset;
		}
	}
	assert((temp_ptov_table[ptov_index].va & ARM_PGMASK) == 0);
	temp_ptov_table[ptov_index].len = round_page(len);
	pa_offset = temp_ptov_table[ptov_index].va - orig_va;
	arm_vm_page_granular_prot(temp_ptov_table[ptov_index].va, temp_ptov_table[ptov_index].len, pa_offset, 1, pte_prot_APX, 1, ziyi_hop_construction_granule(granule));
	++ptov_index;
}

