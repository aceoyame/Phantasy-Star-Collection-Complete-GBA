from mdrec import *
import mdbattle
for idx in range(27):
    m=MD(); m.load('/home/claude/w/md/st_field2.bin')
    if mdbattle._party is None: mdbattle._party=gba_party()
    slots,chars=mdbattle._party
    m.setram(0xF40A,slots); m.setram(0xF500,chars)
    m.w8(0xECFC,idx)
    assert run_until_break(m,0x5199A,30); L.md_set_reg(16,0x5A608)
    m.run(420); m.shot('/home/claude/w/emu/shots/mdboss_%02d.png'%idx)
    m.save('/home/claude/w/md/st_boss_%02d.bin'%idx)
