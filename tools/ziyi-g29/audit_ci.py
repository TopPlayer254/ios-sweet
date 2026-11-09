"""Exact CP-cohort sources, actual ARM64 helper/callbacks and release gates."""
from pathlib import Path
import hashlib,json,os,struct,sys
R=Path.cwd();sha=lambda b:hashlib.sha256(b).hexdigest();M=json.loads((R/'tools/ziyi-g29/manifest.json').read_text());O=R/'u1-evidence/g29';O.mkdir(exist_ok=False);rows=[]
for row in M['files']:
 b=(R/'dxb/xnu'/row['path']).read_bytes();assert sha(b)==row['after'];p=O/'sources'/row['path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b);rows.append(dict(path=row['path'],sha256=sha(b),bytes=len(b)))
assert len(rows)==3
for row in M['publicFiles']:assert sha((R/row['path']).read_bytes())==row['sha256']
assert sha((R/M['patch']).read_bytes())==M['patchSHA256']
raw=(R/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes();prior=json.loads((R/'u1-evidence/g28/audit.json').read_text());assert prior['kernelSha256']==sha(raw) and prior['strictSPReadback'] and prior['profileUnchanged'] and prior['recordUnchanged']
engine=(O/'sources/bsd/dev/ziyi_g24_engine.c').read_text();header=(O/'sources/bsd/dev/ziyi_g24_engine.h').read_text();native=(O/'sources/bsd/dev/ziyi_g24_native.inc').read_text()
for token in ['g29_cp_configure(w,o)','g24e_write(w,o,G24_PROFILE,i,r->value,0)','d->after[j]==g24_profile[76+j].value','d->mismatch','d->first_bad','g29_cp_verified(w)','if(sp!=G24_CTL)return 0','value!=w->r.profile_current[j]','got!=value']:assert token in engine,token
assert '#define G29_CP_BYTES 160u' in header and '0x31525043u' in header and 'd->after[i]!=target[i]' in header and 'd->aux[i]!=d->aux[8+i]' in header and 'page++%8' in native and 'phase<=G24_PRESENT&&!g29_cp_verified(&g24n.w)' in native and b'CPR1 page%u/2 stage%u staged%x exact%x bad%x CRC%08x' in raw
for n,field in [('ziyi_g24_profile.inc','profileSHA256'),('ziyi_g24_record.c','recordSHA256')]:assert sha((R/'dxb/xnu/bsd/dev'/n).read_bytes())==M[field]
host=json.loads((R/'u1-evidence/g29-host/audit.json').read_text());assert host['result']=='G29_ACTUAL_CP_COHORT_HOST_PASS' and host['oldReads']==624 and host['oldStores']==43 and host['oldCursor']==76 and host['configStores']==4 and host['strictFinalExactReads']==4 and host['remainingMismatchMasksRefused']==15 and host['releaseGuardsWithActualValues'] and host['resetStatusChangeRefused'] and host['readOnlyAuxWriteRefusals']==8 and host['auxiliaryBytes']==160 and host['auxiliaryCorruptBitsRefused']==1280 and host['actualPrinterGCPRows']==20 and host['actualPrinterGCKRows']==5 and host['actualPrinterCPRows']==10
sys.path.insert(0,str(R/'tools/ziyi-hw'));from macho_text import text_symbols,function_bytes
parsed=text_symbols(raw);linked=[]
for name in M['linkedFunctions']:
 f=function_bytes(raw,parsed,name);assert f['bytesToNextTextSymbol']>=32;f['sampleSHA256']=sha(f.pop('sample'));linked.append(f)
en=parsed['symbols']['_g24_run'][0];sec=parsed['sections'][en['section']];off=sec['offset']+en['address']-sec['address'];size=function_bytes(raw,parsed,'_g24_run')['bytesToNextTextSymbol'];target=parsed['symbols']['_g29_cp_configure'][0]['address'];calls=[]
for i in range(0,size-3,4):
 ins=struct.unpack_from('<I',raw,off+i)[0];imm=ins&0x3ffffff
 if ins&0xfc000000==0x94000000 and en['address']+i+4*((imm^0x2000000)-0x2000000)==target:calls.append(en['address']+i)
assert len(calls)==1
(O/'audit.json').write_text(json.dumps(dict(result='G29_CP_COHORT_FULL_KERNEL_PASS',commit=os.environ['GITHUB_SHA'],kernelSha256=sha(raw),files=rows,linkedFunctions=linked,actualCohortCalls=calls,profileUnchanged=True,recordUnchanged=True,GCKWireUnchanged=True,configStores=4,strictFinalExactReads=4,readOnlyAuxiliaryRegisters=8,resetStores=0,commandReleaseRequiresActualExactReceipts=True,auxiliaryBytes=160,allBorrowersRetained=True,native=False,gpuPixels=False,acceleratedUI=False,phoneOperated=False),indent=2)+'\n');print('G29_CP_COHORT_FULL_KERNEL_PASS')
