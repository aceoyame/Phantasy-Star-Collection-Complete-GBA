ENTRY(test_start)
SECTIONS {
 . = 0x080000C0;
 .rom : { *(.start) *(.text*) *(.rodata*) }
 . = ALIGN(4); __hot_load = .;
 .hot 0x03000000 : AT(__hot_load) { __hot_start = .; *(.hot*) *(.hotdata*) __hot_end = ALIGN(4); }
 __data_load = __hot_load + SIZEOF(.hot);
 .data 0x02000000 : AT(__data_load) { __data_start = .; *(.data*) __data_end = ALIGN(4); }
 .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = ALIGN(4); }
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.eh_frame*) }
}
