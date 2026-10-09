#include <stdint.h>
#include <stddef.h>
#include "ps4_audio_runtime.h"
/* CP22 universal soundtrack: every PSIV music ID ($81-$B4) is rendered by the
 * PSC-style source-sequenced P134R path (gba/audio_p134r), identical to the PSC
 * fork.  The PCM masters are compiled out and dropped from the link.  Build with
 * PS4_HYBRID_PCM_MUSIC=1 (-DPS4_MUSIC_UNIVERSAL_SEQUENCED=0) to restore them. */
#ifndef PS4_MUSIC_UNIVERSAL_SEQUENCED
#define PS4_MUSIC_UNIVERSAL_SEQUENCED 1
#endif
#if !PS4_MUSIC_UNIVERSAL_SEQUENCED
#include "pcm_packed/ps4_pcm_packed.h"
#endif
#include "ps4_audio_fallback.h"
#include "pcm_sfx/ps4_sfx_bank.h"

/* PSIV static AOT audio backend.
 *
 * Music is the user-approved v18 render.  SFX are source-driver renders in the
 * original $B5-$FA ID order.  ARM7 does no YM2612/PSG synthesis: during active
 * display we mix one 528-sample GBA-frame block into the buffer DMA will consume
 * at the next VBlank.  VBlank itself only stops/repoints/restarts FIFO A DMA.
 */
#define SAMPLES_PER_FRAME 528u
#define TIMER_RELOAD ((uint16_t)(65536u - 532u))
#define AUDIO_FRAME_TICKS 4389u /* 280896 GBA cycles / Timer3 prescale 64 */
#define AUDIO_LATE_TICKS (AUDIO_FRAME_TICKS + (AUDIO_FRAME_TICKS>>1))
#define AUDIO_SAME_VBLANK_TICKS (AUDIO_FRAME_TICKS>>1)

/* Source-recovery oracle for the original two one-frame mixer slots.
 * The live buffer below appends two guard frames per slot.  This type is
 * intentionally uninstantiated, so retaining the recovered shape costs no RAM. */
typedef struct { int8_t mix_buffer[2][SAMPLES_PER_FRAME]; } ps4_audio_legacy_mix_shape;

#if !PS4_MUSIC_UNIVERSAL_SEQUENCED
extern const uint8_t ps4_pcm_track_84_start[], ps4_pcm_track_84_end[];
extern const uint8_t ps4_pcm_track_87_start[], ps4_pcm_track_87_end[];
extern const uint8_t ps4_pcm_track_8A_start[], ps4_pcm_track_8A_end[];
extern const uint8_t ps4_pcm_track_8B_start[], ps4_pcm_track_8B_end[];
extern const uint8_t ps4_pcm_track_8C_start[], ps4_pcm_track_8C_end[];
extern const uint8_t ps4_pcm_track_8F_start[], ps4_pcm_track_8F_end[];
extern const uint8_t ps4_pcm_track_95_start[], ps4_pcm_track_95_end[];
extern const uint8_t ps4_pcm_track_9F_start[], ps4_pcm_track_9F_end[];
extern const uint8_t ps4_pcm_track_AC_start[], ps4_pcm_track_AC_end[];

typedef struct {
    uint8_t id;
    const uint8_t* data;
    uint32_t length;
    uint32_t loop_sample;
    uint32_t loop_bit;
    uint8_t width;
    uint8_t seed;
    uint8_t loop_seed;
    uint8_t one_shot;
} track_t;
#define TRACK_ROW(H) {0x##H##u,ps4_pcm_track_##H##_start,PS4_PCM_TRACK_##H##_LENGTH,PS4_PCM_TRACK_##H##_LOOP_SAMPLE,PS4_PCM_TRACK_##H##_LOOP_BIT,PS4_PCM_TRACK_##H##_WIDTH,PS4_PCM_TRACK_##H##_SEED,PS4_PCM_TRACK_##H##_LOOP_SEED,PS4_PCM_TRACK_##H##_ONE_SHOT}
static const track_t tracks[] = {
    TRACK_ROW(84), TRACK_ROW(87), TRACK_ROW(8A), TRACK_ROW(8B), TRACK_ROW(8C),
    TRACK_ROW(8F), TRACK_ROW(95), TRACK_ROW(9F), TRACK_ROW(AC),
};
#undef TRACK_ROW
#endif
extern const int8_t ps4_sfx_bank_start[], ps4_sfx_bank_end[];

#define REG16(a) (*(volatile uint16_t*)(a))
#define REG32(a) (*(volatile uint32_t*)(a))
#define REG_TM0CNT_L REG16(0x04000100)
#define REG_TM0CNT_H REG16(0x04000102)
#define REG_TM1CNT_L REG16(0x04000104)
#define REG_TM1CNT_H REG16(0x04000106)
#define REG_TM3CNT_L REG16(0x0400010C)
#define REG_TM3CNT_H REG16(0x0400010E)
#define REG_SOUNDCNT_H REG16(0x04000082)
#define REG_SOUNDCNT_X REG16(0x04000084)
#define REG_SOUNDBIAS REG16(0x04000088)
#define REG_FIFO_A REG32(0x040000A0)
#define DMA1SAD REG32(0x040000BC)
#define DMA1DAD REG32(0x040000C0)
#define DMA1CNT_L REG16(0x040000C4)
#define DMA1CNT_H REG16(0x040000C6)

/* R23: sample-clocked transport.  The source Genesis sound driver is still
 * advanced exactly once per display frame, but FIFO publication is no longer
 * phase-reset by VBlank IRQ latency. Timer0 runs continuously at 31,536 Hz and
 * Timer1 cascades exactly 528 Timer0 overflows (= 280,896 CPU cycles, exactly
 * one GBA frame) before requesting the next DMA block. This removes the global
 * drop/repeat seam caused by stopping Timer0 + clearing FIFO at each VBlank.
 * Source production remains at VCount line 0, safely ahead of the next audio
 * block boundary, so music tempo and SFX timing never depend on how long the
 * 68K main loop takes.  R5..R13 produced exactly one PCM frame per
 * PSIV main-loop waitvb(): any iteration longer than one display frame halved
 * music tempo and spliced guard blocks (30 Hz buzz), and multi-frame loads let
 * the repeating FIFO DMA walk past mix_buffer into EWRAM (framebuffers, MD RAM)
 * -- the loud static.  Host proof: tools_host/audio_sim/sim_r14.c.
 *   First VBlank : phase-lock/start Timer0 + cascaded Timer1 and FIFO DMA.
 *   Timer1 IRQ    : publish the next block at the exact 528-sample boundary;
 *                   if IRQ service is delayed, Timer1 itself gives the exact
 *                   number of already-consumed guard samples.
 *   Timer1 IRQ    : produce the following block after publication;
 *                   first start also produces once. VCount is disabled.
 * Build with -DPS4_AUDIO_IRQ_TRANSPORT=0 for the exact R13 polling transport. */
