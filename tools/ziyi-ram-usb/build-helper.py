#!/usr/bin/env python3
"""Build a receive-only RAM USB kernel candidate; never packages/boots a phone."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

PIN = "24856283d18328b8473cd10991c15749b88d7bf9"
here = Path(__file__).resolve().parent
root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(exist_ok=False, parents=True)
build = out / "build"
build.mkdir()


def run(args, **kw):
    print("+", *map(str, args), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kw)


actual = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
if actual != PIN:
    raise RuntimeError("GPL source pin mismatch")
if subprocess.check_output(["git", "-C", str(root), "status", "--porcelain", "--untracked-files=no"]):
    raise RuntimeError("dirty GPL source")
# Xiaomi's published source references an absent, optional hardware-ID Kconfig.
# Add only a NEW disabled stub; never patch unrelated sources or mask failures.
for name, symbol in [('hwid','MI_HARDWARE_ID'),('plaid','XIAOMI_GPU_PLAID')]:
    stub = root / f"drivers/misc/{name}/Kconfig"
    if stub.exists(): raise RuntimeError("optional Xiaomi source assumption changed")
    stub.parent.mkdir(exist_ok=True)
    with stub.open("x") as f: f.write(f'config {symbol}\n\tbool\n\tdefault n\n')
    makefile=stub.with_name('Makefile')
    if not makefile.exists():
        with makefile.open('x') as f: f.write('# Optional Xiaomi component omitted by GPL release; no objects.\n')
missing=[]
for kconfig in root.rglob("Kconfig*"):
    if str(kconfig.relative_to(root)).startswith('scripts/kconfig/tests/'): continue
    if not kconfig.is_file(): continue
    for name in re.findall(r'^\s*source\s+"([^"$*]+)"', kconfig.read_text(errors="replace"), re.M):
        if not (root/name).exists(): missing.append(str(kconfig.relative_to(root))+": "+name)
if missing: raise RuntimeError("unresolved source Kconfigs: " + repr(missing))
run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-DZIYI_HOST_TEST", here / "ram-usb-init.c", here / "sha256.c", "-o", out / "host-server"])
with (out / "protocol-tests.json").open("w") as f:
    run([sys.executable, here / "test-protocol.py", out / "host-server"], stdout=f)
run(["aarch64-linux-gnu-gcc", "-static", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation", "-Wl,--build-id=sha1", here / "ram-usb-init.c", here / "sha256.c", "-o", out / "ziyi-ram-init"])
elf = (out / "ziyi-ram-init").read_bytes()
if elf[:6] != b"\x7fELF\x02\x01" or int.from_bytes(elf[18:20], "little") != 183:
    raise RuntimeError("not ELF64 AArch64 little-endian")
phoff = int.from_bytes(elf[32:40], "little")
phsize = int.from_bytes(elf[54:56], "little")
phnum = int.from_bytes(elf[56:58], "little")
if any(int.from_bytes(elf[phoff+i*phsize:phoff+i*phsize+4], "little") in (2,3) for i in range(phnum)):
    raise RuntimeError("unexpected PT_INTERP/PT_DYNAMIC")
initlist = out / "initramfs.list"
initlist.write_text(f"dir /dev 0755 0 0\nnod /dev/console 0600 0 0 c 5 1\nfile /ziyi-ram-init {out / 'ziyi-ram-init'} 0755 0 0\n")
make = ["make", "-C", root, f"O={build}", "ARCH=arm64", "LLVM=1", "LLVM_IAS=1", "CC=clang-14", "LD=ld.lld-14", "AR=llvm-ar-14", "NM=llvm-nm-14", "OBJCOPY=llvm-objcopy-14", "OBJDUMP=llvm-objdump-14", "STRIP=llvm-strip-14", "CROSS_COMPILE=aarch64-linux-gnu-", "CLANG_TRIPLE=aarch64-linux-gnu-"]
run(make + ["gki_defconfig"])
# Only board/USB/power dependencies may be promoted from vendor modules.
# All other modules become disabled when MODULES=n; no Android module loader.
promote = {
 "COMMON_CLK_QCOM", "MSM_GCC_WAIPIO", "SM_GCC_DIWALI", "QCOM_CLK_RPMH",
 "QCOM_COMMAND_DB", "QCOM_RPMH", "QCOM_RPMH_REGULATOR", "REGULATOR_RPMH", "ARM_SMMU",
 "QCOM_GDSC_REGULATOR", "REGULATOR_STUB", "PINCTRL_WAIPIO", "PINCTRL_DIWALI",
 "PINCTRL_CAPE", "PINCTRL_QCOM_SPMI_PMIC", "SPMI_MSM_PMIC_ARB", "MFD_SPMI_PMIC",
 "QCOM_SCM", "QCOM_SMEM", "QCOM_SOCINFO", "QCOM_AOSS_QMP", "HWSPINLOCK_QCOM",
 "INTERCONNECT_QCOM", "INTERCONNECT_QCOM_DIWALI", "INTERCONNECT_QCOM_WAIPIO",
 "MSM_HSUSB_PHY", "USB_MSM_SSPHY_QMP", "USB_MSM_EUSB2_PHY", "USB_REPEATER",
 "USB_REDRIVER_NB7VPQ904M", "NOP_USB_XCEIV", "USB_DWC3_MSM", "IPC_LOGGING",
 "MSM_QMP", "MSM_GENI_SE", "MSM_GPI_DMA", "QCOM_BAM_DMA", "SPS",
 "QCOM_PDC", "QCOM_MSM_IPCC", "NVMEM_SPMI_SDAM", "QPNP_PBS",
}
vendor = (root / "arch/arm64/configs/vendor/ziyi_GKI.config").read_text()
selected = []
for line in vendor.splitlines():
    m = re.fullmatch(r"CONFIG_(\w+)=(.+)", line)
    if m and (m[1].startswith("ARCH_") or m[1] in promote):
        selected.append(f"CONFIG_{m[1]}={'y' if m[2] == 'm' else m[2]}")
fragment = out / "selected.fragment"
fragment.write_text("\n".join(selected) + "\n" + (here / "ram-helper.fragment").read_text() + f'\nCONFIG_INITRAMFS_SOURCE="{initlist}"\nCONFIG_INITRAMFS_ROOT_UID=0\nCONFIG_INITRAMFS_ROOT_GID=0\nCONFIG_INITRAMFS_COMPRESSION_GZIP=y\n')
run([root / "scripts/kconfig/merge_config.sh", "-m", "-O", build, build / ".config", fragment], cwd=root)
run(make + ["olddefconfig"])
config = (build / ".config").read_text()
settings = dict(re.findall(r"^(CONFIG_\w+)=(.*)$", config, re.M))
required_y = ["KEXEC", "BLK_DEV_INITRD", "INITRAMFS_FORCE", "DEVTMPFS", "PROC_FS", "SYSFS", "CONFIGFS_FS", "USB_DWC3", "USB_DWC3_MSM", "USB_GADGET", "USB_CONFIGFS", "USB_CONFIGFS_ACM", "CMDLINE_FORCE"]
required_n = ["MODULES", "BLOCK", "MTD", "SCSI", "MMC", "PSTORE", "DEVMEM", "DEVKMEM", "EFI", "KEXEC_FILE", "REMOTEPROC", "WATCHDOG", "QCOM_QFPROM", "QCOM_QFPROM_SYS", "NVMEM_QCOM_QFPROM", "POWER_RESET_QCOM_DOWNLOAD_MODE"]
for key in required_y:
    if settings.get("CONFIG_"+key) != "y": raise RuntimeError(f"required builtin {key} missing")
for key in required_n:
    if settings.get("CONFIG_"+key) in ("y", "m"): raise RuntimeError(f"forbidden {key} enabled")
shutil.copy2(build / ".config", out / "helper.config")
run(make + [f"-j{min(os.cpu_count() or 2, 8)}", "Image"])
image = build / "arch/arm64/boot/Image"
with image.open("rb") as f: header = f.read(64)
if header[56:60] != b"ARM\x64" or int.from_bytes(header[16:24],"little") > image.stat().st_size:
    raise RuntimeError("invalid ARM64 Image header/size")
symbols = subprocess.check_output(["llvm-nm-14", "--defined-only", str(build / "vmlinux")], text=True)
for forbidden in ["ufshcd_init", "mmc_init", "mtdcore_init", "efi_set_variable", "ramoops_probe", "qcom_qfprom_probe"]:
    if re.search(r"\b"+forbidden+r"$", symbols, re.M): raise RuntimeError("forbidden linked path: "+forbidden)
if not re.search(r"\bdwc3_msm_probe$", symbols, re.M): raise RuntimeError("MSM USB driver not linked")
shutil.copy2(image, out / "Image")
run(["llvm-readelf-14", "-h", "-l", out / "ziyi-ram-init"], stdout=(out / "init-elf.txt").open("w"))
report = {"result":"HOST_BUILD_PASS_NOT_PHONE_TESTED", "gpl_source": PIN,
 "scope":"receive-only USB; no kexec syscall or phone image packaging", "config_y": required_y,
 "config_n":required_n, "source_hashes":{}, "artifact_hashes":{}}
for f in sorted(here.iterdir()):
    if f.is_file(): report["source_hashes"][f.name]=hashlib.sha256(f.read_bytes()).hexdigest()
for name in ["Image","ziyi-ram-init","helper.config","protocol-tests.json","initramfs.list"]:
    f=out/name
    with f.open("rb") as h: digest=hashlib.file_digest(h,"sha256").hexdigest()
    report["artifact_hashes"][name]={"bytes":f.stat().st_size,"sha256":digest}
(out / "build-audit.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report))
