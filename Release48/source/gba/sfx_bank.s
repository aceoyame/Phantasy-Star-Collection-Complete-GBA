    .section .rodata.ps4_sfx_pcm,"a",%progbits
    .balign 4
    .global ps4_sfx_bank_start, ps4_sfx_bank_end
ps4_sfx_bank_start:
    .incbin "gba/pcm_sfx/ps4_sfx_bank.pcm"
ps4_sfx_bank_end:
    .balign 4