#ifndef PS4_AUDIO_IRQ_TRANSPORT
#define PS4_AUDIO_IRQ_TRANSPORT 1
#endif
#define REG_IME REG16(0x04000208)
#define REG_IE REG16(0x04000200)
#define REG_IF REG16(0x04000202)
#define REG_DISPSTAT REG16(0x04000004)
#define REG_VCOUNT REG16(0x04000006)
#define AUDIO_IRQ_VBLANK 0x0001u
#define AUDIO_IRQ_TIMER1 0x0010u
#define AUDIO_BLOCK_RELOAD ((uint16_t)(65536u-SAMPLES_PER_FRAME))
/* R14A: cheap counters, readable in any emulator memory viewer via the map
 * symbol ps4_audio_diag.  produce_ticks are Timer3 /64 ticks (x64 = cycles). */
typedef struct {
    uint32_t vblanks, productions, underruns, late_commits, waitvb_calls, commands;
    uint16_t max_offset_samples, max_produce_ticks, last_produce_ticks, pad;
} ps4_audio_diag_t;
/* EWRAM .bss has only ~40 bytes left below the 0x0202A000 presentation arena,
 * so R14 transport state lives in IWRAM (.iwram is a copied PROGBITS image:
 * these start at zero on every boot/warm launch). */
#define AUDIO_IWRAM_STATE __attribute__((section(".iwram.audio.state")))
AUDIO_IWRAM_STATE ps4_audio_diag_t ps4_audio_diag;
void ps4_audio_irq_entry(void);
void ps4_irq_cpu_enable(void);
/* R15: the R13 polling-transport bodies are no-ops under the IRQ transport;
 * keep them out of scarce IWRAM unless the polling transport is built. */
#if PS4_AUDIO_IRQ_TRANSPORT
#define AUDIO_POLL_HOT __attribute__((noinline))
#else
#define AUDIO_POLL_HOT __attribute__((section(".iwram.text"),noinline))
#endif
#define AUDIO_CS_ENTER() uint16_t audio_cs_ime=REG_IME; REG_IME=0u
#define AUDIO_CS_LEAVE() REG_IME=audio_cs_ime

/* enable | special/FIFO timing | 32-bit | repeat | destination fixed */
#define DMA_FIFO ((uint16_t)0xB640u)
/* FIFO A 100%, routed to both L/R, Timer0. Bit 11 resets FIFO A. */
#define DS_A_ROUTE ((uint16_t)0x0304u)
#define DS_A_RESET ((uint16_t)0x0800u)

/* Music state.  PCM is stored losslessly as fixed-width signed deltas plus
 * escape bytes.  The decoder reproduces the exact original signed 8-bit PCM
 * at the mixer boundary while recovering enough cartridge space for the
 * source shop/cave tracks. */
#if PS4_MUSIC_UNIVERSAL_SEQUENCED
/* Sequenced music is addressed by an absolute source-frame index that lives in
 * the snapshotted mixer state, so speculative/guard blocks stay exact: the
 * P134R sequencer renders forward into a small cache and a frame is released
 * only after the block that used it becomes FIFO-owned. */
#ifndef PS4_SEQ_CACHE_FRAMES
#define PS4_SEQ_CACHE_FRAMES 2u
#endif
#define PS4_SEQ_FRAME_SAMPLES 224u
static uint32_t music_seq;
static int8_t music_prev_sample;
static int8_t seq_cache[PS4_SEQ_CACHE_FRAMES][PS4_SEQ_FRAME_SAMPLES] __attribute__((aligned(4)));
static uint32_t seq_cache_base;
static uint8_t seq_cache_n;
/* CP22: first sequencer frame index after a source stop command (one-shot songs
 * $8B/$A3/$AB/$AD/$AE/$AF/$B2/$B3); 0xFFFFFFFF while the song is still live.
 * Renderer-frontier state, not snapshotted: it depends only on the song. */
static uint32_t seq_end_seq=0xFFFFFFFFu;
#else
static const uint8_t* music_data;
static uint32_t music_length,music_sample_pos,music_bit_pos;
static uint32_t music_loop_sample,music_loop_bit;
static uint8_t music_width,music_cur,music_loop_seed;
#endif
static uint8_t current_music_id;
static uint8_t music_active;
static uint8_t music_one_shot;
static int8_t music_frame[SAMPLES_PER_FRAME] __attribute__((aligned(4)));

/* Normal SFX ($B5-$F7) and special SFX ($F8-$FA) are separate source classes. */
static const int8_t* normal_start;
static uint32_t normal_len, normal_pos;
static uint8_t normal_id, normal_active;

/* R19: source sound-driver channel ownership.  Normal SFX replace only the
 * music hardware channels they actually use; sequencing continues underneath.
 * generated_songs lane order is DAC, FM1..FM5, PSG1..PSG3. */
static const uint16_t ps4_sfx_music_mute_mask[PS4_SFX_COUNT] = {
    0x020u,0x020u,0x020u,0x020u,0x020u,0x020u,0x020u,0x020u,0x030u,0x020u,
    0x020u,0x030u,0x030u,0x110u,0x120u,0x020u,0x030u,0x020u,0x020u,0x020u,
    0x020u,0x020u,0x020u,0x020u,0x020u,0x030u,0x030u,0x020u,0x020u,0x020u,
    0x020u,0x030u,0x020u,0x020u,0x020u,0x030u,0x020u,0x020u,0x020u,0x020u,
    0x030u,0x020u,0x020u,0x020u,0x020u,0x020u,0x020u,0x020u,0x020u,0x030u,
    0x020u,0x020u,0x030u,0x030u,0x020u,0x030u,0x030u,0x020u,0x020u,0x030u,
    0x030u,0x020u,0x020u,0x030u,0x020u,0x010u,0x020u,0x000u,0x000u,0x000u,
};
static uint16_t normal_music_mute_mask;
static const int8_t* special_start;
static uint32_t special_len, special_pos;
static uint8_t special_id, special_active;
/* PlaySpecialSFX owns music FM4 ($100(a6)); all three retail special
 * headers use hardware channel 4. StopSFX restores that special owner rather
 * than music, and StopSpcSFX retains an ordinary SFX owner on the same lane. */
#if defined(__arm__)
__attribute__((target("thumb"),noinline))
#endif
static void sync_music_mute_mask(void){ ps4_audio_fallback_set_mute_mask((normal_active?normal_music_mute_mask:0u)|(special_active?0x010u:0u)); }

/* Sound_StopMusic ($FB) is PSIV's fade command: normal + special SFX stop
 * immediately, then music fades in 15 source volume steps, one every two driver
 * updates.  A 16-based multiplier makes that exact cadence cheap on ARM7. */
static uint8_t fade_active, fade_level, fade_phase;

/* Candidate mixer state.  Preparing a future DMA block must not advance the
 * authoritative playback cursors until that block is actually owned by FIFO
 * playback.  Keep snapshots in BSS rather than on the already-constrained
 * System stack. */
