"""Actual native ACK replay and actual pre-user entry, strictly on the PC."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
P=Path(__file__).resolve().parent;K=P.parent/'ziyi-g19/files';H=P.parent/'ziyi-hw/files'
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');args=ap.parse_args();o=args.out.resolve();o.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();cc=Path(args.cc);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file() else '')+env['PATH'];env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1';cmds=[];rows=[]
def run(cmd,name,cwd=None):
 cmd=list(map(str,cmd));cmds.append(cmd);q=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True);(o/(name+'.log')).write_text(q.stdout+q.stderr,encoding='utf-8');assert q.returncode==0,q.stdout+q.stderr;return q.stdout
flags=[args.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(K),'-I'+str(H)]
for name,source,version,extra,args in [('live',P/'test_native_replay.c',K/'ziyi_hfi_version.c',['-DG22_PREDECESSOR=0'],[]),('old',P/'test_native_replay.c',P/'predecessor_version.c',['-DG22_PREDECESSOR=1'],[]),('diagnostic',P/'test_native_diagnostic.c',K/'ziyi_hfi_version.c',[],[o])]:
 sources=[source,K/'ziyi_hfi_live.c',version,H/'ziyi_g8_contract.c']
 if name=='diagnostic':sources += [K/'ziyi_hfi_diagnostic.c',K/'ziyi_hfi_record.c']
 run(flags+extra+sources+['-o',o/(name+'.exe')],name+'-compile');result=run([o/(name+'.exe'),*args],name+'-run');rows.append(dict(name=name,output=result.strip()))
probe=(P/'inputs/ziyi_gpu_probe.c').read_text(encoding='utf-8');start=probe.index('int ziyi_gpu_preuser_start(void){');end=probe.index('\n}\n',start)+3;entry=probe[start:end]
model='''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define THREAD_UNINT 1
#define THREAD_CONTINUE_NULL 0
static int present,mode,starts,waits,blocks,channel;
static jmp_buf parked;
static int PE_parse_boot_argn(const char*n,void*out,unsigned z){if(strcmp(n,"ziyi_gpu_preuser")||z!=sizeof(unsigned))abort();*(unsigned*)out=(unsigned)mode;return present;}
static void ziyi_gpu_probe_start(void){starts++;}
static int assert_wait(void*p,int t){if(!p||t!=1)abort();waits++;channel=1;return 0;}
static void thread_block(int t){if(t||!channel)abort();blocks++;longjmp(parked,1);}
'''+entry+'''
int main(void){
 for(int value=0;value<4;value++)for(int found=0;found<2;found++){
  mode=value;present=found;starts=waits=blocks=channel=0;
  if(setjmp(parked)==0){if(ziyi_gpu_preuser_start()!=0||starts||waits||blocks)abort();if(found&&value==1)abort();}
  else if(!found||value!=1||starts!=1||waits!=1||blocks!=1)abort();
 }
 puts("G22_ACTUAL_PREUSER_ENTRY_HOST_PASS modes8 noRootMount noAppleUserspace native=0");return 0;
}
'''
(o/'preuser.c').write_text(model,encoding='utf-8');run(flags+[o/'preuser.c','-o',o/'preuser.exe'],'preuser-compile');rows.append(dict(name='preuser',output=run([o/'preuser.exe'],'preuser-run').strip()))
bsd=(P/'inputs/bsd_init.c').read_text(encoding='utf-8');gate=(P/'preuser_gate.inc').read_text(encoding='utf-8');assert bsd.count(gate)==1
assert bsd.index(gate)<bsd.index('\tbsd_autoconf();')<bsd.index('vfs_mountroot()')<bsd.index('ziyi_gpu_probe_start();')
import importlib.util
spec=importlib.util.spec_from_file_location('g22_wire',P/'record.py');wire=importlib.util.module_from_spec(spec);spec.loader.exec_module(wire)
fixtures=[]
for path in sorted(o.glob('*.bin')):
 value=wire.decode(path.read_bytes());fixtures.append(dict(name=path.name,crc=value['crc32']))
assert len(fixtures)==6 and wire.decode((o/'success.bin').read_bytes())['version']==0x30020004
report=dict(result='G22_PREUSER_NATIVE_ACK_HOST_PASS',tests=rows,fixtures=fixtures,bootstrapGateBeforeRoot=True,sourceNativeCRC='deaba7a0',nativeGpuPixels=False,phoneOperated=False)
(o/'audit.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(result=report['result'],tests=len(rows),fixtures=len(fixtures))))
