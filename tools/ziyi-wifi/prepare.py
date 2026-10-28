"""Verify public inputs and adapt only the donor build's host paths for macOS."""
from pathlib import Path
import argparse,hashlib,json,shutil

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--stage',type=Path,required=True);a=ap.parse_args()
 source=Path(__file__).resolve().parent;out=a.stage.resolve();out.mkdir(exist_ok=False)
 m=json.loads((source/'public-inputs.json').read_text());changes=[]
 for row in m['files']:
  p=source/row['path'];assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
  name=row['path'];target=out/(name[6:] if name.startswith('donor/') else 'xnu-public-headers/'+name[len('station/'):])
  target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
 script=out/'tools/mk-ath11k.sh';text=script.read_text()
 for old,new in [
  ('ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)', 'ROOT=$(cd "$(dirname "$0")/.." && pwd)'),
  ('LD=$ROOT/xnu-deps/host/bin/ld64', 'LD="${ZIYI_LD:?actual Apple linker required}"'),
  ('CC="clang -target arm64-apple-ios16.5"', 'CC="clang -target arm64-apple-macos14.0"'),
  ('source "$ROOT/xnu-env.sh"', 'XNU_SRC="$ROOT/xnu-public-headers"'),
  ('fi\n[ "$1" = fetch ]', 'fi\npython3 "$ROOT/tools/patch_qmi_target.py" "$ROOT"\n[ "$1" = fetch ]'),
 ]:
  assert text.count(old)==1,old;text=text.replace(old,new);changes.append(dict(old=old,new=new))
 script.write_text(text)
 shutil.copyfile(source/'patch_qmi_target.py',out/'tools/patch_qmi_target.py')
 shutil.copyfile(source/'qmi-source-sha256.txt',out/'qmi-source-sha256.txt')
 tools=out/'host-tools';tools.mkdir();shim=tools/'sha256sum';shim.write_text('#!/bin/sh\nexec shasum -a 256 "$@"\n');shim.chmod(0o755)
 (out/'build-adaptation.json').write_text(json.dumps(dict(donorRevision=m['revision'],changes=changes,
  unmodifiedDriverSources=False,driverChanges='Checked target-capabilities hook in qmi.c only',
  target='arm64-apple-macos14.0, matches the actual current XNU kernel build platform',
  unmodifiedLinuxKPI=True,phoneOperated=False),indent=2)+'\n')
 print(json.dumps(dict(result='WIFI_MACOS_BUILD_STAGED',stage=str(out),changes=len(changes))))
if __name__=='__main__':main()
