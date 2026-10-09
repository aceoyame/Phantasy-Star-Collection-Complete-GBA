#!/usr/bin/env python3
"""R40 unchanged native scene/header, source text/action, inventory and party probe.
Production party walking, inventory/macro primitives, source palettes and R36
space pass execute under ASan/UBSan. Map loading, UI input/rendering, ordinary
NPC movement, tile animation and hardware are fixtures. Checks are not a normal
save playthrough or timing/finishability certificate. Requires R39 probe chain.
"""
from pathlib import Path
import re,subprocess,json,os
from md_compression import kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';src=(R/'gba/title_runtime_v06.c').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r39_scene_probe.c').read_text().split('int main(')[0]
s=s.replace('row_fg,col_fg,row_bg,col_bg,collision_plane;} field_map;','row_fg,col_fg,row_bg,col_bg,collision_plane;uint32_t palette;} field_map;')
for address in [0x1EF130,0x1FA480,0x1FAAC0]:
 data=kosinski_decompress(rom[address:]);name=f'tree_{address:x}'
 pos=s.index('static unsigned kos_decode(');s=s[:pos]+f'static const uint8_t {name}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
 pos=s.index('default:assert(0);return 0;}',s.index('static unsigned kos_decode('));s=s[:pos]+f'case {address}u:assert(cap>=sizeof {name});memcpy(d,{name},sizeof {name});return sizeof {name};\n'+s[pos:]
s+='#define PS4_ITEM_DATA 0x2A8E28u\n'+function('ps4_item_ptr')+function('ps4_item_price')
s+=r'''
static unsigned pop_count,pop_left[8],inventory_ui_calls,confirm_ui_calls,message_calls,palette_reload_calls;
static int ui_slots[16],ui_answers[16];static unsigned ui_slot_count,ui_answer_count;
static void panel_rebuild_scaled(void){assert(pop_count<8);pop_left[pop_count++]=field_active_panel_count;}
static void panel_present_scaled(void){waitvb();}
static int ps4_ui_select_inventory(const char*title,unsigned equip,unsigned id){assert(!strcmp(title,"EXCHANGE FOR ECLPS TORCH")&&!equip&&!id&&inventory_ui_calls<ui_slot_count);return ui_slots[inventory_ui_calls++];}
static int ps4_ui_select_strings(const char*title,const char*const*choices,unsigned count,unsigned initial){assert(!strcmp(title,"EXCHANGE SELECTED ITEM?")&&count==2&&initial==1&&!strcmp(choices[0],"EXCHANGE")&&confirm_ui_calls<ui_answer_count);return ui_answers[confirm_ui_calls++];}
static void ps4_ui_message(const char*a,const char*b){assert(!strcmp(a,"KEEP THIS KEY ITEM")&&!b);message_calls++;}
'''
s+=function('field_reload_palette_from_map').replace('field_reload_palette_from_map(','field_reload_palette_from_map_native(')
s+='static void field_reload_palette_from_map(void){assert(!flags[0x9D]&&!flags[0xC0]);palette_reload_calls++;field_reload_palette_from_map_native();}\n'
s+='#include "r40_lashiec_df2_bishop.h"\n'
for id,name in [(0x38,'lashiec_offsets'),(0x3B,'df2_offsets')]:
 d=kosinski_decompress(rom[0x1EF130:]);es=d.split(b'\xff');base=sum(len(x)+1 for x in es[:id]);e=es[id];offsets=[base]+[base+i+1 for i,q in enumerate(e) if q==0xF7]
 s+=f'static const unsigned {name}[]={{'+','.join(map(str,offsets))+'};\n'
