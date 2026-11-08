# G25: reach the first native GPU pixels before iOS

This continuation defers one optional performance-counter setting and keeps
the complete bounded G24 route through SQE/ZAP, CP fence and the verified BLIT.
G24 visually refused register03d0142c readback: ffffffff ->002f7fff. Its GPU
record remains incomplete (C10 unread), so that value is not declared a
qualified hardware writable-bit mask. Native RAM CRCae173297 did pass.

Pinned KGSL adreno_a6xx.c describes this setting as making all blocks
contribute to the GPU BUSY performance counter. G25 leaves it unmodified;
it is absent from all151 profile addresses. All remaining full readback,
ownership, phase order, packet, fault and pixel checks are unchanged.
No generic readback masking or permission relaxation is introduced.

The320-byte GCP1 ABI is unchanged. Actual native printing emits five pages,
each with one header and four rows, every2 seconds. This reduces collision
with periodic two-line framebuffer statistics in the11-line console.

The new kernel has only two changed source scopes, profile and native
adapter include. G24 rollback, own firmware slots,2GiB/eight CPUs/input,
iPhone X2436/HFS/WiFi and kernel-only BSD park remain. No CA/Metal/UI refactor.
No phone operation until the concrete image passes all full-CI and PC gates
and the user supplies one fresh cold-fastboot readiness statement.
