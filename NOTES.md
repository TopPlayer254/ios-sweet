**Ziyi U12 candidate,2026-10-24; NOT phone tested.**
U11X reached an actual lock-screen clock/date/battery per user photo after
camera coverage. Native U11X CRC76d8c733 all copied-RAM checks PASS; actual
launchd/backboardd/logo and RAM sampler stacks on camera. Clock progression,
interactive home screen and real battery telemetry remain unproven.
U12 adds bounded format-aware CPU blit/scaling and whole-canvas fit, matching
HID coordinates, ziyi Gen3 PON/PM7325 GPIO6 keys and Synaptics TCM SPI.
Input is explicit opt-in (ziyi_keys=1/ziyi_touch=1), after verified root only.
TCM first profile requires inherited SPI firmware, idle FIFOs, known TCXO
clock rate and matching pins/reset; otherwise prints STOP and exits safely.
No donor rails/reset/GCC/firmware/flash programming. Key transport uses only
SPMI observer READ commands. No storage/SMMU/DMA/GPT/prefix policy changes.
8319 native host policy checks and exact UFS/keys/touch XNU C compiles PASS.
Full Apple C++ compile, linked artifacts and hardware results still pending.
No U12 phone launch/reflash yet. User requests full software UI/touch/buttons.
Private assets, device identities, partition backups and photos remain local.

**Ziyi UI asset candidate, 2026-10-22 (build only, not phone tested).**
A separate macOS workflow builds a NEW journaled 8GiB HFSX image from verified
iPhone X 16.7.16/20H392 OS/SystemOS/AppOS members. The pinned Hack-iOS
906425b7 selection yields the same43353 OS files as our PC audit, plus36
OS var seed files. It uses SWEET's matching20H392 root tweaks, cryptex/cache
hardlinks and one own launchd-child diagnostic; no16.5 address injections.
Source/path guards and selection match pass on PC; native owners/fsck/reread
must pass in CI. No Apple executable runs on the runner. Only RSA/AES-GCM
protected gzip bytes and generated reports are uploaded; private key stays
local. This is preparation for real UI, not proof of launchd or graphics.
U4 kernel CI39820863072/668cfaeb succeeded; full artifact audits passed and
its entry-disabled RAM preflight is running. No native UFS read or media
write has been proven yet; all phone write gates remain.

**Ziyi U3 actual / U4 candidate, 2026-10-22.**
U3 CI39815749559/6d531831 SUCCESS. Phone P CRC2b77911f and X preentry
CRC9c7d4266: 11 segments/ADT/args match. Darwin HFS md0 and own PID1 work.
Live SMMU proof PASSED, CR2=0; queue gate stopped before ANY queue writes
or DMA on inherited slot0 header01000000/OCS0/UCDff12f100. No native SCSI,
capacity/GPT read, media writes or real Apple execution yet.
U4 accepts a validated legacy CT0 or CT1 descriptor and completed NOP,
ignores only provably inert unused slots, and dumps rejected descriptors.
Unknown/write UPIUs still reject before takeover; own commands remain CT1.
U4 also gates future UFS root entry on a complete bounded 3GiB SHA readback;
its streaming SHA passed the actual private image on PC (271 checks), not
on the phone. All disk/media writes remain absent. Full CI and phone tests
are required before trusting the U4 candidate. Apple launchd/UI are unproven.
Root/userdata images, GPT/boot/recovery backups and restore evidence remain
private. Existing HyperOS Recovery entry was verified without wiping;
Android return after erasure has not been exercised. Userdata contents were
waived; GPT/system backups and bounded-write/restore checks remain required.
Current user explicitly resumed phone tests; old STOP entries are historical.

# Ziyi U3 candidate — 2026-10-22

U3 replaces the U1 explicit-bypass-only gate with a repeated, exact live
SMMUv2 profile check for the surveyed SM7450 SID 0x20 / CB0, including CR2,
IAS and OAS. Unknown/changed context or inherited firmware write descriptors
rejects before takeover. The owned 32 KiB DMA allocation is retained on failure.
Only TUR, READ CAPACITY and bounded READ(10) CDBs can be generated.