s+=r'''
static void reset40(void){story_fresh();pop_count=inventory_ui_calls=confirm_ui_calls=message_calls=palette_reload_calls=ui_slot_count=ui_answer_count=0;field_map.collision_plane=0;memset(cram,0,sizeof cram);memset(field_followers,0,sizeof field_followers);field_follower_count=0;}
static void reload40(unsigned map,unsigned x,unsigned y,unsigned face,unsigned align){
 assert(!ps4_load_field_map(map));field_event_reload_pending=0;ram16w(0xEC28,map);chaz.x=x*8;chaz.y=y*8;chaz.facing=face/4;lead_char_id=ram8r(0xF40A);
 field_followers_sync_aligned(align);r33_party_position(lead_char_id,chaz.x,chaz.y,face);
 for(unsigned i=0;i<field_follower_count;i++){struct PartyFollowerLite*p=&field_followers[i];r33_party_position(p->id,p->x,p->y,face);}
}
static void expect_cues(const unsigned* expected,unsigned n){assert(sound_count==n);for(unsigned i=0;i<n;i++){if(sounds[i].cue!=expected[i])fprintf(stderr,"cue%u got%X expected%X\n",i,sounds[i].cue,expected[i]);assert(sounds[i].cue==expected[i]);}}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned x=0,y=0;
 // Lashiec uses actual Tree22 F7, all source panels/cues,120 map waits and full travel.
 reset40();r33_dialogue_tree(0x1EF130);flags[0x80]=1;ram8w(PS4_INV_BASE,0x81);ram8w(PS4_INV_BASE+1,0x82);ram8w(PS4_INV_BASE+40,0xA5);
 assert(r40_cutscene_lashiec(&x,&y));assert(field_custom_resume_event==0xB1&&!field_custom_resume_phase&&field_event_reload_pending&&map_updates==120&&ram8r(0xF400)==1);
 assert(ram16r(0xEC28)==0x162&&ram16r(0xEC2A)==0xFFFF&&ram16r(0xEC48)==0x4E&&ram16r(0xEC4A)==0x2C&&ram16r(0xEC44)==4&&ram16r(0xEC46)==4&&!(ram8r(0xEC4E)&8));
 assert(ram8r(PS4_INV_BASE+2)==0x8E&&ram8r(PS4_INV_BASE+40)==0xA5&&!inventory_ui_calls&&text_count==2&&texts[0].stop==0xF7&&texts[1].stop==255);
 for(unsigned i=0;i<2;i++)assert(texts[i].tree==0x1EF130&&texts[i].offset==lashiec_offsets[i]);
 unsigned lash_panels[]={0x107,0x108,0x109,0x10A,0x10B,0x10C};assert(panel_count==6);for(unsigned i=0;i<6;i++)assert(panel_id[i]==lash_panels[i]);assert(pop_count==1&&!pop_left[0]);
 unsigned lash_cues[]={0xF3,0xF5,0xED,0xDD,0xDD,0xDD,0xDD,0xDD,0xDD,0xDD,0xDD,0xAD,0xAD,0xAD,0x99,0xF3};expect_cues(lash_cues,sizeof lash_cues/sizeof lash_cues[0]);assert(sounds[12].at-sounds[11].at==40&&sounds[13].at-sounds[12].at==20&&sounds[14].at-sounds[13].at==241&&sounds[15].at-sounds[14].at==1);
 assert(ui_frames==301); // Real unskipped R36 space pass: initial view plus300 source updates.
 reload40(0x162,0x4E,0x2C,4,4);r33_dialogue_tree(0x1FA480);assert(r40_story_resume(&x,&y));assert(field_custom_resume_event==255&&tree==0x1FA480&&text_count==3&&texts[2].tree==0x1EF130&&texts[2].stop==255&&ram8r(0xECEC)==0x93);
 for(unsigned i=0;i<4;i++)assert(ram16r(0xC030+i*64)==0x270&&ram16r(0xC034+i*64)==0x160+i*16);
 // Every ordinary free reward slot remains bounded; no interactive exchange.
 for(unsigned slot=0;slot<40;slot++){reset40();for(unsigned i=0;i<slot;i++)ram8w(PS4_INV_BASE+i,0x81);ram8w(PS4_INV_BASE+40,0xA5);r40_torch_reward();assert(ram8r(PS4_INV_BASE+slot)==0x8E&&ram8r(PS4_INV_BASE+40)==0xA5&&!inventory_ui_calls);}
 // Full40 inventory: cancel, protected key item, declined/cancelled confirmation, then explicit exchange.
 reset40();for(unsigned i=0;i<40;i++)ram8w(PS4_INV_BASE+i,0x81);ram8w(PS4_INV_BASE+4,0x98);ram8w(PS4_INV_BASE+40,0xA5);
 int choices[]={-1,4,6,7,7};memcpy(ui_slots,choices,sizeof choices);ui_slot_count=5;ui_answers[0]=-1;ui_answers[1]=1;ui_answers[2]=0;ui_answer_count=3;r40_torch_reward();
 assert(inventory_ui_calls==5&&confirm_ui_calls==3&&message_calls==1&&ram8r(PS4_INV_BASE+4)==0x98&&ram8r(PS4_INV_BASE+7)==0x8E&&ram8r(PS4_INV_BASE+40)==0xA5);
 for(unsigned i=0;i<40;i++)if(i!=4&&i!=7)assert(ram8r(PS4_INV_BASE+i)==0x81);
 // DF2 all3 F7 seams; all Kyra follower positions; exact carrier/macro compaction.
 for(unsigned kslot=1;kslot<5;kslot++){
  reset40();unsigned rest[]={0,3,5,7},n=0;uint8_t before[5][64];
  for(unsigned i=0;i<5;i++){unsigned id=i==kslot?9:rest[n++];ram8w(0xF40A+i,id);for(unsigned q=0;q<64;q++)before[i][q]=(uint8_t)(i*31+q);memcpy(work_ram+0xC000+i*64,before[i],64);}
  memset(work_ram+PS4_MACROS,0,160);for(unsigned m=0;m<8;m++)for(unsigned i=0;i<5;i++)ps4_macro_write(m,ram8r(0xF40A+i),1,0);ram8w(PS4_CHAR_BASE+9*128+0x16,0x7F);followers();r33_dialogue_tree(0x1EF130);
  assert(r40_cutscene_dark_force2(&x,&y));assert(flags[0xA1]&&map_updates==60&&field_custom_resume_event==0xB1&&field_custom_resume_phase==1&&field_event_reload_pending);
  assert(ram16r(0xEC28)==1&&ram16r(0xEC2A)==0xFFFF&&ram16r(0xEC48)==0x174&&ram16r(0xEC4A)==0xE&&!ram16r(0xEC44)&&!ram16r(0xEC46)&&ram8r(0xECEC)==0x9D);
  assert(text_count==4);for(unsigned i=0;i<4;i++)assert(texts[i].tree==0x1EF130&&texts[i].offset==df2_offsets[i]&&texts[i].stop==(i<3?0xF7:255));
  unsigned df_panels[]={0x10E,0x10F,0x110,0x111,0x112,0x113,0x114,0x115,0x116,0xA8};assert(panel_count==10);for(unsigned i=0;i<10;i++)assert(panel_id[i]==df_panels[i]);assert(pop_count==1&&pop_left[0]==1);
  unsigned df_cues[]={0xAD,0xDD,0xDD,0xDD,0xDD,0xDD,0xF3,0x9D,0xF3,0xF3,0xAD,0xA9,0xF3,0xFB,0x9D};expect_cues(df_cues,sizeof df_cues/sizeof df_cues[0]);for(unsigned j=2;j<=5;j++)assert(sounds[j].at-sounds[j-1].at==8);assert(sounds[8].at-sounds[7].at==60&&sounds[11].at-sounds[10].at==120&&sounds[14].at-sounds[13].at==20);
  for(unsigned i=0;i<4;i++){assert(ram8r(0xF40A+i)==rest[i]);assert(!memcmp(work_ram+0xC000+i*64,before[i+(i>=kslot)],64));}for(unsigned q=0;q<64;q++)assert(!ram8r(0xC100+q));assert(ram8r(0xF40E)==255&&!ram8r(PS4_CHAR_BASE+9*128+0x16));
  for(unsigned m=0;m<8;m++){for(unsigned i=0;i<4;i++)assert(ram8r(PS4_MACROS+m*20+i*4)==rest[i]&&ram8r(PS4_MACROS+m*20+i*4+1)==1);for(unsigned q=16;q<20;q++)assert(!ram8r(PS4_MACROS+m*20+q));}
  reload40(1,0x174,0xE,0,0);assert(r40_story_resume(&x,&y)&&field_custom_resume_event==255&&field_follower_count==3&&chaz.x==0xBA0&&chaz.y==0x70);
 }
 // Bishop actual incoming Tree37 and source travel, then a real Motavia spaceport request.
 reset40();flags[0x80]=1;r33_dialogue_tree(0x1F9580);assert(r40_cutscene_bishop(&x,&y));assert(map_updates==120&&ui_frames==301&&ram8r(0xF400)==0&&field_custom_resume_event==0xB1&&field_custom_resume_phase==2&&field_event_reload_pending);
 assert(text_count==1&&texts[0].tree==0x1F9580&&texts[0].stop==255&&ram16r(0xEC28)==0xBF&&ram16r(0xEC2A)==0xFFFF&&ram16r(0xEC48)==0x3C&&ram16r(0xEC4A)==0x26&&!ram16r(0xEC44)&&!ram16r(0xEC46)&&!flags[0x9D]&&!flags[0xC0]);
 // Full inventory with Torch in every slot remains full after the one-for-one exchange.
 for(unsigned slot=0;slot<40;slot++){
  reset40();field_custom_resume_event=0xB1;field_custom_resume_phase=2;for(unsigned i=0;i<40;i++)ram8w(PS4_INV_BASE+i,0x81);ram8w(PS4_INV_BASE+slot,0x8E);ram8w(PS4_INV_BASE+40,0xA5);
  reload40(0xBF,0x3C,0x26,0,0);field_map.palette=0x1786F4;r33_dialogue_tree(0x1FAAC0);assert(r40_story_resume(&x,&y));
  assert(field_custom_resume_event==255&&map_updates==32&&chaz.x==0x1E0&&chaz.y==0x170&&flags[0x9D]&&flags[0xC0]&&palette_reload_calls==1&&ram8r(PS4_INV_BASE+39)==0x98&&ram8r(PS4_INV_BASE+40)==0xA5);
  for(unsigned i=0;i<39;i++)assert(ram8r(PS4_INV_BASE+i)==0x81);assert(text_count==1&&texts[0].tree==0x1FAAC0&&panel_count==3&&panel_id[0]==0x186&&panel_id[1]==0x117&&panel_id[2]==0x186);
  for(unsigned i=0;i<32;i++)assert(cram[i]==rom16(0x1786F4+i*2));for(unsigned i=0;i<16;i++)assert(cram[32+i]==rom16(0x296300+i*2)&&cram[48+i]==rom16(0x1786F4+0x40+i*2));
 }
 field_custom_resume_event=0xB1;field_custom_resume_phase=9;assert(!r40_story_resume(&x,&y));
 puts("PASS R40 actual text/seams/panels/cues/map handoffs; full source travel; Kyra slots1..4/carrier/macro compaction;40-slot Torch/Hydrofoil exchanges and protected confirmed overflow handling (UI/map/hardware fixtures)");
}
'''
c=O/'r40_scene_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r40_scene_probe')],check=True)
subprocess.run([str(O/'r40_scene_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
report={'scope':__doc__,'source_scenes':[0x16,0x17,0x18],'lashiec_tree':'1EF130','lashiec_f7_seams':1,'df2_f7_seams':3,'bishop_incoming_tree':'1F9580','bishop_final_tree':'1FAAC0','lashiec_final_map':0x162,'df2_final_map':1,'bishop_final_map':0xBF,'source_space_pass_frames_per_trip':300,'kyra_slots_tested':[1,2,3,4],'torch_reward_free_slots_tested':40,'hydrofoil_full_inventory_torch_slots_tested':40,'confirmed_overflow_exchange_key_protection_cancel_and_decline':True,'emulator_playthrough':False}
(O/'R40_SOURCE_PROBE.json').write_text(json.dumps(report,indent=2)+'\n')
