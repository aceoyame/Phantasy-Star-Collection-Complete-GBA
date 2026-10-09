from pathlib import Path
import shutil
W=Path(__file__).resolve().parents[1];A=W/'Release48/source/additional';D=W/'native58';p=A/'mdrender.c'
s=(D/'opaque_mdrender.c').read_text()
s=s.replace('static u32 window_cover;','''static u32 window_cover;
/* CP58: previous exact coverage of sampled guest lines, and every window
 * tile dependency. Only background-only updates may use this proof. */
static u32 hidden_window_lines[8],window_tiles[64];
ROMFN __attribute__((noinline)) static void build_window_tiles(u32 width){
 for(u32 w=0;w<64u;w++)window_tiles[w]=0;
 u32 base=((u32)(M.reg[3]&0x3eu))<<10,ww=(width==320u)?64u:32u;
 for(u32 row=0;row<32u;row++)for(u32 k=0;k<(width>>3);k++){
  u32 tile=VWA(base+(row*ww+k)*2u)&2047u;window_tiles[tile>>5]|=1u<<(tile&31u);
 }
}
''')
s=s.replace('static void render_line(s32 y,u32 width){','''static void render_line(s32 y,u32 width){
  hidden_window_lines[(u32)y>>5]&=~(1u<<((u32)y&31u));''')
s=s.replace('if(win&&wx0==0u&&wx1>=width&&window_cover){plb.any=0;plb.f=0;}', '''if(win&&wx0==0u&&wx1>=width&&window_cover){
    plb.any=0;plb.f=0;
    if(!shas)hidden_window_lines[(u32)y>>5]|=1u<<((u32)y&31u);
  }''')
s=s.replace('u32 anyt=0, sprchg=0;', 'u32 anyt=0, sprchg=0,window_changed=0;')
s=s.replace('if(woffset<wsz){ u32 e0=', 'if(woffset<wsz){ window_changed=1;u32 e0=')
# The last return belongs to collect_dirty, immediately before rendering state.
a=s.index('static ROMFN COLD_THUMB');b=s.index('ROMFN void mdr_render',a);at=s.rfind('  return 0;',a,b);assert at>=0
s=s[:at]+'''  /* No new window name/texture data and no visible or changed sprite:
   * plane updates cannot affect pixels hidden by the proven window. Only
   * newly dirty lines are removed; both pages' earlier pending bits remain. */
  u32 wdirty=window_changed;
  for(u32 w=0;w<64u;w++)wdirty|=tdirty[w]&window_tiles[w];
  if(!wdirty&&!sprchg){
    u32 visible=0;
    for(u32 i=0;i<nspr;i++)if(spr[i].top<(s32)height&&spr[i].top+(s32)spr[i].h*8>0){visible=1;break;}
    if(!visible)for(u32 w=0;w<8u;w++)dl[w]&=~hidden_window_lines[w];
  }
  if(window_changed)build_window_tiles((M.reg[12]&1u)?320u:256u);
'''+s[at:]
s=s.replace('if(full){ tile_index_valid=0;', 'if(full){ for(u32 w=0;w<8u;w++)hidden_window_lines[w]=0;build_window_tiles(width);tile_index_valid=0;')
s=s.replace('  Spr save[80]; u32 ns=nspr;', '  u32 saved_hidden[8];for(u32 w=0;w<8u;w++)saved_hidden[w]=hidden_window_lines[w];\n  Spr save[80]; u32 ns=nspr;')
s=s.replace('for(u32 i=0;i<ns;i++) spr[i]=save[i]; nspr=ns;','for(u32 i=0;i<ns;i++) spr[i]=save[i]; nspr=ns;\n  for(u32 w=0;w<8u;w++)hidden_window_lines[w]=saved_hidden[w];')
p.write_text(s)
