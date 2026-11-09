"""Exact G28 sources, linked selector/view/printer and real ARM64 call sites."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g28/manifest.json').read_text());O=R/'u1-evidence/g28';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);rows.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
assert len(rows)==3
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();previous=json.loads((R/'u1-evidence/g27/audit.json').read_text());assert previous['kernelSha256']==sha(raw) and previous['readbackExact'] and previous['GXClockGatingOff']
engine=(O/'sources/bsd/dev/ziyi_g24_engine.c').read_text();header=(O/'sources/bsd/dev/ziyi_g24_engine.h').read_text();native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text()
for token in ['g28_clock_select(w,o)','d->clock_before!=w->r.profile_current[63]','d->sp_now&0xd7u','got!=value','d->writes!=52','d->valid!=59','w->r.writes!=30','value!=w->r.profile_current[j]','w->r.pending_index!=last','w->r.last_value!=0u'] :assert token in engine,token
assert '#define G28_CLOCK_BYTES 80u' in header and '0x314b4347u' in header and 'page++%8' in native and 'row<5' in native and 'row<4' in native and b'GCK1 clock mode%u valid%02x CRC%08x' in raw and b'GCP1 page%u/5' in raw
profile=(R/'dxb/xnu/bsd/dev/ziyi_g24_profile.inc').read_bytes();assert sha(profile)==M['profileSHA256'];assert sha((R/'dxb/xnu/bsd/dev/ziyi_g24_record.c').read_bytes())==M['recordSHA256']
host=json.loads((R/'u1-evidence/g28-host/audit.json').read_text());assert host['result']=='G28_ACTUAL_ALREADY_OFF_HOST_PASS' and host['oldReads']==515 and host['oldStores']==81 and host['oldCursor']==62 and host['skipClockStores']==0 and host['strictSPReadback'] and host['auxiliaryBytes']==80 and host['auxiliaryCorruptBitsRefused']==640 and host['actualPrinterGCPRows']==20 and host['actualPrinterAuxRows']==5
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);assert f['bytesToNextTextSymbol']>=32;f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def one(caller,callee):
 entry=parsed['symbols'][caller][0];sec=parsed['sections'][entry['section']];off=sec['offset']+entry['address']-sec['address'];size=function_bytes(raw,parsed,caller)['bytesToNextTextSymbol'];target=parsed['symbols'][callee][0]['address'];calls=[]
 for i in range(0,size-3,4):
  ins=struct.unpack_from('<I',raw,off+i)[0];imm=ins&0x3ffffff
  if ins&0xfc000000==0x94000000 and entry['address']+i+4*((imm^0x2000000)-0x2000000)==target:calls.append(entry['address']+i)
 assert len(calls)==1,(caller,callee,calls);return calls[0]
calls=dict(select=one('_g24_run','_g28_clock_select'),view=one('_g24_run','_g24_clock_view'))
assert previous['actualClockViewCalls']==[calls['view']]
(O/'audit.json').write_text(json.dumps(dict(result='G28_ALREADY_OFF_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCalls=calls,profileUnchanged=True,recordUnchanged=True,strictSPReadback=True,alreadyOffNoClockStores=True,stablePostClockReads=174,auxiliaryBytes=80,actualPrinterGCPRows=20,actualPrinterAuxRows=5,originalValuesRetained=True,allOwnerGuardsRetained=True,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G28_ALREADY_OFF_FULL_KERNEL_PASS')
