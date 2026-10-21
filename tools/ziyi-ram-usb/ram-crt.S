/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Own CRT: kernel initialized BSS and aligned stack; main takes no arguments.
 */
.section .text
.global _start
.type _start, %function
_start:
    mov x29, #0
    mov x30, #0
    bl main
    mov x8, #94                 /* exit_group, only if main returns */
    svc #0
1:  b 1b
.size _start, .-_start
.section .note.GNU-stack,"",%progbits
