from pathlib import Path
import hashlib,json,os,sys
R=Path.cwd();sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
sha=lambda b:hashlib.sha256(b).hexdigest()
m=json.loads((R/'tools/ziyi-g20/manifest.json').read_text());o=R/'u1-evidence/g20';o.mkdir(exist_ok=False);rows=[]
for row in m['files']:
 p=R/'dxb/xnu'/row['path'];data=p.read_bytes();assert sha(data)==row['after'],row['path'];dest=o/'sources'/row['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data);rows.append(dict(path=row['path'],sha256=sha(data),bytes=len(data)))
for row in m['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256'],row['path']
assert sha((R/m['patch']).read_bytes())==m['patchSHA256']
kernel=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();parsed=text_symbols(kernel);linked=[]
for name in m['linkedFunctions']:
 f=function_bytes(kernel,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
assert b'ZGPF%u%u %s' in kernel and b'ZIYI G20 pmap call' in kernel
prior=json.loads((R/'u1-evidence/g19/audit.json').read_text());assert prior['kernelSha256']==sha(kernel)
(o/'audit.json').write_text(json.dumps(dict(result='G20_FULL_KERNEL_CPU_TRACE_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(kernel),files=rows,linkedFunctions=linked,native=False,permissionsChanged=False),indent=2)+'\n');print('G20_FULL_KERNEL_CPU_TRACE_PASS')