typedef struct {
#if PS4_MUSIC_UNIVERSAL_SEQUENCED
    uint32_t music_seq; uint8_t current_music_id,music_active,music_one_shot; int8_t music_prev_sample;
#else
    const uint8_t* music_data; uint32_t music_length,music_sample_pos,music_bit_pos,music_loop_sample,music_loop_bit; uint8_t music_width,music_cur,music_loop_seed,current_music_id,music_active,music_one_shot;
#endif
    const int8_t* normal_start; uint32_t normal_len,normal_pos; uint16_t normal_music_mute_mask; uint8_t normal_id,normal_active;
    const int8_t* special_start; uint32_t special_len,special_pos; uint8_t special_id,special_active;
    uint8_t fade_active,fade_level,fade_phase;
} audio_state_t;
static audio_state_t prepared_end_state,prepared_guard_end_state,mix_start_state;
static uint8_t prepared_state_valid,prepared_guard_state_valid;

static void audio_state_capture(audio_state_t* s){
#if PS4_MUSIC_UNIVERSAL_SEQUENCED
    s->music_seq=music_seq;s->current_music_id=current_music_id;s->music_active=music_active;s->music_one_shot=music_one_shot;s->music_prev_sample=music_prev_sample;
#else
    s->music_data=music_data;s->music_length=music_length;s->music_sample_pos=music_sample_pos;s->music_bit_pos=music_bit_pos;s->music_loop_sample=music_loop_sample;s->music_loop_bit=music_loop_bit;s->music_width=music_width;s->music_cur=music_cur;s->music_loop_seed=music_loop_seed;s->current_music_id=current_music_id;s->music_active=music_active;s->music_one_shot=music_one_shot;
#endif
    s->normal_start=normal_start;s->normal_len=normal_len;s->normal_pos=normal_pos;s->normal_music_mute_mask=normal_music_mute_mask;s->normal_id=normal_id;s->normal_active=normal_active;
    s->special_start=special_start;s->special_len=special_len;s->special_pos=special_pos;s->special_id=special_id;s->special_active=special_active;
    s->fade_active=fade_active;s->fade_level=fade_level;s->fade_phase=fade_phase;
}
static void audio_state_restore(const audio_state_t* s){
#if PS4_MUSIC_UNIVERSAL_SEQUENCED
    music_seq=s->music_seq;current_music_id=s->current_music_id;music_active=s->music_active;music_one_shot=s->music_one_shot;music_prev_sample=s->music_prev_sample;
#else
    music_data=s->music_data;music_length=s->music_length;music_sample_pos=s->music_sample_pos;music_bit_pos=s->music_bit_pos;music_loop_sample=s->music_loop_sample;music_loop_bit=s->music_loop_bit;music_width=s->music_width;music_cur=s->music_cur;music_loop_seed=s->music_loop_seed;current_music_id=s->current_music_id;music_active=s->music_active;music_one_shot=s->music_one_shot;
#endif
    normal_start=s->normal_start;normal_len=s->normal_len;normal_pos=s->normal_pos;normal_music_mute_mask=s->normal_music_mute_mask;normal_id=s->normal_id;normal_active=s->normal_active;
    special_start=s->special_start;special_len=s->special_len;special_pos=s->special_pos;special_id=s->special_id;special_active=s->special_active;
    sync_music_mute_mask();
    fade_active=s->fade_active;fade_level=s->fade_level;fade_phase=s->fade_phase;
}

/* EWRAM ping-pong stream windows.  Each physical slot owns one 528-sample
 * primary block plus two guard blocks.  The guards are never the normal DMA
 * target; they make a late VBlank safe by giving a repeating FIFO DMA valid
 * continuation samples instead of letting it walk into unrelated EWRAM. */
static int8_t mix_buffer[2][SAMPLES_PER_FRAME*3u] __attribute__((aligned(4)));
static uint8_t dma_index, prepared_index, prepared_ready, prepared_catchup;
/* CP22 static fix: block (0 or 1) of mix_buffer[dma_index] at which FIFO DMA
 * started for the current commit.  After a late-VBlank catch-up the commit
 * starts at block 1, so that slot's continuation guards begin at block 2.
 * Earlier builds always wrote the next frame's guards to blocks 1/2, i.e. over
 * the block DMA was *currently playing* after a catch-up: an audible splice then
 * a repeated frame (the opening-scene "electric/stuck" static). */
static uint8_t dma_play_frame;
static uint8_t initialized, hw_started;
static uint16_t last_commit_tick;
/* R23 state: slot DMA plays, whether the other slot holds the next block,
 * and the exact Timer1-derived sample offset used after a delayed boundary. */
AUDIO_IWRAM_STATE static uint8_t irq_mode;
AUDIO_IWRAM_STATE static uint8_t irq_play;
AUDIO_IWRAM_STATE static uint8_t irq_ready;
AUDIO_IWRAM_STATE static uint16_t irq_off;
static const int8_t silence_block[SAMPLES_PER_FRAME*3u] __attribute__((aligned(4)))={0};
#if PS4_AUDIO_IRQ_TRANSPORT
static void audio_irq_start(void);
#endif

#if !PS4_MUSIC_UNIVERSAL_SEQUENCED
static const track_t* find_track(uint8_t id)
{
    unsigned i;
    for(i=0u;i<(unsigned)(sizeof(tracks)/sizeof(tracks[0]));++i)
        if(tracks[i].id==id) return &tracks[i];
    return (const track_t*)0;
}
#endif

static void stop_hw(void)
{
    REG_TM0CNT_H=0u;
    REG_TM1CNT_H=0u;
    DMA1CNT_H=0u;
    REG_SOUNDCNT_H=(uint16_t)(DS_A_ROUTE|DS_A_RESET);
    hw_started=0u;
}

#if PS4_MUSIC_UNIVERSAL_SEQUENCED
static void stop_music_state(void)
{
    music_seq=0u;current_music_id=0u;
    music_active=0u; music_one_shot=0u; fade_active=0u; fade_level=16u; fade_phase=0u;
}
static void seq_reset(void){ seq_cache_base=0u; seq_cache_n=0u; seq_end_seq=0xFFFFFFFFu;music_prev_sample=0; }
/* Drop cached source frames that precede the owned music position. */
static void seq_release_before(uint32_t seq)
{
    while(seq_cache_n&&seq_cache_base<seq){
        for(unsigned k=1u;k<seq_cache_n;k++){
            uint32_t* d=(uint32_t*)(void*)seq_cache[k-1u];const uint32_t* q=(const uint32_t*)(const void*)seq_cache[k];
            for(unsigned w=0u;w<PS4_SEQ_FRAME_SAMPLES/4u;w++)d[w]=q[w];
        }
        --seq_cache_n;++seq_cache_base;
    }
}
static const int8_t* seq_frame(uint32_t seq)
{
    if(seq<seq_cache_base)return (const int8_t*)0;   /* never rewinds: owned audio is immutable */
    while(seq>=seq_cache_base+seq_cache_n){
        if(seq_cache_n>=PS4_SEQ_CACHE_FRAMES)seq_release_before(seq_cache_base+1u);
        ps4_audio_fallback_render_224(seq_cache[seq_cache_n]);++seq_cache_n;
        if(seq_end_seq==0xFFFFFFFFu&&!ps4_audio_fallback_playing())seq_end_seq=seq_cache_base+seq_cache_n;
    }
    return seq_cache[seq-seq_cache_base];
}
/* Called whenever a prepared block becomes playback-owned. */
static void music_owned_sync(void)
{
    seq_release_before(music_seq);
    if(!music_active&&ps4_audio_fallback_playing()){ps4_audio_fallback_stop();seq_reset();}
}
#else
static void stop_music_state(void)
{
    music_data=(const uint8_t*)0;music_length=music_sample_pos=music_bit_pos=music_loop_sample=music_loop_bit=0u;
    music_width=music_cur=music_loop_seed=0u;current_music_id=0u;
    music_active=0u; music_one_shot=0u; fade_active=0u; fade_level=16u; fade_phase=0u;
}
static void music_owned_sync(void){}
#endif

