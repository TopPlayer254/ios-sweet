"""Record the actual linked driver, strong ABI and all remaining donor stubs.

Compilation and strong symbols are build evidence, never scan/join evidence.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess

def main():
 ap=argparse.ArgumentParser();ap.add_argument('stage',type=Path);a=ap.parse_args();stage=a.stage.resolve()
 folder=stage/'xnu-deps/build/ath11k/out';obj=folder/'ath11k.o';assert obj.is_file()
 symbols=subprocess.check_output(['llvm-nm','--defined-only',str(obj)],text=True)
 strong=[]
 for name in ['_lx_prepare','_lx_start','_lx_stop','_lx_transmit']:
  lines=[l for l in symbols.splitlines() if l.split()[-1:]==[name]]
  assert len(lines)==1 and lines[0].split()[-2]=='T',(name,lines)
  strong.append(name)
 stubs=(folder/'stubs.txt').read_text().splitlines()
 report=dict(result='WIFI_ATH11K_MACHO_BUILD_PASS',sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),bytes=obj.stat().st_size,
  strongExports=strong,remainingDonorStubs=stubs,
  undefined=subprocess.check_output(['llvm-nm','-u','--format=just-symbols',str(obj)],text=True).splitlines(),
  donorAdaptation=json.loads((stage/'build-adaptation.json').read_text()),
  linkedIntoXNU=False,nativeRadio=False,scan=False,association=False,datapath=False,
  remaining='Review stubs and native adapter, then link with real XNU and test WPSS/QMI/scan/join/data')
 (folder/'ziyi-build-audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(dict(result=report['result'],bytes=report['bytes'],strongExports=strong,stubs=len(stubs))))
if __name__=='__main__':main()
