from pathlib import Path
import argparse,struct,json,zlib
R=Path(__file__).resolve().parents[2];ap=argparse.ArgumentParser();ap.add_argument('--rom',type=Path,default=R/'binaries/R48_ADDITIONAL_CONTENT_PREVIEW.gba');ap.add_argument('--base',type=Path,default=R/'binaries/RUN_THIS_RELEASE48_PSC.gba');a=ap.parse_args();rom=a.rom.resolve().read_bytes();report=json.loads((R/'build_artifacts/ADDITIONAL_CONTENT_PACK.json').read_text());rows=[]
for entry in report['cartridges']:
 if not entry['table']:continue
 if entry['flags']<4:actual=rom[entry['table']-0x8000000:entry['table']-0x8000000+entry['size']]
 else:
  table=struct.unpack_from('<'+'I'*(entry['size']//4096),rom,entry['table']-0x8000000);assert all(p&1 for p in table);actual=b''.join(rom[(p&~1)-0x8000000:(p&~1)-0x8000000+4096] for p in table)
 source=next(p for p in (R/'additional_roms').iterdir() if (p.stem.replace('_',' ')==entry['title']+' - English') or (entry['flags']==1 and p.stem.replace('_',' ').startswith(entry['title'])) or (entry['flags']==2 and p.suffix=='.sms'))
 assert actual==source.read_bytes(),entry['title'];rows.append({'title':entry['title'],'all_bytes_match_input':True,'bytes':len(actual)})
# Main-slot PSI is stored separately from the submenu catalog.
main=report['main_PSI'];off=main['table']-0x8000000;actual=rom[off:off+main['size']];source=(R/'additional_roms'/'Phantasy_Star_1_Retranslated_(English_v2.6.1).sms').read_bytes();assert actual==source;rows.insert(0,{'title':main['title'],'all_bytes_match_input':True,'bytes':len(actual),'main_slot':True})
base=a.base.resolve().read_bytes()
# Existing native-game payloads, slots 1-4 and PSIV executable/data stay intact.
for lo,hi in [(0x80000,0x2d3b80),(0x390000,0x6ca4ec),(0x738000,0x800000),(0x800000,0x1ef2d50)]:assert base[lo:hi]==rom[lo:hi],hex(lo)
(R/'build_artifacts/ADDITIONAL_PRESERVATION.json').write_text(json.dumps({'ROM_CRC32':f'{zlib.crc32(rom):08x}','games':rows,'original_three_game_payloads_and_PSIV_active_module_preserved':True},indent=2)+'\n');print('PASS ten cartridges byte-exact and selected base game payloads preserved')
