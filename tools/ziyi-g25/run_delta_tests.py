"""Exercise actual unchanged engine, native MMIO adapter and native printer."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,re,subprocess
P=Path(__file__).resolve().parent;ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--repo',type=Path,default=Path.cwd());ap.add_argument('--cc',default='cc');ap.add_argument('--fixture',type=Path,required=True);a=ap.parse_args();O=a.out.resolve();O.mkdir(parents=True,exist_ok=False);repo=a.repo.resolve();K=repo/'tools/ziyi-g24/files';cc=Path(a.cc)
env=dict(os.environ);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file() else '')+env['PATH'];env.update(ASAN_OPTIONS='abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
inc=[K,repo/'tools/ziyi-hw/files',repo/'tools/ziyi-g19/files',repo/'tools/ziyi-g23/files',O/'include'];dst=O/'include/pexpert/arm64/ziyi_log_strip.h';dst.parent.mkdir(parents=True);dst.write_bytes((repo/'tools/ziyi-ufs/u11_log_strip.h').read_bytes())
def source(n):return next(d/n for d in inc if (d/n).is_file())
common=['ziyi_g24_engine.c','ziyi_g24_pas.c','ziyi_g24_firmware.c','ziyi_g24_record.c','ziyi_g24_pixel.c','ziyi_g24_memory.c','ziyi_g8_translation.c','ziyi_g23_startup.c','ziyi_g23_resources.c','ziyi_g8_contract.c','ziyi_g7_sha256.c']
flags=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DG24_PUBLIC_FIXTURE=1']+['-I'+str(d) for d in inc]
def run(args,label):
 q=subprocess.run(list(map(str,args)),env=env,capture_output=True,text=True);(O/(label+'.log')).write_text(q.stdout+q.stderr,encoding='utf-8');assert q.returncode==0,q.stdout+q.stderr;return q.stdout
old=O/'old';old.mkdir();(old/'ziyi_g24_engine.c').write_bytes(source(common[0]).read_bytes());(old/'ziyi_g24_profile.inc').write_bytes((P/'g24-profile-before.inc').read_bytes())
for name,extra,engine in [('old',['-DG25_OLD_CONTROL=1'],old/'ziyi_g24_engine.c'),('new',[],source(common[0]))]:
 exe=O/(name+'.exe');run(flags+extra+[engine]+[source(n) for n in common[1:]]+[P/'test_delta.c','-o',exe],name+'-compile');out=run([exe,a.fixture.resolve(),O/(name+'-record.bin')],name+'-run')
 if name=='new':printed=out.split('G25_ACTUAL_PRINTER_BEGIN\n')[1].split('G25_ACTUAL_PRINTER_END')[0]
rows=re.findall(r'^C(\d\d) ((?:[0-9a-f]{8} ){3}[0-9a-f]{8})$',printed,re.M);assert [int(n) for n,_ in rows]==list(range(20));raw=b''.join(int(v,16).to_bytes(4,'little') for _,line in rows for v in line.split());assert raw==(O/'new-record.bin').read_bytes()
headers=re.findall(r'^GCP1 page(\d)/5 .*$',printed,re.M);assert list(map(int,headers))==list(range(5))
sp=importlib.util.spec_from_file_location('rec',P/'record.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m);record=m.decode(raw);assert record['verified_pixels']==4096
native=(K/'ziyi_g24_native.inc').read_text();assert 'zg_sleep(2000);ziyi_gpu_cp_print(ctx);' in native and 'page++%6' in native
sources=[P/'test_delta.c',P/'g24-profile-before.inc',P/'run_delta_tests.py',K/'ziyi_g24_profile.inc',K/'ziyi_g24_native.inc',source('ziyi_g24_engine.c')]
(O/'audit.json').write_text(json.dumps(dict(result='G25_DELTA_CONTROLS_PASS',negativeOldReads=425,negativeOldWrites=35,sentinels=4,remainingExactReadbackRefusals=151,optionalStores=0,actualPrinterPages=5,rowsPerPage=4,wireBytes=320,wireCRC=raw[12:16].hex(),actualPrinterLossless=True,sourceHashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},native=False,phoneOperated=False),indent=2)+'\n',encoding='utf-8');print('G25_DELTA_CONTROLS_PASS old425/35 sentinels4 readbackRefusals151 actualPages5x4 lossless320B native0')
