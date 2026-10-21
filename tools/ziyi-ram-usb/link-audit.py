#!/usr/bin/env python3
"""Preserve failure diagnostics before link-vmlinux.sh deletes vmlinux.o."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

args=sys.argv[1:]
out=Path(os.environ['ZIYI_LINK_DIAGNOSTICS'])
final='-o' in args and Path(args[args.index('-o')+1]).name.startswith('.tmp_vmlinux')
extra=['-Map='+str(out/'link-map.txt')] if final else []
code=subprocess.run(['ld.lld-14',*args,*extra]).returncode
obj=Path('vmlinux.o')
if code and final and obj.exists():
    undefined=subprocess.check_output(['llvm-nm-14','--undefined-only',str(obj)],text=True)
    print('LINK_DIAGNOSTIC undefined and weak references:\n'+undefined,flush=True)
    (out/'link-undefined.txt').write_text(undefined)
    names={line.split()[-1] for line in undefined.splitlines() if line.split()}
    with (out/'link-relocations.txt').open('w') as f:
        proc=subprocess.Popen(['llvm-readelf-14','--relocations','--wide',str(obj)],stdout=subprocess.PIPE,text=True)
        for line in proc.stdout:
            if names.intersection(line.split()):f.write(line)
        assert proc.wait()==0
    shutil.copy2('arch/arm64/kernel/vmlinux.lds',out/'link-vmlinux.lds')
sys.exit(code)
