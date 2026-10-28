"""Compile the station against the genuine XNU buildtree with Apple Clang.

The final relocatable object resolves the LinuxKPI/XNU station ABI. Remaining
platform/firmware/kernel imports are inventoried, not replaced by stubs.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--tree',type=Path,required=True);ap.add_argument('--driver',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
 source=Path(__file__).resolve().parent;tree=a.tree.resolve();out=a.out.resolve();out.mkdir(exist_ok=False)
 overlay=out/'overlay';overlay.mkdir();shutil.copytree(source/'station/bsd/sys',overlay/'sys')
 cfg=overlay/'pexpert/arm64/SWEET.h';cfg.parent.mkdir(parents=True);shutil.copyfile(tree/'xnu/pexpert/pexpert/arm64/SWEET.h',cfg)
 flags=json.loads((source/'xnu-compile-flags.json').read_text());flags=[f.replace('@TREE@',str(tree)).replace('@OVERLAY@',str(overlay)) for f in flags]
 flags+=['-I'+str(source/'native'),'-I'+str(source/'native-kernel')]
 native_manifest=json.loads((source/'native-kernel/manifest.json').read_text())
 for row in native_manifest['files']:assert hashlib.sha256((source/'native-kernel'/row['path']).read_bytes()).hexdigest()==row['sha256']
 cc=subprocess.check_output(['xcrun','--find','clang'],text=True).strip();rows=[];objects=[]
 units=[source/'station/bsd/dev'/('hakios.'+n+'.c') for n in ['lx','net','qmi','sta','apple80211']]
 units+=[source/'native/bdf.c',source/'native/bdf_kernel.c',source/'station/iokit/Kernel/hakios.wlan.cpp']
 units+=[source/'native-kernel'/n for n in ['hakios.wifi.c','binding.c','scm.c','qmp.c','smmu.c','capsule.c','kernel.c']]
 cwd=tree/'build/xnu.obj/DEVELOPMENT_ARM64_SWEET/bsd'
 for src in units:
  obj=out/(src.name+'.o');extra=[]
  if src.suffix=='.cpp':extra=['-std=gnu++17','-fno-exceptions','-fno-rtti','-fapple-kext']
  cmd=[cc,*flags,*extra,'-c',str(src),'-o',str(obj)]
  r=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True);(out/(src.name+'.log')).write_text(r.stdout+r.stderr)
  row=dict(source=str(src.relative_to(source)),exitCode=r.returncode,sha256=hashlib.sha256(src.read_bytes()).hexdigest());rows.append(row)
  if r.returncode:print(src.name,r.stderr[-6000:]);continue
  row['objectSHA256']=hashlib.sha256(obj.read_bytes()).hexdigest();objects.append(obj)
 report=dict(result='WIFI_STATION_APPLE_COMPILE_PASS' if len(objects)==len(units) else 'WIFI_STATION_APPLE_COMPILE_FAILED',
  units=rows,treeSWEET_SHA256=hashlib.sha256(cfg.read_bytes()).hexdigest(),flags=flags,phoneOperated=False,nativeRadio=False,fullXNUKernelLink=False)
 (out/'compile.json').write_text(json.dumps(report,indent=2)+'\n')
 assert len(objects)==len(units),'Station compile failed; real diagnostics retained'
 found=list(a.driver.rglob('ath11k.o'));assert len(found)==1;driver=found[0]
 audits=list(a.driver.rglob('ziyi-build-audit.json'));assert len(audits)==1;d=json.loads(audits[0].read_text());assert hashlib.sha256(driver.read_bytes()).hexdigest()==d['sha256'] and not d['remainingDonorStubs']
 combined=out/'ziyi-wifi-station.o';linker=subprocess.check_output(['xcrun','--find','ld'],text=True).strip()
 r=subprocess.run([linker,'-r','-arch','arm64',*[str(o) for o in objects],str(driver),'-o',str(combined)],capture_output=True,text=True)
 (out/'link.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
 undefined=subprocess.check_output(['nm','-u',str(combined)],text=True).splitlines()
 unresolved={l.split()[-1] for l in undefined if l.split()}
 assert not unresolved.intersection({'_lx_start','_lx_os_qmi_target','_ziyi_wifi_native_binding','_ziyi_wifi_image_file',
  '_hakios_wifi_driver_file','_hakios_wifi_dsp_is_up','_hakios_wifi_radio_map','_hakios_wifi_radio_address',
  '_hakios_wifi_network','_hakios_wifi_option','_hakios_wifi_say','_hakios_wifi_tell','_zws_select','_zwn_pas_start','_zwn_qmp_open'})
 report.update(result='WIFI_STATION_ATH11K_APPLE_LINK_PASS',combinedObjectSHA256=hashlib.sha256(combined.read_bytes()).hexdigest(),
  combinedBytes=combined.stat().st_size,remainingImports=undefined,driverSHA256=d['sha256'],
  remainingNative='Full XNU kernel link/startup integration, private capsule attachment and native lifecycle tests; no radio startup yet')
 (out/'link.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(result=report['result'],units=len(objects),bytes=report['combinedBytes'],nativeRadio=False)))
if __name__=='__main__':main()
