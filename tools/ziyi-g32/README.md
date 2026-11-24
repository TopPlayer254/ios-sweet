# Independent CPU optical observer

This diagnostic changes one native observer region. The CP/SQE/ME_INIT/PAS/fence/pixel packet, original SQE store and original barrier, engine, register profile and all original live hardware guards are retained.

Before releasing worker readiness, a separately bound CPU writes an unarmed heartbeat into a measured framebuffer rectangle. It then sends frozen 160-byte GOB2 snapshots through two CRC-protected ZMP1RAW1 pages, without printf or console locks. Its 60-second lifetime and three-second initial wait do not bound a stalled hardware transaction.

The optical square is written by the CPU. It is not GPU rendering. Progress 2 is store issue, not device acknowledgement; progress 3 is return from the original DSB, not register readback. The existing GPU packet must independently prove completion, fence, pixels and canaries.
