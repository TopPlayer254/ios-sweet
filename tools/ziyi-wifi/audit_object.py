"""Record the actual linked driver, strong ABI and all remaining donor stubs.

Compilation and strong symbols are build evidence, never scan/join evidence.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,struct

def main():
 ap=argparse.ArgumentParser();ap.add_argument('stage',type=Path);a=ap.parse_args();stage=a.stage.resolve()
 folder=stage/'xnu-deps/build/ath11k/out';obj=folder/'ath11k.o';assert obj.is_file()
 data=obj.read_bytes();assert struct.unpack_from('<I',data)[0]==0xfeedfacf
 at=32;platforms=[]
 for _ in range(struct.unpack_from('<I',data,16)[0]):
  cmd,size=struct.unpack_from('<II',data,at);assert size>=8 and at+size<=len(data)
  if cmd==0x32:platforms.append(struct.unpack_from('<I',data,at+8)[0])
  at+=size
 assert platforms==[1], 'Driver must match the actual macOS-platform XNU kernel object ABI'
 symbols=subprocess.check_output(['llvm-nm','--defined-only',str(obj)],text=True)
 strong=[]
 for name in ['_lx_prepare','_lx_start','_lx_stop','_lx_transmit']:
  lines=[l for l in symbols.splitlines() if l.split()[-1:]==[name]]
  assert len(lines)==1 and lines[0].split()[-2]=='T',(name,lines)
  strong.append(name)
 stubs=(folder/'stubs.txt').read_text().splitlines()
 open_join=json.loads((stage/'open-join-adaptation.json').read_text())
 station=(stage/'wifi/lx/src/lx_80211.c').read_bytes()
 assert open_join['result']=='ZIYI_OPEN_JOIN_SOURCE_ADAPTED'
 assert hashlib.sha256(station).hexdigest()==open_join['sourceAfterSHA256']
 assert hashlib.sha256((stage/'wifi/lx/src/ziyi_open_join.h').read_bytes()).hexdigest()==open_join['headerSHA256']
 assert b'OPEN association accepted; no RSN, handshake or keys required' in data
 report=dict(result='WIFI_ATH11K_MACHO_BUILD_PASS',sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),bytes=obj.stat().st_size,
  machoBuildPlatforms=platforms,
  strongExports=strong,remainingDonorStubs=stubs,
  undefined=subprocess.check_output(['llvm-nm','-u','--format=just-symbols',str(obj)],text=True).splitlines(),
  donorAdaptation=json.loads((stage/'build-adaptation.json').read_text()),
  qmiTargetAdaptation=json.loads((stage/'qmi-target-adaptation.json').read_text()),openJoinAdaptation=open_join,
  linkedIntoXNU=False,nativeRadio=False,scan=False,association=False,datapath=False,
  remaining='Review stubs and native adapter, then link with real XNU and test WPSS/QMI/scan/join/data')
 (folder/'ziyi-build-audit.json').write_text(json.dumps(report,indent=2)+'\n')
 assert '_lx_os_qmi_target' in report['undefined'], 'QMI target binding callback must remain a real kernel dependency'
 print(json.dumps(dict(result=report['result'],bytes=report['bytes'],strongExports=strong,stubs=len(stubs))))
if __name__=='__main__':main()
