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
report=dict(result='U23_CI_SOURCE_AUDIT_PASS',commit=os.environ['GITHUB_SHA'],files=checks,
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
