/* Native shared-driver routines. Short budgets and unusual RAM addresses
 * retain the original translated path. No guest clocks are scaled. */
typedef struct {
  u32 a,f,bc,hl,ix,sp,cyc,r,pc;
#ifdef MD_HOST
  u32 de,iy;
#endif
} NativeNote;
static NativeNote native_note HOTDATA;
SND_ARM __attribute__((noinline)) static void native_note_run(u32 tend);
static HOT SND_ARM __attribute__((noinline)) void native_ym_run(void);
