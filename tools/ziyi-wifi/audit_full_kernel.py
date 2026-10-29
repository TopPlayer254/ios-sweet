"""Audit the actual compiled full kernel, retaining the original 75 scopes."""
from pathlib import Path
import hashlib,json,os,struct,sys
ROOT=Path.cwd();sys.path.insert(0,str(ROOT/'tools/ziyi-hw'))
from macho_text import text_symbols,function_bytes
def sha(b):return hashlib.sha256(b).hexdigest()
def audited_function(data,parsed,name):
 if name!='_lx_transmit':return function_bytes(data,parsed,name)
 # Actual pinned ath11k source is a six-instruction conditional tail call:
 # driver != NULL ? lx_station_send(frame,length) : ENETDOWN. Qualify those
 # instructions and the real destination rather than lowering all size gates.
 entries=parsed['symbols'].get(name,[]);identities={(e['address'],e['section']) for e in entries};assert len(identities)==1
 address,index=identities.pop();section=parsed['sections'][index]
 following=[v for v in parsed['addresses'][index] if v>address];stop=min(following) if following else section['address']+section['bytes'];assert stop-address==24
 offset=section['offset']+address-section['address'];body=data[offset:offset+24];ins=struct.unpack('<6I',body)
 assert ins[0]&0x9f00001f==0x90000008 and ins[1]&0xffc003ff==0xf9400108
 assert ins[2]==0xb4000048 and ins[3]&0xfc000000==0x14000000 and ins[4:]==(0x52800640,0xd65f03c0)
 imm=ins[3]&0x3ffffff;imm=(imm^0x2000000)-0x2000000;target=address+12+4*imm
 sent=function_bytes(data,parsed,'_lx_station_send');assert sent['address']==target
 pageimm=((ins[0]>>5)&0x7ffff)<<2|((ins[0]>>29)&3);pageimm=(pageimm^0x100000)-0x100000
 driver=(address&~4095)+(pageimm<<12)+((ins[1]>>10)&4095)*8
 at=32;owned=False
 for _ in range(struct.unpack_from('<I',data,16)[0]):
  cmd,size=struct.unpack_from('<II',data,at)
  if cmd==0x19:
   va,vs=struct.unpack_from('<QQ',data,at+24);prot=struct.unpack_from('<I',data,at+60)[0]
   if va<=driver and driver+8<=va+vs and prot&3==3 and not prot&4:owned=True
  at+=size
 assert owned,'TX wrapper driver pointer outside non-executable writable kernel data'
 return dict(name=name,address=address,section=index,segment=section['segment'],bytesToNextTextSymbol=24,sample=body,
  exactConditionalTailCall=True,tailCallTarget='_lx_station_send',tailCallTargetBytes=sent['bytesToNextTextSymbol'])
