/* RAM HFS probe. No storage devices, mounts, fork/exec or Apple binaries.
 * Open two sentinel paths (one through a symlink), verify bytes, then heartbeat.
 * Use the donor's static Mach-O wrapper with no relocations or writable data.
 */
    .text
    .globl _start
    .p2align 2
_start:
    mov x9, sp
    and x9, x9, #~15
    mov sp, x9
    adr x0, console
    mov x1, #2
    mov x2, #0
    mov x16, #5
    svc #0x80
    b.cs hang
    mov x19, x0
    adr x1, hello
    adr x2, hello_end
    sub x2, x2, x1
    bl output
    mov x16, #20
    svc #0x80
    cmp x0, #1
    b.ne fail
    adr x0, sentinel
    bl check_file
    mov x22, x0
    adr x0, alias
    bl check_file
    and x22, x22, x0
    cbz x22, fail
    adr x1, passed
    adr x2, passed_end
    sub x2, x2, x1
    bl output
    b tick
fail:
    adr x1, failed
    adr x2, failed_end
    sub x2, x2, x1
    bl output
tick:
    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    adr x4, delay
    mov x16, #93
    svc #0x80
    adr x1, alive
    adr x2, alive_end
    sub x2, x2, x1
    bl output
    b tick
output:
    mov x0, x19
    mov x16, #4
    svc #0x80
    ret
/* x0=path; x0=1 only if open/read/close succeed and full contents match.
 * Clobbers x20/x21; no nested calls. SP remains 16-byte aligned.
 */
check_file:
    sub sp, sp, #32
    mov x1, #0
    mov x2, #0
    mov x16, #5
    svc #0x80
    b.cs open_error
    mov x20, x0
    mov x21, #0
    mov x1, sp
    mov x2, #32
    mov x16, #3
    svc #0x80
    b.cs close_file
    adr x10, token
    adr x15, token_end
    sub x15, x15, x10
    cmp x0, x15
    b.ne close_file
    mov x11, sp
    mov x12, #0
compare:
    ldrb w13, [x10, x12]
    ldrb w14, [x11, x12]
    cmp w13, w14
    b.ne close_file
    add x12, x12, #1
    cmp x12, x15
    b.ne compare
    mov x21, #1
close_file:
    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cc close_ok
    mov x21, #0
close_ok:
    mov x0, x21
    add sp, sp, #32
    ret
open_error:
    mov x0, #0
    add sp, sp, #32
    ret
hang:
    b hang
    .p2align 3
delay: .quad 5, 0
console: .asciz "/dev/console"
sentinel: .asciz "/etc/ziyi-root-check"
alias: .asciz "/var/ziyi/volume-check"
token: .ascii "ZIYI-HFS-RAMROOT-v1\n"
token_end:
hello: .ascii "\n*** ZIYI HFS probe: own diagnostic init ***\n"
hello_end:
passed: .ascii "ZIYI HFS proof PASS: PID1, two file reads, symlink lookup\n"
passed_end:
failed: .ascii "ZIYI HFS proof FAIL: PID or sentinel I/O mismatch\n"
failed_end:
alive: .ascii "ZIYI HFS init: still alive\n"
alive_end:
