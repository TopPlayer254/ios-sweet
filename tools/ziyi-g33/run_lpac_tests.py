"""Qualify the actual prefixed/atomic LPAC helper without hardware."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args();O=a.out.resolve();O.mkdir(parents=True,exist_ok=False)
source=(P/'test_lpac.c').read_text().replace('"lpac_route.h"','"ziyi_g33_lpac.h"');(O/'test.c').write_text(source)
cc=Path(a.cc);env=dict(os.environ);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file() else '')+env['PATH'];env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
exe=O/'lpac.exe'
for label,cmd in [('compile',[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(P/'files'),P/'files/ziyi_g33_lpac.c',O/'test.c','-o',exe]),('run',[exe])]:
 q=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True);(O/(label+'.log')).write_text(q.stdout+q.stderr,encoding='utf-8');assert q.returncode==0,q.stdout+q.stderr
output=(O/'run.log').read_text();assert 'G33_LPAC_HOST_PASS' in output
(O/'audit.json').write_text(json.dumps(dict(result='G33_LPAC_HOST_PASS',output=output,syntheticOnly=True,phoneOperated=False,inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [P/'files/ziyi_g33_lpac.c',P/'files/ziyi_g33_lpac.h',P/'test_lpac.c',Path(__file__)]}),indent=2)+'\n');print(output.strip())
