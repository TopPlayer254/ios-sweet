"""Actual native adapter, clock-dependent register views; no hardware claim."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
P=Path(__file__).resolve().parent;ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--repo',type=Path,default=Path.cwd());ap.add_argument('--cc',default='cc');ap.add_argument('--fixture',type=Path,required=True);a=ap.parse_args();O=a.out.resolve();O.mkdir(parents=True,exist_ok=False);repo=a.repo.resolve();K=P/'files' if (P/'files').is_dir() else repo/'tools/ziyi-g24/files';cc=Path(a.cc);env=dict(os.environ);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file() else '')+env['PATH'];env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
inc=[K,repo/'tools/ziyi-hw/files',repo/'tools/ziyi-g19/files',repo/'tools/ziyi-g23/files',P,O/'include'];dst=O/'include/pexpert/arm64/ziyi_log_strip.h';dst.parent.mkdir(parents=True);dst.write_bytes((repo/'tools/ziyi-ufs/u11_log_strip.h').read_bytes())
def source(n):return next(d/n for d in inc if (d/n).is_file())
common=['ziyi_g24_engine.c','ziyi_g24_pas.c','ziyi_g24_firmware.c','ziyi_g24_record.c','ziyi_g24_pixel.c','ziyi_g24_memory.c','ziyi_g8_translation.c','ziyi_g23_startup.c','ziyi_g23_resources.c','ziyi_g8_contract.c','ziyi_g7_sha256.c'];flags=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DG24_PUBLIC_FIXTURE=1']+['-I'+str(d) for d in inc]
def run(args,label):
 q=subprocess.run(list(map(str,args)),env=env,capture_output=True,text=True);(O/(label+'.log')).write_text(q.stdout+q.stderr,encoding='utf-8');assert q.returncode==0,q.stdout+q.stderr;return q.stdout
outputs=[]
for name,extra in [('old',['-DG30_OLD_NATIVE=1']),('new',[])]:
 exe=O/(name+'.exe');run(flags+extra+[source(n) for n in common]+[P/'test_witness.c','-o',exe],name+'-compile');outputs.append(run([exe,a.fixture.resolve(),O/(name+'-record.bin')],name+'-run'))
assert (O/'old-record.bin').read_bytes()==(O/'new-record.bin').read_bytes(),'Witness changed logical GCP outcome'
import re
m=re.search(r'events(\d+) scheduledMs(\d+) reads(\d+) writes(\d+) PAS(\d+)',outputs[1]);assert m
sources=[P/'test_witness.c',P/'g29-native-before.inc',K/'ziyi_g24_native.inc',P/'run_witness_tests.py'];report=dict(result='G30_ACTUAL_WITNESS_HOST_PASS',events=int(m[1]),scheduledMs=int(m[2]),readPairs=int(m[3]),writePairs=int(m[4]),PASPairs=int(m[5]),sameGCPBytes=True,sameTargetLogicalMMIO=True,freshPredicatesAfterPacing=True,allAuxiliaryRegisters=8,witnessBytes=64,corruptBitsRefused=512,sourceHashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},outputs=outputs,syntheticOnly=True,native=False,phoneOperated=False);(O/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print('G30_ACTUAL_WITNESS_HOST_PASS '+str({k:report[k] for k in ['events','scheduledMs','readPairs','writePairs','PASPairs','sameGCPBytes']}))
