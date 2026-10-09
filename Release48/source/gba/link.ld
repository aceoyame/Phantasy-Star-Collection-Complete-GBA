ENTRY(_rom_start)
MEMORY {
  ROM   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
  EWRAM (rwx) : ORIGIN = 0x02024000, LENGTH = 24K
  IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
}

/* PSCSTYLE audit: keep code and the System stack disjoint by construction.
 * SP intentionally remains below the top 0x100 bytes used as a guard. */
__stack_top = 0x03007F00;
__stack_reserve = 0x00000C00;
__stack_bottom = __stack_top - __stack_reserve;
/* R14B: IRQ-mode stack directly below the System stack (audio IRQ chain
 * measured at ~1.0 KiB: service 40 + mix_one_frame 56 + music_decode 40 +
 * mix_psc_mono 736 + update/process/note 88 + BIOS 24). */
__irq_stack_top = __stack_bottom;
__irq_stack_reserve = 0x00000600;
__irq_stack_bottom = __irq_stack_top - __irq_stack_reserve;
SECTIONS {
  . = ORIGIN(ROM);
  .gba_header : { KEEP(*(.gba_header)) } > ROM
  .text : { *(.text.start) *(.text*) KEEP(*(.rodata.saveid)) *(.rodata*) } > ROM

  .iwram : ALIGN(4) {
    __iwram_vma_start = .;
    *(.iwram.text*)
    /* CP22: P134R sequencer state (~2.6 KB, measured .data 2536 + .bss 60 with
     * 32-bit pointers) lives in IWRAM: EWRAM BSS had only 409 B before the fixed
     * presentation scratch.  Copied from ROM with the rest of .iwram at boot;
     * the ASSERT below still guards the reserved System stack. */
    *chip_audio.o(.data* .bss*)
    *(.iwram.audio*)   /* P134R hot mixer when built with PS4_P134R_MIX_IWRAM=1 */
    *(.iwram.battle*)  /* R15 battle span-kernel plan tables */
    __iwram_vma_end = .;
  } > IWRAM AT> ROM
  ASSERT(__iwram_vma_end <= __irq_stack_bottom, "IWRAM code overlaps reserved IRQ/System stacks")
  __iwram_lma_start = LOADADDR(.iwram);

  .data : ALIGN(4) {
    __data_vma_start = .;
    *(.data*)
    __data_vma_end = .;
  } > EWRAM AT> ROM
  __data_lma_start = LOADADDR(.data);

  .bss (NOLOAD) : ALIGN(4) {
    __bss_start = .;
    *(.bss*) *(COMMON)
    __bss_end = .;
  } > EWRAM
  __presentation_scratch_start = 0x0202A000;
  ASSERT(__bss_end <= __presentation_scratch_start, "EWRAM BSS overlaps presentation scratch")

  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*) }
}
