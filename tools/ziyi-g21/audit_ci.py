from pathlib import Path
import hashlib,json,os,re,sys
R=Path.cwd();sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
sha=lambda b:hashlib.sha256(b).hexdigest();m=json.loads((R/'tools/ziyi-g21/manifest.json').read_text());o=R/'u1-evidence/g21';o.mkdir(exist_ok=False);rows=[]
for row in m['files']:
 p=R/'dxb/xnu'/row['path'];data=p.read_bytes();assert sha(data)==row['after'];dest=o/'sources'/row['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data);rows.append(dict(path=row['path'],sha256=sha(data),bytes=len(data)))
for row in m['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/m['patch']).read_bytes())==m['patchSHA256']
header=R/'dxb/xnu/osfmk/arm64/proc_reg.h';data=header.read_bytes();s=data.decode();constants={}
for name in ['ARM_PTE_TYPE_MASK','ARM_PTE_TYPE_VALID','ARM_TTE_VALID']:
 values=re.findall(r'^#define\s+'+name+r'\s+(0x[0-9a-fA-F]+)',s,re.M);assert len(values)==1;constants[name]=int(values[0],16)
assert constants=={'ARM_PTE_TYPE_MASK':2,'ARM_PTE_TYPE_VALID':3,'ARM_TTE_VALID':1}
dest=o/'context/proc_reg.h';dest.parent.mkdir();dest.write_bytes(data)
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();parsed=text_symbols(raw);linked=[]
for name in m['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
assert json.loads((R/'u1-evidence/g20/audit.json').read_text())['kernelSha256']==sha(raw)
(o/'audit.json').write_text(json.dumps(dict(result='G21_VALID_BIT_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,constants=constants,contextHeaderSHA256=sha(data),native=False),indent=2)+'\n');print('G21_VALID_BIT_FULL_KERNEL_PASS')
