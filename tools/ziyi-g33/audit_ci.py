"""Bind bounded LPAC/publication changes to the real linked XNU image."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g33/manifest.json').read_text());O=R/'u1-evidence/g33';O.mkdir(exist_ok=False)
rows=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);rows.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256'],row['path']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
for n,h in M['unchanged'].items():assert sha((R/'dxb/xnu/bsd/dev'/n).read_bytes())==h
native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text();module=(R/'tools/ziyi-g31/g31-observer.inc').read_text();assert module in native
strip=lambda s:'\n'.join(x for x in s.splitlines() if not x.startswith(('#include','#pragma')))+'\n'
assert strip((R/'tools/ziyi-g33/files/ziyi_g33_lpac.c').read_text()) in native
codec=strip((R/'tools/ziyi-g33/files/ziyi_g33_optical.c').read_text())
import re
codec=re.sub(r'^(uint32_t|int) ((?:g32|g33)_\w+)\(',r'__attribute__((noinline)) \1 \2(',codec,flags=re.M);assert codec in module
assert '||!(p[16]||p[17])||(p[16]&16383)' in module
assert 'if(!g33_prepare_lpac(ctx)){g33_worker_stage(35);return 0;}' in module
assert 'if(snapshot)__atomic_store_n(&g33n.armed_ack,1,__ATOMIC_RELEASE);' in module
assert '(void)g32_emit_record(i+2,NULL);break;' in module
write=native.split('static int g24n_write(',1)[1].split('static int g24n_order(',1)[0]
assert write.index('g31_arm(ctx,space,id,at,value)')<write.index('g33_wait_publication(ctx,space,id)')<write.index('/* Pacing yields;')<write.index('g31_sqe_store(p,value)')
wait=native.split('static int g33_wait_publication(void*ctx,',1)[1].split('#endif',1)[0]
assert 'n<300' in wait and 'zg_sleep(10)' in wait and 'g33n.route.stage!=G33_COMPLETE' in wait
assert 'else if(smr||s2cr!=0x20000)return 0;' in native and 'if(i==2){if(!g33_slot_permitted(smr,s2cr))return 0;}' in native
writer=native.split('static int g33_write_native(',1)[1].split('static int g33_order_native(',1)[0]
assert writer.count('*(volatile uint32_t*)p=value')==1
for token in ['id==G33_LPAC_CONTROL&&value==0x300&&g33n.route.stage==G33_CONTROL_ISSUED','id==G33_LPAC_MATCH&&value==0x84000001&&g33n.route.stage==G33_MATCH_ISSUED','g33_live(ctx,g24n.w.r.epoch,g33n.table_pa)']:assert token in writer
host=json.loads((R/'u1-evidence/g33-host/audit.json').read_text());assert host['result']=='G33_BATCH_HOST_PASS' and host['syntheticOnly']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();prior=json.loads((R/'u1-evidence/g32/audit.json').read_text());assert prior['kernelSha256']==sha(raw)
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def body(name):
 en=parsed['symbols'][name][0];sec=parsed['sections'][en['section']];off=sec['offset']+en['address']-sec['address'];size=function_bytes(raw,parsed,name)['bytesToNextTextSymbol'];return en['address'],raw[off:off+size]
def calls(caller,callee,count):
 start,b=body(caller);target=parsed['symbols'][callee][0]['address'];out=[]
 for i in range(0,len(b)-3,4):
  ins=struct.unpack_from('<I',b,i)[0];imm=ins&0x3ffffff
  if ins&0xfc000000 in [0x94000000,0x14000000] and start+i+4*((imm^0x2000000)-0x2000000)==target:out.append(start+i)
 assert len(out)==count,(caller,callee,out,count);return out
actual={key:calls(c,n,count) for key,c,n,count in [('prepare','_g31_arm','_g33_prepare_lpac',1),('install','_g33_prepare_lpac','_g33_install',1),('wait','_g24n_write','_g33_wait_publication',1),('stamp','_g32_emit_record','_g33_stamp',1)]}
assert actual['wait'][0]<json.loads((R/'u1-evidence/g31/audit.json').read_text())['actualCalls']['store']
(O/'audit.json').write_text(json.dumps(dict(result='G33_LPAC_PUBLICATION_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCalls=actual,LPACMatchAddress='0x03da0808',LPACControlAddress='0x03da0c08',LPACMatchValue='0x84000001',LPACControlValue='0x00000300',onlyTwoTypedLPACStores=True,publicationWaitMilliseconds=3000,publicationBeforeFreshSQEGuards=True,wireVersion=2,wireBytes=160,full64BitTableAddress=True,observerErrorFallback=True,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G33_LPAC_PUBLICATION_FULL_KERNEL_PASS')
