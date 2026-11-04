"""Exact UI implementation review beside the historical 75 kernel scopes."""
from pathlib import Path
import hashlib,json,struct,sys
def sha(b):return hashlib.sha256(b).hexdigest()
def body(data,parsed,name):
 entries=parsed['symbols'].get(name,[]);ids={(e['address'],e['section']) for e in entries};assert len(ids)==1,(name,'missing/ambiguous')
 addr,index=ids.pop();section=parsed['sections'][index];following=[v for v in parsed['addresses'][index] if v>addr];stop=min(following) if following else section['address']+section['bytes'];off=section['offset']+addr-section['address'];return addr,data[off:off+stop-addr]
def branch_to(data,parsed,caller,target,link):
 addr,code=body(data,parsed,caller);dest,_=body(data,parsed,target);found=[]
 for off in range(0,len(code)-3,4):
  w=struct.unpack_from('<I',code,off)[0]
  allowed=(0x94000000,0x14000000) if link is None else (0x94000000 if link else 0x14000000,)
  if w&0xfc000000 not in allowed:continue
  imm=w&0x3ffffff;imm=(imm^0x2000000)-0x2000000
  if addr+off+4*imm==dest:found.append(hex(addr+off))
 assert found,(caller,target,'required actual branch absent');return found
def validate_kernel(data,parsed,function_bytes):
 method='__ZN13SweetHIDTouch10touchFrameEPKjS1_j';aks='__ZN23AppleKeyStoreUserClient14externalMethodEjP25IOExternalMethodArgumentsP24IOExternalMethodDispatchP8OSObjectPv'
 assert b'capacity %u/%u returned %u/%u calls %llu -> 0x%x' in data
 receipts=[]
 for name in [method,aks,'_zt_thread']:
  r=function_bytes(data,parsed,name);r['sampleSHA256']=sha(r.pop('sample'));r['address']=hex(r['address']);receipts.append(r)
 address,wrapper=body(data,parsed,'_sweet_hid_touch_frame');assert 16<=len(wrapper)<=128
 # The C wrapper returns int while touchFrame returns bool. ARM64 may use
 # BL + epilogue rather than a tail B; both must reach the exact method.
 return dict(linkedFunctions=receipts,wrapper=dict(address=hex(address),bytes=len(wrapper),sha256=sha(wrapper),actualCalls=branch_to(data,parsed,'_sweet_hid_touch_frame',method,None)),actualDriverCalls=branch_to(data,parsed,'_zt_thread','_sweet_hid_touch_frame',True))
def audit(root,data,parsed):
 root=Path(root);sys.path.insert(0,str(root/'tools/ziyi-hw'));from macho_text import function_bytes
 kit=root/'tools/ziyi-ui';m=json.loads((kit/'manifest.json').read_text());assert m['baseCommit']=='df4b84452d5ffb35ab475e8fa1739dc0cf927ac4' and len(m['sources'])==3
 out=root/'u1-evidence/ui-reviewed';out.mkdir(exist_ok=False);sources=[]
 baseline=json.loads((root/'tools/ziyi-ufs/manifest.json').read_text());assert len(baseline)==75
 for row in m['sources']:
  raw=(root/'dxb/xnu'/row['target']).read_bytes();assert raw==(root/row['repository']).read_bytes() and sha(raw)==row['afterSHA256'],row['target']
  if row['target'] in baseline:assert baseline[row['target']]['after']==row['afterSHA256']
  dest=out/'sources'/row['target'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw);sources.append(dict(path=row['target'],sha256=sha(raw),bytes=len(raw)))
 host=json.loads((root/'u1-evidence/ui-public-host/audit.json').read_text());assert host['result']=='UI_PUBLIC_HOST_PASS' and len(host['tests'])==2
 r=validate_kernel(data,parsed,function_bytes);r.update(result='UI_REVIEWED_SOURCE_AND_LINK_PASS',sources=sources,baselineScopesStill75=True,additionalLegacyScope='iokit/Kernel/arm/SweetKeyStore.cpp',inputFrameCompiled=True,aksBoundedPollCompiled=True,nativeOutcomeProven=False)
 (out/'audit.json').write_text(json.dumps(r,indent=2)+'\n');return r
