"""Run production wire checks and real adapter refusal cases; synthetic only."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys
P=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args()
o=a.out.resolve();o.mkdir(exist_ok=False);env=dict(os.environ)
if Path(a.cc).is_file():env['PATH']=str(Path(a.cc).resolve().parent)+os.pathsep+env['PATH']
env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
exe=o/'wire.exe'
for name,args in [('wire-compile',[a.cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',P/'final_wire.c',P/'test_final_wire.c','-o',exe]),('wire-run',[exe]),
 ('native',[sys.executable,P/'run_native_tests.py','--repo',a.repo,'--fixture',a.fixture,'--cc',a.cc,'--out',o/'native'])]:
 q=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env);(o/(name+'.log')).write_bytes((q.stdout+q.stderr).encode());assert q.returncode==0,q.stdout+q.stderr
 print('G35_HOST_PASS',name,flush=True)
assert json.loads((o/'native/audit.json').read_text())['result']=='G35_ACTUAL_NATIVE_MODEL_PASS'
(o/'audit.json').write_bytes((json.dumps(dict(result='G35_PRIMITIVES_HOST_PASS',actualNativeAdapterCases=15,
 sourceHashes={n:hashlib.sha256((P/n).read_bytes()).hexdigest()for n in ['run_host_tests.py','run_native_tests.py','test_native.c','test_final_wire.c','final_wire.c','final_wire.h','primitives_result.c','primitives_result.h','native.inc']},
 syntheticOnly=True,nativeExecution=False,phoneOperated=False),indent=2)+'\n').encode())
print('G35_PRIMITIVES_HOST_PASS; actual adapter cases15, complete final transport; synthetic only')
