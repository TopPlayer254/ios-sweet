"""Compile/run G34's actual adapter against the retained G33 host environment."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--repo',type=Path,required=True);ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args()
O=a.out.resolve();O.mkdir(parents=True,exist_ok=False);repo=a.repo.resolve()
g31=(repo/'tools/ziyi-g31/test_observer.c').read_text()
needle='static void zg_sleep(unsigned ms){(void)ms;abort();}'
assert g31.count(needle)==1;g31=g31.replace(needle,'static void zg_sleep(unsigned ms){g33_sleep_hook(ms);}')
assert g31.count('0x900100000ULL')==2;g31=g31.replace('0x900100000ULL','g33_table_base')
(O/'g31-host-support.inc').write_bytes(('static unsigned long long g33_table_base=0x900100000ULL;\n'+g31).encode())
g33=(repo/'tools/ziyi-g33/test_native.c').read_text();assert g33.count('int main(')==1
(O/'g33-native-support.inc').write_bytes(g33.replace('int main(','int g34_unused_g33_main(').encode())
inc=[P/'files',repo/'tools/ziyi-g24/files',repo/'tools/ziyi-hw/files',repo/'tools/ziyi-g19/files',repo/'tools/ziyi-g23/files',repo/'tools/ziyi-g31',O,O/'include']
dst=O/'include/pexpert/arm64/ziyi_log_strip.h';dst.parent.mkdir(parents=True);dst.write_bytes((repo/'tools/ziyi-ufs/u11_log_strip.h').read_bytes())
def source(n):return next(d/n for d in inc if(d/n).is_file())
common=['ziyi_g24_engine.c','ziyi_g24_pas.c','ziyi_g24_firmware.c','ziyi_g24_record.c','ziyi_g24_pixel.c','ziyi_g24_memory.c','ziyi_g8_translation.c','ziyi_g23_startup.c','ziyi_g23_resources.c','ziyi_g8_contract.c','ziyi_g7_sha256.c']
cc=Path(a.cc);env=dict(os.environ);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file()else'')+env['PATH'];env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
flags=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DG24_PUBLIC_FIXTURE=1']+['-I'+str(d)for d in inc]
exe=O/'native.exe'
for label,cmd in[('compile',flags+[source(n)for n in common]+[P/'test_native.c','-o',exe]),('run',[exe,a.fixture.resolve(),O/'final.bin'])]:
 q=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True);(O/(label+'.log')).write_bytes((q.stdout+q.stderr).encode());assert q.returncode==0,q.stdout+q.stderr
text=(O/'run.log').read_text();assert 'G34_ACTUAL_NATIVE_MODEL_PASS'in text
(O/'audit.json').write_text(json.dumps(dict(result='G34_ACTUAL_NATIVE_MODEL_PASS',output=text,
 syntheticOnly=True,nativeExecution=False,phoneOperated=False,
 inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in[P/'test_native.c',P/'run_native_tests.py',source('ziyi_g24_native.inc'),O/'g31-host-support.inc',O/'g33-native-support.inc']}),indent=2)+'\n')
print(text.strip())
