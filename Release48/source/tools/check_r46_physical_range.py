#!/usr/bin/env python3
"""Check actual ATTACK C against original target/hit/damage ordering.

The oracle lowers the original ASM's right-hand-first range selection,
loc_B6A2 hit-flag pass and subsequent Character_DamageEnemy pass. RNG words
are deterministic fixtures; this does not model source/GBA hardware counters.
"""
from pathlib import Path
import json, os, random, re, subprocess

R = Path(__file__).resolve().parents[1]
O = R.parent / 'build_artifacts'
source = (R / 'gba/title_runtime_v06.c').read_text()
rom = (R / 'references/roms/ps4built.bin').read_bytes()
asm = (R / 'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text(encoding='latin1')
assert 'Battle_AttackCommand:' in asm and 'loc_B6A2:' in asm
assert 'cmpi.b\t#4, d0\t; is weapon multi-target, 2 handed (guns)?' in asm
assert 'cmpi.b\t#1, d0\n\tbne.s\tloc_B754\n\tclr.b\td0' in asm

def fn(name):
    m = re.search(r'^static[^\n]*\b' + name + r'\([^;\n]*\)\{', source, re.M)
    assert m, name
    p, depth = m.end(), 1
    while depth:
        depth += (source[p] == '{') - (source[p] == '}')
        p += 1
    return source[m.start():p] + '\n'

def item_type(item):
    return rom[0x2a8e28 + (item-1)*22 + 10] if item else 0

def element(item):
    return rom[0x2a8e28 + (item-1)*22 + 18] if item else 0

guns = [0x2b, 0x2e, 0x3b, 0x3e, 0x3f, 0x4c, 0x54, 0x5f, 0x6e, 0x79]
assert all(0x16 <= rom[0x883a + i] <= 0x1c for i in guns)
assert [i for i in guns if item_type(i) == 4] == [0x3f, 0x54]

fixtures = []
def add(cid, rh, lh, live, selected, mode, count=4, status=0):
    # Explicit source right/left decision tree, rather than a native helper.
    rt, lt = item_type(rh), item_type(lh)
    first_type = rt if 1 <= rt <= 4 else lt if 1 <= lt <= 4 else 0
    multi = first_type in (2, 4)
    hp = [v if live & (1 << i) and i < count else 0 for i, v in enumerate((1, 47, 600, 999))]
    st = [status if i == 2 else 0 for i in range(4)]
    atk, dex = ((300, 80), (1700, 255), (30, 1))[mode]
    agi = ((10, 50, 100, 200), (0, 0, 0, 0), (255, 255, 255, 255))[mode]
    props = [[(i + j) % 5 for j in range(14)] for i in range(4)]
    stream = [random.Random(len(fixtures)*17 + j*911 + 3).randrange(65536) for j in range(80)]
    cursor = 0
    def rng():
        nonlocal cursor
        v = stream[cursor]; cursor += 1
        return v
    targets = [i for i in range(count) if hp[i] and not st[i] & 0x44] if multi else [selected]
    if not multi and (selected >= count or not hp[selected]):
        # Single-range retargeting is separately tested on the production core.
        return
    hits = {}
    for i in targets:
        score = 2 * ((rng() & 63) + dex - agi[i])
        hits[i] = -1 if score <= 8 else 0 if multi or score <= 116 else 1
    after, reactions = hp.copy(), [0]*4
    for i, hit in hits.items():
        if hit < 0:
            continue
        factors = [props[i][element(it)-1] for it in (rh, lh) if it and item_type(it) != 5 and 1 <= element(it) <= 14]
        factor = max(factors, default=0)
        power = (atk % 256)//4 if hit else 0
        # Register-width steps from Battle_CalculateDamage + loc_266C.
        total = 8 + sum(rng() % 8 for _ in range(16))
        word = (total * atk) % 65536
        word = ((word // 64) + atk + 2*power) % 65536
        word = (word * factor) % 65536
        word = (word // 4 - (i*13)) % 65536
        signed = word if word < 32768 else word - 65536
        damage = min(999, max(1, signed))
        after[i] = max(0, hp[i] - damage)
        reactions[i] = 1
    fixtures.append({'cid':cid,'rh':rh,'lh':lh,'count':count,'target':selected,
                     'atk':atk,'dex':dex,'agi':agi,'hp':hp,'status':st,'props':props,
                     'stream':stream,'after':after,'reactions':reactions,
                     'rng_calls':cursor,'range':2 if multi else 1,
                     'hit_flags':sum((1<<i)|((16<<i) if h>0 else 0) for i,h in hits.items() if h>=0),
                     'target_mask':sum(1<<i for i in targets)})

for cid in (6,7):
    for gun in guns:
        for live in range(16):
            for target in range(4):
                for mode in range(3):
                    add(cid,gun,0,live,target,mode)
for cid in (1,9):
    for rh,lh in ((3,0),(0,3),(3,3),(3,0x13)):
        for live in range(16):
            for target in (0,3,255):
                for mode in range(3):
                    add(cid,rh,lh,live,target,mode)
for cid,rh,lh in ((0,1,3),(0,0x13,3),(7,0x3f,0),(7,0x54,0)):
    for count in range(1,5):
        for mode in range(3):
            add(cid,rh,lh,15,0,mode,count,status=4)

actor = re.search(r'struct BattleLiteActor \{.*?\n\};', source, re.S).group()
pre = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define BATTLE_MAX_ENEMIES 4
#define PS4_CHAR_BASE 0xf500u
#define PS4_ITEM_DATA 0x2a8e28u
#define PS4_TECH_DATA 0x2ad000u
#define PS4_SKILL_DATA 0x2ae000u
#define PS4_INV_BASE 0xf410u
#define R28_VEHICLE_SKILLS 0x2af000u
static uint8_t rom[0x300000], ram[65536];
static unsigned battle_enemy_count, cursor;
static uint16_t random_words[80];
static uint8_t battle_enemy_reaction_flags[4],battle_suppress_skill_source_audio;
static uint8_t ram8r(uint16_t p){return ram[p];}
static void ram8w(uint16_t p,uint8_t v){ram[p]=v;}
static uint8_t rom8(uint32_t p){assert(p<sizeof rom);return rom[p];}
static uint16_t ps4_rng2_next(void){assert(cursor<80);return random_words[cursor++];}
'''+actor+r'''
struct BattleCommandLite {uint8_t type,id,target,inv_slot;};
static unsigned battle_command_target_kind(const struct BattleCommandLite*c){assert(c->type==1);return 1;}
static void battle_enemy_set_hp(struct BattleLiteActor*e,unsigned hp){assert(hp<=e->max_hp);e->hp=hp;}
static unsigned battle_next_target(const struct BattleLiteActor*e,unsigned t,int dir){(void)e;(void)t;(void)dir;assert(0);return 4;}
static uint32_t ps4_item_ptr(uint8_t i){(void)i;assert(0);return 0;}
static unsigned battle_actor_stat(const struct BattleLiteActor*a,unsigned i){(void)a;(void)i;assert(0);return 0;}
static int battle_action_has_resource(const struct BattleLiteActor*p,unsigned n,unsigned a,const struct BattleCommandLite*c){(void)p;(void)n;(void)a;(void)c;assert(0);return 0;}
static void battle_consume_action_resource(struct BattleLiteActor*p,unsigned a,const struct BattleCommandLite*c){(void)p;(void)a;(void)c;assert(0);}
static void battle_skill_source_audio(uint8_t i,struct BattleLiteActor*e){(void)i;(void)e;assert(0);}
static void battle_item_source_audio(uint8_t i,struct BattleLiteActor*e){(void)i;(void)e;assert(0);}
static void ps4_inventory_remove(unsigned i){(void)i;assert(0);}
static void battle_apply_friendly(struct BattleLiteActor*p,unsigned n,unsigned a,unsigned t,unsigned e,unsigned power,unsigned range){(void)p;(void)n;(void)a;(void)t;(void)e;(void)power;(void)range;assert(0);}
static void battle_apply_enemy_effect(struct BattleLiteActor*e,unsigned a,unsigned p){(void)e;(void)a;(void)p;assert(0);}
'''
funcs = ['battle_char_record','battle_element_factor','battle_calc_damage_ex','battle_calc_chance',
         'battle_physical_hit','battle_source_damage','battle_item_type','battle_item_element',
         'battle_actor_slasher_mask','battle_actor_attack_range','battle_actor_command_target_kind',
         'battle_weapon_factor','battle_mark_enemy_reaction']
body = ''.join(fn(n) for n in funcs)
body += 'static uint8_t battle_slasher_hit_mask,battle_slasher_target_mask;\n'+fn('battle_execute_cmd')
records = []
for q in fixtures:
    fields = [q[k] for k in ('cid','rh','lh','count','target','atk','dex')]+list(q['agi'])+q['hp']+q['status']+sum(q['props'],[])+q['stream']+q['after']+q['reactions']+[q['rng_calls'],q['range']]
    records.append('{'+','.join(str(v) for v in fields[:7])+',{'+','.join(map(str,q['agi']))+'},{'+','.join(map(str,q['hp']))+'},{'+','.join(map(str,q['status']))+'},{{'+ '},{'.join(','.join(map(str,row)) for row in q['props'])+'}},{'+','.join(map(str,q['stream']))+'},{'+','.join(map(str,q['after']))+'},{'+','.join(map(str,q['reactions']))+'},'+str(q['rng_calls'])+','+str(q['range'])+','+str(q['hit_flags'])+','+str(q['target_mask'])+'}')
post = r'''
struct Fixture {unsigned cid,rh,lh,count,target,atk,dex,agi[4],hp[4],status[4],props[4][14];uint16_t stream[80];unsigned after[4],react[4],calls,range,hit_flags,target_mask;};
static const struct Fixture fixtures[]={
'''+',\n'.join(records)+r'''
};
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 for(unsigned k=0;k<sizeof fixtures/sizeof fixtures[0];k++){
  const struct Fixture*q=&fixtures[k];struct BattleLiteActor p[5]={{0}},e[4]={{0}};
  memset(ram,0,sizeof ram);memset(battle_enemy_reaction_flags,0,4);cursor=0;memcpy(random_words,q->stream,sizeof random_words);
  p[0].id=q->cid;p[0].hp=p[0].max_hp=999;p[0].atk=q->atk;p[0].dexterity=q->dex;
  ram[battle_char_record(q->cid)+0x4c]=q->rh;ram[battle_char_record(q->cid)+0x4d]=q->lh;
  for(unsigned i=0;i<4;i++){e[i].hp=q->hp[i];e[i].max_hp=999;e[i].agility=q->agi[i];e[i].def=i*13;e[i].status=q->status[i];for(unsigned j=0;j<14;j++)e[i].element_prop[j]=q->props[i][j];}
  struct BattleCommandLite c={1,0,q->target,255};unsigned target=q->target;battle_enemy_count=q->count;
  struct BattleLiteActor before=p[0];uint8_t ram_before[65536];memcpy(ram_before,ram,sizeof ram);
  assert(battle_actor_command_target_kind(&p[0],&c)==q->range);
  battle_slasher_hit_mask=255;battle_slasher_target_mask=255;battle_execute_cmd(p,1,e,0,&c,&target);
  assert(battle_slasher_hit_mask==q->hit_flags&&battle_slasher_target_mask==q->target_mask);
  for(unsigned i=0;i<4;i++)if(e[i].hp!=q->after[i]||battle_enemy_reaction_flags[i]!=q->react[i]){fprintf(stderr,"case %u item %02x target %u enemy %u HP %u/%u reaction %u/%u\n",k,q->rh,q->target,i,e[i].hp,q->after[i],battle_enemy_reaction_flags[i],q->react[i]);return 1;}
  assert(cursor==q->calls);assert(!memcmp(&p[0],&before,sizeof before));assert(!memcmp(ram,ram_before,sizeof ram));
 }
 printf("PASS %zu native ATTACK cases: all ten guns, both android IDs, Alys/Kyra slashers, living masks, misses/criticals, damage limits, original RNG order, menu range and unchanged resources\n",sizeof fixtures/sizeof fixtures[0]);
}
'''
f=O/'r46_physical_range_probe.c';f.write_text(pre+body+post)
exe=O/'r46_physical_range_probe'
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(f),'-o',str(exe)],check=True)
p=subprocess.run([str(exe),str(R/'references/roms/ps4built.bin')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
(O/'R46_PHYSICAL_RANGE_CHECK.log').write_text(p.stdout+p.stderr)
assert p.returncode==0,p.stdout+p.stderr
assert source.count('unsigned tk=battle_actor_command_target_kind(&party[a],&cmds[a]);')==2
report={'result':'PASS','cases':len(fixtures),'guns':guns,'all_range_guns':[0x3f,0x54],
        'original_source':['Battle_AttackCommand 15EC','loc_B6A2/B716','Character_DamageEnemy/Battle_CalculateDamage'],
        'exact_hit_critical_target_masks':True,'poisoned_previous_flags_reset':True,
        'output':p.stdout.strip(),'scope':'Actual ATTACK/eligibility/hit/damage/reaction C against an independent instruction-order oracle, ASan/UBSan, including exact hit/critical/target masks. RNG word stream, non-ATTACK dependencies, HP death transition and hardware are fixtures. Full visuals, audio/hardware RNG phase and native death integration tested separately or remain open.'}
(O/'R46_PHYSICAL_RANGE_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n')
print(p.stdout,end='')
