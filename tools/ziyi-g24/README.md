# G24: full bounded GPU-first-picture candidate

WORK IN PROGRESS on the PC. Not published, compiled as a full XNU kernel or
booted. G23 is CLOSED with native RAM 2fc847c4, GST1 234c628b (five successful
startup ACKs) and GDR 115c0ae5 (quiet not proven, memory retained). No phone
operation is allowed until the next concrete image passes every inherited
gate and the user supplies a fresh cold-fastboot statement.

User target: real GPU operation and drawing in XNU BEFORE Apple userspace.
Keep 2 GiB/eight CPUs and G21 software-iOS rollback. No CA/Metal/refactor,
partition/rootfs write or phone query while preparing this candidate.

Prepared here: copies of the already host-qualified seven-page/24-leaf CP
memory builder and actual CP_BLIT/all-pixel checker, plus an own-firmware
capsule parser and bounded PAS13 transaction. Actual engine/native/PAS/codec host controls and the real XNU GPU translation
unit compile pass. Full macOS kernel/native GPU qualification remain pending.

Critical source finding: own A662 MDT has THREE program headers. Executable
PT_LOAD is index 1, flags 08000007, linked PA 1000, file/memory size 1976.
`a662_zap.b01` is CODE. Index 2/type0/flags02000000 is HASHES (3416 bytes),
`b02` is NOT code. The 3564-byte MDT is 148-byte header + those hashes. The
load is relocatable, one 4-KiB image at 8bb1a000 in an 8-KiB reserved range.
Do not copy Umi's A650 b02-as-code or its PA/SID constants. Own SQE is a660,
43068 bytes, version 016dc109; skip exactly its first four bytes when loading.
Primary mdt_loader.c already saved at 20261026-wifi-lifecycle-design-v1.

Implementation route (continue without broad donor re-audit):

1. Allocate EIGHT whole private 16-KiB Normal-NC pages through the unchanged
   hop_xnu provider from zg_hfi_prepare, while its existing reset/drain/unmapped
   evidence is valid. First seven are tables/ring/pixels/fences/three SQE; the
   eighth is contiguous PAS metadata and is NEVER in the core SMMU mappings.
   Use a separate hop/hop_xnu context and mark LIVE conservatively before
   GMU publication. Never change page size or export aliases. Preserve HFI.
2. After native G23 COMPLETE only: fresh IRQ grant must initially be clear;
   request GX via GMU_HOST2GMU_INTR_SET 40000000, poll INFO80000000 <=500ms,
   clear just that ACK. Check SPTPRAC status &c0==0 and GX GDSC power bit.
   Canonical KGSL register IDs are relative to GPU3d base03d00000; GMU OOB
   SET/CLR/INFO are03d7e650/644/648, SPTPRAC03d7e340.
3. Prove initial core inactive from two stable snapshots, preserve all old
   register values before writes. Keep GMU SMR0/CB2 exactly. Core uses fresh
   SMR1/CB0, SID0 mask400 (NOT Umi mask401). SMMU CB0base03db0000, CBAR0
   03da1000, CBA2R003da1800, SMR103da0804/S2CR103da0c04. Follow the G13/G14
   unused-context format rebase rules; do not compare undefined AArch32 TTBR
   bits after enabling AArch64. Exact full readback, TCR2 38071, TCR803519,
   SCTLRc00127 (G15 explicit ASIDPNE0 + HUPCF), CBAR1f300, ASID0. Do not reuse
   ASID2. Separate table roots, AP-unpriv data and read-only SQE leaves.
4. Apply extracted A662-selected profiles, NOT A650-only config: bank15,
   MAL32, UBWC4 gives mode0/amsbc1/rgb5651; RB NC814/TPL NC4/SP NC24/UCHE400000.
   PC ECO00300200, ROQ02000140/8040362c, LPAC FIFO20, pool128, CP_CHICKEN1,
   UCHE_CMDQ66906, GBIF_CX clear3c0/set140, UCHEmode etc and all42 protected
   spans. HWCG a660 table (49 rows); VBIF a650 table (5 rows). Preserve the
   native GMU clock/control state when configuring GX HWCG; use source RMW.
5. Use a 16-KiB ring (quadword size log2=11, block2), actual RPTR copy target
   in owned fence page+64, SQE GPU RO mapping. CP_ME_INIT eight-word payload
   [2f,3,20000000,0,0,0,0,0], bounded rptr/idle/fault checks. Own signed ZAP
   PAS_INIT/MEM/AUTH with metadata only in the eighth NC page. No blind PAS
   SHUTDOWN or trust-register bypass. Standard 64-bit convention, first INFO
   atomic only; retain interrupted session x6 and original x1..x5; refuse
   WAITQ/EBUSY, no ambiguous retry.
6. After successful PAS_MEM_SETUP, map the CPU16K reserved page WC, but
   read ONLY the authorized4K image at8bb1a000 into two backups and verify
   equal/SHA. Never access the shared lower8K or extra upper4K. Write ONLY
  1976 b01 code bytes and readback the4K authorized range with unchanged tail.
   Retain both backups/mapping/secure ownership; no premature restore.
7. CP_SET_SECURE_MODE0, then a separate CP_MEM_WRITE fence (not rendering),
   then real 87-word CP_BLIT. Require fresh tokens, ring consumption, no CP/
   SMMU fault, all4096 pixels and untouched guards. Only then copy verified
   pixels to the existing framebuffer; keep console strip visible. Optional
   repeated submissions must use fresh tokens and stay within ring bounds.

LIFETIME: the G8 caller normally resets GMU after its firmware_ready hook.
Do NOT return to that cleanup while GX/CP or PAS may borrow G24 resources.
Once G24's first OOB/PAS/core store can have happened, finish the bounded test,
print its compact CRC result and park the diagnostic worker with zg_sleep;
retain lease/mappings/pages on success or uncertainty. This explicit opt-in
kernel-only park avoids changing or weakening the old G8 cleanup. Other cores
and timer stay alive. Before any G24 hardware side effect a refusal may return
to the inherited G23 path. No native success claim comes from CPU models.

Next code: generated immutable register plan + engine/native adapter, host
fault model of the actual engine/adapter, actual XNU TU compiles, patch/source/
linked-call gates, THREE private data slots (existing GMU+Wi-Fi plus new64K
SQE/MDT/b01 slot), whole image/loader proof, then one cold RAM boot. Public
slot contains no vendor bytes; private derivation must preserve every other
byte with inverse + two verified restore copies. Existing G23 CI is already
successful; do not rerun it. A new G24 source needs one new full CI.
