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
