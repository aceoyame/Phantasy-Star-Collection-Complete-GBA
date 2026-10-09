"""Host-only original YM patch -> VGM -> installed libgme reference capture."""
import ctypes,ctypes.util,struct
import numpy as np

YM_CLOCK=7670454
FNUM=(606,644,683,723,766,813,860,911,965,1023,1084,1148)
REGISTERS=(0x30,0x38,0x34,0x3c,0x50,0x58,0x54,0x5c,0x60,0x68,0x64,0x6c,0x70,0x78,0x74,0x7c,0x80,0x88,0x84,0x8c,0x40,0x48,0x44,0x4c)
_lib=None
def library():
 global _lib
 if _lib is None:
  _lib=ctypes.CDLL(ctypes.util.find_library('gme'))
  _lib.gme_open_data.argtypes=[ctypes.c_void_p,ctypes.c_long,ctypes.POINTER(ctypes.c_void_p),ctypes.c_long];_lib.gme_open_data.restype=ctypes.c_char_p
  _lib.gme_start_track.argtypes=[ctypes.c_void_p,ctypes.c_int];_lib.gme_start_track.restype=ctypes.c_char_p
  _lib.gme_play.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.POINTER(ctypes.c_short)];_lib.gme_play.restype=ctypes.c_char_p
  _lib.gme_ignore_silence.argtypes=[ctypes.c_void_p,ctypes.c_int]
  _lib.gme_delete.argtypes=[ctypes.c_void_p]
 return _lib
def make_vgm(patch,note,duration=1.0):
 assert len(patch)==25
 commands=bytearray()
 def ym(reg,val):commands.extend((0x52,reg,val&255))
 for channel in range(6):ym(0x28,(0,1,2,4,5,6)[channel])
 ym(0x22,0);ym(0x27,0);ym(0x2b,0)
 ym(0xb0,patch[0]);ym(0xb4,0xc0)
 for register,value in zip(REGISTERS,patch[1:]):ym(register,value)
 word=((note//12)&7)<<11|FNUM[note%12]
 ym(0xa4,word>>8);ym(0xa0,word&255);ym(0x28,0xf0)
 samples=round(duration*44100)
 while samples:count=min(65535,samples);commands.extend((0x61,count&255,count>>8));samples-=count
 commands.append(0x66)
 header=bytearray(0x40);header[:4]=b'Vgm '
 for offset,value in [(4,len(header)+len(commands)-4),(8,0x150),(0x18,round(duration*44100)),(0x2c,YM_CLOCK),(0x34,0xc)]:struct.pack_into('<I',header,offset,value)
 return bytes(header+commands)
def render(patch,note,rate=192000,duration=1.0):
 lib=library();data=make_vgm(patch,note,duration);buf=ctypes.create_string_buffer(data);emu=ctypes.c_void_p()
 error=lib.gme_open_data(buf,len(data),ctypes.byref(emu),rate)
 if error:raise RuntimeError(error.decode())
 try:
  lib.gme_ignore_silence(emu,1)
  error=lib.gme_start_track(emu,0)
  if error:raise RuntimeError(error.decode())
  pcm=np.empty(round(rate*duration)*2,dtype=np.int16)
  error=lib.gme_play(emu,len(pcm),pcm.ctypes.data_as(ctypes.POINTER(ctypes.c_short)))
  if error:raise RuntimeError(error.decode())
  return pcm.reshape(-1,2).mean(axis=1)
 finally:lib.gme_delete(emu)
def source_hz(note):return FNUM[note%12]*(1<<((note//12)&7))*YM_CLOCK/(144*(1<<21))
if __name__=='__main__':
 from pathlib import Path
 rom=(Path(__file__).resolve().parents[1]/'references/roms/ps4built.bin').read_bytes()
 patch=rom[0xd570f:0xd570f+25];note=36;x=render(patch,note)
 bins=abs(np.fft.rfft(x[19200:38400]));freq=np.fft.rfftfreq(19200,1/192000)
 print('nominal source Hz',source_hz(note),'top spectral bins',sorted(zip(bins,freq),reverse=True)[:10],flush=True)
