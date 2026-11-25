"""Bind G35 shader/descriptor bytes and guarded calls to the actual full XNU."""
from pathlib import Path
import hashlib,json,os,re,struct,sys
R=Path.cwd();P=R/'tools/ziyi-g35';M=json.loads((P/'manifest.json').read_text());O=R/'u1-evidence/g35';O.mkdir(exist_ok=False)
sha=lambda b:hashlib.sha256(b).hexdigest()
sources=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);sources.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256'],row['path']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
for name,h in M['unchanged'].items():assert sha((R/'dxb/xnu/bsd/dev'/name).read_bytes())==h
native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text();body=(P/'native.inc').read_text();assert body in native
strip=lambda s:'\n'.join(x for x in s.splitlines()if not x.startswith(('#include','#pragma')))+'\n'
for name in ['final_wire.h','final_wire.c','primitives_result.h','primitives_result.c']:
 s=strip((P/name).read_text())
 if name.endswith('.c'):s=re.sub(r'^(uint32_t|int) (g35_\w+)\(',r'__attribute__((noinline)) \1 \2(',s,flags=re.M)
 assert s in native,name
assert strip((P/'files/ziyi_g35_asset.inc').read_text())in native
publish=body.split('static int g35n_publish(',1)[1].split('static int g35n_present(',1)[0]
assert publish.count('*(volatile uint32_t*)p=G35_RING_END;zg_barrier(ctx);')==1
assert publish.count('g35n_guard(ctx)')==2 and publish.count('g35n_exact_pages(0)')==2 and publish.count('g35n_sample(ctx)')==2
for token in ['io_map(','ml_io_unmap(','smc #','hop_xnu_prepare(','kalloc(']:assert token not in body,token
assert '19200000ull*3'in body and'n<20000'in body
assert 'g24n.w.pixels['not in body and '*(volatile uint32_t*)g24n.w.pixels'not in body
assert '__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)'in body
host=json.loads((R/'u1-evidence/g35-host/audit.json').read_text());assert host['result']=='G35_PRIMITIVES_HOST_PASS'and host['actualNativeAdapterCases']==15 and host['syntheticOnly']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();assert json.loads((R/'u1-evidence/g34/audit.json').read_text())['kernelSha256']==sha(raw)
assert (P/'data/ring-template.bin').read_bytes()[1212:]in raw
assert (P/'data/fence-template.bin').read_bytes()[256:]in raw
asset=json.loads((P/'asset.json').read_text());assert asset['result']=='G35_TYPED_ASSET_PASS'and asset['inverseExact']and asset['shaderCodeByteUnchanged']and asset['noNewPages']and asset['noColorTargetCPUWrites']
assert len(asset['relocations'])==126 and len(asset['shaderPreloads'])==9 and asset['begin']==303 and asset['end']==512
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def code(name):
 e=parsed['symbols'][name][0];s=parsed['sections'][e['section']];off=s['offset']+e['address']-s['address'];n=function_bytes(raw,parsed,name)['bytesToNextTextSymbol'];return e['address'],raw[off:off+n]
def calls(caller,callee,allowed):
 address,b=code(caller);target=parsed['symbols'][callee][0]['address'];out=[]
 for i in range(0,len(b)-3,4):
  w=struct.unpack_from('<I',b,i)[0];v=w&0x3ffffff
  if w&0xfc000000 in [0x94000000,0x14000000]and address+i+4*((v^0x2000000)-0x2000000)==target:out.append(address+i)
 assert len(out)in allowed,(caller,callee,len(out),allowed);return out
actual={}
for key,caller,callee,allowed in [
 ('run','_ziyi_gpu_cp_continue','_g35n_run',[1]),('freeze','_ziyi_gpu_cp_continue','_g35n_finish_record',[1]),('emit','_ziyi_gpu_cp_continue','_g35n_emit_final',[1]),
 ('publish','_g35n_run','_g35n_publish',[1]),('pixels','_g35n_run','_g35_check_pixels',[1]),('depth','_g35n_run','_g35_check_depth',[1]),('present','_g35n_run','_g35n_present',[1]),
 ('record','_g35n_finish_record','_g35_result_pack',[1]),('fullRecord','_g35n_finish_record','_g35_final_pack',[1]),
 ('packet','_g35n_emit_final','_g35_final_packet',[1,2,10]),('draw','_g35n_emit_final','_g35_final_draw',[1,2,10]),
 ('freshOwner','_g35n_publish','_g35n_guard',[2]),('exactBytes','_g35n_publish','_g35n_exact_pages',[2])]:actual[key]=calls(caller,callee,allowed)
assert actual['run'][0]<actual['freeze'][0]and actual['publish'][0]<actual['pixels'][0]<actual['depth'][0]<actual['present'][0]
# Site counts are NOT loop execution counts: the pure CPU producer source is
# hash-bound above with both exact i<10 loops. Actual C/Python/ten-page capture
# qualification is separate. Do not infer ten machine sites from ten draws.
at,b=code('_g35n_publish');w=struct.unpack('<'+'I'*(len(b)//4),b[:len(b)//4*4]);stores=[]
for i in range(len(w)-2):
 m,s,d=w[i:i+3]
 if(m&0x7f800000)==0x52800000 and((m>>5)&65535)==512 and((m>>21)&3)==0 and(s&0xffc00000)==0xb9000000 and((s>>10)&4095)==0 and(s&31)==(m&31)and d==0xd5033f9f:stores.append(at+(i+1)*4)
assert len(stores)==1 and max(actual['freshOwner'])<stores[0]and max(actual['exactBytes'])<stores[0]
assert not any('g35_model'in name for name in parsed['symbols'])
(O/'audit.json').write_bytes((json.dumps(dict(result='G35_PRIMITIVES_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=sources,linkedFunctions=linked,
 actualCalls=actual,actualWPTR512Store=stores[0],typedPointers=126,actualShaderPreloads=9,noNewPages=True,originalG34Retained=True,
 finalRecordBytes=880,finalPages=10,engineBoundMs=3000,nativeExecution=False,metalReady=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n').encode())
print('G35_PRIMITIVES_FULL_KERNEL_PASS exact shader/descriptor bytes,15 helpers and actual WPTR512 STR/DSB')
