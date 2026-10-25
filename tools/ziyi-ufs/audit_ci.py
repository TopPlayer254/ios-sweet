"""Verify the U1 sources actually compiled by the full XNU CI build."""
from pathlib import Path
import hashlib,json,os
root=Path.cwd()
expected=json.loads((root/'tools/ziyi-ufs/manifest.json').read_text())
checks=[]
for name,item in expected.items():
    data=(root/'dxb/xnu'/name).read_bytes()
    actual=hashlib.sha256(data).hexdigest()
    assert actual==item['after'],(name,actual,item)
    dest=root/'u1-evidence/sources'/name
    dest.parent.mkdir(parents=True,exist_ok=True)
    with dest.open('xb') as f:f.write(data)
    checks.append(dict(path=name,sha256=actual,bytes=len(data)))
report=dict(result='U28_CI_SOURCE_AUDIT_PASS',commit=os.environ['GITHUB_SHA'],files=checks,
            phoneTested=False,mediaWritesImplemented=True,writeGate="exact GPT and full install SHA or bound installed HFS identity; opt-in prefix only, FUA/readback")
(root/'u1-evidence/audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))

# The exercised header must be the exact kernel header, and transport
# must call it directly. Old FIFO tests remain historical regression tests.
touch=(root/'dxb/xnu/bsd/dev/sweet_touch.c').read_text()
assert '#include "ziyi_u18_gpio_spi.h"' in touch
assert 'int e=bb_transfer(&zt_board_io,&zt_board,tx,rx,bytes);' in touch
assert 'int take=bb_take(&zt_board_io,&zt_board);' in touch
assert 'int restore=bb_release(&zt_board_io,&zt_board);' in touch
assert (root/'tools/ziyi-ufs/u21_gpio_spi.h').read_bytes()==(root/'dxb/xnu/bsd/dev/ziyi_u18_gpio_spi.h').read_bytes()
assert (root/'tools/ziyi-ufs/u16_board.h').read_bytes()==(root/'dxb/xnu/bsd/dev/ziyi_u13_board.h').read_bytes()

assert (root/'tools/ziyi-ufs/u14_ui.h').read_bytes()==(root/'dxb/xnu/pexpert/pexpert/arm64/ziyi_u12_ui.h').read_bytes()
assert (root/'tools/ziyi-ufs/u14_timing.h').read_bytes()==(root/'dxb/xnu/pexpert/pexpert/arm64/ziyi_u14_timing.h').read_bytes()
assert (root/'tools/ziyi-ufs/u14_gpi.h').read_bytes()==(root/'dxb/xnu/bsd/dev/ziyi_u14_gpi.h').read_bytes()

assert '#include "ziyi_u21_power.h"' in touch
assert 'int power=zp_start(&zt_power_io,&zt_power);' in touch
assert (root/'tools/ziyi-ufs/u22_power.h').read_bytes()==(root/'dxb/xnu/bsd/dev/ziyi_u21_power.h').read_bytes()

assert "ZIYI U23 RPMh phase=" in touch and "ZIYI U23 RPMh raw ctl=" in touch

assert '#include "ziyi_u23_native_touch.h"' in touch
assert 'zi_tcm_report_native(zt_config,zt_config_len,zt_message,n,contacts,zt_max_x,zt_max_y)' in touch
assert (root/'tools/ziyi-ufs/u23_native_touch.h').read_bytes()==(root/'dxb/xnu/bsd/dev/ziyi_u23_native_touch.h').read_bytes()

# U25 checks the actual compiled SMP profile, IRQ hook and policy header.
smp=(root/'dxb/xnu/iokit/Kernel/arm/SweetCPU.cpp').read_text()
irq=(root/'dxb/xnu/osfmk/arm64/sleh.c').read_text()
cfg=(root/'dxb/xnu/pexpert/pexpert/arm64/SWEET.h').read_text()
assert '#define SWEET_SMP_MAX_CPUS         8 /* U25: native-checked SM7450 SMP */' in cfg
assert '#define SWEET_VTIMER_INTID        27' in cfg and '#define GICR_PE_SIZE              0x40000' in cfg
assert 'z25_validate(topology,processors);' in smp and 'ZIYI U25 SMP PASS' in smp
assert 'ziyi_u25_irq_record(cdp->cpu_number,1);' in irq
assert 'ziyi_u25_irq_record(cdp->cpu_number,2);' in irq
assert (root/'tools/ziyi-ufs/u25_smp_policy.h').read_bytes()==(root/'dxb/xnu/pexpert/pexpert/arm64/ziyi_u25_smp_policy.h').read_bytes()

# U27 service diagnostics are opt-in and strictly read-only.
sample=(root/'dxb/xnu/bsd/dev/sweet_sample.c').read_text()
exception=(root/'dxb/xnu/osfmk/kern/exception.c').read_text()
assert 'ZIYI U28 SVC path=' in sample and 'vnode_lookup(paths[i],0,&vp,ctx)' in sample
assert 'vnode_getattr(vp,&va,ctx)' in sample and 'struct vnode_attr va={0}' in sample
assert 'for (int i = 0; i < 120; i++)' in sample and '5 * hz)' in sample
assert '__atomic_exchange_n(&started,1,__ATOMIC_ACQ_REL)' in sample
assert all(x not in sample for x in ['VNOP_WRITE','vn_rdwr','vnode_open','VNOP_SYMLINK'])
body=exception[exception.index('sweet_sample_task(const char *name, int pid, task_t task)\n{'):]
body=body[:body.index('\n}\n')]
assert 'thread_t selected[1]' in body and 'thread_reference(thread)' in body
assert body.index('task_unlock(task)')<body.index('vm_map_read_user')
assert 'thread_deallocate(thread);continue;' in body

assert 'sweet_service_path((unsigned)i);' in sample
assert 'i < 8 && fp' in body
assert 'if(used==1)break' in body
