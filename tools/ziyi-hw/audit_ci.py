"""Mandatory exact-source and nonempty linked adapter audit after full Apple CI."""
from pathlib import Path
import hashlib,json,os,re,struct,subprocess
ROOT=Path.cwd();KIT=ROOT/'tools/ziyi-hw';XNU=ROOT/'dxb/xnu';OUT=ROOT/'u1-evidence/hw-in-system'
OUT.mkdir(parents=True,exist_ok=True)
manifest=json.loads((KIT/'manifest.json').read_text());base=json.loads((KIT/'base-u25-manifest.json').read_text());full=json.loads((ROOT/'tools/ziyi-ufs/manifest.json').read_text())
sha=lambda b:hashlib.sha256(b).hexdigest()
assert len(base)==31 and len(full)==57
unchanged=[]
for name,row in base.items():
    if name=='bsd/kern/bsd_init.c':continue
    assert full[name]==row,(name,'baseline manifest changed')
    assert sha((XNU/name).read_bytes())==row['after'],name
    unchanged.append(name)
assert len(unchanged)==30
checks=[]
for row in manifest['sources']:
    source=ROOT/row['repository'];target=XNU/row['target'];a=source.read_bytes();b=target.read_bytes()
    assert a==b and sha(b)==row['sha256'],row['target']
    checks.append(dict(path=row['target'],sha256=sha(b),bytes=len(b)))
hook=manifest['startupHook'];text=(XNU/hook['path']).read_bytes();block=hook['insert'].encode()
assert text.count(block)==1 and sha(text.replace(block,b'',1))==base[hook['path']]['after']
for name in ['ziyi_gpu_probe_start','ziyi_wifi_probe_start']:
    assert text.count((name+'();').encode())==1
filelist=(XNU/'bsd/conf/files.arm64').read_bytes();assert sha(filelist)==manifest['fileListAfter']
for name in manifest['compileUnits']:assert filelist.decode().count(name+' standard')==1
for row in manifest['publicFiles']:assert sha((ROOT/row['path']).read_bytes())==row['sha256'],row['path']
kernel=ROOT/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet';data=kernel.read_bytes()
assert data[:4]==bytes.fromhex('cffaedfe'),'Expected plain ARM64 Mach-O'
for marker in manifest['linkedMarkers']:assert marker.encode() in data,marker
nm=subprocess.run(['nm',str(kernel)],capture_output=True,text=True,check=True).stdout
(OUT/'kernel-symbols.txt').write_text(nm)
symbols={}
for line in nm.splitlines():
    m=re.match(r'^([0-9a-fA-F]+)\s+([Tt])\s+(\S+)$',line)
    if m:symbols[m[3]]=int(m[1],16)
segments=[];at=32
for _ in range(struct.unpack_from('<I',data,16)[0]):
    cmd,size=struct.unpack_from('<II',data,at);assert size>=8 and at+size<=len(data)
    if cmd==0x19:
        va,vs,off,fs=struct.unpack_from('<QQQQ',data,at+24);segments.append((va,vs,off,fs))
    at+=size
linked=[]
for name in manifest['linkedFunctions']:
    assert name in symbols,name
    address=symbols[name];following=sorted({v for v in symbols.values() if v>address});assert following
    length=following[0]-address;assert length>=32,(name,length,'empty/inlined hook')
    candidates=[s for s in segments if s[0]<=address<s[0]+s[3]];assert len(candidates)==1
    va,vs,offset,filesize=candidates[0];start=offset+address-va;sample=data[start:start+min(length,128)]
    assert len(sample)>=32 and any(sample) and sample[:4]!=bytes.fromhex('c0035fd6'),name
    linked.append(dict(name=name,address=hex(address),bytesToNextTextSymbol=length,sampleSHA256=sha(sample)))
report=dict(result='ZIYI_HW_IN_SYSTEM_FULL_CI_SOURCE_AND_LINK_PASS',commit=os.environ['GITHUB_SHA'],kernelSHA256=sha(data),preservedU25Bodies=unchanged,newSources=checks,linkedFunctions=linked,markers=manifest['linkedMarkers'],phoneTested=False,GPUAcceleration=False,WiFiRadio=False)
(OUT/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
