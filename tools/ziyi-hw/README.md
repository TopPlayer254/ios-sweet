# Opt-in ziyi hardware tests inside iOS

This branch preserves the native U25/U26 CPU-rendered UI, input and UFS bodies.
Patch0085 adds explicit opt-in GPU acquisition/clock and Wi-Fi lifecycle samplers.
They wait for SpringBoard and do not implement GPU rendering or a Wi-Fi radio.

Kernel opt-ins: `ziyi_gpu_probe=1 ziyi_gpu_probe_delay=60` and
`ziyi_wifi_probe=1 ziyi_wifi_probe_delay=60`. Add both exact ADT nodes in
`adt-gpu.json` and `adt-wifi.json`, preserving their respective schemas.
Default boot runs neither sampler. Use the established bounded bottom console
to preserve the GUI and read results. Existing fallback payload is unchanged.

CI compiles public synthetic tests against the exact patch files, then the full
Apple kernel. The mandatory audit checks all57 source hashes, unchanged30 U25
bodies, startup hooks, opt-in adapter markers and nonempty linked functions.
CI success does not establish a phone boot, firmware execution or feature use.
No private phone fixture, identity, credentials, Apple binary or firmware ships.
