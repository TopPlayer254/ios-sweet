"""Check actual BSD call order and the linked XNU logger, before packaging."""
import hashlib, json, struct, sys
from pathlib import Path
root = Path.cwd()
sys.path.insert(0, str(root/'tools/ziyi-hw'))
from macho_text import text_symbols, function_bytes
expected = {'bsd/dev/sweet_ufs.c': 'd19936a1a1a4a7dbc288dffe10cf1672bc08e7a4a2f1340a32df4a4c4171d55c', 'bsd/kern/bsd_init.c': '9cab4a679041a425f047b13649042bc22af728c428ac23f3102f2eccdb3dbb0e'}
for name, digest in expected.items():
    assert hashlib.sha256((root/'dxb/xnu'/name).read_bytes()).hexdigest() == digest
raw = (root/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet').read_bytes()
symbols = text_symbols(raw)
def call(caller, callee):
    f = function_bytes(raw, symbols, caller)
    target = symbols['symbols'][callee][0]['address']
    section = symbols['sections'][f['section']]
    start = f['address']
    offset = section['offset'] + start-section['address']
    code = raw[offset:offset+f['bytesToNextTextSymbol']]
    matches = []
    for i in range(0, len(code)-3, 4):
        ins = struct.unpack_from('<I', code, i)[0]
        if ins & 0xfc000000 != 0x94000000:
            continue
        imm = (ins & 0x3ffffff) ^ 0x2000000
        if start+i+4*(imm-0x2000000) == target:
            matches.append(start+i)
    assert len(matches) == 1, (caller, callee, matches)
    return matches[0]
gate = call('_bsd_init', '_ziyi_log_probe')
assert gate < call('_bsd_init', '_ziyi_gpu_preuser_start') < call('_bsd_init', '_IOKitBSDInit')
for name in ['_sweet_log_append', '_ziyi_bootlog_snapshot', '_ziyi_log_probe']:
    assert function_bytes(raw, symbols, name)['bytesToNextTextSymbol'] > 16
assert b'ZIYI headless XNU BSD checkpoint' in raw
(root/'u1-evidence/xnu-pstore.json').write_text(json.dumps(dict(result='XNU_PSTORE_LINKED',
    files=expected, kernelSHA256=hashlib.sha256(raw).hexdigest(), bootProbePC=gate), indent=2)+'\n')
