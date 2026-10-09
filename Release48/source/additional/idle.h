/* Fold only a side-effect-free TST.W RAM / BNE tight wait, within one
 * scanline deadline. Registers, PC, flags and total cycles match interpreting
 * every pair. RAM cannot change without CPU writes or an interrupt; neither
 * occurs inside this pair. VDP lines and interrupts still execute normally.
 */
static void md_fold_ram_wait(uint64_t end){
 uint32_t pc=C.pc;if(pc!=0x3446)return;
 if(m68k_read16(pc)!=0x4a78 || m68k_read16(pc+4)!=0x66fa)return;
 uint32_t addr=(uint32_t)(int32_t)(int16_t)m68k_read16(pc+2)&0xffffff;
 if(addr<0xe00000 || !m68k_read16(addr))return;
 uint64_t before=C.cycles;m68k_step();
 if(C.cycles>=end)return;
 m68k_step();
 if(C.pc==pc){uint32_t period=(uint32_t)(C.cycles-before);if(period&&end>C.cycles)C.cycles+=((uint32_t)(end-C.cycles)/period)*period;}
}
