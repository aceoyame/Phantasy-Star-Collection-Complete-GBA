.syntax unified
.cpu arm7tdmi
.thumb
.section .text,"ax",%progbits
.global psc5_hook
.thumb_func
psc5_hook:
 mov r0,sp
 adds r0,#10
 ldrb r2,[r0]
 push {r5,r6,lr}
 ldr r4,[r1]
 ldr r0,=0x0203fff0
 ldr r5,[r0]
 ldr r6,=0x34435350
 cmp r5,r6
 beq 1f
 str r6,[r0]
 movs r5,#0
 strb r5,[r0,#4]
 movs r5,#8
 strb r5,[r0,#5]
1:
 ldrb r5,[r0,#4]
 movs r6,#0
 cmp r4,#2
 blo 2f
 movs r6,r4
 subs r6,#2
2:
 cmp r5,r6
 beq 5f
 mov r1,r9
 cmp r1,#8
 beq 3f
 movs r3,#8
 negs r3,r3
 cmp r1,r3
 bne 4f
3:
 strb r1,[r0,#5]
4:
 cmp r2,#0x80
 beq 6f
 movs r4,#3
 b 8f
6:
 strb r6,[r0,#4]
 push {r0,r4}
 movs r0,#3
 cmp r6,#0
 beq 9f
 cmp r6,#1
 beq 10f
 ldr r1,=0x08008000
 ldr r2,=0x08009400
 b 11f
9:
 ldr r1,=0x08016580
 ldr r2,=0x08017776
 b 11f
10:
 ldr r1,=0x08042000
 ldr r2,=0x08043600
11:
 ldr r3,=0x08043b00
 bl loader
 pop {r0,r4}
 ldrb r1,[r0,#5]
 cmp r1,#8
 beq 12f
 movs r1,#8
 negs r1,r1
12:
 mov r9,r1
5:
 cmp r4,#2
 blo 8f
 movs r4,#2
8:
 movs r7,#0x80
 movs r3,#0
 pop {r5,r6,pc}
loader:
 ldr r5,=0x0800095d
 bx r5
.ltorg
