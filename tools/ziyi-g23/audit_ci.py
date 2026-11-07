"""Exact G23 source and actual linked startup call checks after full XNU build."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
sha=lambda d:hashlib.sha256(d).hexdigest();M=json.loads((R/'tools/ziyi-g23/manifest.json').read_text());O=R/'u1-evidence/g23';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 d=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(d)==row['after'],row['path'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(d);rows.append(dict(path=row['path'],sha256=sha(d),bytes=len(d)))
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256'],row['path']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def one(caller,callee,allow_tail=False):
 f=function_bytes(raw,parsed,caller);e=parsed['symbols'][caller][0];s=parsed['sections'][e['section']];offset=s['offset']+e['address']-s['address'];body=raw[offset:offset+f['bytesToNextTextSymbol']];dest=parsed['symbols'][callee][0]['address'];out=[]
 for at in range(0,len(body)-3,4):
  word=struct.unpack_from('<I',body,at)[0]
  if word&0xfc000000==0x94000000 or allow_tail and word&0xfc000000==0x14000000:
   imm=word&0x3ffffff;pc=e['address']+at
   if pc+4*((imm^0x2000000)-0x2000000)==dest:out.append(pc)
 assert len(out)==1,(caller,callee,out);return out[0]
calls={label:one(caller,callee,tail) for label,caller,callee,tail in [
 ('prepare','_zg_worker','_ziyi_gpu_startup_prepare',False),
 ('versionContinuation','_zg_hfi_ready','_ziyi_gpu_startup_continue',True),
 ('batch','_ziyi_gpu_startup_continue','_gs_run',False),
 ('record','_zg_g8_run','_ziyi_gpu_startup_print',False),
 ('codec','_ziyi_gpu_startup_print','_gs_encode',False),
 ('readonlyMap','_ziyi_gpu_startup_prepare','_io_map',False),
 ('readonlyUnmap','_ziyi_gpu_startup_prepare','_ml_io_unmap',False)]}
calls['clocks']=one('_zg_worker','_g7c_run');assert calls['prepare']<calls['clocks']
assert json.loads((R/'u1-evidence/g22/audit.json').read_text())['kernelSha256']==sha(raw)
(O/'audit.json').write_text(json.dumps(dict(result='G23_OWN_RESOURCE_HFI_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCalls=calls,commands=5,native=False,gpuPixels=False),indent=2)+'\n');print('G23_OWN_RESOURCE_HFI_FULL_KERNEL_PASS')
