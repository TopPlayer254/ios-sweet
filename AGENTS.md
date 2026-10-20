# ziyi port constraints

- Target ziyi/SM7450, iPhone X D22AP/iPhone10,3 userland, iOS 16.7.16.
  Read README.md, NOTES.md and Documentation/ziyi/DONOR-AUDIT-20261020.md.
- Current goal is a real Apple lock screen with advancing clocks. Record
  PosterBoard process/rendering evidence separately; a shim drawing is not iOS.
- First complete-source PID1 evidence is S2X build10: the source-CI kernel boots
  native Darwin through BSD and executes our diagnostic PID1 on mockfs/md0,
  with console/syscalls/5-second heartbeat. X CRC907f3943, all11 segment/BSS,
  ADT and boot_args hashes matched. Read the S2-PID1 checkpoint report.
  Apple launchd/logo/PreBoard/PosterBoard/SpringBoard remain unproven.
- Apple OS/SystemOS/AppOS images and APFS/Mach-O inventories now pass host
  verification. Runtime cache2.55GiB exceeds the tested128MiB arena.
  CI39507288859 built an own-source32MiB HFSX diagnostic root and passed
  fsck/readonly reread plus PC digest/CRC/content checks. Phone S2P build11
  PASS CRC1241a774 proves11segment/ADT/args hashes, entry disabled.
  HFS mount/new init follow-on results are below; use completed
  P proof before entry. Read the Apple userland audit report.
- HFS X11 passed RAM hashes then hit uipc_mbuf.c:1743 before mount. X12
  adds only mbuf_pool=2 to the same payload/layout; a pinned byte-checked
  command-line transition reuses P11 root/kernel proof, with new args hashed
  on the phone before entry. X12 PASSED HFS md0 mount, own PID1, two file
  reads/symlink lookup and5s heartbeat (6/10 at300/320s), no visible panic.
  X preentry CRC25075121,11segment/ADT/new args SHA match. Apple UI is unproven.
- Current experiments remain RAM-only. The user's 2026-10-20 conditional
  storage authorization applies only near userspace, after complete verified
  backups of both GPT copies and all affected partitions including userdata,
  exact identity/sizes, independent on-device hashes and an established
  restore procedure. These conditions are not yet met. Upstream permissions
  belong to its author's device/session and do not authorize writes here.
- Before overwriting existing local port files, create a new scoped backup
  and verify hashes, source stability, full inventory and independent restore
  copies. The main workspace uses tools/ios/verified_backup.py. Preserve all
  frozen experiment evidence and never overwrite previous backup folders.
- S2 excludes UFS/persistent logging and unported touch/keys/USB/RPMh/SPMI
  provider bodies. Keep the shim UFS fallback excluded. fastboot boot does
  not itself prevent writes made by the payload.
- Port hardware addresses/topology from actual ziyi measurements. Do not
  copy donor RAM, framebuffer, GIC or storage settings. Start with one CPU.
- Keep Apple assets, phone dumps, serials and credentials out of Git. Record
  exact commits, hashes, CI IDs and what is host-checked versus phone-tested.
- Commit changes on the codex/ branch and maintain the active NOTES handoff.
