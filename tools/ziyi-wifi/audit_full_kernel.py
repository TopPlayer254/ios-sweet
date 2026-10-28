"""Audit the actual compiled full kernel, retaining the original 75 scopes."""
from pathlib import Path
import hashlib,json,os,struct,sys
ROOT=Path.cwd();sys.path.insert(0,str(ROOT/'tools/ziyi-hw'))
from macho_text import text_symbols,function_bytes
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 xnu=ROOT/'dxb/xnu';kit=ROOT/'tools/ziyi-wifi/full-kernel';out=ROOT/'u1-evidence/wifi-native'
 manifest=json.loads((kit/'manifest.json').read_text());baseline=json.loads((out/'base-75-manifest.json').read_text());assert len(baseline)==75
 changes={r['path']:r for r in manifest['files'] if not r['new']};new={r['path']:r for r in manifest['files'] if r['new']}
 scopes=[]
 for name,row in baseline.items():
  data=(xnu/name).read_bytes();expected=changes[name]['after'] if name in changes else row['after'];assert sha(data)==expected,name
  scopes.append(dict(path=name,sha256=sha(data),unchanged=name not in changes))
 assert sum(r['unchanged'] for r in scopes)==73
 for row in manifest['files']:
  data=(xnu/row['path']).read_bytes();assert sha(data)==row['after'],row['path']
  dest=out/'sources'/row['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
 for name in ['bsd/conf/files.arm64','iokit/conf/files.arm64']:
  text=(xnu/name).read_text();units=manifest['cUnits'] if name.startswith('bsd') else manifest['cppUnits']
  for n in units:assert text.count(n+' ')==1,n
 for name in ['iokit/conf/files.arm64','makedefs/MakeInc.def']:
  assert sha((kit/'before'/name).read_bytes())==changes[name]['before']
 text=(xnu/'bsd/kern/bsd_init.c').read_text();hook=manifest['startupHook'];assert text.count(hook)==1
 assert sha(text.replace(hook+'\n','',1).encode())==baseline['bsd/kern/bsd_init.c']['after']
 # The original G8 adapter sources and the real API implementation remain
 # byte-identical; new Wi-Fi sources do not replace the working UI/input/SMP.
 hw=json.loads((ROOT/'tools/ziyi-hw/manifest.json').read_text())
 for row in hw['sources']:assert sha((xnu/row['target']).read_bytes())==row['sha256']
 for row in hw['unchangedAPISources']:assert sha((xnu/row['path']).read_bytes())==row['sha256']
 kernel=ROOT/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet';data=kernel.read_bytes();parsed=text_symbols(data)
 names=hw['linkedFunctions']+['_hakios_wifi_init','_hakios_sta_ask_join_standing','_lx_start','_lx_transmit',
  '_lx_os_qmi_target','_zwn_pas_probe','_zwn_pas_start','_zwn_qmp_open','_zws_select','_ziyi_wifi_image_file','_HakiosWLANStart']
 bodies=[]
 for name in names:
  body=function_bytes(data,parsed,name);body['sampleSHA256']=sha(body.pop('sample'));body['address']=hex(body['address']);bodies.append(body)
 # Firmware section must be file-backed, 16 KiB aligned and non-executable.
 at=32;slots=[]
 for _ in range(struct.unpack_from('<I',data,16)[0]):
  cmd,size=struct.unpack_from('<II',data,at)
  if cmd==0x19:
   va,vs,off,fs=struct.unpack_from('<4Q',data,at+24);prot,n=struct.unpack_from('<II',data,at+60)
   for i in range(n):
    pos=at+72+80*i
    if data[pos:pos+16].split(b'\0')[0]!=b'__ziyi_wifi':continue
    addr,length,fileoff,align=struct.unpack_from('<QQII',data,pos+32);flags=struct.unpack_from('<I',data,pos+64)[0]
    assert length==0xc00000 and align>=14 and flags&255==0 and not prot&4 and addr%16384==fileoff%16384==0
    assert off<=fileoff and fileoff+length<=off+fs<=len(data) and fileoff-off==addr-va
    assert data[fileoff:fileoff+16]==b'ZIYI_WIFI_SLOT1\0';slots.append(dict(offset=fileoff,bytes=length,address=hex(addr)))
  at+=size
 assert len(slots)==1
 report=dict(result='WIFI_NATIVE_FULL_XNU_SOURCE_LINK_PASS',commit=os.environ['GITHUB_SHA'],kernelSHA256=sha(data),baselineScopes=scopes,
  addedFiles=list(new),linkedFunctions=bodies,privateFirmwareSlots=slots,nativeRadio=False,association=False,phoneOperated=False)
 (out/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(result=report['result'],baseline73Unchanged=True,addedFiles=len(new),linkedFunctions=len(bodies))))
if __name__=='__main__':main()
