import sys
base=bytearray(open(sys.argv[1],'rb').read()); blob=open(sys.argv[2],'rb').read()
LO,HI=0x6CA500,0x6E2000
assert len(blob)<=HI-LO,(len(blob),HI-LO)
base[LO:HI]=bytes(HI-LO); base[LO:LO+len(blob)]=blob
open(sys.argv[3],'wb').write(base); print('injected',len(blob))
