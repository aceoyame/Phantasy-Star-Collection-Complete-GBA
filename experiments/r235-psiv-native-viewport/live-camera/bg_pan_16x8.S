.syntax unified
.cpu arm7tdmi
.arm
.section .probe,"ax",%progbits
.global bg_scroll_shift_probe
bg_scroll_shift_probe:
 @ Replacement for 08C4D198-08C4D1B8, 161 scanlines x 4 packed HOFS/VOFS values.
 @ Both X and Y must shift together with field OBJ camera; never resample.
 push {r1,r2,r4,r5}
 add r1,r1,#16
 add r1,r1,#524288
 add r2,r2,#16
 add r2,r2,#524288
 mov r3,r1
 mov r4,r2
 mov r5,#161
1: stmia r0!,{r1,r2,r3,r4}
 subs r5,r5,#1
 bne 1b
 sub r3,r3,#16
 sub r3,r3,#524288
 pop {r1,r2,r4,r5}
 bx lr
