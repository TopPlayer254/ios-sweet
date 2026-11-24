"""Exercise G33's actual native adapter, retaining the G31 host MMIO model."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--repo',type=Path,required=True);ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args()
O=a.out.resolve();O.mkdir(parents=True,exist_ok=False);repo=a.repo.resolve()
base=repo/'tools/ziyi-g31/test_observer.c';support=base.read_text()
old='static void zg_sleep(unsigned ms){(void)ms;abort();}'
assert support.count(old)==1
support=support.replace(old,'static void zg_sleep(unsigned ms){g33_sleep_hook(ms);}')
assert support.count('0x900100000ULL')==2
support=support.replace('0x900100000ULL','g33_table_base')
support='static unsigned long long g33_table_base=0x900100000ULL;\n'+support
(O/'g31-host-support.inc').write_text(support)
inc=[P/'files',repo/'tools/ziyi-g24/files',repo/'tools/ziyi-hw/files',repo/'tools/ziyi-g19/files',repo/'tools/ziyi-g23/files',repo/'tools/ziyi-g31',O,O/'include']
dst=O/'include/pexpert/arm64/ziyi_log_strip.h';dst.parent.mkdir(parents=True);dst.write_bytes((repo/'tools/ziyi-ufs/u11_log_strip.h').read_bytes())
def source(n):return next(d/n for d in inc if (d/n).is_file())
common=['ziyi_g24_engine.c','ziyi_g24_pas.c','ziyi_g24_firmware.c','ziyi_g24_record.c','ziyi_g24_pixel.c','ziyi_g24_memory.c','ziyi_g8_translation.c','ziyi_g23_startup.c','ziyi_g23_resources.c','ziyi_g8_contract.c','ziyi_g7_sha256.c']
cc=Path(a.cc);env=dict(os.environ);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file() else '')+env['PATH'];env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
flags=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DG24_PUBLIC_FIXTURE=1']+['-I'+str(d) for d in inc]
exe=O/'native.exe'
for label,cmd in [('compile',flags+[source(n) for n in common]+[P/'test_native.c','-o',exe]),('run',[exe,a.fixture.resolve(),O/'record.bin'])]:
 q=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True);(O/(label+'.log')).write_text(q.stdout+q.stderr,encoding='utf-8');assert q.returncode==0,q.stdout+q.stderr
output=(O/'run.log').read_text();assert 'G33_NATIVE_ADAPTER_HOST_PASS' in output
inputs=[P/'test_native.c',P/'run_native_tests.py',base,source('ziyi_g24_native.inc'),O/'g31-host-support.inc']
(O/'audit.json').write_text(json.dumps(dict(result='G33_NATIVE_ADAPTER_HOST_PASS',output=output,fullPackets=2,modeledPixels=4096,syntheticOnly=True,phoneOperated=False,inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}),indent=2)+'\n')
print(output.strip())
