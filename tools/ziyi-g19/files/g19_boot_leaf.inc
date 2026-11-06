/* Included after kva_active in arm_vm_init.c. Boot construction only.
 * No live block demotion. PTOV placement continues to use original granule. */
#if SWEET && !XNU_MONITOR
SECURITY_READ_ONLY_LATE(static boolean_t) ziyi_hop_leaf_mode = FALSE;

static unsigned int
ziyi_hop_construction_granule(unsigned int original)
{
 if (ziyi_hop_leaf_mode) {
  assert(!kva_active);
  return 0;
 }
 return original;
}

int ziyi_hop_boot_leaf_ready(void);
int
ziyi_hop_boot_leaf_ready(void)
{
 return ziyi_hop_leaf_mode && kva_active && PAGE_SHIFT == 14 &&
        PAGE_SIZE == 16384 && gPhysBase == 0x900000000ULL &&
        mem_size == 0x80000000ULL && real_phys_size == 0x80000000ULL;
}
#else
#define ziyi_hop_construction_granule(original) (original)
#endif
