    .arm
    .section .text
    .global _start
_start:
    b hang          @ 0x00 reset
    b hang          @ 0x04 undef
    b swi_handler   @ 0x08 swi
    b hang          @ 0x0C pabt
    b hang          @ 0x10 dabt
    b hang          @ 0x14
    b irq_handler   @ 0x18 irq
    b hang          @ 0x1C fiq
hang:
    b hang
irq_handler:
    stmfd sp!,{r0-r3,r12,lr}
    mov r0,#0x04000000
    add lr,pc,#0
    ldr pc,[r0,#-4]
    ldmfd sp!,{r0-r3,r12,lr}
    subs pc,lr,#4
swi_handler:
    stmfd sp!,{r11,r12,lr}
    mrs r11,spsr
    tst r11,#0x20
    ldrbne r12,[lr,#-2]
    ldreq r12,[lr,#-4]
    moveq r12,r12,lsr #16
    andeq r12,r12,#0xFF
    stmfd sp!,{r11}
    and r11,r11,#0x80
    orr r11,r11,#0x1F
    msr cpsr_fc,r11
    cmp r12,#2
    beq do_halt
    cmp r12,#4
    beq do_intrwait
    cmp r12,#5
    beq do_vblankwait
    .word 0xE7FFDEFE        @ HLE trap: emulator services SWI r12
swi_ret:
    mov r12,#0xD3
    msr cpsr_fc,r12
    ldmfd sp!,{r11}
    msr spsr_fc,r11
    ldmfd sp!,{r11,r12,lr}
    movs pc,lr
do_halt:
    mov r12,#0x04000000
    mov r11,#0
    strb r11,[r12,#0x301]
    b swi_ret
do_vblankwait:
    mov r0,#1
    mov r1,#1
do_intrwait:
    stmfd sp!,{r2,r3}
    mov r12,#0x04000000
    cmp r0,#0
    beq 1f
    ldrh r2,[r12,#-8]
    bic r2,r2,r1
    strh r2,[r12,#-8]
1:  mov r2,#1
    add r12,r12,#0x200
    strh r2,[r12,#8]
    sub r12,r12,#0x200
    mrs r3,cpsr
    bic r2,r3,#0x80
    msr cpsr_c,r2
2:  mov r2,#0
    strb r2,[r12,#0x301]
    ldrh r2,[r12,#-8]
    ands r2,r2,r1
    beq 2b
    ldrh r2,[r12,#-8]
    bic r2,r2,r1
    strh r2,[r12,#-8]
    msr cpsr_c,r3
    ldmfd sp!,{r2,r3}
    b swi_ret
