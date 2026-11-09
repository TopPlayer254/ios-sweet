"""Exact native-only CPU witness, fresh original predicates and real ARM64 calls."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g30/manifest.json').read_text());O=R/'u1-evidence/g30';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);rows.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
assert len(rows)==1
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
for n,h in M['unchanged'].items():assert sha((R/'dxb/xnu/bsd/dev'/n).read_bytes())==h
native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text();producer=native.split('static int g30_cp_witness(',1)[1].split('static int g24n_read(',1)[0]
assert 'g30_witness_pack(p,g24n.witness_bytes)' in producer and 'if(ms)zg_sleep(ms)' in producer and 'zg_barrier(ctx)' in producer and 'g24n_raw(' not in producer and 'io_map(' not in producer and 'smc #0' not in producer
assert 'g30_target(2,space,id,g24n.w.r.phase,g24n.w.r.profile_cursor)&&!g24n_qualified' in native and 'if(g30_target(5,space,id,w->r.phase,w->r.profile_cursor)){' in native and 'memcmp(witness_args,r,sizeof(witness_args))' in native
old=(R/'tools/ziyi-g30/g29-native-before.inc').read_text();prefix=old.split('static int g24n_write(void*ctx,unsigned space,unsigned id,uint32_t value){\n',1)[1].split(' void*p=g24n_pointer(at);',1)[0];assert native.count(prefix)==2
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();assert b'GWT1 seq%u kind%u sp%u id%u CRC%08x' in raw
prior=json.loads((R/'u1-evidence/g29/audit.json').read_text());assert prior['kernelSha256']==sha(raw) and prior['commandReleaseRequiresActualExactReceipts'] and not prior['resetStores']
host=json.loads((R/'u1-evidence/g30-host/audit.json').read_text());assert host['result']=='G30_ACTUAL_WITNESS_HOST_PASS' and host['sameGCPBytes'] and host['sameTargetLogicalMMIO'] and host['freshPredicatesAfterPacing'] and host['events']==791 and host['scheduledMs']==10080 and host['allAuxiliaryRegisters']==8 and host['witnessBytes']==64 and host['corruptBitsRefused']==512
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);assert f['bytesToNextTextSymbol']>=32;f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def calls(caller,callee):
 en=parsed['symbols'][caller][0];sec=parsed['sections'][en['section']];off=sec['offset']+en['address']-sec['address'];size=function_bytes(raw,parsed,caller)['bytesToNextTextSymbol'];target=parsed['symbols'][callee][0]['address'];out=[]
 for i in range(0,size-3,4):
  ins=struct.unpack_from('<I',raw,off+i)[0];imm=ins&0x3ffffff
  if ins&0xfc000000 in [0x94000000,0x14000000] and en['address']+i+4*((imm^0x2000000)-0x2000000)==target:out.append(en['address']+i)
 return out
actual={n:calls(n,'_g30_cp_witness') for n in ['_g24n_read','_g24n_write','_g24n_smc']};assert [len(actual[n]) for n in actual]==[3,3,2],actual
pack=calls('_g30_cp_witness','_g30_witness_pack');assert len(pack)==1
(O/'audit.json').write_text(json.dumps(dict(result='G30_NATIVE_WITNESS_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualWitnessCalls=actual,actualPackCalls=pack,engineHeaderProfileUnchanged=True,GCPGCKCPRUnchanged=True,CPUOnlyWitness=True,targetOperationsUnchanged=True,originalPredicatesRepeatedAfterPacing=True,deadlineSeconds=15,scheduledMsModel=10080,witnessBytes=64,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G30_NATIVE_WITNESS_FULL_KERNEL_PASS')
