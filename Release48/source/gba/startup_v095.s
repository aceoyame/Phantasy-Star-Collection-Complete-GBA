.syntax unified
.cpu arm7tdmi
.arm
.section .gba_header,"ax",%progbits
.global _rom_start
_rom_start:
    b _start
    .incbin "nintendo_logo.bin"
    .ascii "PS4RC095    "      @ 12-byte title
    .ascii "P495"              @ game code
    .ascii "00"                @ maker
    .byte 0x96                 @ fixed value
    .byte 0x00                 @ unit code
    .byte 0x00                 @ device type
    .space 7,0                 @ reserved
    .byte 0x00                 @ version
    .byte 0x00                 @ checksum patched post-link
    .space 2,0

.section .text.start,"ax",%progbits
.align 2
.global _start
.type _start,%function
_start:
    ldr sp, =0x03007F00
    /* PSC's native PSII/PSIII runtime enables GamePak prefetch + WS0 3/1.
       Do it before copying our own IWRAM image so even startup ROM reads use
       the fast policy; $4017 retains 8-cycle SRAM timing. */
    ldr r0, =0x04000204
    ldr r1, =0x00004017
    strh r1, [r0]
    mov r0, #0
    msr cpsr_c, #0x1F          @ System mode, IRQ/FIQ enabled state irrelevant here
    ldr sp, =0x03007F00
    /* Copy .iwram.text load image from ROM to on-chip WRAM before any SRAM read. */
    ldr r0, =__iwram_lma_start
    ldr r1, =__iwram_vma_start
    ldr r2, =__iwram_vma_end
0:
    cmp r1, r2
    bhs 2f
    ldrb r3, [r0], #1
    strb r3, [r1], #1
    b 0b
2:
    /* Copy initialized .data from ROM into EWRAM. */
    ldr r0, =__data_lma_start
    ldr r1, =__data_vma_start
    ldr r2, =__data_vma_end
3:
    cmp r1, r2
    bhs 4f
    ldrb r3, [r0], #1
    strb r3, [r1], #1
    b 3b
4:
    /* Zero .bss in EWRAM. Mutable C globals must never live in Game Pak ROM. */
    ldr r1, =__bss_start
    ldr r2, =__bss_end
    mov r3, #0
5:
    cmp r1, r2
    bhs 6f
    strb r3, [r1], #1
    b 5b
6:
    bl gba_main
1:  b 1b
