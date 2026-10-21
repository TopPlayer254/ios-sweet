/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Own pre-libc entry marker. Uses only normal Linux syscalls, never MMIO.
 * Preserve the initial stack and rtld_fini x0 expected by glibc _start.
 */
.section .text
.global ziyi_linux_entry
.type ziyi_linux_entry, %function
ziyi_linux_entry:
    mov x19, x0
    mov x0, #-100
    adrp x1, kmsg_path
    add x1, x1, :lo12:kmsg_path
    mov x2, #1
    mov x3, #0
    mov x8, #56                 /* openat: /dev/kmsg is a builtin c1:11 node */
    svc #0
    tbnz x0, #63, 1f
    mov x20, x0
    adrp x1, raw_marker
    add x1, x1, :lo12:raw_marker
    mov x2, #raw_marker_bytes
    mov x8, #64                 /* write */
    svc #0
    mov x0, x20
    mov x8, #57                 /* close */
    svc #0
1:
    mov x0, x19
    b _start
.size ziyi_linux_entry, .-ziyi_linux_entry
.section .rodata
kmsg_path: .asciz "/dev/kmsg"
raw_marker: .ascii "<6>ZIYI RAM USB RAW EL0 ENTRY before libc\n"
.equ raw_marker_bytes, .-raw_marker
.section .note.GNU-stack,"",%progbits
