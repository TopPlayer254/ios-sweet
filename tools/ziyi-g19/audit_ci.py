"""Mandatory exact source and active linked G19 audit after full XNU build."""
from pathlib import Path
import hashlib,json,os,sys
R=Path.cwd();sys.path.insert(0,str(R/'tools/ziyi-hw'))
from macho_text import text_symbols,function_bytes
def main():
 m=json.loads((R/'tools/ziyi-g19/manifest.json').read_text());o=R/'u1-evidence/g19';o.mkdir(exist_ok=False);rows=[]
 for row in m['files']:
  p=R/'dxb/xnu'/row['path'];b=p.read_bytes();assert hashlib.sha256(b).hexdigest()==row['after'],row['path'];dest=o/'sources'/row['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(b);rows.append(dict(path=row['path'],sha256=row['after'],bytes=len(b)))
 for row in m['publicFiles']:assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
 assert hashlib.sha256((R/m['patch']).read_bytes()).hexdigest()==m['patchSHA256']
 for n in ['ziyi_gpu_probe.c','ziyi_g19_owned_pages.c','ziyi_hfi_diagnostic.c']:
  assert (R/'dxb/xnu/bsd/dev'/n).stat().st_size>1000
 kernel=R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet';b=kernel.read_bytes();p=text_symbols(b);linked=[]
 for name in m['linkedFunctions']:
  f=function_bytes(b,p,name);f['sampleSHA256']=hashlib.sha256(f.pop('sample')).hexdigest();linked.append(f)
 assert b'ZIYI HFI result stage' in b and b'ZIYI HFI memory STOP' in b
 result=dict(result='G19_FULL_KERNEL_SOURCE_LINK_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=hashlib.sha256(b).hexdigest(),files=rows,linkedFunctions=linked,nativeHfiAck=False,acceleratedUI=False,phoneOperated=False)
 (o/'audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ['files','linkedFunctions']}))
if __name__=='__main__':main()
