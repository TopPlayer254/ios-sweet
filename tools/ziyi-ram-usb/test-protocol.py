#!/usr/bin/env python3
"""Exercise the actual native C server using pipes, including malformed traffic."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("client", here / "ram-usb-client.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class Pipe:
    def __init__(self, p): self.p = p
    def read(self, n): return self.p.stdout.read(n)
    def write(self, b):
        n = self.p.stdin.write(b)
        self.p.stdin.flush()
        return n


passed = []
exe = sys.argv[1]


def session(fn):
    p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        fn(mod.Client(Pipe(p)), p)
    finally:
        p.stdin.close()
        assert p.wait(timeout=10) == 0
        p.stdout.close()


def full(c, p):
    c.hello(p.pid)
    assert c.request(mod.ECHO, bytes(range(256)) * 256) == bytes(range(256)) * 256
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "root"
        path.write_bytes(bytes(range(256)) * 65536)  # actual complete 16 MiB
        r = c.transfer(path, 0)
        assert r["bytes"] == 16 << 20 and r["sealed"]
    assert c.request(mod.DATA, struct.pack("<IQ", 0, 0) + b"x", True)[0] == 4
    passed.append("16MiB full transfer, independent SHA256, read-only seal")


session(full)


def invalid(c, p):
    assert c.request(mod.ECHO, b"x", True)[0] == 2
    c.hello(p.pid)
    for id_, size in [(0, 0), (0, (3 << 30) + 1), (1, (128 << 20) + 1), (2, (2 << 20) + 1), (3, 1), (0, 2**64 - 1)]:
        assert c.request(mod.BEGIN, struct.pack("<IQ", id_, size) + bytes(32), True)[0] == 3
    assert c.request(99, b"", True)[0] == 3
    assert c.request(mod.BEGIN, struct.pack("<IQ", 0, 3) + hashlib.sha256(b"abc").digest()) == b""
    assert c.request(mod.END, struct.pack("<I", 0), True)[0] == 4
    assert c.request(mod.DATA, struct.pack("<IQ", 0, 2**64 - 1) + b"x", True)[0] == 4
    assert c.request(mod.DATA, struct.pack("<IQ", 0, 1) + b"x", True)[0] == 4
    assert c.request(mod.DATA, struct.pack("<IQ", 0, 0) + b"abcd", True)[0] == 4
    c.request(mod.DATA, struct.pack("<IQ", 0, 0) + b"abd")
    assert c.request(mod.END, struct.pack("<I", 0), True)[0] == 5
    assert struct.unpack("<QQI", c.request(mod.INFO, struct.pack("<I", 0))) == (3, 3, 0)
    passed.append("no-session, bounds/overflow, offset, overrun, premature END, bad SHA, unknown op rejected")


session(invalid)
for name, h, body in [
    ("bad magic", struct.pack("<8sIIII", b"INVALID!", 1, 1, 0, 0), b""),
    ("oversize", struct.pack("<8sIIII", mod.MAGIC, 1, 1, 65537, 0), b""),
    ("replayed sequence", struct.pack("<8sIIII", mod.MAGIC, 1, 0, 0, 0), b""),
    ("bad CRC", struct.pack("<8sIIII", mod.MAGIC, 1, 1, 16, 0), bytes(16)),
]:
    def bad(c, p):
        p.stdin.write(h + body); p.stdin.flush()
        header = c.read(24)
        n = struct.unpack_from("<I", header, 16)[0]
        assert struct.unpack_from("<I", c.read(n))[0] == 1
        assert p.wait(timeout=10) == 0
    session(bad)
    passed.append(name + " closes session")
print(json.dumps({"result": "PASS", "checks": passed}, indent=2))
