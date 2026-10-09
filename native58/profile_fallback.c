#define main old_main
#include "/workspace/scratch/06a78a973e00/work57/Release48/source/tools_host/md_host/mdhost.c"
#undef main

void mdsnd_ym_68k(u32 a,u32 v){}void mdsnd_psg_68k(u32 v){}void mdsnd_busreq(u32 v){}void mdsnd_reset(u32 v){}
u32 count_pc[1<<24],count_op[65536],count_on;
int main(int argc,char**argv){boot(argv[1]);for(u32 f=0;f<450;f++){
 count_on=f>=150;
 u32 keys=f>=180&&f<184?K_START:f>=400&&f<404?K_B:0;
 md_begin_frame_input(keys);md_run(0);}
 for(u32 k=0;k<40;k++){u32 bi=0;for(u32 i=0;i<(1<<24);i++)if(count_pc[i]>count_pc[bi])bi=i;
 printf("PC %06x %u\n",bi,count_pc[bi]);count_pc[bi]=0;}
 for(u32 k=0;k<15;k++){u32 bi=0;for(u32 i=0;i<65536;i++)if(count_op[i]>count_op[bi])bi=i;
 printf("OP %04x %u\n",bi,count_op[bi]);count_op[bi]=0;}return 0;}
