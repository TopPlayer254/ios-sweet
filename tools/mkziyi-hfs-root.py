"""Build a small OWN diagnostic HFSX RAM root on a macOS runner.

No Apple firmware, phone access, storage drivers or deployment commands.
Only a newly created local disk-image file is mounted. Output contains our init.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import re
import struct
import subprocess
import sys

MKINIT_SHA = '79b8585105d4ecfc6f47542563577d0c55c22e7eaa517cffb367cf24e7c4e8ac'
TOKEN = b'ZIYI-HFS-RAMROOT-v1\n'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, **kw):
    return subprocess.run([str(x) for x in args], check=True, **kw)


def validate_init(raw):
    magic, cpu, subtype, kind, count, size, flags, _ = struct.unpack_from('<8I', raw)
    assert (magic, cpu, subtype, kind, flags) == (0xfeedfacf, 0x100000c, 0, 2, 1)
    pos = 32
    signature = None
    commands = []
    for _ in range(count):
        cmd, length = struct.unpack_from('<II', raw, pos)
        assert cmd in (0x19, 5, 0x1d) and length >= 8 and pos + length <= 32 + size
        commands.append(cmd)
        if cmd == 0x1d:
            signature = struct.unpack_from('<II', raw, pos + 8)
        pos += length
    assert pos == 32 + size and signature
    so, ss = signature
    blob = raw[so:so+ss]
    assert len(blob) == ss and so+ss == len(raw)
    assert struct.unpack_from('>3I', blob) == (0xfade0cc0, ss, 1)
    slot, off = struct.unpack_from('>2I', blob, 12)
    assert slot == 0
    cd = blob[off:]
    head = struct.unpack_from('>9I', cd)
    assert (head[0], head[1], head[2], head[3], head[6], head[7], head[8]) == (0xfade0c02, len(cd), 0x20400, 2, 0, 4, 0x4000)
    assert cd[36:40] == bytes((32, 2, 0, 12))
    for i in range(4):
        assert cd[head[4]+i*32:head[4]+(i+1)*32] == hashlib.sha256(raw[i*4096:(i+1)*4096]).digest()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(),
            'static': True, 'signaturePagesVerified': 4, 'loadCommands': commands}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True, type=Path)
    ap.add_argument('--mkinit', default='tools/mkinit.py', type=Path)
    ap.add_argument('--clang', default='clang')
    ap.add_argument('--prepare-only', action='store_true')
    a = ap.parse_args()
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    wrapper = a.mkinit.resolve()
    # Git's Windows checkout may use CRLF; pin the canonical Git source bytes.
    assert hashlib.sha256(wrapper.read_bytes().replace(b'\r\n', b'\n')).hexdigest() == MKINIT_SHA
    source = Path(__file__).with_name('ziyi-hfs-init.S')
    run([a.clang, '-target', 'arm64-apple-ios16.0', '-c', source, '-o', out/'init.o'])
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('mkinit', wrapper)
    mk = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mk)
    mk.IDENT = b'org.ziyi.hfs-ram-init\0'
    raw = mk.build((out/'init.o').read_bytes())
    executable = out/'ziyi-hfs-init'
    executable.write_bytes(raw)
    report = {'result': 'HOST_INIT_PREPARED', 'phoneAccess': False, 'appleAssets': False,
              'wrapperSha256': MKINIT_SHA, 'sourceSha256': sha(source), 'init': validate_init(raw)}
    if not a.prepare_only:
        assert sys.platform == 'darwin'
        writable = out/'writable.dmg'
        mount = out/'mount'
        mount.mkdir()
        run(['hdiutil', 'create', '-size', '32m', '-fs', 'HFSX', '-layout', 'NONE',
             '-volname', 'ZIYIRAMRoot', writable])
        mounted = False
        try:
            run(['hdiutil', 'attach', '-nobrowse', '-owners', 'on', '-mountpoint', mount, writable])
            mounted = True
            for name in ('sbin', 'dev', 'etc', 'private/var/ziyi', 'usr/lib'):
                run(['sudo', 'mkdir', '-p', mount/name])
            run(['sudo', 'install', '-o', '0', '-g', '0', '-m', '0755', executable, mount/'sbin/launchd'])
            token = out/'token'
            token.write_bytes(TOKEN)
            for name in ('etc/ziyi-root-check', 'private/var/ziyi/volume-check'):
                run(['sudo', 'install', '-o', '0', '-g', '0', '-m', '0644', token, mount/name])
            run(['sudo', 'ln', '-s', 'private/var', mount/'var'])
            run(['sudo', 'chown', '-R', '0:0', mount])
        finally:
            if mounted:
                run(['hdiutil', 'detach', mount])
        run(['hdiutil', 'convert', writable, '-format', 'UDTO', '-o', out/'ziyi-hfs-root'])
        image = out/'ziyi-hfs-root.cdr'
        assert image.stat().st_size == 32*1024*1024
        with image.open('rb') as f:
            f.seek(1024)
            assert f.read(2) == b'HX'
        # Native fsck operates on the raw device of this exact new image.
        # The attachment is read-only and unmounted; never guess a disk number.
        attached = run(['hdiutil', 'attach', '-readonly', '-nomount', '-plist', image], capture_output=True)
        devices = [e['dev-entry'] for e in plistlib.loads(attached.stdout)['system-entities'] if 'dev-entry' in e]
        assert len(devices) == 1 and re.fullmatch(r'/dev/disk[0-9]+', devices[0]), devices
        device = devices[0]
        try:
            check = subprocess.run(['sudo', '/sbin/fsck_hfs', '-f', '-n', device.replace('/dev/disk', '/dev/rdisk')], capture_output=True)
            (out/'fsck.txt').write_bytes(check.stdout+check.stderr)
            print((check.stdout+check.stderr).decode(errors='replace'), flush=True)
            check.check_returncode()
        finally:
            run(['hdiutil', 'detach', device])
        mounted = False
        try:
            run(['hdiutil', 'attach', '-readonly', '-nobrowse', '-owners', 'on', '-mountpoint', mount, image])
            mounted = True
            assert (mount/'sbin/launchd').read_bytes() == raw
            assert (mount/'etc/ziyi-root-check').read_bytes() == TOKEN
            assert (mount/'var').is_symlink() and (mount/'var').readlink() == Path('private/var')
            assert (mount/'var/ziyi/volume-check').read_bytes() == TOKEN
            for name in ('sbin/launchd', 'etc/ziyi-root-check', 'private/var/ziyi/volume-check'):
                s = (mount/name).stat()
                assert (s.st_uid, s.st_gid) == (0, 0)
            assert (mount/'sbin/launchd').stat().st_mode & 0o7777 == 0o755
        finally:
            if mounted:
                run(['hdiutil', 'detach', mount])
        report.update(result='HOST_HFS_ROOT_PASS', rootBytes=image.stat().st_size,
                      rootSha256=sha(image), rootFormat='bare HFSX, no partition map, 32 MiB',
                      nativeReadOnlyReread=True, nativeFsckPass=True, phoneTested=False)
    with (out/'audit.json').open('x') as f:
        json.dump(report, f, indent=2)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