The same candidate adds dual-GPT/CRC/repeat reads and a read-only BSD disk
for the userdata partition. `ziyi_ufs_probe=1 rd=md0` is the initial probe;
`ziyi_ufs_root=1 rd=ufsroot0` is reserved for a separately validated HFS image.
All write opens, strategies and character writes are rejected. No SMMU,
PHY or media writes; no persistent logging. The shim storage fallback remains
excluded. CI runs both policy suites under ASan/UBSan and hashes compiled sources.

Host syntax/patch reconstruction and 951 new policy checks pass. This source
has NOT yet passed full CI or a phone boot. Real Apple launchd/UI remain unproven.
GPT/boot/recovery backups are local and private; they do not authorize flashing.
Userdata contents were explicitly waived, while restore and bounded-write gates
remain mandatory. Earlier build-only STOP entries below are historical and
superseded by the user's current resumption.

# Active ziyi handoff — 2026-10-21

**FINAL2026-10-21: HOST_BUILD_COMPLETE_NOT_RUN.** CI39595655078 and local
artifact/source/config/builtin-init/boot packaging audits PASS. Final image:
`Artifacts/ziyi/ios-sweet/20261021-linux-usb-boot-v3/ziyi-ram-usb-build-only.img`,
12550144bytes, SHA256
`a4bbbdc1f438276af464fefcac7bffaf08af5ca75ff43d886fca29057850abba`.
Build finished; STOPPED per latest user instruction, no new phone operations.
Read Documentation/ziyi/IOS-SWEET-20261021-BUILD-ONLY-COMPLETE.md.


**Current instruction: build complete, STOP without phone operations.**
The earlier phone-ready reply is superseded. Storage backup/restore gates
remain unsatisfied.
The next large milestone is real Apple launchd and complete root delivery.
CI39516221989 built a3GiB HFSX headless launchd/dyld/cache probe root; native
fsck/readonly reread and PC GCM/full-root/all48 files hashes PASS. Both own
static probes match an independent Windows build. Boot-task fsck/mount are
explicitly OWN no-op fixtures; the real launchd is Apple's original binary.
The storage-disabled Linux USB loader is under build/audit; it currently has
receive-only protocol, no kexec RPC. See
Documentation/ziyi/IOS-SWEET-20261021-LARGE-RAM-USERSPACE.md.

Hardware is Xiaomi 13 Lite / ziyi / SM7450. Userland target is iPhone X
D22AP / iPhone10,3, iOS 16.7.16 (20H392). The next visible goal is a real
lock screen with advancing clocks, with separate PosterBoard process proof.

