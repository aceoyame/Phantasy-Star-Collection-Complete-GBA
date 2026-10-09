"""Actual shared IWRAM span kernel with unused branch fixtures for PD tests."""
import re
from pathlib import Path
def pd_span_support(source,fn):
 job=re.search(r'struct battle_span_job\s*\{.*?\};',source,re.S)[0]
 clips=(Path(__file__).resolve().parents[1]/'gba/r46_battle_clips_runtime.h').read_text()
 match=re.search(r'^static void r46_comb_odd\([^;]*?\)\s*\{',clips,re.M);assert match
 p=match.end();depth=1
 while depth:depth+=(clips[p]=='{')-(clips[p]=='}');p+=1
 odd=clips[match.start():p]
 return r'''
static void dma_copy16(const void*s,void*d,unsigned n){assert(!(n&1));memcpy(d,s,n);}
typedef uint16_t battle_u16_alias __attribute__((may_alias));
#define BATTLE_ENEMY_RESOLVED_TILES 288u
#define BATTLE_ENEMY_TILE_TRANSPARENT 0xFFFFu
#define BATTLE_ENEMY_TILE_SOURCE 0x8000u
static uint16_t battle_enemy_resolved_tiles[4][288];
static uint8_t battle_span_lx[240],battle_span_run_x0[48],battle_span_run_n[48],battle_span_run_lx0[48];
static uint16_t battle_span_run_tx[48];
static uint32_t battle_enemy_death_row_mask(unsigned s,unsigned e,unsigned t,unsigned r){(void)s;(void)e;(void)t;(void)r;return 0xFFFFFFFFu;}
'''+job+'\n'+fn('battle_enemy_span_rows')+'\n'+odd+'\n'
