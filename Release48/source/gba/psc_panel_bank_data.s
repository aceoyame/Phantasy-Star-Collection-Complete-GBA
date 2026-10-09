    .section .rodata.psc_panel_bank,"a",%progbits
    .balign 4
    .global ps4_psc_panel_bank
    .type ps4_psc_panel_bank, %object
ps4_psc_panel_bank:
    .incbin "gba/psc_panel_bank.bin"
    .size ps4_psc_panel_bank, .-ps4_psc_panel_bank
