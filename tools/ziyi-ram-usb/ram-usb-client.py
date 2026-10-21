#!/usr/bin/env python3
"""Bounded receive-only RAM protocol client. Never opens fastboot or a disk."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import time
import zlib

MAGIC = b"ZYRMUSB1"
HELLO, ECHO, BEGIN, DATA, END, INFO = range(1, 7)
MAX_PACKET = 65536


class Client:
    def __init__(self, transport):
        self.io = transport
        self.sequence = 0

    def read(self, n):
        out = bytearray()
        while len(out) < n:
            chunk = self.io.read(n - len(out))
            if not chunk:
                raise RuntimeError("USB EOF/timeout; no automatic replay")
            out.extend(chunk)
        return bytes(out)

    def request(self, op, data=b"", allow_error=False):
        if len(data) > MAX_PACKET:
            raise ValueError("packet too large")
        self.sequence += 1
        packet = struct.pack("<8sIIII", MAGIC, op, self.sequence, len(data), zlib.crc32(data)) + data
        view = memoryview(packet)
        while view:
            n = self.io.write(view)
            if not n:
                raise RuntimeError("USB short write")
            view = view[n:]
        magic, reply_op, seq, length, crc = struct.unpack("<8sIIII", self.read(24))
        if (magic, reply_op, seq) != (MAGIC, op | 0x80000000, self.sequence) or not 4 <= length <= MAX_PACKET + 4:
            raise RuntimeError("invalid response envelope")
        response = self.read(length)
        if zlib.crc32(response) != crc:
            raise RuntimeError("response CRC mismatch")
        status, = struct.unpack_from("<I", response)
        if status and not allow_error:
            raise RuntimeError(f"RAM protocol rejected op {op}, status {status}")
        return (status, response[4:]) if allow_error else response[4:]

    def hello(self, expected_pid=1):
        nonce = os.urandom(16)
        result = self.request(HELLO, nonce)
        if len(result) != 48 or result[:16] != nonce:
            raise RuntimeError("nonce/hello mismatch")
        pid, version, *limits = struct.unpack("<IIQQQ", result[16:])
        if pid != expected_pid or version != 1 or limits != [3 << 30, 128 << 20, 2 << 20]:
            raise RuntimeError("wrong PID1/protocol/limits")
        return {"pid": pid, "version": version, "limits": limits, "nonce": nonce.hex()}

    def transfer(self, path, asset_id):
        path = Path(path)
        before = path.stat()
        with path.open("rb") as f:
            expected = hashlib.file_digest(f, "sha256").digest()
        self.request(BEGIN, struct.pack("<IQ", asset_id, before.st_size) + expected)
        sent = 0
        digest = hashlib.sha256()
        start = time.monotonic()
        with path.open("rb") as f:
            while chunk := f.read(MAX_PACKET - 12):
                self.request(DATA, struct.pack("<IQ", asset_id, sent) + chunk)
                digest.update(chunk)
                sent += len(chunk)
        after = path.stat()
        stable = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) == (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
        if sent != before.st_size or digest.digest() != expected or not stable:
            raise RuntimeError("source changed during transfer")
        result = self.request(END, struct.pack("<I", asset_id))
        if result != expected:
            raise RuntimeError("independent in-RAM SHA256 mismatch")
        info = self.request(INFO, struct.pack("<I", asset_id))
        if struct.unpack("<QQI", info) != (sent, sent, 1):
            raise RuntimeError("asset is not fully received and read-only")
        return {"asset_id": asset_id, "bytes": sent, "sha256": expected.hex(),
                "seconds": time.monotonic() - start, "independent_ram_hash": True, "sealed": True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True)
    p.add_argument("--root", type=Path, required=True)
    p.add_argument("--report", type=Path, required=True)
    a = p.parse_args()
    if a.report.exists():
        p.error("report must be new")
    if not 0 < a.root.stat().st_size <= 3 << 30:
        p.error("root must be 1 byte .. 3 GiB")
    import serial  # pip install pyserial; imported only for real USB use
    with serial.Serial(a.port, 115200, timeout=180, write_timeout=180) as io:
        c = Client(io)
        report = {"hello": c.hello(), "root": c.transfer(a.root, 0),
                  "scope": "RAM delivery only; kexec/Apple execution not attempted"}
    a.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
