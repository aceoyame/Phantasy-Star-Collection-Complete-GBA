/* R235 field-camera live-path canary; same instructions as the R86
   source/party86.s shim (verified byte-identical against accepted R235).
   Relocate from 08CB9F00 to retired PCM sample 42 gap 08F97D64. */
.syntax unified
.cpu arm7tdmi
.arm
.equ DISPCNT,0x04000000
.section .r235_field_hook_probe,"ax",%progbits
.global r235_field_hook_probe
r235_field_hook_probe:
    push {r0,r1}
    tst r5,#0x200
    beq no_active_display
    mov r0,#DISPCNT
    ldrh r0,[r0,#6]
    cmp r0,#160
    bhs no_active_display
    mov r3,#0x208
    orr r3,r3,#DISPCNT
    ldrh r2,[r3]
    mov r0,#0
    strh r0,[r3]
    ldr r1,[r4]
    str r0,[r4]
    tst r1,#2
    beq no_audio_drain
    push {r2,r3}
    ldr r0,=0x088664e4
    mov lr,pc
    bx r0
    pop {r2,r3}
no_audio_drain:
    strh r2,[r3]
    mov r5,#0
    mov r0,#1
    str r0,[r4]
no_active_display:
    pop {r0,r1}
    ldr lr,=0x08cd3858
    ldr pc,=0x08cb9374
    .ltorg
