"""macOS-only read-only Apple cache sizing; upload reports, NEVER Apple assets.

Downloads only the ZIP's SystemOS member, verifies ZIP CRC + manifest SHA384
and an independent image reread, attaches the NEW local file read-only and
measures gzip transport bytes with an independent decode/hash round-trip.
No Apple code is executed and no phone connection is used.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import zlib

URL='https://updates.cdn-apple.com/2025FallFCS/fullrestores/122-77006/1158E502-D76D-4226-AA06-ED5E30CD1111/iPhone10,3,iPhone10,6_16.7.16_20H392_Restore.ipsw'
NAME='087-86593-047.dmg'
COMPRESSED=1097727251
SIZE=2743074816
CRC=0x7f8fabcd
SHA384='775e110985ba8fd0dd2ad21d3fd5a896eec8887af219fc0dbc1e55ace791de1d51ecca2477f2e650b24c557cb74a0627'
CHUNK=4*1024*1024


def get(out,start,count):
    data=bytearray();failures=0
    while len(data)<count:
        pos=start+len(data);end=start+count-1
        tag=f'{start}-{count}-{len(data)}-{failures}'
        body=out/('body-'+tag);header=out/('header-'+tag)
        r=subprocess.run(['curl','-4','-sSL','--max-time','45','--max-filesize',str(end-pos+1),
            '-r',f'{pos}-{end}','-D',str(header),'-o',str(body),URL],capture_output=True)
        text=header.read_text(errors='replace') if header.exists() else ''
        ranges=re.findall(r'(?im)^content-range:\s*bytes\s+(\d+)-(\d+)/(\d+)\s*$',text)
        valid=ranges and tuple(map(int,ranges[-1][:2]))==(pos,end)
        b=body.read_bytes() if body.exists() else b''
        if valid and 0<len(b)<=end-pos+1 and r.returncode in (0,18,28):data.extend(b)
        else:
            failures+=1
            if failures>=5:raise RuntimeError(f'range failed: {pos}+{end-pos+1}, curl={r.returncode}')
            time.sleep(min(2**failures,8))
    return bytes(data)


def main():
    assert sys.platform=='darwin' and len(sys.argv)==2
    out=Path(sys.argv[1]).resolve();out.mkdir(exist_ok=False)
    network=out/'network';network.mkdir()
    header=get(network,0,30)
    assert header[:4]==b'PK\x03\x04'
    flags,method=struct.unpack_from('<2H',header,6)
    assert not flags&1 and method==8
    nlen,xlen=struct.unpack_from('<2H',header,26)
    assert nlen<1024 and xlen<65536 and get(network,30,nlen)==NAME.encode()
    start=30+nlen+xlen;n=(COMPRESSED+CHUNK-1)//CHUNK
    image=out/NAME;decoder=zlib.decompressobj(-15);h=hashlib.sha384();crc=0;written=0
    with ThreadPoolExecutor(max_workers=4) as pool,image.open('xb') as f:
        def submit(i):return pool.submit(get,network,start+i*CHUNK,min(CHUNK,COMPRESSED-i*CHUNK))
        pending={i:submit(i) for i in range(min(4,n))}
        for i in range(n):
            b=pending.pop(i).result()
            if i+4<n:pending[i+4]=submit(i+4)
            raw=decoder.decompress(b);assert written+len(raw)<=SIZE
            f.write(raw);h.update(raw);crc=zlib.crc32(raw,crc);written+=len(raw)
            if i%16==0:print(f'SystemOS compressed range {i+1}/{n}',flush=True)
        tail=decoder.flush();f.write(tail);h.update(tail);crc=zlib.crc32(tail,crc);written+=len(tail)
    assert decoder.eof and not decoder.unused_data and written==SIZE
    assert h.hexdigest()==SHA384 and crc==CRC
    h=hashlib.sha384()
    with image.open('rb') as f:
        for b in iter(lambda:f.read(CHUNK),b''):h.update(b)
    assert h.hexdigest()==SHA384
    mount=out/'mount';mount.mkdir();mounted=False
    report={'phoneAccess':False,'appleCodeExecuted':False,'imageSha384':SHA384,'zipCrc32':f'{CRC:08x}',
            'independentImageReread':True,'source':URL,'component':NAME}
    try:
        subprocess.run(['hdiutil','attach','-readonly','-nobrowse','-mountpoint',str(mount),str(image)],check=True)
        mounted=True
        root=mount/'System/Library/Caches/com.apple.dyld'
        base=(root/'dyld_shared_cache_arm64').read_bytes()
        assert hashlib.sha256(base).hexdigest()=='76874e8375252ee643011ff8063f0cce12355f05cf5f9897a34e5679667a65aa'
        off,count=struct.unpack_from('<2I',base,392);assert count==42
        names=[('dyld_shared_cache_arm64',base[88:104].hex())]
        for i in range(count):
            uuid,vm,suffix=struct.unpack_from('<16sQ32s',base,off+i*56)
            name='dyld_shared_cache_arm64'+suffix.split(b'\0')[0].decode()
            assert re.fullmatch(r'dyld_shared_cache_arm64\.\d+(?:\.dyld(?:data|linkedit))?',name)
            names.append((name,uuid.hex()))
        encoder=zlib.compressobj(6,zlib.DEFLATED,31);check=zlib.decompressobj(31)
        rawsha=hashlib.sha256();packsha=hashlib.sha256();checksha=hashlib.sha256()
        rawtotal=packedtotal=checkedtotal=0;rows=[]
        def packed(b):
            nonlocal packedtotal,checkedtotal
            packsha.update(b);packedtotal+=len(b)
            decoded=check.decompress(b);checksha.update(decoded);checkedtotal+=len(decoded)
        for name,uuid in names:
            file=root/name;before=file.stat();sha=hashlib.sha256()
            with file.open('rb') as f:head=f.read(104)
            assert head[:16]==b'dyld_v1   arm64\0' and head[88:104].hex()==uuid
            with file.open('rb') as f:
                for b in iter(lambda:f.read(CHUNK),b''):
                    sha.update(b);rawsha.update(b);rawtotal+=len(b);packed(encoder.compress(b))
            assert (before.st_size,before.st_mtime_ns)==(file.stat().st_size,file.stat().st_mtime_ns)
            rows.append({'file':name,'bytes':before.st_size,'sha256':sha.hexdigest(),'uuid':uuid})
            print(name,'verified/compressed',len(rows),'/43',flush=True)
        packed(encoder.flush());rest=check.flush();checksha.update(rest);checkedtotal+=len(rest)
        assert check.eof and not check.unused_data and rawtotal==checkedtotal==2738257920
        assert checksha.digest()==rawsha.digest()
        report.update(result='NATIVE_CACHE_TRANSPORT_PASS',files=rows,rawBytes=rawtotal,
            gzipLevel=6,gzipBytes=packedtotal,gzipSha256=packsha.hexdigest(),aggregateRawSha256=rawsha.hexdigest(),
            allSubcacheUuidsMatch=True,roundTripVerified=True,fits512MiBStaging=packedtotal<=512*1024*1024,
            caveat='Byte-concatenation sizing only; no root image, native inflater or enlarged phone arena validated.')
    finally:
        if mounted:subprocess.run(['hdiutil','detach',str(mount)],check=True)
    with (out/'report.json').open('x') as f:json.dump(report,f,indent=2)
    print(json.dumps({k:v for k,v in report.items() if k!='files'},indent=2))


if __name__=='__main__':main()
