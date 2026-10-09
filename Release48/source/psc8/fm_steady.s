@ PSC8 FM steady-state channel loop (key on, both envelopes in sustain).
@ void fm_steady_asm(FmSteadyA *s, int16_t *acc, unsigned n)   n > 0
@ struct: 0 pm, 4 pc, 8 inc_m, 12 inc_c, 16 prev, 20 modlev, 24 carlev, 28 fb, 32 modtab, 36 cartab
@ Same arithmetic as fastopll_calc (bit-exact; host-tested through the C mirror in fast_opll_block.c).
.syntax unified
.arm
.section .hot.fmsteady,"ax",%progbits
.align 2
.global fm_steady_asm
fm_steady_asm:
    push {r4-r11, lr}
    push {r0}
    mov r12, r0
    mov r0, r1
    mov r1, r2
    ldmia r12, {r2-r11}
1:
    add r2, r2, r4
    mul r12, r6, r9
    mov lr, r2, lsr #24
    add lr, lr, r12, asr #6
    and lr, lr, #255
    ldrsb lr, [r10, lr]
    mul r12, lr, r7
    add r3, r3, r5
    mov r6, r12, asr #14
    mov lr, r3, lsr #24
    add lr, lr, r6, asr #2
    and lr, lr, #255
    ldrsb lr, [r11, lr]
    ldrsh r12, [r0]
    mul lr, r8, lr
    add r12, r12, lr, asr #12
    strh r12, [r0], #2
    subs r1, r1, #1
    bne 1b
    pop {r12}
    str r2, [r12, #0]
    str r3, [r12, #4]
    str r6, [r12, #16]
    pop {r4-r11, lr}
    bx lr

@ PSC8 FM ramp loop: like fm_steady_asm, but both envelopes step every sample (attack / fast decay / release run) and both
@ levels are recomputed from them.  X = (modulator envelope << 16) | carrier envelope, D = per-sample step in the same packing.
@ void fm_ramp_asm(FmRampA *s, int16_t *acc, unsigned n)   n > 0
@ struct: 0 pm, 4 pc, 8 inc_m, 12 inc_c, 16 prev, 20 X, 24 D, 28 fb, 32 modtab, 36 cartab, 40 tl, 44 volf
@ Per sample (bit-exact with fastopll_calc):  m = ((s * (X>>24)) * tl) >> 14,  out = ((c * ((X>>8)&255)) * volf) >> 12.
@ tl and volf live on the stack (no registers left); pm, pc and prev are written back.
.global fm_ramp_asm
fm_ramp_asm:
    push {r4-r11, lr}
    push {r0}
    ldr r12, [r0, #40]
    ldr lr, [r0, #44]
    push {r12, lr}
    mov r12, r0
    mov r0, r1
    mov r1, r2
    ldmia r12, {r2-r11}
1:
    add r7, r7, r8
    add r2, r2, r4
    mul r12, r6, r9
    mov lr, r2, lsr #24
    add lr, lr, r12, asr #6
    and lr, lr, #255
    ldrsb lr, [r10, lr]
    mov r12, r7, lsr #24
    mul r12, lr, r12
    ldr lr, [sp, #0]
    mul r12, lr, r12
    mov r6, r12, asr #14
    add r3, r3, r5
    mov lr, r3, lsr #24
    add lr, lr, r6, asr #2
    and lr, lr, #255
    ldrsb lr, [r11, lr]
    mov r12, r7, lsl #16
    mov r12, r12, lsr #24
    mul r12, lr, r12
    ldr lr, [sp, #4]
    mul r12, lr, r12
    ldrsh lr, [r0]
    add lr, lr, r12, asr #12
    strh lr, [r0], #2
    subs r1, r1, #1
    bne 1b
    add sp, sp, #8
    pop {r12}
    str r2, [r12, #0]
    str r3, [r12, #4]
    str r6, [r12, #16]
    pop {r4-r11, lr}
    bx lr

@ PSC8 PSG tone channel, block form (bit-exact with psg_tone integration).
@ void psg_tone_asm(PsgToneA *t, int16_t *L, int16_t *R, unsigned n)   n > 0, L/R always valid
@ struct: 0 p, 4 limit, 8 half, 12 tri, 16 amp, 20 fr   (p, tri written back)
@ Fast path: while the phase stays on one ramp of the triangle the integrated step is exactly +-step,
@ so the sample is exactly +amp (rising) or -amp (falling); only samples that cross a corner use the
@ full integration (/256 and /50468 as in psg.h).
.section .hot.psgtone,"ax",%progbits
.align 2
.global psg_tone_asm
psg_tone_asm:
    push {r4-r11, lr}
    push {r0}
    mov r12, r0
    mov r0, r1
    mov r1, r2
    mov r2, r3
    ldr r3, [r12, #0]
    ldr r4, [r12, #4]
    ldr r5, [r12, #8]
    ldr r6, [r12, #12]
    ldr r7, [r12, #16]
    ldr r8, [r12, #20]
    ldr r9, =12919920
1:  add r8, r8, #60
    add r11, r3, r9
    add r11, r11, r8, lsr #8
    and r8, r8, #255
    cmp r3, r5
    bhi 3f
    cmp r11, r5
    bhi 5f
    mov r3, r11
    mov r6, r11
    mov r12, r7
    b 4f
3:  cmp r11, r4
    bhs 5f
    mov r3, r11
    sub r6, r4, r11
    rsb r12, r7, #0
4:  ldrsh lr, [r0]
    add lr, lr, r12
    strh lr, [r0], #2
    ldrsh lr, [r1]
    add lr, lr, r12
    strh lr, [r1], #2
    subs r2, r2, #1
    bne 1b
    pop {r12}
    str r3, [r12, #0]
    str r6, [r12, #12]
    pop {r4-r11, lr}
    bx lr
5:  mov r3, r11
6:  cmp r3, r4
    subhs r3, r3, r4
    bhs 6b
    cmp r3, r5
    movls r11, r3
    subhi r11, r4, r3
    sub r12, r11, r6
    mov r6, r11
    mov lr, r12, asr #31
    add r12, r12, lr, lsr #24
    mov r12, r12, asr #8
    mul lr, r12, r7
    ldr r11, =0x531bb22b
    smull r12, r11, lr, r11
    mov r12, r11, asr #14
    add r12, r12, r11, lsr #31
    b 4b
.ltorg