def main():
 xnu=ROOT/'dxb/xnu';kit=ROOT/'tools/ziyi-wifi/full-kernel';out=ROOT/'u1-evidence/wifi-native'
 manifest=json.loads((kit/'manifest.json').read_text());baseline=json.loads((out/'base-75-manifest.json').read_text());assert len(baseline)==75
 changes={r['path']:r for r in manifest['files'] if not r['new']};new={r['path']:r for r in manifest['files'] if r['new']}
 scopes=[]
 for name,row in baseline.items():
  data=(xnu/name).read_bytes();expected=changes[name]['after'] if name in changes else row['after'];assert sha(data)==expected,name
  scopes.append(dict(path=name,sha256=sha(data),unchanged=name not in changes))
 assert sum(r['unchanged'] for r in scopes)==72
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
 cache=(xnu/'osfmk/arm/pmap/pmap.c').read_text()
 assert cache.count('flush_dcache((vm_offset_t)pp << PAGE_SHIFT, PAGE_SIZE, TRUE);')==1
 assert '(vm_offset_t) (pp << PAGE_SHIFT)' not in cache
 ufs=(xnu/'bsd/dev/sweet_ufs.c').read_text()
 assert ufs.count('zws_storage_coexists(&a,&b,u1.owned?&u1.identity:&a)')==1
 assert 'for(unsigned i=0;i<ZWS_SMRS;i++)' in ufs
 assert ufs.count('zws_storage_epoch()')==2 and 'if(epoch&1u)' in ufs
 wifi=(xnu/'bsd/dev/hakios.wifi.c').read_text()
 assert wifi.count('zws_storage_write_begin()')==1 and wifi.count('zws_storage_write_end(storage_token)')==1
 assert 'if(zws_poisoned)goto out;' in wifi and 'radio_space.on=true;ok=true;' in wifi
 assert '!zws_context_matches(actual,wanted)' in wifi
 assert wifi.index('zyw_status_thread(')<wifi.index('\n#define printf say\n')
 assert '\t\tprintf("%s", line);\n\t\tprintf("%s", line);' not in wifi
 assert 'zyw_mark(ZYW_PREPARE' in wifi and 'zyw_mark(ZYW_CAPSULE' in wifi
 assert 'zyw_remember(line);' in wifi and 'tick<180' in wifi
 context=(xnu/'bsd/dev/ziyi_wifi_smmu.c').read_text()
 assert 'w[0]!=0x127u||w[1]!=0x38071u' in context
 assert '#define SMMU_SCTLR_ON           (1u << 0 | 1u << 1 | 1u << 2 | 1u << 5 | 1u << 8)' in wifi
 assert '#define SMMU_TCR2               (1u | 1u << 4 | 3u << 5 | 7u << 15)' in wifi
 assert wifi.count('zws_fsr_clean(')==3
 assert 'zyw_map_receipt(b->name,b->phys,b->size,b->base);' in wifi
 assert 'ZWIFI W10 BEGIN step=%u done=%u' in wifi
 assert 'if(!leaving)sleep_ms(500);' in wifi
 assert 'SMMU lease quarantined until coldboot; no old CBAR replay' in wifi
 assert 'restore cbar' not in wifi
 scm=(xnu/'bsd/dev/ziyi_wifi_pas.c').read_text()
 assert 'operation==ZWN_PAS_PROBE&&!s->calls&&function==INFO' in scm
 fsr=(xnu/'bsd/dev/ziyi_wifi_smmu.h').read_text()
 assert 'configured_aarch64?raw==ZWS_FSR_AARCH64:(raw&~ZWS_FSR_FORMAT_MASK)==0' in fsr
 kernel=ROOT/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet';data=kernel.read_bytes();parsed=text_symbols(data)
 names=hw['linkedFunctions']+['_hakios_wifi_init','_hakios_sta_ask_join_standing','_lx_start','_lx_transmit',
  '_lx_os_qmi_target','_zwn_pas_probe','_zwn_pas_start','_zwn_qmp_open','_zws_select','_ziyi_wifi_image_file','_HakiosWLANStart',
  '_zws_storage_lease','_zws_storage_coexists','_zws_context_matches',
  '_zyw_status_thread','_ziyi_wifi_native_diagnostic','_zyw_map_receipt']
 bodies=[]
 for name in names:
  body=audited_function(data,parsed,name);body['sampleSHA256']=sha(body.pop('sample'));body['address']=hex(body['address']);bodies.append(body)
 # Inspect the actual compiled cache helper and all straight-line ppnum
 # argument sequences, including both compiler-inlined copies seen on W5.
 ce=parsed['symbols']['_pmap_sync_page_attributes_phys'];assert len(ce)==1
 cs=parsed['sections'][ce[0]['section']];co=cs['offset']+ce[0]['address']-cs['address']
 assert struct.unpack_from('<3I',data,co)==(0xd3727c00,0x52880001,0x52800022)
 flush=parsed['symbols']['_flush_dcache'][0]['address'];bad=[]
 for sec in parsed['sections'].values():
  for off in range(sec['offset'],sec['offset']+sec['bytes']-15,4):
   w,a,b,br=struct.unpack_from('<4I',data,off)
   if w&0xfffffc1f!=0x53124400 or (a,b)!=(0x52880001,0x52800022) or br&0x7c000000!=0x14000000:continue
   imm=br&0x3ffffff;pc=sec['address']+off-sec['offset']
   if pc+12+4*((imm^0x2000000)-0x2000000)==flush:bad.append(hex(pc))
 assert not bad,('Truncated physical cache-flush arguments',bad)
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
  addedFiles=list(new),linkedFunctions=bodies,privateFirmwareSlots=slots,cachePA64Compiled=True,ufsExactWifiLeaseCompiled=True,storageEpochCompiled=True,wifiMapPoisonCompiled=True,allContextControlsQualified=True,retainedWifiStatusCompiled=True,explicitNativeContextCompiled=True,fsrFormatQualifiedCompiled=True,mapReceiptsCompiled=True,atomicConventionProbeCompiled=True,bootLifetimeLeaseCompiled=True,nativeRadio=False,association=False,phoneOperated=False)
 (out/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(result=report['result'],baseline72Unchanged=True,addedFiles=len(new),linkedFunctions=len(bodies))))
if __name__=='__main__':main()
