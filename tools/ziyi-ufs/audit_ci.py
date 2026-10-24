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
report=dict(result='U16_CI_SOURCE_AUDIT_PASS',commit=os.environ['GITHUB_SHA'],files=checks,
            phoneTested=False,mediaWritesImplemented=True,writeGate="exact GPT and full install SHA or bound installed HFS identity; opt-in prefix only, FUA/readback")
(root/'u1-evidence/audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))

# Bind the fake-MMIO transport test to the actual compiled SPI functions.
touch=(root/'dxb/xnu/bsd/dev/sweet_touch.c').read_text()
spi=(root/'tools/ziyi-ufs/test_u16_spi.c').read_text()
assert touch[touch.index('/* Snapshot status/configuration only:'):touch.index('static int zt_read_message(')] == spi[spi.index('/* Snapshot status/configuration only:'):spi.index('static void reset(')]
assert (root/'tools/ziyi-ufs/u16_board.h').read_bytes()==(root/'dxb/xnu/bsd/dev/ziyi_u13_board.h').read_bytes()

assert (root/'tools/ziyi-ufs/u14_ui.h').read_bytes()==(root/'dxb/xnu/pexpert/pexpert/arm64/ziyi_u12_ui.h').read_bytes()
assert (root/'tools/ziyi-ufs/u14_timing.h').read_bytes()==(root/'dxb/xnu/pexpert/pexpert/arm64/ziyi_u14_timing.h').read_bytes()
assert (root/'tools/ziyi-ufs/u14_gpi.h').read_bytes()==(root/'dxb/xnu/bsd/dev/ziyi_u14_gpi.h').read_bytes()
