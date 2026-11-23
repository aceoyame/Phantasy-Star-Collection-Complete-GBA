.syntax unified
.cpu arm7tdmi
.arm
.equ DISPCNT,0x04000000
.section .probe,"ax",%progbits
.global actor_shift_probe
actor_shift_probe:
 push {r0,r1}
 tst r5,#0x200
 beq 1f
 mov r0,#DISPCNT
 ldrh r0,[r0,#6]
 cmp r0,#160
 bhs 1f
 mov r3,#0x208
 orr r3,r3,#DISPCNT
 ldrh r2,[r3]
 mov r0,#0
 strh r0,[r3]
 ldr r1,[r4]
 str r0,[r4]
 tst r1,#2
 beq 2f
 push {r2,r3}
 ldr r0,=0x088664e4
 mov lr,pc
 bx r0
 pop {r2,r3}
2: strh r2,[r3]
 mov r5,#0
 mov r0,#1
 str r0,[r4]
1: pop {r0,r1}
 add r0,r0,#16
 add r1,r1,#8
 ldr lr,=0x08cd3858
 ldr pc,=0x08cb9374
 .ltorg
