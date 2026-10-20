# iOS / XNU bring-up for Xiaomi 13 Lite (ziyi)

Target: ziyi / SM7450, iPhone X (`iPhone10,3`, D22AP) iOS 16.7.16 userland.
Current goal: a real Apple lock screen with advancing clocks; prove
PosterBoard execution/rendering separately from SpringBoard's lock screen.

**iOS and Darwin have not booted on this ziyi yet.** The inherited sweet
screenshots and working-feature claims describe the donor phone.

As of 2026-10-20:

- S1 passed one RAM-only native shim diagnostic on ziyi, with owned
  stack/vectors, controlled exception, counter and CRC-checked telemetry.
- S2 XNU compiled successfully in [CI 39465333531](https://github.com/TopPlayer254/ios-sweet/actions/runs/39465333531).
  Downloaded artifact digests, kernel sums, all ten changed source files,
  plain arm64 Mach-O and ten linked disabled hardware providers were checked.
- The 17170616-byte kernel SHA-256 is
  `cef8ea2eab946ce04e0fe3c70371b27784429297d5eb47d0f4311ef951d8637d`.
- Verified Apple D22AP identity metadata and a static RAM payload layout
  exist locally. A bounded native loader and runtime memory/interrupt
  validation remain before an S2 phone boot. This branch is not a ready image.

Current phone experiments remain RAM-only. Storage writes are conditionally
authorized only immediately before userspace, after complete verified GPT
and affected-partition backups (including userdata) and an established
restore procedure. Those gates have not been met. The S2 kernel removes
UFS/persistent logging and unported input/USB/power provider bodies.

See [the donor audit and experiment plan](Documentation/ziyi/DONOR-AUDIT-20261020.md)
and [NOTES.md](NOTES.md) for the active handoff.

Source foundation and credit: [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet/tree/fdad0de8dc0e75c5ea84762adc03f7232dc91f19),
Apple's open-source XNU 8796.141.3, and
[blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build).
The original donor guide/history remain available at that pinned upstream.
No Apple root filesystem or device dumps are published here.
