import struct, sys
R = open('/home/claude/work/ref/ps2/ps2disassm/ps2original.bin', 'rb').read()
print(len(R), R[0x100:0x110], R[0x1f0:0x200])
for name, pb in (('UE', 0xB8568), ('J', 0xB8528)):
    ptrs = [struct.unpack_from('<H', R, pb + 2 * i)[0] for i in range(48)]
    print(name, [hex(p) for p in ptrs])
print('UE reg list', R[0xBA818:0xBA818 + 26].hex())
