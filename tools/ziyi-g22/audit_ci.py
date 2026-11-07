from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
sha=lambda b:hashlib.sha256(b).hexdigest();m=json.loads((R/'tools/ziyi-g22/manifest.json').read_text());o=R/'u1-evidence/g22';o.mkdir(exist_ok=False);rows=[]
for row in m['files']:
 data=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(data)==row['after'];p=o/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);rows.append(dict(path=row['path'],sha256=sha(data),bytes=len(data)))
for row in m['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/m['patch']).read_bytes())==m['patchSHA256']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();parsed=text_symbols(raw);linked=[]
for name in m['linkedFunctions']:
 f=function_bytes(raw,parsed,name);f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
def calls(name):
 e=parsed['symbols'][name][0];section=parsed['sections'][e['section']];start=e['address'];stop=min([v for v in parsed['addresses'][e['section']] if v>start],default=section['address']+section['bytes']);offset=section['offset']+start-section['address'];body=raw[offset:offset+stop-start];out=[]
 for at in range(0,len(body)-3,4):
  ins=struct.unpack_from('<I',body,at)[0]
  if ins&0xfc000000==0x94000000:
   imm=ins&0x3ffffff;out.append((start+at,start+at+4*((imm^0x2000000)-0x2000000)))
 return out
def one(caller,callee):
 address=parsed['symbols'][callee][0]['address'];at=[pc for pc,dest in calls(caller) if dest==address];assert len(at)==1,(caller,callee,at);return at[0]
# Clang -O2 inlines bsd_autoconf and ziyi_gpu_probe_start. Check their
# real operational callees, not an out-of-line call the compiler eliminated.
gate=one('_bsd_init','_ziyi_gpu_preuser_start');autoconf=one('_bsd_init','_IOKitBSDInit');root=one('_bsd_init','_IOFindBSDRoot');mount=one('_bsd_init','_vfs_mountroot');assert gate<autoconf<root<mount
park=dict(start=one('_ziyi_gpu_preuser_start','_kernel_thread_start'),release=one('_ziyi_gpu_preuser_start','_thread_deallocate'),wait=one('_ziyi_gpu_preuser_start','_assert_wait'),block=one('_ziyi_gpu_preuser_start','_thread_block'))
assert park['start']<park['release']<park['wait']<park['block']
def word(pc):
 for sec in parsed['sections'].values():
  if sec['address']<=pc<sec['address']+sec['bytes']:return struct.unpack_from('<I',raw,sec['offset']+pc-sec['address'])[0]
 raise AssertionError('PC outside executable text')
# Exact ADR x0 -> worker argument followed by NOP, x2 stack destination,
# x1 = NULL; the wait/block back edge cannot return to BSD root handling.
worker=word(park['start']-16);assert worker&0x9f00001f==0x10000000
imm=((worker>>29)&3)|(((worker>>5)&0x7ffff)<<2);target=park['start']-16+((imm^0x100000)-0x100000)
assert target==parsed['symbols']['_zg_worker'][0]['address']
assert word(park['start']-12)==0xd503201f and word(park['start']-4)==0xd2800001
loop=word(park['block']+4);assert loop&0xfc000000==0x14000000
imm=loop&0x3ffffff;assert park['block']+4+4*((imm^0x2000000)-0x2000000)==park['wait']-8
assert word(park['wait']-4)==0x52800001 and word(park['block']-4)==0xd2800000

assert json.loads((R/'u1-evidence/g21/audit.json').read_text())['kernelSha256']==sha(raw)
(o/'audit.json').write_text(json.dumps(dict(result='G22_PREUSER_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,bsdCallOrder=dict(gate=gate,autoconf=autoconf,findRoot=root,mountroot=mount),actualParkCalls=park,native=False,gpuPixels=False),indent=2)+'\n');print('G22_PREUSER_FULL_KERNEL_PASS')
