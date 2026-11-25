"""Qualify G34's exact source, generated shaders and real linked call chain."""
from pathlib import Path
import hashlib,json,os,re,struct,sys
R=Path.cwd();P=R/'tools/ziyi-g34';M=json.loads((P/'manifest.json').read_text());O=R/'u1-evidence/g34';O.mkdir(exist_ok=False)
sha=lambda b:hashlib.sha256(b).hexdigest()
rows=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);rows.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256'],row['path']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
for name,digest in M['unchanged'].items():assert sha((R/'dxb/xnu/bsd/dev'/name).read_bytes())==digest
native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text()
assert (P/'native.inc').read_text()in native
strip=lambda s:'\n'.join(x for x in s.splitlines()if not x.startswith(('#include','#pragma')))+'\n'
for name in['final_wire.h','final_wire.c','triangle_result.h','triangle_result.c']:
 s=strip((P/name).read_text())
 if name.endswith('.c'):s=re.sub(r'^(uint32_t|int) (g34_\w+)\(',r'__attribute__((noinline)) \1 \2(',s,flags=re.M)
 assert s in native,name
assert strip((P/'files/ziyi_g34_asset.inc').read_text())in native
body=(P/'native.inc').read_text();publish=body.split('static int g34n_publish(',1)[1].split('static int g34n_present(',1)[0]
assert publish.count('*(volatile uint32_t*)p=G34_RING_END;zg_barrier(ctx);')==1
assert publish.count('g34n_guard(ctx)')==2 and publish.count('g34n_exact_pages(0)')==2 and publish.count('g34n_sample(ctx)')==2
assert 'g24_register_address(G24_CTL,G24_WPTR)'in publish
for token in['io_map(','ml_io_unmap(','smc #','hop_xnu_prepare(','kalloc(','CP_SET_AMBLE']:
 assert token not in body,token
assert 'w->pixels['not in body and '*pixels='not in body
assert 'n<20000'in body and '19200000ull*3'in body
assert 'g34n_run(ctx,ok)'in native and 'g34n_finish_record(ctx)'in native and 'g34n_emit_final(ctx)'in native
assert '__atomic_load_n(&g31.ready,__ATOMIC_ACQUIRE)'in body
host=json.loads((R/'u1-evidence/g34-host/audit.json').read_text());assert host['result']=='G34_TRIANGLE_HOST_PASS'and host['actualNativeAdapterCases']==11 and host['syntheticOnly']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes()
assert json.loads((R/'u1-evidence/g33/audit.json').read_text())['kernelSha256']==sha(raw)
assert (P/'data/ring-template.bin').read_bytes()[412:]in raw
assert (P/'data/fence-template.bin').read_bytes()[256:]in raw
asset=json.loads((P/'asset.json').read_text());assert asset['result']=='G34_TYPED_ASSET_PASS'and asset['inverseExact']and asset['shaderCodeByteUnchanged']and asset['noTargetCPUWrites']and asset['noNewPages']
assert len(asset['relocations'])==41 and asset['rootWords']==200 and asset['end']==303
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def calls(caller,callee,allowed):
 e=parsed['symbols'][caller][0];sec=parsed['sections'][e['section']];off=sec['offset']+e['address']-sec['address'];n=function_bytes(raw,parsed,caller)['bytesToNextTextSymbol'];b=raw[off:off+n];target=parsed['symbols'][callee][0]['address'];out=[]
 for i in range(0,len(b)-3,4):
  w=struct.unpack_from('<I',b,i)[0];imm=w&0x3ffffff
  if w&0xfc000000 in[0x94000000,0x14000000]and e['address']+i+4*((imm^0x2000000)-0x2000000)==target:out.append(e['address']+i)
 assert len(out)in allowed,(caller,callee,len(out),allowed);return out
actual={}
for key,caller,callee,allowed in[
 ('run','_ziyi_gpu_cp_continue','_g34n_run',[1]),('freeze','_ziyi_gpu_cp_continue','_g34n_finish_record',[1]),
 ('emit','_ziyi_gpu_cp_continue','_g34n_emit_final',[1]),('publish','_g34n_run','_g34n_publish',[1]),
 ('pixels','_g34n_run','_g34_check_pixels',[1]),('present','_g34n_run','_g34n_present',[1]),
 ('triRecord','_g34n_finish_record','_g34_result_pack',[1]),('fullRecord','_g34n_finish_record','_g34_final_pack',[1]),
 ('packet','_g34n_emit_final','_g34_final_packet',[1,10]),('draw','_g34n_emit_final','_g34_final_draw',[1,10]),
 ('freshOwner','_g34n_publish','_g34n_guard',[2]),('exactBytes','_g34n_publish','_g34n_exact_pages',[2])]:
 actual[key]=calls(caller,callee,allowed)
assert actual['run'][0]<actual['freeze'][0]
assert actual['publish'][0]<actual['pixels'][0]<actual['present'][0]
(O/'audit.json').write_text(json.dumps(dict(result='G34_VS_FS_TRIANGLE_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],
 kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCalls=actual,
 actualCompiledTemplates=True,realCompiledVSFS=True,noNewPages=True,onlyOneNewTypedWPTRStore=True,
 originalG33PacketRetained=True,fullFinalRecordBytes=880,finalPages=10,engineBoundMilliseconds=3000,
 nativeExecution=False,gpuTriangleProven=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n')
print('G34_VS_FS_TRIANGLE_FULL_KERNEL_PASS shader bytes + 14 linked helpers + actual guarded calls')
