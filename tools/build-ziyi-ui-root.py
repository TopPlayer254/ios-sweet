#!/usr/bin/env python3
"""Build a new private 20H392 HFSX UI candidate on macOS; encrypt before upload.

Only fresh CI images are mounted. No Apple executable is run. No phone access.
Uses the audited Hack-iOS selection recipe, never its address-based injections.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import zlib

HERE=Path(__file__).resolve().parent
ROOT_BYTES=8*1024**3
CHUNK=4*1024**2
PINS={
    'build-ziyi-headless-root.py':'e907d1e1683824c671f21df8daad91c3208a907cb4231625b9b7d71fcecb9b9b',
    'audit-ziyi-apple-cache.py':'dfb735cda0e3a22cb424fe4dbec83c2090ad16335e0b901a05b352ec1db24bd9',
    'sweet-root-tweaks.py':'d604e9fd74f8344b87a351cf573346f4b8d2a3f73e9b3948cb720abcefd56398',
    'headless-root-public.pem':'154de3b312abbadd92485d34e428abc105394cdbad361ce58e3bda9f6304cc37',
    'mkinit.py':'79b8585105d4ecfc6f47542563577d0c55c22e7eaa517cffb367cf24e7c4e8ac',
}
SELECTOR_SHA='d8dd3562ec31b0e03de63b34f7c4f9aad6758e9f52763d9b993402ea4a6eecf1'


def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m


def run(args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)


def safe(root,name):
    p=Path(name)
    if p.is_absolute() or '..' in p.parts or not p.parts:raise ValueError('Unsafe image-relative path')
    dest=root/p
    # Image symlinks may be absolute. Never follow them onto the CI host.
    if not dest.parent.resolve().is_relative_to(root.resolve()):raise ValueError('Image path escapes via symlink: '+name)
    return dest


def parents(root,dest):
    relative=dest.parent.relative_to(root)
    for i in range(1,len(relative.parts)+1):
        p=safe(root,str(Path(*relative.parts[:i])))
        if p.is_symlink():raise ValueError('Refusing to traverse output symlink')
        p.mkdir(exist_ok=True)


def regular_target(root,name):
    p=safe(root,name)
    if p.is_symlink() or (p.exists() and not p.is_file()):raise ValueError('Expected regular output file')
    return p


def selftest():
    with tempfile.TemporaryDirectory() as temp:
        base=Path(temp);root=base/'image';root.mkdir();outside=base/'host';outside.mkdir()
        source=base/'source';source.write_bytes(b'own test data')
        marker=outside/'marker';marker.write_bytes(b'unchanged')
        (root/'escape').symlink_to(outside,target_is_directory=True)
        for name in ['../marker','/marker','escape/marker']:
            try:safe(root,name)
            except ValueError:pass
            else:raise AssertionError('Unsafe path accepted')
        (root/'direct').symlink_to(marker)
        try:regular_target(root,'direct')
        except ValueError:pass
        else:raise AssertionError('Direct symlink write accepted')
        try:copy_entry(source,root,'direct')
        except FileExistsError:pass
        else:raise AssertionError('Exclusive create followed symlink')
        copy_entry(source,root,'owned/file')
        assert (root/'owned/file').read_bytes()==source.read_bytes() and marker.read_bytes()==b'unchanged'
    print('UI_ROOT_PATH_SELFTEST_PASS')


def copy_entry(source,root,name):
    dest=safe(root,name);parents(root,dest);s=source.lstat()
    if stat.S_ISLNK(s.st_mode):
        if not os.path.lexists(dest):os.symlink(os.readlink(source),dest)
        else:assert dest.is_symlink() and os.readlink(dest)==os.readlink(source)
        os.chown(dest,s.st_uid,s.st_gid,follow_symlinks=False)
    elif stat.S_ISDIR(s.st_mode):
        if dest.is_symlink():raise ValueError('Directory collides with symlink')
        dest.mkdir(exist_ok=True);os.chown(dest,s.st_uid,s.st_gid);os.chmod(dest,stat.S_IMODE(s.st_mode))
    elif stat.S_ISREG(s.st_mode):
        expected=digest(source)
        with source.open('rb') as src,dest.open('xb') as dst:shutil.copyfileobj(src,dst,CHUNK)
        assert dest.stat().st_size==s.st_size and digest(dest)==expected
        os.chown(dest,s.st_uid,s.st_gid);os.chmod(dest,stat.S_IMODE(s.st_mode))
        os.utime(dest,ns=(s.st_atime_ns,s.st_mtime_ns))
    else:raise ValueError('Unexpected source node')


def walk(root):
    for directory,dirs,files in os.walk(root,followlinks=False):
        for name in sorted(dirs+files):
            p=Path(directory)/name;yield p,p.relative_to(root).as_posix(),p.lstat()


def os_selection(source,root,selector):
    meta={};staged=selector.STAGED_APPS.rstrip('/')
    for p,name,s in walk(source):
        relocated=name==staged or name.startswith(staged+'/')
        mapped='Applications'+name[len(staged):] if relocated else name
        seed=name.startswith('private/var/') and not relocated
        # The recipe moves staged system apps into Applications; prefer them
        # deterministically if an OS placeholder has the same destination.
        if mapped not in meta or relocated:meta[mapped]=(p,s,seed)
    chosen=set();regular=seed_files=0
    for mapped,(p,s,seed) in meta.items():
        if seed:
            keep=True
            if stat.S_ISREG(s.st_mode):seed_files+=1
        elif stat.S_ISREG(s.st_mode):
            keep=selector.wanted_file(mapped,s.st_size)
            if keep:regular+=1
        elif stat.S_ISLNK(s.st_mode):
            keep='/' not in mapped or mapped.startswith(selector.WHOLE) and not mapped.startswith(selector.SKIP_PREFIX)
        elif stat.S_ISDIR(s.st_mode):
            lang=re.search(r'/([^/]+)\.lproj$',mapped)
            keep='/' not in mapped or mapped in ('private','private/var','private/tmp','private/etc','private/preboot','private/xarts') or bool(lang) and lang.group(1) in selector.KEEP_LANG and (mapped.startswith(selector.WHOLE) and not mapped.startswith(selector.SKIP_PREFIX) or mapped.startswith(selector.AUDIO) and not mapped.startswith(selector.SKIP_AUDIO))
        else:continue
        if keep:chosen.add(mapped)
    for name in list(chosen):
        while '/' in name:
            name=name.rsplit('/',1)[0]
            if name in meta:chosen.add(name)
    for name in sorted(chosen,key=lambda p:(p.count('/'),p)):
        copy_entry(meta[name][0],root,name)
    assert regular==43353 and seed_files==36,(regular,seed_files)
    return regular,seed_files


def cryptex(source,root,label):
    prefix='private/preboot/Cryptexes/'+label
    for p,name,s in walk(source):
        if not (stat.S_ISREG(s.st_mode) or stat.S_ISDIR(s.st_mode) or stat.S_ISLNK(s.st_mode)):continue
        copy_entry(p,root,prefix+'/'+name)
        if stat.S_ISDIR(s.st_mode):continue
        alias=safe(root,name)
        if os.path.lexists(alias):continue
        parents(root,alias)
        stored=safe(root,prefix+'/'+name)
        if stored.is_symlink():os.symlink(os.readlink(stored),alias);os.chown(alias,s.st_uid,s.st_gid,follow_symlinks=False)
        else:os.link(stored,alias)


def inventory(root):
    rows=[]
    for p,name,s in walk(root):
        row=dict(path=name,mode=stat.S_IMODE(s.st_mode),uid=s.st_uid,gid=s.st_gid)
        if stat.S_ISLNK(s.st_mode):row.update(type='link',target=os.readlink(p))
        elif stat.S_ISREG(s.st_mode):row.update(type='file',bytes=s.st_size,sha256=digest(p))
        elif stat.S_ISDIR(s.st_mode):row.update(type='directory')
        else:raise ValueError('Unexpected output node')
        rows.append(row)
    return sorted(rows,key=lambda r:r['path'])


def main():
    assert sys.platform=='darwin' and os.geteuid()==0 and len(sys.argv)==3
    from cryptography.hazmat.primitives import hashes,serialization
    from cryptography.hazmat.primitives.asymmetric import padding
    from cryptography.hazmat.primitives.ciphers import Cipher,algorithms,modes
    out=Path(sys.argv[1]).resolve();assert not out.exists();out.mkdir()
    selector=Path(sys.argv[2]).resolve();assert hashlib.sha256(selector.read_text().encode()).hexdigest()==SELECTOR_SHA
    for name,expected in PINS.items():assert hashlib.sha256((HERE/name).read_text().encode()).hexdigest()==expected,name
    recipe=module('ui_selector',selector);base=module('headless',HERE/'build-ziyi-headless-root.py')
    mount=out/'mount-root';mount.mkdir();image=out/'ui.sparseimage'
    run(['hdiutil','create','-size','8192m','-type','SPARSE','-fs','HFSX','-layout','NONE','-volname','ZIYIUI',image])
    run(['hdiutil','attach','-nobrowse','-owners','on','-mountpoint',mount,image])
    report=dict(target='iPhone10,3 D22AP / 16.7.16 20H392',phoneAccess=False,appleCodeExecuted=False,
                rootBytes=ROOT_BYTES,selectionCommit='906425b75e02b4f99eb5d7bb1ef7c9583472d772',
                addressBasedInjections=False,workflowCommit=os.environ.get('GITHUB_SHA'))
    try:
        jobs=[('OS',base.OS_NAME,1191497236,0,4730814044,4730814044,0x66ccd39f,base.OS_SHA),
              ('SystemOS',base.cache.NAME,0,8,base.cache.COMPRESSED,base.cache.SIZE,base.cache.CRC,base.cache.SHA384),
              ('AppOS','087-86715-047.dmg',5922311347,8,2381197,10485760,0xed284220,'853c4f945ea09b1614bedeaffc777aefdf2c5bcb15f1a66986e965207757246c2d3f8712a5a5c491cfc49a5a45e81502')]
        for label,*job in jobs:
            source=base.download(out,*job)
            # Only newly created HTTP cache files, after complete CRC/SHA/reread.
            temp=out/('network-'+job[0]);assert temp.parent==out and not temp.is_symlink();shutil.rmtree(temp)
            srcmount=out/('mount-'+label);srcmount.mkdir()
            run(['hdiutil','attach','-readonly','-nobrowse','-owners','on','-mountpoint',srcmount,source])
            try:
                if label=='OS':
                    version=plistlib.loads((srcmount/'System/Library/CoreServices/SystemVersion.plist').read_bytes())
                    assert (version['ProductVersion'],version['ProductBuildVersion'])==('16.7.16','20H392')
                    report['selectedFiles'],report['varSeedFiles']=os_selection(srcmount,mount,recipe)
                else:cryptex(srcmount,mount,'OS' if label=='SystemOS' else 'App')
            finally:run(['hdiutil','detach',srcmount])
            assert source.parent==out and source.name==job[0];source.unlink()
        for directory,uid,mode in [('private/var/mobile/Library/Preferences',501,0o755),('private/var/db/diagnostics',272,0o751),('private/var/db/uuidtext',272,0o755),('private/var/db/diagnostics/timesync',272,0o755),('private/var/keybags',0,0o700)]:
            dest=safe(mount,directory);parents(mount,dest);dest.mkdir(exist_ok=True);os.chown(dest,uid,uid);os.chmod(dest,mode)
        for directory in ['diagnostics','uuidtext','diagnostics/timesync']:
            p=regular_target(mount,'private/var/db/'+directory+'/.migrated');p.touch(exist_ok=True);os.chown(p,272,272);os.chmod(p,0o644)
        keybag=regular_target(mount,'private/var/keybags/systembag.kb')
        keybag.write_bytes(plistlib.dumps({'KeyBagKeys':b'SWEET'}));os.chown(keybag,0,0);os.chmod(keybag,0o600)
        run([sys.executable,HERE/'sweet-root-tweaks.py',mount])
        mk=module('mkinit',HERE/'mkinit.py');mk.IDENT=b'org.ziyi.launchd-child\0'
        run(['clang','-target','arm64-apple-ios16.0','-c',HERE/'ziyi-launchd-child.S','-o',out/'child.o'])
        child=out/'ziyi-launchd-child';child.write_bytes(mk.build((out/'child.o').read_bytes()));os.chmod(child,0o755)
        copy_entry(child,mount,'usr/libexec/ziyi-launchd-child')
        job=regular_target(mount,'System/Library/LaunchDaemons/org.ziyi.launchd-child.plist');parents(mount,job)
        job.write_bytes(plistlib.dumps({'Label':'org.ziyi.launchd-child','ProgramArguments':['/usr/libexec/ziyi-launchd-child'],'RunAtLoad':True,'KeepAlive':False}));os.chown(job,0,0);os.chmod(job,0o644)
        # Enable the journal on this NEW image only; assert its header below.
        run(['diskutil','enableJournal',mount])
        before=inventory(mount)
        required=['sbin/launchd','usr/lib/dyld','usr/libexec/backboardd','System/Library/CoreServices/SpringBoard.app/SpringBoard','Applications/PosterBoard.app/PosterBoard','System/Library/Frameworks/QuartzCore.framework/default.metallib']
        assert all(safe(mount,p).is_file() for p in required)
        cache_dir=safe(mount,'System/Library/Caches/com.apple.dyld')
        assert len(list(cache_dir.glob('dyld_shared_cache_arm64*')))==43
        report.update(requiredFilesPresent=True,cacheFiles=43,files=len(before),nativeInventory=True)
    finally:run(['hdiutil','detach',mount])
    run(['hdiutil','attach','-readonly','-nobrowse','-owners','on','-mountpoint',mount,image])
    try:assert inventory(mount)==before
    finally:run(['hdiutil','detach',mount])
    attach=run(['hdiutil','attach','-readonly','-nomount','-plist',image],capture_output=True)
    devices=[e['dev-entry'] for e in plistlib.loads(attach.stdout)['system-entities'] if 'dev-entry' in e]
    assert len(devices)==1 and re.fullmatch(r'/dev/disk[0-9]+',devices[0])
    device=devices[0].replace('/dev/disk','/dev/rdisk')
    try:
        device_info=plistlib.loads(run(['diskutil','info','-plist',devices[0]],capture_output=True).stdout)
        assert device_info['TotalSize']==ROOT_BYTES
        check=run(['/sbin/fsck_hfs','-f','-n',device],capture_output=True);(out/'fsck.txt').write_bytes(check.stdout+check.stderr)
        with open(device,'rb',buffering=0) as f:f.seek(1024);header=f.read(512)
        assert header[:4]==b'HX\x00\x05' and struct.unpack_from('>I',header,4)[0]&0x2000
        assert struct.unpack_from('>I',header,40)[0]*struct.unpack_from('>I',header,44)[0]==ROOT_BYTES
        public=serialization.load_pem_public_key((HERE/'headless-root-public.pem').read_bytes());key=os.urandom(32);nonce=os.urandom(12)
        wrapped=public.encrypt(key,padding.OAEP(mgf=padding.MGF1(hashes.SHA256()),algorithm=hashes.SHA256(),label=None))
        envelope=b'ZYROOT01'+wrapped+nonce;enc=Cipher(algorithms.AES(key),modes.GCM(nonce)).encryptor();enc.authenticate_additional_data(envelope)
        compressor=zlib.compressobj(6,zlib.DEFLATED,31);h=hashlib.sha256();total=0;artifact=out/'ziyi-ui-root.enc'
        with open(device,'rb',buffering=0) as src,artifact.open('xb') as dst:
            dst.write(envelope)
            while total<ROOT_BYTES:
                data=src.read(min(CHUNK,ROOT_BYTES-total));assert data and len(data)%4096==0
                total+=len(data);h.update(data);dst.write(enc.update(compressor.compress(data)))
            assert total==ROOT_BYTES
            dst.write(enc.update(compressor.flush()));dst.write(enc.finalize());dst.write(enc.tag)
        report.update(result='HOST_UI_ROOT_PASS_NOT_PHONE_TESTED',rootSha256=h.hexdigest(),artifactSha256=digest(artifact),artifactBytes=artifact.stat().st_size,nativeReadOnlyReread=True,nativeFsckPass=True,journaledHFSX=True)
    finally:run(['hdiutil','detach',devices[0]])
    (out/'inventory.json').write_text(json.dumps(before,ensure_ascii=False)+'\n')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':
    if sys.argv[1:]==['--selftest']:selftest()
    else:main()
