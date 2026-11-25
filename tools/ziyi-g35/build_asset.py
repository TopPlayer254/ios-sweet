"""Pack typed shader/texture/depth commands into the same G33/G34 owned pages."""
from pathlib import Path
import hashlib,importlib.util,json,struct,zlib
S=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('g35_review',S/'typed_capture.py');review=importlib.util.module_from_spec(sp);sp.loader.exec_module(review)
def build(capture,mesa,out):
 capture=Path(capture);out=Path(out);assert not out.exists();v=review.typed(capture,mesa)
 m=json.loads((capture/'submit.json').read_text());bos={b['index']:b for b in m['bos']}
 original={i:(capture/b['file']).read_bytes()for i,b in bos.items()};mutable={i:bytearray(b)for i,b in original.items()}
 assert m['commands']==[dict(boIndex=7,offset=0,bytes=840,type=1)]
 # The three page identities and all page-table mappings stay exactly G34.
 # Ring[0x1000:0x3000] becomes writable D16. Ring[0x3000:0x4000] is the
 # captured event scratch/border-color block. Target remains GPU-only output.
 pieces=[dict(bo=0,start=0x4000,bytes=4096,new=0x61003000,kind='scratch-border'),
         dict(bo=3,start=0,bytes=16384,new=0x61004000,kind='GPU-color-output'),
         dict(bo=4,start=0,bytes=8192,new=0x61001000,kind='GPU-D16-output')]
 intervals={}
 def need(bo,start,n):
  assert n>0 and 0<=start and start+n<=len(original[bo]);intervals.setdefault(bo,[]).append((start,start+n))
 for r in v['relocations']:
  if r['sourceBO']not in [0,3,4]:need(r['sourceBO'],r['sourceOffset'],max(r['bytes'],1))
 for stream in v['streams']:
  if stream['kind']!='ROOT':need(stream['boIndex'],stream['address']-bos[stream['boIndex']]['iova'],stream['bytes'])
 cursor=0x400
 for bo,spans in sorted(intervals.items()):
  merged=[]
  for lo,hi in sorted(spans):
   if merged and lo<=merged[-1][1]:merged[-1]=(merged[-1][0],max(hi,merged[-1][1]))
   else:merged.append((lo,hi))
  for lo,hi in merged:
   # Preserve the low seven address bits of every byte, including shader/UBO
   # alignment, while compacting only unreferenced padding between ranges.
   cursor=(cursor+127)&~127;cursor+=lo&127
   assert cursor+hi-lo<=16384,('Owned fence page capacity',bo,lo,hi,cursor)
   pieces.append(dict(bo=bo,start=lo,bytes=hi-lo,new=0x61008000+cursor,kind='typed-input'))
   cursor+=hi-lo
 def translate(address,n):
  a=[p for p in pieces if bos[p['bo']]['iova']+p['start']<=address and
     address+max(n,1)<=bos[p['bo']]['iova']+p['start']+p['bytes']]
  assert len(a)==1,('Unplanned typed range',hex(address),n)
  p=a[0];return p['new']+address-bos[p['bo']]['iova']-p['start']
 def location(address):
  a=[b for b in bos.values()if b['iova']<=address and address+4<=b['iova']+b['bytes']]
  assert len(a)==1;return a[0]['index'],address-a[0]['iova']
 changed=set();inverse=[];pointers=[]
 for r in v['relocations']:
  lo_bo,lo=location(r['lowAddress']);hi_bo,hi=location(r['highAddress']);assert lo_bo==hi_bo
  assert(lo_bo,lo)not in changed and(hi_bo,hi)not in changed;changed.update([(lo_bo,lo),(hi_bo,hi)])
  oldlo=struct.unpack_from('<I',mutable[lo_bo],lo)[0];oldhi=struct.unpack_from('<I',mutable[hi_bo],hi)[0]
  assert(oldlo&r['lowMask'])|((oldhi&r['highMask'])<<32)==r['oldAddress']
  new=translate(r['oldAddress'],r['bytes']);assert not(new&~r['lowMask']&0xffffffff)
  struct.pack_into('<I',mutable[lo_bo],lo,(oldlo&~r['lowMask'])|(new&r['lowMask']))
  struct.pack_into('<I',mutable[hi_bo],hi,(oldhi&~r['highMask'])|((new>>32)&r['highMask']))
  inverse.extend([(lo_bo,lo,oldlo),(hi_bo,hi,oldhi)]);pointers.append(dict(**r,newAddress=new))
 restored={i:bytearray(b)for i,b in mutable.items()}
 for i,off,value in inverse:struct.pack_into('<I',restored[i],off,value)
 assert all(bytes(restored[i])==original[i]for i in original)
 shaders=[]
 for row in v['shaderPreloads']:
  b=next(b for b in bos.values()if b['iova']<=row['address']<b['iova']+b['bytes']);off=row['address']-b['iova']
  code=bytes(mutable[b['index']][off:off+row['bytes']]);assert hashlib.sha256(code).hexdigest()==row['sha256']
  shaders.append(dict(**row,newAddress=translate(row['address'],row['bytes']),crc32=zlib.crc32(code)))
 root=bytearray()
 for p in v['packets']:
  if p['kind']!='ROOT'or p['opcode']==0x55:continue
  off=p['address']-bos[7]['iova'];root+=mutable[7][off:off+(len(p['words'])+1)*4]
 assert len(root)==792
 def p7(op,n):return 0x70000000|n|(1^(n.bit_count()&1))<<15|op<<16|(1^(op.bit_count()&1))<<23
 root+=struct.pack('<11I',p7(0x46,4),0x1d,0x61008014,0,0xc0351001,p7(0x46,4),4,0x61008018,0,0xc0351002,p7(0x26,0))
 begin=303;end=begin+len(root)//4;assert end==512
 ring=bytearray((0xc5^(i*29&255))for i in range(16384));fence=bytearray((0x35^(i*31&255))for i in range(16384))
 ring[:begin*4]=bytes(begin*4);fence[:256]=bytes(256);ring[begin*4:end*4]=root
 for p in pieces:
  if p['bo']==3:continue # no CPU writes to the output color image
  dst=ring if p['new']<0x61004000 else fence;base=0x61000000 if dst is ring else 0x61008000
  offset=p['new']-base;assert offset>=0 and offset+p['bytes']<=len(dst)
  dst[offset:offset+p['bytes']]=mutable[p['bo']][p['start']:p['start']+p['bytes']]
 assert all(p['new']%128==(bos[p['bo']]['iova']+p['start'])%128 for p in pieces)
 identity=hashlib.sha256(ring+fence).digest();out.mkdir()
 (out/'ring-template.bin').write_bytes(ring);(out/'fence-template.bin').write_bytes(fence)
 def array(name,b):return f'static const uint8_t {name}[{len(b)}] __attribute__((aligned(128)))={{\n'+''.join(','.join(f'0x{x:02x}'for x in b[i:i+16])+',\n'for i in range(0,len(b),16))+'};\n'
 text='/* Own captured A662 texture/blend/depth packet; no firmware or device IO. */\n'
 text+='#define G35_RING_BEGIN 303u\n#define G35_RING_END 512u\n#define G35_TOKEN0_WORD 505u\n#define G35_TOKEN1_WORD 510u\n'
 text+='#define G35_DEPTH_OFFSET 0x1000u\n#define G35_DEPTH_BYTES 8192u\n#define G35_TEXTURE_BYTES 1024u\n'
 text+=array('g35_asset_identity',identity)+array('g35_ring_template',ring)+array('g35_fence_template',fence)
 (out/'primitives_asset.inc').write_bytes(text.encode())
 record=dict(result='G35_TYPED_ASSET_PASS',chipId='06060201',begin=begin,end=end,rootWords=len(root)//4,
  pieces=pieces,relocations=pointers,shaderPreloads=shaders,assetIdentity=identity.hex(),
  sourceIdentity='2535b7a5e17280021e5b445994ae2b0e0d9242e7a39197a4598e4e190702b8f3',
  inverseExact=True,shaderCodeByteUnchanged=True,rawScanUsedForRelocation=False,noNewPages=True,
  noColorTargetCPUWrites=True,ownedFenceUsedBytes=cursor,depthBytes=8192,textureBytes=1024,
  nativeExecution=False,phoneOperated=False,ringTemplateSha256=hashlib.sha256(ring).hexdigest(),
  fenceTemplateSha256=hashlib.sha256(fence).hexdigest(),cIncludeSha256=hashlib.sha256(text.encode()).hexdigest())
 (out/'asset.json').write_bytes((json.dumps(record,indent=2)+'\n').encode())
 print('G35_TYPED_ASSET_PASS',len(pointers),'typed pointers; fence bytes',cursor,'of16384')
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('capture');p.add_argument('--mesa',required=True);p.add_argument('--out',required=True);a=p.parse_args();build(a.capture,a.mesa,a.out)
