"""Actual bounded adapter, readback oracle, and final binary transport."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--repo',type=Path,required=True);ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args();O=a.out.resolve();O.mkdir(exist_ok=False)
env=dict(os.environ);cc=Path(a.cc)
if cc.is_file():env['PATH']=str(cc.resolve().parent)+os.pathsep+env['PATH']
env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
steps=[('wire',['final_wire.c','test_final_wire.c']),('pixels',['final_wire.c','triangle_result.c','test_triangle_result.c'])]
for name,files in steps:
 exe=O/(name+'.exe')
 for label,cmd in [('compile',[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',*[P/f for f in files],'-o',exe]),('run',[exe])]:
  q=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True);(O/f'{name}-{label}.log').write_bytes((q.stdout+q.stderr).encode());assert q.returncode==0,q.stdout+q.stderr
 print('G34',name,'PASS',flush=True)
q=subprocess.run(list(map(str,[sys.executable,P/'run_native_tests.py','--cc',a.cc,'--repo',a.repo,'--fixture',a.fixture,'--out',O/'native'])),env=env,capture_output=True,text=True)
(O/'native.log').write_bytes((q.stdout+q.stderr).encode());assert q.returncode==0,q.stdout+q.stderr
assert json.loads((O/'native/audit.json').read_text())['result']=='G34_ACTUAL_NATIVE_MODEL_PASS'
sources={n:hashlib.sha256((P/n).read_bytes()).hexdigest()for n in['run_host_tests.py','run_native_tests.py','test_native.c','final_wire.c','final_wire.h','triangle_result.c','triangle_result.h','test_final_wire.c','test_triangle_result.c']}
(O/'audit.json').write_text(json.dumps(dict(result='G34_TRIANGLE_HOST_PASS',suites=3,actualNativeAdapterCases=11,
 nativeExecution=False,syntheticOnly=True,phoneOperated=False,inputs=sources),indent=2)+'\n')
print('G34_TRIANGLE_HOST_PASS actual adapter + independent coverage + ten-page transport; synthetic only')