static void stop_normal_state(void)
{
    normal_start=(const int8_t*)0; normal_len=normal_pos=0u; normal_id=0u; normal_active=0u;normal_music_mute_mask=0u;
    sync_music_mute_mask();
}

static void stop_special_state(void)
{
    special_start=(const int8_t*)0; special_len=special_pos=0u; special_id=0u; special_active=0u;
    sync_music_mute_mask();
}

static void invalidate_prepared(void){ if(irq_mode)return; prepared_ready=0u;prepared_catchup=0u;prepared_state_valid=0u;prepared_guard_state_valid=0u; }

#if PS4_MUSIC_UNIVERSAL_SEQUENCED
static int start_music(uint8_t id)
{
    if(!initialized) return 0;
    /* PlayMusic in the original driver relinquishes both SFX classes. */
    stop_normal_state(); stop_special_state();
    if(music_active && current_music_id==id && !fade_active){ invalidate_prepared(); return 1; }
    invalidate_prepared();
    if(!ps4_audio_fallback_play(id)){ stop_music_state(); seq_reset(); return 0; }
    seq_reset(); music_seq=0u;
    current_music_id=id; music_active=1u; music_one_shot=0u;
    fade_active=0u; fade_level=16u; fade_phase=0u;
    return 1;
}
#else
static int start_music(uint8_t id)
{
    const track_t* t;
    if(!initialized) return 0;
    /* PlayMusic in the original driver relinquishes both SFX classes. */
    stop_normal_state(); stop_special_state();
    if(music_active && current_music_id==id && !fade_active){ invalidate_prepared(); return 1; }
    t=find_track(id);
    if(!t){ stop_music_state(); invalidate_prepared(); return 0; }
    music_data=t->data;music_length=t->length;music_sample_pos=0u;music_bit_pos=0u;
    music_loop_sample=t->loop_sample;music_loop_bit=t->loop_bit;music_width=t->width;
    music_cur=t->seed;music_loop_seed=t->loop_seed;
    current_music_id=id; music_active=1u; music_one_shot=t->one_shot;
    fade_active=0u; fade_level=16u; fade_phase=0u; invalidate_prepared();
    return 1;
}
#endif

static const ps4_sfx_desc_t* sfx_desc(uint8_t id)
{
    if(id<PS4_SFX_FIRST_ID||id>PS4_SFX_LAST_ID) return (const ps4_sfx_desc_t*)0;
    return &ps4_sfx_descs[(unsigned)(id-PS4_SFX_FIRST_ID)];
}

static int start_normal(uint8_t id)
{
    const ps4_sfx_desc_t* d=sfx_desc(id);
    if(!d || (d->flags&PS4_SFX_FLAG_SPECIAL_LOOP)) return 0;
    stop_normal_state();
    if(!(d->flags&PS4_SFX_FLAG_SILENT) && d->length){
        normal_start=ps4_sfx_bank_start+d->offset; normal_len=d->length; normal_pos=0u;
        normal_id=id; normal_active=1u;
        normal_music_mute_mask=ps4_sfx_music_mute_mask[(unsigned)(id-PS4_SFX_FIRST_ID)];
        sync_music_mute_mask();
    }
    invalidate_prepared(); return 1;
}

static int start_special(uint8_t id)
{
    const ps4_sfx_desc_t* d=sfx_desc(id);
    if(!d || !(d->flags&PS4_SFX_FLAG_SPECIAL_LOOP)) return 0;
    stop_special_state();
    if(d->length){
        special_start=ps4_sfx_bank_start+d->offset; special_len=d->length; special_pos=0u;
        special_id=id; special_active=1u;
    }
    sync_music_mute_mask();
    invalidate_prepared(); return 1;
}

static void stop_all_state(void)
{
    stop_music_state(); stop_normal_state(); stop_special_state(); invalidate_prepared();
}

void ps4_audio_init(void)
{
    stop_hw();
    REG_SOUNDCNT_X=0x0080u;
    REG_SOUNDBIAS=0x0200u;
    REG_TM0CNT_L=TIMER_RELOAD;
    /* Free-running /64 timer used only to recognize a genuinely missed VBlank.
     * One GBA frame is exactly 4389 Timer3 ticks. */
    REG_TM3CNT_H=0u;REG_TM3CNT_L=0u;REG_TM3CNT_H=0x0081u;
#if PS4_MUSIC_UNIVERSAL_SEQUENCED
    music_seq=0u;seq_reset();ps4_audio_fallback_stop();
#else
    music_data=(const uint8_t*)0;
    music_length=music_sample_pos=music_bit_pos=music_loop_sample=music_loop_bit=0u;
    music_width=music_cur=music_loop_seed=0u;
#endif
    normal_start=special_start=(const int8_t*)0;
    normal_len=normal_pos=special_len=special_pos=0u;
    current_music_id=normal_id=special_id=0u;
    music_active=normal_active=special_active=music_one_shot=0u;normal_music_mute_mask=0u;
    ps4_audio_fallback_set_mute_mask(0u);
    fade_active=fade_phase=0u;fade_level=16u;
    dma_index=0u;dma_play_frame=0u;prepared_index=0u;prepared_ready=0u;prepared_catchup=0u;prepared_state_valid=0u;prepared_guard_state_valid=0u;
    last_commit_tick=REG_TM3CNT_L;hw_started=0u;initialized=1u;
#if PS4_AUDIO_IRQ_TRANSPORT
    if(!irq_mode)audio_irq_start();
#endif
}

#if PS4_MUSIC_UNIVERSAL_SEQUENCED
/* The IRQ transport owns each produced frame immediately. Avoid the cache's
 * 224-byte move when rollback is impossible; keep polling machinery in ROM. */
