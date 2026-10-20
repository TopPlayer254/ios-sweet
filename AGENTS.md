# ziyi port constraints

- Target ziyi/SM7450, iPhone X D22AP/iPhone10,3 userland, iOS 16.7.16.
  Read README.md, NOTES.md and Documentation/ziyi/DONOR-AUDIT-20261020.md.
- Current goal is a real Apple lock screen with advancing clocks. Record
  PosterBoard process/rendering evidence separately; a shim drawing is not iOS.
- Last phone evidence is S1 native diagnostic. S2 CI/kernel/provider/layout
  checks are host-only. Darwin, Apple logo and userspace remain unproven.
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
