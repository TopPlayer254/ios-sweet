#!/usr/bin/env python3
"""Guarded PID1-only printk instrumentation; no changed exec decisions."""
import hashlib
import subprocess

PIN = '24856283d18328b8473cd10991c15749b88d7bf9'


def apply(root, source_fixes):
    def stage(label):
        assert all(c.isalnum() or c in ' _./-' for c in label)
        return '\tif (current->pid == 1) pr_info("ZIYI EXEC ' + label + '\\n");\n'

    def function(text, signature, points):
        assert text.count(signature) == 1
        start = text.index(signature)
        end = text.index('\n}\n', start) + 3
        body = text[start:end]
        for anchor, label in points:
            assert body.count(anchor) == 1, (signature, label)
            body = body.replace(anchor, stage(label + ' before') + anchor + stage(label + ' after'), 1)
        return text[:start] + body + text[end:]

    changes = {
        'fs/exec.c': [
            ('int kernel_execve(', [
                ('\tfilename = getname_kernel(kernel_filename);\n', 'kernel getname'),
                ('\tbprm = alloc_bprm(fd, filename);\n', 'kernel alloc_bprm'),
                ('\tretval = copy_strings_kernel(bprm->argc, argv, bprm);\n', 'kernel copy argv'),
                ('\tretval = bprm_execve(bprm, fd, filename, 0);\n', 'kernel bprm_execve'),
                ('\tfree_bprm(bprm);\n', 'kernel free_bprm')]),
            ('static int bprm_execve(', [
                ('\tio_uring_task_cancel();\n', 'bprm io_uring cancel'),
                ('\tretval = unshare_files(&displaced);\n', 'bprm unshare files'),
                ('\tretval = prepare_bprm_creds(bprm);\n', 'bprm prepare creds'),
                ('\tfile = do_open_execat(fd, filename, flags);\n', 'bprm open exec'),
                ('\tretval = security_bprm_creds_for_exec(bprm);\n', 'bprm security creds'),
                ('\tretval = exec_binprm(bprm);\n', 'bprm exec_binprm')]),
            ('int begin_new_exec(', [
                ('\tretval = bprm_creds_from_file(bprm);\n', 'begin file creds'),
                ('\tretval = de_thread(me);\n', 'begin de_thread'),
                ('\tretval = exec_mmap(bprm->mm);\n', 'begin exec_mmap'),
                ('\tflush_thread();\n', 'begin flush_thread'),
                ('\tcommit_creds(bprm->cred);\n', 'begin commit_creds')])],
        'fs/binfmt_elf.c': [
            ('static int load_elf_binary(struct linux_binprm *bprm)\n{', [
                ('\tretval = -ENOEXEC;\n\t/* First of all, some simple consistency checks */\n', 'elf entry'),
                ('\tretval = begin_new_exec(bprm);\n', 'elf begin_new_exec'),
                ('\tretval = setup_arg_pages(bprm, randomize_stack_top(STACK_TOP),\n\t\t\t\t executable_stack);\n', 'elf setup stack'),
                ('\tretval = set_brk(elf_bss, elf_brk, bss_prot);\n', 'elf mapped segments set_brk'),
                ('\tretval = arch_setup_additional_pages(bprm, !!interpreter);\n', 'elf additional pages'),
                ('\tretval = create_elf_tables(bprm, elf_ex,\n\t\t\t  load_addr, interp_load_addr, e_entry);\n', 'elf aux tables'),
                ('\tfinalize_exec(bprm);\n', 'elf finalize'),
                ('\tstart_thread(regs, elf_entry, bprm->p);\n', 'elf start_thread')])]
    }
    for name, functions in changes.items():
        path = root / name
        original = path.read_bytes()
        assert original == subprocess.check_output(['git', '-C', str(root), 'show', PIN + ':' + name])
        text = original.decode()
        for signature, points in functions:
            text = function(text, signature, points)
        assert path.read_bytes() == original
        path.write_bytes(text.encode())
        source_fixes[name] = {'before': hashlib.sha256(original).hexdigest(),
                              'after': hashlib.sha256(path.read_bytes()).hexdigest(),
                              'scope': 'PID1 printk before/after only; no exec control-flow changes'}
