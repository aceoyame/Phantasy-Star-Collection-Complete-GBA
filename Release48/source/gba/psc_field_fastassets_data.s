    .section .rodata.psc_field_fastassets,"a",%progbits
    .balign 32
    .global ps4_psc_field_fastassets
    .type ps4_psc_field_fastassets, %object
ps4_psc_field_fastassets:
    .incbin "gba/psc_field_fastassets.bin"
    .size ps4_psc_field_fastassets, .-ps4_psc_field_fastassets
