"""Requalify the unchanged G33 kernel after narrowing a stale audit boundary."""
from pathlib import Path
import argparse,hashlib,io,json,os,shutil,subprocess,urllib.request,urllib.error,zipfile
SOURCE='2f0ddb6f45542f6e11b9a39846284ebd43359ab7';RUN=45489987168
FAILURE='Exact G30 preaccess witness and fresh guard linked callbacks';API='https://api.github.com/repos/TopPlayer254/ios-sweet'
NEW_FILES=['.github/workflows/ziyi-g33-audit-recovery.yml','tools/ziyi-g33/recover_ci.py']
sha=lambda b:hashlib.sha256(b).hexdigest()
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):return None
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--pc-only',action='store_true');ap.add_argument('--out',type=Path,default=Path('g33-recovery'));a=ap.parse_args();root=Path.cwd();out=a.out.resolve();out.mkdir(exist_ok=False)
 checker=os.environ['GITHUB_SHA'];token=os.environ['GH_TOKEN']
 if not a.pc_only:
  changed=subprocess.check_output(['git','diff','--name-only',SOURCE,checker],text=True).splitlines();assert sorted(changed)==NEW_FILES,changed
 else:assert checker==SOURCE
 headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json'}
 if a.pc_only:
  import requests
 def get(path):
  if a.pc_only:
   r=requests.get(API+path,headers=headers,timeout=45);r.raise_for_status();return r.json()
  with urllib.request.urlopen(urllib.request.Request(API+path,headers=headers),timeout=45) as r:return json.load(r)
 run=get('/actions/runs/'+str(RUN));assert run['head_sha']==SOURCE and run['status']=='completed' and run['conclusion']=='failure' and run['path']=='.github/workflows/ziyi-wifi-native-kernel.yml'
 jobs=get('/actions/runs/'+str(RUN)+'/jobs')['jobs'];assert len(jobs)==1;steps=jobs[0]['steps'];assert [s['name'] for s in steps if s['conclusion']=='failure']==[FAILURE]
 required=['Build','Audit every legacy IIG invocation','Verify U1 compiled sources','Audit exact combined sources and nonempty linked adapters','Exact G19 source and active linked HFI audit','Exact G20 CPU snapshot source and link audit','Exact G21 valid-bit source and link audit','Exact G22 pre-user source and linked gate audit','Exact G23 own-resource startup source and linked calls','Exact G24 sources linked callbacks and real SMC','Exact G25 optional counter deferral and linked printer','Exact G26 clock group current-view source and linked helper','Exact G27 defined GX clock mode and linked guard','Exact G28 clock selector and auxiliary telemetry linked paths','Exact G29 CP cohort and actual command release linked guards','G33 LPAC native adapter and diagnostic publication batch']
 for name in required:assert next(s for s in steps if s['name']==name)['conclusion']=='success',name
 (out/'source-run.json').write_text(json.dumps(run,indent=2)+'\n');(out/'source-jobs.json').write_text(json.dumps(jobs,indent=2)+'\n')
 artifacts=get('/actions/runs/'+str(RUN)+'/artifacts')['artifacts'];receipts=[]
 for name in ['ziyi-u1-evidence','xnu-kernel']:
  meta,=[v for v in artifacts if v['name']==name];assert not meta['expired'] and 0<meta['size_in_bytes']<100<<20
  request=urllib.request.Request(API+'/actions/artifacts/'+str(meta['id'])+'/zip',headers=headers)
  if a.pc_only:
   r=requests.get(request.full_url,headers=headers,allow_redirects=False,timeout=45);assert r.status_code==302;url=r.headers['Location']
  else:
   try:urllib.request.build_opener(NoRedirect).open(request,timeout=45)
   except urllib.error.HTTPError as e:assert e.code==302;url=e.headers['Location']
   else:raise AssertionError('Expected artifact redirect')
  assert url.startswith('https://')
  if a.pc_only:
   r=requests.get(url,timeout=90);r.raise_for_status();data=r.content
  else:
   with urllib.request.urlopen(url,timeout=90) as r:data=r.read()
  assert len(data)==meta['size_in_bytes'] and 'sha256:'+sha(data)==meta['digest'];(out/(name+'.zip')).write_bytes(data)
  z=zipfile.ZipFile(io.BytesIO(data));assert z.testzip() is None and sum(i.file_size for i in z.infolist())<300<<20;dest=out/'original'/name
  for item in z.infolist():
   if item.is_dir():continue
   assert '\\' not in item.filename and ':' not in item.filename and not item.filename.startswith('/') and (item.external_attr>>16)&0o170000!=0o120000
   p=(dest/item.filename).resolve();assert p.is_relative_to(dest);p.parent.mkdir(parents=True,exist_ok=True)
   with p.open('xb') as f:f.write(z.read(item))
  receipts.append(dict(name=name,id=meta['id'],bytes=len(data),sha256=sha(data)))
 evidence=out/'original/ziyi-u1-evidence';kernel=out/'original/xnu-kernel/kernel.development.sweet';raw=kernel.read_bytes();prior=json.loads((evidence/'g29/audit.json').read_text());assert prior['commit']==SOURCE and prior['kernelSha256']==sha(raw)
 work=out/'work';work.mkdir();shutil.copytree(root/'tools',work/'tools',ignore=shutil.ignore_patterns('__pycache__'));shutil.copytree(root/'xnu/patches',work/'xnu/patches');shutil.copytree(evidence,work/'u1-evidence')
 manifest=json.loads((root/'tools/ziyi-g33/manifest.json').read_text())
 for row in manifest['files']:
  data=(evidence/'g29/sources'/row['path']).read_bytes();assert sha(data)==row['after'];p=work/'dxb/xnu'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 for name,h in manifest['unchanged'].items():
  data=(evidence/'g24/sources/bsd/dev'/name).read_bytes();assert sha(data)==h;(work/'dxb/xnu/bsd/dev'/name).write_bytes(data)
 p=work/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet';p.parent.mkdir(parents=True);shutil.copyfile(kernel,p)
 original=(root/'tools/ziyi-g30/audit_ci.py').read_text();old=".split('static int g24n_read(',1)[0]";new=".split('static int g33_live(',1)[0]";assert original.count(old)==1
 fixed=original.replace(old,new).replace("O=R/'u1-evidence/g30';","O=R/'u1-evidence/g30-recovered';")
 native=(work/'dxb/xnu/bsd/dev/ziyi_g24_native.inc').read_text();producer=native.split('static int g30_cp_witness(',1)[1].split('static int g33_live(',1)[0]
 assert 'g24n_raw(' not in producer and 'io_map(' not in producer and 'smc #0' not in producer
 assert native.count('static int g33_live(')==1 and 'g33_read_native' not in producer
 (out/'original-audit.py').write_text(original);(out/'corrected-audit.py').write_text(fixed)
 old_cwd=Path.cwd();os.chdir(work);os.environ['GITHUB_SHA']=SOURCE
 try:
  exec(compile(fixed,str(out/'corrected-audit.py'),'exec'),{'__file__':str(work/'tools/ziyi-g30/audit_ci.py'),'__name__':'g33_recovery_g30'})
  recovered=work/'u1-evidence/g30-recovered'
  for p in recovered.rglob('*'):
   if not p.is_file():continue
   dst=work/'u1-evidence/g30'/p.relative_to(recovered);dst.parent.mkdir(parents=True,exist_ok=True)
   if dst.exists():assert dst.read_bytes()==p.read_bytes()
   else:shutil.copyfile(p,dst)
  for kit in ['g31','g32','g33']:
   p=work/f'tools/ziyi-{kit}/audit_ci.py';exec(compile(p.read_text(),str(p),'exec'),{'__file__':str(p),'__name__':'g33_recovery_'+kit})
 finally:os.chdir(old_cwd);os.environ['GITHUB_SHA']=checker
 for kit in ['g30','g31','g32','g33']:shutil.copytree(work/f'u1-evidence/{kit}',out/kit)
 g31=json.loads((out/'g31/audit.json').read_text());g33=json.loads((out/'g33/audit.json').read_text());assert g31['exactOriginalSingleMMIOStore'] and g31['exactOriginalSingleDSB'] and g33['onlyTwoTypedLPACStores']
 (out/'recovery.json').write_text(json.dumps(dict(result='G33_PC_UNCHANGED_KERNEL_AUDIT_PASS' if a.pc_only else 'G33_UNCHANGED_KERNEL_AUDIT_RECOVERY_PASS',sourceCommit=SOURCE,sourceRun=RUN,sourceRunConclusion='failure',onlyFailedStep=FAILURE,checkerCommit=checker,kernelSha256=sha(raw),kernelInputsUnchanged=True,originalAuditSHA256=sha(original.encode()),correctedAuditSHA256=sha(fixed.encode()),onlyAuditBoundaryAndOutputChanged=True,exactOriginalSQEStoreAndDSBRetained=True,earlierFullBuildAuditsPassed=required,recoveredAudits=['g30','g31','g32','g33'],artifacts=receipts,native=False,gpuPixels=False,phoneOperated=False),indent=2)+'\n');print('G33_UNCHANGED_KERNEL_AUDIT_PASS',sha(raw))
if __name__=='__main__':main()
