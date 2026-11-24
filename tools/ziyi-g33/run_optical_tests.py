"""Actual G33 C wire/render tests; all outputs synthetic."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args();o=a.out.resolve();o.mkdir(parents=True,exist_ok=False)
env=dict(os.environ);cc=Path(a.cc)
if cc.is_file():env['PATH']=str(cc.resolve().parent)+os.pathsep+env['PATH']
env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
commands=[('compile',[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(P/'files/ziyi_g33_optical.c'),str(P/'test_optical.c'),'-o',str(o/'optical.exe')]),('run',[str(o/'optical.exe'),str(a.fixture.resolve()),str(o)])]
for name,cmd in commands:
 p=subprocess.run(cmd,capture_output=True,text=True,env=env);(o/(name+'.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
text=(o/'run.log').read_text();assert 'G33_OPTICAL_HOST_PASS' in text
(o/'audit.json').write_text(json.dumps(dict(result='G33_OPTICAL_HOST_PASS',output=text.strip(),syntheticOnly=True,phoneOperated=False,inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [P/'files/ziyi_g33_optical.c',P/'files/ziyi_g33_optical.h',P/'test_optical.c',Path(__file__),a.fixture.resolve()]}),indent=2)+'\n');print(text.strip())
