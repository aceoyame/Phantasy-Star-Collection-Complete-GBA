.syntax unified
.cpu arm7tdmi
.arm
.section .start,"ax"
.global psc8_start
psc8_start:
    mov r0,#0x12
    orr r0,r0,#0xc0
    msr cpsr_c,r0
    ldr sp,=0x03007fa0
    mov r0,#0x1f
    orr r0,r0,#0xc0
    msr cpsr_c,r0
    ldr sp,=0x03007e00
    ldr r0,=0x04000208
    mov r1,#0
    strh r1,[r0]
    ldr r0,=0x040000b0
    str r1,[r0,#8]
    str r1,[r0,#20]
    str r1,[r0,#32]
    str r1,[r0,#44]
    ldr r0,=__bss_start
    ldr r2,=__bss_end
1:  cmp r0,r2
    strlo r1,[r0],#4
    blo 1b
    ldr r0,=__hot_start
    ldr r1,=__hot_load
    ldr r2,=__hot_end
2:  cmp r0,r2
    ldrlo r3,[r1],#4
    strlo r3,[r0],#4
    blo 2b
    ldr r0,=__data_start
    ldr r1,=__data_load
    ldr r2,=__data_end
3:  cmp r0,r2
    ldrlo r3,[r1],#4
    strlo r3,[r0],#4
    blo 3b
    ldr r0,=psc8_main
    bx r0
