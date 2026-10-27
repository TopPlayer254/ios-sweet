"""Public CI test of the unchanged8796 SecureDT implementation and board bindings."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
HERE=Path(__file__).resolve().parent
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--kernel-dir',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=False);kernel=a.kernel_dir.resolve()
 origins=json.loads((HERE/'api-sources.json').read_text())
 for row in origins:assert hashlib.sha256((HERE/row['file']).read_bytes()).hexdigest()==row['sha256'],row['file']
 env=os.environ.copy();env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1';env['ASAN_OPTIONS']='abort_on_error=1'
 cc=Path(a.cc)
 if cc.is_file():env['PATH']=str(cc.resolve().parent)+os.pathsep+env['PATH']
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(HERE/'include'),'-I'+str(kernel)];results=[]
 for profile,definitions in [('sweet',['-DSWEET=1']),('protected',['-DSWEET=1','-DKERNEL_INTEGRITY_CTRR=1']),('generic',['-DSWEET=0']),('protected-generic',['-DSWEET=0','-DKERNEL_INTEGRITY_CTRR=1'])]:
  exe=a.out/(profile+('.exe' if os.name=='nt' else ''));command=[a.cc]+flags+definitions+[str(HERE/'device_tree.c'),str(HERE/'test_actual_api.c'),str(kernel/'ziyi_gpu_probe_contract.c'),'-o',str(exe)]
  p=subprocess.run(command,capture_output=True,text=True,env=env);(a.out/(profile+'-compile.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
  p=subprocess.run([str(exe.resolve()),str(HERE/'public-normalized.adt')],capture_output=True,text=True,env=env);(a.out/(profile+'-tests.log')).write_text(p.stdout+p.stderr)
  assert p.returncode==0 and 'ACTUAL_8796_SECURE_DT_API_PASS' in p.stdout and 'runtime error:' not in p.stderr and 'Sanitizer' not in p.stderr,p.stdout+p.stderr;results.append(dict(profile=profile,result=p.stdout.strip()))
 report=dict(result='HW_ACTUAL_8796_PUBLIC_SECURE_DT_API_PASS',profiles=results,realAPISources=origins,noPrivateFixture=True,noPhoneOperation=True)
 (a.out/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
