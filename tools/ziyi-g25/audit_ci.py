"""Keep every G24 gate; qualify exact G25 profile and active native printer."""
from pathlib import Path
import hashlib,json,os,re,struct,sys
R=Path.cwd();M=json.loads((R/'tools/ziyi-g25/manifest.json').read_text());O=R/'u1-evidence/g25';O.mkdir(exist_ok=False);sha=lambda b:hashlib.sha256(b).hexdigest();rows=[]
for row in M['files']:
 data=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(data)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);rows.append(dict(path=row['path'],sha256=sha(data),bytes=len(data)))
assert len(rows)==2
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();old=json.loads((R/'u1-evidence/g24/audit.json').read_text());assert old['result']=='G24_CORE_FULL_KERNEL_PASS' and old['kernelSha256']==sha(raw)
profile=(O/'sources/bsd/dev/ziyi_g24_profile.inc').read_text();values=[tuple(int(v,16) for v in row) for row in re.findall(r'\{0x([0-9a-f]+)ULL,0x([0-9a-f]+)u,0x([0-9a-f]+)u\}',profile)];assert len(values)==151 and all(v[0]!=0x03d0142c for v in values)
blob=b''.join(struct.pack('<QII',*v) for v in values);assert raw.count(blob)==1
assert sha((R/'dxb/xnu/bsd/dev/ziyi_g24_record.c').read_bytes())=='ab3c671481e06256becc4d73430542088f448dfae2075302a61d699b388ec91f'
assert sha((R/'dxb/xnu/bsd/dev/ziyi_g24_engine.c').read_bytes())=='319a6eb3fee2a78af13e360e4b33f8e43ef5688d790fb4bc2e7e794fbc8c2c35'
native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text();assert 'page++%5' in native and 'row<4' in native and 'unsigned n=p*16+row*4' in native and 'zg_sleep(2000);ziyi_gpu_cp_print(ctx);' in native and b'GCP1 page%u/5' in raw
host=json.loads((R/'u1-evidence/g25-host/audit.json').read_text());assert host['result']=='G25_DELTA_CONTROLS_PASS' and host['remainingExactReadbackRefusals']==151 and host['sentinels']==4 and host['optionalStores']==0 and host['actualPrinterLossless']
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
(O/'audit.json').write_text(json.dumps(dict(result='G25_COUNTER_DEFERRED_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,profileRows=151,profileBytesSHA256=sha(blob),optionalCounterAbsent=True,remainingReadbackExact=True,wireABI='GCP1/v1/320B',actualPrinterPages=5,rowsPerPage=4,periodMs=2000,actualPrinterLossless=True,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G25_COUNTER_DEFERRED_FULL_KERNEL_PASS')
