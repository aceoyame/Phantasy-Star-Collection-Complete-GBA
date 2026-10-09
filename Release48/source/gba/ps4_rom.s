.syntax unified
.section .rodata.ps4rom,"a",%progbits
.align 2
.global ps4_reversed_rom
.type ps4_reversed_rom,%object
ps4_reversed_rom:
    .incbin "ps4_reversed.bin"
.size ps4_reversed_rom, .-ps4_reversed_rom
