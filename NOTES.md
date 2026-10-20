# Active ziyi handoff — 2026-10-21

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
