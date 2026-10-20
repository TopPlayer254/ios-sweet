# Active ziyi handoff — 2026-10-20

Hardware is Xiaomi 13 Lite / ziyi / SM7450. Userland target is iPhone X
D22AP / iPhone10,3, iOS 16.7.16 (20H392). The next visible goal is a real
lock screen with advancing clocks, with separate PosterBoard process proof.

Upstream success on sweet and claims in its historical notes are donor
evidence only. Its storage permissions belonged to another user/device.
Original [upstream engineering notes](https://github.com/dipa-studio/ios-sweet/blob/fdad0de8dc0e75c5ea84762adc03f7232dc91f19/NOTES.md)
are retained in Git history and at the pinned source.

S1 passed one direct native RAM diagnostic boot; this is the last phone result.
Darwin, Apple logo and Apple userspace on ziyi remain unproven.

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
Static payload: entry PA 80148b488, ADT 8020c0000, boot_args 8020c4000,
topOfKernelData 8020c8000; remaining unreserved candidate tail 99844096 bytes.
Runtime bootstrap/allocator budget and bounded native loader are pending.

Next: audit the loader's complete ranges/digest/overlaps, then RAM-only XNU
early boot with framebuffer diagnostics; subsequently a small md0 PID1 root.
Do not run the original sweet shim UFS fallback or its installation commands.

Storage is conditionally authorized only near userspace and after validated
complete backups of both GPT copies and all affected partitions, including
userdata, with independent device read/hash and an established restore path.
The gates are not satisfied. A GPT backup does not contain user files.

See [donor audit](Documentation/ziyi/DONOR-AUDIT-20261020.md) for the inspected
parent/forks, source limitations, local evidence paths and subsequent plan.
