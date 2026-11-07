"""Independent strict little-endian GHF1 CRC decoder. No native operations."""
import argparse
import json
import struct
import zlib
from pathlib import Path

SIZE = 320

def decode(data):
    if len(data) != SIZE:
        raise ValueError("record size")
    words = struct.unpack("<80I", data)
    if words[:3] != (0x31464847, 1, SIZE):
        raise ValueError("record identity")
    if words[3] != zlib.crc32(data[:12] + b"\0" * 4 + data[16:]):
        raise ValueError("record CRC")
    stage, error, flags, live = words[4:8]
    if any(words[74:]) or not 1 <= stage <= 8 or live > 3 or flags & ~1023 or not flags & 256:
        raise ValueError("record schema")
    if words[18] > 19 or words[17] > 255 or words[19] > 1 or words[20] > 15 or words[21] > 15:
        raise ValueError("record bounds")
    if words[12] != words[13] + words[14] or any(words[i] > 1 for i in (61, 62, 72, 73)) or words[60] > 6:
        raise ValueError("record counters")
    if flags & 1 and (not 1 <= words[8] <= 4095 or words[9:11] != (words[8] << 20 | 0x201, 0x20000000)):
        raise ValueError("version command")
    for flag, index in ((2, 72), (16, 73), (8, 61), (32, 62)):
        if bool(flags & flag) != bool(words[index]):
            raise ValueError("publication counters")
    if flags & 4 and not flags & 2 or flags & 8 and (not flags & 1 or words[60] < 5):
        raise ValueError("publication state")
    if flags & 16 and not flags & 8 or flags & 32 and (not flags & 8 or words[18] < 3):
        raise ValueError("completion state")
    raw = list(words[30:30 + words[18]])
    if any(words[30 + words[18]:49]) or raw and words[17] != (raw[0] >> 8 & 255):
        raise ValueError("raw prefix")
    wide = lambda at: words[at] | words[at + 1] << 32
    if flags & 512 and wide(67) < wide(65):
        raise ValueError("clock order")
    if not error and (stage != 8 or live != 2 or not flags & 32 or words[15] or words[18] < 4 or (words[16] >> 28 != 2 and words[16] != 0x30020004)):
        raise ValueError("success state")
    return dict(schema=1, crc32=f"{words[3]:08x}", stage=stage, error=error,
                flags=flags, live_state=live, sequence=words[8], sent=list(words[9:11]),
                poll_cap=words[11], polls=words[12], ready_polls=words[13], ack_polls=words[14],
                remote_status=words[15], version=words[16], raw_declared=words[17], raw=raw,
                owner_failed=bool(words[19]), first_mask=words[20], last_mask=words[21],
                queue_first=list(words[22:26]), queue_last=list(words[26:30]),
                first_health=list(words[49:54]), last_health=list(words[54:59]),
                gmu_address=words[59], stores=words[60], q0_write_stores=words[61],
                q1_read_stores=words[62], epoch=wide(63), start_tick=wide(65),
                last_tick=wide(67), timeout_ticks=wide(69), delay_us=words[71],
                start_calls=words[72], doorbells=words[73])

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("record", type=Path)
    args = parser.parse_args()
    print(json.dumps(decode(args.record.read_bytes()), indent=2))
