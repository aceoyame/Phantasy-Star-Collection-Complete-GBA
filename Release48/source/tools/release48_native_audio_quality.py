#!/usr/bin/env python3
"""Low-risk native PSII/PSIII final-mixer quantizer patch.

The retail six-voice mixer converts its signed accumulator with arithmetic ASR #8,
which biases negative samples downward.  This byte-identical-size replacement
performs symmetric signed round-to-nearest before the same saturation/store.
"""
MIXER_OFFSETS={'II':0x083140,'III':0x395530}
OLD_MIXER=bytes.fromhex('3f002de90040a0e1160ea0e30324a0e3025a82e20010d2e5013195e76011d2e5011195e7013083e0c012d2e5011195e7013083e02014d2e5011195e7013083e08015d2e5011195e7013083e0e016d2e5011195e7013083e04334a0e1800053e37f30a053800073e38030a0430030c4e5014084e2012082e2010050e2e4ffff1a3f00bde81eff2fe1')
NEW_MIXER=bytes.fromhex('3f002de90040a0e1160ea0e30324a0e3025a82e20110d2e4013195e75f11d2e5011195e7013083e0bf12d2e5011195e7013083e01f14d2e5011195e7013083e07f15d2e5011195e7013083e0df16d2e5011195e7013093e0803083e2013043424334a0e1800053e37f30a053800073e38030a0430130c4e4010050e2e4ffff1a3f00bde81eff2fe1')
assert len(OLD_MIXER)==len(NEW_MIXER)==136

def apply(shell: bytes) -> bytes:
    if len(shell) < 0x400000:
        raise ValueError('expected PSC shell containing PSII/PSIII native mixers')
    out=bytearray(shell)
    for game,off in MIXER_OFFSETS.items():
        got=bytes(out[off:off+len(OLD_MIXER)])
        if got!=OLD_MIXER:
            # Idempotence makes composed/checkpoint rebuilds less fragile.
            if got==NEW_MIXER:
                continue
            raise ValueError(f'{game} native mixer signature mismatch at {off:#x}')
        out[off:off+len(NEW_MIXER)]=NEW_MIXER
    return bytes(out)
