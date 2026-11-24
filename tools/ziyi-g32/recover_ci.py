"""Requalify an unchanged full kernel after a strict checker-count correction."""
from pathlib import Path
import hashlib,json,os,shutil,struct,subprocess,urllib.request,urllib.error,zipfile,io

ROOT=Path.cwd()
SOURCE='887d3e3a344ae00a394147ae46a914f8a50d8e25'
RUN=45453517427
FAILURE='Exact G32 independent CPU optical channel'
API='https://api.github.com/repos/TopPlayer254/ios-sweet'
NEW_FILES=['.github/workflows/ziyi-g32-audit-recovery.yml','tools/ziyi-g32/recover_ci.py']
sha=lambda b:hashlib.sha256(b).hexdigest()

class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):return None

def main():
 checker=os.environ['GITHUB_SHA'];token=os.environ['GH_TOKEN']
 changed=subprocess.check_output(['git','diff','--name-only',SOURCE,checker],text=True).splitlines()
 assert sorted(changed)==NEW_FILES,changed
 out=ROOT/'g32-recovery';out.mkdir(exist_ok=False)
 headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json'}
 def get(path):
  with urllib.request.urlopen(urllib.request.Request(API+path,headers=headers),timeout=45) as r:return json.load(r)
 run=get('/actions/runs/'+str(RUN));assert run['head_sha']==SOURCE and run['conclusion']=='failure'
 assert run['path']=='.github/workflows/ziyi-wifi-native-kernel.yml'
 jobs=get('/actions/runs/'+str(RUN)+'/jobs')['jobs'];assert len(jobs)==1
 steps=jobs[0]['steps'];assert [s['name'] for s in steps if s['conclusion']=='failure']==[FAILURE]
 for name in ['Build','Audit every legacy IIG invocation','Verify U1 compiled sources','Audit exact combined sources and nonempty linked adapters',
              'Exact G19 source and active linked HFI audit','Exact G20 CPU snapshot source and link audit',
              'Exact G21 valid-bit source and link audit','Exact G22 pre-user source and linked gate audit',
              'Exact G23 own-resource startup source and linked calls','Exact G24 sources linked callbacks and real SMC',
              'Exact G25 optional counter deferral and linked printer','Exact G26 clock group current-view source and linked helper',
              'Exact G27 defined GX clock mode and linked guard','Exact G28 clock selector and auxiliary telemetry linked paths',
              'Exact G29 CP cohort and actual command release linked guards','Exact G30 preaccess witness and fresh guard linked callbacks',
              'Exact G31 CPU observer and SQE instruction sequence']:
  assert next(s for s in steps if s['name']==name)['conclusion']=='success',name
 artifacts=get('/actions/runs/'+str(RUN)+'/artifacts')['artifacts'];receipts=[]
 for name in ['ziyi-u1-evidence','xnu-kernel']:
  meta,=[v for v in artifacts if v['name']==name];assert not meta['expired']
  request=urllib.request.Request(API+'/actions/artifacts/'+str(meta['id'])+'/zip',headers=headers)
  try:urllib.request.build_opener(NoRedirect).open(request,timeout=45)
  except urllib.error.HTTPError as e:
   assert e.code==302;url=e.headers['Location']
  else:raise AssertionError('Expected artifact redirect')
  assert url.startswith('https://')
  with urllib.request.urlopen(url,timeout=90) as r:data=r.read()
  assert len(data)==meta['size_in_bytes'] and 'sha256:'+sha(data)==meta['digest']
  archive=zipfile.ZipFile(io.BytesIO(data));assert archive.testzip() is None
  dest=out/'original'/name
  for entry in archive.infolist():
   if entry.is_dir():continue
   assert '\\' not in entry.filename and ':' not in entry.filename and not entry.filename.startswith('/')
   assert (entry.external_attr>>16)&0o170000!=0o120000
   p=(dest/entry.filename).resolve();assert p.is_relative_to(dest.resolve());p.parent.mkdir(parents=True,exist_ok=True)
   with p.open('xb') as stream:stream.write(archive.read(entry))
  receipts.append(dict(name=name,id=meta['id'],bytes=len(data),sha256=sha(data)))
 evidence=out/'original/ziyi-u1-evidence';kernel=out/'original/xnu-kernel/kernel.development.sweet'
 raw=kernel.read_bytes();prior=json.loads((evidence/'g31/audit.json').read_text())
 assert prior['commit']==SOURCE and prior['kernelSha256']==sha(raw)
 assert prior['exactOriginalSingleMMIOStore'] and prior['exactOriginalSingleDSB']
 # Preserve downloaded sources. Recreate the audit's build-shaped inputs separately.
 shutil.copytree(evidence,ROOT/'u1-evidence')
 manifest=json.loads((ROOT/'tools/ziyi-g32/manifest.json').read_text())
 for row in manifest['files']:
  data=(evidence/'g31/sources'/row['path']).read_bytes();assert sha(data)==row['after']
  p=ROOT/'dxb/xnu'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 for name,h in manifest['unchanged'].items():
  data=(evidence/'g24/sources/bsd/dev'/name).read_bytes();assert sha(data)==h
  p=ROOT/'dxb/xnu/bsd/dev'/name;p.write_bytes(data)
 p=ROOT/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet';p.parent.mkdir(parents=True);shutil.copyfile(kernel,p)
 original=(ROOT/'tools/ziyi-g32/audit_ci.py').read_text()
 fixed=original.replace("O=R/'u1-evidence/g32';", "O=R/'u1-evidence/g32-recovered';")
 needle="('draw','_g32_emit_record','_g32_draw',1)";assert fixed.count(needle)==1
 fixed=fixed.replace(needle,"('draw','_g32_emit_record','_g32_draw',2)")
 needle='words.count(0xd5033e9f)==1';assert fixed.count(needle)==1
 fixed=fixed.replace(needle,'words.count(0xd5033e9f)==2')
 (out/'original-audit.py').write_text(original);(out/'corrected-audit.py').write_text(fixed)
 # Attribute the unchanged kernel to its true source build, separately from checker revision.
 os.environ['GITHUB_SHA']=SOURCE
 namespace={'__file__':str(ROOT/'tools/ziyi-g32/audit_ci.py'),'__name__':'g32_recovery_audit'}
 try:exec(compile(fixed,str(out/'corrected-audit.py'),'exec'),namespace)
 finally:os.environ['GITHUB_SHA']=checker
 start,code=namespace['body']('_g32_emit_record');words=struct.unpack('<%dI'%(len(code)//4),code)
 barriers=[start+4*i for i,w in enumerate(words) if w==0xd5033e9f]
 calls=namespace['actual'];draws=calls['draw']
 assert len(draws)==len(barriers)==2 and draws[0]<barriers[0]<draws[1]<barriers[1]
 assert calls['record'][0]<calls['packet'][0]<calls['packet'][1]<draws[0]
 shutil.copytree(ROOT/'u1-evidence/g32-recovered',out/'g32')
 (out/'recovery.json').write_text(json.dumps(dict(result='G32_UNCHANGED_KERNEL_AUDIT_RECOVERY_PASS',
  sourceCommit=SOURCE,sourceRun=RUN,sourceRunConclusion='failure',onlyFailedStep=FAILURE,
  checkerCommit=checker,kernelSha256=sha(raw),kernelInputsUnchanged=True,
  originalAuditSHA256=sha(original.encode()),correctedAuditSHA256=sha(fixed.encode()),
  drawCalls=draws,observerStoreBarriers=barriers,twoPagesUnrolled=True,
  exactOriginalSQEStoreAndDSBRetained=True,allEarlierFullBuildAuditsPassed=True,
  artifacts=receipts,native=False,gpuPixels=False,phoneOperated=False),indent=2)+'\n')
 print('G32_UNCHANGED_KERNEL_AUDIT_RECOVERY_PASS',sha(raw))

if __name__=='__main__':main()