__attribute__((noinline))
static const int8_t* music_source_frame(void)
{
    if(!music_active)return (const int8_t*)0;
    if(irq_mode){
        ps4_audio_fallback_render_224(seq_cache[0]);
        ++music_seq;
        if(!ps4_audio_fallback_playing())stop_music_state();
        return seq_cache[0];
    }
    const int8_t* f=seq_frame(music_seq);
    if(f){++music_seq;if(music_seq>=seq_end_seq)stop_music_state();}
    return f;
}
/* One source frame (224 @13379 Hz) -> one GBA frame (528 @31536 Hz). */
__attribute__((section(".iwram.text"),noinline))
static void music_decode_frame(int8_t* out)
{
    const int8_t* f=music_source_frame();
    unsigned i;
    if(!f){for(i=0u;i<SAMPLES_PER_FRAME;i++)out[i]=0;return;}
    /* One-shot completion: once the frame containing the source stop is mixed,
     * the candidate state has no music (ownership then stops the sequencer), so
     * current_music reports 0 and replaying the same jingle starts it again. */
    /* R26 causal linear interpolation, including the preceding frame's final
     * sample. The source still advances exactly 224 samples per frame. */
    unsigned si=0u,acc=0u;
    for(i=0u;i<SAMPLES_PER_FRAME;i++){
        const int a=si ? f[si-1u] : music_prev_sample;
        const int b=f[si];
        out[i]=(int8_t)(a+(((b-a)*(int)acc*31)>>14));acc+=PS4_SEQ_FRAME_SAMPLES;
        if(acc>=SAMPLES_PER_FRAME){acc-=SAMPLES_PER_FRAME;if(si<PS4_SEQ_FRAME_SAMPLES-1u)++si;}
    }
    music_prev_sample=f[PS4_SEQ_FRAME_SAMPLES-1u];
}

#else
static inline unsigned music_read_bits(unsigned n)
{
    uint32_t bit=music_bit_pos;const uint8_t* p=music_data+(bit>>3);unsigned sh=(unsigned)(bit&7u);
    uint32_t v=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16);
    music_bit_pos=bit+n;return (unsigned)((v>>sh)&((1u<<n)-1u));
}
static inline int music_next_delta(void)
{
    unsigned w=(unsigned)music_width,code=music_read_bits(w),esc=(1u<<w)-1u;
    if(code==esc){unsigned q=music_read_bits(8u);return (int)(int8_t)(uint8_t)q;}
    return (int)code-(int)((1u<<(w-1u))-1u);
}
static void music_decode_frame(int8_t* out)
{
    unsigned i;
    for(i=0u;i<SAMPLES_PER_FRAME;i++){
        if(!music_active||!music_data||!music_length){while(i<SAMPLES_PER_FRAME)out[i++]=0;return;}
        if(music_sample_pos>=music_length){
            if(music_one_shot){stop_music_state();while(i<SAMPLES_PER_FRAME)out[i++]=0;return;}
            music_sample_pos=music_loop_sample;music_bit_pos=music_loop_bit;music_cur=music_loop_seed;
        }
        out[i]=(int8_t)music_cur;++music_sample_pos;
        if(music_sample_pos<music_length)music_cur=(uint8_t)(music_cur+(uint8_t)music_next_delta());
    }
}

#endif

static inline int normal_sample(void)
{
    int v;
    if(!normal_active||!normal_start||normal_pos>=normal_len){stop_normal_state();return 0;}
    v=(int)normal_start[normal_pos++];
    if(normal_pos>=normal_len) normal_active=0u;
    return v;
}

static inline int special_sample(void)
{
    int v;
    if(!special_active||!special_start||!special_len) return 0;
    if(special_pos>=special_len) special_pos=0u;
    v=(int)special_start[special_pos++];
    if(special_pos>=special_len) special_pos=0u;
    return v;
}

#if defined(__arm__)
__attribute__((target("thumb"),noinline))
#endif
static unsigned special_frame_audible(void)
{
    if(!special_active)return 0u;
    if(!normal_active||!(normal_music_mute_mask&0x010u))return 1u;
    /* Original PlaySFX suppresses the FM4 special object while its clock
     * continues. Resume its current position when the ordinary owner ends. */
    if(special_len){special_pos+=SAMPLES_PER_FRAME;while(special_pos>=special_len)special_pos-=special_len;}
    return 0u;
}

static inline int clamp_s8(int v)
{
    if(v>127) return 127;
    if(v<-128) return -128;
    return v;
}

static void audio_copy_block(const int8_t* src,int8_t* dst) __attribute__((section(".iwram.text"),noinline));
static void audio_copy_block(const int8_t* src,int8_t* dst)
{
#if defined(__arm__)
    /* 528 bytes = sixteen 32-byte bursts plus one 16-byte burst. */
    __asm__ volatile(
        "mov r2, #16\n"
        "1: ldmia %0!, {r3-r10}\n"
        "stmia %1!, {r3-r10}\n"
        "subs r2, r2, #1\n"
        "bne 1b\n"
        "ldmia %0!, {r3-r6}\n"
        "stmia %1!, {r3-r6}\n"
        : "+r"(src), "+r"(dst) :
        : "r2","r3","r4","r5","r6","r7","r8","r9","r10","cc","memory");
#else
    const uint32_t* s=(const uint32_t*)(const void*)src;uint32_t* d=(uint32_t*)(void*)dst;
    unsigned i=0u,words=SAMPLES_PER_FRAME/4u;
    for(;i+8u<=words;i+=8u){d[i]=s[i];d[i+1u]=s[i+1u];d[i+2u]=s[i+2u];d[i+3u]=s[i+3u];d[i+4u]=s[i+4u];d[i+5u]=s[i+5u];d[i+6u]=s[i+6u];d[i+7u]=s[i+7u];}
    for(;i<words;i++)d[i]=s[i];
#endif
}
/* Continuation guard k (1 = next block after the one DMA started on).  Only
 * blocks inside the 3-block slot exist; beyond that there is nothing safe to
 * overwrite, so the copy is skipped rather than landing on live samples. */
static void dma_guard_copy(const int8_t* out,unsigned k)
{
    unsigned b=(unsigned)dma_play_frame+k;
    if(b<=2u)audio_copy_block(out,mix_buffer[dma_index]+SAMPLES_PER_FRAME*b);
}

