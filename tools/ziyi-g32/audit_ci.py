"""G32 actual linked CPU optical output; never a GPU/native success claim."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g32/manifest.json').read_text());O=R/'u1-evidence/g32';O.mkdir(exist_ok=False)
rows=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);rows.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
for name,h in M['unchanged'].items():assert sha((R/'dxb/xnu/bsd/dev'/name).read_bytes())==h
native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text();module=(R/'tools/ziyi-g31/g31-observer.inc').read_text();assert module in native
observer=module.split('static void g31_observer(',1)[1].split('static int g31_observer_start(',1)[0]
emit=module.split('static int g32_emit_record(',1)[1].split('static void g31_observer(',1)[0]
for s in [observer,emit]:
 for bad in ['printf(', 'console_write(', 'g24n_raw(', 'g24n_pointer(', 'g24n_qualified(', 'zg_barrier(', 'smc #']:assert bad not in s
assert observer.index('g32_emit_record(1,NULL)')<observer.index('__atomic_store_n(&g31.ready,1')
assert '19200000ULL*60' in observer and 'i<160' in observer and 'zg_sleep(150)' in emit
assert emit.count('"dsb st"')==1 and 'uint8_t record[G32_BYTES],packets[2][128]' in emit
assert emit.index('g32_packet(record,sizeof(record),run,1')<emit.index('g32_draw(')
assert 'ZIYI-20261113-G32X-CPU-OPTICAL' in emit
start=module.split('static int g31_observer_start(',1)[1].split('#else',1)[0]
assert start.index('g32_fb_prepare(ctx)')<start.index('kernel_thread_start(') and 'n<300' in start
host=json.loads((R/'u1-evidence/g32-host/audit.json').read_text());assert host['result']=='G32_OPTICAL_ASAN_UBSAN_HOST_PASS'
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();prior=json.loads((R/'u1-evidence/g31/audit.json').read_text());assert prior['kernelSha256']==sha(raw) and prior['exactOriginalSingleMMIOStore'] and prior['exactOriginalSingleDSB'] and prior['observerNoGPURegisterAccess'] and prior['g32OpticalObserver']
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
actual={key:calls(c,n,count) for key,c,n,count in [('map','_g31_observer_start','_g32_fb_prepare',1),('emit','_g31_observer','_g32_emit_record',3),('record','_g32_emit_record','_g32_record',1),('packet','_g32_emit_record','_g32_packet',2),('draw','_g32_emit_record','_g32_draw',2),('tag','_g32_emit_record','_g32_crc',1),('console','_g32_fb_prepare','_PE_current_console',1),('mapping','_g32_fb_prepare','_io_map',1)]}
assert actual['map'][0]<prior['actualCalls']['thread']
_,b=body('_g32_emit_record');words=struct.unpack('<%dI'%(len(b)//4),b);assert words.count(0xd5033e9f)==2
assert b'ZIYI-20261113-G32X-CPU-OPTICAL\0' in raw and b'GSR1 CPU step%u worker%u observer%u CRC%08x' not in raw
(O/'audit.json').write_text(json.dumps(dict(result='G32_OPTICAL_OBSERVER_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCalls=actual,observerOnlyPublicationBarrier=True,observerNoGPURegisters=True,observerNoConsole=True,initialPairBeforeWorkerGate=True,snapshotFrozenForTwoPages=True,wireBytes=160,observerBudgetSeconds=60,readyWaitMilliseconds=3000,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G32_OPTICAL_OBSERVER_FULL_KERNEL_PASS')
