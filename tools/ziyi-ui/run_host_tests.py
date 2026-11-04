"""Mandatory sanitizer tests using the actual published method/helper text."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--cc',default=os.environ.get('CXX','clang++'));a=p.parse_args();kit=Path(__file__).resolve().parent;o=a.out.resolve();o.mkdir(exist_ok=False);m=json.loads((kit/'manifest.json').read_text());repo=kit.parents[1]
 for r in m['sources']:assert hashlib.sha256((repo/r['repository']).read_bytes()).hexdigest()==r['afterSHA256']
 s=(kit/'files/iokit/Kernel/arm/hid/SweetHID.cpp').read_text();start=s.index('/* Atomic full TCM snapshot;');end=s.index('/* key: 0 power',start);hid=s[start:end]
 s=(kit/'files/iokit/Kernel/arm/SweetKeyStore.cpp').read_text();start=s.index('static IOReturn\nsweet_aks_struct_out(');end=s.index('static uint32_t\nsweet_aks_struct_out_size',start);aks=s[start:end];start=s.index('static uint64_t sweet_aks_calls');end=s.index('IOReturn\nAppleKeyStoreUserClient::externalMethod',start);aks+='\n'+s[start:end]
 tests=[];env=os.environ.copy();env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
 for name,code,marker in [('hid',hid,'FULL_TCM_HID_FRAME_HOST_PASS'),('aks',aks,'AKS_POLL_HOST_PASS')]:
  source=o/(name+'.cpp');source.write_text((kit/(name+'-host-prelude.inc')).read_text()+code+(kit/(name+'-host-main.inc')).read_text())
  exe=o/(name+('.exe' if os.name=='nt' else ''));cmd=[a.cc,'-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(source),'-o',str(exe)]
  for tag,args in [('compile',cmd),('run',[str(exe)]+([str(o/'device-state.der')] if name=='aks' else []))]:
   r=subprocess.run(args,capture_output=True,text=True,env=env);(o/(name+'-'+tag+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0 and 'runtime error:' not in r.stderr,name+' '+tag+' failed'
  assert r.stdout.startswith(marker);tests.append(dict(name=name,result=r.stdout.strip(),executableSHA256=hashlib.sha256(exe.read_bytes()).hexdigest()))
 (o/'audit.json').write_text(json.dumps(dict(result='UI_PUBLIC_HOST_PASS',tests=tests,nativeOutcomeProven=False),indent=2)+'\n');print('UI_PUBLIC_HOST_PASS tests=2')
if __name__=='__main__':main()
