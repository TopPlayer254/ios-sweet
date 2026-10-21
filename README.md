# iOS / XNU bring-up for Xiaomi 13 Lite (ziyi)

Target: ziyi / SM7450, iPhone X (`iPhone10,3`, D22AP) iOS 16.7.16 userland.
Current goal: a real Apple lock screen with advancing clocks; prove
PosterBoard execution/rendering separately from SpringBoard's lock screen.

**Native Darwin/XNU, a real HFSX RAM root and our diagnostic PID1 boot on ziyi.**
Apple launchd, the Apple logo and Apple graphical userspace remain ahead.
The inherited sweet screenshots describe the donor phone.

As of 2026-10-21:

- S1 passed one RAM-only native shim diagnostic on ziyi, with owned
  stack/vectors, controlled exception, counter and CRC-checked telemetry.
- S2X booted through 16K MMU/VM/pmap, platform IOKit, boot CPU 1/1,
  self-SGI1, BSD/HFS and kernel-thread tests. A 32K mockfs RAM disk runs
  our static diagnostic PID1, with console/syscalls and a 5-second heartbeat.
- Reproduced with the exact source-built [CI 39475233522](https://github.com/TopPlayer254/ios-sweet/actions/runs/39475233522)
  kernel, commit `08b7f85b9899ae7ff5e839232a764b57cf01b89c`.
  Kernel SHA-256 `cb8d9246ce087ac472de727e4ee4a42b894eac6a2b5a8168fca2296cb40465ec`.
  Artifact digests/CRC, twelve source files, final Mach-O/provider stubs
  and phone RAM hashes were verified; source-CI X snapshot CRC `907f3943`.
- Initial fixes: narrow early framebuffer mapping to active pixels,
  omit an inaccessible physical-timer precaution, and match SweetCPU
  to actual `qcom,sm7450` compatibility. CNTV remains enabled; self-SGI
  delivery and timed sleeps work.
- No Apple root filesystem or graphical iOS process has run on ziyi yet.
  OS/SystemOS/AppOS images now pass manifest SHA384/ZIP CRC and independent
  reread. A read-only APFS/Mach-O audit identified plain arm64 launchd/dyld
  and the actual PreBoard/PosterBoard executables. The runtime cache is
  2.55GiB without symbols, beyond the tested128MiB arena.
  A separate32MiB diagnostic HFS RAM-root workflow prepares the next
  filesystem experiment. CI39507288859 passed native fsck/readonly reread
  and independent PC artifact checks. Phone S2P build11 passed full RAM
  delivery hashes (CRC1241a774), with XNU entry disabled. HFS mount has
  now passed: X build12 adds only mbuf_pool=2 to avoid the64MiB auto-config
  gate, mounts HFS md0 and runs our PID1 with two verified file reads,
  symlink traversal and5s heartbeat (6/10 messages at300/320s), no visible panic.
  X preentry CRC25075121; kernel/root/ADT/layout bytes are unchanged.

The latest instruction **resumes audited RAM-only tests toward a real lock
screen with clocks**; the phone is ready. Storage-write gates remain unmet.
CI39516221989 built
a3GiB headless Apple launchd/dyld root with all43 runtime cache files. Native
fsck/readback and independent PC GCM/full-root/48file hashes passed. It uses
explicit own no-op fsck/mount fixtures and one own child job; no stock GUI.
The receive-only Linux USB loader is a separate candidate under build/audit.
See [the large RAM userspace stage](Documentation/ziyi/IOS-SWEET-20261021-LARGE-RAM-USERSPACE.md).

Current phone experiments remain RAM-only. Storage writes are conditionally
authorized only immediately before userspace, after complete verified GPT
and affected-partition backups (including userdata) and an established
restore procedure. Those gates have not been met. The S2 kernel removes
UFS/persistent logging and unported input/USB/power provider bodies.

See [the PID1 checkpoint](Documentation/ziyi/IOS-SWEET-20261020-S2-PID1.md),
[Apple userland audit](Documentation/ziyi/IOS-SWEET-20261021-APPLE-USERLAND.md),
[the donor audit](Documentation/ziyi/DONOR-AUDIT-20261020.md)
and [NOTES.md](NOTES.md) for the active handoff.

Source foundation and credit: [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet/tree/fdad0de8dc0e75c5ea84762adc03f7232dc91f19),
Apple's open-source XNU 8796.141.3, and
[blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build).
The original donor guide/history remain available at that pinned upstream.
No Apple root filesystem or device dumps are published here.
