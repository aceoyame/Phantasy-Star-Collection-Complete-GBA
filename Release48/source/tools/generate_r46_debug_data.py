#!/usr/bin/env python3
"""Generate ROM-only debug labels/help from pinned PSIV constants and formations."""
from pathlib import Path
import json, re
from md_compression import kosinski_decompress

R = Path(__file__).resolve().parents[1]
D = R / 'references/disassembly/ps4disasm/ps4disasm'
constants = (D / 'ps4.constants.asm').read_text()
rom = (R / 'references/roms/ps4built.bin').read_bytes()
def number(s): return int(s[1:], 16) if s.startswith('$') else int(s)
def human(s):
    return re.sub(r'(?<=[a-z])(?=[A-Z0-9])', ' ', s).replace('_', ' ').upper()
def literal(s): return json.dumps(s, ensure_ascii=True)

enemy_names = [''] * 153
for m in re.finditer(r'^EnemyID_(\w+)\s*=.*?;\s*(\$[0-9A-Fa-f]+|\d+)\s*$', constants, re.M):
    enemy_names[number(m[2])] = human(m[1])
assert all(enemy_names)
enemies = [f'{i:02X} {n}' for i,n in enumerate(enemy_names)]
data = kosinski_decompress(rom[0x284F7C:])
records = data.split(b'\xff')[:27]
assert len(records) == 27
formations = []
members = set()
for i,rec in enumerate(records):
    assert len(rec) >= 8 and 1 <= rec[4] <= 4
    ids = list(rec[7:7+2*rec[4]:2]); assert len(ids) == rec[4]
    members.update(ids)
    # Show every member in a separate help line; compact menu labels retain the
    # authored formation ID and leading actor, even for duplicate encounters.
    formations.append((i, enemy_names[ids[1] if i == 0 else ids[0]], ids))
# PD phases 2/3 have no separate event formations. Include standalone variants
# as well as every actor used by an authored boss formation.
boss_ids = sorted(members | {0x85,0x86,0x87,0x8B,0x8C,0x98})
bosses = [(f'F{i:02X} {n}', 0, i, ' + '.join(enemy_names[j] for j in ids))
          for i,n,ids in formations]
bosses += [(f'E{i:02X} {enemy_names[i]}',1,i,'SINGLE ENEMY / NO STORY ADVANCE') for i in boss_ids]

flags = []
for prefix,kind in [('EventFlag_',0),('ChestFlag_',1),('TempEveFlag_',2)]:
    labels = ['UNNAMED FLAG'] * 256
    helps = ['NO SOURCE DESCRIPTION. SET CHANGES THIS BIT ONLY.'] * 256
    for m in re.finditer(r'^'+prefix+r'(\w+)\s*=\s*(\$[0-9A-Fa-f]+|\d+)([^\n]*)', constants, re.M):
        i=number(m[2]); labels[i]=human(m[1])
        comment=m[3].partition(';')[2].strip()
        if kind==1:
            helps[i]='SET MARKS THIS CHEST OPENED. NO ITEM IS GRANTED. CLEAR ALLOWS IT TO BE OPENED AGAIN.'
        elif comment:
            helps[i]=comment.upper().replace("YOU'RE",'YOU ARE')
        else:
            helps[i]='NO SOURCE DESCRIPTION. SET CHANGES THIS BIT ONLY.'
    if kind==0:
        helps[0xD9]='ELSYDEON SCENE RECORDED. GUILD JOBS CLOSE; FINAL-DUNGEON ACCESS CHECKS THIS FLAG. NO SWORD OR SCENE IS GRANTED.'
        helps[0xE0]='ALLOWS ANGER TOWER ENTRY. DOES NOT GRANT MEGID.'
        helps[0xE1]='RE-FAZE CHOICE RECORDED. DOES NOT TEACH MEGID OR RUN THE BATTLE.'
        for i in range(0xDB,0xE0):
            helps[i]='FIFTH-MEMBER CHOICE RECORDED. SETTING THIS BIT DOES NOT CHANGE THE PARTY OR CLEAR THE OTHER CHOICE FLAGS.'
    flags.append(([f'{i:02X} {name}' for i,name in enumerate(labels)],helps))

out=['/* R46: generated from pinned source; strings and tables stay in ROM. */\n',
     'struct R46DebugBoss { const char* label; const char* help; uint8_t single; uint8_t id; };\n']
out.append('static const char* const r46_debug_enemy_names[153]={\n'+''.join('    '+literal(s)+',\n' for s in enemies)+'};\n')
out.append('static const struct R46DebugBoss r46_debug_bosses[]={\n'+''.join('    {'+literal(n)+','+literal(h)+f',{single}u,{i}u'+'},\n' for n,single,i,h in bosses)+'};\n')
for suffix,j in [('names',0),('help',1)]:
    out.append(f'static const char* const r46_debug_flag_{suffix}[3][256]={{\n')
    for values in flags:
        out.append('  {\n'+''.join('    '+literal(s)+',\n' for s in values[j])+'  },\n')
    out.append('};\n')
(R/'gba/r46_debug_data.h').write_text(''.join(out))
(R.parent/'build_artifacts/R46_DEBUG_CATALOG.json').write_text(json.dumps({
    'enemies':153,'source_formations':27,'standalone_bosses':boss_ids,
    'boss_menu_entries':len(bosses),'named_flags':[sum('UNNAMED FLAG' not in s for s in v[0]) for v in flags],
    'scope':'All source enemy IDs and all event formations; standalone final-boss phases and variants. Descriptions explain flag-only mutation; no event or reward is invented.'},indent=2)+'\n')
print('PASS debug catalog:',len(enemies),'enemies,',len(bosses),'boss entries')
