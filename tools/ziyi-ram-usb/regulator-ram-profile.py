#!/usr/bin/env python3
"""Keep inherited unused rails in the opt-in RAM helper, after supply resolution."""
import hashlib
import subprocess

PIN = '24856283d18328b8473cd10991c15749b88d7bf9'
NAME = 'drivers/regulator/core.c'
MARKER = 'ZIYI RAM REGULATOR: preserve inherited unused rails\n'


def apply(root, source_fixes):
    path = root / NAME
    original = path.read_bytes()
    assert original == subprocess.check_output(['git', '-C', str(root), 'show', PIN + ':' + NAME])
    text = original.decode()
    signature = 'static void regulator_init_complete_work_function(struct work_struct *work)\n{'
    assert text.count(signature) == 1
    start = text.index(signature); end = text.index('\n}\n', start) + 3
    body = text[start:end]
    anchor = '\tclass_for_each_device(&regulator_class, NULL, NULL,\n\t\t\t      regulator_late_cleanup);\n'
    assert body.count(anchor) == 1
    guard = ('\t{\n'
             '\t\tstruct device_node *ram_usb = of_find_node_by_path("/soc/ssusb@a600000");\n'
             '\t\tbool keep_inherited = ram_usb &&\n'
             '\t\t\tof_property_read_bool(ram_usb, "ziyi,ram-usb-device-only");\n'
             '\t\tof_node_put(ram_usb);\n'
             '\t\tif (keep_inherited) {\n'
             '\t\t\t/* Supply resolution above and normal driver/thermal controls remain. */\n'
             '\t\t\tpr_info("' + MARKER.replace('\n', '\\n') + '");\n'
             '\t\t\treturn;\n\t\t}\n\t}\n')
    body = body.replace(anchor, guard + anchor, 1)
    updated = (text[:start] + body + text[end:]).encode()
    assert path.read_bytes() == original
    path.write_bytes(updated)
    source_fixes[NAME] = {'before': hashlib.sha256(original).hexdigest(),
                          'after': hashlib.sha256(updated).hexdigest(),
                          'requiresDtbBoolean': 'ziyi,ram-usb-device-only',
                          'supplyResolutionPreserved': True,
                          'onlyUnusedLateCleanupSkipped': True}
