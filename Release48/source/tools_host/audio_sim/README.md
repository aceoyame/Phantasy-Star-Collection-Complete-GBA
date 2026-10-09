Host simulation of gba/ps4_audio_stream.c (CP18). Regenerate audio_host.c/data_host.s with the
sed/printf recipe in PROGRESS.md, then: gcc -O1 -w -no-pie -fno-pie -o sim sim.c audio_host.c data_host.s
./sim <stall_frames> <play_at_frame>  -> checks BGM switch + counts FIFO samples read past the 3-frame guard.
