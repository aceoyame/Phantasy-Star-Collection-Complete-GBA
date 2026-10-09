    .section .rodata.psc_field_predecode,"a",%progbits
    .balign 4
    .global ps4_psc_field_predecode
    .type ps4_psc_field_predecode, %object
ps4_psc_field_predecode:
    .incbin "gba/psc_field_predecode.bin"
    .size ps4_psc_field_predecode, .-ps4_psc_field_predecode
