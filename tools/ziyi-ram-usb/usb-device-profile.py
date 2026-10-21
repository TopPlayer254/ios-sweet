#!/usr/bin/env python3
"""Pinned, opt-in peripheral-only profile for the storage-disabled RAM helper."""
import hashlib
import subprocess

PIN = '24856283d18328b8473cd10991c15749b88d7bf9'
NAME = 'drivers/usb/dwc3/dwc3-msm-core.c'
PROPERTY = 'ziyi,ram-usb-device-only'
MARKERS = ('ZIYI RAM USB device-only profile requires peripheral dr_mode\n',
           'ZIYI RAM USB device-only profile: native peripheral start\n')


def apply(root, source_fixes):
    path = root / NAME
    original = path.read_bytes()
    assert original == subprocess.check_output(['git', '-C', str(root), 'show', PIN + ':' + NAME])
    text = original.decode()
    signature = 'static int dwc3_msm_probe(struct platform_device *pdev)\n{'
    assert text.count(signature) == 1
    start = text.index(signature)
    end = text.index('\n}\n', start) + 3
    body = text[start:end]
    changes = [
        ('\tu32 val;\n', '\tu32 val;\n\tbool ziyi_ram_device_only = of_property_read_bool(node, "' + PROPERTY + '");\n'),
        ('\tdwc3_msm_parse_core_params(mdwc, dwc3_node);\n',
         '\tdwc3_msm_parse_core_params(mdwc, dwc3_node);\n'
         '\t/* RAM receiver: never select host or depend on Android cable policy. */\n'
         '\tif (ziyi_ram_device_only && mdwc->dr_mode != USB_DR_MODE_PERIPHERAL) {\n'
         '\t\tdev_err(dev, "' + MARKERS[0].replace('\n', '\\n') + '");\n'
         '\t\tret = -EINVAL;\n\t\tgoto err;\n\t}\n'),
        ('\tif (of_property_read_bool(node, "usb-role-switch")) {\n',
         '\tif (!ziyi_ram_device_only && of_property_read_bool(node, "usb-role-switch")) {\n'),
        ('\tif (of_property_read_bool(node, "extcon")) {\n',
         '\tif (!ziyi_ram_device_only && of_property_read_bool(node, "extcon")) {\n'),
        ('\t\tdwc3_ext_event_notify(mdwc);\n',
         '\t\tif (ziyi_ram_device_only)\n'
         '\t\t\tdev_info(dev, "' + MARKERS[1].replace('\n', '\\n') + '");\n'
         '\t\tdwc3_ext_event_notify(mdwc);\n')]
    for before, after in changes:
        assert body.count(before) == 1, before
        body = body.replace(before, after, 1)
    updated = (text[:start] + body + text[end:]).encode()
    assert path.read_bytes() == original
    path.write_bytes(updated)
    source_fixes[NAME] = {'before': hashlib.sha256(original).hexdigest(),
                          'after': hashlib.sha256(updated).hexdigest(),
                          'requiresDtbBoolean': PROPERTY,
                          'peripheralOnly': True,
                          'resourceChecksPreserved': True}
