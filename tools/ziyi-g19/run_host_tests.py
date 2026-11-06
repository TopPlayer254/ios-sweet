"""Exercise the exact G19 HFI, memory, boot-construction and native guards."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
P=Path(__file__).resolve().parent;K=P/'files';H=P.parent/'ziyi-hw/files'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--cc',default='cc');a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=False)
 env=os.environ.copy();cc=Path(a.cc)
 if cc.is_file():env['PATH']=str(cc.resolve().parent)+os.pathsep+env['PATH']
 env['ASAN_OPTIONS']='abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(K),'-I'+str(H)]
 cases=[('hfi',[K/'test_diagnostic.c',K/'ziyi_hfi_diagnostic.c',K/'ziyi_hfi_record.c',K/'ziyi_hfi_live.c',K/'ziyi_hfi_version.c',H/'ziyi_g8_contract.c'],[],[str(a.out.resolve())],'G19_HFI_DIAGNOSTIC_HOST_PASS'),
 ('owned',[K/'test_owned_pages.c',K/'ziyi_g19_owned_pages.c',K/'ziyi_hfi_page_observation.c'],[],[],'HFI_OWNED_PAGES_HOST_PASS'),
 ('pmap',[K/'test_pmap_helper.c'],[],[],'HFI_PMAP_HELPER_MODELED_PRIMITIVES_PASS'),
 ('external',[K/'test_external_hfi.c',H/'ziyi_g8_memory.c',H/'ziyi_g8_translation.c',H/'ziyi_g8_contract.c'],[],[],'G19_EXTERNAL_HFI_MAPPING_PASS'),
 ('store',[P/'test_native_store.c'],[],[],'G19_ACTUAL_NATIVE_STORE_GUARD_HOST_PASS'),
 ('boot-old',[K/'test_construction.c'],['-DG19_TEST=0'],[],'G19_ACTUAL_BOOT_MAPPER_HOST_PASS'),
 ('boot-new',[K/'test_construction.c'],['-DG19_TEST=1'],[],'G19_ACTUAL_BOOT_MAPPER_HOST_PASS')]
 rows=[]
 for name,sources,extra,args,marker in cases:
  exe=a.out/(name+('.exe' if os.name=='nt' else ''));cmd=[a.cc]+flags+extra+list(map(str,sources))+['-o',str(exe)]
  p=subprocess.run(cmd,env=env,capture_output=True,text=True);(a.out/(name+'-compile.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
  p=subprocess.run([str(exe.resolve())]+args,env=env,capture_output=True,text=True);(a.out/(name+'-run.log')).write_text(p.stdout+p.stderr);assert p.returncode==0 and marker in p.stdout and 'runtime error:' not in p.stderr,p.stdout+p.stderr
  rows.append(dict(name=name,result=p.stdout.strip(),executableSHA256=hashlib.sha256(exe.read_bytes()).hexdigest()))
 result=dict(result='G19_EXACT_PUBLIC_HOST_PASS',tests=rows,nativeHfiAck=False,phone=False);(a.out/'audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(result=result['result'],tests=len(rows))))
if __name__=='__main__':main()
