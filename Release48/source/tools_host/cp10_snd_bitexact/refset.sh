#!/bin/bash
# refset.sh TAG : renders a fixed set of songs/effects with sndtest_TAG, prints md5 per run (bit-exactness check)
export SND_ROM=/home/claude/work/cp10/cp10_a.gba
R=/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/additional_roms
cd /home/claude/work/cp10/snd; mkdir -p out_$1
for job in Anne:81 Anne:88 Rudger:83 Kinds:86 Shilka:8c Amia:90 Huey:a0 Eusis:b0 Nei:c0 Nei:84 Shilka:b3 Rudger:9a; do
  g=${job%%:*}; id=${job##*:}
  if [ $((0x$id)) -le $((0x8f)) ]; then box=1c0a; else box=1c0b; fi
  ./sndtest_$1 $R/${g}_Adventure_-_English.bin 700 out_$1/${g}_$id.wav -inj 120:$box:$id >/dev/null 2>&1 &
done; wait
md5sum out_$1/*.wav | sed 's|out_[^/]*/||'