static void ps4_audio_mix_one_frame(int8_t* out) __attribute__((section(".iwram.text"),noinline));
static void ps4_audio_mix_one_frame(int8_t* out)
{
    unsigned i;
    uint8_t have_m,have_n,have_s;
    have_m=music_active; have_n=normal_active; have_s=(uint8_t)special_frame_audible();
    /* Decode BGM exactly once for this source frame.  This is lossless storage:
     * music_frame contains the same signed 8-bit samples as the former raw ROM
     * stream before any fade/headroom mixing is applied. */
    if(have_m&&!have_n&&!have_s&&!fade_active){music_decode_frame(out);return;}
    if(have_m)music_decode_frame(music_frame);
    switch((unsigned)have_m|((unsigned)have_n<<1)|((unsigned)have_s<<2)){
    case 0u:{
        uint32_t* z=(uint32_t*)(void*)out;unsigned words=SAMPLES_PER_FRAME/4u;
        for(i=0u;i+8u<=words;i+=8u){z[i]=0u;z[i+1u]=0u;z[i+2u]=0u;z[i+3u]=0u;z[i+4u]=0u;z[i+5u]=0u;z[i+6u]=0u;z[i+7u]=0u;}
        for(;i<words;i++)z[i]=0u;
        break;
    }
    case 1u:{
        const int level=(int)fade_level;
        for(i=0u;i<SAMPLES_PER_FRAME;i++){int m=((int)music_frame[i]*level)>>4;out[i]=(int8_t)clamp_s8(m);}
        break;
    }
    case 2u:{
        unsigned n=(normal_active&&normal_start&&normal_pos<normal_len)?(unsigned)(normal_len-normal_pos):0u;
        if(n>SAMPLES_PER_FRAME)n=SAMPLES_PER_FRAME;
        for(i=0u;i<n;i++)out[i]=normal_start[normal_pos+i];normal_pos+=n;
        if(normal_pos>=normal_len)normal_active=0u;
        for(;i<SAMPLES_PER_FRAME;i++)out[i]=0;
        break;
    }
    case 3u:
        if(fade_active){const int level=(int)fade_level;for(i=0u;i<SAMPLES_PER_FRAME;i++){int m=((int)music_frame[i]*level)>>4;int n=normal_sample();out[i]=(int8_t)clamp_s8(((m*3)>>2)+n);}}
        else for(i=0u;i<SAMPLES_PER_FRAME;i++){int m=music_frame[i];int n=normal_sample();out[i]=(int8_t)clamp_s8(((m*3)>>2)+n);}
        break;
    case 4u:{
        unsigned di=0u;
        if(!special_active||!special_start||!special_len){for(i=0u;i<SAMPLES_PER_FRAME;i++)out[i]=0;break;}
        while(di<SAMPLES_PER_FRAME){if(special_pos>=special_len)special_pos=0u;unsigned n=(unsigned)(special_len-special_pos),left=SAMPLES_PER_FRAME-di;if(n>left)n=left;for(i=0u;i<n;i++)out[di+i]=special_start[special_pos+i];special_pos+=n;di+=n;}
        if(special_pos>=special_len)special_pos=0u;
        break;
    }
    case 5u:
        if(fade_active){const int level=(int)fade_level;for(i=0u;i<SAMPLES_PER_FRAME;i++){int m=((int)music_frame[i]*level)>>4;int q=special_sample();out[i]=(int8_t)clamp_s8((m>>1)+(q>>1));}}
        else for(i=0u;i<SAMPLES_PER_FRAME;i++){int m=music_frame[i];int q=special_sample();out[i]=(int8_t)clamp_s8((m>>1)+(q>>1));}
        break;
    case 6u:
        for(i=0u;i<SAMPLES_PER_FRAME;i++){int n=normal_sample();int q=special_sample();out[i]=(int8_t)clamp_s8((n>>1)+(q>>1));}
        break;
    default:
        if(fade_active){const int level=(int)fade_level;for(i=0u;i<SAMPLES_PER_FRAME;i++){int m=((int)music_frame[i]*level)>>4;int n=normal_sample();int q=special_sample();out[i]=(int8_t)clamp_s8((m>>1)+((n*3)>>2)+(q>>2));}}
        else for(i=0u;i<SAMPLES_PER_FRAME;i++){int m=music_frame[i];int n=normal_sample();int q=special_sample();out[i]=(int8_t)clamp_s8((m>>1)+((n*3)>>2)+(q>>2));}
        break;
    }
    if(!normal_active && normal_music_mute_mask){normal_music_mute_mask=0u;sync_music_mute_mask();}
    if(fade_active){fade_phase^=1u;if(!fade_phase){if(fade_level)--fade_level;if(!fade_level)stop_music_state();}}
}

static void ps4_audio_catchup_one_frame(void) AUDIO_POLL_HOT;
static void ps4_audio_catchup_one_frame(void)
{
    int8_t* out;
    if(!initialized||!hw_started||!prepared_ready||prepared_catchup||!prepared_state_valid)return;
    /* Guard #1 has become playback without a serviced VBlank.  If a heavy
     * presenter asked for look-ahead, guard #2 already contains the exact
     * following PCM block and its candidate end-state.  Promote that block
     * without decoding again in the late-VBlank path.  Otherwise retain the
     * original transactional catch-up as a bounded fallback. */
    if(prepared_guard_state_valid){
        audio_state_restore(&prepared_end_state);      /* guard #1 is owned */
        music_owned_sync();
        audio_state_capture(&mix_start_state);         /* new committed base */
        audio_state_restore(&prepared_guard_end_state);
        audio_state_capture(&prepared_end_state);      /* candidate after guard #2 */
        audio_state_restore(&mix_start_state);
        prepared_guard_state_valid=0u;prepared_catchup=1u;return;
    }
    audio_state_restore(&prepared_end_state);
    music_owned_sync();
    audio_state_capture(&mix_start_state);
    out=mix_buffer[prepared_index];
    ps4_audio_mix_one_frame(out);
    audio_state_capture(&prepared_end_state);
    audio_state_restore(&mix_start_state);
    audio_copy_block(out,mix_buffer[prepared_index]+SAMPLES_PER_FRAME);
    audio_copy_block(out,mix_buffer[prepared_index]+SAMPLES_PER_FRAME*2u);
    dma_guard_copy(out,2u);
    prepared_state_valid=1u;prepared_catchup=1u;
}

void ps4_audio_late_vblank(void)
{
    if(irq_mode)return;
    /* R11: never synthesize an extra PCM frame from inside a late VBlank.
     * Catch-up mixing can turn one missed deadline into sustained CPU debt.
     * The normal ping-pong/guard blocks remain authoritative. */
    (void)initialized;
}

AUDIO_POLL_HOT
void ps4_audio_prepare_frame(void)
{
    int8_t* out;
    if(irq_mode){++ps4_audio_diag.waitvb_calls;return;}
    if(!initialized)return;
    if(prepared_ready){
        /* R11: a prepared block is already valid.  Do not spend another full
         * mixer frame trying to catch up; that work can itself cause the next
         * VBlank miss. */
        return;
    }
    prepared_index=(uint8_t)(dma_index^1u);
    out=mix_buffer[prepared_index];
    /* Mix against a snapshot, retain only the candidate end-state, and restore
     * committed playback state until the block is actually consumed. */
    audio_state_capture(&mix_start_state);
    ps4_audio_mix_one_frame(out);
    audio_state_capture(&prepared_end_state);
    audio_state_restore(&mix_start_state);
    prepared_state_valid=1u;
    audio_copy_block(out,mix_buffer[prepared_index]+SAMPLES_PER_FRAME);
    dma_guard_copy(out,1u);
    dma_guard_copy(out,2u);
    prepared_guard_state_valid=0u;prepared_catchup=0u;prepared_ready=1u;
}

/* Heavy title/opening composition can occasionally occupy more than one
 * display interval.  Precompute exactly one additional future PCM frame only
 * when those presenters request it.  Normal gameplay keeps the cheaper
 * one-frame preparation path.  The extra block is installed as current DMA
 * guard #2 and as the prepared slot's first guard, so a single missed VBlank
 * advances A->B->C instead of replaying B (the audible "electric/stuck"
 * symptom reported during the startup scroll). */
