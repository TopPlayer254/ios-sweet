from pathlib import Path
import argparse,json,os,shutil,subprocess
P=Path(__file__).resolve().parent;H=P.parent/'ziyi-g20/files';p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);env=os.environ.copy();cc=Path(a.cc)
if cc.is_file():env['PATH']=str(cc.resolve().parent)+os.pathsep+env['PATH']
env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1';c=a.out/'control';c.mkdir();shutil.copyfile(H/'test_pmap_helper.c',c/'test_pmap_helper.c');shutil.copyfile(P/'predecessor.inc',c/'pmap_owned_page.inc');results=[]
for name,inc,flags,marker in [('fixed',H,[],'G21_REAL_MASK_VALID_BIT_HOST_PASS'),('predecessor',c,['-DG21_PREDECESSOR'],'G21_PREDECESSOR_NATIVE_MASK_FAILURE_REPRODUCED')]:
 exe=a.out/(name+('.exe' if os.name=='nt' else ''));cmd=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(inc),*flags,str(P/'test_native_types.c'),'-o',str(exe)];q=subprocess.run(cmd,env=env,capture_output=True,text=True);(a.out/(name+'-compile.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stdout+q.stderr
 q=subprocess.run([str(exe.resolve())],env=env,capture_output=True,text=True);(a.out/(name+'-run.log')).write_text(q.stdout+q.stderr);assert q.returncode==0 and marker in q.stdout and 'runtime error:' not in q.stderr,q.stdout+q.stderr;results.append(q.stdout.strip())
(a.out/'audit.json').write_text(json.dumps(dict(result='G21_REAL_MASK_CONTROLS_PASS',tests=results,native=False),indent=2)+'\n');print('G21_REAL_MASK_CONTROLS_PASS')