Upstream success on sweet and claims in its historical notes are donor
evidence only. Its storage permissions belonged to another user/device.
Original [upstream engineering notes](https://github.com/dipa-studio/ios-sweet/blob/fdad0de8dc0e75c5ea84762adc03f7232dc91f19/NOTES.md)
are retained in Git history and at the pinned source.

Latest: native Darwin/XNU + diagnostic RAM PID1 PASS. S2X build9 first
reached md0/mockfs and PID1 on a pinned diagnostic derivative. Build10
reproduced it using the complete source-CI kernel: 7 vs11 heartbeat lines
at camera155/175s (5-second interval), no visible panic. This is our
diagnostic init, not Apple launchd or graphical iOS userspace.

Source commit `08b7f85b9899ae7ff5e839232a764b57cf01b89c`, CI39475233522
success. Exact phone-tested kernel SHA
`cb8d9246ce087ac472de727e4ee4a42b894eac6a2b5a8168fca2296cb40465ec`.
Build10 P image SHA `4a84b7f98ffa1c27da124bc9d575e5bbe0a2c561046c0a8e6905a37af8157268`,
CRC8c79c0c7; X image SHA `9ce4e7941d721aac1a1c02783d510cb8e34b473b3053c44c591bb94c79b939da`,
CRC907f3943. All11 copied segments/BSS, ADT and boot_args matched phone
SHA256. ABI/stack/BRK/counter guards and optical packet CRCs passed.
Local evidence is frozen; no Apple assets, dumps or serials are published.

Earlier source build:

S2 commit `b9d1a47f55134629fa12fbb420082d5afc6eeca8` adds the early ziyi
overlay and dedicated workflow. Run 39465333531 completed/success.
Kernel SHA-256: `cef8ea2eab946ce04e0fe3c70371b27784429297d5eb47d0f4311ef951d8637d`.
Both artifact API digests/ZIP CRC, kernel SHA256SUMS and ten changed source
files matched. Ten linked hardware provider functions are MOV/RET stubs;
removed UFS helpers are absent. This is host evidence, not a phone boot.

The real D22AP DeviceTree matched Apple's manifest SHA-384. Only `/product`
identity properties were imported into a separate ziyi ADT, not Apple MMIO
nodes. Candidate memory is `0x800000000+0x8000000`, framebuffer B8000000,
GICD 17100000, GICR 17180000 / stride 40000, vtimer 27, one CPU.
Current RAM PID1 layout: entry PA80148b488; RAMDisk8020c0000+8000;
ADT8020c8000, boot_args8020cc000, top8020d0000. Kernel-managed arena128MiB.
VM/pmap, CPU/SGI, BSD, root mount/exec and diagnostic syscalls are tested.
This small arena is not sufficient evidence of Apple GUI memory capacity.

Fixes proved on hardware: early-fbcon mapping10MiB instead of43MiB reservation;
skip unused CNTP_CTL_EL0 precaution that trapped at EL1, preserving CNTV;
correct CPU personality from qcom,kryo470 to actual qcom,sm7450.

OS/SystemOS/AppOS full manifest SHA384/ZIP CRC/reread verification and
read-only APFS/Mach-O inventories now pass. launchd/dyld and selected UI
binaries are plain arm64. The main runtime shared cache is2738257920bytes
(43files, excluding symbols), too large for the current128MiB arena.
Next: a separate32MiB HFS diagnostic RAM root proves a real directory tree,
file reads and symlink traversal with our static PID1, before Apple runtime.
The macOS workflow builds only our root/init, without Apple assets.
CI39507288859 passed native fsck/readonly reread, ZIP digest/CRC and
PC HFS/init/sentinel read-back; source files match a fresh pinned clone.
P/X build11 pass guarded loader/optical audits. Follow-on HFS results are below.
S2P build11/device9 passed on the phone: CRC1241a774, all11 segment/BSS,
ADT and boot_args hashes matched, entry disabled;214.471s uncached hashing.
HFS root SHA b69a1ae3b0df915c4a267338bafaf8d519b2c5d612a5f403471327dd93b4fa43,
PA8020c0000+2000000, ADT8040c0000, args8040c4000, top8040c8000.
The128MiB managed arena and all10 kernel segment bytes are unchanged.
X build11 preentry CRC83724eda passed, then uipc_mbuf.c:1743 asserted before
HFS mount. The32MiB root leaves sane_size below the64MiB auto-config gate;
the medium fallback yields zero BIGCL minimum. Build12 adds only mbuf_pool=2,
with kernel/root/ADT/layout unchanged. Host audits and five negative runner
guard cases pass. It reuses exact P11 root/kernel proof and validates the
new command line/hash before device I/O and again on the phone before entry.
Build12 then PASSED HFS mount on md0 and our PID1's two file reads/symlink
lookup.6/10 heartbeat messages at300/320s (5s interval), no visible panic.
X preentry CRC25075121, all11segments/ADT/new args hashes matched.
Apple launchd/UI remain unproven; next host work measures runtime cache
transport size and a feasible RAM/root layout before Apple code execution.
See [Apple userland audit](Documentation/ziyi/IOS-SWEET-20261021-APPLE-USERLAND.md).
Keep phone storage untouched until all backup/restore gates below are met.
Do not run the original sweet shim UFS fallback or its installation commands.

Storage is conditionally authorized only near userspace and after validated
complete backups of both GPT copies and all affected partitions, including
userdata, with independent device read/hash and an established restore path.
The gates are not satisfied. A GPT backup does not contain user files.

See [donor audit](Documentation/ziyi/DONOR-AUDIT-20261020.md) for the inspected
parent/forks, source limitations, local evidence paths and subsequent plan.
See [PID1 checkpoint](Documentation/ziyi/IOS-SWEET-20261020-S2-PID1.md)
and [full S2 report](Documentation/ziyi/IOS-SWEET-20261020-S2.md).
Latest instruction after Linux device2: finish the build and stop without
running it. Earlier phone readiness is superseded; no further phone tests.