AUDIO_POLL_HOT
void ps4_audio_prepare_guard_frame(void)
{
    int8_t* out;
    if(irq_mode)return;
    if(!initialized)return;
    ps4_audio_prepare_frame();
    if(!prepared_ready||prepared_catchup||!prepared_state_valid||prepared_guard_state_valid)return;
    audio_state_restore(&prepared_end_state);
    out=mix_buffer[prepared_index]+SAMPLES_PER_FRAME;
    ps4_audio_mix_one_frame(out);
    audio_state_capture(&prepared_guard_end_state);
    audio_state_restore(&mix_start_state);
    audio_copy_block(out,mix_buffer[prepared_index]+SAMPLES_PER_FRAME*2u);
    dma_guard_copy(out,2u);
    prepared_guard_state_valid=1u;
}

AUDIO_POLL_HOT
void ps4_audio_vblank_commit(void)
{
    const int8_t* src;
    if(irq_mode) return;
    if(!initialized) return;
    if(!prepared_ready){
        if(!music_active&&!normal_active&&!special_active) stop_hw();
        return;
    }
    src=mix_buffer[prepared_index]+(prepared_catchup?SAMPLES_PER_FRAME:0u);
    REG_TM0CNT_H=0u;
    DMA1CNT_H=0u;
    REG_SOUNDCNT_H=(uint16_t)(DS_A_ROUTE|DS_A_RESET);
    REG_TM0CNT_L=TIMER_RELOAD;
    DMA1SAD=(uint32_t)(uintptr_t)src;
    DMA1DAD=(uint32_t)(uintptr_t)&REG_FIFO_A;
    DMA1CNT_L=4u;
    DMA1CNT_H=DMA_FIFO;
    REG_TM0CNT_H=0x0080u;
    dma_index=prepared_index;dma_play_frame=(uint8_t)(prepared_catchup?1u:0u);
    if(prepared_state_valid){audio_state_restore(&prepared_end_state);music_owned_sync();}
    prepared_ready=0u;prepared_catchup=0u;prepared_state_valid=0u;prepared_guard_state_valid=0u;
    last_commit_tick=REG_TM3CNT_L;hw_started=1u;
}

#if PS4_AUDIO_IRQ_TRANSPORT
/* First publication starts the sample clock.  Timer1 cascades Timer0 and
 * overflows after exactly 528 output samples; after this point Timer0 is never
 * stopped/reloaded by the per-frame transport. */
static void audio_irq_hw_first_start(const int8_t* src)
{
    REG_TM0CNT_H=0u;
    REG_TM1CNT_H=0u;
    DMA1CNT_H=0u;
    REG_SOUNDCNT_H=(uint16_t)(DS_A_ROUTE|DS_A_RESET);
    /* Seed a full DMA burst before starting the timer. FIFO-timed DMA is
     * requested by sample consumption; enabling it alone leaves FIFO empty. */
    const uint32_t* seed=(const uint32_t*)(const void*)src;
    REG_FIFO_A=seed[0];REG_FIFO_A=seed[1];REG_FIFO_A=seed[2];REG_FIFO_A=seed[3];
    REG_TM0CNT_L=TIMER_RELOAD;
    REG_TM1CNT_L=AUDIO_BLOCK_RELOAD;
    DMA1SAD=(uint32_t)(uintptr_t)(src+16u);
    DMA1DAD=(uint32_t)(uintptr_t)&REG_FIFO_A;
    DMA1CNT_L=4u;
    DMA1CNT_H=DMA_FIFO;
    /* Timer1: enable + IRQ + cascade.  Start it before Timer0 so the very first
     * sample overflow is counted. */
    REG_TM1CNT_H=0x00C4u;
    REG_TM0CNT_H=0x0080u;
    last_commit_tick=REG_TM3CNT_L;hw_started=1u;
}

/* Keep the FIFO and sample clock running. Wait for a stable sample phase
 * after DMA has settled, then repoint the next 16-byte burst. The 16-byte
 * startup seed establishes this cadence: consumption loads the word at sample
 * 1, and the FIFO request observes the reduced queue at sample 2 (modulo 16).
 * Thus offsets 0/1 retain a burst ending at 16; offsets 2..17 end at 32. */
static void audio_irq_hw_repoint(const int8_t* src)
{
    uint16_t phase;
    do{phase=REG_TM0CNT_L;}while(phase>0xFF80u || phase<0xFE10u);
    DMA1CNT_H=0u;
    unsigned off=(uint16_t)(REG_TM1CNT_L-AUDIO_BLOCK_RELOAD);
    if(off>=SAMPLES_PER_FRAME)off=0u;
    DMA1SAD=(uint32_t)(uintptr_t)(src+((off+14u)&~15u)+16u);
    DMA1DAD=(uint32_t)(uintptr_t)&REG_FIFO_A;
    DMA1CNT_L=4u;
    DMA1CNT_H=DMA_FIFO;
    last_commit_tick=REG_TM3CNT_L;
}

/* VBlank only performs the one-time phase-lock/start. Keep this cold path in
 * ROM; normal per-frame VBlank bookkeeping stays in the tiny IWRAM service. */
__attribute__((noinline))
static void audio_irq_vblank_start(void)
{
    if(!irq_ready){++ps4_audio_diag.underruns;irq_off=0u;audio_irq_hw_first_start(silence_block);return;}
    irq_play^=1u;irq_ready=0u;irq_off=0u;
    audio_irq_hw_first_start(mix_buffer[irq_play]);
}

/* Timer1 is phase-locked to the DAC sample clock. If IRQ service itself was
 * delayed, its live cascade count gives the exact number of samples already
 * consumed from the current slot's guard. Preserve the partly consumed word
 * and repoint future samples using a stable live offset. */
__attribute__((section(".iwram.text"),noinline))
static void audio_irq_commit(void)
{
    unsigned off=(uint16_t)(REG_TM1CNT_L-AUDIO_BLOCK_RELOAD);
    /* Word-rounded offset is diagnostic only; DMA publication reads its own
     * live offset after the sample-phase gate. */
    if(off>=SAMPLES_PER_FRAME)off=0u;
    off=(off+3u)&~3u;
    if(off){++ps4_audio_diag.late_commits;if(off>ps4_audio_diag.max_offset_samples)ps4_audio_diag.max_offset_samples=(uint16_t)off;}
    if(!irq_ready){
        ++ps4_audio_diag.underruns;
        /* A missed production deadline is already an audible discontinuity.
         * Continuing from the delayed Timer1 offset keeps the transport phase
         * inherited from the starvation event and can leave subsequent blocks
         * crackling/stuttering even after the renderer becomes cheap again.
         * Publish one bounded silence frame and hard re-phase both cascaded
         * timers/FIFO. The boundary producer has a full frame to produce the next
         * source block, which resumes from the unchanged sequencer state. */
        irq_off=0u;
        audio_irq_hw_first_start(silence_block);
        return;
    }
    irq_play^=1u;irq_ready=0u;irq_off=(uint16_t)off;
    audio_irq_hw_repoint(mix_buffer[irq_play]);
}
/* Produce exactly once per sample-clock block, immediately after publication
 * (also after first start). A delayed first VBlank can move Timer1 relative to
 * display scanlines; a fixed VCount producer can then finish after guard reads
 * have started. Timer1 scheduling gives the entire next block to fill its guard.
 * Main-loop commands under AUDIO_CS apply to the next production. */
