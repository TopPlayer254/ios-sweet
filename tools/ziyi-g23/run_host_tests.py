"""Actual G23 transport/native adapter controls on a CPU, without phone access."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,struct,subprocess
P=Path(__file__).resolve().parent;ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');ap.add_argument('--hardware-dir',type=Path,default=P.parent/'ziyi-hw/files');a=ap.parse_args();O=a.out.resolve();O.mkdir(parents=True,exist_ok=False);K=P/'files';H=a.hardware_dir.resolve()
facts=json.loads((P/'expected.json').read_text(encoding='utf-8'));data=bytearray(2248);data[:144]=bytes.fromhex(facts['nativeHeaderHex']);data[56:72]=bytes(16)
for slave in [3,5]:
 rows=[r for r in facts['resources'].values() if r['slave']==slave];base=min(r['entryFileOffset'] for r in rows);slot=0 if slave==3 else 2;_,head,aux,count,*_=struct.unpack_from('<8H',data,8+16*slot)
 for row in rows:
  index=(row['entryFileOffset']-base)//24;assert index<count and row['entryFileOffset']-base==24*index
  entry=bytes.fromhex(row['entryHex']);offset=struct.unpack_from('<H',entry,22)[0];value=bytes.fromhex(row['auxHex']);data[144+head+24*index:144+head+24*(index+1)]=entry;data[144+aux+offset:144+aux+offset+len(value)]=value
fixture=O/'source-valued-cpu-fixture.bin';fixture.write_bytes(data)
env=dict(os.environ);cc=Path(a.cc);env['PATH']=(str(cc.resolve().parent)+os.pathsep if cc.is_file() else '')+env['PATH'];env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1';rows=[]
def run(cmd,label):
 q=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True);(O/(label+'.log')).write_text(q.stdout+q.stderr,encoding='utf-8');assert q.returncode==0,q.stdout+q.stderr;return q.stdout
flags=[a.cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(K),'-I'+str(H)]
for name,test,extra in [('startup','test_startup.c',[O/'success.bin']),('native','test_native.c',[])]:
 run(flags+[K/'ziyi_g23_resources.c',K/'ziyi_g23_startup.c',P/test,H/'ziyi_g8_contract.c','-o',O/(name+'.exe')],name+'-compile')
 output=run([O/(name+'.exe'),fixture,*extra],name+'-run');rows.append(dict(name=name,output=output.strip()))
sp=importlib.util.spec_from_file_location('gst',P/'record.py');codec=importlib.util.module_from_spec(sp);sp.loader.exec_module(codec);raw=(O/'success.bin').read_bytes();state=codec.decode(raw);assert state['state']==3 and state['completedAcks']==5
for bit in range(768*8):
 bad=bytearray(raw);bad[bit//8]^=1<<(bit%8)
 try:codec.decode(bad)
 except ValueError:pass
 else:raise AssertionError('Python accepted bad GST1 CRC')
report=dict(result='G23_PUBLIC_EXACT_HOST_PASS',tests=rows,fixture='CPU layout with own source-derived resource facts; not a native memory dump',successCRC=state['crc32'],independentPythonCorruptBits=6144,sourceHashes={str(p.relative_to(P)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [P/'test_startup.c',P/'test_native.c',P/'record.py',P/'expected.json',*sorted(K.glob('ziyi_g23_*'))]},native=False,phoneOperated=False)
(O/'audit.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(result=report['result'],tests=len(rows),successCRC=state['crc32'])))
