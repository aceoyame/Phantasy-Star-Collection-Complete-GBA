    .section .rodata.ps4_precomp,"a",%progbits
    .balign 4
    .global ps4_precomp_assets
    .type ps4_precomp_assets, %object
ps4_precomp_assets:
    .incbin "gba/precomp_assets.bin"
    .size ps4_precomp_assets, .-ps4_precomp_assets
    .balign 4
    .global r45_pd_art_bank
    .type r45_pd_art_bank, %object
r45_pd_art_bank:
    .incbin "gba/r45_pd_art.bin"
    .size r45_pd_art_bank, .-r45_pd_art_bank
