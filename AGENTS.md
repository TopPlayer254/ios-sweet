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

# ziyi port constraints

**FINAL2026-10-21: HOST_BUILD_COMPLETE_NOT_RUN.** CI39595655078 and local
artifact/source/config/builtin-init/boot packaging audits PASS. Final image:
`Artifacts/ziyi/ios-sweet/20261021-linux-usb-boot-v3/ziyi-ram-usb-build-only.img`,
12550144bytes, SHA256
`a4bbbdc1f438276af464fefcac7bffaf08af5ca75ff43d886fca29057850abba`.
Build finished; STOPPED per latest user instruction, no new phone operations.
Read Documentation/ziyi/IOS-SWEET-20261021-BUILD-ONLY-COMPLETE.md.


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
- Latest instruction2026-10-21: finish the build and STOP WITHOUT RUNNING.
  The host build/audits/packaging are complete. No phone operations or
  fastboot readiness requests until explicit user resumption. Storage-write
  backup/restore conditions remain unsatisfied.
  Read Documentation/ziyi/IOS-SWEET-20261021-LARGE-RAM-USERSPACE.md.
  A3GiB headless real Apple launchd/dyld/cache root has passed macOS fsck/
  reread and PC GCM/full-root/48file hashes. Own no-op boot-task fixtures
  and one own child job are included; no phone Apple execution or GUI yet.
- Experiments are paused; future authorized experiments remain RAM-only. The user's 2026-10-20 conditional
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
# Latest user instruction after Linux device2: finish the build, then STOP.
# NO NEW PHONE RUNS. Earlier fastboot readiness/resumed RAM tests are superseded.
# Complete CI and PC artifact/source/config/packaging audits only.

