from pathlib import Path
import argparse,json,os,subprocess
P=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();cc=Path(a.cc)
if cc.is_file():env['PATH']=str(cc.resolve().parent)+os.pathsep+env['PATH']
env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
exe=a.out/('pmap-trace.exe' if os.name=='nt' else 'pmap-trace')
cmd=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(P/'files'),str(P/'files/test_g20_pmap.c'),'-o',str(exe)]
q=subprocess.run(cmd,env=env,capture_output=True,text=True);(a.out/'compile.log').write_text(q.stdout+q.stderr);assert q.returncode==0,q.stdout+q.stderr
q=subprocess.run([str(exe.resolve()),str(a.out.resolve())],env=env,capture_output=True,text=True);(a.out/'run.log').write_text(q.stdout+q.stderr);assert q.returncode==0 and 'G20_PMAP_CPU_TRACE_HOST_PASS' in q.stdout and 'runtime error:' not in q.stderr,q.stdout+q.stderr
(a.out/'audit.json').write_text(json.dumps(dict(result='G20_CPU_TRACE_HOST_PASS',output=q.stdout.strip(),native=False),indent=2)+'\n');print(q.stdout.strip())
