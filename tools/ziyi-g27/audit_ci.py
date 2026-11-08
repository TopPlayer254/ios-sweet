"""Qualify defined GX-only HWCG-off values without relaxing any readback."""
from pathlib import Path
import hashlib,json,os,re,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g27/manifest.json').read_text());O=R/'u1-evidence/g27';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 data=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(data)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);rows.append(dict(path=row['path'],sha256=sha(data),bytes=len(data)))
assert len(rows)==2
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
def table(p):return [tuple(int(v,16) for v in row) for row in re.findall(r'\{0x([0-9a-f]+)ULL,0x([0-9a-f]+)u,0x([0-9a-f]+)u\}',p.read_text())]
old=table(R/'tools/ziyi-g27/g26-profile-before.inc');new=table(O/'sources/bsd/dev/ziyi_g24_profile.inc');expected=[(a,0 if 13<=i<62 or i==63 else v,m) for i,(a,v,m) in enumerate(old)];assert len(new)==151 and new==expected and sum(a!=b for a,b in zip(old,new))==43
assert [(a,m) for a,v,m in old]==[(a,m) for a,v,m in new] and new[12][1]==0 and new[62][1]==1 and all(new[i][1]==0 for i in [*range(13,62),63])
engine=(O/'sources/bsd/dev/ziyi_g24_engine.c').read_text();assert 'G24_GX_HWCG_OFF==1' in engine and 'w->r.pending_value!=0u' in engine and 'w->r.last_value!=0u' in engine and 'got!=value' in engine
host=json.loads((R/'u1-evidence/g27-host/audit.json').read_text());assert host['result']=='G27_ACTUAL_GX_OFF_HOST_PASS' and host['oldReads']==455 and host['oldStores']==51 and host['zeroModeWrongReadRefused'] and host['ROQZeroReadRefused'] and host['GXClockGatingOff'] and host['stablePostClockReads']==174
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();blob=b''.join(struct.pack('<QII',*v) for v in new);assert raw.count(blob)==1;previous=json.loads((R/'u1-evidence/g26/audit.json').read_text());assert previous['kernelSha256']==sha(raw) and previous['GXClockGatingOff'] and previous['allAddressesMasksUnchanged'] and not previous['allTriplesUnchanged']
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);f=function_bytes(raw,parsed,'_g24_clock_view');f['sampleSHA256']=sha(f.pop('sample'));assert previous['linkedFunctions']==[f]
(O/'audit.json').write_text(json.dumps(dict(result='G27_GX_HWCG_OFF_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=[f],actualClockViewCalls=previous['actualClockViewCalls'],profileRows=151,changedValues=43,allAddressesMasksUnchanged=True,GXClockGatingOff=True,GMUHfiContextUnchanged=True,clockEntries=52,clockTableZeroValues=49,finalClockValue=0,stablePostClockReads=174,originalValuesRetained=True,readbackExact=True,profileBytesSHA256=sha(blob),native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G27_GX_HWCG_OFF_FULL_KERNEL_PASS')
