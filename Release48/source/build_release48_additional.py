#!/usr/bin/env python3
"""Experimental Additional Content menu. Preserves Release48 PSIV/native games.
Genesis cartridge data use deduplicated direct-read 4 KiB pages.
"""
from pathlib import Path
import argparse,collections,struct,subprocess,os,json,zlib
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent;A=R/'additional';O=A/'build';O.mkdir(exist_ok=True)
ap=argparse.ArgumentParser()
ap.add_argument('--base',type=Path,default=R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
ap.add_argument('--out',type=Path,default=R.parent/'binaries/R48_ADDITIONAL_CONTENT_PREVIEW.gba')
a=ap.parse_args()
rom=bytearray(a.base.resolve().read_bytes());baseline=bytes(rom)
assert len(baseline)==0x2000000,"requires 32 MiB Release48 baseline"
assert not any(baseline[0x6CA4EC:0x738000]),"requires certified empty PSII padding for PSC8/MD engines"
CC=os.environ.get('PSC8_CC','clang');LD=os.environ.get('PSC8_LD','ld.lld');OC=os.environ.get('PSC8_OBJCOPY','llvm-objcopy')
F=['--target=arm-none-eabi','-mcpu=arm7tdmi','-marm','-Os','-ffreestanding','-fno-builtin','-fno-unwind-tables','-I'+str(R/'psc8/include'),'-I'+str(A)]
def build(name,files,addr,entry,hot=False):
 start=(R/'psc8/start.s').read_text().replace('psc8_start',name+'_start').replace('psc8_main',entry)
 sp=O/(name+'_start.s');sp.write_text(start);objs=[]
 for file in [sp,*files]:
  obj=O/(name+'_'+file.name+'.o');subprocess.run([CC,*F,'-c',str(file),'-o',str(obj)],check=True);
  if file.name in ('m68k.c','md.c'):subprocess.run([OC,'--rename-section','.text=.hot,alloc,load,readonly,code,contents',str(obj)],check=True)
  objs.append(str(obj))
 script=f'''ENTRY({name}_start)\nSECTIONS {{ . = {hex(addr)}; .rom : {{ *(.start) *(.text*) *(.rodata*) }} . = ALIGN(4); __hot_load = .; .hot 0x03000000 : AT(__hot_load) {{ __hot_start = .; *(.hot*) *(.hotdata*) __hot_end = ALIGN(4); }} __data_load = __hot_load + SIZEOF(.hot); .data 0x02000000 : AT(__data_load) {{ __data_start = .; *(.data*) __data_end = ALIGN(4); }} .bss (NOLOAD) : {{ __bss_start = .; *(.bss*) *(COMMON) __bss_end = ALIGN(4); }} /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.eh_frame*) }} ASSERT(__bss_end < 0x0203e000,"EWRAM overflow") ASSERT(__hot_end < 0x03007000,"IWRAM overflow") }}'''
 ld=O/(name+'.ld');ld.write_text(script);elf=O/(name+'.elf');subprocess.run([LD,'-T',str(ld),'-Map='+str(O/(name+'.map')),*objs,'-o',str(elf)],check=True);out=O/(name+'.bin');subprocess.run([OC,'-O','binary',str(elf),str(out)],check=True);return out.read_bytes()
def put(off,data,end):
 assert off+len(data)<=end,(hex(off),len(data),hex(end));assert all(b==(255 if off<0x9cac else 0) for b in baseline[off:off+len(data)]),(hex(off),'nonfree');rom[off:off+len(data)]=data
# Allocate only the certified PSIII padding and unused PSIV tail.
regions=[[0x2d3b80,0x390000],[0x1ef2d50,0x2000000]];allocs=[]
def alloc(data,label):
 for reg in regions:
  start=(reg[0]+3)&~3
  if start+len(data)<=reg[1]:
   assert all(b==0 if start<0x800000 else b==255 for b in baseline[start:start+len(data)])
   rom[start:start+len(data)]=data;reg[0]=start+len(data);allocs.append({'label':label,'offset':start,'bytes':len(data)});return 0x08000000+start
 raise RuntimeError('ROM capacity exceeded: '+label)
def compress(data):
 out=bytearray();chains=collections.defaultdict(collections.deque);i=0
 while i<len(data):
  flag=len(out);out.append(0);bits=0
  for bit in range(8):
   if i>=len(data):break
   best=0;dist=0
   if i+3<=len(data):
    key=data[i:i+3];q=chains[key]
    while q and i-q[0]>4096:q.popleft()
    for p in reversed(list(q)[-64:]):
     n=3
     while n<18 and i+n<len(data) and data[p+n]==data[i+n]:n+=1
     if n>best:best=n;dist=i-p
   n=best if best>=3 else 1
   if n>1:bits|=1<<bit;out+=struct.pack('<H',((dist-1)<<4)|(n-3))
   else:out.append(data[i])
   for p in range(i,i+n):
    if p+3<=len(data):chains[data[p:p+3]].append(p)
   i+=n
  out[flag]=bits
 return bytes(out)
def decode(data):
 o=bytearray();i=0
 while len(o)<4096:
  f=data[i];i+=1
  for b in range(8):
   if len(o)>=4096:break
   if f&(1<<b):
    v=data[i]|(data[i+1]<<8);i+=2;d=(v>>4)+1;count=(v&15)+3
    for _ in range(count):o.append(o[-d])
   else:o.append(data[i]);i+=1
 return bytes(o)
romdir=R.parent/'additional_roms'
psi_title='Phantasy Star I Retranslated';psi_name='Phantasy_Star_1_Retranslated_(English_v2.6.1).sms'
psi_data=(romdir/psi_name).read_bytes();assert len(psi_data)==524288
psi_addr=alloc(psi_data,psi_title+' main cartridge')
psi_entry={'title':psi_title,'table':psi_addr,'size':len(psi_data),'flags':10,'crc32':f'{zlib.crc32(psi_data):08x}','state':'main collection slot I via PSC8 FM engine'}
spec=[('Phantasy Star Adventure','Phantasy_Star_Adventure_(English_v1.0).gg',1),('Phantasy Star Gaiden','Phantasy_Star_Gaiden.gg',1)]+[(n+' Adventure',n+'_Adventure_-_English.bin',4) for n in ['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis']]
entries=[];dedup={};total_raw=len(psi_data);total_packed=len(psi_data)
for title,name,flags in spec:
 if name is None:entries.append({'title':title,'table':0,'size':0,'flags':flags,'state':'missing ROM; archive contains IPS only'});continue
 data=(romdir/name).read_bytes();assert len(data)&(len(data)-1)==0;pages=[]
 if flags<4:
  table=alloc(data,title+' cartridge');total_raw+=len(data);total_packed+=len(data);entries.append({'title':title,'table':table,'size':len(data),'flags':flags,'crc32':f'{zlib.crc32(data):08x}','state':'experimental launch'});continue
 for index in range(0,len(data),4096):
  raw=data[index:index+4096];packed=compress(raw);assert decode(packed)==raw
  uncompressed=True;blob=raw;key=(blob,uncompressed)
  if key not in dedup:dedup[key]=alloc(blob,'cartridge page')|int(uncompressed);total_packed+=len(blob)
  pages.append(dedup[key])
 table=alloc(struct.pack('<'+'I'*len(pages),*pages),title+' page table');total_raw+=len(data);entries.append({'title':title,'table':table,'size':len(data),'flags':flags,'crc32':f'{zlib.crc32(data):08x}','state':'experimental launch'})
# Shared title style: angular gold letters over blue bars.
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSerif-BoldItalic.ttf',20)
card=Image.new('L',(240,160));d=ImageDraw.Draw(card)
for text,y in [('ADDITIONAL',57),('CONTENT',83)]:
 box=d.textbbox((0,0),text,font=font);w=box[2];d.text(((240-w)//2,y),text,font=font,fill=14)
d.line((35,112,205,112),fill=3,width=2)
tiles=[bytes(32)];lookup={tiles[0]:0};mapping=[]
for ty in range(20):
 for tx in range(30):
  tile=bytes(card.getpixel((tx*8+x,ty*8+y))|(card.getpixel((tx*8+x+1,ty*8+y))<<4) for y in range(8) for x in range(0,8,2))
  if tile not in lookup:lookup[tile]=len(tiles);tiles.append(tile)
  mapping.append(lookup[tile])
gfx=b''.join(b'\0'+x for x in tiles)+b'\x80';put(0x8000,gfx,0x9400);put(0x9400,struct.pack('<600H',*mapping),0x9cac)
# Submenu background is exactly 240x160 indexed pixels.
bg=Image.new('L',(240,160));d=ImageDraw.Draw(bg);d.rounded_rectangle((4,4,235,132),radius=4,outline=4,width=1)
small=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',16);d.text((25,11),'ADDITIONAL CONTENT',font=small,fill=3,stroke_width=0);d.line((14,34,225,34),fill=4)
bgaddr=alloc(bg.tobytes(),'menu background');(A/'catalog.h').write_text('typedef struct{const char*title;uint32_t table,size,flags;} Cart;\n#define CATALOG_COUNT %du\nstatic const Cart catalog[CATALOG_COUNT]={\n'%len(entries)+''.join('{"%s",%du,%du,%du},\n'%(x['title'],x['table'],x['size'],x['flags']) for x in entries)+'};\n#define menu_background ((const unsigned char*)%du)\n'%bgaddr)
mc=A/'menu.c';s=mc.read_text().replace('extern const unsigned char menu_background[38400];\n','');mc.write_text(s)
# Slot I now launches the supplied 512 KiB retranslation through the generic SMS/FM engine.
# This tiny ARM stub supplies the cartridge configuration before entering PSC8.
psi_stub_src=O/'psi_main.s';psi_stub_src.write_text(f'''\n.syntax unified\n.cpu arm7tdmi\n.arm\n.section .text,"ax",%progbits\n.global psi_main_start\npsi_main_start:\n ldr r0,=0x0203ff00\n ldr r1,={psi_addr:#010x}\n str r1,[r0]\n ldr r1,=0x00080000\n str r1,[r0,#4]\n mov r1,#10\n str r1,[r0,#8]\n mov r1,#0\n str r1,[r0,#12]\n ldr r0,=0x086ca500\n bx r0\n.ltorg\n''')
psi_obj=O/'psi_main.o';subprocess.run([CC,*F,'-marm','-c',str(psi_stub_src),'-o',str(psi_obj)],check=True)
psi_ld=O/'psi_main.ld';psi_ld.write_text('SECTIONS { . = 0x08004f00; .text : { *(.text*) } /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes*) } }')
subprocess.run([LD,'-T',str(psi_ld),str(psi_obj),'-o',str(O/'psi_main.elf')],check=True);subprocess.run([OC,'-O','binary',str(O/'psi_main.elf'),str(O/'psi_main.bin')],check=True)
psi_stub=(O/'psi_main.bin').read_bytes();put(0x4f00,psi_stub,0x5000)
menu=build('menu',[mc],0x08005000,'menu_main');put(0x5000,menu,0x7f00)
# Same ARM startup routine handles each engine's independent RAM.
md=build('md',[A/'md.c',A/'m68k.c',A/'divmod.s'],0x086e2000,'md_main');put(0x6e2000,md,0x738000)
subprocess.run([os.sys.executable,str(R/'psc8/build.py')],check=True);psc=(R/'psc8/build/psc8.bin').read_bytes();put(0x6ca500,psc,0x6e2000)
s=(A/'launcher.s').read_text().replace('0x08008800','0x08008000').replace('0x08009a00','0x08009400');(A/'launcher.s').write_text(s)
obj=O/'hook.o';subprocess.run([CC,*F,'-c',str(A/'launcher.s'),'-o',str(obj)],check=True);ld=O/'hook.ld';ld.write_text('SECTIONS { . = 0x08004170; .text : { *(.text*) } /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes*) } }');subprocess.run([LD,'-T',str(ld),str(obj),'-o',str(O/'hook.elf')],check=True);subprocess.run([OC,'-O','binary',str(O/'hook.elf'),str(O/'hook.bin')],check=True);put(0x4170,(O/'hook.bin').read_bytes(),0x5000)
def thumb_bl(src,dst):
 delta=dst-(src+4);return struct.pack('<HH',0xf000|((delta>>12)&0x7ff),0xf800|((delta>>1)&0x7ff))
rom[0x6a8:0x6ac]=thumb_bl(0x080006a8,0x08004170);struct.pack_into('<H',rom,0x698,0x2004);struct.pack_into('<H',rom,0x69e,0x2804);put(0x7f00,struct.pack('<I',0x08004f00)+baseline[0x18840:0x1884c]+struct.pack('<I',0x08005000),0x8000)
for literal in (0x7ac,0x8e8):
 assert struct.unpack_from('<I',baseline,literal)[0]==0x0801883c
 struct.pack_into('<I',rom,literal,0x08007f00)
assert rom[0x800000:0x1ef2d50]==baseline[0x800000:0x1ef2d50]
out=a.out.resolve();out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(rom)
report={'output':out.name,'main_PSI':psi_entry,'cartridges':entries,'raw_cartridge_bytes':total_raw,'packed_unique_cartridge_bytes':total_packed,'engines':{'psc8_bytes':len(psc),'md_bytes':len(md),'menu_bytes':len(menu),'psi_main_stub_bytes':len(psi_stub)},'allocations':allocs,'PSIV_active_bytes_unchanged':True,'main_PSI_replaced_with_retranslation':True,'main_PSI_old_native_payload_retained_for_rollback':True,'CRC32':f'{zlib.crc32(rom):08x}','limits':['FM/Game Gear experimental slow engine','Genesis runtime lacks sound and persistent saves','Added cartridge playthroughs not verified']};(R.parent/'build_artifacts/ADDITIONAL_CONTENT_PACK.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='allocations'},indent=2))
