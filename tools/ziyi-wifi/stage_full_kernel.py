"""Stage only the declared Wi-Fi delta in the isolated GitHub runner."""
from pathlib import Path
import argparse,hashlib,json,os,shutil
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--driver',type=Path,required=True);a=ap.parse_args();root=Path.cwd();kit=root/'tools/ziyi-wifi/full-kernel';out=root/'u1-evidence/wifi-native';out.mkdir(exist_ok=False)
 assert os.environ['MACHINE_CONFIG']=='SWEET' and os.environ['MACOS_VERSION']=='13.5'
 m=json.loads((kit/'manifest.json').read_text());patch=kit/'0089-ziyi-wifi-native.patch';assert patch.is_file()
 objects=list(a.driver.rglob('ath11k.o'));audits=list(a.driver.rglob('ziyi-build-audit.json'));assert len(objects)==len(audits)==1
 audit=json.loads(audits[0].read_text());assert sha(objects[0])==audit['sha256'] and not audit['remainingDonorStubs'] and audit['machoBuildPlatforms']==[1]
 expected=root/'tools/ziyi-ufs/manifest.json';old=expected.read_bytes();(out/'base-75-manifest.json').write_bytes(old);v=json.loads(old)
 # Existing baseline verifier still reads every original scope. Only two
 # original files change, each with an exact inverse and recorded before hash.
 changed=[]
 for row in m['files']:
  if row['new']:assert sha(kit/'files'/row['path'])==row['after'];continue
  assert sha(kit/'before'/row['path'])==row['before']
  if row['path'] in v:
   assert v[row['path']]['after']==row['before'];v[row['path']]['after']=row['after'];changed.append(row['path'])
 assert sorted(changed)==['bsd/conf/files.arm64','bsd/kern/bsd_init.c']
 expected.write_text(json.dumps(v,indent=2)+'\n')
 shutil.copyfile(patch,root/'dxb/patches/0089-ziyi-wifi-native.patch')
 with open(os.environ['GITHUB_ENV'],'a') as f:f.write('ZIYI_WIFI_ATH11K_OBJECT='+str(objects[0].resolve())+'\n')
 (out/'staged.json').write_text(json.dumps(dict(result='WIFI_NATIVE_FULL_KERNEL_STAGED',driverSHA256=audit['sha256'],patchSHA256=sha(patch),baselineScopes=75,changedBaselineFiles=changed,phoneOperated=False),indent=2)+'\n')
 print('Staged public WPSS/ath11k kernel; private firmware placeholder remains invalid')
if __name__=='__main__':main()
