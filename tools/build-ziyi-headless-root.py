#!/usr/bin/env python3
"""Build a 3GiB HFSX REAL launchd/cache probe root on macOS, encrypt artifact.

Only new local images are mounted. Apple binaries are read/copied, not executed.
This is a headless probe with OWN no-op fsck/mount fixtures and one own daemon;
it is not a complete iOS system or a PosterBoard-ready filesystem.
"""
from concurrent.futures import ThreadPoolExecutor
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import sys
import zlib
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('cache', HERE/'audit-ziyi-apple-cache.py')
cache=importlib.util.module_from_spec(spec);spec.loader.exec_module(cache)
CHUNK=4<<20
ROOT_BYTES=3<<30
OS_NAME='087-86698-044.dmg'
OS_SHA='103b835176572a97ae36bc5ebe4c332c40b921e1f151b844116bbda49959c46c64364d343ceb0c4e9d95a51ba60d0e51'
APPLE={
 'sbin/launchd': '12370e5ee35bf28ae479b6c4b4455f4f4693ab803e37c5eb3032537c18cac61c',
 'usr/lib/dyld': '5806834564fe6462ae981d0915d612b1eb2f6f168822b5d278bee56254933757',
}


def run(args, **kw): return subprocess.run(list(map(str,args)),check=True,**kw)
def digest(path,kind='sha256'):
    with path.open('rb') as f:return hashlib.file_digest(f,kind).hexdigest()


def download(out,name,offset,method,csize,size,crc,expected):
    network=out/('network-'+name);network.mkdir()
    header=cache.get(network,offset,30)
    assert header[:4]==b'PK\x03\x04' and struct.unpack_from('<H',header,8)[0]==method
    flags=struct.unpack_from('<H',header,6)[0];assert not flags&1
    nlen,xlen=struct.unpack_from('<2H',header,26)
    assert 0<nlen<1024 and xlen<65536 and cache.get(network,offset+30,nlen)==name.encode()
    start=offset+30+nlen+xlen;n=(csize+CHUNK-1)//CHUNK
    decoder=zlib.decompressobj(-15) if method==8 else None
    image=out/name;h=hashlib.sha384();checkcrc=written=0
    with ThreadPoolExecutor(max_workers=4) as pool,image.open('xb') as f:
        def submit(i):return pool.submit(cache.get,network,start+i*CHUNK,min(CHUNK,csize-i*CHUNK))
        pending={i:submit(i) for i in range(min(4,n))}
        for i in range(n):
            b=pending.pop(i).result()
            if i+4<n:pending[i+4]=submit(i+4)
            raw=decoder.decompress(b) if decoder else b
            assert written+len(raw)<=size
            f.write(raw);h.update(raw);checkcrc=zlib.crc32(raw,checkcrc);written+=len(raw)
            if i%64==0:print(name,i+1,'/',n,flush=True)
        if decoder:
            tail=decoder.flush();f.write(tail);h.update(tail);checkcrc=zlib.crc32(tail,checkcrc);written+=len(tail)
            assert decoder.eof and not decoder.unused_data
    assert written==size and checkcrc==crc and h.hexdigest()==expected
    assert digest(image,'sha384')==expected
    return image


def copy_file(source,dest,mode=0o755):
    expected=digest(source)
    run(['sudo','mkdir','-p',dest.parent])
    # Decompress APFS resource-fork files through normal reads; don't copy their
    # UF_COMPRESSED flags/xattrs to a new HFS volume without resource forks.
    with source.open('rb') as src,subprocess.Popen(['sudo','tee',str(dest)],stdin=subprocess.PIPE,stdout=subprocess.DEVNULL) as p:
        shutil.copyfileobj(src,p.stdin,CHUNK);p.stdin.close();assert p.wait()==0
    run(['sudo','chown','0:0',dest]);run(['sudo','chmod',f'{mode:o}',dest])
    assert digest(dest)==expected
    return {'path':str(dest).split('/mount-root/')[-1], 'bytes':source.stat().st_size,'sha256':expected}


