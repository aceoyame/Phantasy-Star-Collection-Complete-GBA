"""Verified retail PSC padding reserved independently of the PSIV module."""
from build_psc_four_game import (PSC_PANEL_HOLE_OFF,PSC_PANEL_HOLE_END,
                                 PSC_ENEMY_HOLE_OFF,PSC_ENEMY_HOLE_END)
REGIONS=(('PSIII trailing padding',PSC_PANEL_HOLE_OFF,PSC_PANEL_HOLE_END),
         ('PSII trailing padding',PSC_ENEMY_HOLE_OFF,PSC_ENEMY_HOLE_END))

def reclaim_padding(shell):
    assert len(shell)==0x800000
    result=bytearray(shell)
    for _,start,end in REGIONS:result[start:end]=bytes(end-start)
    return bytes(result)

def check_reclaimed_padding(before,after):
    assert len(before)==len(after)==0x800000
    assert after==reclaim_padding(before),'PSC changed outside the verified padding ranges'
    return [{'name':name,'file_offset':hex(start),'ROM_address':hex(0x08000000+start),
             'end_exclusive':hex(end),'bytes':end-start,
             'cleared_nonzero_bytes':sum(v!=0 for v in before[start:end]),
             'reserved_for':'future PSC original-game additions'} for name,start,end in REGIONS]

def available_padding(shell):
    return sum(end-start for _,start,end in REGIONS if not any(shell[start:end]))
