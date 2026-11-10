"""Exact CPU observer and STR/CPU-step/DSB sequence, no native success claim."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g31/manifest.json').read_text());O=R/'u1-evidence/g31';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);rows.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
assert len(rows)==1
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text();module=(R/'tools/ziyi-g31/g31-observer.inc').read_text();assert module in native
for name,h in M['unchanged'].items():assert sha((R/'dxb/xnu/bsd/dev'/name).read_bytes())==h
observer=module.split('static void g31_observer(',1)[1].split('static int g31_observer_start(',1)[0]
for token in ['g24n_raw(', 'io_map(', 'zg_barrier(', 'g24n_pointer(', 'g24n_qualified(', 'smc #']:assert token not in observer
assert 'cpu_to_processor((int)g31.observer_cpu)' in observer and 'cpu_number()!=g31.observer_cpu' in observer and '19200000ULL*60' in observer
assert 'for(unsigned i=4;i<w->fw.sqe_bytes;i++)if(sqe[i-4]!=w->fw.sqe[i])return 0;' in module
write=native.split('static int g24n_write(',1)[1].split('static int g24n_order(',1)[0];assert write.index('g31_arm(ctx,space,id,at,value)')<write.index('/* Pacing yields;')<write.index('g31_sqe_store(p,value)')
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();prior=json.loads((R/'u1-evidence/g30/audit.json').read_text());assert prior['kernelSha256']==sha(raw) and prior['originalPredicatesRepeatedAfterPacing']
host=json.loads((R/'u1-evidence/g31-host/audit.json').read_text());assert host['result']=='G31_CPU_OBSERVER_HOST_PASS' and host['stages']==3 and host['stallWindows']==3 and host['codecBitRefusals']==3072 and host['sameGCPBytes'] and host['noAdditionalTargetMMIO'] and host['noAdditionalStoreBarrier'] and host['workerProgressNoPrintSleep'] and not host['observerMMIO']
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def body(name):
 en=parsed['symbols'][name][0];sec=parsed['sections'][en['section']];off=sec['offset']+en['address']-sec['address'];size=function_bytes(raw,parsed,name)['bytesToNextTextSymbol'];return en['address'],raw[off:off+size]
def calls(caller,callee):
 start,b=body(caller);target=parsed['symbols'][callee][0]['address'];out=[]
 for i in range(0,len(b)-3,4):
  ins=struct.unpack_from('<I',b,i)[0];imm=ins&0x3ffffff
  if ins&0xfc000000 in [0x94000000,0x14000000] and start+i+4*((imm^0x2000000)-0x2000000)==target:out.append(start+i)
 assert len(out)==1,(caller,callee,out);return out[0]
actual={k:calls(c,n) for k,c,n in [('start','_ziyi_gpu_cp_continue','_g31_observer_start'),('engine','_ziyi_gpu_cp_continue','_g24_run'),('arm','_g24n_write','_g31_arm'),('store','_g24n_write','_g31_sqe_store'),('thread','_g31_observer_start','_kernel_thread_start'),('snapshot','_g31_observer','_g31_snapshot'),('pack','_g31_snapshot','_g31_record_pack'),('bindWorker','_g31_observer_start','_thread_bind'),('bindObserver','_g31_observer','_thread_bind'),('checkBinding','_g31_arm','_g31_record_pack')]};assert actual['start']<actual['engine'] and actual['arm']<actual['store']
start,b=body('_g31_sqe_store');words=list(struct.unpack('<%dI'%(len(b)//4),b));padding=0
while len(words)>11 and words[-1]==0xd503201f:words.pop();padding+=1
assert len(words)==11 and words[2:10]==[0x52800029,0xb9000109,0xb9000001,0x52800049,0xb9000109,0xd5033f9f,0x52800069,0xb9000109] and words[10]==0xd65f03c0
imm=((words[0]>>29)&3)|(((words[0]>>5)&0x7ffff)<<2);delta=(imm^0x100000)-0x100000
if words[0]&0x9f00001f==0x90000008:
 assert words[1]&0xffc003ff==0x91000108;cpuAddress=(start&~4095)+(delta<<12)+((words[1]>>10)&4095)
else:
 assert words[0]&0x9f00001f==0x10000008 and words[1]==0xd503201f;cpuAddress=start+delta

at=32;symbols=None;writable=[]
for _ in range(struct.unpack_from('<I',raw,16)[0]):
 cmd,size=struct.unpack_from('<II',raw,at)
 if cmd==2:symbols=struct.unpack_from('<IIII',raw,at+8)
 if cmd==0x19:
  va,vs=struct.unpack_from('<QQ',raw,at+24);prot=struct.unpack_from('<I',raw,at+60)[0]
  if prot&2:writable.append((va,va+vs))
 at+=size
assert symbols;off,count,strings,ss=symbols;found=[]
for i in range(count):
 name,kind,section,desc,value=struct.unpack_from('<IBBHQ',raw,off+16*i)
 if name and kind&0xe0==0 and kind&0x0e==0x0e and raw[strings+name:raw.find(b'\0',strings+name)]==b'_g31':found.append(value)
assert found==[cpuAddress] and not cpuAddress&3 and any(a<=cpuAddress<b for a,b in writable)
assert b'GSR1 CPU step%u worker%u observer%u CRC%08x' in raw
(O/'audit.json').write_text(json.dumps(dict(result='G31_CPU_OBSERVER_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCalls=actual,storeWords=words,paddingNOPs=padding,CPUProgressAddress=cpuAddress,exactOriginalSingleMMIOStore=True,exactOriginalSingleDSB=True,progressStoresCPUOnly=True,observerNoMMIO=True,observerDistinctCPURequired=True,bindingsAndSQEBytesChecked=True,allFreshPredicatesAfterArming=True,wireBytes=128,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G31_CPU_OBSERVER_FULL_KERNEL_PASS')
