.section .rodata.r46_recorded,"a",%progbits
.balign 4
.global r46_recorded_tables
r46_recorded_tables:
.incbin "gba/r46_recorded_clips.bin"
.balign 4
.global r46_tile_pool
r46_tile_pool:
.incbin "gba/r46_recorded_tiles.bin"
.balign 4