__attribute__((noinline))
static void audio_irq_produce(void)
{
    uint16_t t0;int8_t* out;
    if(irq_ready)return;
    t0=REG_TM3CNT_L;
    out=mix_buffer[irq_play^1u];
    ps4_audio_mix_one_frame(out);
    music_owned_sync();
    audio_copy_block(out,mix_buffer[irq_play]+SAMPLES_PER_FRAME);
    irq_ready=1u;++ps4_audio_diag.productions;
    t0=(uint16_t)(REG_TM3CNT_L-t0);ps4_audio_diag.last_produce_ticks=t0;
    if(t0>ps4_audio_diag.max_produce_ticks)ps4_audio_diag.max_produce_ticks=t0;
}
static void audio_irq_start(void)
{
    REG_IME=0u;
    for(unsigned b=0u;b<2u;b++)for(unsigned i=0u;i<SAMPLES_PER_FRAME*3u;i++)mix_buffer[b][i]=0;
    irq_play=0u;irq_ready=1u;irq_off=0u;hw_started=0u;irq_mode=1u;
    REG_TM1CNT_H=0u;REG_TM1CNT_L=AUDIO_BLOCK_RELOAD;
    *(void (* volatile*)(void))0x03007FFCu=ps4_audio_irq_entry;
    REG_DISPSTAT=0x0008u;
    REG_IF=0xFFFFu;
    REG_IE=(uint16_t)(AUDIO_IRQ_VBLANK|AUDIO_IRQ_TIMER1);
    REG_IME=1u;
    ps4_irq_cpu_enable();
}
#endif
/* Called by gba/ps4_audio_irq.s (IRQ mode, dedicated IRQ stack). */
__attribute__((section(".iwram.text"),noinline))
void ps4_audio_irq_service(unsigned pending)
{
#if PS4_AUDIO_IRQ_TRANSPORT
    if(!irq_mode)return;
    /* Audio boundary first: it has only one sample period before the next FIFO
     * byte is due. VBlank is bookkeeping/first-start only. */
    if(pending&AUDIO_IRQ_TIMER1){audio_irq_commit();audio_irq_produce();}
    if(pending&AUDIO_IRQ_VBLANK){++ps4_audio_diag.vblanks;if(!hw_started){audio_irq_vblank_start();audio_irq_produce();}}
#else
    (void)pending;
#endif
}

void ps4_audio_frame_service(void)
{
    ps4_audio_prepare_frame();
    ps4_audio_vblank_commit();
}

static void audio_commit_consumed_guard_before_command(void){
    if(irq_mode)return;
    if(!initialized||!hw_started||!prepared_ready||!prepared_state_valid)return;
    /* Once the previous primary block has lasted one exact GBA frame, guard #1
     * is playback-owned.  After catch-up, the new candidate is guard #2 and
     * becomes playback-owned at two frames from last_commit_tick.  Do not use
     * the looser 1.5-frame missed-VBlank heuristic here: a command must never
     * discard state for samples the FIFO has already started consuming. */
    uint16_t elapsed=(uint16_t)(REG_TM3CNT_L-last_commit_tick);
    uint16_t owned_at=(uint16_t)(prepared_catchup?(AUDIO_FRAME_TICKS*2u):AUDIO_FRAME_TICKS);
    if(elapsed>=owned_at){
        audio_state_restore(&prepared_end_state);
        music_owned_sync();
        prepared_ready=0u;prepared_catchup=0u;prepared_state_valid=0u;prepared_guard_state_valid=0u;
    }
}

static int audio_sound_index_locked(uint8_t id);
int ps4_audio_sound_index(uint8_t id)
{
    int r;
    AUDIO_CS_ENTER();
    ++ps4_audio_diag.commands;
    r=audio_sound_index_locked(id);
    AUDIO_CS_LEAVE();
    return r;
}
static int audio_sound_index_locked(uint8_t id)
{
    if(!initialized) return 0;
    audio_commit_consumed_guard_before_command();
    /* CP22: source DoSoundQueue ($D05F8) reads each queue byte, clears it, and
     * does `subi.b #$81,d0 / bcs skip`: any request below $81 (including $00
     * and $80) is ignored and never reaches PlaySoundID.  Earlier builds
     * treated $00-$7F as StopAll, so a dynamic Sound_Index write carrying 0
     * (text actions, generated event/cutscene/ability cue tables) silently
     * killed the current BGM.  Stopping is only ever $FB-$FE or an explicit
     * native ps4_audio_stop() call. */
    if(id<0x81u) return 1;
    if(id>=0x81u&&id<=0xB4u) return start_music(id);
    if(id>=0xB5u&&id<=0xF7u) return start_normal(id);
    if(id>=0xF8u&&id<=0xFAu) return start_special(id);
    if(id==0xFBu){
        /* FadeOut: source stops both SFX classes immediately, then fades music. */
        stop_normal_state();stop_special_state();
        if(music_active){fade_active=1u;fade_level=15u;fade_phase=0u;}
        invalidate_prepared(); return 1;
    }
    if(id==0xFCu){stop_normal_state();invalidate_prepared();return 1;}
    if(id==0xFDu){stop_special_state();invalidate_prepared();return 1;}
    if(id==0xFEu){ps4_audio_stop();return 1;}
    return 0;
}

int ps4_audio_play(uint8_t id)
{
    return ps4_audio_sound_index(id);
}

void ps4_audio_stop(void)
{
    AUDIO_CS_ENTER();
    if(initialized&&irq_mode){
        /* Keep the sample-clocked transport running; silence every queued
         * block so the stop is audible immediately, as R13's stop_hw() was. */
        for(unsigned b=0u;b<2u;b++)for(unsigned i=0u;i<SAMPLES_PER_FRAME*3u;i++)mix_buffer[b][i]=0;
    }else if(initialized){
        /* DMA/FIFO can replay the last ping-pong block while the title/opening
         * path is doing expensive decompression.  Stop the hardware first,
         * then erase both source blocks and the FIFO latch so a later restart
         * begins from silence rather than stale Direct-Sound bytes. */
        stop_hw();
        for(unsigned b=0u;b<2u;b++)for(unsigned i=0u;i<SAMPLES_PER_FRAME*3u;i++)mix_buffer[b][i]=0;
        dma_index=0u;dma_play_frame=0u;prepared_index=0u;prepared_ready=0u;prepared_catchup=0u;prepared_state_valid=0u;prepared_guard_state_valid=0u;REG_FIFO_A=0u;
    }
    stop_all_state();
#if PS4_MUSIC_UNIVERSAL_SEQUENCED
    ps4_audio_fallback_stop();seq_reset();
#endif
    AUDIO_CS_LEAVE();
}

uint8_t ps4_audio_current_music(void){ return current_music_id; }
uint8_t ps4_audio_current_sfx(void){ return normal_id; }
uint8_t ps4_audio_current_special_sfx(void){ return special_id; }
