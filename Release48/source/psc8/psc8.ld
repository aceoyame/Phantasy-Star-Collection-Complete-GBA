ENTRY(psc8_start)
SECTIONS {
 . = 0x086CA500;
 .rom : { *(.start) *(.text*) *(.rodata*) } 
 . = ALIGN(4); __hot_load = .;
 .hot 0x03000000 : AT(__hot_load) { __hot_start = .; *(.hot*) *(.hotdata*) __hot_end = ALIGN(4); }
 __data_load = __hot_load + SIZEOF(.hot);
 .data 0x02000000 : AT(__data_load) { __data_start = .; *(.data*) __data_end = ALIGN(4); }
 .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = ALIGN(4); }
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.eh_frame*) }
 ASSERT(__hot_end < 0x03007980, "PSC8 IWRAM code overflow")
 ASSERT(__bss_end < 0x0203E000, "PSC8 EWRAM overflow")
 ASSERT(__data_load + SIZEOF(.data) < 0x08738000, "PSC8 exceeds PSII padding")
}
