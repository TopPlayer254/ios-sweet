"""G35 typed A6xx pointer review, including bindless and texture descriptors.

Only pinned XML fields and explicit packet semantics yield relocations.
Numerical scanner candidates are diagnostic data and never drive this code.
"""
from pathlib import Path
import json,struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'gpu-g34-triangle-20261125'))
from inspect_capture import inspect
from typed_capture import register_map

def typed(capture,mesa):
 capture=Path(capture);mesa=Path(mesa);v=inspect(capture)
 regs,address_regs=register_map(mesa/'src/freedreno/registers/adreno/a6xx.xml')
 m=json.loads((capture/'submit.json').read_text());bos={b['index']:b for b in m['bos']}
 data={i:(capture/b['file']).read_bytes()for i,b in bos.items()}
 omitted=[p for p in v['packets']if p['opcode']==0x55]
 assert [p['words']for p in omitted]==[[0x25000,1,0x10006e],[0,0,0],[0,0,0x200000]]
 reloc=[];writes=[];draws=[];bindless=[]
 def containing(addr,n):
  a=[b for b in bos.values()if b['iova']<=addr and addr+max(n,1)<=b['iova']+b['bytes']]
  assert len(a)==1,('Unbound typed address',hex(addr),n);return a[0]
 def pointer(atlo,athi,lo,hi,n,kind,lowmask=0xffffffff,highmask=0xffffffff):
  addr=(lo&lowmask)|((hi&highmask)<<32)
  if not addr:return
  b=containing(addr,n)
  reloc.append(dict(lowAddress=atlo,highAddress=athi,oldAddress=addr,bytes=n,kind=kind,
   lowMask=lowmask,highMask=highmask,sourceBO=b['index'],sourceOffset=addr-b['iova']))
 def add(p,lo,hi,n,kind,lowmask=0xffffffff,highmask=0xffffffff):
  pointer(p['address']+4*(lo+1),p['address']+4*(hi+1),p['words'][lo],p['words'][hi],n,kind,lowmask,highmask)
 permitted={0x26,0x43,0x55,0x5c,0x65,0x1d,0x64,0x63,0x3f,0x32,0x34,0x38,0x46,0x47,0x2c}
 for p in v['packets']:
  op=p['opcode'];w=p['words'];local={}
  if op==0x55:continue
  if op=='REG':local={p['reg']+i:(i,value)for i,value in enumerate(w)}
  elif op==0x5c:
   assert len(w)%2==0
   for i in range(0,len(w),2):assert w[i]not in local;local[w[i]]=(i+1,w[i+1])
  else:assert op in permitted,('Unreviewed packet',op)
  for reg,(i,value)in local.items():
   assert reg in regs and(reg>=0x8000 or reg==0xe12),(hex(reg),regs.get(reg))
   writes.append(dict(packetAddress=p['address'],reg=reg,name=regs[reg],value=value))
   if reg in address_regs:
    assert reg+1 in local;add(p,i,local[reg+1][0],1,address_regs[reg])
   # a6xx.xml SP_GFX_BINDLESS_BASE/HLSQ_BINDLESS_BASE: bits0..1 DESC_SIZE,
   # bits2..63 ADDR. A captured descriptor size3 is exactly64bytes, two entries.
   if reg in [0xab10,0xbb20]:
    assert value&3==3 and reg+1 in local
    add(p,i,local[reg+1][0],128,'BINDLESS_DESCRIPTOR_TABLE',0xfffffffc)
    bindless.append((value&0xfffffffc)|local[reg+1][1]<<32)
  if op==0x3f:assert len(w)==3;add(p,0,1,w[2]*4,'IB')
  if op==0x43:
   assert len(w)%3==0
   for i in range(0,len(w),3):add(p,i+1,i+2,(w[i]&65535)*4,'DRAW_STATE')
  if op in [0x32,0x34]:
   mode=(w[0]>>16)&3;typ=(w[0]>>14)&3;block=(w[0]>>18)&15;units=w[0]>>22
   if mode==2:
    assert typ in [0,1];add(p,1,2,units*(128 if typ==0 else 16),'SHADER'if typ==0 else'CONSTANTS')
   elif mode==0:
    assert w[1]==w[2]==0
    if typ==2:
     assert len(w)==5 and units==1;add(p,3,4,(w[4]>>17)*16,'UBO',highmask=0x1ffff)
    else:assert typ==1 and len(w)==3+units*4
   elif mode==1:
    # Explicit bindless texture0/sampler1 in the one reviewed table.
    assert block==4 and units==1 and typ in [0,1]and w[1:]==([0,0]if typ==1 else[16,0])
   else:raise AssertionError(('Unsupported state mode',w))
  if op==0x46:
   assert len(w)in [1,4]
   if len(w)==4:add(p,1,2,4,'EVENT_WRITE')
  if op==0x47:assert w in [[0x34000000,37],[0x38000000,85]] # no address field
  if op==0x2c:assert w==[3] # CP_BLIT / BLIT_OP_SCALE for both attachment clears
  if op==0x38:assert w==[0xd84,1,3];draws.append(p['address'])
 assert len(draws)==4 and len(bindless)==6 and all(x==bindless[0]for x in bindless)
 at=bindless[0];b=containing(at,128);off=at-b['iova'];desc=struct.unpack_from('<32I',data[b['index']],off)
 # a6xx_descriptors.xml: TILE_MODE[1:0], single-plane dimensions/pitch,
 # BASE_LO[31:5], BASE_HI[16:0], DEPTH[29:17], FLAG[28]. No compressed input.
 assert desc[0]&3==0 and not(desc[3]&(1<<28))
 assert desc[1]&0x7fff==2 and(desc[1]>>15)&0x7fff==2 and(desc[2]>>7)&0x3fffff==256
 assert desc[5]>>17==1 and desc[7]==desc[8]==0 and not any(desc[9:16])
 assert desc[16:]==(0x920,)+(0,)*15
 pointer(at+16,at+20,desc[4],desc[5],1024,'TEXTURE_2X2_LINEAR_RGBA',0xffffffe0,0x1ffff)
 assert len(v['shaderPreloads'])==9 and[(s['stage'],s['bytes'])for s in v['shaderPreloads']]==[(8,256),(8,256),(12,128)]*3
 v.update(result='G35_TYPED_CAPTURE_REVIEW_PASS',relocations=reloc,registerWrites=writes,draws=draws,
  omittedContextAmbleRegistrations=omitted,typed=True,rawScanUsedForRelocation=False,
  textureDescriptor=dict(address=at,bytes=128,textureBytes=1024,rowPitch=256),nativeExecution=False)
 return v

if __name__=='__main__':
 import argparse
 a=argparse.ArgumentParser();a.add_argument('capture');a.add_argument('--mesa',type=Path,required=True);a.add_argument('--out',type=Path,required=True);q=a.parse_args()
 assert not q.out.exists();v=typed(q.capture,q.mesa);q.out.write_bytes((json.dumps(v,indent=2)+'\n').encode())
 print('G35_TYPED_CAPTURE_REVIEW_PASS',len(v['relocations']),'typed pointers')
