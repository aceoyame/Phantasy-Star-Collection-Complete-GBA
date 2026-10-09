.syntax unified
.cpu arm7tdmi
.arm
.section .gba_header,"ax",%progbits
.global _rom_start
_rom_start:
    b _start
    .incbin "nintendo_logo.bin"
    .ascii "PS4RC098    "      @ 12-byte title
    .ascii "P498"              @ game code
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
    /* Warm-launch contract for the PSC fourth-game module. The collection may
       leave IRQs, HBlank/FIFO DMA, timers and sound live. Quiesce every shared
       hardware owner before touching IWRAM/EWRAM, then stay polling-only. */
    msr cpsr_c, #0xDF          @ System mode, IRQ+FIQ masked
    ldr sp, =__stack_top
    /* R14B: dedicated IRQ-mode stack for the VBlank-clocked audio IRQ.
       Never inherit PSC's/BIOS SP_irq (0x03007FA0: only 0xA0 bytes above the
       System stack top). */
    msr cpsr_c, #0xD2          @ IRQ mode, IRQ+FIQ masked
    ldr sp, =__irq_stack_top
    msr cpsr_c, #0xDF          @ back to System mode
    mov r1, #0
    ldr r0, =0x04000208        @ IME
    strh r1, [r0]
    ldr r0, =0x04000200        @ IE
    strh r1, [r0]
    mvn r1, #0
    ldr r0, =0x04000202        @ IF: acknowledge all pending sources
    strh r1, [r0]
    mov r1, #0
    ldr r0, =0x040000BA        @ DMA0CNT_H
    strh r1, [r0]
    ldr r0, =0x040000C6        @ DMA1CNT_H
    strh r1, [r0]
    ldr r0, =0x040000D2        @ DMA2CNT_H
    strh r1, [r0]
    ldr r0, =0x040000DE        @ DMA3CNT_H
    strh r1, [r0]
    ldr r0, =0x04000102        @ timers 0..3 control
    strh r1, [r0]
    strh r1, [r0, #4]
    strh r1, [r0, #8]
    strh r1, [r0, #12]
    ldr r0, =0x04000084        @ SOUNDCNT_X
    strh r1, [r0]
    mov r1, #0x80              @ force blank before memory/palette ownership changes
    ldr r0, =0x04000000
    strh r1, [r0]
    /* PSC native modules use prefetch + WS0 3/1. Re-establish it explicitly. */
    ldr r0, =0x04000204
    ldr r1, =0x00004017
    strh r1, [r0]
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
