.syntax unified
.cpu arm7tdmi
.arm
/* R14B VBlank-clocked audio interrupt entry.
 * The BIOS dispatcher enters here in ARM/IRQ mode with IRQs masked, r0 =
 * 0x04000000, and {r0-r3,r12,lr} already pushed on SP_irq (set by _start to
 * __irq_stack_top, a dedicated IWRAM reserve checked by the linker script).
 * IRQs are never re-enabled inside the handler (no nesting). */
.section .iwram.text.ps4_audio_irq,"ax",%progbits
.align 2
.global ps4_audio_irq_entry
.type ps4_audio_irq_entry,%function
ps4_audio_irq_entry:
    add   r3, r0, #0x200
    ldr   r1, [r3]                @ IE | (IF << 16)
    and   r1, r1, r1, lsr #16     @ pending & enabled
    strh  r1, [r3, #2]            @ acknowledge exactly those sources
    ldr   r2, =0x03007FF8         @ BIOS IntrWait flags (kept coherent)
    ldrh  r3, [r2]
    orr   r3, r3, r1
    strh  r3, [r2]
    mov   r0, r1
    stmfd sp!, {r4, lr}           @ 8-byte aligned frame for AAPCS C code
    bl    ps4_audio_irq_service
    ldmfd sp!, {r4, lr}
    bx    lr
.pool
.size ps4_audio_irq_entry, .-ps4_audio_irq_entry

.section .text.ps4_irq_cpu_enable,"ax",%progbits
.align 2
.global ps4_irq_cpu_enable
.type ps4_irq_cpu_enable,%function
ps4_irq_cpu_enable:
    mrs   r0, cpsr
    bic   r0, r0, #0x80           @ CPSR.I = 0; FIQ stays masked
    msr   cpsr_c, r0
    bx    lr
.size ps4_irq_cpu_enable, .-ps4_irq_cpu_enable
