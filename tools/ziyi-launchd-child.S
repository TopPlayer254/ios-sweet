/* Own static child, intended to be spawned by REAL Apple launchd.
 * A separately compiled ZIYI_BOOT_FIXTURE variant only prints and exits 0.
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
#ifdef ZIYI_BOOT_FIXTURE
    b fixture
#endif
    mov x16, #20
    svc #0x80
    cmp x0, #1
    b.ls failed
    mov x16, #39
    svc #0x80
    cmp x0, #1
    b.ne failed
    adr x1, passed
    adr x2, passed_end
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
fixture:
    adr x1, fixture_text
    adr x2, fixture_end
    sub x2, x2, x1
    bl output
    mov x0, #0
    mov x16, #1
    svc #0x80
    b hang
failed:
    adr x1, failure
    adr x2, failure_end
    sub x2, x2, x1
    bl output
hang:
    b hang
output:
    mov x0, x19
    mov x16, #4
    svc #0x80
    ret
    .p2align 3
delay: .quad 5, 0
console: .asciz "/dev/console"
passed: .ascii "ZIYI APPLE-LAUNCHD CHILD PASS: pid>1, ppid=1\n"
passed_end:
alive: .ascii "ZIYI launchd child: still alive\n"
alive_end:
fixture_text: .ascii "ZIYI OWN BOOT-TASK FIXTURE: RAM-only no-op, not Apple fsck/mount\n"
fixture_end:
failure: .ascii "ZIYI APPLE-LAUNCHD CHILD FAIL: wrong pid/parent\n"
failure_end:
