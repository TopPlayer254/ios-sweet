"""Exercise the exact compiled G8 source with public synthetic data only."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
P=Path(__file__).resolve().parent;K=P.parent/'ziyi-hw/files'
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
cc=Path(a.cc)
if cc.is_file():env['PATH']=str(cc.resolve().parent)+os.pathsep+env['PATH']
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(K)]
tests=[('engine',['test_g8_engine.c'],['ziyi_g8_engine.c','ziyi_g8_registers.c','ziyi_g8_memory.c','ziyi_g8_translation.c','ziyi_g8_drain.c','ziyi_g8_contract.c','ziyi_g7_sha256.c'],'G8_SYNTHETIC_ENGINE_PASS'),('slot',['test_g8_slot.c'],['ziyi_g8_slot.c','ziyi_g7_sha256.c'],'G8_SLOT_SYNTHETIC_PASS')];rows=[]
for name,local,core,marker in tests:
 exe=a.out/(name+('.exe' if os.name=='nt' else ''));command=[a.cc]+flags+[str(P/n) for n in local]+[str(K/n) for n in core]+['-o',str(exe)]
 p=subprocess.run(command,capture_output=True,text=True,env=env);(a.out/(name+'-compile.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
 p=subprocess.run([str(exe.resolve())],capture_output=True,text=True,env=env);(a.out/(name+'-tests.log')).write_text(p.stdout+p.stderr);assert p.returncode==0 and marker in p.stdout and 'runtime error:' not in p.stderr and 'Sanitizer' not in p.stderr,p.stdout+p.stderr
 rows.append(dict(name=name,result=p.stdout.strip(),executableSHA256=hashlib.sha256(exe.read_bytes()).hexdigest()))
report=dict(result='G8_EXACT_SOURCE_PUBLIC_HOST_PASS',tests=rows,firmwareIncluded=False,nativeExecuted=False)
(a.out/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
