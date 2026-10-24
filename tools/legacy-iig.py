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
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1<<20),b''):h.update(block)
    return h.hexdigest()
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
# This private framebuffer implementation is not an IIG declaration/input
# dependency. Its reviewed rendering-only change is pinned on both sides;
# every other inherited source/header and every generated RPC byte stays exact.
IMPLEMENTATION_OVERRIDE={'iokit/Kernel/arm/SweetFramebuffer.cpp': ('aa561c09542267087427deee903fb7afacb20e2265044a8e1b12586b5337565f', '8d7ca2c7c10347ce497f8b5f6da0117c6aa2061b408cb6bbe8eae27d7cd32f57'), 'iokit/Kernel/arm/hid/SweetHID.cpp': ('cb8afe01c324d084d95e46bfe42575a2e1200f07155326bdd1448ee4c71ccb8a', '330eacf65469f17d73eac782a2b936c36ba6a4766035a08fb4a292551acefd3e')}
def verify_inputs(root,manifest):
    reviewed={}
    for p,digest in manifest['inputs'].items():
        actual=sha(root/p)
        if p in IMPLEMENTATION_OVERRIDE:
            legacy,current=IMPLEMENTATION_OVERRIDE[p]
            assert digest==legacy and actual==current,'Unapproved private implementation: '+p
            reviewed[p]=dict(legacySha256=legacy,currentSha256=current,rpcDeclarationsUnchanged=True)
        else:
            assert actual==digest,'Legacy IIG input changed: '+p
    assert set(reviewed)==set(IMPLEMENTATION_OVERRIDE)
    return reviewed

def invoke(args):
    cache=Path(os.environ['ZIYI_LEGACY_IIG_CACHE']).resolve();build=Path(os.environ['ZIYI_LEGACY_IIG_BUILD']).resolve()
    split=args.index('--');flags=args[:split]
    assert len(flags)==8 and flags[0]=='--def' and flags[2]=='--header' and flags[4]=='--impl' and flags[6:]==['--framework-name','DriverKit'],flags
    source=Path(flags[1]).resolve();assert source.parent.name=='DriverKit' and source.parent.parent.name=='iokit' and source.suffix=='.iig'
    root=source.parent.parent.parent;manifest=json.loads((cache/'manifest.json').read_text());assert manifest['archiveSha256']==TAR_SHA
    name=source.stem;assert name in manifest['definitions']
    # No inherited source/type declarations may drift while reusing generation.
    reviewed=verify_inputs(root,manifest)
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
        with receipt.open('x') as f:json.dump(dict(source=name,outputs=result,legacyAbi=True,implementationReview=reviewed),f)
    print('ZIYI LEGACY IIG',name,'source/header hashes PASS')
def audit(cache):
    m=json.loads((cache/'manifest.json').read_text());calls={p.stem for p in (cache/'calls').glob('*.json')}
    assert calls==set(m['definitions']),('Missing IIG calls',sorted(set(m['definitions'])-calls))
    for name in calls:
        proof=json.loads((cache/'calls'/(name+'.json')).read_text())
        assert proof['legacyAbi'] and all(m['outputs'][p]==v for p,v in proof['outputs'].items())
        assert proof['implementationReview']=={p:dict(legacySha256=v[0],currentSha256=v[1],rpcDeclarationsUnchanged=True) for p,v in IMPLEMENTATION_OVERRIDE.items()}
    print('LEGACY_IIG_FULL_AUDIT_PASS definitions='+str(len(calls)))
if __name__=='__main__':
    if sys.argv[1:2]==['--prepare']:prepare(Path(sys.argv[2]),Path(sys.argv[3]))
    elif sys.argv[1:2]==['--audit']:audit(Path(sys.argv[2]))
    else:invoke(sys.argv[1:])
