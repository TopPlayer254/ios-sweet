# LPAC route and pre-SQE CPU diagnostics

The stock ziyi LPAC SID 1 / mask 0x400 is bound to the already-owned core context 0 through exactly two typed writes, S2CR2 then SMR2. Empty-slot, live-owner, core-table, GMU and exact receipt guards remain mandatory. Other stream slots and context banks are not programmed.

A valid 64-bit table PA may have a zero low word. GOB2 v2 reports the raw PA, worker stage, observer error and LPAC stage/error, including an unarmed fallback after an armed-record error. The worker waits at most three seconds for successful CPU publication of an armed pair, then repeats its original fresh SQE guards. CPU publication is not a device acknowledgement or camera receipt.

The exact target SQE STR and original DSB, existing CP/ME_INIT/PAS/fence/pixel packet, engine and profile remain unchanged. Tests are synthetic until independently observed on the device. CPU optical output and a black screen are not GPU-rendered pixels.
