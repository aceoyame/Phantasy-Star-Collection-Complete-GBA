    .section .rodata.title_intro_rows,"a",%progbits
    .balign 4
    .global ps4_title_intro_rows
    .type ps4_title_intro_rows, %object
ps4_title_intro_rows:
    .incbin "gba/title_intro_rows.bin"
    .size ps4_title_intro_rows, .-ps4_title_intro_rows
