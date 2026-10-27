"""Run only public synthetic tests against the exact files placed in patch0085."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[2];KIT=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1';env['ASAN_OPTIONS']='abort_on_error=1'
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(KIT/'files'),'-I'+str(KIT)]
rows=[]
tests=[('wifi',[KIT/'test_wifi_probe.c'],'KWIFI_ACTUAL_PUBLIC_SYNTHETIC_HOST_PASS'),('gpu-contract',[KIT/'test_gpu_contract.c',KIT/'files/ziyi_gpu_probe_contract.c'],'GPU_ACTUAL_CONTRACT_HOST_PASS'),('gpu-engine',[KIT/'test-gpu-engine.c',KIT/'files/ziyi_g7_engine.c',KIT/'files/ziyi_g7_policy.c',KIT/'files/ziyi_g7_sha256.c'],'G7_ACQUISITION_TCM_ENGINE_PASS'),('gpu-clock',[KIT/'test-gpu-clock.c',KIT/'files/ziyi_g7_clock.c',KIT/'files/ziyi_g7_clock_crc.c'],'G2_ACTUAL_POWER_ENGINE_HOST_PASS')]
for name,inputs,marker in tests:
    exe=a.out/(name+('.exe' if os.name=='nt' else ''));command=[a.cc]+flags+list(map(str,inputs))+['-o',str(exe)]
    compile=subprocess.run(command,capture_output=True,text=True,env=env);(a.out/(name+'-compile.log')).write_text(compile.stdout+compile.stderr);assert compile.returncode==0,compile.stderr
    command=[str(exe)]+([str(a.out/'wifi-public-record.bin')] if name=='wifi' else [])
    run=subprocess.run(command,capture_output=True,text=True,env=env);(a.out/(name+'-tests.log')).write_text(run.stdout+run.stderr)
    assert run.returncode==0 and marker in run.stdout and 'runtime error:' not in run.stderr and 'Sanitizer' not in run.stderr,run.stdout+run.stderr
    rows.append(dict(name=name,result=run.stdout.strip(),executableSHA256=hashlib.sha256(exe.read_bytes()).hexdigest()))
report=dict(result='ZIYI_HW_PUBLIC_SYNTHETIC_HOST_PASS',tests=rows,phoneOperated=False)
(a.out/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