def main():
    assert sys.platform=='darwin' and len(sys.argv)==2
    out=Path(sys.argv[1]).resolve();out.mkdir(exist_ok=False)
    report={'phoneAccess':False,'appleCodeExecuted':False,'rootBytes':ROOT_BYTES,
            'scope':'headless Apple launchd probe; own no-op fsck/mount fixtures; no GUI or stock jobs'}
    # Fetch OS first, retain only two independently verified files, then release
    # this newly downloaded local DMG before building the large root.
    os_image=download(out,OS_NAME,1191497236,0,4730814044,4730814044,0x66ccd39f,OS_SHA)
    osmount=out/'mount-os';osmount.mkdir();selected=out/'selected';selected.mkdir()
    run(['hdiutil','attach','-readonly','-nobrowse','-mountpoint',osmount,os_image])
    try:
        for name,sha in APPLE.items():
            src=osmount/name;assert digest(src)==sha
            dest=selected/Path(name).name
            with src.open('rb') as s,dest.open('xb') as d:shutil.copyfileobj(s,d,CHUNK)
            assert digest(dest)==sha
    finally:run(['hdiutil','detach',osmount])
    assert os_image.parent==out and digest(os_image,'sha384')==OS_SHA
    os_image.unlink() # only our new CI download; immutable local evidence untouched
    system=download(out,cache.NAME,0,8,cache.COMPRESSED,cache.SIZE,cache.CRC,cache.SHA384)
    systemmount=out/'mount-system';systemmount.mkdir()
    writable=out/'headless-writable.dmg';mount=out/'mount-root';mount.mkdir()
    run(['hdiutil','create','-size','3072m','-fs','HFSX','-layout','NONE','-volname','ZIYIAppleProbe',writable])
    run(['hdiutil','attach','-readonly','-nobrowse','-mountpoint',systemmount,system])
    run(['hdiutil','attach','-nobrowse','-owners','on','-mountpoint',mount,writable])
    rows=[]
    try:
        for d in ['dev','sbin','usr/lib','usr/libexec','private/etc','private/var/db','private/var/run','private/var/tmp','private/var/log','System/Library/LaunchDaemons','Library/LaunchDaemons']:
            run(['sudo','mkdir','-p',mount/d])
        for alias,target in [('etc','private/etc'),('var','private/var'),('tmp','private/var/tmp')]:run(['sudo','ln','-s',target,mount/alias])
        run(['sudo','chmod','1777',mount/'private/var/tmp'])
        for name,sha in APPLE.items():rows.append(copy_file(selected/Path(name).name,mount/name))
        cache_root=systemmount/'System/Library/Caches/com.apple.dyld'
        base=(cache_root/'dyld_shared_cache_arm64').read_bytes()
        assert hashlib.sha256(base).hexdigest()=='76874e8375252ee643011ff8063f0cce12355f05cf5f9897a34e5679667a65aa'
        offset,count=struct.unpack_from('<2I',base,392);assert count==42
        names=[('dyld_shared_cache_arm64',base[88:104])]
        for i in range(count):
            uuid,vm,suffix=struct.unpack_from('<16sQ32s',base,offset+i*56)
            name='dyld_shared_cache_arm64'+suffix.split(b'\0')[0].decode()
            assert re.fullmatch(r'dyld_shared_cache_arm64\.\d+(?:\.dyld(?:data|linkedit))?',name)
            names.append((name,uuid))
        for name,uuid in names:
            src=cache_root/name
            with src.open('rb') as f:head=f.read(104)
            assert head[88:104]==uuid
            rows.append(copy_file(src,mount/'System/Library/Caches/com.apple.dyld'/name))
        assert sum(r['bytes'] for r in rows[2:])==2738257920
        wrapper=Path('tools/mkinit.py').resolve()
        assert hashlib.sha256(wrapper.read_bytes().replace(b'\r\n',b'\n')).hexdigest()=='79b8585105d4ecfc6f47542563577d0c55c22e7eaa517cffb367cf24e7c4e8ac'
        spec=importlib.util.spec_from_file_location('mkinit',wrapper);mk=importlib.util.module_from_spec(spec);spec.loader.exec_module(mk)
        run(['clang','-target','arm64-apple-ios16.0','-c',HERE/'ziyi-launchd-child.S','-o',out/'child.o'])
        mk.IDENT=b'org.ziyi.launchd-child\0';child=out/'ziyi-launchd-child';child.write_bytes(mk.build((out/'child.o').read_bytes()))
        rows.append(copy_file(child,mount/'usr/libexec/ziyi-launchd-child'))
        run(['clang','-target','arm64-apple-ios16.0','-DZIYI_BOOT_FIXTURE','-c',HERE/'ziyi-launchd-child.S','-o',out/'boot-task.o'])
        mk.IDENT=b'org.ziyi.boot-task-fixture\0';task=out/'ziyi-boot-task';task.write_bytes(mk.build((out/'boot-task.o').read_bytes()))
        rows.append(copy_file(task,mount/'usr/libexec/ziyi-boot-task'))
        for name in ['fsck','mount']:run(['sudo','ln','-s','../usr/libexec/ziyi-boot-task',mount/'sbin'/name])
        plist={'Label':'org.ziyi.launchd-child','ProgramArguments':['/usr/libexec/ziyi-launchd-child'],'RunAtLoad':True,'KeepAlive':False}
        fixture=out/'org.ziyi.launchd-child.plist';fixture.write_bytes(plistlib.dumps(plist))
        rows.append(copy_file(fixture,mount/'System/Library/LaunchDaemons/org.ziyi.launchd-child.plist',0o644))
        report.update(files=rows,cacheFiles=43,cacheBytes=2738257920,allSubcacheUuidsMatch=True)
    finally:
        run(['hdiutil','detach',mount]);run(['hdiutil','detach',systemmount])
    run(['hdiutil','convert',writable,'-format','UDTO','-o',out/'ziyi-headless-root'])
    image=out/'ziyi-headless-root.cdr';assert image.stat().st_size==ROOT_BYTES
    attached=run(['hdiutil','attach','-readonly','-nomount','-plist',image],capture_output=True)
    devices=[e['dev-entry'] for e in plistlib.loads(attached.stdout)['system-entities'] if 'dev-entry' in e]
    assert len(devices)==1 and re.fullmatch(r'/dev/disk[0-9]+',devices[0])
    try:
        check=run(['sudo','/sbin/fsck_hfs','-f','-n',devices[0].replace('/dev/disk','/dev/rdisk')],capture_output=True)
        (out/'fsck.txt').write_bytes(check.stdout+check.stderr)
    finally:run(['hdiutil','detach',devices[0]])
    run(['hdiutil','attach','-readonly','-nobrowse','-owners','on','-mountpoint',mount,image])
    try:
        for row in rows:
            f=mount/row['path'];assert f.stat().st_size==row['bytes'] and digest(f)==row['sha256']
            assert (f.stat().st_uid,f.stat().st_gid)==(0,0)
        assert (mount/'sbin/fsck').is_symlink() and (mount/'var').is_symlink()
    finally:run(['hdiutil','detach',mount])
    rootsha=digest(image);report.update(rootSha256=rootsha,nativeReadOnlyReread=True,nativeFsckPass=True)
    # Envelope: magic, RSA-wrapped AES256 key (384B), nonce12, ciphertext(gzip), tag16.
    # The private key is only on the user's PC. Never upload plaintext Apple assets.
    public=serialization.load_pem_public_key((HERE/'headless-root-public.pem').read_bytes())
    key=os.urandom(32);nonce=os.urandom(12)
    wrapped=public.encrypt(key,padding.OAEP(mgf=padding.MGF1(hashes.SHA256()),algorithm=hashes.SHA256(),label=None))
    header=b'ZYROOT01'+wrapped+nonce;enc=Cipher(algorithms.AES(key),modes.GCM(nonce)).encryptor();enc.authenticate_additional_data(header)
    artifact=out/'ziyi-headless-root.enc'
    compressor=zlib.compressobj(6,zlib.DEFLATED,31)
    with artifact.open('xb') as f,image.open('rb') as src:
        f.write(header)
        for block in iter(lambda:src.read(CHUNK),b''):f.write(enc.update(compressor.compress(block)))
        f.write(enc.update(compressor.flush()));f.write(enc.finalize());f.write(enc.tag)
    report.update(result='HOST_HEADLESS_ROOT_PASS_NOT_PHONE_TESTED',encryption='RSA3072-OAEP-SHA256 + AES256-GCM, gzip6',
                  artifactBytes=artifact.stat().st_size,artifactSha256=digest(artifact))
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='files'},indent=2))


if __name__=='__main__':main()
