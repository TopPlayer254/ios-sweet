#!/usr/bin/env python3
"""Use native firmware-coordinated ALWAYS_ON domains in the opt-in RAM helper."""
import hashlib
import subprocess

PIN = '24856283d18328b8473cd10991c15749b88d7bf9'
NAME = 'drivers/cpuidle/cpuidle-psci-domain.c'
MARKER = 'ZIYI PSCI RAM: retain firmware coordination and ALWAYS_ON domains\n'
RUNTIME_MARKER = 'ZIYI PSCI RAM: runtime idle driver skipped; genpd provider retained\n'


def apply(root, source_fixes):
    path = root / NAME
    original = path.read_bytes()
    assert original == subprocess.check_output(['git', '-C', str(root), 'show', PIN + ':' + NAME])
    text = original.decode()
    signature = 'static int psci_cpuidle_domain_probe(struct platform_device *pdev)\n{'
    assert text.count(signature) == 1
    start = text.index(signature); end = text.index('\n}\n', start) + 3
    body = text[start:end]
    changes = [
        ('\tstruct device_node *node;\n', '\textern bool ziyi_ram_usb_mode(void);\n\tstruct device_node *node, *ram_usb;\n'),
        ('\tbool use_osi;\n', '\tbool use_osi, keep_ram_powered;\n'),
        ('\tuse_osi = psci_pd_try_set_osi_mode();\n',
         '\tram_usb = of_find_node_by_path("/soc/ssusb@a600000");\n'
         '\tkeep_ram_powered = ziyi_ram_usb_mode() || (ram_usb &&\n'
         '\t\tof_property_read_bool(ram_usb, "ziyi,ram-usb-device-only"));\n'
         '\tof_node_put(ram_usb);\n'
         '\tif (keep_ram_powered) {\n'
         '\t\t/* Native use_osi=false path registers genpd ALWAYS_ON. */\n'
         '\t\tuse_osi = false;\n'
         '\t\tdev_info(&pdev->dev, "' + MARKER.replace('\n', '\\n') + '");\n'
         '\t} else {\n'
         '\t\tuse_osi = psci_pd_try_set_osi_mode();\n'
         '\t}\n')]
    for before, after in changes:
        assert body.count(before) == 1, before
        body = body.replace(before, after, 1)
    updated = (text[:start] + body + text[end:]).encode()
    assert path.read_bytes() == original
    path.write_bytes(updated)
    source_fixes[NAME] = {'before': hashlib.sha256(original).hexdigest(),
                          'after': hashlib.sha256(updated).hexdigest(),
                          'selector': 'ziyi_ram_usb=1 OR DTB ziyi,ram-usb-device-only',
                          'nativeFirmwareCoordinatedAlwaysOnPath': True}
    name = 'drivers/cpuidle/cpuidle-psci.c'
    path = root / name
    original = path.read_bytes()
    assert original == subprocess.check_output(['git', '-C', str(root), 'show', PIN + ':' + name])
    text = original.decode()
    signature = 'static int psci_cpuidle_probe(struct platform_device *pdev)\n{'
    assert text.count(signature) == 1
    start = text.index(signature); end = text.index('\n}\n', start) + 3
    body = text[start:end]
    anchor = '\tfor_each_possible_cpu(cpu) {\n'
    assert body.count(anchor) == 1
    guard = ('\t{\n\t\textern bool ziyi_ram_usb_mode(void);\n'
             '\t\tstruct device_node *ram_usb = of_find_node_by_path("/soc/ssusb@a600000");\n'
             '\t\tbool keep_active = ziyi_ram_usb_mode() || (ram_usb &&\n'
             '\t\t\tof_property_read_bool(ram_usb, "ziyi,ram-usb-device-only"));\n'
             '\t\tof_node_put(ram_usb);\n'
             '\t\tif (keep_active) {\n'
             '\t\t\tdev_info(&pdev->dev, "' + RUNTIME_MARKER.replace('\n', '\\n') + '");\n'
             '\t\t\treturn -ENODEV;\n\t\t}\n\t}\n')
    updated = (text[:start] + body.replace(anchor, guard + anchor, 1) + text[end:]).encode()
    assert path.read_bytes() == original
    path.write_bytes(updated)
    source_fixes[name] = {'before': hashlib.sha256(original).hexdigest(),
                          'after': hashlib.sha256(updated).hexdigest(),
                          'selector': 'ziyi_ram_usb=1 OR DTB ziyi,ram-usb-device-only',
                          'runtimeIdleDriverSkippedBeforeCpuInit': True,
                          'separateGenpdProviderRetained': True}
