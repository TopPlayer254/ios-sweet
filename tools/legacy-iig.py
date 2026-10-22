#!/usr/bin/env python3
"""Replay pinned OPEN-SOURCE generated DriverKit bindings, never a newer IIG ABI.

The source/headers are compared with the old successful tree on EVERY call.
Unknown inputs/flags/paths fail. This is a host build tool, no device access.
"""
import hashlib,json,os,shutil,sys,tarfile
from pathlib import Path
TAR_SHA='1c7ee74a755a311c6e3b34bde7ba804e29b72295d2fd5c4d059df655290bd9ec'
PREFIX='build/xnu.obj/DEVELOPMENT_ARM64_SWEET/iokit/DEVELOPMENT/DriverKit/'
INPUTS=('xnu/iokit/','xnu/libkern/','xnu/osfmk/mach/','xnu/bsd/sys/')
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def prepare(archive,out):
    assert sha(archive)==TAR_SHA and not out.exists();out.mkdir()
    inputs={};outputs={};seen=set();size=0
    with tarfile.open(archive,'r:gz') as tar:
        for member in tar:
            name=member.name
            assert name not in seen;seen.add(name)
            if not member.isfile():continue
            selected=name.startswith(INPUTS)
            generated=name.startswith(PREFIX) and '/' not in name[len(PREFIX):] and (name.endswith('.h') or name.endswith('.iig.cpp'))
            if not selected and not generated:continue
            assert 0<=member.size<16<<20;size+=member.size;assert size<128<<20
            data=tar.extractfile(member).read();assert len(data)==member.size
            digest=hashlib.sha256(data).hexdigest()
            if selected:inputs[name[4:]]=digest
            if generated:
                leaf=name[len(PREFIX):];assert 'kernelContent' not in data.decode('utf-8')
                with (out/leaf).open('xb') as f:f.write(data)
                outputs[leaf]=digest
    definitions={Path(p).stem for p in inputs if p.startswith('iokit/DriverKit/') and p.endswith('.iig')}
    assert len(definitions)==17 and {'IOService','IOUserClient','IOEventLink'}<=definitions
    assert all(n+'.h' in outputs and n+'.iig.cpp' in outputs for n in definitions)
    manifest=dict(result='LEGACY_IIG_CACHE_PREPARED',archiveSha256=TAR_SHA,inputs=inputs,outputs=outputs,definitions=sorted(definitions))
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(out/'calls').mkdir()
    print(json.dumps(dict(result=manifest['result'],sourceFiles=len(inputs),generatedFiles=len(outputs),definitions=len(definitions))))
def invoke(args):
    cache=Path(os.environ['ZIYI_LEGACY_IIG_CACHE']).resolve();build=Path(os.environ['ZIYI_LEGACY_IIG_BUILD']).resolve()
    split=args.index('--');flags=args[:split]
    assert len(flags)==8 and flags[0]=='--def' and flags[2]=='--header' and flags[4]=='--impl' and flags[6:]==['--framework-name','DriverKit'],flags
    source=Path(flags[1]).resolve();assert source.parent.name=='DriverKit' and source.parent.parent.name=='iokit' and source.suffix=='.iig'
    root=source.parent.parent.parent;manifest=json.loads((cache/'manifest.json').read_text());assert manifest['archiveSha256']==TAR_SHA
    name=source.stem;assert name in manifest['definitions']
    # No inherited source/type declarations may drift while reusing generation.
    for p,digest in manifest['inputs'].items():assert sha(root/p)==digest,'Legacy IIG input changed: '+p
    result={}
    for raw,leaf in [(flags[3],name+'.h'),(flags[5],name+'.iig.cpp')]:
        target=Path(raw).resolve();assert target.is_relative_to(build) and target.name==leaf
        expected=manifest['outputs'][leaf];assert sha(cache/leaf)==expected
        if target.exists():assert sha(target)==expected
        else:
            target.parent.mkdir(parents=True,exist_ok=True)
            with (cache/leaf).open('rb') as src,target.open('xb') as dst:shutil.copyfileobj(src,dst)
        result[leaf]=expected
    receipt=cache/'calls'/(name+'.json')
    if not receipt.exists():
        with receipt.open('x') as f:json.dump(dict(source=name,outputs=result,legacyAbi=True),f)
    print('ZIYI LEGACY IIG',name,'source/header hashes PASS')
def audit(cache):
    m=json.loads((cache/'manifest.json').read_text());calls={p.stem for p in (cache/'calls').glob('*.json')}
    assert calls==set(m['definitions']),('Missing IIG calls',sorted(set(m['definitions'])-calls))
    for name in calls:
        proof=json.loads((cache/'calls'/(name+'.json')).read_text())
        assert proof['legacyAbi'] and all(m['outputs'][p]==v for p,v in proof['outputs'].items())
    print('LEGACY_IIG_FULL_AUDIT_PASS definitions='+str(len(calls)))
if __name__=='__main__':
    if sys.argv[1:2]==['--prepare']:prepare(Path(sys.argv[2]),Path(sys.argv[3]))
    elif sys.argv[1:2]==['--audit']:audit(Path(sys.argv[2]))
    else:invoke(sys.argv[1:])
