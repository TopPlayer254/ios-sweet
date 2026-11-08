"""Exact order, stable-view source, active linked helper; all older gates stay."""
from pathlib import Path
import hashlib,json,os,re,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g26/manifest.json').read_text());O=R/'u1-evidence/g26';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 data=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(data)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);rows.append(dict(path=row['path'],sha256=sha(data),bytes=len(data)))
assert len(rows)==2
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
def table(p):return [tuple(int(v,16) for v in row) for row in re.findall(r'\{0x([0-9a-f]+)ULL,0x([0-9a-f]+)u,0x([0-9a-f]+)u\}',p.read_text())]
old=table(R/'tools/ziyi-g26/g25-profile-before.inc');new=table(O/'sources/bsd/dev/ziyi_g24_profile.inc');assert len(old)==len(new)==151 and sorted(old)==sorted(new) and new==old[:12]+old[99:]+old[12:99]
assert new[12]==(0x03d6a200,0,1) and new[62]==(0x03d6a200,1,1) and new[63]==(0x03d002b8,0x8aa8aa82,0xffffffff) and new[76][0]==0x03d02308
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();blob=b''.join(struct.pack('<QII',*row) for row in new);assert raw.count(blob)==1
engine=(O/'sources/bsd/dev/ziyi_g24_engine.c').read_text();assert 'value!=w->r.profile_current[j]' in engine and 'got!=value' in engine and 'w->r.pending_index!=last' in engine
host=json.loads((R/'u1-evidence/g26-host/audit.json').read_text());assert host['result']=='G26_ACTUAL_CLOCK_ORDER_HOST_PASS' and host['oldReads']==439 and host['oldStores']==43 and host['clockEntries']==52 and host['stablePostClockReads']==174 and host['originalValuesRetained'] and host['unstableViewRefused'] and host['badClockReceiptRefused']
prior=json.loads((R/'u1-evidence/g25/audit.json').read_text());assert prior['result']=='G25_COUNTER_DEFERRED_FULL_KERNEL_PASS' and prior['kernelSha256']==sha(raw)
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);f=function_bytes(raw,parsed,'_g24_clock_view');f['sampleSHA256']=sha(f.pop('sample'));entry=parsed['symbols']['_g24_run'][0];section=parsed['sections'][entry['section']];off=section['offset']+entry['address']-section['address'];n=function_bytes(raw,parsed,'_g24_run')['bytesToNextTextSymbol'];target=parsed['symbols']['_g24_clock_view'][0]['address'];calls=[]
for at in range(0,n-3,4):
 w=struct.unpack_from('<I',raw,off+at)[0]
 if w&0xfc000000==0x94000000:
  imm=w&0x3ffffff;pc=entry['address']+at
  if pc+4*((imm^0x2000000)-0x2000000)==target:calls.append(pc)
assert len(calls)==1
(O/'audit.json').write_text(json.dumps(dict(result='G26_HWCG_CURRENT_VIEW_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=[f],actualClockViewCalls=calls,profileRows=151,allTriplesUnchanged=True,clockEntries=52,clockBegin=12,clockEnd=64,stablePostClockReads=174,originalValuesRetained=True,readbackExact=True,profileBytesSHA256=sha(blob),native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G26_HWCG_CURRENT_VIEW_FULL_KERNEL_PASS')
