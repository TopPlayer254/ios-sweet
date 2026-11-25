"""Requalify the unchanged G33 kernel after narrowing a stale audit boundary."""
from pathlib import Path
import argparse,hashlib,io,json,os,shutil,subprocess,urllib.request,urllib.error,zipfile
SOURCE='404ca1e73057f894968374bef33d6f3d28d59ed0';RUN=45586553421
FAILURE='Exact G34 compiled VS FS and guarded triangle calls';API='https://api.github.com/repos/TopPlayer254/ios-sweet'
NEW_FILES=['.github/workflows/ziyi-g34-audit-recovery.yml','tools/ziyi-g34/recover_ci.py']
sha=lambda b:hashlib.sha256(b).hexdigest()
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):return None
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--pc-only',action='store_true');ap.add_argument('--out',type=Path,default=Path('g34-recovery'));a=ap.parse_args();root=Path.cwd();out=a.out.resolve();out.mkdir(exist_ok=False)
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
 required=['Build','Audit every legacy IIG invocation','Verify U1 compiled sources','Audit exact combined sources and nonempty linked adapters','Exact G19 source and active linked HFI audit','Exact G20 CPU snapshot source and link audit','Exact G21 valid-bit source and link audit','Exact G22 pre-user source and linked gate audit','Exact G23 own-resource startup source and linked calls','Exact G24 sources linked callbacks and real SMC','Exact G25 optional counter deferral and linked printer','Exact G26 clock group current-view source and linked helper','Exact G27 defined GX clock mode and linked guard','Exact G28 clock selector and auxiliary telemetry linked paths','Exact G29 CP cohort and actual command release linked guards','G33 LPAC native adapter and diagnostic publication batch','Exact G30 preaccess witness and fresh guard linked callbacks','Exact G31 CPU observer and SQE instruction sequence','Exact G32 independent CPU optical channel','Exact G33 LPAC route and pre-SQE CPU publication','G34 actual triangle adapter and final receipts']
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
 manifest=json.loads((root/'tools/ziyi-g34/manifest.json').read_text())
 for row in manifest['files']:
  data=(evidence/'g29/sources'/row['path']).read_bytes();assert sha(data)==row['after'];p=work/'dxb/xnu'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 for name,h in manifest['unchanged'].items():
  data=(evidence/'g24/sources/bsd/dev'/name).read_bytes();assert sha(data)==h;(work/'dxb/xnu/bsd/dev'/name).write_bytes(data)
 p=work/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet';p.parent.mkdir(parents=True);shutil.copyfile(kernel,p)
 original=(root/'tools/ziyi-g34/audit_ci.py').read_text()
 old="('draw','_g34n_emit_final','_g34_final_draw',[1,10])"
 new="('draw','_g34n_emit_final','_g34_final_draw',[2])"
 assert original.count(old)==1
 fixed=original.replace(old,new).replace("O=R/'u1-evidence/g34';","O=R/'u1-evidence/g34-recovered';")
 (out/'original-audit.py').write_text(original);(out/'corrected-audit.py').write_text(fixed)
 assert sha(raw)=='6507fa02b4052d33fb4ea652caba385d732962f963ed15414c7390c5ba450b24'
 old_cwd=Path.cwd();os.chdir(work);os.environ['GITHUB_SHA']=SOURCE
 try:
  ns={'__file__':str(work/'tools/ziyi-g34/audit_ci.py'),'__name__':'g34_recovery'}
  exec(compile(fixed,str(out/'corrected-audit.py'),'exec'),ns)
 finally:os.chdir(old_cwd);os.environ['GITHUB_SHA']=checker
 # Independently qualify the compiler's actual first-page peel, not a loose
 # acceptance of two calls. Ten packet constructors are completely unrolled.
 parsed=ns['parsed'];fb=ns['function_bytes'];e=parsed['symbols']['_g34n_emit_final'][0]
 sec=parsed['sections'][e['section']];off=sec['offset']+e['address']-sec['address']
 import struct
 size=fb(raw,parsed,'_g34n_emit_final')['bytesToNextTextSymbol'];body=raw[off:off+size]
 w=list(struct.unpack('<'+'I'*(len(body)//4),body[:len(body)//4*4]))
 assert w.count(0x52801016)==1 # initial byte offset x22=128
 assert sum(w[i:i+2]==[0x910202d7,0xf11602ff]for i in range(len(w)-1))==1 # next=x22+128; sentinel1408
 assert sum(w[i:i+2]==[0xaa1703f6,0x35fffd20]for i in range(len(w)-1))==1 # update and successful loop back
 audit=json.loads((work/'u1-evidence/g34-recovered/audit.json').read_text())
 assert len(audit['actualCalls']['packet'])==10 and len(audit['actualCalls']['draw'])==2
 for page,call in enumerate(audit['actualCalls']['packet']):
  at=(call-e['address'])//4
  assert (0x52800002|(page<<5)) in w[at-6:at] # exact w2=page0..9, not inferred from a count
 assert [n*128 for n in range(1,10)]==list(range(128,1280,128))
 shutil.copytree(work/'u1-evidence/g34-recovered',out/'g34')
 proof=dict(result='G34_PC_UNCHANGED_KERNEL_AUDIT_PASS'if a.pc_only else'G34_UNCHANGED_KERNEL_AUDIT_RECOVERY_PASS')

 proof.update(sourceCommit=SOURCE,sourceRun=RUN,sourceRunConclusion='failure',onlyFailedStep=FAILURE,checkerCommit=checker,kernelSha256=sha(raw),kernelInputsUnchanged=True,
  originalAuditSHA256=sha(original.encode()),correctedAuditSHA256=sha(fixed.encode()),onlyOpticalCallCountAndOutputChanged=True,
  exactPeeledFirstThenNineDrawLoop=True,packetPages=list(range(10)),drawCallsites=2,drawExecutions=10,
  loopInitialBytes=128,loopStepBytes=128,loopSentinelBytes=1408,shaderCodeAndGPUDriverUnchanged=True,
  earlierFullBuildAuditsPassed=required,recoveredAudits=['g34'],artifacts=receipts,native=False,gpuTriangle=False,phoneOperated=False)
 (out/'recovery.json').write_text(json.dumps(proof,indent=2)+'\n');print('G34_UNCHANGED_KERNEL_AUDIT_PASS',sha(raw))
if __name__=='__main__':main()
