"""Actual G24 CPU controls with synthetic firmware; no phone or native claims."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,struct,subprocess,zlib
P=Path(__file__).resolve().parent;ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');ap.add_argument('--repo',type=Path,default=Path.cwd());a=ap.parse_args();O=a.out.resolve();O.mkdir(parents=True,exist_ok=False);repo=a.repo.resolve();K=P/'files';H=repo/'tools/ziyi-hw/files'
include=[K,H,repo/'tools/ziyi-g19/files',repo/'tools/ziyi-g23/files',O/'include']
header=repo/'tools/ziyi-ufs/u11_log_strip.h';baseline=json.loads((repo/'tools/ziyi-ufs/manifest.json').read_text());assert hashlib.sha256(header.read_bytes()).hexdigest()==baseline['pexpert/pexpert/arm64/ziyi_log_strip.h']['after'];dst=O/'include/pexpert/arm64/ziyi_log_strip.h';dst.parent.mkdir(parents=True);dst.write_bytes(header.read_bytes())
# Deliberately synthetic code/hashes, structurally valid MDT; never PAS-signed.
blob=bytearray(65536);blob[:16]=b'ZIYI_G24_CP_FW01';struct.pack_into('<4I',blob,16,1,65536,0x06060201,1)
sqe=bytearray((i*17+11)%256 for i in range(43068));struct.pack_into('<2I',sqe,0,0,0x016dc109)
mdt=bytearray(3564);mdt[:7]=b'\x7fELF\x01\x01\x01';struct.pack_into('<HH',mdt,16,2,164);struct.pack_into('<I',mdt,28,52);struct.pack_into('<HHH',mdt,40,52,32,3)
for i,row in enumerate([(0,0,0,0,148,0,0x07000000,0),(1,0x1000,0x1000,0x1000,1976,1976,0x08000007,0x100000),(0,0x2000,0,0,3416,3416,0x02000000,0x1000)]):struct.pack_into('<8I',mdt,52+32*i,*row)
code=bytes((i*19+7)%256 for i in range(1976))
for i,(off,data) in enumerate(zip([256,43328,46896],[sqe,mdt,code])):struct.pack_into('<2I',blob,32+8*i,off,len(data));blob[off:off+len(data)]=data;blob[64+32*i:96+32*i]=hashlib.sha256(data).digest()
struct.pack_into('<2I',blob,56,4096,4096);blob[160:192]=hashlib.sha256(blob[256:]).digest();struct.pack_into('<I',blob,252,zlib.crc32(blob[:252]));fixture=O/'synthetic-not-signed-capsule.bin';fixture.write_bytes(blob)
env=dict(os.environ);cc=Path(a.cc);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file() else '')+env['PATH'];env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
def source(n):
 candidates=[d/n for d in include if (d/n).is_file()];assert candidates,n;return candidates[0]
common=['ziyi_g24_engine.c','ziyi_g24_pas.c','ziyi_g24_firmware.c','ziyi_g24_record.c','ziyi_g24_pixel.c','ziyi_g24_memory.c','ziyi_g8_translation.c','ziyi_g23_startup.c','ziyi_g23_resources.c','ziyi_g8_contract.c','ziyi_g7_sha256.c']
flags=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DG24_PUBLIC_FIXTURE=1']+['-I'+str(d) for d in include]
rows=[]
def run(args,label):
 q=subprocess.run(list(map(str,args)),env=env,capture_output=True,text=True);(O/(label+'.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stdout+q.stderr;return q.stdout
for name,test,args in [('engine','test_engine.c',[]),('firmware_pas','test_firmware_pas.c',[fixture]),('native','test_native.c',[fixture]),('record','test_record.c',[O/'record.bin'])]:
 exe=O/(name+'.exe');run(flags+[source(n) for n in common]+[P/test,'-o',exe],name+'-compile');out=run([exe,*args],name+'-run');rows.append(dict(name=name,output=out))
sp=importlib.util.spec_from_file_location('gcp',P/'record.py');mod=importlib.util.module_from_spec(sp);sp.loader.exec_module(mod);raw=(O/'record.bin').read_bytes();record=mod.decode(raw);assert record['phase']==13 and record['epoch']==0x123456789abcdef0 and record['start']==0x9988776600000000 and record['verified_pixels']==4096 and record['zap_backup_crc']==0x1234
for bit in range(2560):
 bad=bytearray(raw);bad[bit//8]^=1<<(bit%8)
 try:mod.decode(bad)
 except ValueError:pass
 else:raise AssertionError('Python accepted corrupt GCP1')
files=[P/n for n in ['test_engine.c','test_native.c','test_firmware_pas.c','test_record.c','record.py','record-fields.json']]+[source(n) for n in common]+[K/'ziyi_g24_native.inc',K/'ziyi_g24_profile.inc',K/'ziyi_g24_engine.h']
report=dict(result='G24_PUBLIC_EXACT_HOST_PASS',tests=rows,independentPythonCorruptBits=2560,sourceHashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},syntheticFirmware=True,syntheticMmioAndSmc=True,native=False,phoneOperated=False);(O/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print('G24_PUBLIC_EXACT_HOST_PASS tests4 PythonBitRefusals2560 native0')
