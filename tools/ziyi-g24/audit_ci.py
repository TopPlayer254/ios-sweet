"""Fail closed on exact G24 source, real linked paths and SMC callback."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
sys.path.insert(0,str(R/'tools/ziyi-g24'));from linked import audited_function
sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g24/manifest.json').read_text());O=R/'u1-evidence/g24';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 d=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(d)==row['after'],row['path'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(d);rows.append(dict(path=row['path'],sha256=sha(d),bytes=len(d)))
assert len(rows)==17
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256'],row['path']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();parsed=text_symbols(raw);linked=[]
def body(name):
 f=function_bytes(raw,parsed,name);e=parsed['symbols'][name][0];s=parsed['sections'][e['section']];off=s['offset']+e['address']-s['address'];return e['address'],raw[off:off+f['bytesToNextTextSymbol']]
for name in M['linkedFunctions']:
 f=audited_function(raw,parsed,name,function_bytes);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def one(caller,callee,tail=False):
 start,b=body(caller);dest=parsed['symbols'][callee][0]['address'];out=[]
 for at in range(0,len(b)-3,4):
  w=struct.unpack_from('<I',b,at)[0]
  if w&0xfc000000==0x94000000 or tail and w&0xfc000000==0x14000000:
   imm=w&0x3ffffff;pc=start+at
   if pc+4*((imm^0x2000000)-0x2000000)==dest:out.append(pc)
 assert len(out)==1,(caller,callee,out);return out[0]
calls={label:one(caller,callee,tail) for label,caller,callee,tail in [
 ('prepare','_zg_hfi_prepare','_ziyi_gpu_cp_prepare',False),
 ('afterStartup','_zg_hfi_ready','_ziyi_gpu_cp_continue',True),
 ('engine','_ziyi_gpu_cp_continue','_g24_run',False),
 ('capsule','_ziyi_gpu_cp_prepare','_g24_slot_view',False),
 ('codec','_ziyi_gpu_cp_print','_g24_record_pack',False) ]}
smc_start,code=body('_g24n_smc');smc=[smc_start+i for i in range(0,len(code)-3,4) if struct.unpack_from('<I',code,i)[0]==0xd4000003];assert len(smc)==1
assert b'GCP1 page%u/5' in raw and b'no return into GMU reset' in raw
assert json.loads((R/'u1-evidence/g23/audit.json').read_text())['kernelSha256']==sha(raw)
host=json.loads((R/'u1-evidence/g24-host/audit.json').read_text());assert host['result']=='G24_PUBLIC_EXACT_HOST_PASS' and len(host['tests'])==4
# The public data placeholder remains invalid until the private-only derivative.
assert raw.count(b'ZIYI_G24_CP_FW01')>=1
(O/'audit.json').write_text(json.dumps(dict(result='G24_CORE_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCalls=calls,actualSmc=smc,publicHostTests=4,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G24_CORE_FULL_KERNEL_PASS')
