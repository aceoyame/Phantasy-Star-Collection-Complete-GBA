.syntax unified
.cpu arm7tdmi
.arm
.section .hot.z80a,"ax",%progbits
.align 2
.global z80a_run
.global z80a_ctx_size
z80a_run:
    push {r4-r11, lr}
    ldr r4, =z80a_ctx
    str r0, [r4, #348]
    mov r6, r0
    ldr r7, [r4, #356]
    ldr r8, [r4, #360]
    ldr r9, [r4, #372]
    ldr r10, [r4, #364]
    ldr r11, [r4, #368]
    ldr r5, [r4, #376]
    ldr r12, [r4, #380]
    ldr r3, [r4, #328]
    add lr, r4, #512
    ldrb r0, [r4, #341]
    cmp r0, #0
    bne .Lentry_halted
    ldrb r0, [r4, #342]
    ldrb r1, [r4, #339]
    ands r0, r0, r1
    beq .Lgo
    str r6, [r4, #332]
    mov r6, #1
    mov r0, #1
    strb r0, [r4, #345]
.Lgo:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
.Lentry_halted:
    ldrb r0, [r4, #342]
    ldrb r1, [r4, #339]
    ands r0, r0, r1
    bne .Lhalt_take
    add r0, r6, #3
    bic r0, r0, #3
    sub r6, r6, r0
    b .Lstore
.Lhalt_take:
    sub r6, r6, #4
    b .Ltake_irq
z80a_exit:
    ldrb r0, [r4, #345]
    cmp r0, #0
    bne .Lirqchk
.Lstore:
    str r7, [r4, #356]
    str r8, [r4, #360]
    str r9, [r4, #372]
    str r10, [r4, #364]
    str r11, [r4, #368]
    mov r5, r5, lsl #16
    mov r5, r5, lsr #16
    str r5, [r4, #376]
    str r12, [r4, #380]
    mov r0, r6
    pop {r4-r11, lr}
    bx lr
.Lirqchk:
    mov r0, #0
    strb r0, [r4, #345]
    ldr r0, [r4, #332]
    sub r1, r6, #1
    add r6, r0, r1
    ldrb r0, [r4, #342]
    ldrb r1, [r4, #339]
    ands r0, r0, r1
    beq .Lresume
.Ltake_irq:
    mov r0, #0
    strb r0, [r4, #342]
    strb r0, [r4, #339]
    strb r0, [r4, #340]
    ldrb r0, [r4, #341]
    cmp r0, #0
    movne r0, #0
    strbne r0, [r4, #341]
    addne r5, r5, #1
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    ldrb r0, [r4, #338]
    cmp r0, #2
    beq .Lim2
    mov r5, #0x38
    cmp r0, #1
    subeq r6, r6, #13
    subne r6, r6, #11
    b .Lresume
.Lim2:
    ldrb r0, [r4, #336]
    ldrb r1, [r4, #343]
    orr r0, r1, r0, lsl #8
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r5, [r2, r0]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    orr r5, r5, r1, lsl #8
    sub r6, r6, #19
.Lresume:
    cmp r6, #0
    ble .Lstore
z80a_dispatch:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
z80a_arm_irq:
    ldrb r0, [r4, #345]
    cmp r0, #0
    bne 1f
    ldrb r0, [r4, #342]
    cmp r0, #0
    beq 1f
    cmp r6, #0
    ble 1f
    str r6, [r4, #332]
    mov r6, #1
    mov r0, #1
    strb r0, [r4, #345]
    b .Lresume
1:
    cmp r6, #0
    bgt z80a_dispatch
    b z80a_exit
z80a_wr8:
    cmp r0, #0xC000
    blo .Lwr_low
    mov lr, r0, lsr #10
    ldr lr, [r4, lr, lsl #2]
    strb r1, [lr, r0]
    add lr, r0, #4
    tst lr, #0x10000
    add lr, r4, #512
    moveq pc, r2
    b .Lwr_call
.Lwr_low:
    cmp r0, #0x8000
    movlo pc, r2
    ldrb lr, [r4, #344]
    cmp lr, #0
    add lr, r4, #512
    moveq pc, r2
.Lwr_call:
    str r2, [r4, #324]
    str r12, [r4, #296]
    str r3, [r4, #396]
    ldr r2, =z80a_wr_slow
    mov lr, pc
    bx r2
    ldr r3, [r4, #396]
    ldr r12, [r4, #296]
    add lr, r4, #512
    ldr r2, [r4, #324]
    mov pc, r2
z80a_in_call:
    and r1, r0, #0xC1
    cmp r1, #0x40
    bne .Lin_tab
    ldr r1, [r4, #348]
    sub r1, r1, r6
    ldr r2, [r4, #436]
    add r1, r1, r2
    ldr r0, =301401214
    umull r2, r0, r1, r0
    mov r0, r0, lsr #4
    ldr r1, [r4, #440]
    add r0, r0, r1
    cmp r0, #0xDA
    subhi r0, r0, #6
    and r0, r0, #255
    b .Lin_fast
.Lin_tab:
    add r1, r4, #0x600
    ldrb r2, [r1, r0]
    cmp r2, #0
    beq .Lin_slow
    add r1, r4, #0x700
    ldrb r0, [r1, r0]
    ldr r1, [r4, #448]
    add r1, r1, #1
    str r1, [r4, #448]
.Lin_fast:
    mov r2, lr
    add lr, r4, #512
    mov pc, r2
z80a_in_a:
    and r1, r0, #0xC1
    cmp r1, #0x40
    bne .Lin_tab
    b z80a_vwait
z80a_out_call:
    and r2, r0, #0xC1
    cmp r2, #0x81
    beq .Lout_ctl
    cmp r2, #0x80
    bne .Lout_slow
    ldrb r2, [r4, #408]
    cmp r2, #3
    beq .Lout_slow
    str lr, [r4, #324]
    mov r2, #0
    strb r2, [r4, #409]
    ldr r0, [r4, #404]
    ldr r2, [r4, #412]
    strb r1, [r2, r0]
    strb r1, [r4, #410]
    ldr r2, [r4, #416]
    add r2, r2, r0, lsr #8
    ldrb r1, [r2]
    mov lr, r0, lsr #5
    and lr, lr, #7
    mov r3, #1
    orr r1, r1, r3, lsl lr
    strb r1, [r2]
    ldr r2, [r4, #424]
    sub r2, r0, r2
    mov r2, r2, lsl #18
    mov r2, r2, lsr #18
    cmp r2, #2048
    bhs 1f
    ldr r1, [r4, #420]
    add r1, r1, r2, lsr #4
    mov r2, r2, lsr #1
    and r2, r2, #7
    ldrb lr, [r1]
    orr lr, lr, r3, lsl r2
    strb lr, [r1]
1:
    ldr r2, [r4, #428]
    sub r2, r0, r2
    mov r2, r2, lsl #18
    mov r2, r2, lsr #18
    cmp r2, #64
    blo 2f
    sub r2, r2, #128
    cmp r2, #128
    bhs 3f
2:
    strb r3, [r4, #411]
3:
    add r0, r0, #1
    mov r0, r0, lsl #18
    mov r0, r0, lsr #18
    str r0, [r4, #404]
    ldr r3, [r4, #328]
    ldr r2, [r4, #324]
    add lr, r4, #512
    mov pc, r2
.Lout_ctl:
    ldrb r2, [r4, #409]
    cmp r2, #0
    bne 1f
    strb r1, [r4, #432]
    mov r2, #1
    strb r2, [r4, #409]
    mov r2, lr
    add lr, r4, #512
    mov pc, r2
1:
    mov r2, r1, lsr #6
    cmp r2, #2
    beq .Lout_slow
    mov r0, #0
    strb r0, [r4, #409]
    strb r2, [r4, #408]
    and r1, r1, #63
    ldrb r0, [r4, #432]
    orr r0, r0, r1, lsl #8
    cmp r2, #0
    bne 2f
    str lr, [r4, #324]
    ldr r1, [r4, #412]
    ldrb r1, [r1, r0]
    strb r1, [r4, #410]
    add r0, r0, #1
    mov r0, r0, lsl #18
    mov r0, r0, lsr #18
    ldr lr, [r4, #324]
2:
    str r0, [r4, #404]
    mov r2, lr
    add lr, r4, #512
    mov pc, r2
.Lin_slow:
    str lr, [r4, #324]
    str r12, [r4, #296]
    str r3, [r4, #396]
    str r6, [r4, #352]
    str r10, [r4, #364]
    str r11, [r4, #368]
    str r9, [r4, #372]
    ldr r2, =z80a_in_c
    mov lr, pc
    bx r2
    ldr r3, [r4, #396]
    ldr r12, [r4, #296]
    ldr r2, [r4, #324]
    add lr, r4, #512
    ldrb r1, [r4, #444]
    cmp r1, #0
    movne r6, #0
    mov pc, r2
.Lout_slow:
    str lr, [r4, #324]
    str r12, [r4, #296]
    str r3, [r4, #396]
    str r6, [r4, #352]
    str r10, [r4, #364]
    str r11, [r4, #368]
    str r9, [r4, #372]
    ldr r2, =z80a_out_c
    mov lr, pc
    bx r2
    ldr r3, [r4, #396]
    ldr r12, [r4, #296]
    ldr r2, [r4, #324]
    add lr, r4, #512
    ldrb r1, [r4, #444]
    cmp r1, #0
    movne r6, #0
    mov pc, r2
z80a_idle:
    cmp r0, #0xF9000000
    bne .Lz48
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    cmp r2, #0x3A
    movne pc, lr
    add r2, r5, #3
    mov r1, r2, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r2]
    cmp r2, #0xFE
    movne pc, lr
    mov r6, #0
    mov pc, lr
.Lz48:
    cmp r0, #0xFA000000
    bne .Lz49
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    cmp r2, #0x3A
    movne pc, lr
    add r2, r5, #3
    mov r1, r2, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r2]
    cmp r2, #0xB7
    cmpne r2, #0xA7
    movne pc, lr
    mov r6, #0
    mov pc, lr
.Lz49:
    cmp r0, #0xFD000000
    bne .Lz50
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    cmp r2, #0xBE
    movne pc, lr
    mov r6, #0
    mov pc, lr
.Lz50:
    cmp r0, #0xFC000000
    bne .Lz51
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    cmp r2, #0x7E
    movne pc, lr
    add r2, r5, #1
    mov r1, r2, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r2]
    cmp r2, #0xB7
    cmpne r2, #0xA7
    movne pc, lr
    mov r6, #0
    mov pc, lr
.Lz51:
    cmp r0, #0xFB000000
    bne .Lz52
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    cmp r2, #0x7E
    movne pc, lr
    add r2, r5, #1
    mov r1, r2, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r2]
    cmp r2, #0xFE
    movne pc, lr
    mov r6, #0
    mov pc, lr
.Lz52:
    mov pc, lr
.ltorg
z80a_cb:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    str r0, [r4, #388]
    and r1, r0, #7
    cmp r1, #6
    beq .Lcb_mem
    cmp r0, #0x04
    beq .Lcb_rlch
    cmp r0, #0x3F
    beq .Lcb_srla
    ldr r2, =cb_get
    ldr pc, [r2, r1, lsl #2]
.Lcb_srla:
    ldr r0, [r4, #452]
    cmp r0, #0
    beq .Lcb_srla_one
    ldr r1, =0x4C0
    cmp r5, r1
    ldrne r1, =0x4D1
    cmpne r5, r1
    bne .Lcb_srla_one
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    cmp r1, r0
    bne .Lcb_srla_one
    cmp r6, #24
    blt .Lcb_srla_one
    and r1, r7, #4
    mov r7, r7, lsr #3
    ldrb r8, [lr, r7]
    orr r8, r8, r1, lsr #2
    add r5, r5, #4
    subs r6, r6, #24
    bgt z80a_dispatch
    b z80a_exit
.Lcb_srla_one:
    and r1, r7, #1
    mov r7, r7, lsr #1
    ldrb r8, [lr, r7]
    orr r8, r8, r1
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcb_rlch:
    mov r0, r9, lsr #8
    mov r0, r0, lsl #1
    orr r0, r0, r0, lsr #8
    and r0, r0, #255
    ldrb r8, [lr, r0]
    tst r0, #1
    orrne r8, r8, #1
    and r9, r9, #255
    orr r9, r9, r0, lsl #8
    ldr r0, [r4, #452]
    cmp r0, #0
    beq .Lcb_rlch_one
    ldr r1, =0x43B
    cmp r5, r1
    ldrne r1, =0x46E
    cmpne r5, r1
    bne .Lcb_rlch_one
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    cmp r1, r0
    bne .Lcb_rlch_one
    ldr r1, =0xFFFC
    cmp r11, r1
    bhs .Lcb_rlch_one
    cmp r6, #70
    blt .Lcb_rlch_one
.Lgaiden_body:
    sub r6, r6, #8
    tst r8, #1
    subne r6, r6, #12
    bne .Lgaiden_put
    sub r6, r6, #36
    ldr r0, [r4, #288]
    mov r1, r0, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r7, [r1, r0]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    str r0, [r4, #288]
.Lgaiden_put:
    mov r0, r11
    mov r1, r7
    mov r2, pc
    b z80a_wr8
    add r11, r11, #1
    mov r11, r11, lsl #16
    mov r11, r11, lsr #16
    sub r6, r6, #13
    sub r0, r10, #256
    and r0, r0, #0xFF00
    and r10, r10, #255
    orr r10, r10, r0
    ldr r1, =0x43B
    cmp r5, r1
    ldreq r5, =0x438
    ldrne r5, =0x466
    cmp r0, #0
    subne r6, r6, #13
    bne .Lgaiden_repeat
    ldr r1, =0x438
    cmp r5, r1
    addeq r5, r5, #14
    addne r5, r5, #19
    sub r6, r6, #8
.Lgaiden_done:
    cmp r6, #0
    bgt z80a_dispatch
    b z80a_exit
.Lgaiden_repeat:
    ldr r1, =0xFFFC
    cmp r11, r1
    bhs .Lgaiden_done
    ldr r0, [r4, #452]
    ldr r1, [r4, #4]
    cmp r0, r1
    bne .Lgaiden_done
    ldr r1, =0x438
    cmp r5, r1
    bne .Lgaiden_repeat_second
    cmp r6, #74
    blt .Lgaiden_done
    and r7, r9, #255
    sub r6, r6, #4
    add r5, r5, #3
    b .Lgaiden_rotate
.Lgaiden_repeat_second:
    cmp r6, #103
    blt .Lgaiden_done
    ldr r0, [r4, #292]
    mov r1, r0, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r7, [r1, r0]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    str r0, [r4, #292]
    and r0, r9, #255
    eor r7, r7, r0
    sub r6, r6, #33
    add r5, r5, #8
.Lgaiden_rotate:
    mov r0, r9, lsr #8
    mov r0, r0, lsl #1
    orr r0, r0, r0, lsr #8
    and r0, r0, #255
    ldrb r8, [lr, r0]
    tst r0, #1
    orrne r8, r8, #1
    and r9, r9, #255
    orr r9, r9, r0, lsl #8
    b .Lgaiden_body
.Lcb_rlch_one:
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcb_mem:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
.Lcb_op:
    ldr r1, [r4, #388]
    mov r1, r1, lsr #3
    ldr r2, =cb_ops
    ldr pc, [r2, r1, lsl #2]
.Lcb_put:
    ldr r1, [r4, #388]
    and r2, r1, #7
    ldr r1, =cb_put
    ldr pc, [r1, r2, lsl #2]
.Lcbg0:
    mov r0, r10, lsr #8
    b .Lcb_op
.Lcbg1:
    and r0, r10, #255
    b .Lcb_op
.Lcbg2:
    mov r0, r11, lsr #8
    b .Lcb_op
.Lcbg3:
    and r0, r11, #255
    b .Lcb_op
.Lcbg4:
    mov r0, r9, lsr #8
    b .Lcb_op
.Lcbg5:
    and r0, r9, #255
    b .Lcb_op
.Lcbg7:
    mov r0, r7
    b .Lcb_op
.Lcbp0:
    and r10, r10, #255
    orr r10, r10, r0, lsl #8
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcbp1:
    bic r10, r10, #255
    orr r10, r10, r0
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcbp2:
    and r11, r11, #255
    orr r11, r11, r0, lsl #8
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcbp3:
    bic r11, r11, #255
    orr r11, r11, r0
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcbp4:
    and r9, r9, #255
    orr r9, r9, r0, lsl #8
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcbp5:
    bic r9, r9, #255
    orr r9, r9, r0
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcbp6:
    mov r1, r0
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #15
    bgt z80a_dispatch
    b z80a_exit
.Lcbp7:
    mov r7, r0
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
.Lcbo0:
    mov r0, r0, lsl #1
    orr r0, r0, r0, lsr #8
    and r0, r0, #255
    ldrb r8, [lr, r0]
    tst r0, #1
    orrne r8, r8, #1
    b .Lcb_put
.Lcbo1:
    and r1, r0, #1
    orr r0, r0, r0, lsl #8
    mov r0, r0, lsr #1
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lcb_put
.Lcbo2:
    and r1, r8, #1
    orr r0, r1, r0, lsl #1
    mov r1, r0, lsr #8
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lcb_put
.Lcbo3:
    and r1, r8, #1
    orr r0, r0, r1, lsl #8
    and r1, r0, #1
    mov r0, r0, lsr #1
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lcb_put
.Lcbo4:
    mov r1, r0, lsr #7
    mov r0, r0, lsl #1
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lcb_put
.Lcbo5:
    and r1, r0, #1
    and r2, r0, #0x80
    orr r0, r2, r0, lsr #1
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lcb_put
.Lcbo6:
    mov r1, r0, lsr #7
    mov r0, r0, lsl #1
    orr r0, r0, #1
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lcb_put
.Lcbo7:
    and r1, r0, #1
    mov r0, r0, lsr #1
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lcb_put
.Lcbo8:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #1
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo9:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #2
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo10:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #4
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo11:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #8
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo12:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #16
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo13:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #32
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo14:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #64
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo15:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #388]
    and r1, r1, #7
    cmp r1, #6
    andne r2, r0, #0x28
    orrne r8, r8, r2
    ands r2, r0, #128
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    cmp r1, #6
    beq 1f
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
1:
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
.Lcbo16:
    bic r0, r0, #1
    b .Lcb_put
.Lcbo17:
    bic r0, r0, #2
    b .Lcb_put
.Lcbo18:
    bic r0, r0, #4
    b .Lcb_put
.Lcbo19:
    bic r0, r0, #8
    b .Lcb_put
.Lcbo20:
    bic r0, r0, #16
    b .Lcb_put
.Lcbo21:
    bic r0, r0, #32
    b .Lcb_put
.Lcbo22:
    bic r0, r0, #64
    b .Lcb_put
.Lcbo23:
    bic r0, r0, #128
    b .Lcb_put
.Lcbo24:
    orr r0, r0, #1
    b .Lcb_put
.Lcbo25:
    orr r0, r0, #2
    b .Lcb_put
.Lcbo26:
    orr r0, r0, #4
    b .Lcb_put
.Lcbo27:
    orr r0, r0, #8
    b .Lcb_put
.Lcbo28:
    orr r0, r0, #16
    b .Lcb_put
.Lcbo29:
    orr r0, r0, #32
    b .Lcb_put
.Lcbo30:
    orr r0, r0, #64
    b .Lcb_put
.Lcbo31:
    orr r0, r0, #128
    b .Lcb_put
z80a_xy:
    str r1, [r4, #320]
    str r9, [r4, #316]
    ldr r9, [r4, r1]
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr r1, =xy_table
    ldr pc, [r1, r0, lsl #2]
xy_plain:
    ldr r9, [r4, #316]
    sub r6, r6, #4
    ldr pc, [r3, r0, lsl #2]
.section .text.z80a_xcb,"ax",%progbits
.align 2
z80a_xycb:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    str r0, [r4, #384]
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    str r1, [r4, #388]
    ldr r1, [r4, #320]
    str r9, [r4, r1]
    ldr r9, [r4, #316]
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    ldr r1, [r4, #388]
    mov r1, r1, lsr #3
    ldr r2, =xcb_ops
    ldr pc, [r2, r1, lsl #2]
.Lxcb_put:
    mov r1, r0
    ldr r0, [r4, #384]
    str r1, [r4, #384]
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #384]
    ldr r1, [r4, #388]
    and r1, r1, #7
    ldr r2, =xcb_copy
    ldr pc, [r2, r1, lsl #2]
.Lxcc0:
    and r10, r10, #255
    orr r10, r10, r0, lsl #8
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxcc1:
    bic r10, r10, #255
    orr r10, r10, r0
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxcc2:
    and r11, r11, #255
    orr r11, r11, r0, lsl #8
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxcc3:
    bic r11, r11, #255
    orr r11, r11, r0
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxcc4:
    and r9, r9, #255
    orr r9, r9, r0, lsl #8
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxcc5:
    bic r9, r9, #255
    orr r9, r9, r0
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxcc6:
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxcc7:
    mov r7, r0
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco0:
    mov r0, r0, lsl #1
    orr r0, r0, r0, lsr #8
    and r0, r0, #255
    ldrb r8, [lr, r0]
    tst r0, #1
    orrne r8, r8, #1
    b .Lxcb_put
.Lxco1:
    and r1, r0, #1
    orr r0, r0, r0, lsl #8
    mov r0, r0, lsr #1
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lxcb_put
.Lxco2:
    and r1, r8, #1
    orr r0, r1, r0, lsl #1
    mov r1, r0, lsr #8
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lxcb_put
.Lxco3:
    and r1, r8, #1
    orr r0, r0, r1, lsl #8
    and r1, r0, #1
    mov r0, r0, lsr #1
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lxcb_put
.Lxco4:
    mov r1, r0, lsr #7
    mov r0, r0, lsl #1
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lxcb_put
.Lxco5:
    and r1, r0, #1
    and r2, r0, #0x80
    orr r0, r2, r0, lsr #1
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lxcb_put
.Lxco6:
    mov r1, r0, lsr #7
    mov r0, r0, lsl #1
    orr r0, r0, #1
    and r0, r0, #255
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lxcb_put
.Lxco7:
    and r1, r0, #1
    mov r0, r0, lsr #1
    ldrb r8, [lr, r0]
    orr r8, r8, r1
    b .Lxcb_put
.Lxco8:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #1
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco9:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #2
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco10:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #4
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco11:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #8
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco12:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #16
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco13:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #32
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco14:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #64
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco15:
    and r8, r8, #1
    orr r8, r8, #0x10
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    and r1, r1, #0x28
    orr r8, r8, r1
    ands r2, r0, #128
    orreq r8, r8, #0x44
    andne r2, r2, #0x80
    orrne r8, r8, r2
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.Lxco16:
    bic r0, r0, #1
    b .Lxcb_put
.Lxco17:
    bic r0, r0, #2
    b .Lxcb_put
.Lxco18:
    bic r0, r0, #4
    b .Lxcb_put
.Lxco19:
    bic r0, r0, #8
    b .Lxcb_put
.Lxco20:
    bic r0, r0, #16
    b .Lxcb_put
.Lxco21:
    bic r0, r0, #32
    b .Lxcb_put
.Lxco22:
    bic r0, r0, #64
    b .Lxcb_put
.Lxco23:
    bic r0, r0, #128
    b .Lxcb_put
.Lxco24:
    orr r0, r0, #1
    b .Lxcb_put
.Lxco25:
    orr r0, r0, #2
    b .Lxcb_put
.Lxco26:
    orr r0, r0, #4
    b .Lxcb_put
.Lxco27:
    orr r0, r0, #8
    b .Lxcb_put
.Lxco28:
    orr r0, r0, #16
    b .Lxcb_put
.Lxco29:
    orr r0, r0, #32
    b .Lxcb_put
.Lxco30:
    orr r0, r0, #64
    b .Lxcb_put
.Lxco31:
    orr r0, r0, #128
    b .Lxcb_put
.ltorg
.section .hot.z80a,"ax",%progbits
.align 2
z80a_ed:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr r1, =ed_table
    ldr pc, [r1, r0, lsl #2]
ed_nop:
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
op_ill:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
.ltorg
op_00:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_01:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r10, r0, r2, lsl #8
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_02:
    mov r0, r10
    mov r1, r7
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_03:
    add r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_04:
    mov r1, r10, lsr #8
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_05:
    mov r1, r10, lsr #8
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_06:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_07:
    mov r7, r7, lsl #1
    orr r7, r7, r7, lsr #8
    and r7, r7, #255
    and r8, r8, #0xC4
    and r0, r7, #0x29
    orr r8, r8, r0
    subs r6, r6, #4
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_08:
    ldr r0, [r4, #300]
    orr r1, r8, r7, lsl #8
    str r1, [r4, #300]
    mov r7, r0, lsr #8
    and r8, r0, #255
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_09:
    add r0, r9, r10
    eor r1, r9, r10
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_0A:
    mov r2, r10, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r7, [r2, r10]
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_0B:
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_0C:
    and r1, r10, #255
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_0D:
    and r1, r10, #255
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_0E:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_0F:
    and r8, r8, #0xC4
    and r0, r7, #1
    orr r8, r8, r0
    orr r7, r7, r7, lsl #8
    mov r7, r7, lsr #1
    and r7, r7, #255
    and r0, r7, #0x28
    orr r8, r8, r0
    subs r6, r6, #4
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_10:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    tst r10, #0xFF00
    beq .Lz1
    mov r0, r0, lsl #24
    add r5, r5, r0, asr #24
    mov r5, r5, lsl #16
    mov r5, r5, lsr #16
    sub r6, r6, #5
.Lz1:
    subs r6, r6, #8
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_11:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r11, r0, r2, lsl #8
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_12:
    mov r0, r11
    mov r1, r7
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_13:
    add r11, r11, #1
    mov r11, r11, lsl #16
    mov r11, r11, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_14:
    mov r1, r11, lsr #8
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_16:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_17:
    and r0, r8, #1
    orr r7, r0, r7, lsl #1
    and r8, r8, #0xC4
    orr r8, r8, r7, lsr #8
    and r7, r7, #255
    and r0, r7, #0x28
    orr r8, r8, r0
    subs r6, r6, #4
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_18:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r5, r5, r0, asr #24
    mov r5, r5, lsl #16
    mov r5, r5, lsr #16
    subs r6, r6, #12
    bgt z80a_dispatch
    b z80a_exit
op_19:
    add r0, r9, r11
    eor r1, r9, r11
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_1A:
    mov r2, r11, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r7, [r2, r11]
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_1B:
    sub r11, r11, #1
    mov r11, r11, lsl #16
    mov r11, r11, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_1C:
    and r1, r11, #255
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_1E:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_20:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    tst r8, #0x40
    bne .Lz2
    mov r0, r0, lsl #24
    add r5, r5, r0, asr #24
    mov r5, r5, lsl #16
    mov r5, r5, lsr #16
    cmp r0, #0xF9000000
    blo .Lz3
    cmp r0, #0xFE000000
    bhs .Lz3
    bl z80a_idle
    add lr, r4, #512
.Lz3:
    sub r6, r6, #5
.Lz2:
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_21:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r9, r0, r2, lsl #8
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_22:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    and r1, r9, #255
    str r0, [r4, #384]
    mov r2, pc
    b z80a_wr8
    ldr r0, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r9, lsr #8
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #16
    bgt z80a_dispatch
    b z80a_exit
op_23:
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_24:
    mov r1, r9, lsr #8
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_26:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_28:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    tst r8, #0x40
    beq .Lz4
    mov r0, r0, lsl #24
    add r5, r5, r0, asr #24
    mov r5, r5, lsl #16
    mov r5, r5, lsr #16
    cmp r0, #0xF9000000
    blo .Lz5
    cmp r0, #0xFE000000
    bhs .Lz5
    bl z80a_idle
    add lr, r4, #512
.Lz5:
    sub r6, r6, #5
.Lz4:
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_29:
    add r0, r9, r9
    eor r1, r9, r9
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_2A:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r1, r0, r2, lsl #8
    mov r0, r1
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r9, [r2, r0]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    orr r9, r9, r1, lsl #8
    subs r6, r6, #16
    bgt z80a_dispatch
    b z80a_exit
op_2B:
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_2C:
    and r1, r9, #255
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
.ltorg
op_2E:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_2F:
    eor r7, r7, #255
    and r8, r8, #0xC5
    orr r8, r8, #0x12
    and r0, r7, #0x28
    orr r8, r8, r0
    subs r6, r6, #4
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_30:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    tst r8, #1
    bne .Lz6
    mov r0, r0, lsl #24
    add r5, r5, r0, asr #24
    mov r5, r5, lsl #16
    mov r5, r5, lsr #16
    sub r6, r6, #5
.Lz6:
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_32:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    mov r1, r7
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #13
    bgt z80a_dispatch
    b z80a_exit
op_33:
    add r12, r12, #1
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_34:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_35:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_36:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_38:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    tst r8, #1
    beq .Lz7
    mov r0, r0, lsl #24
    add r5, r5, r0, asr #24
    mov r5, r5, lsl #16
    mov r5, r5, lsr #16
    sub r6, r6, #5
.Lz7:
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_3A:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r7, [r2, r0]
    subs r6, r6, #13
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_3B:
    sub r12, r12, #1
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
op_3C:
    mov r1, r7
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_3D:
    mov r1, r7
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_3E:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    mov r7, r1
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_40:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_41:
    and r1, r10, #255
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_42:
    mov r1, r11, lsr #8
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_43:
    and r1, r11, #255
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_44:
    mov r1, r9, lsr #8
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_45:
    and r1, r9, #255
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_46:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_47:
    mov r1, r7
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_48:
    mov r1, r10, lsr #8
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_49:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_4A:
    mov r1, r11, lsr #8
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_4B:
    and r1, r11, #255
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_4C:
    mov r1, r9, lsr #8
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_4D:
    and r1, r9, #255
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_4E:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_4F:
    mov r1, r7
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_50:
    mov r1, r10, lsr #8
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_51:
    and r1, r10, #255
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_52:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_53:
    and r1, r11, #255
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_54:
    mov r1, r9, lsr #8
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_55:
    and r1, r9, #255
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_56:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_57:
    mov r1, r7
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_58:
    mov r1, r10, lsr #8
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_59:
    and r1, r10, #255
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
.ltorg
op_5A:
    mov r1, r11, lsr #8
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_5B:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_5C:
    mov r1, r9, lsr #8
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_5D:
    and r1, r9, #255
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_5E:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_5F:
    mov r1, r7
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_60:
    mov r1, r10, lsr #8
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_61:
    and r1, r10, #255
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_62:
    mov r1, r11, lsr #8
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_63:
    and r1, r11, #255
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_64:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_65:
    and r1, r9, #255
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_66:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_67:
    mov r1, r7
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_68:
    mov r1, r10, lsr #8
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_69:
    and r1, r10, #255
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_6A:
    mov r1, r11, lsr #8
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_6B:
    and r1, r11, #255
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_6C:
    mov r1, r9, lsr #8
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_6D:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_6E:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_6F:
    mov r1, r7
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_70:
    mov r1, r10, lsr #8
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_71:
    and r1, r10, #255
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_72:
    mov r1, r11, lsr #8
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_73:
    and r1, r11, #255
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_74:
    mov r1, r9, lsr #8
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_75:
    and r1, r9, #255
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_77:
    mov r1, r7
    mov r0, r9
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_78:
    mov r1, r10, lsr #8
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_79:
    and r1, r10, #255
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_7A:
    mov r1, r11, lsr #8
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_7B:
    and r1, r11, #255
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_7C:
    mov r1, r9, lsr #8
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_7D:
    and r1, r9, #255
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_7E:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    mov r7, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_7F:
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_81:
    and r0, r10, #255
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_82:
    mov r0, r11, lsr #8
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_83:
    and r0, r11, #255
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
.ltorg
op_85:
    and r0, r9, #255
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_87:
    mov r0, r7
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_8A:
    mov r0, r11, lsr #8
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_8C:
    mov r0, r9, lsr #8
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_8F:
    mov r0, r7
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_90:
    mov r0, r10, lsr #8
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_91:
    and r0, r10, #255
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_93:
    and r0, r11, #255
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_95:
    and r0, r9, #255
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_9F:
    mov r0, r7
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A0:
    mov r0, r10, lsr #8
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A1:
    and r0, r10, #255
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A2:
    mov r0, r11, lsr #8
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A3:
    and r0, r11, #255
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A4:
    mov r0, r9, lsr #8
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A5:
    and r0, r9, #255
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A6:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_A7:
    mov r0, r7
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A8:
    mov r0, r10, lsr #8
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_A9:
    and r0, r10, #255
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_AA:
    mov r0, r11, lsr #8
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_AB:
    and r0, r11, #255
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_AC:
    mov r0, r9, lsr #8
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_AD:
    and r0, r9, #255
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_AE:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_AF:
    mov r0, r7
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B0:
    mov r0, r10, lsr #8
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B1:
    and r0, r10, #255
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B2:
    mov r0, r11, lsr #8
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B3:
    and r0, r11, #255
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B4:
    mov r0, r9, lsr #8
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B5:
    and r0, r9, #255
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B6:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_B7:
    mov r0, r7
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B8:
    mov r0, r10, lsr #8
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_B9:
    and r0, r10, #255
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_BE:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_C0:
    tst r8, #0x40
    bne .Lz8
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz8:
    subs r6, r6, #5
    bgt z80a_dispatch
    b z80a_exit
op_C1:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r10, r0, r1, lsl #8
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_C2:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x40
    bne .Lz9
    mov r5, r0
.Lz9:
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
.ltorg
op_C3:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r5, r0, r2, lsl #8
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_C4:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x40
    bne .Lz10
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz10:
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_C5:
    str r10, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #11
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_C6:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_C8:
    tst r8, #0x40
    beq .Lz11
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz11:
    subs r6, r6, #5
    bgt z80a_dispatch
    b z80a_exit
op_C9:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_CA:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x40
    beq .Lz12
    mov r5, r0
.Lz12:
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_CB:
    b z80a_cb
op_CC:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x40
    beq .Lz13
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz13:
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_CD:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    ldr r5, [r4, #388]
    subs r6, r6, #17
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_CF:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    mov r5, #8
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_D0:
    tst r8, #1
    bne .Lz14
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz14:
    subs r6, r6, #5
    bgt z80a_dispatch
    b z80a_exit
op_D1:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r11, r0, r1, lsl #8
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_D2:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #1
    bne .Lz15
    mov r5, r0
.Lz15:
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_D3:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r7
    bl z80a_out_call
    subs r6, r6, #11
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_D5:
    str r11, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_D6:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_D7:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    mov r5, #16
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_D8:
    tst r8, #1
    beq .Lz17
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz17:
    subs r6, r6, #5
    bgt z80a_dispatch
    b z80a_exit
op_DA:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #1
    beq .Lz18
    mov r5, r0
.Lz18:
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_DB:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    bl z80a_in_a
    mov r7, r0
    subs r6, r6, #11
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_DD:
    mov r1, #288
    b z80a_xy
op_DF:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    mov r5, #24
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_E1:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r9, r0, r1, lsl #8
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_E5:
    str r9, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_E6:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_E9:
    mov r5, r9
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_EA:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #4
    beq .Lz24
    mov r5, r0
.Lz24:
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_EB:
    mov r0, r9
    mov r9, r11
    mov r11, r0
    subs r6, r6, #4
    bgt z80a_dispatch
    b z80a_exit
op_ED:
    b z80a_ed
op_EE:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #7
    bgt z80a_dispatch
    b z80a_exit
op_EF:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    mov r5, #40
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_F0:
    tst r8, #0x80
    bne .Lz26
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz26:
    subs r6, r6, #5
    bgt z80a_dispatch
    b z80a_exit
op_F1:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r1, r0, r1, lsl #8
    mov r7, r1, lsr #8
    and r8, r1, #255
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_F2:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x80
    bne .Lz27
    mov r5, r0
.Lz27:
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_F5:
    orr r1, r8, r7, lsl #8
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    subs r6, r6, #11
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_F6:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
op_F7:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    mov r5, #48
    subs r6, r6, #11
    bgt z80a_dispatch
    b z80a_exit
op_F8:
    tst r8, #0x80
    beq .Lz29
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz29:
    subs r6, r6, #5
    bgt z80a_dispatch
    b z80a_exit
op_F9:
    mov r12, r9
    subs r6, r6, #6
    bgt z80a_dispatch
    b z80a_exit
.ltorg
op_FA:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x80
    beq .Lz30
    mov r5, r0
.Lz30:
    subs r6, r6, #10
    bgt z80a_dispatch
    b z80a_exit
op_FD:
    mov r1, #292
    b z80a_xy
op_FE:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #7
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
xy_19:
    add r0, r9, r11
    eor r1, r9, r11
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #15
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
xy_21:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r9, r0, r2, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #14
    bgt z80a_dispatch
    b z80a_exit
xy_23:
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #10
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
xy_34:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    str r0, [r4, #384]
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    ldr r0, [r4, #384]
    mov r2, pc
    b z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #23
    bgt z80a_dispatch
    b z80a_exit
xy_35:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    str r0, [r4, #384]
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    ldr r0, [r4, #384]
    mov r2, pc
    b z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #23
    bgt z80a_dispatch
    b z80a_exit
xy_36:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    str r0, [r4, #384]
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    ldr r0, [r4, #384]
    mov r2, pc
    b z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_46:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_56:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_5E:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    bic r11, r11, #255
    orr r11, r11, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_70:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r10, lsr #8
    mov r2, pc
    b z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_77:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r7
    mov r2, pc
    b z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_7D:
    and r1, r9, #255
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    bgt z80a_dispatch
    b z80a_exit
xy_7E:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ble z80a_exit
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    ldr pc, [r3, r0, lsl #2]
xy_86:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_96:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_B6:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_BE:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    bgt z80a_dispatch
    b z80a_exit
xy_E1:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r9, r0, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #14
    bgt z80a_dispatch
    b z80a_exit
xy_E5:
    str r9, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    b z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    b z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #15
    bgt z80a_dispatch
    b z80a_exit
ed_A3:
    and r0, r10, #0xC1
    cmp r0, #0x80
    bne ed_A3_gen
    ldrb r0, [r4, #408]
    cmp r0, #3
    beq ed_A3_gen
    ldr pc, =z80a_blk_outi
ed_A3_gen:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    and r0, r10, #255
    bl z80a_out_call
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    subs r6, r6, #16
    bgt z80a_dispatch
    b z80a_exit
ed_B0:
    subs r6, r6, #16
.Lz32:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    add r0, r1, r7
    str r0, [r4, #388]
    mov r0, r11
    mov r2, pc
    b z80a_wr8
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    add r11, r11, #1
    mov r11, r11, lsl #16
    mov r11, r11, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    ldr r1, [r4, #388]
    and r8, r8, #0xC1
    cmp r10, #0
    orrne r8, r8, #4
    and r0, r1, #8
    orr r8, r8, r0
    tst r1, #2
    orrne r8, r8, #0x20
    cmp r10, #0
    beq .Lz33
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz32
1:
    sub r5, r5, #2
    b z80a_exit
.Lz33:
    cmp r6, #0
    bgt z80a_dispatch
    b z80a_exit
ed_B3:
    mov r0, r10, lsr #8
    sub r0, r0, #1
    and r0, r0, #255
    cmp r0, #3
    blo ed_B3_gen
    and r0, r10, #0xC1
    cmp r0, #0x80
    bne ed_B3_gen
    ldrb r0, [r4, #408]
    cmp r0, #3
    beq ed_B3_gen
    ldr pc, =z80a_blk_otir
ed_B3_gen:
    subs r6, r6, #16
.Lz38:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    and r0, r10, #255
    bl z80a_out_call
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    tst r10, #0xFF00
    beq .Lz39
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz38
1:
    sub r5, r5, #2
    b z80a_exit
.Lz39:
    cmp r6, #0
    bgt z80a_dispatch
    b z80a_exit
z80a_vwait:
    stmfd sp!, {r3, lr}
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    cmp r0, #0xFE
    bne .Lva_go
    add r0, r5, #3
    ldrb r0, [r1, r0]
    tst r0, #0x80
    beq .Lva_go
    add r0, r5, #2
    ldrb lr, [r1, r0]
    sub r0, lr, #0x20
    tst r0, #0xE7
    bne .Lva_go
    add r0, r5, #1
    ldrb r3, [r1, r0]
    cmp r3, #0xD4
    bhi .Lva_late
    ldr r1, [r4, #348]
    sub r1, r1, r6
    ldr r0, [r4, #436]
    add r1, r1, r0
    ldr r0, =301401214
    umull r2, r0, r1, r0
    mov r0, r0, lsr #4
    ldr r2, [r4, #440]
    add r0, r0, r2
    cmp r0, #0xDA
    subhi r2, r0, #6
    movls r2, r0
    cmp lr, #0x30
    beq .Lva_nc
    bhi .Lva_c
    cmp lr, #0x28
    beq .Lva_z
    cmp r2, r3
    beq .Lva_go
    blo .Lva_to
    b .Lva_all
.Lva_z:
    cmp r2, r3
    bne .Lva_go
    add r3, r3, #1
    b .Lva_to
.Lva_nc:
    cmp r2, r3
    blo .Lva_go
    b .Lva_all
.Lva_c:
    cmp r2, r3
    bhs .Lva_go
.Lva_to:
    ldr r2, [r4, #440]
    sub r2, r3, r2
    mov r0, r2, lsl #8
    sub r0, r0, r2, lsl #5
    add r0, r0, r2, lsl #2
    subs r0, r0, r1
    ble .Lva_go
    subs r6, r6, r0
    movle r6, #0
    b .Lva_go
.Lva_all:
    mov r6, #0
    b .Lva_go
.Lva_late:
    cmp r3, #0xDA
    bhi .Lva_go
    cmp lr, #0x38
    bne .Lva_go
    add r0, r5, #3
    ldrb r0, [r1, r0]
    cmp r0, #0xFA
    bne .Lva_go
    ldr r1, [r4, #348]
    sub r1, r1, r6
    ldr r0, [r4, #436]
    add r1, r1, r0
    ldr r2, [r4, #440]
    sub r2, r3, r2
    mov r0, r2, lsl #8
    sub r0, r0, r2, lsl #5
    add r0, r0, r2, lsl #2
    subs r0, r0, r1
    ble .Lva_go
    sub r1, r6, #1
    cmp r0, r1
    movhi r0, r1
    ldr r1, =0x88888889
    umull r2, r1, r0, r1
    mov r1, r1, lsr #4
    rsb r1, r1, r1, lsl #4
    sub r6, r6, r1, lsl #1
.Lva_go:
    mov r0, #0x7E
    ldmfd sp!, {r3, lr}
    b z80a_in_call
.ltorg
.ltorg
.section .text.z80a_cold,"ax",%progbits
.align 2
z80a_blk_outi:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    cmp r0, #0xED
    bne .Lbo_jp
    add r2, r5, #1
    mov r1, r2, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r1, [r1, r2]
    cmp r1, #0xA3
    bne .Lbo_no
    mov r0, #2
    b z80a_blk
.Lbo_jp:
    cmp r0, #0xC2
    bne .Lbo_no
    mov r1, r10, lsr #8
    sub r1, r1, #1
    and r1, r1, #255
    cmp r1, #3
    blo .Lbo_no
    add r2, r5, #1
    mov r1, r2, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r2]
    add r2, r5, #2
    mov r1, r2, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r1, [r1, r2]
    orr r0, r0, r1, lsl #8
    sub r1, r5, #2
    mov r1, r1, lsl #16
    mov r1, r1, lsr #16
    cmp r0, r1
    bne .Lbo_no
    mov r0, #1
    b z80a_blk
.Lbo_no:
    ldr pc, =ed_A3_gen
z80a_blk_otir:
    mov r0, #0
z80a_blk:
    str r12, [r4, #296]
    str r3, [r4, #396]
    str r6, [r4, #352]
    str r10, [r4, #364]
    str r9, [r4, #372]
    str r8, [r4, #360]
    str r5, [r4, #376]
    ldr r1, =z80a_blk_c
    mov lr, pc
    bx r1
    ldr r3, [r4, #396]
    ldr r12, [r4, #296]
    ldr r6, [r4, #352]
    ldr r10, [r4, #364]
    ldr r9, [r4, #372]
    ldr r8, [r4, #360]
    ldr r5, [r4, #376]
    add lr, r4, #512
    cmp r6, #0
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.ltorg
op_15:
    mov r1, r11, lsr #8
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_1D:
    and r1, r11, #255
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_1F:
    and r0, r8, #1
    and r8, r8, #0xC4
    and r1, r7, #1
    orr r8, r8, r1
    orr r7, r7, r0, lsl #8
    mov r7, r7, lsr #1
    and r0, r7, #0x28
    orr r8, r8, r0
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_25:
    mov r1, r9, lsr #8
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_27:
    ldr r1, [r4, #392]
    and r0, r8, #0x13
    and r2, r0, #3
    orr r2, r2, r0, lsr #2
    orr r0, r7, r2, lsl #8
    mov r0, r0, lsl #1
    ldrh r0, [r1, r0]
    mov r7, r0, lsr #8
    and r8, r0, #255
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_2D:
    and r1, r9, #255
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_31:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r12, r0, r2, lsl #8
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_37:
    and r8, r8, #0xC4
    orr r8, r8, #1
    and r0, r7, #0x28
    orr r8, r8, r0
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_39:
    add r0, r9, r12
    eor r1, r9, r12
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    subs r6, r6, #11
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_3F:
    and r0, r8, #1
    and r8, r8, #0xC5
    orr r8, r8, r0, lsl #4
    eor r8, r8, #1
    and r0, r7, #0x28
    orr r8, r8, r0
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_76:
    mov r0, #1
    strb r0, [r4, #341]
    sub r5, r5, #1
    sub r6, r6, #4
    cmp r6, #0
    ldrle pc, =z80a_exit
    add r0, r6, #3
    bic r0, r0, #3
    sub r6, r6, r0
    ldr pc, =z80a_exit
op_80:
    mov r0, r10, lsr #8
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_84:
    mov r0, r9, lsr #8
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_86:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_88:
    mov r0, r10, lsr #8
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_89:
    and r0, r10, #255
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_8B:
    and r0, r11, #255
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_8D:
    and r0, r9, #255
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_8E:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_92:
    mov r0, r11, lsr #8
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_94:
    mov r0, r9, lsr #8
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_96:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_97:
    mov r0, r7
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_98:
    mov r0, r10, lsr #8
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_99:
    and r0, r10, #255
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_9A:
    mov r0, r11, lsr #8
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_9B:
    and r0, r11, #255
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_9C:
    mov r0, r9, lsr #8
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_9D:
    and r0, r9, #255
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_9E:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r9]
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.ltorg
op_BA:
    mov r0, r11, lsr #8
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_BB:
    and r0, r11, #255
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_BC:
    mov r0, r9, lsr #8
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_BD:
    and r0, r9, #255
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_BF:
    mov r0, r7
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_C7:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r5, #0
    subs r6, r6, #11
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_CE:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_D4:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #1
    bne .Lz16
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz16:
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_D9:
    ldr r0, [r4, #304]
    str r10, [r4, #304]
    mov r10, r0
    ldr r0, [r4, #308]
    str r11, [r4, #308]
    mov r11, r0
    ldr r0, [r4, #312]
    str r9, [r4, #312]
    mov r9, r0
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_DC:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #1
    beq .Lz19
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz19:
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_DE:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #7
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_E0:
    tst r8, #4
    bne .Lz20
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz20:
    subs r6, r6, #5
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_E2:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #4
    bne .Lz21
    mov r5, r0
.Lz21:
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_E3:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r1, r12, #1
    mov r1, r1, lsl #16
    mov r1, r1, lsr #16
    mov r2, r1, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r1]
    orr r0, r0, r1, lsl #8
    str r0, [r4, #384]
    mov r0, r12
    and r1, r9, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r9, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r9, [r4, #384]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_E4:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #4
    bne .Lz22
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz22:
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_E7:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r5, #32
    subs r6, r6, #11
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_E8:
    tst r8, #4
    beq .Lz23
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    sub r6, r6, #6
.Lz23:
    subs r6, r6, #5
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_EC:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #4
    beq .Lz25
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz25:
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_F3:
    mov r0, #0
    strb r0, [r4, #339]
    strb r0, [r4, #340]
    subs r6, r6, #4
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_F4:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x80
    bne .Lz28
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz28:
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_FB:
    mov r0, #1
    strb r0, [r4, #339]
    strb r0, [r4, #340]
    subs r6, r6, #4
    ldr pc, =z80a_arm_irq
op_FC:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    tst r8, #0x80
    beq .Lz31
    str r0, [r4, #388]
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r5, [r4, #388]
    sub r6, r6, #7
.Lz31:
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
op_FF:
    mov r1, r5, lsl #16
    mov r1, r1, lsr #16
    str r1, [r4, #384]
    sub r12, r12, #2
    mov r12, r12, lsl #16
    mov r12, r12, lsr #16
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #384]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r0, r12
    ldr r1, [r4, #384]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    mov r5, #56
    subs r6, r6, #11
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_09:
    add r0, r9, r10
    eor r1, r9, r10
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_22:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    and r1, r9, #255
    str r0, [r4, #384]
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r9, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_24:
    mov r1, r9, lsr #8
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_25:
    mov r1, r9, lsr #8
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_26:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #11
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_29:
    add r0, r9, r9
    eor r1, r9, r9
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_2A:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r1, r0, r2, lsl #8
    mov r0, r1
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r9, [r2, r0]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.ltorg
xy_2B:
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_2C:
    and r1, r9, #255
    add r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #512
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_2D:
    and r1, r9, #255
    sub r1, r1, #1
    and r1, r1, #255
    and r8, r8, #1
    add r2, lr, #768
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_2E:
    mov r2, r5, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r5]
    add r5, r5, #1
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #11
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_39:
    add r0, r9, r12
    eor r1, r9, r12
    eor r1, r1, r0
    and r8, r8, #0xC4
    and r1, r1, #0x1000
    orr r8, r8, r1, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    and r1, r0, #0x2800
    orr r8, r8, r1, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_44:
    mov r1, r9, lsr #8
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_45:
    and r1, r9, #255
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_4C:
    mov r1, r9, lsr #8
    bic r10, r10, #255
    orr r10, r10, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_4D:
    and r1, r9, #255
    bic r10, r10, #255
    orr r10, r10, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_4E:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    bic r10, r10, #255
    orr r10, r10, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_54:
    mov r1, r9, lsr #8
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_55:
    and r1, r9, #255
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_5C:
    mov r1, r9, lsr #8
    bic r11, r11, #255
    orr r11, r11, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_5D:
    and r1, r9, #255
    bic r11, r11, #255
    orr r11, r11, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_60:
    mov r1, r10, lsr #8
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_61:
    and r1, r10, #255
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_62:
    mov r1, r11, lsr #8
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_63:
    and r1, r11, #255
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_64:
    mov r1, r9, lsr #8
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_65:
    and r1, r9, #255
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_66:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    ldr r0, [r4, #316]
    and r0, r0, #255
    orr r0, r0, r1, lsl #8
    str r0, [r4, #316]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_67:
    mov r1, r7
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_68:
    mov r1, r10, lsr #8
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_69:
    and r1, r10, #255
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_6A:
    mov r1, r11, lsr #8
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_6B:
    and r1, r11, #255
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_6C:
    mov r1, r9, lsr #8
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_6D:
    and r1, r9, #255
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_6E:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    ldr r0, [r4, #316]
    bic r0, r0, #255
    orr r0, r0, r1
    str r0, [r4, #316]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_6F:
    mov r1, r7
    bic r9, r9, #255
    orr r9, r9, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.ltorg
xy_71:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    and r1, r10, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_72:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r11, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_73:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    and r1, r11, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_74:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #316]
    mov r1, r1, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_75:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    ldr r1, [r4, #316]
    and r1, r1, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_7C:
    mov r1, r9, lsr #8
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_84:
    mov r0, r9, lsr #8
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_85:
    and r0, r9, #255
    add r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_8C:
    mov r0, r9, lsr #8
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_8D:
    and r0, r9, #255
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_8E:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    and r2, r8, #1
    add r1, r7, r0
    add r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    mvn r2, r2
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_94:
    mov r0, r9, lsr #8
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_95:
    and r0, r9, #255
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_9C:
    mov r0, r9, lsr #8
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_9D:
    and r0, r9, #255
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_9E:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    and r2, r8, #1
    sub r1, r7, r0
    sub r1, r1, r2
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_A4:
    mov r0, r9, lsr #8
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_A5:
    and r0, r9, #255
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_A6:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    and r7, r7, r0
    ldrb r8, [lr, r7]
    orr r8, r8, #0x10
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_AC:
    mov r0, r9, lsr #8
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_AD:
    and r0, r9, #255
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_AE:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r0, r0, lsl #24
    add r0, r9, r0, asr #24
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r0]
    eor r7, r7, r0
    ldrb r8, [lr, r7]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #19
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_B4:
    mov r0, r9, lsr #8
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_B5:
    and r0, r9, #255
    orr r7, r7, r0
    ldrb r8, [lr, r7]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_BC:
    mov r0, r9, lsr #8
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_BD:
    and r0, r9, #255
    sub r1, r7, r0
    and r8, r0, #0x28
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    orr r8, r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    bic r2, r2, #0x28
    orr r8, r8, r2
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_CB:
    ldr pc, =z80a_xycb
xy_E3:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r1, r12, #1
    mov r1, r1, lsl #16
    mov r1, r1, lsr #16
    mov r2, r1, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r1]
    orr r0, r0, r1, lsl #8
    str r0, [r4, #384]
    mov r0, r12
    and r1, r9, #255
    mov r2, pc
    ldr pc, =z80a_wr8
    add r0, r12, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r9, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r9, [r4, #384]
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #23
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_E9:
    mov r5, r9
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
xy_F9:
    mov r12, r9
    ldr r0, [r4, #320]
    str r9, [r4, r0]
    ldr r9, [r4, #316]
    subs r6, r6, #10
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.ltorg
ed_40:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    mov r1, r0
    and r10, r10, #255
    orr r10, r10, r1, lsl #8
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_41:
    mov r1, r10, lsr #8
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_42:
    str r12, [r4, #296]
    mov r1, r10
    and r2, r8, #1
    sub r0, r9, r1
    sub r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    orr r8, r8, #2
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r12, r9, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_43:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    and r1, r10, #255
    str r0, [r4, #384]
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r10, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_44:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_45:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
ed_46:
    mov r0, #0
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_47:
    strb r7, [r4, #336]
    subs r6, r6, #9
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_48:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    mov r1, r0
    bic r10, r10, #255
    orr r10, r10, r1
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_49:
    and r1, r10, #255
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_4A:
    str r12, [r4, #296]
    mov r1, r10
    and r2, r8, #1
    add r0, r9, r1
    add r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r2, r2, #0x8000
    eor r12, r1, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_4B:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r1, r0, r2, lsl #8
    mov r0, r1
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    str r1, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    ldr r0, [r4, #384]
    orr r10, r0, r1, lsl #8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_4C:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_4D:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
ed_4E:
    mov r0, #0
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_4F:
    strb r7, [r4, #337]
    and r0, r7, #0x80
    strb r0, [r4, #346]
    subs r6, r6, #9
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_50:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    mov r1, r0
    and r11, r11, #255
    orr r11, r11, r1, lsl #8
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_51:
    mov r1, r11, lsr #8
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_52:
    str r12, [r4, #296]
    mov r1, r11
    and r2, r8, #1
    sub r0, r9, r1
    sub r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    orr r8, r8, #2
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r12, r9, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_53:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    and r1, r11, #255
    str r0, [r4, #384]
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r11, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_54:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_55:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
ed_56:
    mov r0, #1
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_57:
    ldrb r7, [r4, #336]
    and r8, r8, #1
    add r0, lr, #256
    ldrb r0, [r0, r7]
    orr r8, r8, r0
    ldrb r0, [r4, #340]
    cmp r0, #0
    orrne r8, r8, #4
    subs r6, r6, #9
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_58:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    mov r1, r0
    bic r11, r11, #255
    orr r11, r11, r1
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_59:
    and r1, r11, #255
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_5A:
    str r12, [r4, #296]
    mov r1, r11
    and r2, r8, #1
    add r0, r9, r1
    add r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r2, r2, #0x8000
    eor r12, r1, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_5B:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r1, r0, r2, lsl #8
    mov r0, r1
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    str r1, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    ldr r0, [r4, #384]
    orr r11, r0, r1, lsl #8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_5C:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_5D:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
.ltorg
ed_5E:
    mov r0, #2
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_5F:
    and r7, r6, #0x7F
    ldrb r0, [r4, #346]
    orr r7, r7, r0
    and r8, r8, #1
    add r0, lr, #256
    ldrb r0, [r0, r7]
    orr r8, r8, r0
    ldrb r0, [r4, #340]
    cmp r0, #0
    orrne r8, r8, #4
    subs r6, r6, #9
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_60:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    mov r1, r0
    and r9, r9, #255
    orr r9, r9, r1, lsl #8
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_61:
    mov r1, r9, lsr #8
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_62:
    str r12, [r4, #296]
    mov r1, r9
    and r2, r8, #1
    sub r0, r9, r1
    sub r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    orr r8, r8, #2
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r12, r9, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_63:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    and r1, r9, #255
    str r0, [r4, #384]
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r9, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_64:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_65:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
ed_66:
    mov r0, #0
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_67:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    and r0, r1, #15
    mov r1, r1, lsr #4
    orr r1, r1, r7, lsl #4
    and r1, r1, #255
    bic r7, r7, #15
    orr r7, r7, r0
    and r8, r8, #1
    ldrb r0, [lr, r7]
    orr r8, r8, r0
    mov r0, r9
    mov r2, pc
    ldr pc, =z80a_wr8
    subs r6, r6, #18
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_68:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    mov r1, r0
    bic r9, r9, #255
    orr r9, r9, r1
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_69:
    and r1, r9, #255
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_6A:
    str r12, [r4, #296]
    mov r1, r9
    and r2, r8, #1
    add r0, r9, r1
    add r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r2, r2, #0x8000
    eor r12, r1, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_6B:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r1, r0, r2, lsl #8
    mov r0, r1
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    str r1, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    ldr r0, [r4, #384]
    orr r9, r0, r1, lsl #8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_6C:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_6D:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
ed_6E:
    mov r0, #0
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_6F:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    mov r0, r1, lsr #4
    and r2, r7, #15
    orr r1, r2, r1, lsl #4
    and r1, r1, #255
    bic r7, r7, #15
    orr r7, r7, r0
    and r8, r8, #1
    ldrb r0, [lr, r7]
    orr r8, r8, r0
    mov r0, r9
    mov r2, pc
    ldr pc, =z80a_wr8
    subs r6, r6, #18
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_70:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_71:
    mov r1, #0
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_72:
    str r12, [r4, #296]
    mov r1, r12
    and r2, r8, #1
    sub r0, r9, r1
    sub r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    orr r8, r8, #2
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r12, r9, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_73:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r0, r0, r2, lsl #8
    and r1, r12, #255
    str r0, [r4, #384]
    mov r2, pc
    ldr pc, =z80a_wr8
    ldr r0, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r1, r12, lsr #8
    mov r2, pc
    ldr pc, =z80a_wr8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_74:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_75:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
ed_76:
    mov r0, #1
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_78:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    and r8, r8, #1
    ldrb r1, [lr, r0]
    orr r8, r8, r1
    mov r1, r0
    mov r7, r1
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_79:
    mov r1, r7
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    subs r6, r6, #12
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_7A:
    str r12, [r4, #296]
    mov r1, r12
    and r2, r8, #1
    add r0, r9, r1
    add r0, r0, r2
    eor r2, r9, r1
    eor r2, r2, r0
    and r8, r2, #0x1000
    mov r8, r8, lsr #8
    tst r0, #0x10000
    orrne r8, r8, #1
    eor r2, r1, r9
    eor r2, r2, #0x8000
    eor r12, r1, r0
    and r2, r2, r12
    and r2, r2, #0x8000
    orr r8, r8, r2, lsr #13
    and r2, r0, #0xA800
    orr r8, r8, r2, lsr #8
    mov r9, r0, lsl #16
    mov r9, r9, lsr #16
    cmp r9, #0
    orreq r8, r8, #0x40
    ldr r12, [r4, #296]
    subs r6, r6, #15
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_7B:
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r0, [r1, r5]
    add r5, r5, #1
    mov r1, r5, lsr #10
    ldr r1, [r4, r1, lsl #2]
    ldrb r2, [r1, r5]
    add r5, r5, #1
    orr r1, r0, r2, lsl #8
    mov r0, r1
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    str r1, [r4, #384]
    add r0, r0, #1
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    mov r2, r0, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r0]
    ldr r0, [r4, #384]
    orr r12, r0, r1, lsl #8
    subs r6, r6, #20
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_7C:
    mov r0, r7
    mov r7, #0
    sub r1, r7, r0
    eor r2, r7, r0
    eor r2, r2, r1
    and r2, r2, #0x10
    mov r8, r2
    eor r2, r7, r0
    eor r0, r7, r1
    and r2, r2, r0
    and r2, r2, #0x80
    orr r8, r8, r2, lsr #5
    tst r1, #0x100
    orrne r8, r8, #1
    orr r8, r8, #2
    and r1, r1, #255
    add r2, lr, #256
    ldrb r2, [r2, r1]
    orr r8, r8, r2
    mov r7, r1
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.ltorg
ed_7D:
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r0, [r2, r12]
    add r12, r12, #1
    mov r2, r12, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r12]
    add r12, r12, #1
    bic r12, r12, #0x10000
    orr r5, r0, r1, lsl #8
    ldrb r0, [r4, #340]
    strb r0, [r4, #339]
    subs r6, r6, #14
    ldr pc, =z80a_arm_irq
ed_7E:
    mov r0, #2
    strb r0, [r4, #338]
    subs r6, r6, #8
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_A0:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    add r0, r1, r7
    str r0, [r4, #388]
    mov r0, r11
    mov r2, pc
    ldr pc, =z80a_wr8
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    add r11, r11, #1
    mov r11, r11, lsl #16
    mov r11, r11, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    ldr r1, [r4, #388]
    and r8, r8, #0xC1
    cmp r10, #0
    orrne r8, r8, #4
    and r0, r1, #8
    orr r8, r8, r0
    tst r1, #2
    orrne r8, r8, #0x20
    subs r6, r6, #16
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_A1:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    sub r0, r7, r1
    and r0, r0, #255
    and r8, r8, #1
    orr r8, r8, #2
    add r2, lr, #256
    ldrb r2, [r2, r0]
    bic r2, r2, #0x28
    orr r8, r8, r2
    eor r2, r7, r1
    eor r2, r2, r0
    and r2, r2, #0x10
    orr r8, r8, r2
    cmp r10, #0
    orrne r8, r8, #4
    tst r8, #0x10
    subne r0, r0, #1
    and r1, r0, #8
    orr r8, r8, r1
    tst r0, #2
    orrne r8, r8, #0x20
    subs r6, r6, #16
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_A2:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    mov r1, r0
    mov r0, r9
    mov r2, pc
    ldr pc, =z80a_wr8
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    subs r6, r6, #16
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_A8:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    add r0, r1, r7
    str r0, [r4, #388]
    mov r0, r11
    mov r2, pc
    ldr pc, =z80a_wr8
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r11, r11, #1
    mov r11, r11, lsl #16
    mov r11, r11, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    ldr r1, [r4, #388]
    and r8, r8, #0xC1
    cmp r10, #0
    orrne r8, r8, #4
    and r0, r1, #8
    orr r8, r8, r0
    tst r1, #2
    orrne r8, r8, #0x20
    subs r6, r6, #16
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_A9:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    sub r0, r7, r1
    and r0, r0, #255
    and r8, r8, #1
    orr r8, r8, #2
    add r2, lr, #256
    ldrb r2, [r2, r0]
    bic r2, r2, #0x28
    orr r8, r8, r2
    eor r2, r7, r1
    eor r2, r2, r0
    and r2, r2, #0x10
    orr r8, r8, r2
    cmp r10, #0
    orrne r8, r8, #4
    tst r8, #0x10
    subne r0, r0, #1
    and r1, r0, #8
    orr r8, r8, r1
    tst r0, #2
    orrne r8, r8, #0x20
    subs r6, r6, #16
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_AA:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    mov r1, r0
    mov r0, r9
    mov r2, pc
    ldr pc, =z80a_wr8
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    subs r6, r6, #16
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_AB:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    subs r6, r6, #16
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_B1:
    subs r6, r6, #16
.Lz34:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    sub r0, r7, r1
    and r0, r0, #255
    and r8, r8, #1
    orr r8, r8, #2
    add r2, lr, #256
    ldrb r2, [r2, r0]
    bic r2, r2, #0x28
    orr r8, r8, r2
    eor r2, r7, r1
    eor r2, r2, r0
    and r2, r2, #0x10
    orr r8, r8, r2
    cmp r10, #0
    orrne r8, r8, #4
    tst r8, #0x10
    subne r0, r0, #1
    and r1, r0, #8
    orr r8, r8, r1
    tst r0, #2
    orrne r8, r8, #0x20
    cmp r10, #0
    beq .Lz35
    tst r8, #0x40
    bne .Lz35
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz34
1:
    sub r5, r5, #2
    ldr pc, =z80a_exit
.Lz35:
    cmp r6, #0
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_B2:
    subs r6, r6, #16
.Lz36:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    mov r1, r0
    mov r0, r9
    mov r2, pc
    ldr pc, =z80a_wr8
    add r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    tst r10, #0xFF00
    beq .Lz37
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz36
1:
    sub r5, r5, #2
    ldr pc, =z80a_exit
.Lz37:
    cmp r6, #0
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_B8:
    subs r6, r6, #16
.Lz40:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    add r0, r1, r7
    str r0, [r4, #388]
    mov r0, r11
    mov r2, pc
    ldr pc, =z80a_wr8
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r11, r11, #1
    mov r11, r11, lsl #16
    mov r11, r11, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    ldr r1, [r4, #388]
    and r8, r8, #0xC1
    cmp r10, #0
    orrne r8, r8, #4
    and r0, r1, #8
    orr r8, r8, r0
    tst r1, #2
    orrne r8, r8, #0x20
    cmp r10, #0
    beq .Lz41
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz40
1:
    sub r5, r5, #2
    ldr pc, =z80a_exit
.Lz41:
    cmp r6, #0
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_B9:
    subs r6, r6, #16
.Lz42:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #1
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    sub r0, r7, r1
    and r0, r0, #255
    and r8, r8, #1
    orr r8, r8, #2
    add r2, lr, #256
    ldrb r2, [r2, r0]
    bic r2, r2, #0x28
    orr r8, r8, r2
    eor r2, r7, r1
    eor r2, r2, r0
    and r2, r2, #0x10
    orr r8, r8, r2
    cmp r10, #0
    orrne r8, r8, #4
    tst r8, #0x10
    subne r0, r0, #1
    and r1, r0, #8
    orr r8, r8, r1
    tst r0, #2
    orrne r8, r8, #0x20
    cmp r10, #0
    beq .Lz43
    tst r8, #0x40
    bne .Lz43
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz42
1:
    sub r5, r5, #2
    ldr pc, =z80a_exit
.Lz43:
    cmp r6, #0
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_BA:
    subs r6, r6, #16
.Lz44:
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_in_call
    mov r1, r0
    mov r0, r9
    mov r2, pc
    ldr pc, =z80a_wr8
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    tst r10, #0xFF00
    beq .Lz45
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz44
1:
    sub r5, r5, #2
    ldr pc, =z80a_exit
.Lz45:
    cmp r6, #0
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
ed_BB:
    subs r6, r6, #16
.Lz46:
    mov r2, r9, lsr #10
    ldr r2, [r4, r2, lsl #2]
    ldrb r1, [r2, r9]
    sub r10, r10, #0x100
    mov r10, r10, lsl #16
    mov r10, r10, lsr #16
    and r0, r10, #255
    mov lr, pc
    ldr pc, =z80a_out_call
    sub r9, r9, #1
    mov r9, r9, lsl #16
    mov r9, r9, lsr #16
    bic r8, r8, #0x40
    orr r8, r8, #2
    tst r10, #0xFF00
    orreq r8, r8, #0x40
    tst r10, #0xFF00
    beq .Lz47
    subs r6, r6, #5
    ble 1f
    sub r6, r6, #16
    b .Lz46
1:
    sub r5, r5, #2
    ldr pc, =z80a_exit
.Lz47:
    cmp r6, #0
    ldrgt pc, =z80a_dispatch
    ldr pc, =z80a_exit
.ltorg
.ltorg
.section .hotdata.z80a,"aw",%progbits
.align 2
.global z80a_main_table
z80a_main_table:
.word op_00
.word op_01
.word op_02
.word op_03
.word op_04
.word op_05
.word op_06
.word op_07
.word op_08
.word op_09
.word op_0A
.word op_0B
.word op_0C
.word op_0D
.word op_0E
.word op_0F
.word op_10
.word op_11
.word op_12
.word op_13
.word op_14
.word op_15
.word op_16
.word op_17
.word op_18
.word op_19
.word op_1A
.word op_1B
.word op_1C
.word op_1D
.word op_1E
.word op_1F
.word op_20
.word op_21
.word op_22
.word op_23
.word op_24
.word op_25
.word op_26
.word op_27
.word op_28
.word op_29
.word op_2A
.word op_2B
.word op_2C
.word op_2D
.word op_2E
.word op_2F
.word op_30
.word op_31
.word op_32
.word op_33
.word op_34
.word op_35
.word op_36
.word op_37
.word op_38
.word op_39
.word op_3A
.word op_3B
.word op_3C
.word op_3D
.word op_3E
.word op_3F
.word op_40
.word op_41
.word op_42
.word op_43
.word op_44
.word op_45
.word op_46
.word op_47
.word op_48
.word op_49
.word op_4A
.word op_4B
.word op_4C
.word op_4D
.word op_4E
.word op_4F
.word op_50
.word op_51
.word op_52
.word op_53
.word op_54
.word op_55
.word op_56
.word op_57
.word op_58
.word op_59
.word op_5A
.word op_5B
.word op_5C
.word op_5D
.word op_5E
.word op_5F
.word op_60
.word op_61
.word op_62
.word op_63
.word op_64
.word op_65
.word op_66
.word op_67
.word op_68
.word op_69
.word op_6A
.word op_6B
.word op_6C
.word op_6D
.word op_6E
.word op_6F
.word op_70
.word op_71
.word op_72
.word op_73
.word op_74
.word op_75
.word op_76
.word op_77
.word op_78
.word op_79
.word op_7A
.word op_7B
.word op_7C
.word op_7D
.word op_7E
.word op_7F
.word op_80
.word op_81
.word op_82
.word op_83
.word op_84
.word op_85
.word op_86
.word op_87
.word op_88
.word op_89
.word op_8A
.word op_8B
.word op_8C
.word op_8D
.word op_8E
.word op_8F
.word op_90
.word op_91
.word op_92
.word op_93
.word op_94
.word op_95
.word op_96
.word op_97
.word op_98
.word op_99
.word op_9A
.word op_9B
.word op_9C
.word op_9D
.word op_9E
.word op_9F
.word op_A0
.word op_A1
.word op_A2
.word op_A3
.word op_A4
.word op_A5
.word op_A6
.word op_A7
.word op_A8
.word op_A9
.word op_AA
.word op_AB
.word op_AC
.word op_AD
.word op_AE
.word op_AF
.word op_B0
.word op_B1
.word op_B2
.word op_B3
.word op_B4
.word op_B5
.word op_B6
.word op_B7
.word op_B8
.word op_B9
.word op_BA
.word op_BB
.word op_BC
.word op_BD
.word op_BE
.word op_BF
.word op_C0
.word op_C1
.word op_C2
.word op_C3
.word op_C4
.word op_C5
.word op_C6
.word op_C7
.word op_C8
.word op_C9
.word op_CA
.word op_CB
.word op_CC
.word op_CD
.word op_CE
.word op_CF
.word op_D0
.word op_D1
.word op_D2
.word op_D3
.word op_D4
.word op_D5
.word op_D6
.word op_D7
.word op_D8
.word op_D9
.word op_DA
.word op_DB
.word op_DC
.word op_DD
.word op_DE
.word op_DF
.word op_E0
.word op_E1
.word op_E2
.word op_E3
.word op_E4
.word op_E5
.word op_E6
.word op_E7
.word op_E8
.word op_E9
.word op_EA
.word op_EB
.word op_EC
.word op_ED
.word op_EE
.word op_EF
.word op_F0
.word op_F1
.word op_F2
.word op_F3
.word op_F4
.word op_F5
.word op_F6
.word op_F7
.word op_F8
.word op_F9
.word op_FA
.word op_FB
.word op_FC
.word op_FD
.word op_FE
.word op_FF
.section .rodata.z80a,"a",%progbits
.align 2
xy_table:
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_09
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_19
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_21
.word xy_22
.word xy_23
.word xy_24
.word xy_25
.word xy_26
.word xy_plain
.word xy_plain
.word xy_29
.word xy_2A
.word xy_2B
.word xy_2C
.word xy_2D
.word xy_2E
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_34
.word xy_35
.word xy_36
.word xy_plain
.word xy_plain
.word xy_39
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_44
.word xy_45
.word xy_46
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_4C
.word xy_4D
.word xy_4E
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_54
.word xy_55
.word xy_56
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_5C
.word xy_5D
.word xy_5E
.word xy_plain
.word xy_60
.word xy_61
.word xy_62
.word xy_63
.word xy_64
.word xy_65
.word xy_66
.word xy_67
.word xy_68
.word xy_69
.word xy_6A
.word xy_6B
.word xy_6C
.word xy_6D
.word xy_6E
.word xy_6F
.word xy_70
.word xy_71
.word xy_72
.word xy_73
.word xy_74
.word xy_75
.word xy_plain
.word xy_77
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_7C
.word xy_7D
.word xy_7E
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_84
.word xy_85
.word xy_86
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_8C
.word xy_8D
.word xy_8E
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_94
.word xy_95
.word xy_96
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_9C
.word xy_9D
.word xy_9E
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_A4
.word xy_A5
.word xy_A6
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_AC
.word xy_AD
.word xy_AE
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_B4
.word xy_B5
.word xy_B6
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_BC
.word xy_BD
.word xy_BE
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_CB
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_E1
.word xy_plain
.word xy_E3
.word xy_plain
.word xy_E5
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_E9
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_F9
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
.word xy_plain
ed_table:
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_40
.word ed_41
.word ed_42
.word ed_43
.word ed_44
.word ed_45
.word ed_46
.word ed_47
.word ed_48
.word ed_49
.word ed_4A
.word ed_4B
.word ed_4C
.word ed_4D
.word ed_4E
.word ed_4F
.word ed_50
.word ed_51
.word ed_52
.word ed_53
.word ed_54
.word ed_55
.word ed_56
.word ed_57
.word ed_58
.word ed_59
.word ed_5A
.word ed_5B
.word ed_5C
.word ed_5D
.word ed_5E
.word ed_5F
.word ed_60
.word ed_61
.word ed_62
.word ed_63
.word ed_64
.word ed_65
.word ed_66
.word ed_67
.word ed_68
.word ed_69
.word ed_6A
.word ed_6B
.word ed_6C
.word ed_6D
.word ed_6E
.word ed_6F
.word ed_70
.word ed_71
.word ed_72
.word ed_73
.word ed_74
.word ed_75
.word ed_76
.word ed_nop
.word ed_78
.word ed_79
.word ed_7A
.word ed_7B
.word ed_7C
.word ed_7D
.word ed_7E
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_A0
.word ed_A1
.word ed_A2
.word ed_A3
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_A8
.word ed_A9
.word ed_AA
.word ed_AB
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_B0
.word ed_B1
.word ed_B2
.word ed_B3
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_B8
.word ed_B9
.word ed_BA
.word ed_BB
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.word ed_nop
.section .hotdata.z80a,"aw",%progbits
.align 2
cb_get:
.word .Lcbg0
.word .Lcbg1
.word .Lcbg2
.word .Lcbg3
.word .Lcbg4
.word .Lcbg5
.word .Lcb_mem
.word .Lcbg7
cb_put:
.word .Lcbp0
.word .Lcbp1
.word .Lcbp2
.word .Lcbp3
.word .Lcbp4
.word .Lcbp5
.word .Lcbp6
.word .Lcbp7
cb_ops:
.word .Lcbo0
.word .Lcbo1
.word .Lcbo2
.word .Lcbo3
.word .Lcbo4
.word .Lcbo5
.word .Lcbo6
.word .Lcbo7
.word .Lcbo8
.word .Lcbo9
.word .Lcbo10
.word .Lcbo11
.word .Lcbo12
.word .Lcbo13
.word .Lcbo14
.word .Lcbo15
.word .Lcbo16
.word .Lcbo17
.word .Lcbo18
.word .Lcbo19
.word .Lcbo20
.word .Lcbo21
.word .Lcbo22
.word .Lcbo23
.word .Lcbo24
.word .Lcbo25
.word .Lcbo26
.word .Lcbo27
.word .Lcbo28
.word .Lcbo29
.word .Lcbo30
.word .Lcbo31
xcb_ops:
.word .Lxco0
.word .Lxco1
.word .Lxco2
.word .Lxco3
.word .Lxco4
.word .Lxco5
.word .Lxco6
.word .Lxco7
.word .Lxco8
.word .Lxco9
.word .Lxco10
.word .Lxco11
.word .Lxco12
.word .Lxco13
.word .Lxco14
.word .Lxco15
.word .Lxco16
.word .Lxco17
.word .Lxco18
.word .Lxco19
.word .Lxco20
.word .Lxco21
.word .Lxco22
.word .Lxco23
.word .Lxco24
.word .Lxco25
.word .Lxco26
.word .Lxco27
.word .Lxco28
.word .Lxco29
.word .Lxco30
.word .Lxco31
xcb_copy:
.word .Lxcc0
.word .Lxcc1
.word .Lxcc2
.word .Lxcc3
.word .Lxcc4
.word .Lxcc5
.word .Lxcc6
.word .Lxcc7
.section .hotdata.z80actx,"aw",%progbits
.align 4
.global z80a_ctx
    z80a_ctx: .space 2048
