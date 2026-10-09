#include <stdint.h>
#include <stddef.h>
#include "generated_simple_events_v0982.h"
#include "generated_cutscene_scripts_v0982.h"
#include "md_io_runtime.h"
#include "md_bus_runtime.h"
#include "precomp_assets.h"
#include "title_intro_rows.h"
#include "ps4_audio_runtime.h"
#include "ps4_window_assets.h"
#include "psc_field_predecode.h"
#include "psc_map_desc.h"
#include "psc_runtime_plan.h"
#include "psc_panel_bank.h"
#include "psc_field_fastassets.h"
#include "source_ability_effects.h"
#include "source_combo_effects.h"
#include "source_enemy_effects.h"
#include "r28_readable_font.h"
#include "r29_wren_load.h"
#include "r28_debug_names.h"
#include "r28_vehicle_assets.h"

_Static_assert(sizeof(Ps4SourceEnemyDesc)==10u,"enemy source descriptor must stay compact");
_Static_assert(sizeof(Ps4SourceAbilityDesc)==6u,"ability source descriptor must stay compact");
_Static_assert(sizeof(Ps4SourceComboDesc)==6u,"combo source descriptor must stay compact");
_Static_assert(sizeof(ps4_source_enemy_palettes[0])==32u,"enemy palette stride must remain DMA32-safe");
_Static_assert(sizeof(ps4_source_ability_palettes[0])==32u,"ability palette stride must remain DMA32-safe");
_Static_assert(sizeof(ps4_source_combo_palettes[0])==32u,"combo palette stride must remain DMA32-safe");

#define REG_DISPCNT  (*(volatile uint16_t*)0x04000000)
#define REG_VCOUNT   (*(volatile uint16_t*)0x04000006)
#define REG_KEYINPUT (*(volatile uint16_t*)0x04000130)
#define REG_TM2CNT_L (*(volatile uint16_t*)0x04000108)
#define REG_TM2CNT_H (*(volatile uint16_t*)0x0400010A)
#define REG_BLDCNT   (*(volatile uint16_t*)0x04000050)
#define REG_BLDY     (*(volatile uint16_t*)0x04000054)
#define REG_BG0CNT  (*(volatile uint16_t*)0x04000008)
#define REG_BG1CNT  (*(volatile uint16_t*)0x0400000A)
#define REG_BG2CNT  (*(volatile uint16_t*)0x0400000C)
#define REG_BG3CNT  (*(volatile uint16_t*)0x0400000E)
#define REG_BG0HOFS (*(volatile uint16_t*)0x04000010)
#define REG_BG0VOFS (*(volatile uint16_t*)0x04000012)
#define REG_BG1HOFS (*(volatile uint16_t*)0x04000014)
#define REG_BG1VOFS (*(volatile uint16_t*)0x04000016)
#define REG_BG2HOFS (*(volatile uint16_t*)0x04000018)
#define REG_BG2VOFS (*(volatile uint16_t*)0x0400001A)
#define REG_BG3HOFS (*(volatile uint16_t*)0x0400001C)
#define REG_BG3VOFS (*(volatile uint16_t*)0x0400001E)
#define VRAM16 ((volatile uint16_t*)0x06000000)
#define BG_PALETTE ((volatile uint16_t*)0x05000000)
#define OBJ_PALETTE ((volatile uint16_t*)0x05000200)
#define OBJ_VRAM16 ((volatile uint16_t*)0x06010000)
#define OAM16 ((volatile uint16_t*)0x07000000)
#define MODE4_VRAM16 ((volatile uint16_t*)0x06000000)
#define MODE4_PAGE1 ((volatile uint16_t*)0x0600A000)
#define MODE4_FRAME1 (1u<<4)
#define REG_DMA0SAD (*(volatile uint32_t*)0x040000B0)
#define REG_DMA0DAD (*(volatile uint32_t*)0x040000B4)
#define REG_DMA0CNT (*(volatile uint32_t*)0x040000B8)
#define REG_DMA3SAD (*(volatile uint32_t*)0x040000D4)
#define REG_DMA3DAD (*(volatile uint32_t*)0x040000D8)
#define REG_DMA3CNT (*(volatile uint32_t*)0x040000DC)
#define DMA_ENABLE (1u<<31)
#define DMA_32BIT  (1u<<26)
#define DMA_SRC_FIXED (2u<<23)
#define DMA_DST_RELOAD (3u<<21)
#define DMA_REPEAT (1u<<25)
#define DMA_START_HBLANK (2u<<28)
#define MODE0 0
#define MODE3 3
#define MODE4 4
#define BG0_ENABLE (1u<<8)
#define BG1_ENABLE (1u<<9)
#define BG2_ENABLE (1u<<10)
#define BG3_ENABLE (1u<<11)
#define OBJ_ENABLE (1u<<12)
#define OBJ_1D (1u<<6)
#define FORCE_BLANK (1u<<7)
#define BLD_TGT_BG2 (1u<<2)
#define KEY_START (1u<<3)
#define KEY_SELECT (1u<<2)

extern const uint8_t ps4_reversed_rom[];
extern int ps4_nemesis_decode(uint8_t* work_ram, uint32_t source_address, uint32_t dest_address);

/* PSC's PSII/PSIII GBA compatibility code uses DMA3 for VRAM/OBJ transfers.
 * Keep all large copies/clears on the GBA's native 16/32-bit buses; byte stores
 * to VRAM are invalid for packed graphics because the GBA mirrors an 8-bit write
 * across the containing halfword. */
static inline void dma_compiler_fence(void){ __asm__ volatile("" ::: "memory"); }
static void dma_copy16(const void* src,void* dst,unsigned bytes){
    if(!bytes) return;
    dma_compiler_fence();
    REG_DMA3SAD=(uint32_t)(uintptr_t)src; REG_DMA3DAD=(uint32_t)(uintptr_t)dst;
    REG_DMA3CNT=DMA_ENABLE|(bytes>>1);
    while(REG_DMA3CNT&DMA_ENABLE){}
    dma_compiler_fence();
}
static void dma_copy32(const void* src,void* dst,unsigned bytes){
    if(!bytes) return;
    dma_compiler_fence();
    REG_DMA3SAD=(uint32_t)(uintptr_t)src; REG_DMA3DAD=(uint32_t)(uintptr_t)dst;
    REG_DMA3CNT=DMA_ENABLE|DMA_32BIT|(bytes>>2);
    while(REG_DMA3CNT&DMA_ENABLE){}
    dma_compiler_fence();
}
static volatile uint32_t dma_fill_seed;
static void dma_fill32(void* dst,unsigned bytes,uint32_t value){
    if(!bytes) return;
    /* Keep the fixed DMA source in stable EWRAM/IWRAM-visible storage.  A local
     * stack seed is legal in principle, but proved compiler/order-sensitive in
     * the PSCSTYLE comparison build and produced striped clears in mGBA. */
    dma_fill_seed=value;
    dma_compiler_fence();
    REG_DMA3SAD=(uint32_t)(uintptr_t)&dma_fill_seed;
    REG_DMA3DAD=(uint32_t)(uintptr_t)dst;
    REG_DMA3CNT=DMA_ENABLE|DMA_32BIT|DMA_SRC_FIXED|(bytes>>2);
    while(REG_DMA3CNT&DMA_ENABLE){}
    dma_compiler_fence();
}

/* PSC-style immutable-stream fast path.  PSII/PSIII in Phantasy Star
 * Collection move substantial work into native/generated ROM data.  Do the
 * same for PSIV's immutable field streams: the host build decodes every
 * reachable map Kosinski/Nemesis stream once, keyed by its original 68k ROM
 * address.  Runtime preserves source-address semantics but turns a decode into
 * a ROM->EWRAM DMA/copy.  Unknown/dynamic streams fall through to the exact
 * canonical decoders below. */
static int psc_predecode_copy(unsigned codec,uint32_t src,uint8_t* out,unsigned cap,unsigned* copied){
    const uint8_t* data;unsigned total;if(!ps4_psc_predecode_lookup(codec,src,&data,&total))return 0;if(total>cap){*copied=0u;return 1;}
    unsigned remain=total,words=remain&~3u;
    if(words&&(((uintptr_t)out&3u)==0u)){dma_copy32(data,out,words);data+=words;out+=words;remain-=words;}
    while(remain--)*out++=*data++;
    *copied=total;return 1;
}

/* Standard GBA save-type marker used by mGBA and other emulators for unknown/homebrew ROMs. */
__attribute__((used,section(".rodata.saveid"))) static const char gba_save_type[]="SRAM_V110";

static uint8_t* const work_ram=(uint8_t*)0x02000000;
static uint8_t* const md_vram=(uint8_t*)0x02010000;
static uint16_t* const plane_a=(uint16_t*)0x02020000;
static uint16_t* const plane_b=(uint16_t*)0x02021000;
static uint16_t* const eni_tmp=(uint16_t*)0x02022000;
static uint16_t* const cram=(uint16_t*)0x02023000;
/* Dedicated no-init EWRAM shadow.  v0.9.0 accidentally left this at
 * 0x02026000 while the new audio .bss grew through 0x02026DE5.  That made the
 * title/cutscene renderer overwrite mixer state and let audio DMA buffers scribble
 * into manga frames.  Keep the linker-owned EWRAM below 0x0202A000 and reserve
 * the entire upper arena explicitly for lifetime-reused presentation scratch. */
static uint8_t* const title_fb=(uint8_t*)0x0202A000;

/* Title-dissolve acceleration scratch.  These regions sit above title_fb and below
 * the end of EWRAM (0x02040000).  After Start they are dead; field dialogue may
 * safely reuse part of this arena.  They remain outside .bss for the same
 * reason.  The cache is presentation-only: PSIV's virtual MD VRAM/planes remain
 * authoritative.  This pattern is suitable for the MD2GBA renderer backend too:
 * precompute source-pixel dependencies once, then update only pixels affected by
 * a source tile mutation instead of re-running a full compositor every VBlank. */
#define TITLE_LOGO_X0 30u
#define TITLE_LOGO_Y0 18u
#define TITLE_LOGO_X1 203u
#define TITLE_LOGO_Y1 114u
#define TITLE_LOGO_W (TITLE_LOGO_X1-TITLE_LOGO_X0+1u)
#define TITLE_LOGO_H (TITLE_LOGO_Y1-TITLE_LOGO_Y0+1u)
#define TITLE_LOGO_PIXELS (TITLE_LOGO_W*TITLE_LOGO_H)
#define TITLE_TOKEN_NONE 0xFFFFu
static uint16_t* const title_logo_token=(uint16_t*)0x02033600;
static uint8_t* const title_logo_fallback=(uint8_t*)0x0203BA00;

typedef uint16_t ps4_u16_alias __attribute__((may_alias));
static inline uint8_t rom8(uint32_t a){ return ps4_reversed_rom[0x2FFFFFu-(a&0xFFFFFFu)]; }
static inline uint16_t rom16(uint32_t a){
    uint32_t q=a&0xFFFFFFu;
    /* Reversed storage turns an even-addressed MD big-endian word into one
     * naturally aligned little-endian ARM halfword at 0x2FFFFE-q. */
    if(!(q&1u)&&q<=0x2FFFFEu)return *(const ps4_u16_alias*)(const void*)(ps4_reversed_rom+0x2FFFFEu-q);
    return ((uint16_t)rom8(q)<<8)|rom8(q+1u);
}
static inline uint32_t rom32(uint32_t a){ return ((uint32_t)rom16(a)<<16)|rom16(a+2u); }

/* Optimized native lowering of PSIV Nemesis semantics.  The mechanically
 * translated $041752 implementation remains linked as a golden/reference path,
 * but its one-original-PC-per-state dispatch is intentionally conservative and
 * too expensive for repeated live loading on a 16.78 MHz ARM7. */
/* CP17: keep the normal immutable-source predecode hit off the canonical
 * decoder's 256-entry table stack frame.  The fallback is explicitly noinline
 * so Clang cannot merge that large local allocation back into this wrapper. */
__attribute__((noinline))
static unsigned nemesis_fast_fallback(uint32_t src,uint8_t* out,unsigned cap){
    uint32_t p=src;
    uint16_t header=rom16(p); p+=2u;
    unsigned xor_mode=(header&0x8000u)!=0u;
    unsigned rows=(unsigned)(header&0x7FFFu)*8u;
    if(rows*4u>cap) return 0u;
    uint16_t table[256]; for(unsigned i=0;i<256u;i++) table[i]=0u;
    uint8_t d0=rom8(p++);
    while(d0!=0xFFu){
        unsigned pal=d0&15u;
        for(;;){
            d0=rom8(p++);
            if(d0>=0x80u) break;
            unsigned repeat=(d0>>4)&7u, nbits=d0&15u;
            uint8_t code=rom8(p++);
            uint16_t packed=(uint16_t)((nbits<<8)|(repeat<<4)|pal);
            unsigned fill=8u-nbits, base=((unsigned)code<<fill)&255u;
            for(unsigned i=0;i<(1u<<fill);i++) table[(base+i)&255u]=packed;
        }
    }
    uint16_t d5=rom16(p); p+=2u; unsigned d6=16u;
    uint32_t prev=0u,row=0u; unsigned pixels=8u,o=0u;
#define NEM_REFILL() do { if(d6<9u){ d6+=8u; d5=(uint16_t)((d5<<8)|rom8(p++)); } } while(0)
    while(rows){
        unsigned top=(d5>>(d6-8u))&255u,pixel,repeat;
        if(top>=0xFCu){
            d6-=6u; NEM_REFILL(); d6-=7u;
            uint16_t v=(uint16_t)(d5>>d6); pixel=v&15u; repeat=(v>>4)&7u;
            NEM_REFILL();
        }else{
            uint16_t ent=table[top]; unsigned nbits=(ent>>8)&255u;
            d6-=nbits; NEM_REFILL(); pixel=ent&15u; repeat=(ent>>4)&15u;
        }
        for(unsigned r=0;r<=repeat;r++){
            row=(row<<4)|pixel;
            if(--pixels==0u){
                uint32_t v=row; if(xor_mode){ prev^=row; v=prev; }
                out[o++]=(uint8_t)(v>>24); out[o++]=(uint8_t)(v>>16);
                out[o++]=(uint8_t)(v>>8);  out[o++]=(uint8_t)v;
                --rows; if(!rows) return o;
                row=0u; pixels=8u;
            }
        }
    }
#undef NEM_REFILL
    return o;
}

__attribute__((noinline))
static unsigned nemesis_fast(uint32_t src,uint8_t* out,unsigned cap){
    unsigned predecoded;
    if(psc_predecode_copy(PS4_PREDECODE_NEM,src,out,cap,&predecoded))return predecoded;
    return nemesis_fast_fallback(src,out,cap);
}

struct BitReader { uint32_t pos; uint16_t buf; uint8_t n; };
static uint8_t bit_get(struct BitReader* b){
    if(!b->n){ b->buf=rom16(b->pos); b->pos+=2; b->n=16; }
    --b->n; return (uint8_t)((b->buf>>b->n)&1u);
}
static uint16_t bit_read(struct BitReader* b,unsigned n){ uint16_t v=0; while(n--) v=(uint16_t)((v<<1)|bit_get(b)); return v; }

/* Temporary generic compatibility helper. md2gba's translated original EniDecomp can replace this at the same boundary. */
static uint16_t enigma_inline(struct BitReader* bs,uint8_t enabled,uint8_t packet,uint16_t base){
    uint16_t flags=0;
    for(int i=4;i>=0;i--) if(enabled&(1u<<i)) flags|=(uint16_t)(bit_get(bs)<<(11+i));
    return (uint16_t)(base+flags+bit_read(bs,packet));
}

/* Temporary generic compatibility helper. md2gba's translated original EniDecomp can replace this at the same boundary. */
__attribute__((noinline))
static unsigned enigma_decode(uint32_t src,uint16_t base,uint16_t* out,unsigned cap){
    uint8_t packet=rom8(src), enabled=rom8(src+1);
    uint16_t inc=(uint16_t)(rom16(src+2)+base), common=(uint16_t)(rom16(src+4)+base);
    struct BitReader bs={src+6,0,0}; unsigned o=0;
    for(;;){
        if(bit_get(&bs)){
            uint8_t mode=(uint8_t)bit_read(&bs,2); uint8_t count=(uint8_t)bit_read(&bs,4);
            if(mode==3 && count==15) break;
            if(mode==3){
                for(unsigned i=0;i<(unsigned)count+1;i++){ uint16_t v=enigma_inline(&bs,enabled,packet,base); if(o<cap) out[o++]=v; }
            } else {
                uint16_t v=enigma_inline(&bs,enabled,packet,base);
                int delta=mode==0?0:(mode==1?1:-1);
                for(unsigned i=0;i<(unsigned)count+1;i++){ if(o<cap) out[o++]=v; v=(uint16_t)(v+delta); }
            }
        } else {
            uint8_t mode=bit_get(&bs); uint8_t count=(uint8_t)bit_read(&bs,4);
            if(mode){ for(unsigned i=0;i<(unsigned)count+1;i++) if(o<cap) out[o++]=common; }
            else { for(unsigned i=0;i<(unsigned)count+1;i++){ if(o<cap) out[o++]=inc; ++inc; } }
        }
    }
    return o;
}



/* Exact PSIV Kosinski compatibility decoder.  This is a temporary generic runtime
 * helper until md2gba lowers the original $0418xx routine directly to ARM.  The
 * descriptor prefetch on the 16th bit matches PSIV's 68000 implementation. */
struct KosReader { uint32_t pos; uint16_t desc; uint8_t bits; };
static void kos_init(struct KosReader* k,uint32_t src){
    k->pos=src+2u; k->desc=(uint16_t)((uint16_t)rom8(src)|((uint16_t)rom8(src+1u)<<8)); k->bits=16u;
}
static uint8_t kos_bit(struct KosReader* k){
    uint8_t b=(uint8_t)(k->desc&1u); k->desc>>=1; --k->bits;
    if(!k->bits){ k->desc=(uint16_t)((uint16_t)rom8(k->pos)|((uint16_t)rom8(k->pos+1u)<<8)); k->pos+=2u; k->bits=16u; }
    return b;
}
__attribute__((noinline))
static unsigned kos_decode(uint32_t src,uint8_t* out,unsigned cap){
    unsigned predecoded;if(psc_predecode_copy(PS4_PREDECODE_KOS,src,out,cap,&predecoded))return predecoded;
    struct KosReader k; kos_init(&k,src); unsigned o=0;
    for(;;){
        if(kos_bit(&k)){
            if(o>=cap) return 0; out[o++]=rom8(k.pos++); continue;
        }
        int off; unsigned len;
        if(!kos_bit(&k)){
            unsigned n=((unsigned)kos_bit(&k)<<1)|(unsigned)kos_bit(&k);
            len=n+2u; off=(int)rom8(k.pos++)-256;
        }else{
            uint8_t lo=rom8(k.pos++),hi=rom8(k.pos++);
            off=(int)((((unsigned)hi&0xF8u)<<5)|(unsigned)lo)-0x2000;
            unsigned code=(unsigned)hi&7u;
            if(code) len=code+2u;
            else{
                uint8_t x=rom8(k.pos++);
                if(x==0u) return o;
                if(x==1u) continue;
                len=(unsigned)x+1u;
            }
        }
        if(off>=0 || (unsigned)(-off)>o || o+len>cap) return 0;
        for(unsigned i=0;i<len;i++){ out[o]=out[(unsigned)((int)o+off)]; ++o; }
    }
}

static void plane_write(uint16_t addr,uint16_t v){
    addr&=0xFFFFu;
    if(addr>=0x8000u && addr<0x9000u) plane_a[((addr-0x8000u)&0x0FFFu)>>1]=v;
    else if(addr>=0x9000u && addr<0xA000u) plane_b[((addr-0x9000u)&0x0FFFu)>>1]=v;
}
static void plane_map(const uint16_t* words,uint16_t addr,unsigned w,unsigned h){
    unsigned k=0; uint16_t a1=addr;
    for(unsigned y=0;y<h;y++){
        uint16_t a2=a1;
        for(unsigned x=0;x<w;x++){
            plane_write(a2,words[k++]); a2=(uint16_t)(a2+2u); if((a2&0x7Fu)==0) a2=(uint16_t)(a2-0x80u);
        }
        a1=(uint16_t)(a1+0x80u); if((a1&0x0F80u)==0) a1=(uint16_t)(a1-0x1000u);
    }
}
static void plane_fill(uint16_t addr,unsigned w,unsigned h,uint16_t v){
    for(unsigned i=0;i<w*h;i++) eni_tmp[i]=v; plane_map(eni_tmp,addr,w,h);
}

static int load_nem(uint32_t src,uint16_t tile,unsigned len,uint16_t temp_off){
    unsigned n=nemesis_fast(src,&work_ram[temp_off],len);
    if(n!=len) return 1;
    uint16_t p=(uint16_t)(tile*32u);
    if(((unsigned)p+len)<=65536u && (len&3u)==0u) dma_copy32(&work_ram[temp_off],&md_vram[p],len);
    else for(unsigned i=0;i<len;i++){ md_vram[p]=work_ram[(temp_off+i)&0xFFFFu]; ++p; }
    return 0;
}

static int build_original_title_final_planes(void){
    unsigned n;
    n=enigma_decode(0x1D223Eu,0x60E3u,eni_tmp,2048); if(n!=8u*28u) return 9; plane_map(eni_tmp,0x900Cu,8,28);
    n=enigma_decode(0x1D22CAu,0x60E3u,eni_tmp,2048); if(n!=6u*28u) return 10; plane_map(eni_tmp,0x9000u,6,28);
    n=enigma_decode(0x1D236Cu,0x60E3u,eni_tmp,2048); if(n!=26u*28u) return 11; plane_map(eni_tmp,0x901Cu,26,28);
    plane_fill(0x8000u,40,28,0);
    n=enigma_decode(0x1D20F2u,0x0010u,eni_tmp,2048); if(n!=17u*13u) return 12; plane_map(eni_tmp,0x8196u,17,13);
    n=enigma_decode(0x1D2126u,0x0092u,eni_tmp,2048); if(n!=29u*3u) return 13; plane_map(eni_tmp,0x888Au,29,3);
    n=enigma_decode(0x1D2160u,0x40D8u,eni_tmp,2048); if(n!=18u) return 14; plane_map(eni_tmp,0x8B16u,18,1);
    n=enigma_decode(0x1D2152u,0x20CAu,eni_tmp,2048); if(n!=17u) return 15; plane_map(eni_tmp,0x8C98u,17,1);
    return 0;
}

static int build_original_title_state(void){
    dma_fill32(work_ram,65536u,0u); dma_fill32(md_vram,65536u,0u);
    dma_fill32(plane_a,4096u,0u); dma_fill32(plane_b,4096u,0u);

    /* Original Title_ArtPtrs order, executed through translated PSIV Nemesis. */
    if(load_nem(0x1CD370u,0x00CAu,0x01A0u,0x6000u)) return 1;
    if(load_nem(0x1CD2A6u,0x00D8u,0x0160u,0x6000u)) return 2;
    if(load_nem(0x1CD462u,0x00E3u,0x33C0u,0x6000u)) return 3;
    if(load_nem(0x2A2B7Eu,0x0680u,0x1000u,0x6000u)) return 4;
    if(load_nem(0x2A303Au,0x07C0u,0x0AE0u,0x6000u)) return 5;
    if(load_nem(0x2A303Au,0x0681u,0x0AE0u,0x6000u)) return 6;

    /* Original loc_42024: solid-color tiles 0..15. */
    for(unsigned t=0;t<16;t++){ uint8_t q=(uint8_t)((t<<4)|t); for(unsigned i=0;i<32;i++) md_vram[t*32+i]=q; }

    /* Original final title reveal: translated Nemesis to $FFFF2000/$FFFF3040 then DMA 0x1740 bytes -> VRAM $0200. */
    if(nemesis_fast(0x1CC47Au,&work_ram[0x2000u],0x1040u)!=0x1040u) return 7;
    if(nemesis_fast(0x1CCEAAu,&work_ram[0x3040u],0x0700u)!=0x0700u) return 8;
    dma_copy32(&work_ram[0x2000u],&md_vram[0x0200u],0x1740u);

    {int prc=build_original_title_final_planes();if(prc)return prc;}

    for(unsigned i=0;i<64;i++) cram[i]=rom16(0x1D29BCu+i*2u);
    cram[0x3E/2]=0x0EEEu; cram[0x5E/2]=0x0EEEu;
    return 0;
}



/* Source-faithful first-run title/new-game helpers. All offsets are original $FFxxxx RAM addresses. */
static inline void ram8w(uint16_t off,uint8_t v){ work_ram[off]=v; if(off==0x500Au) (void)ps4_audio_sound_index(v); }
static inline uint8_t ram8r(uint16_t off){ return work_ram[off]; }
static inline void ram16w(uint16_t off,uint16_t v){ work_ram[off]=(uint8_t)(v>>8); work_ram[(uint16_t)(off+1)]=(uint8_t)v; }
static inline uint16_t ram16r(uint16_t off){ return (uint16_t)(((uint16_t)work_ram[off]<<8)|work_ram[(uint16_t)(off+1)]); }
static inline void ram32w(uint16_t off,uint32_t v){ ram16w(off,(uint16_t)(v>>16)); ram16w((uint16_t)(off+2),(uint16_t)v); }

static int ps4_sram_header_present(void){
    static const char sig[]="PHANTASY STAR 4";
    int ok=1; md_write8(0xA130F1u,1u);
    for(unsigned i=0;i<sizeof(sig)-1;i++) if(md_read8(0x200005u+i*2u)!=(uint8_t)sig[i]){ok=0;break;}
    md_write8(0xA130F1u,0u); return ok;
}

/* Original InitSRAM $64D24 logical effect: clear 8192 odd-lane bytes, then install the source signature. */
static void ps4_init_sram(void){
    md_write8(0xA130F1u,1u);
    for(unsigned i=0;i<8192u;i++) md_write8(0x200001u+i*2u,0u);
    for(unsigned i=0;i<15u;i++) md_write8(0x200005u+i*2u,rom8(0x64D7Au+i));
    md_write8(0xA130F1u,0u);
    ram8w(0xED59u,0u);
}

static int ps4_verify_sram_after_init(void){
    int ok=1;
    md_write8(0xA130F1u,1u);
    for(unsigned i=0;i<15u;i++) if(md_read8(0x200005u+i*2u)!=rom8(0x64D7Au+i)){ok=0;break;}
    if(ok){
        /* InitSRAM cleared the rest of this source header block; sample both sides of the signature. */
        if(md_read8(0x200001u)!=0u || md_read8(0x200003u)!=0u || md_read8(0x200023u)!=0u) ok=0;
    }
    md_write8(0xA130F1u,0u);
    return ok;
}

/* Original loc_64EAC first step: copy the 256-byte logical SRAM header block into $FFF000. */
static void ps4_copy_sram_header_to_ram(void){
    md_write8(0xA130F1u,1u);
    for(unsigned i=0;i<256u;i++) work_ram[0xF000u+i]=md_read8(0x200001u+i*2u);
    md_write8(0xA130F1u,0u);
}

static uint16_t ps4_sram_slot_sum(unsigned slot){
    uint16_t sum=0u;uint32_t base=0x200201u+(uint32_t)(slot%3u)*0x1400u;md_write8(0xA130F1u,1u);
    for(unsigned i=0u;i<0xA00u;i++)sum=(uint16_t)(sum+md_read8(base+(uint32_t)i*2u));md_write8(0xA130F1u,0u);return sum;
}
static uint8_t ps4_sram_invalid_mask(void){
    ps4_copy_sram_header_to_ram();uint8_t mask=0u;
    for(unsigned slot=0u;slot<3u;slot++){uint16_t sum=ps4_sram_slot_sum(slot),stored=ram16r((uint16_t)(0xF014u+slot*2u));if(!sum||sum!=stored)mask|=(uint8_t)(1u<<slot);}
    ram8w(0xED59u,mask);return mask;
}
static int ps4_sram_load_slot(unsigned slot){
    slot%=3u;ps4_copy_sram_header_to_ram();uint16_t sum=ps4_sram_slot_sum(slot);if(!sum||sum!=ram16r((uint16_t)(0xF014u+slot*2u)))return 0;
    uint32_t base=0x200201u+(uint32_t)slot*0x1400u;md_write8(0xA130F1u,1u);
    for(unsigned i=0u;i<0xA00u;i++)ram8w((uint16_t)(0xF100u+i),md_read8(base+(uint32_t)i*2u));md_write8(0xA130F1u,0u);ram16w(0xF012u,(uint16_t)slot);return 1;
}
static void ps4_sram_erase_slot(unsigned slot){
    slot%=3u;uint32_t base=0x200201u+(uint32_t)slot*0x1400u;md_write8(0xA130F1u,1u);
    for(unsigned i=0u;i<0xA00u;i++)md_write8(base+(uint32_t)i*2u,0u);
    md_write8(0xA130F1u,0u);ps4_copy_sram_header_to_ram();ram16w((uint16_t)(0xF014u+slot*2u),0u);md_write8(0xA130F1u,1u);for(unsigned i=0u;i<0x100u;i++)md_write8(0x200001u+i*2u,ram8r((uint16_t)(0xF000u+i)));md_write8(0xA130F1u,0u);
}

static inline int8_t item_s8(uint8_t item,unsigned off){
    if(!item) return 0;
    return (int8_t)rom8(0x2A8E28u+((uint32_t)item-1u)*0x16u+off);
}
static inline uint8_t item_u8(uint8_t item,unsigned off){
    if(!item) return 0;
    return rom8(0x2A8E28u+((uint32_t)item-1u)*0x16u+off);
}

/* Exact logical effect of UpdateCharModStats $5F754 and UpdateCharElems $5FD2A. */
static void update_one_char_derived(unsigned ci){
    uint16_t d=(uint16_t)(0xF500u+ci*0x80u);
    uint8_t eq[4]={ram8r((uint16_t)(d+0x4Cu)),ram8r((uint16_t)(d+0x4Du)),ram8r((uint16_t)(d+0x4Eu)),ram8r((uint16_t)(d+0x4Fu))};
    uint8_t str=ram8r((uint16_t)(d+0x18u)), men=ram8r((uint16_t)(d+0x1Bu));
    uint8_t agi=ram8r((uint16_t)(d+0x1Eu)), dex=ram8r((uint16_t)(d+0x21u));
    uint8_t sm=str, mm=men, am=agi, dm=dex;
    int atk=str, dfs=agi, mdf=men;
    for(unsigned i=0;i<4u;i++){
        sm=(uint8_t)(sm+(uint8_t)item_s8(eq[i],0x0Bu));
        mm=(uint8_t)(mm+(uint8_t)item_s8(eq[i],0x0Cu));
        am=(uint8_t)(am+(uint8_t)item_s8(eq[i],0x0Du));
        dm=(uint8_t)(dm+(uint8_t)item_s8(eq[i],0x0Eu));
        atk += item_s8(eq[i],0x0Bu) + item_s8(eq[i],0x0Fu);
        dfs += item_s8(eq[i],0x0Du) + item_s8(eq[i],0x10u);
        mdf += item_s8(eq[i],0x0Cu) + item_s8(eq[i],0x11u);
    }
    ram8w((uint16_t)(d+0x19u),sm); ram8w((uint16_t)(d+0x1Cu),mm);
    ram8w((uint16_t)(d+0x1Fu),am); ram8w((uint16_t)(d+0x22u),dm);
    ram16w((uint16_t)(d+0x24u),(uint16_t)atk); ram16w((uint16_t)(d+0x28u),(uint16_t)dfs); ram16w((uint16_t)(d+0x2Cu),(uint16_t)mdf);

    /* UpdateCharElems clears the equipment-derived byte of each element pair and weapon-effect word. */
    for(unsigned i=0;i<14u;i++) ram8w((uint16_t)(d+0x30u+i*2u),0u);
    ram16w((uint16_t)(d+0x50u),0u);
    for(unsigned slot=0;slot<4u;slot++) if(eq[slot]){
        uint8_t elem=item_u8(eq[slot],0x12u);
        if(slot<2u && item_u8(eq[slot],0x0Au)!=5u){
            ram8w((uint16_t)(d+0x50u+slot),elem);
        }else if(elem){
            unsigned ei=(unsigned)elem-1u;
            if(ei<14u) ram8w((uint16_t)(d+0x30u+ei*2u),1u);
        }
    }
    for(unsigned i=0;i<14u;i++){
        uint16_t e=(uint16_t)(d+0x30u+i*2u);
        if(ram8r(e)==0u) ram8w(e,ram8r((uint16_t)(e+1u)));
    }
}

static void init_one_char(unsigned ci,uint32_t* srcp){
    uint16_t dst=(uint16_t)(0xF500u+ci*0x80u); uint32_t p=*srcp;
    ram16w((uint16_t)(dst+0x06),rom16(p)); p+=2;
    ram16w((uint16_t)(dst+0x08),rom16(p)); p+=2;
    ram32w((uint16_t)(dst+0x0A),((uint32_t)rom16(p)<<16)|rom16(p+2)); p+=4;
    uint16_t hp=rom16(p); p+=2; ram16w((uint16_t)(dst+0x0E),hp); ram16w((uint16_t)(dst+0x10),hp);
    uint16_t tp=rom16(p); p+=2; ram16w((uint16_t)(dst+0x12),tp); ram16w((uint16_t)(dst+0x14),tp);
    ram8w((uint16_t)(dst+0x18),rom8(p++));
    ram8w((uint16_t)(dst+0x1B),rom8(p++));
    ram8w((uint16_t)(dst+0x1E),rom8(p++));
    ram8w((uint16_t)(dst+0x21),rom8(p++));
    for(unsigned i=0;i<14u;i++) ram8w((uint16_t)(dst+0x31u+i*2u),rom8(p++));
    for(unsigned i=0;i<4u;i++) ram8w((uint16_t)(dst+0x4Cu+i),rom8(p++));
    for(unsigned i=0;i<16u;i++) ram8w((uint16_t)(dst+0x52u+i),rom8(p++));
    for(unsigned i=0;i<8u;i++) ram8w((uint16_t)(dst+0x62u+i),rom8(p++));
    for(unsigned i=0;i<8u;i++){uint8_t u=rom8(p++);ram8w((uint16_t)(dst+0x6Au+i*2u),u);ram8w((uint16_t)(dst+0x6Bu+i*2u),u);}
    *srcp=p;
}

/* Source $44414 new-game initializer, through the fields required before GameMode_LoadFieldMap.
   Base records plus UpdateCharModStats/UpdateCharElems logical effects are reproduced from source. */
static void ps4_new_game_init_core(void){
    ps4_copy_sram_header_to_ram();
    ram16w(0xF012u,0u);          /* Save_Slot */
    ram16w(0xF440u,2u);          /* Message_Speed */
    ram16w(0xF442u,2u);          /* Battle_Speed */
    ram16w(0xF400u,0u);          /* World_Index */
    ram8w(0xEC80u,2u);
    ram8w(0xF40Au,0u); ram8w(0xF40Bu,1u); /* Chaz, Alys */
    for(unsigned i=2;i<6u;i++) ram8w((uint16_t)(0xF40Au+i),0xFFu);
    for(unsigned i=0;i<0x580u;i++) work_ram[0xF500u+i]=0;

    /* Character names copied exactly like $44482-$444A8. */
    uint32_t np=0x2A8DA0u;
    for(unsigned c=0;c<11u;c++){
        uint16_t d=(uint16_t)(0xF500u+c*0x80u); unsigned j=0;
        for(;;){uint8_t q=rom8(np++);if(q==0xFFu){ram8w((uint16_t)(d+j),0xFEu);break;}ram8w((uint16_t)(d+j++),q);}
    }
    ram32w(0xF438u,500u);        /* Current_Money */
    for(unsigned i=0;i<40u;i++) ram8w((uint16_t)(0xF410u+i),0u);

    uint32_t sp=0x2A8ACAu;
    for(unsigned c=0;c<11u;c++){ init_one_char(c,&sp); update_one_char_derived(c); }

    /* Vehicle base stats from $444CA onward. */
    ram16w(0xFA80u,0x02E4u); ram16w(0xFA82u,0x02E4u); ram8w(0xFA84u,3u); for(unsigned i=6;i<=9;i++)ram8w((uint16_t)(0xFA80u+i),8u);
    ram16w(0xFAA0u,0x03C0u); ram16w(0xFAA2u,0x03C0u); ram8w(0xFAA4u,0x50u); ram8w(0xFAA6u,8u);ram8w(0xFAA7u,8u);ram8w(0xFAA8u,4u);ram8w(0xFAA9u,4u);
    ram16w(0xFAC0u,0x02A8u); ram16w(0xFAC2u,0x02A8u); ram8w(0xFAC4u,0x0Cu); ram8w(0xFAC6u,8u);ram8w(0xFAC7u,8u);ram8w(0xFAC8u,2u);ram8w(0xFAC9u,2u);

    for(unsigned i=0;i<0xA0u;i++) ram8w((uint16_t)(0xF444u+i),rom8(0x44592u+i));
    for(unsigned i=0;i<0x200u;i++) ram8w((uint16_t)(0xF100u+i),0u);
    for(unsigned i=0;i<0x20u;i++) ram8w((uint16_t)(0xF120u+i),rom8(0x44632u+i));
    ram16w(0xF160u,0x8000u); ram8w(0xF162u,0x80u); ram8w(0xF163u,0x40u);

    /* Title_StartOption $43770-$43798. */
    ram16w(0xEC28u,0x0011u);     /* MapID_PiataAcademy */
    ram8w(0xECE0u,1u); ram8w(0xECECu,0u);
    ram16w(0xEF00u,0x000Cu);     /* GameMode_LoadFieldMap */
    ram16w(0xEC20u,0x000Cu);     /* Game_Mode_Routine */
    ram16w(0xECA8u,0x009Fu);     /* opening Event_Index */
    ram32w(0xEC9Eu,0u);
}

static void plane_a_cell(unsigned x,unsigned y,uint16_t v){ plane_a[(y&31u)*64u+(x&63u)]=v; }

/* Final frame of Window_Draw for WinGroup_Other entry #4: 14x3 cells at (13,12). */
static void draw_start_window(void){
    const unsigned x0=13u,y0=12u,w=14u;
    plane_a_cell(x0,y0,0xC6E9u); for(unsigned x=1;x<w-1;x++)plane_a_cell(x0+x,y0,0xC6EAu); plane_a_cell(x0+w-1,y0,0xCEE9u);
    plane_a_cell(x0,y0+1,0xC6F3u); for(unsigned x=1;x<w-1;x++)plane_a_cell(x0+x,y0+1,0xC680u); plane_a_cell(x0+w-1,y0+1,0xCEF3u);
    plane_a_cell(x0,y0+2,0xD6E9u); for(unsigned x=1;x<w-1;x++)plane_a_cell(x0+x,y0+2,0xD6EAu); plane_a_cell(x0+w-1,y0+2,0xDEE9u);
    unsigned x=x0+1u,y=y0+1u; uint32_t p=0x2A9ED8u;
    for(;;){uint8_t q=rom8(p++);if(q==0xFEu)break;if(q>=0xF0u)continue;uint16_t code=q;if(q&0x80u)code=(uint16_t)(code+0xC0u);plane_a_cell(x++,y,(uint16_t)(0xC680u+code));}
}

static void clear_start_window(void){for(unsigned y=12u;y<15u;y++)for(unsigned x=13u;x<27u;x++)plane_a_cell(x,y,0u);}

static void render(uint16_t press);
static uint8_t tile_entry_index(uint16_t e,unsigned x,unsigned y);

/*
 * GBA presentation adapter: Genesis 8x8 UI glyphs cannot survive the 320x224 -> 240x160
 * pixel decimator used for the title artwork.  Keep the virtual MD plane byte-exact, but
 * composite the source window cells at native 8x8 resolution on the GBA framebuffer.
 */
static void render_start_window_native(uint16_t press){
    (void)press;
    const unsigned gx=13u, gy=12u, gw=14u, gh=3u;
    const unsigned dx=64u, dy=72u; /* centered 112x24 GBA presentation */
    for(unsigned ty=0;ty<gh;ty++) for(unsigned tx=0;tx<gw;tx++){
        uint16_t e=plane_a[((gy+ty)&31u)*64u+((gx+tx)&63u)];
        for(unsigned py=0;py<8u;py++) for(unsigned px=0;px<8u;px++){
            unsigned x=dx+tx*8u+px, y=dy+ty*8u+py;
            if(x<240u && y<160u) title_fb[y*240u+x]=tile_entry_index(e,px,py);
        }
    }
}

static uint16_t menu_bg_pal3[16];
static void waitvb(void);
static void title_present_full(void);
static void title_present_band(unsigned y0,unsigned y1);
static void title_window_cells(unsigned gx,unsigned gy,unsigned gw,unsigned gh){
    plane_a_cell(gx,gy,0xC6E9u);for(unsigned x=1u;x<gw-1u;x++)plane_a_cell(gx+x,gy,0xC6EAu);plane_a_cell(gx+gw-1u,gy,0xCEE9u);
    for(unsigned y=1u;y<gh-1u;y++){plane_a_cell(gx,gy+y,0xC6F3u);for(unsigned x=1u;x<gw-1u;x++)plane_a_cell(gx+x,gy+y,0xC680u);plane_a_cell(gx+gw-1u,gy+y,0xCEF3u);}
    plane_a_cell(gx,gy+gh-1u,0xD6E9u);for(unsigned x=1u;x<gw-1u;x++)plane_a_cell(gx+x,gy+gh-1u,0xD6EAu);plane_a_cell(gx+gw-1u,gy+gh-1u,0xDEE9u);
}
static uint16_t title_menu_glyph(char c,int selected){
    unsigned code=0u;if(c>='A'&&c<='Z')code=0x80u+(unsigned)(c-'A');else if(c>='0'&&c<='9')code=0x40u+(unsigned)(c-'0');
    return (uint16_t)((selected?0xE680u:0xC680u)+code);
}
static void title_menu_text(unsigned gx,unsigned gy,const char* text,int selected){for(;*text;text++,gx++)plane_a_cell(gx,gy,title_menu_glyph(*text,selected));}
static void title_menu_clear_region(unsigned gx,unsigned gy,unsigned gw,unsigned gh){for(unsigned y=0u;y<gh;y++)for(unsigned x=0u;x<gw;x++)plane_a_cell(gx+x,gy+y,0u);}
static void title_render_native_region(unsigned gx,unsigned gy,unsigned gw,unsigned gh,unsigned dx,unsigned dy,uint16_t press){
    uint16_t cells[128];unsigned n=gw*gh;if(n>128u)return;for(unsigned i=0u;i<n;i++){unsigned x=i%gw,y=i/gw;cells[i]=plane_a[((gy+y)&31u)*64u+((gx+x)&63u)];plane_a[((gy+y)&31u)*64u+((gx+x)&63u)]=0u;}
    for(unsigned i=0u;i<16u;i++){uint16_t q=cram[48u+i];cram[48u+i]=menu_bg_pal3[i];menu_bg_pal3[i]=q;}render(press);for(unsigned i=0u;i<16u;i++){uint16_t q=cram[48u+i];cram[48u+i]=menu_bg_pal3[i];menu_bg_pal3[i]=q;}
    for(unsigned i=0u;i<n;i++){unsigned x=i%gw,y=i/gw;plane_a[((gy+y)&31u)*64u+((gx+x)&63u)]=cells[i];uint16_t e=cells[i];for(unsigned py=0u;py<8u;py++)for(unsigned px=0u;px<8u;px++){unsigned sx=dx+x*8u+px,sy=dy+y*8u+py;if(sx<240u&&sy<160u)title_fb[sy*240u+sx]=tile_entry_index(e,px,py);}}
}
static int title_pick_three(const char* a,const char* b,const char* c,uint8_t disabled_mask,uint16_t press){
    const char* labels[3]={a,b,c};unsigned sel=0u;while(disabled_mask&(1u<<sel)){if(++sel>=3u)return -1;}uint8_t prev=md_io_read_psiv_pad();unsigned dirty=1u;
    for(;;){
        if(dirty){title_window_cells(13u,10u,14u,7u);for(unsigned i=0u;i<3u;i++)title_menu_text(14u,11u+i*2u,labels[i],i==sel);title_render_native_region(13u,10u,14u,7u,64u,52u,press);}
        waitvb();if(dirty){title_present_band(52u,108u);dirty=0u;}uint8_t now=md_io_read_psiv_pad(),e=(uint8_t)(now&~prev);prev=now;
        if(e&1u){unsigned q=sel;do{q=q?q-1u:2u;}while(disabled_mask&(1u<<q));if(q!=sel){sel=q;dirty=1u;}}
        else if(e&2u){unsigned q=sel;do{q=q==2u?0u:q+1u;}while(disabled_mask&(1u<<q));if(q!=sel){sel=q;dirty=1u;}}
        else if(e&0xE0u){while(md_io_read_psiv_pad()&0xF0u)waitvb();return (int)sel;}else if(e&0x10u){while(md_io_read_psiv_pad()&0xF0u)waitvb();return -1;}
    }
}


/* Presentation-only adapter for the title menu.  The original title transition
 * repurposes CRAM line 3 before the menu.  Preserve that exact virtual CRAM state,
 * but present Plane B with its pre-transition colors and suppress the decimated
 * copy of the 14x3 window before drawing the source 8x8 UI at native resolution. */
static uint16_t menu_bg_pal3[16];
static void render_start_menu_clean(uint16_t press){
    uint16_t cells[42],live_pal3[16]; unsigned k=0;
    for(unsigned y=12u;y<15u;y++)for(unsigned x=13u;x<27u;x++){
        unsigned i=(y&31u)*64u+(x&63u); cells[k++]=plane_a[i]; plane_a[i]=0u;
    }
    for(unsigned i=0;i<16u;i++){ live_pal3[i]=cram[48u+i]; cram[48u+i]=menu_bg_pal3[i]; }
    render(press);
    for(unsigned i=0;i<16u;i++)cram[48u+i]=live_pal3[i];
    k=0;for(unsigned y=12u;y<15u;y++)for(unsigned x=13u;x<27u;x++)plane_a[(y&31u)*64u+(x&63u)]=cells[k++];
    render_start_window_native(press);
}

static void waitvb(void);

static void fade_framebuffer_to_black(void){
    /* PSIV PalFadeOut_ClrSpriteTbl updates once per VBlank until the largest
     * Genesis color nibble reaches zero (normally 14 frames for $E).  Do the
     * presentation fade in GBA hardware instead of rewriting 38,400 Mode-3
     * pixels in software; the virtual Genesis palette remains untouched. */
    REG_BLDCNT=(uint16_t)((3u<<6)|BLD_TGT_BG2); /* brightness decrease, BG2 target */
    for(unsigned step=1u;step<=14u;step++){
        REG_BLDY=(uint16_t)((step*16u+13u)/14u);
        waitvb();
    }
    REG_BLDY=16u;
}

static void field_palette_fade_out(void){
    /* PSIV Pal_FadeOut ($4223E) walks all 64 CRAM entries over successive
     * VBlanks.  Keep the translated Genesis CRAM authoritative and use the
     * GBA brightness unit only as a presentation adapter for the same field
     * transition lifecycle. */
    REG_BLDCNT=(uint16_t)((3u<<6)|0x003Fu); /* decrease BG0-3, OBJ + backdrop */
    for(unsigned step=1u;step<=14u;step++){
        REG_BLDY=(uint16_t)((step*16u+13u)/14u);
        waitvb();
    }
    REG_BLDY=16u;
}
static void field_palette_fade_in(void){
    /* PSIV Pal_FadeIn ($421D4) restores the resident palette incrementally.
     * The field loader has already installed the new map/palettes at this
     * point, so reveal them over the same multi-VBlank envelope. */
    REG_BLDCNT=(uint16_t)((3u<<6)|0x003Fu);
    REG_BLDY=16u;
    for(unsigned step=14u;step>0u;step--){
        REG_BLDY=(uint16_t)(((step-1u)*16u+13u)/14u);
        waitvb();
    }
    REG_BLDY=0u;REG_BLDCNT=0u;
}

static void fade_to_black(void){
    uint16_t save[64];for(unsigned i=0;i<64u;i++)save[i]=cram[i];
    for(unsigned step=0;step<8u;step++){
        for(unsigned i=0;i<64u;i++){
            uint16_t c=save[i];unsigned r=(c>>1)&7u,g=(c>>5)&7u,b=(c>>9)&7u;
            r=(r*(7u-step))/7u;g=(g*(7u-step))/7u;b=(b*(7u-step))/7u;cram[i]=(uint16_t)((r<<1)|(g<<5)|(b<<9));
        }
        render(0);waitvb();waitvb();
    }
    for(unsigned i=0;i<64u;i++)cram[i]=0;render(0);
}

static inline uint32_t fnv1a_byte(uint32_t h,uint8_t b){ return (h^(uint32_t)b)*16777619u; }
static uint32_t new_game_state_hash(void){
    static const uint16_t ranges[][2]={
        {0xEC20u,0xEC2Au},{0xEC80u,0xEC81u},{0xEC9Eu,0xECA2u},{0xECA8u,0xECAAu},
        {0xECE0u,0xECE1u},{0xECECu,0xECEDu},{0xEF00u,0xEF02u},
        {0xF000u,0xF300u},{0xF400u,0xF4E4u},{0xF500u,0xFACAu}
    };
    uint32_t h=2166136261u;
    for(unsigned r=0;r<sizeof(ranges)/sizeof(ranges[0]);r++){
        uint16_t a=ranges[r][0],b=ranges[r][1];
        h=fnv1a_byte(h,(uint8_t)(a>>8));h=fnv1a_byte(h,(uint8_t)a);
        h=fnv1a_byte(h,(uint8_t)(b>>8));h=fnv1a_byte(h,(uint8_t)b);
        for(uint16_t q=a;q!=b;q=(uint16_t)(q+1u)) h=fnv1a_byte(h,work_ram[q]);
    }
    return h;
}

static inline uint16_t md2gba(uint16_t c);
static inline unsigned tile_px(uint16_t e,unsigned x,unsigned y);
static inline uint16_t pal(unsigned p,unsigned i,uint16_t press);
static const uint16_t src_x[240];
static const uint16_t src_y[160];
static void fail(unsigned code);

/* Descriptor-driven PSIV field-runtime bring-up.  Normal field maps are
 * loaded from FieldMapPtrs below; piata_f1_state_hash() remains only as a
 * regression gate for the corrected opening handoff map. */
static uint32_t piata_f1_state_hash(void){
    uint32_t h=2166136261u;
    h=fnv1a_byte(h,'V'); for(unsigned i=0x0200u;i<0x7CE0u;i++)h=fnv1a_byte(h,md_vram[i]);
    h=fnv1a_byte(h,'C'); for(unsigned i=0;i<5376u;i++)h=fnv1a_byte(h,work_ram[0x6000u+i]);
    h=fnv1a_byte(h,'F'); for(unsigned i=0;i<1024u;i++)h=fnv1a_byte(h,work_ram[0xA000u+i]);
    h=fnv1a_byte(h,'B'); for(unsigned i=0;i<1024u;i++)h=fnv1a_byte(h,work_ram[0xB000u+i]);
    h=fnv1a_byte(h,'P'); for(unsigned i=0;i<64u;i++){uint16_t q=cram[i];h=fnv1a_byte(h,(uint8_t)(q>>8));h=fnv1a_byte(h,(uint8_t)q);}
    return h;
}

/* -------------------------------------------------------------------------
 * PSIV field/runtime lowering, safe v0.8.2
 *
 * Expensive Genesis -> GBA work is load-time only.  The per-frame path is a
 * four-BG hardware ring streamer + OAM sprites.  Source map descriptors,
 * collision, transitions, dialogue bytecode and event flags remain the
 * authority.  The same split is intended for MD2GBA's generated runtime.
 * ------------------------------------------------------------------------- */
#define FIELD_MAP_PTRS 0x100000u
#define FIELD_MAX_OBJECTS 32u
#define FIELD_DIALOGUE_CAP 0x2000u
#define FIELD_FOLLOWER_TILE_BASE 448u
#define FIELD_OBJ_TILE_BASE 480u
#define DIALOGUE_OBJ_TILE_BASE 768u
/* Globally audited field-object allocation ends at tile 736.  Reserve the
 * remaining pre-dialogue tiles for source event effects that share graphics. */
#define FIELD_EVENT_TILE_PARTICLE 736u
#define FIELD_EVENT_TILE_DIVIDE 744u
#define FIELD_EVENT_TILE_FADEIN 752u
#define DIALOGUE_CHOICE_OAM 108u
#define DIALOGUE_OAM_BASE 112u

struct FieldObjLite {
    uint16_t type;
    uint8_t facing,dialog;
    uint16_t base_tile;
    uint16_t x,y;
    uint8_t active,anim,blocks;
    /* Compact lowering of the common FieldObj_NPC* state.  These are source
     * movement-policy counters, not hand-authored destination coordinates. */
    uint8_t move_kind,move_dir,move_left,move_phase,timer,seq_pos;
    uint8_t treasure_type,treasure_flag,treasure_item;
    int8_t bound_x,bound_y,max_x,max_y;
    /* GBA lowering of the source sprite mapping.  Most field actors use the
     * common 16x32 mapping; exceptional source mappings (Igglanova is 32x32)
     * receive their original footprint and a variable-size tile allocation. */
    uint16_t gba_tile;
    uint8_t render_w,render_h;
    int8_t render_x,render_y;
};
struct Ps4FieldMapDesc {
    uint16_t map_id;
    uint8_t general,music,is_world;
    uint8_t row_fg,col_fg,row_bg,col_bg,collision_plane;
    uint32_t kos_tiles,nem_tiles,kos_ram,chunks;
    uint32_t map_updates,trans1,trans2,objects,treasure,layout_fg,layout_bg,data_manager;
    uint32_t dialogue_tree,interactions,events,palette;
};
struct PlayerLite {
    int x,y;
    uint8_t facing,anim,anim_clock;
    int8_t dx,dy;
    uint8_t step_left;
};
struct FieldTransition { uint16_t map_id,x,y; uint8_t facing,align; };

static struct Ps4FieldMapDesc field_map;
static struct FieldObjLite field_objs[FIELD_MAX_OBJECTS];
static unsigned field_obj_count;
static struct PlayerLite chaz;
static uint8_t lead_char_id;
/* FieldRoutine_Cutscene and several normal EventPtrs can call the source map
 * refresh helper after writing Field_Map_Index / start coordinates.  Keep the
 * request as shared field state so story-driven warps use the same outer map
 * loader as ordinary authored transitions instead of re-entering a map loader
 * from inside an event executor. */
static uint8_t field_event_reload_pending;
static uint16_t ps4_debug_event_pending;
static uint8_t battle_debug_battle_active;
/* A source cutscene may call RefreshMap in the middle of its routine, then
 * continue with dialogue/SFX/flags on the newly loaded map.  Keep a tiny
 * generated-script cursor across the outer field reload instead of executing
 * post-refresh operations against the old map state. */
static uint8_t field_cutscene_resume_index=0xFFu;
static uint8_t field_cutscene_resume_op;
/* Normal EventPtrs can use the same source RefreshMap seam.  Keep their
 * generated-script cursor separate from the high-bit cutscene cursor so both
 * dispatch tables remain source-shaped and neither needs a map-specific hack. */
static uint8_t field_transition_resume_index=0xFFu;
static uint8_t field_transition_resume_op;
static uint8_t field_custom_resume_event=0xFFu;
static uint8_t field_custom_resume_phase;
static void game_start_world_text_sequence(void);
static int battle_run_random(void);
static int battle_run_debug_enemy(unsigned enemy_id);
static int battle_run_debug_boss(unsigned event_id);
static int battle_run_debug_vehicle(unsigned vehicle,unsigned enemy);
static int battle_run_debug_random(void);
static void ps4_debug_level60_party(void);
static uint8_t ps4_debug_autoplace_pending;
/* RunRandomBattles field-state lowering.  The original resets $ECE4 to 10
 * on map entry/after battle, then tests one encounter roll per completed tile
 * after that warm-up. */
static uint8_t field_random_battle_delay=10u;
static uint8_t field_step_completed;

/* PSIV FieldObj_Move propagates each party member's previous tile destination
 * to the next slot.  Keep that exact low-cost chain instead of duplicating the
 * lead or performing pathfinding every frame. */
#define FIELD_MAX_FOLLOWERS 4u
struct PartyFollowerLite {
    int x,y,tx,ty;
    uint8_t id,active,facing,anim,anim_clock,moving;
};
static struct PartyFollowerLite field_followers[FIELD_MAX_FOLLOWERS];
static unsigned field_follower_count;

/* Title-time scratch is dead after Start.  Reuse the explicitly reserved upper
 * EWRAM arena by lifetime instead of growing .bss.  Across all 361 parsed PSIV
 * field descriptors the largest decompressed dialogue tree is 7,220 bytes, so an
 * 8 KiB dialogue window is sufficient and leaves a linker-enforced guard gap. */
static uint8_t* const field_pause_fb=(uint8_t*)0x0202A000;
static uint8_t* const field_dialogue=(uint8_t*)0x02036000;
static unsigned field_dialogue_len;
/* PSIV RunText stores A0 after consuming the terminal $FF into
 * Saved_Dialogue_Addr ($ECF0).  Keep the equivalent native cursor for source
 * EventPtrs that inspect the following dialogue entry. */
static const uint8_t* field_dialogue_saved_cursor;
/* Mode-4 dialogue/cutscene palette shadow.  Keep the live Mode-0 palette untouched
 * while expensive presentation work is prepared. */
static uint16_t* const field_pause_pal=(uint16_t*)0x02023400;
/* Manga art can expand to 19,744 bytes in the source catalog.  Give it a
 * full 20 KiB arena at the top of EWRAM.  This range is title-time scratch
 * before Start and is disjoint from field dialogue (0x36000..0x37FFF) and
 * dialogue OBJ pixels (0x38000..0x39FFF) during field/cutscene runtime. */
static uint8_t* const panel_art_scratch=(uint8_t*)0x0203B000;
#define PANEL_ART_SCRATCH_SIZE 0x5000u
static uint16_t* const panel_map_scratch=(uint16_t*)0x02022000;
#define FIELD_PANEL_STACK_MAX 16u
static uint16_t field_active_panels[FIELD_PANEL_STACK_MAX];
static unsigned field_active_panel_count;
static uint8_t field_cutscene_panels;
static uint8_t field_dialogue_paused_music;
static uint8_t field_idle_anim_clock;
static uint16_t native_main_frame_count;
/* PSC-like explicit continuation state: resolve the active map's generated
 * update list once at load time instead of re-indexing the 417-entry plan
 * table on every display frame. */
static const uint8_t* field_update_plan_ids;
static const uint8_t* field_event_plan_ids;
static const Ps4PscTransition* field_trans1_plan;
static const Ps4PscTransition* field_trans2_plan;
static uint8_t field_update_plan_count,field_event_plan_count,field_trans1_plan_count,field_trans2_plan_count,field_runtime_plan_valid;

/* Load-time tile namespace compaction.  Genesis can address 2048 tiles while a
 * GBA text BG entry has 10 tile bits.  PSIV maps reference far fewer than 1024
 * unique tiles, so compact once and keep the frame path remap-free. */
static uint16_t field_tile_remap[2048];
static uint16_t field_tile_source[1024];
static unsigned field_tile_count;
/* For immutable map/event graphics, remember the matching host-converted GBA
 * tile in ROM.  0xFFFF means the virtual MD VRAM tile was produced or mutated
 * dynamically and must use the canonical nibble conversion path. */
static uint16_t field_gba_ready_tile[2048];
/* CP18: exact ordinary-field OBJ art ownership.  NPC movement chooses facing
 * before collision rejection, so blocked attempts can request an identical
 * pose repeatedly.  Keep an exact stamp for each object slot and invalidate
 * all stamps whenever the source tile namespace changes or field OBJ VRAM
 * ownership is re-established.  This suppresses only identical composition /
 * upload work; simulation, facing, animation, RNG and collision still run. */
struct FieldObjArtStamp { uint32_t key0,key1,epoch; };
static struct FieldObjArtStamp field_obj_art_stamp[FIELD_MAX_OBJECTS];
static uint32_t field_obj_art_epoch=1u;
static void field_obj_art_epoch_advance(void){
    ++field_obj_art_epoch;if(!field_obj_art_epoch){field_obj_art_epoch=1u;for(unsigned i=0u;i<FIELD_MAX_OBJECTS;i++)field_obj_art_stamp[i].epoch=0u;}
}
/* PSC-style GBA-facing CRAM shadow.  Genesis-format cram[] stays authoritative;
 * translated colors are maintained at the same write/mutation boundary so
 * physical palette publication is a DMA copy instead of 64 conversions. */
static uint16_t field_gba_palette[64];
static void field_palette_shadow_rebuild(unsigned count){if(count>64u)count=64u;for(unsigned i=0u;i<count;i++)field_gba_palette[i]=md2gba(cram[i]);}
static void field_gba_ready_reset(void){dma_fill32(field_gba_ready_tile,sizeof(field_gba_ready_tile),0xFFFFFFFFu);field_obj_art_epoch_advance();}
static void field_gba_ready_invalidate(unsigned tile,unsigned count){if(tile>=2048u)return;if(count>2048u-tile)count=2048u-tile;for(unsigned i=0u;i<count;i++)field_gba_ready_tile[tile+i]=0xFFFFu;field_obj_art_epoch_advance();}
static void field_gba_ready_bind(unsigned codec,uint32_t src,unsigned tile,unsigned bytes){
    unsigned tiles=bytes>>5;if(tile>=2048u||!tiles)return;if(tiles>2048u-tile)tiles=2048u-tile;field_obj_art_epoch_advance();
    const uint8_t* data;unsigned n;if(ps4_psc_fastgfx_lookup(codec,src,&data,&n)&&n==bytes){
        unsigned base=(unsigned)((data-ps4_psc_field_fastassets)>>5);for(unsigned i=0u;i<tiles;i++)field_gba_ready_tile[tile+i]=(uint16_t)(base+i);
    }else field_gba_ready_invalidate(tile,tiles);
}

static uint32_t skip_word_records(uint32_t p,unsigned step){
    while(p<0x2FFFFEu && rom16(p)!=0xFFFFu)p+=step;
    return p+2u;
}
static uint32_t skip_ff_bytes(uint32_t p){
    while(p<0x300000u && rom8(p)!=0xFFu)++p;
    return p+1u;
}
static int ps4_event_flag_test(unsigned id){
    unsigned b=(id&0xFFu)>>3,bit=7u-(id&7u);
    return (ram8r((uint16_t)(0xF100u+b))&(uint8_t)(1u<<bit))!=0u;
}
static void ps4_event_flag_set(unsigned id){
    unsigned b=(id&0xFFu)>>3,bit=7u-(id&7u);
    uint16_t a=(uint16_t)(0xF100u+b);
    ram8w(a,(uint8_t)(ram8r(a)|(uint8_t)(1u<<bit)));
}
static uint16_t ps4_flag_base(unsigned type){return type==1u?0xF120u:(type==2u?0xF140u:0xF100u);}
static int ps4_flag_test_type(unsigned type,unsigned id){unsigned b=(id&0xFFu)>>3,bit=7u-(id&7u);return (ram8r((uint16_t)(ps4_flag_base(type)+b))&(uint8_t)(1u<<bit))!=0u;}
static int ps4_flag_test_and_set(unsigned type,unsigned id){if(type==0u&&id==0u)return 0;unsigned b=(id&0xFFu)>>3,bit=7u-(id&7u);uint16_t a=(uint16_t)(ps4_flag_base(type)+b);uint8_t mask=(uint8_t)(1u<<bit),v=ram8r(a);ram8w(a,(uint8_t)(v|mask));return (v&mask)!=0u;}
static void ps4_flag_toggle_type(unsigned type,unsigned id){unsigned b=(id&0xFFu)>>3,bit=7u-(id&7u);uint16_t a=(uint16_t)(ps4_flag_base(type)+b);ram8w(a,(uint8_t)(ram8r(a)^(uint8_t)(1u<<bit)));}
static void ps4_flag_clear_type(unsigned type,unsigned id){unsigned b=(id&0xFFu)>>3,bit=7u-(id&7u);uint16_t a=(uint16_t)(ps4_flag_base(type)+b);ram8w(a,(uint8_t)(ram8r(a)&(uint8_t)~(1u<<bit)));}
static void ps4_flag_set_type(unsigned type,unsigned id){unsigned b=(id&0xFFu)>>3,bit=7u-(id&7u);uint16_t a=(uint16_t)(ps4_flag_base(type)+b);ram8w(a,(uint8_t)(ram8r(a)|(uint8_t)(1u<<bit)));}
static inline uint32_t ram32r(uint16_t off){
    return ((uint32_t)ram16r(off)<<16)|ram16r((uint16_t)(off+2u));
}

static int ps4_parse_field_map_canonical(uint16_t map_id,struct Ps4FieldMapDesc* d){
    /* FieldMapPtrs contains 0x1A1 entries. Null slots occur throughout the
     * table, not only at $03-$0F; the source marks every one by pointing at
     * ErrorTrap.  Compare the actual pointer target (vector 2 also points at
     * ErrorTrap) rather than maintaining a partial map-ID blacklist. */
    if(map_id>=0x01A1u)return 0;
    uint32_t base=rom32(FIELD_MAP_PTRS+(uint32_t)map_id*4u);
    if(base==rom32(8u)||base>=0x300000u)return 0;
    uint32_t p=base;
    d->map_id=map_id; d->is_world=(uint8_t)(map_id<2u);
    d->general=rom8(p++); d->music=rom8(p++);
    d->kos_tiles=p; while(rom16(p)<0x8000u)p+=6u; p+=2u;
    d->nem_tiles=p;
    for(;;){
        uint16_t w=rom16(p);
        if(w==0xFFFEu){p+=8u;continue;}
        if(w&0x8000u){p+=2u;break;}
        p+=6u;
    }
    d->kos_ram=p; while(rom16(p)!=0xFFFFu)p+=6u; p+=2u;
    d->row_fg=rom8(p++); d->col_fg=rom8(p++);
    d->row_bg=rom8(p++); d->col_bg=rom8(p++);
    d->collision_plane=rom8(p++); ++p;
    if(d->collision_plane>1u)return 0;
    uint16_t mode=rom16(p); p+=2u; if(!mode)p+=8u;
    mode=rom16(p); p+=2u; if(!mode)p+=8u;
    d->chunks=p; while(rom16(p)!=0xFFFFu)p+=4u; p+=2u;
    /* Source MapData stores a byte-coded RunMapUpdates list here.  Preserve
     * its ROM pointer instead of skipping it so dynamic map behavior can be
     * lowered centrally from the authored table. */
    d->map_updates=p;
    p=skip_ff_bytes(p);
    d->trans1=p; p=skip_word_records(p,10u);
    d->trans2=p; p=skip_word_records(p,10u);
    d->objects=p; p=skip_word_records(p,10u);
    d->treasure=p; p=skip_word_records(p,6u);
    p=skip_word_records(p,2u); /* tile animations */
    if(d->is_world){ d->layout_fg=0u; d->layout_bg=0u; }
    else { d->layout_fg=rom32(p);p+=4u; d->layout_bg=rom32(p);p+=4u; }
    d->dialogue_tree=rom32(p);p+=4u;
    d->interactions=p;
    while(rom16(p)!=0xFFFFu)p+=2u; /* terminator-safe word scan */
    p+=2u;
    d->events=p;
    p=skip_ff_bytes(p);
    d->palette=rom32(p);p+=4u;
    /* Poison/random/teleport/dungeon bytes are followed by the source
     * MapDataManager word list.  Preserve that list so map-load behavior is
     * implemented through one shared dispatcher instead of map-ID patches. */
    p+=4u;d->data_manager=p;
    return d->palette<0x300000u;
}

/* PSC-style fixed native MapData dispatch.  The original variable-length ROM
 * record remains the correctness fallback, but every valid retail map is
 * compiled to a constant descriptor by the host builder. */
static int ps4_parse_field_map(uint16_t map_id,struct Ps4FieldMapDesc* d){
    const Ps4PscMapDesc* s=ps4_psc_map_desc_get(map_id);if(!s)return ps4_parse_field_map_canonical(map_id,d);
    d->map_id=map_id;d->general=s->general;d->music=s->music;d->is_world=s->is_world;d->row_fg=s->row_fg;d->col_fg=s->col_fg;d->row_bg=s->row_bg;d->col_bg=s->col_bg;d->collision_plane=s->collision_plane;
    d->kos_tiles=s->kos_tiles;d->nem_tiles=s->nem_tiles;d->kos_ram=s->kos_ram;d->chunks=s->chunks;d->map_updates=s->map_updates;d->trans1=s->trans1;d->trans2=s->trans2;d->objects=s->objects;d->treasure=s->treasure;d->layout_fg=s->layout_fg;d->layout_bg=s->layout_bg;d->data_manager=s->data_manager;d->dialogue_tree=s->dialogue_tree;d->interactions=s->interactions;d->events=s->events;d->palette=s->palette;return 1;
}

static unsigned field_plane_chunk_width(uint16_t layout_off){
    return (unsigned)((layout_off==0xB000u?field_map.row_bg:field_map.row_fg)+1u);
}
static unsigned field_plane_chunk_height(uint16_t layout_off){
    return (unsigned)((layout_off==0xB000u?field_map.col_bg:field_map.col_fg)+1u);
}
static unsigned field_wrap_unsigned(unsigned v,unsigned span){
    /* Runtime samples cross at most one seam.  Avoid %, which can pull an ARM
     * divide helper into the 59.7 Hz path. */
    if(v>=span)v-=span;
    return v;
}
static int field_wrap_signed(int v,int span){
    if(v<0)v+=span; else if(v>=span)v-=span;
    return v;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static uint8_t field_chunk_id(uint16_t layout_off,unsigned cx,unsigned cy){
    if(field_map.is_world){
        /* PSIV world maps: 128x128 chunks as sixteen raw 128x8 ROM pages. */
        uint32_t table;
        if(field_map.map_id==0u)table=(layout_off==0xB000u)?0x10BE02u:0x107DC2u;
        else                    table=(layout_off==0xB000u)?0x1175C4u:0x115584u;
        cx&=127u; cy&=127u;
        uint32_t page=rom32(table+((cy>>3)&15u)*4u);
        return rom8(page+(cy&7u)*128u+cx);
    }
    unsigned cw=field_plane_chunk_width(layout_off);
    return work_ram[(uint16_t)(layout_off+cy*cw+cx)];
}
__attribute__((section(".iwram.text"),noinline))
static uint16_t field_entry(uint16_t layout_off,unsigned wx,unsigned wy){
    unsigned cw=field_plane_chunk_width(layout_off),ch=field_plane_chunk_height(layout_off);
    wx=field_wrap_unsigned(wx,cw*32u); wy=field_wrap_unsigned(wy,ch*32u);
    unsigned cx=wx>>5,cy=wy>>5;
    uint8_t cid=field_chunk_id(layout_off,cx,cy);
    unsigned tx=(wx&31u)>>3,ty=(wy&31u)>>3;
    return (uint16_t)(ram16r((uint16_t)(0x6000u+(unsigned)cid*32u+(ty*4u+tx)*2u))&0xBFFFu);
}
static uint16_t field_sample(unsigned wx,unsigned wy){
    uint16_t a=field_entry(0xA000u,wx,wy),b=field_entry(0xB000u,wx,wy);
    unsigned x=wx&7u,y=wy&7u,ia=tile_px(a,x,y),ib=tile_px(b,x,y);
    unsigned pa=(a>>13)&3u,pb=(b>>13)&3u,pra=a>>15,prb=b>>15;
    if(ia&&(pra||!ib||!prb))return pal(pa,ia,0u);
    if(ib)return pal(pb,ib,0u);
    if(ia)return pal(pa,ia,0u);
    return md2gba(cram[0]);
}
static uint8_t field_sample_index(unsigned wx,unsigned wy){
    uint16_t a=field_entry(0xA000u,wx,wy),b=field_entry(0xB000u,wx,wy);
    unsigned x=wx&7u,y=wy&7u,ia=tile_px(a,x,y),ib=tile_px(b,x,y);
    unsigned pa=(a>>13)&3u,pb=(b>>13)&3u,pra=a>>15,prb=b>>15;
    if(ia&&(pra||!ib||!prb))return (uint8_t)(pa*16u+ia);
    if(ib)return (uint8_t)(pb*16u+ib);
    if(ia)return (uint8_t)(pa*16u+ia);
    return 0u;
}
static void render_field(unsigned camx,unsigned camy){
    for(unsigned y=0;y<160u;y++)for(unsigned x=0;x<240u;x++)VRAM16[y*240u+x]=field_sample(camx+x,camy+y);
}

/* Bit-exact lowering of GetChunkAndCollision ($45A5C).  One source quirk is
 * easy to miss: the collision-plane flag chooses one plane's layout but the
 * opposite row-size byte as the RAM-layout stride.  The older runtime used the
 * selected plane's own width, which is wrong on maps where FG/BG geometry
 * differs and can turn valid floor into walls (or vice versa). */
static uint8_t field_chunk_index_at(int world_x,int world_y,int dx,int dy){
    const unsigned cp=field_map.collision_plane?1u:0u;
    const unsigned wrap_cw=(unsigned)(cp?field_map.row_bg:field_map.row_fg)+1u;
    const unsigned wrap_ch=(unsigned)(cp?field_map.col_bg:field_map.col_fg)+1u;
    int x=field_wrap_signed(world_x+dx,(int)(wrap_cw*32u));
    int y=field_wrap_signed(world_y+dy+16,(int)(wrap_ch*32u));
    unsigned cx=(unsigned)x>>5,cy=(unsigned)y>>5;
    if(field_map.is_world)return field_chunk_id(cp?0xB000u:0xA000u,cx,cy);
    const unsigned stride=(unsigned)(cp?field_map.row_fg:field_map.row_bg)+1u;
    return work_ram[(uint16_t)((cp?0xB000u:0xA000u)+cy*stride+cx)];
}
/* CP20: active-map collision nibbles lowered from the canonical 32-byte
 * chunk records.  Four 4-bit quadrant values per chunk fit one uint16_t.
 * Rebuilt after all map-load managers so authored chunk copies are reflected. */
static uint16_t field_collision_nibbles[256];
static void field_collision_nibbles_rebuild(void){
    for(unsigned cid=0u;cid<256u;cid++){
        uint16_t packed=0u;unsigned chunk=0x6000u+cid*32u;
        for(unsigned qid=0u;qid<4u;qid++){
            unsigned off=chunk+((qid&1u)?4u:0u)+((qid&2u)?0x10u:0u);const uint8_t* q=&work_ram[off];uint8_t c=0u;
            if(q[0]&0x40u)c|=1u;if(q[2]&0x40u)c|=2u;if(q[8]&0x40u)c|=4u;if(q[10]&0x40u)c|=8u;
            packed=(uint16_t)(packed|((uint16_t)c<<(qid*4u)));
        }
        field_collision_nibbles[cid]=packed;
    }
}
__attribute__((section(".iwram.text"),noinline))
static uint8_t field_collision_at(int world_x,int world_y,int dx,int dy){
    const unsigned cp=field_map.collision_plane?1u:0u;
    const unsigned wrap_cw=(unsigned)(cp?field_map.row_bg:field_map.row_fg)+1u;
    const unsigned wrap_ch=(unsigned)(cp?field_map.col_bg:field_map.col_fg)+1u;
    int x=field_wrap_signed(world_x+dx,(int)(wrap_cw*32u));
    int y=field_wrap_signed(world_y+dy+16,(int)(wrap_ch*32u));
    unsigned tx=(unsigned)x>>3,ty=(unsigned)y>>3,cx=(unsigned)x>>5,cy=(unsigned)y>>5;
    uint8_t cid;
    if(field_map.is_world){
        cid=field_chunk_id(cp?0xB000u:0xA000u,cx,cy);
    }else{
        const unsigned stride=(unsigned)(cp?field_map.row_fg:field_map.row_bg)+1u;
        const uint16_t layout_off=cp?0xB000u:0xA000u;
        cid=work_ram[(uint16_t)(layout_off+cy*stride+cx)];
    }
    unsigned qid=((tx&2u)?1u:0u)|((ty&2u)?2u:0u);
    return (uint8_t)((field_collision_nibbles[cid]>>(qid*4u))&15u);
}
static int collision_blocks(uint8_t c){return c>=8u&&c<=12u;}

static int ps4_debug_find_safe_spawn(uint16_t* outx,uint16_t* outy){
    /* Debug-only generic placement: search the active source collision plane
     * for the closest clean 16-pixel anchor to the map center.  This keeps the
     * all-map warp independent of hand-authored per-map coordinates. */
    unsigned w=((unsigned)(field_map.collision_plane?field_map.row_bg:field_map.row_fg)+1u)*32u;
    unsigned h=((unsigned)(field_map.collision_plane?field_map.col_bg:field_map.col_fg)+1u)*32u;
    if(w<32u||h<32u)return 0;unsigned cx=(w>>1)&~15u,cy=(h>>1)&~15u;
    unsigned best=0xFFFFFFFFu,bx=16u,by=16u;int found=0;
    for(unsigned pass=0u;pass<2u&&!found;pass++){
        best=0xFFFFFFFFu;
        for(unsigned y=16u;y+16u<h;y+=16u)for(unsigned x=16u;x+16u<w;x+=16u){
            uint8_t c=field_collision_at((int)x,(int)y,0,0);if(collision_blocks(c)||c==1u)continue;if(pass==0u&&c!=0u)continue;
            unsigned dx=x>cx?x-cx:cx-x,dy=y>cy?y-cy:cy-y,score=dx+dy;if(score<best){best=score;bx=x;by=y;found=1;}
        }
    }
    if(!found)return 0;*outx=(uint16_t)bx;*outy=(uint16_t)by;return 1;
}

static unsigned load_kos_vram_auto(uint32_t src,uint16_t tile){
    /* Generic Event/Map art must never borrow Chunk_Table ($FFFF6000).
     * Several source events load Kosinski art while the live map is still
     * resident, so the old port's work_ram[0x6000] scratch silently replaced
     * collision/layout chunks.  Decode straight into the virtual MD VRAM
     * destination: the compressed input is immutable ROM and both translated
     * decompressors support a bounded output buffer. */
    unsigned dst=(unsigned)tile*32u;if(dst>=65536u)return 0u;
    unsigned n=kos_decode(src,&md_vram[dst],65536u-dst);if(n)field_gba_ready_bind(PS4_FASTGFX_KOS,src,tile,n);return n;
}
static unsigned load_nem_vram_auto(uint32_t src,uint16_t tile){
    unsigned dst=(unsigned)tile*32u;if(dst>=65536u)return 0u;
    unsigned n=nemesis_fast(src,&md_vram[dst],65536u-dst);if(n)field_gba_ready_bind(PS4_FASTGFX_NEM,src,tile,n);return n;
}

static int field_build_tile_remap(void){
    for(unsigned i=0;i<2048u;i++)field_tile_remap[i]=0xFFFFu;
    field_tile_count=0u;
    field_tile_remap[0]=0u; field_tile_source[field_tile_count++]=0u;
    /* First find which chunk IDs are actually reachable, then inspect each only
     * once.  World maps have 32k layout cells; this cuts map-load work by orders
     * of magnitude without changing any frame-time code. */
    uint8_t seen_chunk[256]; for(unsigned i=0;i<256u;i++)seen_chunk[i]=0u;
    const uint16_t offs[2]={0xA000u,0xB000u};
    for(unsigned plane=0;plane<2u;plane++){
        uint16_t lo=offs[plane]; unsigned cw=field_plane_chunk_width(lo),ch=field_plane_chunk_height(lo);
        for(unsigned cy=0;cy<ch;cy++)for(unsigned cx=0;cx<cw;cx++)seen_chunk[field_chunk_id(lo,cx,cy)]=1u;
    }
    for(unsigned cid=0;cid<256u;cid++)if(seen_chunk[cid]){
        unsigned base=0x6000u+cid*32u;
        for(unsigned q=0;q<32u;q+=2u){
            unsigned src=ram16r((uint16_t)(base+q))&0x07FFu;
            if(field_tile_remap[src]==0xFFFFu){
                if(field_tile_count>=1024u)return 0;
                field_tile_remap[src]=(uint16_t)field_tile_count;
                field_tile_source[field_tile_count++]=(uint16_t)src;
            }
        }
    }
    return 1;
}

__attribute__((section(".iwram.text"),noinline,aligned(16)))
static uint16_t field_npc_rng_next(void){
    /* Exact PSIV UpdateRNGSeed ($4236C).  Keep the authoritative seed in the
     * original virtual RAM location $EF0C so field events, AI, encounters and
     * every other translated consumer observe one shared source RNG stream. */
    uint32_t q=ram32r(0xEF0Cu);if((uint16_t)q==0u)q=0x2A6D365Bu;
    q=q*41u;uint16_t lo=(uint16_t)q,hi=(uint16_t)(q>>16),sum=(uint16_t)(lo+hi);
    ram32w(0xEF0Cu,((uint32_t)sum<<16)|lo);return sum;
}
static uint16_t ps4_md_hv_counter(void){
    /* On the Mega Drive, a5 is initialized to $C00000, so UpdateRNGSeed2's
     * 8(a5) is the VDP H/V counter at $C00008.  The GBA has an exact VCOUNT
     * register but no public horizontal-dot counter.  Timer 2 is otherwise
     * unused by this runtime, so run it free at the CPU clock and use its
     * sub-scanline phase as the hardware H-counter analogue.  >>2 gives a
     * roughly scanline-sized 8-bit phase while preserving real hardware timing. */
    return (uint16_t)(((REG_VCOUNT&0xFFu)<<8)|((REG_TM2CNT_L>>2)&0xFFu));
}
static uint16_t ps4_rng2_next(void){
    /* PSIV UpdateRNGSeed2 ($4239E):
     *   d0 = 8(a5) + $EF1C - $EF0C; ror.w #1,$EF0C.
     * Here 8(a5) is represented by the GBA hardware H/V equivalent above;
     * $EF1C remains the source VBlank counter and is advanced by waitvb(). */
    uint16_t seed=ram16r(0xEF0Cu);
    uint16_t out=(uint16_t)(ps4_md_hv_counter()+ram16r(0xEF1Cu)-seed);
    seed=(uint16_t)((seed>>1)|(seed<<15));ram16w(0xEF0Cu,seed);return out;
}
/* FieldObj_DoObjCollision tests render_flags bit 3.  This table is the
 * source-initialization result derived from FieldObjectsJmpTbl handlers: only
 * handlers that explicitly bset #3 own collision.  Objects that leave the bit
 * clear (effects, bosses used as scenery, vehicles, transient graphics, etc.)
 * must not become solid merely because the native renderer has an object slot. */
static uint8_t field_obj_source_blocks(uint16_t type){
    switch(type){
    case 0x038u: case 0x03Cu: case 0x040u: case 0x044u: case 0x048u: case 0x04Cu: case 0x050u: case 0x054u:
    case 0x058u: case 0x05Cu: case 0x060u: case 0x064u: case 0x068u: case 0x06Cu: case 0x070u: case 0x074u:
    case 0x088u: case 0x08Cu: case 0x09Cu: case 0x0A0u:
    case 0x0C0u: case 0x0C4u: case 0x0C8u: case 0x0CCu: case 0x0D0u: case 0x0D4u: case 0x0D8u: case 0x0DCu:
    case 0x0E0u: case 0x0E4u: case 0x0E8u: case 0x0ECu: case 0x0F0u: case 0x0F4u: case 0x0F8u: case 0x0FCu:
    case 0x100u: case 0x104u: case 0x108u: case 0x10Cu: case 0x110u: case 0x114u: case 0x118u: case 0x11Cu:
    case 0x128u: case 0x12Cu: case 0x130u: case 0x134u: case 0x138u: case 0x13Cu: case 0x144u: case 0x148u: case 0x14Cu:
    case 0x15Cu: case 0x164u: case 0x168u: case 0x180u: case 0x18Cu: case 0x190u: case 0x194u: case 0x198u: case 0x19Cu:
    case 0x1B4u: case 0x1D4u: case 0x1F0u:
    case 0x224u: case 0x228u: case 0x22Cu: case 0x230u: case 0x23Cu: case 0x240u: case 0x244u: case 0x248u:
    case 0x24Cu: case 0x250u: case 0x254u: case 0x258u: case 0x260u: case 0x264u: case 0x268u: case 0x26Cu:
    case 0x270u: case 0x274u: case 0x278u: case 0x27Cu: case 0x298u: case 0x29Cu:
    case 0x2C8u: case 0x2CCu: case 0x2D0u: case 0x2D4u: case 0x2D8u: case 0x2DCu: case 0x2E0u: case 0x2E4u:
    case 0x2E8u: case 0x2ECu: case 0x2F0u: case 0x2F4u: case 0x2FCu: case 0x310u: case 0x328u: case 0x330u:
    case 0x34Cu: case 0x350u: case 0x354u:
        return 1u;
    default:return 0u;
    }
}

static void field_obj_init_motion(struct FieldObjLite* o){
    o->anim=0u;o->move_kind=0u;o->move_dir=0u;o->move_left=0u;o->move_phase=0u;o->timer=0u;o->seq_pos=0u;
    o->bound_x=o->bound_y=o->max_x=o->max_y=0;
    /* Source FieldObjectsJmpTbl lowering.  Packed boundary words are X in the
     * high byte and Y in the low byte, matching loc_4A3B6.  Fixed-facing
     * classes set the source facing here even though they cannot leave their
     * zero-width movement axis. */
    switch(o->type){
    case 0x0184u: o->move_kind=1u;o->max_x=o->max_y=2;o->bound_x=o->bound_y=1;break;
    case 0x0038u: o->facing=0u; break;                                      /* NPCType1 down */
    case 0x003Cu: o->move_kind=1u;o->max_x=o->max_y=4;o->bound_x=o->bound_y=2;break;
    case 0x0040u: o->move_kind=2u;o->max_x=o->max_y=4;o->bound_x=o->bound_y=2;break;
    case 0x0044u: o->move_kind=3u;o->max_x=o->max_y=4;o->bound_x=o->bound_y=2;break;
    case 0x0048u: o->move_kind=4u;o->max_x=o->max_y=2;o->bound_x=0;o->bound_y=2;break;
    case 0x004Cu: o->move_kind=6u;o->max_x=2;o->max_y=0;o->bound_x=1;o->bound_y=0;break; /* Piata guards */
    case 0x0050u: o->move_kind=7u;o->max_x=1;o->max_y=0;o->bound_x=1;o->bound_y=0;break; /* Mile rancher */
    case 0x0054u: o->move_kind=8u;o->max_x=o->max_y=16;o->bound_x=o->bound_y=8;break;     /* Zema */
    case 0x0058u: o->facing=4u;break;                                       /* NPCType9 up */
    case 0x005Cu: o->facing=12u;break;                                      /* NPCType10 left */
    case 0x0060u: o->move_kind=9u;o->max_x=11;o->max_y=1;o->bound_x=4;o->bound_y=1;break;
    case 0x0064u: o->move_kind=10u;o->max_x=o->max_y=16;o->bound_x=o->bound_y=8;break;
    case 0x006Cu: o->move_kind=5u;o->max_x=1;o->max_y=0;o->bound_x=1;o->bound_y=0;break; /* Hahn */
    case 0x0088u: o->move_kind=11u;o->max_x=4;o->max_y=0;o->bound_x=0;o->bound_y=0;break;
    case 0x00C0u: o->facing=0u;break;                                       /* NPCType16 down */
    case 0x00C4u: o->move_kind=12u;o->max_x=o->max_y=4;o->bound_x=o->bound_y=0;break;
    case 0x00C8u: o->facing=12u;break;                                      /* Piata dorm woman left */
    case 0x00CCu: o->facing=4u;break;                                       /* child up */
    case 0x00F4u: o->facing=0u;break;
    case 0x00F8u: o->move_kind=3u;o->max_x=o->max_y=8;o->bound_x=o->bound_y=4;break;
    case 0x0100u: o->facing=8u;break;                                       /* right */
    case 0x0104u: o->facing=0u;break;
    case 0x0108u: o->facing=0u;break;
    case 0x010Cu: o->facing=12u;break;
    case 0x0110u: o->facing=8u;break;
    case 0x0118u: o->move_kind=13u;o->max_x=6;o->max_y=4;o->bound_x=0;o->bound_y=0;break; /* Aiedo small kid course A */
    case 0x011Cu: o->move_kind=14u;o->max_x=6;o->max_y=4;o->bound_x=2;o->bound_y=0;break; /* Aiedo small girl course B */
    case 0x0128u: o->move_kind=15u;o->max_x=4;o->max_y=0;o->bound_x=1;o->bound_y=0;break; /* Aiedo savings man */
    case 0x0138u: o->move_kind=3u;o->max_x=o->max_y=8;o->bound_x=o->bound_y=4;break;       /* Aiedo shopper */
    case 0x013Cu: o->move_kind=16u;o->max_x=16;o->max_y=4;o->bound_x=1;o->bound_y=0;break;/* Aiedo mother + child course */
    default: break;
    }
}

#ifndef PS4_NPCTYPE1_STANDARD_NPC
#define PS4_NPCTYPE1_STANDARD_NPC 1
#endif
static unsigned field_obj_configure_render(struct FieldObjLite* o,unsigned tile){
    o->gba_tile=(uint16_t)tile;o->render_w=16u;o->render_h=32u;
    /* Standard NPC mapping loc_4A7D0 uses Y=$FE (-2) and source mapping
     * X byte +4 = 0.  PSIV's field sprite builder reads byte +4, then skips
     * byte +5; v0.9.10.2 accidentally interpreted byte +5 ($F0) as signed
     * X=-16, shifting ordinary actors one full source tile left of the map,
     * collision and transition coordinate system.  Keep the source logical
     * coordinate as the native GBA sprite X anchor. */
    o->render_x=0;o->render_y=-2;
    if(o->type==0x0074u||o->type==0x0078u||o->type==0x007Cu||o->type==0x0084u){
        /* $74 is a permanent invisible collision block.  $78/$7C/$84 are
         * event-only effect classes whose visible instances are constructed by
         * field_event_add_transient() with explicit GBA art.  A map-record
         * instance has no validated static carrier and must not submit stale OBJ
         * tiles (the old regression could look like a random enemy sprite). */
        o->render_w=0u;o->render_h=0u;return 0u;
    }
    if(o->type==0x00D8u||o->type==0x00DCu||o->type==0x00E4u){
        /* CP22 NPC placement audit: FieldObj_NPCType22/23/25 install mappings
         * loc_4A918/loc_4A920/loc_4A930 = one 16x32 piece at Y=0 (not the
         * standard Y=$FE).  Same frame carrier; 2 px lower. */
        o->render_w=16u;o->render_h=32u;o->render_x=0;o->render_y=0;return 8u;
    }
    if(o->type==0x018Cu){
        /* FieldObj_ProfHoltPetrified -> loc_4AA3C: one 16x32 piece at X=+8,
         * Y=-2.  The default template drew him 8 px left of the source. */
        o->render_w=16u;o->render_h=32u;o->render_x=8;o->render_y=-2;return 8u;
    }
    if(o->type==0x0108u||o->type==0x0140u){
        /* CP22 counter NPCs (runtime IMG_0855).  FieldObjectsJmpTbl[$108]
         * FieldObj_NPCType32 ("person in fixed position facing down", e.g. the
         * Piata item-shop keeper) and [$140] loc_49502 (Aiedo store keeper
         * facing down) install mapping loc_4A948 instead of the standard
         * loc_4A7D0: two 8x16 pieces at Y=0, X=0/+8, tiles +0/+4.  In source
         * column-major tile order those are the top two cells of each column of
         * the ordinary 16x32 frame, i.e. only head and shoulders are drawn and
         * the counter tiles supply the rest.  PSIV does not hide these actors
         * with plane priority.  Keep the ordinary 8-tile frame carrier (the GBA
         * 1D row-major copy puts the top 16 rows in its first four tiles) and
         * submit a 16x16 OBJ at the source Y=0 anchor. */
        o->render_w=16u;o->render_h=16u;o->render_x=0;o->render_y=0;return 8u;
    }
#if PS4_NPCTYPE1_STANDARD_NPC
    if(o->type==0x0098u){
        /* R16: FieldObj_CaveWallPiece ($98, ps4.asm ~98335) is the object that
         * installs mapping loc_4CABC (one 32x32 piece, tile offset +$10,
         * X byte +4 = 0, Y=0) via frame list loc_4C434. */
        o->render_w=32u;o->render_h=32u;o->render_x=0;o->render_y=0;return 16u;
    }
    /* FieldObj_NPCType1 ($38, ps4.asm 94741) is an ordinary 16x32 NPC:
     * mappings_addr loc_4A504 (walk frames loc_4A5E0.. -> loc_4A7D0/7D8/7E0),
     * NPCMoveDown + Animate2.  R15 rendered all 36 instances (27 maps: the
     * Principal, Aiedo gate NPCs, Culver's mother, ...) through the 32x32
     * CaveWallPiece mapping, which shows NPC frames 2+3 side by side -- the
     * "two Principals" and the doubled Aiedo guards.  Falls through to the
     * standard 16x32 template below. */
#else
    if(o->type==0x0038u){
        o->render_w=32u;o->render_h=32u;o->render_x=0;o->render_y=0;return 16u;
    }
#endif
    if(o->type==0x00ECu){
        /* FieldObjectsJmpTbl[$EC] -> $4C66E -> mapping $4CE8A.  The single
         * 32x24 piece starts at source X byte +4 = 0,Y=0.  The following $E0
         * byte is skipped by the original field sprite builder. */
        o->render_w=32u;o->render_h=32u;o->render_x=0;o->render_y=0;return 16u;
    }
    if(o->type==0x00A0u){
        /* FieldObj_TreasureChest: source X byte +4 is 0 in both mappings;
         * their union is x=0..16,y=8..32. */
        o->render_w=16u;o->render_h=32u;o->render_x=0;o->render_y=8;return 8u;
    }
    if(o->type==0x01D4u){
        /* FieldObj_WhiteTreasureChest: with the source X byte (+4), the two
         * mappings span x=-4..20,y=6..30.  Use a transparent 32x32 carrier. */
        o->render_w=32u;o->render_h=32u;o->render_x=-4;o->render_y=6;return 16u;
    }
    if(o->type==0x01A0u){
        /* FieldObj_SayaStars ($1A0): the six source mappings grow from an
         * 8x8 sparkle to a roughly 32x32 star cluster around Saya. */
        o->render_w=32u;o->render_h=32u;o->render_x=-12;o->render_y=-12;return 16u;
    }
    if(o->type==0x0184u){
        /* FieldObj_Xanafalgue: the Piata B1 source stream at base tile $27F
         * contains two 3x3 (24x24) frames, nine tiles per frame.  Earlier
         * builds fell through to the ordinary 16x32 NPC copier, interpreting
         * the monster sheet with the wrong stride and producing the distorted
         * enemy-looking tile near the start of B1.  Use a 32x32 GBA carrier
         * with the authored 24x24 image in its upper-left 3x3 tile region.
         * CP22: both source frames (loc_4CA96/loc_4CA9E) are Y=0, X=0. */
        o->render_w=32u;o->render_h=32u;o->render_x=0;o->render_y=0;
        return 16u;
    }
    if(o->type==0x0188u){
        /* FieldObj_Igglanova -> loc_4CCCC/loc_4CCD4: 4x4 source mapping at
         * X 0, Y -2.  CP22: loc_4CCD4's pattern word $0800 is the H-flip bit on
         * tile 0; the alternate idle frame mirrors the first frame.  (Earlier
         * builds read it as +8 tiles, showing half the monster plus 8 unrelated
         * tiles every other step: the partial/flickering field Igglanova.) */
        o->render_w=32u;o->render_h=32u;o->render_x=0;o->render_y=-2;
        return 16u;
    }
    return 8u;
}

/* Shared lowering for source MapDataManager / RunMapUpdates lists.
 * Only routines whose complete argument shape and side effects are currently
 * proven are enabled; unknown routines stop the walk rather than guessing how
 * many bytes/words they consume.  New maps therefore extend one dispatcher,
 * never acquire room-specific rendering patches. */
static uint8_t field_manager_party_position_pending;
static void field_palette_live_word(unsigned index,uint16_t value){
    if(index>=64u)return;cram[index]=value;uint16_t q=md2gba(value);field_gba_palette[index]=q;
    ram16w((uint16_t)(0xFB00u+index*2u),value);
    BG_PALETTE[index]=q;OBJ_PALETTE[index]=q;
}
static void field_palette_copy_rom_words(unsigned dst,uint32_t src,unsigned count){
    for(unsigned i=0u;i<count&&dst+i<64u;i++)field_palette_live_word(dst+i,rom16(src+i*2u));
}
static void field_palette_cycle4(unsigned dst,uint32_t src,unsigned frame_mask,unsigned frame_shift){
    unsigned phase=(native_main_frame_count&frame_mask)>>frame_shift;
    for(unsigned i=0u;i<4u;i++)field_palette_live_word(dst+i,rom16(src+phase*2u+i*8u));
}
static void field_palette_cycle_pair(unsigned dst,uint32_t src,unsigned phase,unsigned split){
    field_palette_live_word(dst,rom16(src+phase*2u));field_palette_live_word(dst+1u,rom16(src+split+phase*2u));
}
static uint16_t field_manager_obj_addr(unsigned slot,unsigned off){return (uint16_t)(0xC300u+slot*0x40u+off);}
static void field_manager_clear_secondary(unsigned slot){
    /* Field_Obj_Secondary is the source map-object list at $C300.  Preserve
     * source slot identity in both the native carrier and source RAM mirror. */
    if(slot<field_obj_count){field_objs[slot].active=0u;field_objs[slot].type=0u;}
    if(slot<32u)ram16w(field_manager_obj_addr(slot,0u),0u);
}
static void field_manager_set_type(unsigned slot,uint16_t type){
    if(slot<field_obj_count){field_objs[slot].type=type;field_objs[slot].active=(uint8_t)(type!=0u);field_objs[slot].blocks=field_obj_source_blocks(type);field_obj_init_motion(&field_objs[slot]);}
    if(slot<32u)ram16w(field_manager_obj_addr(slot,0u),type);
}
static void field_manager_set_dialogue(unsigned slot,uint8_t dialogue){
    if(slot<field_obj_count)field_objs[slot].dialog=dialogue;
    if(slot<32u)ram8w(field_manager_obj_addr(slot,0x14u),dialogue);
}
static void field_manager_set_position(unsigned slot,uint16_t x,uint16_t y){
    if(slot<field_obj_count){field_objs[slot].x=x;field_objs[slot].y=y;}
    if(slot<32u){ram16w(field_manager_obj_addr(slot,0x30u),x);ram16w(field_manager_obj_addr(slot,0x34u),y);}
}
static int field_manager_layout_base(unsigned plane,unsigned x,unsigned y,int* addr){
    if(field_map.is_world)return 0;unsigned w=(unsigned)(plane?field_map.row_bg:field_map.row_fg)+1u;
    unsigned h=(unsigned)(plane?field_map.col_bg:field_map.col_fg)+1u;if(x>=w||y>=h)return 0;
    *addr=(int)(plane?0xB000u:0xA000u)+(int)(y*w+x);return 1;
}
static void field_manager_layout_write_raw(unsigned plane,unsigned x,unsigned y,int delta,uint8_t value){
    int a;if(!field_manager_layout_base(plane,x,y,&a))return;a+=delta;if(a<0||a>=0x10000)return;
    work_ram[(uint16_t)a]=value;
}
static void field_manager_copy_ram_words(uint16_t src,uint16_t dst,unsigned count){
    for(unsigned i=0u;i<count;i++)ram16w((uint16_t)(dst+i*2u),ram16r((uint16_t)(src+i*2u)));
}
static void field_manager_copy_palette(unsigned dst,uint32_t src,unsigned count){field_palette_copy_rom_words(dst,src,count);}
static uint8_t field_hscroll_mode;
static uint8_t field_hscroll_dirty;
static uint8_t field_hblank_active_index;
static uint8_t field_hscroll_prepared;
static uint8_t field_hscroll_prepared_index;
/* The source VDP horizontal-scroll table lives at $FFFF0000.  Keep that exact
 * RAM image authoritative and translate it to GBA HBlank register streams only
 * at the presentation boundary.  Mode 2 consumes one pair per 8 scanlines;
 * mode 3 consumes one pair per scanline. */
static void field_hscroll_word(unsigned off,uint16_t a,uint16_t b){
    ram16w((uint16_t)off,a);ram16w((uint16_t)(off+2u),b);
}
static int8_t field_source_sine8(uint8_t angle){
    if(angle<0x80u)return (int8_t)rom8(0x423CCu+(uint32_t)angle);
    return (int8_t)(-(int)rom8(0x423CCu+(uint32_t)(0xFFu-angle)));
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_source_hscroll_update06(void){
    uint16_t a=(uint16_t)(0u-ram16r(0xEF94u));
    uint16_t b0=(uint16_t)(0u-ram16r(0xEF9Cu));
    uint16_t b1=(uint16_t)(0u-(uint16_t)(ram16r(0xEF9Cu)<<1));
    for(unsigned block=0u;block<32u;block++)field_hscroll_word(block*0x20u,a,block<16u?b0:b1);
    {int32_t v=(int32_t)ram32r(0xEC58u);v>>=2;ram32w(0xEC58u,(uint32_t)v);}
    ram8w(0xEC6Au,(uint8_t)(ram8r(0xEC6Au)|1u));
    field_hscroll_mode=2u;field_hscroll_dirty=1u;field_hscroll_prepared=0u;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_source_hscroll_update26(void){
    uint8_t phase=(uint8_t)(ram8r(0xECF2u)+1u);ram8w(0xECF2u,phase);
    uint8_t ang=phase;uint16_t a=(uint16_t)(0u-ram16r(0xEF94u));
    for(unsigned y=0u;y<256u;y++){
        int8_t sv=field_source_sine8(ang);
        /* Source: LSR.W #8 then LSR.W #3 after the sine routine's <<8 result. */
        uint16_t b=(uint16_t)(((uint8_t)sv)>>3);
        field_hscroll_word(y*4u,a,b);ang=(uint8_t)(ang+0x60u);
    }
    field_hscroll_mode=3u;field_hscroll_dirty=1u;field_hscroll_prepared=0u;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_source_hscroll_update31(void){
    uint8_t phase=(uint8_t)(ram8r(0xECFFu)+1u);ram8w(0xECFFu,phase);
    uint8_t ang=phase;uint16_t a=(uint16_t)(0u-ram16r(0xEF94u));
    for(unsigned y=0u;y<256u;y++){
        int v=(int)field_source_sine8(ang);v>>=4;v-=0x10;
        field_hscroll_word(y*4u,a,(uint16_t)(int16_t)v);ang=(uint8_t)(ang+4u);
    }
    field_hscroll_mode=3u;field_hscroll_dirty=1u;field_hscroll_prepared=0u;
}
static void field_source_palette_update2e(void){
    /* $55C36 clears palette words 0..13 every call, then on even frames places
     * the five-word source strip at a rotating position 1..13. */
    for(unsigned i=0u;i<14u;i++)field_palette_live_word(i,0u);
    if(native_main_frame_count&1u)return;
    uint8_t q=ram8r(0xECF2u);if(q>=13u)q=0u;unsigned pos=1u+q;
    for(unsigned i=0u;i<5u;i++){field_palette_live_word(pos,rom16(0x55C8Au+i*2u));if(++pos>=14u)pos=1u;}
    ram8w(0xECF2u,(uint8_t)(q+1u));
}
static void field_source_palette_update2f(void){
    if(native_main_frame_count&3u)return;uint8_t q=ram8r(0xECF3u);if(q>=28u)q=0u;
    uint32_t src=field_map.palette+0x60u+(uint32_t)q*28u;
    for(unsigned i=0u;i<14u;i++){uint16_t c=rom16(src+i*2u);field_palette_live_word(i,c);field_palette_live_word(16u+i,c);}
    ram8w(0xECF3u,(uint8_t)(q+1u));
}
static void field_source_palette_update30(void){
    if(native_main_frame_count&3u)return;uint8_t q=ram8r(0xECF3u);if(q>=14u)q=0u;
    uint32_t src=field_map.palette+0x60u+(uint32_t)q*2u;
    for(unsigned i=0u;i<14u;i++){uint16_t c=rom16(src+i*2u);field_palette_live_word(i,c);field_palette_live_word(16u+i,c);}
    ram8w(0xECF3u,(uint8_t)(q+1u));
}
static void field_apply_map_manager_load(unsigned id){
    switch(id){
    case 0x00u: break; /* $51DE4 MapDataMan_Null */
    case 0x01u: if(ps4_event_flag_test(0x08u))field_manager_clear_secondary(7u);break;
    case 0x02u: if(ps4_event_flag_test(0x0Au)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);}break;
    case 0x03u: if(ps4_flag_test_type(2u,0x13u))field_manager_clear_secondary(0u);break;
    case 0x04u: if(ps4_event_flag_test(0x0Bu)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);}break;
    case 0x05u:{ /* $51E4C: flag $33 palette + four authored BG layout pairs. */
        if(ps4_event_flag_test(0x33u)){static const uint8_t xy[8]={0x15u,0x0Bu,0x14u,0x15u,0x0Cu,0x0Fu,0x11u,0x0Fu};field_manager_copy_palette(48u,0x133E20u,16u);for(unsigned i=0u;i<4u;i++){field_manager_layout_write_raw(1u,xy[i*2u],xy[i*2u+1u],0,0x59u);field_manager_layout_write_raw(1u,xy[i*2u],xy[i*2u+1u],1,0x5Au);}}
        break;}
    case 0x06u: if(ps4_event_flag_test(0x10u))for(unsigned i=0u;i<4u;i++)field_manager_clear_secondary(i);break;
    case 0x07u: if(ps4_event_flag_test(0x11u))field_manager_clear_secondary(4u);break;
    case 0x08u: /* $51ED6 alternate chunk palette line */
        for(unsigned i=0u;i<0x1000u;i++){uint16_t a=(uint16_t)(0x6000u+i*2u);ram16w(a,(uint16_t)(ram16r(a)|0x2000u));}break;
    case 0x09u:{ /* $51FA2: source ASR.W #4 convergence around $258. */
        int16_t x=(int16_t)ram16r(0xEF9Cu),y=(int16_t)ram16r(0xEF98u);int16_t dx=(int16_t)(x-(int16_t)0x0258),dy=(int16_t)(y-(int16_t)0x0258);x=(int16_t)(x-(dx>>4));y=(int16_t)(y-(dy>>4));ram16w(0xEF9Cu,(uint16_t)x);ram16w(0xEF98u,(uint16_t)y);break;}
    case 0x0Au:{ /* $51FC2: initialize split 8-line horizontal scroll. */
        ram16w(0xEF9Cu,ram16r(0xEF9Cu)>=0x0160u?0x0080u:0u);
        ram32w(0xEC50u,0u);ram32w(0xEC54u,0u);ram32w(0xEC58u,0u);ram32w(0xEC5Cu,0u);
        field_source_hscroll_update06();break;}
    case 0x0Bu: if(ps4_flag_test_type(2u,0x00u)){field_manager_layout_write_raw(1u,0x0Bu,0x07u,0,0x29u);field_manager_layout_write_raw(1u,0x0Bu,0x08u,0,0x2Au);}break;
    case 0x0Cu: if(!ps4_event_flag_test(0x2Au))field_manager_clear_secondary(4u);break;
    case 0x0Du: if(!ps4_event_flag_test(0x33u)){field_manager_copy_palette(48u,0x12F570u,16u);field_manager_set_position(0u,0x0210u,0x0160u);field_manager_set_position(1u,0x0220u,0x0170u);field_manager_clear_secondary(2u);field_manager_clear_secondary(3u);}break;
    case 0x0Eu: if(ps4_event_flag_test(0x33u)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);}break;
    case 0x0Fu: if(!ps4_event_flag_test(0x33u)){field_manager_copy_palette(48u,0x12F570u,16u);field_manager_clear_secondary(3u);field_manager_clear_secondary(4u);}break;
    case 0x10u: if(ps4_event_flag_test(0x33u)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);field_manager_clear_secondary(5u);}break;
    case 0x11u: if(ps4_event_flag_test(0x34u))for(unsigned i=0u;i<6u;i++)field_manager_clear_secondary(i);break;
    case 0x12u:{ /* $52114: flag $33 false -> two exact raw 8x8 tile uploads. */
        if(!ps4_event_flag_test(0x33u)){
            for(unsigned i=0u;i<32u;i++)md_vram[0x0373u*32u+i]=rom8(0x1353BAu+i);
            for(unsigned i=0u;i<32u;i++)md_vram[0x0377u*32u+i]=rom8(0x1353DAu+i);
            field_gba_ready_invalidate(0x0373u,1u);field_gba_ready_invalidate(0x0377u,1u);
        }
        break;}
    case 0x13u: field_palette_live_word(10u,0x068Au);field_palette_live_word(11u,0x0468u);break;
    case 0x14u:{static const uint8_t f[6]={0x09u,0x0Au,0x0Du,0x0Eu,0x0Fu,0x10u};for(unsigned i=0;i<6u;i++)ps4_flag_clear_type(2u,f[i]);break;}
    case 0x15u:{unsigned y0=ps4_event_flag_test(0x09u)?0x16u:0x11u,y1=ps4_event_flag_test(0x0Au)?0x18u:0x1Du;field_manager_layout_write_raw(0u,0x16u,y0,0,0xCCu);field_manager_layout_write_raw(0u,0x16u,y0,1,0xCDu);field_manager_layout_write_raw(0u,0x16u,y1,0,0xCCu);field_manager_layout_write_raw(0u,0x16u,y1,1,0xCDu);break;}
    case 0x16u:{static const uint8_t x[4]={0x0Eu,0x11u,0x1Cu,0x1Fu},y0[4]={0x15u,0x0Bu,0x0Bu,0x15u},y1[4]={0x1Cu,0x12u,0x12u,0x1Cu},fl[4]={0x0Du,0x0Eu,0x0Fu,0x10u};for(unsigned i=0;i<4u;i++){unsigned y=ps4_event_flag_test(fl[i])?y1[i]:y0[i];field_manager_layout_write_raw(0u,x[i],y,0,0xCCu);field_manager_layout_write_raw(0u,x[i],y,1,0xCDu);}break;}
    case 0x17u:{static const uint8_t f[4]={0x0Bu,0x0Cu,0x11u,0x12u};for(unsigned i=0;i<4u;i++)ps4_flag_clear_type(2u,f[i]);break;}
    case 0x18u: if(!ps4_event_flag_test(0x0Bu))ps4_flag_clear_type(2u,0x13u);break;
    case 0x19u: if(ps4_event_flag_test(0x0Bu)&&!ps4_event_flag_test(0x0Fu))field_manager_party_position_pending=1u;break;
    case 0x1Au: if(!ps4_event_flag_test(0x33u))ps4_event_flag_set(0x14u);else ps4_flag_clear_type(0u,0x14u);break;
    case 0x1Bu: break; case 0x1Cu: break;
    case 0x1Du: if(!ps4_event_flag_test(0x34u))field_manager_clear_secondary(1u);break;
    case 0x1Eu: /* R16 fix (differential oracle): clear Hahn if Reunion($DA) || !AfterAlysDeath($63); R15 had the flags swapped. */
        if(ps4_event_flag_test(0xDAu)||!ps4_event_flag_test(0x63u))field_manager_clear_secondary(2u);break;
    case 0x1Fu: if(!ps4_event_flag_test(0x63u)&&ps4_event_flag_test(0x42u))field_manager_clear_secondary(0u);break;
    case 0x20u:{int f67=ps4_event_flag_test(0x67u),f42=ps4_event_flag_test(0x42u);if(f67||!f42)for(unsigned i=0u;i<4u;i++)field_manager_clear_secondary(i);if(f67&&f42){field_manager_layout_write_raw(1u,0x11u,0x10u,0,0xA2u);field_manager_layout_write_raw(1u,0x11u,0x10u,-0x1000,0xA4u);field_manager_layout_write_raw(1u,0x11u,0x10u,-0x1020,0xA3u);}break;}
    case 0x21u: if(!ps4_event_flag_test(0x65u)&&ps4_event_flag_test(0x30u))field_manager_clear_secondary(0u);break;
    case 0x22u: /* R16 fix: MapDataMan_TonoeGryz clears Gryz if Elsydeon($D9) || (!ZioNurvus($65) && GryzJoined($30)). */
        if(ps4_event_flag_test(0xD9u)||(!ps4_event_flag_test(0x65u)&&ps4_event_flag_test(0x30u)))field_manager_clear_secondary(2u);break;
    case 0x23u:{ /* $52478: post-$65 object teardown + source Nemesis upload. */
        if(ps4_event_flag_test(0x65u)){
            for(unsigned i=0u;i<4u;i++)field_manager_clear_secondary(i);
            for(unsigned i=5u;i<=8u;i++)field_manager_clear_secondary(i);
            (void)load_nem_vram_auto(0x121600u,0x02DBu);
        }
        break;}
    case 0x24u: if(ps4_event_flag_test(0x65u))field_manager_clear_secondary(0u);break;
    case 0x25u: if(ps4_event_flag_test(0x65u)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);}break;
    case 0x26u: if(ps4_event_flag_test(0x9Eu)){field_manager_copy_ram_words(0x6060u,0x6020u,32u);field_manager_copy_ram_words(0x60C0u,0x60A0u,16u);field_manager_copy_ram_words(0x6160u,0x60E0u,16u);field_manager_copy_ram_words(0x6180u,0x6200u,16u);field_manager_copy_ram_words(0x6000u,0x62C0u,16u);field_manager_copy_ram_words(0x6000u,0x6920u,16u);field_manager_copy_ram_words(0x6000u,0x6940u,16u);field_manager_copy_ram_words(0x6000u,0x6CA0u,16u);field_manager_copy_ram_words(0x6000u,0x6CC0u,16u);field_manager_copy_ram_words(0x6000u,0x6CE0u,16u);if(ps4_event_flag_test(0x9Cu))field_manager_copy_ram_words(0x6100u,0x6960u,16u);}break;
    case 0x27u: field_palette_live_word(12u,0x0680u);field_palette_live_word(13u,0x0640u);field_palette_live_word(19u,0x068Au);field_palette_live_word(20u,0x0468u);field_palette_live_word(21u,0x0024u);break;
    case 0x28u: field_manager_copy_palette(17u,0x52608u,5u);break;
    case 0x29u:{ /* $52612: palette-line clone/patch + two exact foreground layout runs. */
        for(unsigned i=0u;i<16u;i++)field_palette_live_word(16u+i,ram16r((uint16_t)(0xFB00u+i*2u)));
        field_palette_copy_rom_words(1u,0x52696u,9u);
        if(ps4_event_flag_test(0x40u)){
            field_palette_live_word(13u,0x0CA0u);
            for(unsigned x=7u;x<24u;x++){field_manager_layout_write_raw(0u,x,12u,0,0xA6u);field_manager_layout_write_raw(0u,x,13u,0,0xA6u);}
        }else field_palette_live_word(13u,0x0420u);
        break;}
    case 0x2Au: field_manager_copy_palette(17u,0x526BEu,8u);break;
    case 0x2Bu: field_manager_copy_palette(17u,0x526E4u,13u);break;
    case 0x2Cu: field_manager_copy_palette(17u,0x52714u,13u);break;
    case 0x2Du: field_manager_copy_palette(17u,0x52744u,5u);break;
    case 0x2Eu: field_manager_copy_palette(17u,0x52764u,13u);break;
    case 0x2Fu: field_manager_copy_palette(17u,0x52794u,10u);break;
    case 0x30u: field_manager_copy_palette(18u,0x527BEu,6u);break;
    case 0x31u: if(ps4_event_flag_test(0x46u)||ps4_event_flag_test(0x42u)){field_manager_clear_secondary(3u);field_manager_clear_secondary(4u);}break;
    case 0x32u: if(ps4_event_flag_test(0x7Du)||(!ps4_event_flag_test(0x46u)&&!ps4_event_flag_test(0x42u))){field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);}break;
    case 0x33u: ram16w(0xEF9Cu,0x0160u);ram16w(0xEF98u,0x0160u);break;
    case 0x34u:{static const uint16_t pal[14]={0x0000u,0x0EEEu,0x0000u,0x0226u,0x0448u,0x0AAEu,0x066Au,0x0EEEu,0x0040u,0x0262u,0x0484u,0x00AEu,0x04A8u,0x0EEEu};for(unsigned i=0u;i<14u;i++)field_palette_live_word(48u+i,pal[i]);break;}
    case 0x35u: if(ps4_event_flag_test(0x62u))field_manager_clear_secondary(0u);break;
    case 0x36u: field_manager_clear_secondary(1u);break;
    case 0x37u: field_manager_copy_palette(17u,0x5289Au,5u);break;
    case 0x38u: if(!ps4_event_flag_test(0x48u)){field_manager_layout_write_raw(1u,0x0Cu,0x09u,0,0x16u);field_manager_layout_write_raw(1u,0x0Cu,0x09u,1,0x16u);field_manager_layout_write_raw(1u,0x0Cu,0x09u,0x20,0x16u);field_manager_layout_write_raw(1u,0x0Cu,0x09u,0x21,0x16u);}if(ps4_event_flag_test(0x41u))field_manager_clear_secondary(0u);break;
    case 0x39u: if(ps4_event_flag_test(0x67u)){field_manager_layout_write_raw(0u,0x09u,0x05u,0,0x8Fu);field_manager_layout_write_raw(1u,0x09u,0x06u,0,0x90u);}break;
    case 0x3Au: if(ps4_event_flag_test(0x65u))field_manager_clear_secondary(9u);else{field_manager_clear_secondary(4u);field_manager_clear_secondary(7u);}break;
    case 0x3Bu: if(ps4_flag_test_type(2u,0x14u)){(void)kos_decode(0x1C8F7Au,&work_ram[0xA000u],0x1000u);(void)kos_decode(0x1C92BAu,&work_ram[0xB000u],0x1000u);}break;
    case 0x3Cu: if(ps4_flag_test_type(2u,0x16u)){(void)kos_decode(0x1CA2AEu,&work_ram[0xB000u],0x1000u);(void)kos_decode(0x1CA54Eu,&work_ram[0xA000u],0x1000u);}break;
    case 0x3Du: ps4_flag_clear_type(2u,0x15u);if(ram16r(0xC030u)>=0x03C0u&&ram16r(0xC034u)>=0x03C0u)ps4_flag_set_type(2u,0x15u);break;
    case 0x3Eu: ps4_flag_clear_type(2u,0x17u);if(ram16r(0xC030u)>=0x03E0u&&ram16r(0xC034u)<=0x0180u)ps4_flag_set_type(2u,0x17u);break;
    /* $3F ($529FC) calls the stateful palette routine $5598A before forcing
     * chunk priority; keep it in the explicit TODO set rather than half-run it. */
    case 0x3Fu: /* $529FC: source per-line sine scroll + chunk priority bit. */
        field_source_hscroll_update26();
        for(unsigned i=0u;i<32u;i++){uint16_t a=(uint16_t)(0x64E0u+i*2u);ram16w(a,(uint16_t)(ram16r(a)|0x8000u));}
        break;
    case 0x40u: field_manager_copy_palette(3u,0x52A42u,7u);field_palette_live_word(12u,0x0068u);field_palette_live_word(13u,0x0680u);break;
    case 0x41u: field_manager_copy_palette(17u,0x52A66u,9u);break;
    case 0x42u: if(ps4_event_flag_test(0x64u))for(unsigned i=4u;i<12u;i++)field_manager_clear_secondary(i);break;
    case 0x43u: if(ps4_event_flag_test(0x70u))field_manager_clear_secondary(0u);break;
    case 0x44u: if(ps4_event_flag_test(0x88u))field_manager_clear_secondary(0u);if(ps4_event_flag_test(0x85u)){field_manager_layout_write_raw(1u,0x2Fu,0x09u,0,0x56u);field_manager_layout_write_raw(1u,0x2Fu,0x09u,1,0x57u);field_manager_layout_write_raw(1u,0x2Fu,0x09u,0x40,0x5Eu);field_manager_layout_write_raw(1u,0x2Fu,0x09u,0x41,0x5Fu);field_palette_live_word(1u,0u);}break;
    case 0x45u: if(ps4_event_flag_test(0x69u))field_manager_clear_secondary(0u);break;
    case 0x46u: ram8w(0xECF4u,1u);break;
    case 0x47u: ps4_flag_clear_type(2u,0x19u);break;
    case 0x48u: if(!ps4_event_flag_test(0x13u)){field_manager_layout_write_raw(1u,0x0Fu,0x0Eu,0,0x0Eu);field_manager_layout_write_raw(1u,0x0Fu,0x0Eu,1,0x0Fu);field_manager_layout_write_raw(1u,0x0Fu,0x0Eu,0x20,0x16u);field_manager_layout_write_raw(1u,0x0Fu,0x0Eu,0x21,0x17u);}break;
    case 0x49u: if(ps4_event_flag_test(0x83u))field_manager_clear_secondary(0u);break;
    case 0x4Au: if(ps4_event_flag_test(0x90u)){field_manager_set_type(0u,0x022Cu);field_manager_set_position(0u,0x0100u,0x0150u);field_manager_set_dialogue(0u,0x1Du);field_manager_set_type(1u,0x022Cu);field_manager_set_position(1u,0x0110u,0x0150u);field_manager_set_dialogue(1u,0x1Du);}break;
    case 0x4Bu: if(!ps4_event_flag_test(0x9Du))field_manager_clear_secondary(12u);break;
    case 0x4Cu: if(ps4_event_flag_test(0xA1u))field_manager_clear_secondary(2u);break;
    case 0x4Du: if(ps4_event_flag_test(0x9Eu)){field_manager_clear_secondary(2u);field_manager_clear_secondary(3u);field_manager_clear_secondary(4u);}break;
    case 0x4Eu: /* R16 fix (oracle): MapDataMan_MeeseClinicKyra hides Kyra if Reunion($DA) || !SnowstormGone($A1). */
        if(ps4_event_flag_test(0xDAu)||!ps4_event_flag_test(0xA1u))field_manager_clear_secondary(3u);break;
    case 0x4Fu: if(!ps4_event_flag_test(0x9Eu))for(unsigned i=25u;i<=30u;i++)field_manager_clear_secondary(i);break;
    case 0x50u: if(ps4_event_flag_test(0x9Eu))field_manager_clear_secondary(1u);break;
    case 0x51u: if(ps4_event_flag_test(0x9Eu))field_manager_clear_secondary(0u);break;
    case 0x52u: if(ps4_event_flag_test(0x9Eu)){field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);}break;
    case 0x53u: if(!ps4_event_flag_test(0x9Eu)){field_manager_clear_secondary(3u);field_manager_clear_secondary(4u);field_manager_clear_secondary(5u);}break;
    case 0x54u: if(ps4_event_flag_test(0x96u)){field_manager_set_type(3u,0x0104u);if(3u<field_obj_count)field_manager_set_position(3u,0x02E0u,field_objs[3u].y);else ram16w(field_manager_obj_addr(3u,0x30u),0x02E0u);field_manager_set_type(4u,0x0104u);if(4u<field_obj_count)field_manager_set_position(4u,0x0310u,field_objs[4u].y);else ram16w(field_manager_obj_addr(4u,0x30u),0x0310u);}break;
    case 0x55u:{int d9=ps4_event_flag_test(0xD9u),d8=ps4_event_flag_test(0xD8u);if(d9||!d8){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);}else{field_manager_layout_write_raw(1u,0x0Fu,0x0Bu,-0x1000,0x5Cu);field_manager_layout_write_raw(1u,0x0Fu,0x0Bu,0x20,0x07u);}break;}
    case 0x56u: if(ps4_event_flag_test(0xD7u)||!ps4_event_flag_test(0x97u))field_manager_set_dialogue(0u,0x26u);else field_manager_clear_secondary(0u);break;
    case 0x57u: /* R16 fix (oracle): MapDataMan_InnerSanctuaryB1Priest hides the priest if LeRoofStory2($D7) || !LutzRevelation($97). */
        if(ps4_event_flag_test(0xD7u)||!ps4_event_flag_test(0x97u))field_manager_clear_secondary(3u);break;
    /* $58 ($52D78) drives ECFF through $55D1E and issues VDP command $8B03. */
    case 0x58u: /* $52D78: initialize per-line wave scroll. */
        ram8w(0xECFFu,0u);field_source_hscroll_update31();break;
    case 0x59u: for(unsigned i=0u;i<32u;i++){uint16_t a=(uint16_t)(0x64E0u+i*2u);ram16w(a,(uint16_t)(ram16r(a)|0x8000u));}break;
    case 0x5Au: if(ps4_event_flag_test(0x9Eu))field_manager_set_type(24u,0x003Cu);break;
    case 0x5Bu: if(!ps4_event_flag_test(0x2Fu))field_manager_copy_palette(49u,0x52DDEu,7u);break;
    case 0x5Cu: if(ps4_event_flag_test(0x93u)||ps4_event_flag_test(0x9Eu))field_manager_clear_secondary(0u);break;
    case 0x5Du: if(ps4_event_flag_test(0x2Fu)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);field_manager_clear_secondary(3u);}break;
    case 0x5Eu: if(ps4_event_flag_test(0x65u)||ps4_event_flag_test(0x60u))for(unsigned i=1u;i<=5u;i++)field_manager_clear_secondary(i);break;
    case 0x5Fu: if(ps4_event_flag_test(0x93u))field_manager_clear_secondary(0u);break;
    case 0x60u: if(ps4_event_flag_test(0x94u))field_manager_set_dialogue(2u,0x59u);break;
    case 0x61u: if(ps4_event_flag_test(0x9Eu))field_manager_clear_secondary(0u);break;
    case 0x62u: if(ps4_event_flag_test(0x98u)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);}break;
    case 0x63u: if(ps4_event_flag_test(0x9Au))for(unsigned i=0u;i<=5u;i++)field_manager_clear_secondary(i);break;
    case 0x64u: if(ps4_event_flag_test(0xE2u)){field_manager_clear_secondary(5u);field_manager_clear_secondary(6u);}if(ps4_event_flag_test(0xE5u))for(unsigned i=0u;i<=4u;i++)field_manager_clear_secondary(i);if(!ps4_event_flag_test(0xD4u))for(unsigned i=7u;i<=9u;i++)field_manager_clear_secondary(i);break;
    case 0x65u: if(ps4_event_flag_test(0xE3u)){field_manager_clear_secondary(5u);field_manager_clear_secondary(6u);}if(ps4_event_flag_test(0xE6u))for(unsigned i=0u;i<=4u;i++)field_manager_clear_secondary(i);if(!ps4_event_flag_test(0xD2u)){field_manager_clear_secondary(7u);field_manager_clear_secondary(8u);}break;
    case 0x66u: if(!ps4_event_flag_test(0x35u))for(unsigned i=7u;i<=9u;i++)field_manager_clear_secondary(i);break;
    case 0x67u: if(!ps4_event_flag_test(0xD9u)||ps4_event_flag_test(0xDAu))for(unsigned i=0u;i<=4u;i++)field_manager_clear_secondary(i);if(!ps4_event_flag_test(0xDAu))for(unsigned i=5u;i<=9u;i++)field_manager_clear_secondary(i);if(ps4_event_flag_test(0xDCu))field_manager_clear_secondary(5u);if(ps4_event_flag_test(0xDBu))field_manager_clear_secondary(6u);if(ps4_event_flag_test(0xDDu))field_manager_clear_secondary(7u);if(ps4_event_flag_test(0xDEu))field_manager_clear_secondary(8u);if(ps4_event_flag_test(0xDFu))field_manager_clear_secondary(9u);break;
    case 0x68u: if(ps4_event_flag_test(0xDAu)||!ps4_event_flag_test(0xD9u)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);}break;
    case 0x69u: if(ps4_event_flag_test(0xE4u))field_manager_clear_secondary(0u);break;
    case 0x6Au: if(ps4_event_flag_test(0x42u)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);}break;
    case 0x6Bu: if(ps4_event_flag_test(0x31u)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);field_manager_clear_secondary(2u);}break;
    case 0x6Cu: /* $530E0: if EventFlag $9E is clear, palette=( *(ECD2) + $60 ). */
        if(!ps4_event_flag_test(0x9Eu))field_palette_copy_rom_words(0u,field_map.palette+0x60u,32u);
        break;
    case 0x6Du: field_manager_layout_write_raw(0u,0x09u,0x06u,0,0xB5u);break;
    case 0x6Eu: if(ps4_event_flag_test(0x9Eu)){field_manager_layout_write_raw(1u,0x15u,0x0Du,0,0x18u);field_manager_layout_write_raw(1u,0x15u,0x0Du,1,0x19u);}break;
    case 0x6Fu: if(ps4_event_flag_test(0xDAu)){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);}else{field_manager_clear_secondary(2u);field_manager_clear_secondary(3u);}break;
    case 0x70u: if(ps4_event_flag_test(0x1Au)){field_manager_set_type(0u,0x0038u);field_manager_set_position(0u,0x0110u,0x02D0u);field_manager_clear_secondary(8u);field_manager_clear_secondary(9u);}break;
    case 0x71u: if(ps4_event_flag_test(0x19u))field_manager_clear_secondary(10u);break;
    case 0x72u: if(ps4_event_flag_test(0x24u)||ps4_event_flag_test(0x23u)||ps4_event_flag_test(0x22u)||(!ps4_event_flag_test(0x21u)&&(ps4_event_flag_test(0x20u)||ps4_event_flag_test(0x1Fu))))field_manager_clear_secondary(11u);break;
    case 0x73u: if(ps4_event_flag_test(0x24u)||ps4_event_flag_test(0x23u)||(!ps4_event_flag_test(0x22u)&&(ps4_event_flag_test(0x21u)||ps4_event_flag_test(0x20u)||!ps4_event_flag_test(0x1Fu))))field_manager_clear_secondary(7u);break;
    case 0x74u: if(ps4_event_flag_test(0x24u)||(!ps4_event_flag_test(0x23u)&&(ps4_event_flag_test(0x22u)||ps4_event_flag_test(0x21u)||!ps4_event_flag_test(0x20u))))field_manager_clear_secondary(9u);break;
    case 0x75u: /* R16 fix (oracle): RockyHome($24) clear -> hide Rocky (slot 3); set -> slot 2 becomes FieldObj_Tinkerbell ($330). R15 was inverted. */
        if(!ps4_event_flag_test(0x24u))field_manager_clear_secondary(3u);else field_manager_set_type(2u,0x0330u);break;
    case 0x76u: if(ps4_event_flag_test(0x2Au)||!ps4_event_flag_test(0x28u))for(unsigned i=1u;i<=4u;i++)field_manager_clear_secondary(i);break;
    case 0x77u: if(!ps4_event_flag_test(0x2Fu)&&ps4_event_flag_test(0x65u))field_manager_clear_secondary(1u);break;
    case 0x78u: if(!ps4_event_flag_test(0x2Fu)&&ps4_event_flag_test(0x2Du))field_manager_layout_write_raw(1u,0x15u,0x12u,0,0x01u);break;
    case 0x79u: if(!ps4_event_flag_test(0x7Du)){field_manager_clear_secondary(2u);field_manager_clear_secondary(3u);}break;
    case 0x7Au: if(ps4_event_flag_test(0xBAu))field_manager_clear_secondary(0u);break;
    case 0x7Bu: if(ps4_event_flag_test(0xB3u))for(unsigned i=1u;i<=5u;i++)field_manager_clear_secondary(i);break;
    case 0x7Cu: if(ps4_event_flag_test(0xB6u))for(unsigned i=2u;i<=7u;i++)field_manager_clear_secondary(i);break;
    case 0x7Du: /* R16 fix (oracle): AlisSword($AA) clear -> hide running boy (slot 5); set -> mom moves, bed boy cleared. R15 was inverted. */
        if(!ps4_event_flag_test(0xAAu))field_manager_clear_secondary(5u);else{field_manager_set_type(1u,0x0038u);field_manager_set_position(1u,0x01B0u,0x01A0u);field_manager_clear_secondary(2u);field_manager_clear_secondary(3u);field_manager_clear_secondary(4u);}break;
    case 0x7Eu: if(ps4_event_flag_test(0xDAu)||!ps4_event_flag_test(0x94u))for(unsigned i=4u;i<=6u;i++)field_manager_clear_secondary(i);break;
    case 0x7Fu: if(ps4_event_flag_test(0x28u))field_manager_clear_secondary(4u);break;
    case 0x80u:{int ba=ps4_event_flag_test(0xBAu),af=ps4_event_flag_test(0xAFu);if(ba)field_manager_clear_secondary(2u);if(ba||af){field_manager_clear_secondary(0u);field_manager_clear_secondary(1u);uint16_t a=field_manager_obj_addr(2u,2u);ram8w(a,(uint8_t)(ram8r(a)&~0x02u));}break;}
    case 0x81u:{static const uint8_t xy[6]={0x0Fu,0x0Bu,0x12u,0x15u,0x14u,0x10u};for(unsigned i=0u;i<3u;i++){field_manager_layout_write_raw(1u,xy[i*2u],xy[i*2u+1u],0,0x55u);field_manager_layout_write_raw(1u,xy[i*2u],xy[i*2u+1u],1,0x56u);}break;}
    case 0x82u: if(!ps4_flag_test_type(2u,0x1Cu))for(unsigned i=0u;i<=3u;i++)field_manager_clear_secondary(i);if(!ps4_flag_test_type(2u,0x1Du))for(unsigned i=4u;i<=8u;i++)field_manager_clear_secondary(i);break;
    case 0x83u: ram8w(0xB706u,6u);ram8w(0xB709u,6u);break;
    case 0x84u: ps4_flag_clear_type(2u,0x08u);break;
    default: break;
    }
}
#include "r30_palette_updates.h"
#include "r30_machinery.h"
static void r30_tile_animation_load(uint32_t treasure);
static void r30_vehicle_palette(unsigned vehicle);
static void r30_tile_animation_update(void);
static void field_hscroll_present_current(void);
static void field_apply_map_managers_load(void){
    /* PSC-style generated per-map continuation plan: every retail manager ID is
     * compiled ahead of time in authored order, so load no longer walks a
     * Genesis $FFFF sentinel list.  The exact existing dispatcher remains the
     * semantic authority. */
    const Ps4PscRuntimePlan* plan=ps4_psc_runtime_plan_get(field_map.map_id);
    if(plan){for(unsigned i=0u;i<plan->manager_count;i++)field_apply_map_manager_load(ps4_psc_manager_ids[plan->manager_offset+i]);return;}
    uint32_t p=field_map.data_manager;for(unsigned guard=0u;guard<64u&&p+1u<0x300000u;guard++){unsigned id=rom16(p);if(id==0xFFFFu)break;field_apply_map_manager_load(id);p+=2u;}
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_apply_map_update_id(unsigned id);
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_run_map_updates(void){
    /* Compile the per-map byte list to only updates that have native semantics.
     * The active continuation pointer/count are latched by map load, avoiding a
     * generated-table lookup on every field frame. */
    if(field_runtime_plan_valid){
        for(unsigned i=0u;i<field_update_plan_count;i++)field_apply_map_update_id(field_update_plan_ids[i]);
    }else{
        uint32_t p=field_map.map_updates;for(unsigned guard=0u;guard<64u;guard++){uint8_t id=rom8(p++);if(id==0xFFu)break;if(id)field_apply_map_update_id(id);}
    }
}

__attribute__((noinline))
static void field_apply_map_update_id(unsigned id){
    switch(id){
        case 1u: /* Motavia water: 16-frame cadence, four interleaved phases. */
            if((native_main_frame_count&15u)==0u)field_palette_cycle4(25u,0x54AB0u,0x30u,4u);break;
        case 2u: /* Motavia town water */
            if((native_main_frame_count&7u)==0u)field_palette_cycle4(26u,0x54B0Cu,0x18u,3u);break;
        case 3u: /* Piata B2 monsters */
            if((native_main_frame_count&15u)==0u){unsigned q=(native_main_frame_count>>4)&3u;field_palette_live_word(25u,rom16(0x54B62u+q*2u));field_palette_live_word(26u,rom16(0x54B62u+8u+q*2u));field_palette_live_word(27u,rom16(0x54B62u+16u+q*2u));}break;
        case 4u: /* Motavia quicksand */
            if((native_main_frame_count&7u)==0u)field_palette_cycle4(10u,0x54BB4u,0x18u,3u);break;
        case 8u: /* cave water */
            if((native_main_frame_count&7u)==0u)field_palette_cycle4(10u,0x54CBAu,0x18u,3u);break;
        case 9u: /* Soldiers' Temple water */
            if((native_main_frame_count&7u)==0u)field_palette_cycle4(26u,0x54D16u,0x18u,3u);break;
        case 0x0Au: /* Valley Maze water */
            if((native_main_frame_count&7u)==0u)field_palette_cycle4(10u,0x54D72u,0x18u,3u);break;
        case 0x0Bu: /* Strip Club lights uses the source ECF2 duration/index state. */
            {uint8_t dur=(uint8_t)(ram8r(0xECF2u)+1u);ram8w(0xECF2u,dur);if((dur&7u)==0u){uint8_t q=ram8r(0xECF3u);if(q>=6u)q=0u;field_palette_live_word(26u,rom16(0x54DD4u+(uint32_t)q*2u));ram8w(0xECF3u,(uint8_t)(q+1u));}}break;
        case 0x0Cu: /* Dezolis snowstorm */
            if((native_main_frame_count&7u)==0u)field_palette_cycle4(19u,0x54E1Au,0x18u,3u);break;
        case 0x0Eu: /* Bio Plant dual palette cycles. */
            if((native_main_frame_count&7u)==0u){uint8_t a=ram8r(0xECF2u),b=ram8r(0xECF3u);if(a>=6u)a=0u;if(b>=8u)b=0u;field_palette_cycle_pair(12u,0x54EB8u,a,0x0Cu);field_palette_cycle_pair(28u,0x54ED0u,b,0x10u);ram8w(0xECF2u,(uint8_t)(a+1u));ram8w(0xECF3u,(uint8_t)(b+1u));}break;
        case 0x13u: /* Wreckage line 1 */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF2u);if(q>=6u)q=0u;field_palette_cycle_pair(12u,0x55258u,q,0x0Cu);ram8w(0xECF2u,(uint8_t)(q+1u));}break;
        case 0x14u: /* Wreckage line 2 */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF3u);if(q>=8u)q=0u;field_palette_cycle_pair(28u,0x552ACu,q,0x0Cu);ram8w(0xECF3u,(uint8_t)(q+1u));}break;
        case 0x15u: /* Machine Center */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF2u);if(q>=6u)q=0u;field_palette_cycle_pair(12u,0x55308u,q,0x0Cu);ram8w(0xECF2u,(uint8_t)(q+1u));}break;
        case 0x1Au: /* Climate Center: two paired cycles from one 4-line table. */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF2u);if(q>=6u)q=0u;field_palette_cycle_pair(28u,0x55550u,q,0x0Cu);field_palette_cycle_pair(12u,0x55550u+0x18u,q,0x0Cu);ram8w(0xECF2u,(uint8_t)(q+1u));}break;
        case 0x1Bu: /* Zelan F1 */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF3u);if(q>=8u)q=0u;field_palette_cycle_pair(28u,0x555BCu,q,0x0Cu);ram8w(0xECF3u,(uint8_t)(q+1u));}break;
        case 0x1Eu: /* Air Castle line 2, first pair. */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF3u);if(q>=14u)q=0u;field_palette_live_word(26u,rom16(0x556DCu+(uint32_t)q*2u));field_palette_live_word(28u,rom16(0x556DCu+0x1Cu+(uint32_t)q*2u));ram8w(0xECF3u,(uint8_t)(q+1u));}break;
        case 0x1Fu: /* Air Castle line 2, second pair; source derives 16 phases from frame count. */
            if((native_main_frame_count&7u)==0u){unsigned q=(native_main_frame_count>>3)&15u;field_palette_live_word(27u,rom16(0x5573Eu+q*2u));field_palette_live_word(29u,rom16(0x5573Eu+0x20u+q*2u));}break;
        case 0x21u: /* Kuran F2 */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF2u);if(q>=6u)q=0u;field_palette_cycle_pair(28u,0x5581Au,q,0x0Cu);ram8w(0xECF2u,(uint8_t)(q+1u));}break;
        case 0x06u: field_source_hscroll_update06();break;
        case 0x26u: field_source_hscroll_update26();break;
        case 0x2Eu: field_source_palette_update2e();break;
        case 0x2Fu: field_source_palette_update2f();break;
        case 0x30u: field_source_palette_update30();break;
        case 0x31u: field_source_hscroll_update31();break;
        case 0x2Bu: /* Zelan/Kuran space */
            if((native_main_frame_count&7u)==0u){uint8_t q=ram8r(0xECF2u);if(q>=6u)q=0u;field_palette_cycle_pair(12u,0x55BBCu,q,0x0Cu);ram8w(0xECF2u,(uint8_t)(q+1u));}break;
        case 0x37u: /* MapUpdate_LadeaTwAirCstlMusic */
            ram16w(0xEC2Au,0xFFFFu);ram8w(0xECECu,0xA7u);break;
        case 0x38u: /* Source unreferenced ChestFlag_Unused clear. */
            ps4_flag_clear_type(1u,0xA9u);break;
        case 0x39u: /* Dark Force 2 arena: encounters only below source Y=$160. */
            ram8w(0xECE7u,(uint8_t)(chaz.y>0x0160));break;
        case 0x11u:case 0x12u:r30_conveyor_update(id);break;
        case 0x17u:case 0x18u:case 0x1Cu:case 0x1Du:case 0x20u:case 0x23u:case 0x24u:case 0x27u:case 0x28u:case 0x29u:case 0x2Au:case 0x2Cu:case 0x35u:case 0x36u:r30_palette_update(id);break;
        case 0x2Du:if(ps4_flag_test_type(1u,0x0Bu))ps4_event_flag_set(0x72u);break;
        case 0x33u:if(ps4_flag_test_type(1u,0xA1u)&&ps4_flag_test_type(1u,0xA2u)&&ps4_flag_test_type(1u,0xA3u))ps4_event_flag_set(0xD5u);break;
        case 0x34u:if(ps4_flag_test_type(1u,0xA4u)&&ps4_flag_test_type(1u,0xA5u))ps4_event_flag_set(0xD3u);break;
        default:
            /* Movement, conveyor, flag, object and more complex palette routines
             * remain explicit TODO entries; never substitute a visual guess. */
            break;
        }
}

static void r31_actor_load(void);
static int ps4_load_field_map(uint16_t map_id){
    struct Ps4FieldMapDesc d; if(!ps4_parse_field_map(map_id,&d))return 60;
    field_manager_party_position_pending=0u;REG_DMA0CNT=0u;field_hscroll_mode=0u;field_hscroll_dirty=0u;field_hscroll_prepared=0u;field_hblank_active_index=0u;
    /* Keep this copy field-by-field.  Clang can lower a whole-struct assignment
     * here to __aeabi_memcpy4, which this freestanding ARM7TDMI runtime does not
     * link.  These are also the exact descriptor fields consumed by the shared
     * map/collision backend. */
    field_map.map_id=d.map_id;
    field_map.general=d.general;field_map.music=d.music;field_map.is_world=d.is_world;
    field_map.row_fg=d.row_fg;field_map.col_fg=d.col_fg;field_map.row_bg=d.row_bg;field_map.col_bg=d.col_bg;field_map.collision_plane=d.collision_plane;
    field_map.kos_tiles=d.kos_tiles;field_map.nem_tiles=d.nem_tiles;field_map.kos_ram=d.kos_ram;field_map.chunks=d.chunks;
    field_map.map_updates=d.map_updates;field_map.trans1=d.trans1;field_map.trans2=d.trans2;field_map.objects=d.objects;field_map.treasure=d.treasure;
    field_map.layout_fg=d.layout_fg;field_map.layout_bg=d.layout_bg;field_map.data_manager=d.data_manager;field_map.dialogue_tree=d.dialogue_tree;
    field_map.interactions=d.interactions;field_map.events=d.events;field_map.palette=d.palette;
    /* The four authored map-policy bytes follow the palette pointer. Source
     * RefreshMap reloads them even when the palette/music remain unchanged. */
    {uint32_t policy=d.data_manager-4u;
     ram8w(0xECE6u,rom8(policy));ram8w(0xECE7u,rom8(policy+1u));
     ram8w(0xED50u,rom8(policy+2u));uint8_t escape=rom8(policy+3u);
     if(!(escape&0x80u))ram8w(0xED51u,escape);}
    {const Ps4PscRuntimePlan* plan=ps4_psc_runtime_plan_get(map_id);field_runtime_plan_valid=(uint8_t)(plan!=0);
     field_update_plan_count=plan?plan->update_count:0u;field_update_plan_ids=plan?(ps4_psc_update_ids+plan->update_offset):0;
     field_event_plan_count=plan?plan->event_count:0u;field_event_plan_ids=plan?(ps4_psc_event_ids+plan->event_offset):0;
     field_trans1_plan_count=plan?plan->trans1_count:0u;field_trans1_plan=plan?(ps4_psc_transitions+plan->trans1_offset):0;
     field_trans2_plan_count=plan?plan->trans2_count:0u;field_trans2_plan=plan?(ps4_psc_transitions+plan->trans2_offset):0;}
    /* Source map load stores the active GetMapLayoutChunk plane selector in
     * Map_Collision_Plane ($EC24). Keep virtual source RAM synchronized with
     * the native parsed map descriptor so generated event choreography can
     * share the same state as collision/elevator/platform code. */
    ram8w(0xEC24u,d.collision_plane);
    ram16w(0xECF2u,0u); /* source map load clears both palette/update phase bytes */
    dma_fill32(md_vram,65536u,0u);field_gba_ready_reset();
    uint32_t p=d.kos_tiles;
    while(rom16(p)<0x8000u){uint16_t tile=rom16(p);if(!load_kos_vram_auto(rom32(p+2u),tile))return 61;p+=6u;}
    p=d.nem_tiles;
    for(;;){
        uint16_t tile=rom16(p); if(tile==0xFFFFu)break;
        if(tile==0xFFFEu){
            uint16_t dest=rom16(p+2u);uint32_t src=rom32(p+4u);unsigned off=((unsigned)dest*32u)&0xFFFFu;
            if(!nemesis_fast(src,&work_ram[off],65536u-off))return 62;p+=8u;continue;
        }
        if(!load_nem_vram_auto(rom32(p+2u),tile))return 63;p+=6u;
    }
    p=d.kos_ram;
    while(rom16(p)!=0xFFFFu){
        uint16_t dest=rom16(p);unsigned n=kos_decode(rom32(p+2u),&work_ram[dest],65536u-dest);
        if(!n)return 64;p+=6u;
    }
    unsigned chunk_off=0x6000u;p=d.chunks;
    while(rom16(p)!=0xFFFFu){
        unsigned n=kos_decode(rom32(p),&work_ram[chunk_off],0xA000u-chunk_off);if(!n)return 65;
        chunk_off+=n;p+=4u;
    }
    unsigned fg_cells=((unsigned)d.row_fg+1u)*((unsigned)d.col_fg+1u);
    unsigned bg_cells=((unsigned)d.row_bg+1u)*((unsigned)d.col_bg+1u);
    if(!d.is_world){
        if(fg_cells>0x1000u||bg_cells>0x1000u)return 66;
        unsigned nf=kos_decode(d.layout_fg,&work_ram[0xA000u],0x1000u);if(nf<fg_cells)return 67;
        unsigned nb=kos_decode(d.layout_bg,&work_ram[0xB000u],0x1000u);if(nb<bg_cells)return 68;
    }
    for(unsigned i=0;i<64u;i++)cram[i]=0u;
    for(unsigned i=0;i<32u;i++)cram[i]=rom16(d.palette+i*2u);
    for(unsigned i=0;i<16u;i++)cram[32u+i]=rom16(0x296300u+i*2u);
    for(unsigned i=0;i<16u;i++)cram[48u+i]=rom16(d.palette+0x40u+i*2u);
    r30_vehicle_palette(ram16r(0xF43Cu));field_palette_shadow_rebuild(64u);

    field_obj_count=0u;unsigned obj_tile=FIELD_OBJ_TILE_BASE+8u;p=d.objects;
    while(rom16(p)!=0xFFFFu&&field_obj_count<FIELD_MAX_OBJECTS){
        struct FieldObjLite* o=&field_objs[field_obj_count++];
        o->type=rom16(p);o->facing=rom8(p+2u);o->dialog=rom8(p+3u);o->base_tile=rom16(p+4u);
        o->treasure_type=o->treasure_flag=o->treasure_item=0u;
        o->x=(uint16_t)(rom16(p+6u)*8u);o->y=(uint16_t)(rom16(p+8u)*8u);o->active=1u;o->blocks=field_obj_source_blocks(o->type);field_obj_init_motion(o);
        obj_tile+=field_obj_configure_render(o,obj_tile);if(obj_tile>DIALOGUE_OBJ_TILE_BASE)return 74;
        p+=10u;
    }
    if(rom16(p)!=0xFFFFu)return 69;

    /* LoadTreasureChests ($5373E): treasure records are six bytes and are
     * distinct from ordinary 10-byte field objects.  The original loader
     * installs the common chest Nemesis art at tiles $4DC/$4E6, selects a
     * special normal-chest stream for Map_General_Var $32, then creates
     * FieldObj_TreasureChest ($A0) / FieldObj_WhiteTreasureChest ($1D4). */
    p=d.treasure;
    if(rom16(p)!=0xFFFFu){
        if(!load_nem_vram_auto(d.general==0x32u?0x2976FAu:0x2975AEu,0x04DCu))return 75;
        if(!load_nem_vram_auto(0x297674u,0x04E6u))return 76;
    }
    while(rom16(p)!=0xFFFFu&&field_obj_count<FIELD_MAX_OBJECTS){
        struct FieldObjLite* o=&field_objs[field_obj_count++];
        unsigned white=rom8(p)!=0u;
        o->type=(uint16_t)(white?0x01D4u:0x00A0u);o->facing=(uint8_t)(ps4_flag_test_type(1u,rom8(p+2u))?4u:0u);o->dialog=0u;
        o->base_tile=(uint16_t)(white?0x04E6u:0x04DCu);o->treasure_type=rom8(p+1u);o->treasure_flag=rom8(p+2u);o->treasure_item=rom8(p+3u);
        o->x=(uint16_t)((unsigned)rom8(p+4u)*16u);o->y=(uint16_t)((unsigned)rom8(p+5u)*16u);o->active=1u;o->blocks=field_obj_source_blocks(o->type);field_obj_init_motion(o);
        obj_tile+=field_obj_configure_render(o,obj_tile);if(obj_tile>DIALOGUE_OBJ_TILE_BASE)return 74;
        p+=6u;
    }
    if(rom16(p)!=0xFFFFu)return 77;
    r30_tile_animation_load(d.treasure);
    field_apply_map_managers_load();
    r31_actor_load();
    field_collision_nibbles_rebuild();
    field_dialogue_len=kos_decode(d.dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);
    if(!field_dialogue_len)return 70;
    if(!field_build_tile_remap())return 72;
    return 0;
}

__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int field_obj_reserves(const struct FieldObjLite* o,int x,int y){
    /* CP21: stationary objects own only their current endpoint.  Handle that
     * dominant case before deriving a moving destination/departure pair. */
    int tx=(int)o->x,ty=(int)o->y;if(!o->move_left)return tx==x&&ty==y;
    int dx=0,dy=0;
    if(o->move_dir==1u){ty-=(int)o->move_left;dy=-16;}
    else if(o->move_dir==2u){ty+=(int)o->move_left;dy=16;}
    else if(o->move_dir==3u){tx-=(int)o->move_left;dx=-16;}
    else if(o->move_dir==4u){tx+=(int)o->move_left;dx=16;}
    if(tx==x&&ty==y)return 1;
    return tx-dx==x&&ty-dy==y;
}
static int player_reserves(int x,int y){
    int tx=chaz.x,ty=chaz.y;if(chaz.step_left){tx+=chaz.dx*(int)chaz.step_left;ty+=chaz.dy*(int)chaz.step_left;}
    if(tx==x&&ty==y)return 1;
    return chaz.step_left&&tx-chaz.dx*16==x&&ty-chaz.dy*16==y;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int follower_reserves(const struct PartyFollowerLite* f,int x,int y){
    if(!f->active)return 0;if(f->tx==x&&f->ty==y)return 1;if(!f->moving)return f->x==x&&f->y==y;
    int dx=f->x<f->tx?-16:(f->x>f->tx?16:0),dy=f->y<f->ty?-16:(f->y>f->ty?16:0);
    return f->tx+dx==x&&f->ty+dy==y;
}
static int field_object_blocks(int x,int y){
    /* Source FieldObj_DoObjCollision is disabled on Motavia/Dezolis world maps. */
    if((field_map.map_id&0xFFFEu)==0u)return 0;
    /* A moving source object owns both endpoints for its entire 16-pixel step.
     * This closes the old source-tile hole without comparing fractional pixels. */
    for(unsigned i=0;i<field_obj_count;i++)if(field_objs[i].active&&field_objs[i].blocks&&field_obj_reserves(&field_objs[i],x,y))return 1;
    return 0;
}
static int field_follower_blocks_lead(int x,int y){
    /* In the original, party members occupy Field_Objects_Memory slots and
     * therefore participate in FieldObj_DoObjCollision during normal play.
     * The native split follower array must preserve that shared occupancy or
     * Character_1 can reverse into Character_2 and overlap it.  Event mode
     * deliberately bypasses this through its separate choreography helpers,
     * matching FieldObj_Move's source event-mode collision bypass. */
    if((field_map.map_id&0xFFFEu)==0u)return 0;
    for(unsigned i=0u;i<field_follower_count;i++)if(follower_reserves(&field_followers[i],x,y))return 1;
    return 0;
}

static unsigned sprite_frame_offset(uint8_t facing,unsigned anim){
    static const uint8_t seq[4]={0,8,0,16};unsigned d=0;
    if(facing==1u)d=0x30u;else if(facing==2u||facing==3u)d=0x18u;
    return d+(unsigned)seq[anim&3u];
}
static unsigned sprite_art_px_rom(uint32_t art,unsigned frame,unsigned x,unsigned y,int hflip){
    if(hflip)x=15u-x;unsigned col=x>>3,row=y>>3,tile=frame+col*4u+row;
    uint8_t v=rom8(art+tile*32u+(y&7u)*4u+((x&7u)>>1));return (x&1u)?(v&15u):(v>>4);
}
static unsigned sprite_art_px_vram(unsigned base,unsigned frame,unsigned x,unsigned y,int hflip){
    if(hflip)x=15u-x;unsigned col=x>>3,row=y>>3,tile=base+frame+col*4u+row;
    uint8_t v=md_vram[(tile*32u+(y&7u)*4u+((x&7u)>>1))&0xFFFFu];return (x&1u)?(v&15u):(v>>4);
}
static void draw_sprite_rom(uint32_t art,unsigned frame,int sx,int sy,unsigned pal_line,int hflip){
    for(unsigned y=0;y<32u;y++){int py=sy+(int)y;if(py<0||py>=160)continue;
        for(unsigned x=0;x<16u;x++){int px=sx+(int)x;if(px<0||px>=240)continue;unsigned ci=sprite_art_px_rom(art,frame,x,y,hflip);if(ci)VRAM16[(unsigned)py*240u+(unsigned)px]=md2gba(cram[pal_line*16u+ci]);}}
}
static void draw_sprite_vram(unsigned base,unsigned frame,int sx,int sy,unsigned pal_line,int hflip){
    for(unsigned y=0;y<32u;y++){int py=sy+(int)y;if(py<0||py>=160)continue;
        for(unsigned x=0;x<16u;x++){int px=sx+(int)x;if(px<0||px>=240)continue;unsigned ci=sprite_art_px_vram(base,frame,x,y,hflip);if(ci)VRAM16[(unsigned)py*240u+(unsigned)px]=md2gba(cram[pal_line*16u+ci]);}}
}
static uint32_t char_field_art_address(unsigned id){
    /* Chaz..Seth are eleven consecutive $900-byte source field sheets. */
    return id<=10u?0x290000u+(uint32_t)id*0x900u:0x290000u;
}
static uint32_t lead_art_address(void){return char_field_art_address(lead_char_id);}

#define BG0_MAP ((volatile uint16_t*)0x06008000)
#define BG1_MAP ((volatile uint16_t*)0x06008800)
#define BG2_MAP ((volatile uint16_t*)0x06009000)
#define BG3_MAP ((volatile uint16_t*)0x06009800)
/* Special source line-scroll maps use 64x32 text backgrounds.  Each layer then
 * consumes two screen blocks, so keep the four priority layers disjoint. */
#define BG0_MAP64 ((volatile uint16_t*)0x06008000) /* blocks 16-17, plane A high */
#define BG1_MAP64 ((volatile uint16_t*)0x06009000) /* blocks 18-19, plane B high */
#define BG2_MAP64 ((volatile uint16_t*)0x0600A000) /* blocks 20-21, plane A low  */
#define BG3_MAP64 ((volatile uint16_t*)0x0600B000) /* blocks 22-23, plane B low  */
#define BG_TILE16 ((volatile uint16_t*)0x06000000)
static int hw_ring_tx,hw_ring_ty;
static uint8_t hw_ring_layout_dirty;
#define FIELD_DIRTY_CHUNK_MAX 8u
struct FieldDirtyChunk { uint16_t cx,cy; uint8_t plane; };
static struct FieldDirtyChunk field_dirty_chunks[FIELD_DIRTY_CHUNK_MAX];
static uint8_t field_dirty_chunk_count;
static int hw_hsa_tx,hw_hsa_ty,hw_hsb_tx,hw_hsb_ty;
static uint8_t hw_hscroll_ring_valid,hw_last_cam_valid;
static unsigned hw_last_camx,hw_last_camy;
static uint16_t hw_chaz_key=0xFFFFu;
static uint16_t hw_follower_key[FIELD_MAX_FOLLOWERS]={0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu};
/* Dead title-cache space is reused only while the field renderer is active. */
#define FIELD_HBLANK_ROWS 161u
#define FIELD_HBLANK_WORDS (FIELD_HBLANK_ROWS*8u)
static uint16_t* const field_hblank_regs0=(uint16_t*)0x02034000;
static uint16_t* const field_hblank_regs1=(uint16_t*)0x02034A10;
static int field_hscroll_pending_atx,field_hscroll_pending_btx,field_hscroll_pending_ty;
static uint16_t* field_hblank_table(unsigned index){return index?field_hblank_regs1:field_hblank_regs0;}
static inline uint8_t nibswap(uint8_t v){return (uint8_t)((v<<4)|(v>>4));}
static inline uint16_t nibswap_pair_aligned(const uint8_t* p){
    /* All callers pass even EWRAM addresses. One LDRH replaces two LDRB loads;
     * the bit transform is exactly nibswap(p[0]) | nibswap(p[1])<<8. */
    uint16_t v=*(const uint16_t*)(const void*)p;
    return (uint16_t)(((v&0x0F0Fu)<<4)|((v&0xF0F0u)>>4));
}
static inline uint16_t md_entry_to_gba(uint16_t e){
    uint16_t mapped=field_tile_remap[e&0x07FFu];if(mapped==0xFFFFu)mapped=0u;
    return (uint16_t)((mapped&0x03FFu)|((e&0x0800u)?0x0400u:0u)|((e&0x1000u)?0x0800u:0u)|(((e>>13)&3u)<<12));
}
static unsigned field_wrap_pixel_int(int v,unsigned span){
    /* Special rings stay within one authored map seam of their prior origin.
     * Spell out bounded correction instead of a loop so Clang cannot strength-
     * reduce it into __aeabi_uidiv/mod on ARM7. */
    if(!span)return 0u;if(v<0)v+=(int)span;if(v<0)v+=(int)span;if((unsigned)v>=span)v-=(int)span;if((unsigned)v>=span)v-=(int)span;return (unsigned)v;
}
static int field_near_tile_origin(int wanted,int old,unsigned span_tiles){
    if(!span_tiles)return wanted;int half=(int)(span_tiles>>1),d=wanted-old;if(d>half)wanted-=(int)span_tiles;else if(d<-half)wanted+=(int)span_tiles;return wanted;
}
static unsigned hw_map64_index(int tx,int ty){unsigned x=(unsigned)tx&63u,y=(unsigned)ty&31u;return y*32u+(x&31u)+((x&32u)?1024u:0u);}
__attribute__((section(".iwram.text"),noinline))
static void hw_write_world_tile(int wtx,int wty){
    unsigned wx=(unsigned)wtx*8u,wy=(unsigned)wty*8u;
    uint16_t a=field_entry(0xA000u,wx,wy),b=field_entry(0xB000u,wx,wy);
    unsigned mi=((unsigned)wty&31u)*32u+((unsigned)wtx&31u);uint16_t ga=md_entry_to_gba(a),gb=md_entry_to_gba(b);
    if(a&0x8000u){BG0_MAP[mi]=ga;BG2_MAP[mi]=0u;}else{BG0_MAP[mi]=0u;BG2_MAP[mi]=ga;}
    if(b&0x8000u){BG1_MAP[mi]=gb;BG3_MAP[mi]=0u;}else{BG1_MAP[mi]=0u;BG3_MAP[mi]=gb;}
}
__attribute__((section(".iwram.text"),noinline))
static void hw_write_hscroll_plane(int plane,int wtx,int wty){
    unsigned spanx=((unsigned)(plane?field_map.row_bg:field_map.row_fg)+1u)*32u;
    unsigned spany=((unsigned)(plane?field_map.col_bg:field_map.col_fg)+1u)*32u;
    unsigned wx=field_wrap_pixel_int(wtx*8,spanx),wy=field_wrap_pixel_int(wty*8,spany);
    uint16_t e=field_entry(plane?0xB000u:0xA000u,wx,wy),g=md_entry_to_gba(e);unsigned mi=hw_map64_index(wtx,wty);
    volatile uint16_t* hi=plane?BG1_MAP64:BG0_MAP64;volatile uint16_t* lo=plane?BG3_MAP64:BG2_MAP64;
    if(e&0x8000u){hi[mi]=g;lo[mi]=0u;}else{hi[mi]=0u;lo[mi]=g;}
}
__attribute__((section(".iwram.text"),noinline))
static void hw_fill_hscroll_plane(int plane,int tx,int ty){for(int y=0;y<32;y++)for(int x=0;x<64;x++)hw_write_hscroll_plane(plane,tx+x,ty+y);if(plane){hw_hsb_tx=tx;hw_hsb_ty=ty;}else{hw_hsa_tx=tx;hw_hsa_ty=ty;}}
__attribute__((section(".iwram.text"),noinline))
static void hw_stream_hscroll_plane(int plane,int tx,int ty){
    int* ox=plane?&hw_hsb_tx:&hw_hsa_tx;int* oy=plane?&hw_hsb_ty:&hw_hsa_ty;
    while(*ox<tx){++*ox;int wx=*ox+63;for(int y=0;y<32;y++)hw_write_hscroll_plane(plane,wx,*oy+y);}
    while(*ox>tx){--*ox;int wx=*ox;for(int y=0;y<32;y++)hw_write_hscroll_plane(plane,wx,*oy+y);}
    while(*oy<ty){++*oy;int wy=*oy+31;for(int x=0;x<64;x++)hw_write_hscroll_plane(plane,*ox+x,wy);}
    while(*oy>ty){--*oy;int wy=*oy;for(int x=0;x<64;x++)hw_write_hscroll_plane(plane,*ox+x,wy);}
}
static void hw_fill_ring(int camtx,int camty){for(int y=0;y<32;y++)for(int x=0;x<32;x++)hw_write_world_tile(camtx+x,camty+y);hw_ring_tx=camtx;hw_ring_ty=camty;hw_ring_layout_dirty=0u;field_dirty_chunk_count=0u;}
static void hw_stream_ring_to(int camtx,int camty){
    while(hw_ring_tx<camtx){++hw_ring_tx;int wx=hw_ring_tx+31;for(int y=0;y<32;y++)hw_write_world_tile(wx,hw_ring_ty+y);}
    while(hw_ring_tx>camtx){--hw_ring_tx;int wx=hw_ring_tx;for(int y=0;y<32;y++)hw_write_world_tile(wx,hw_ring_ty+y);}
    while(hw_ring_ty<camty){++hw_ring_ty;int wy=hw_ring_ty+31;for(int x=0;x<32;x++)hw_write_world_tile(hw_ring_tx+x,wy);}
    while(hw_ring_ty>camty){--hw_ring_ty;int wy=hw_ring_ty;for(int x=0;x<32;x++)hw_write_world_tile(hw_ring_tx+x,wy);}
}
/* Event layout writes normally touch one 32x32-pixel source chunk.  Keep a
 * bounded list so an event does not repopulate all 1,024 visible ring cells.
 * Unknown/direct writers retain hw_ring_layout_dirty as the conservative full
 * refresh escape hatch. */
static void field_dirty_chunk_mark(unsigned plane,unsigned cx,unsigned cy){
    if(hw_ring_layout_dirty)return;
    for(unsigned i=0u;i<field_dirty_chunk_count;i++)if(field_dirty_chunks[i].plane==plane&&field_dirty_chunks[i].cx==cx&&field_dirty_chunks[i].cy==cy)return;
    if(field_dirty_chunk_count>=FIELD_DIRTY_CHUNK_MAX){hw_ring_layout_dirty=1u;field_dirty_chunk_count=0u;return;}
    struct FieldDirtyChunk* d=&field_dirty_chunks[field_dirty_chunk_count++];d->plane=(uint8_t)plane;d->cx=(uint16_t)cx;d->cy=(uint16_t)cy;
}
/* Source absolute layout writers must invalidate their actual plane chunk. */
static void field_update_layout_byte(uint16_t addr,uint8_t value){
    if(addr<0xA000u||addr>=0xC000u||work_ram[addr]==value)return;
    unsigned plane=addr>=0xB000u,idx=addr-(plane?0xB000u:0xA000u),cw=(unsigned)(plane?field_map.row_bg:field_map.row_fg)+1u;
    unsigned ch=(unsigned)(plane?field_map.col_bg:field_map.col_fg)+1u;if(idx>=cw*ch)return;
    work_ram[addr]=value;unsigned cy=0u;while(idx>=cw){idx-=cw;++cy;}field_dirty_chunk_mark(plane,idx,cy);
}
__attribute__((optnone,noinline))
static int field_alias_first(int origin,int base,unsigned span){
    if(!span)return origin;int v=base,step=(int)span;while(v<origin)v+=step;while(v-step>=origin)v-=step;return v;
}
static void field_patch_chunk_ring(const struct FieldDirtyChunk* d){
    unsigned cw=(unsigned)(d->plane?field_map.row_bg:field_map.row_fg)+1u,ch=(unsigned)(d->plane?field_map.col_bg:field_map.col_fg)+1u;
    unsigned sx=cw*4u,sy=ch*4u;if(!sx||!sy)return;
    int ox=hw_ring_tx,oy=hw_ring_ty,fx=field_alias_first(ox,(int)d->cx*4,sx),fy=field_alias_first(oy,(int)d->cy*4,sy);
    for(int by=fy;by<oy+32;by+=(int)sy)for(int bx=fx;bx<ox+32;bx+=(int)sx)
        for(int y=0;y<4&&by+y<oy+32;y++)for(int x=0;x<4&&bx+x<ox+32;x++)hw_write_world_tile(bx+x,by+y);
}
static void field_patch_chunk_hscroll(const struct FieldDirtyChunk* d){
    if(!hw_hscroll_ring_valid)return;unsigned cw=(unsigned)(d->plane?field_map.row_bg:field_map.row_fg)+1u,ch=(unsigned)(d->plane?field_map.col_bg:field_map.col_fg)+1u;
    unsigned sx=cw*4u,sy=ch*4u;if(!sx||!sy)return;int ox=d->plane?hw_hsb_tx:hw_hsa_tx,oy=d->plane?hw_hsb_ty:hw_hsa_ty;
    int fx=field_alias_first(ox,(int)d->cx*4,sx),fy=field_alias_first(oy,(int)d->cy*4,sy);
    for(int by=fy;by<oy+32;by+=(int)sy)for(int bx=fx;bx<ox+64;bx+=(int)sx)
        for(int y=0;y<4&&by+y<oy+32;y++)for(int x=0;x<4&&bx+x<ox+64;x++)hw_write_hscroll_plane(d->plane,bx+x,by+y);
}
static void field_publish_layout_dirty(int camtx,int camty){
    if(hw_ring_layout_dirty){
        if(field_hscroll_mode){if(hw_hscroll_ring_valid){hw_fill_hscroll_plane(0,hw_hsa_tx,hw_hsa_ty);hw_fill_hscroll_plane(1,hw_hsb_tx,hw_hsb_ty);}hw_ring_layout_dirty=0u;field_dirty_chunk_count=0u;}
        else hw_fill_ring(camtx,camty);
        return;
    }
    if(!field_dirty_chunk_count)return;
    if(field_hscroll_mode){if(hw_hscroll_ring_valid)for(unsigned i=0u;i<field_dirty_chunk_count;i++)field_patch_chunk_hscroll(&field_dirty_chunks[i]);}
    else for(unsigned i=0u;i<field_dirty_chunk_count;i++)field_patch_chunk_ring(&field_dirty_chunks[i]);
    field_dirty_chunk_count=0u;
}
static void field_hscroll_source_pair(unsigned y,int16_t* a,int16_t* b){
    unsigned off=field_hscroll_mode==2u?((y>>3)*0x20u):(y*4u);*a=(int16_t)ram16r((uint16_t)off);*b=(int16_t)ram16r((uint16_t)(off+2u));
}
static int field_hscroll_native(int16_t source,unsigned span){return (int)field_wrap_pixel_int(-(int)source,span);}
static int field_floor_div8(int v){return v>=0?(v>>3):-(((-v)+7)>>3);}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_hscroll_rearm(void){
    if(!field_hscroll_mode)return;
    uint16_t* r=field_hblank_table(field_hblank_active_index);
    REG_DMA0CNT=0u;
    REG_BG0HOFS=r[0];REG_BG0VOFS=r[1];REG_BG1HOFS=r[2];REG_BG1VOFS=r[3];
    REG_BG2HOFS=r[4];REG_BG2VOFS=r[5];REG_BG3HOFS=r[6];REG_BG3VOFS=r[7];
    REG_DMA0SAD=(uint32_t)(uintptr_t)&r[8];REG_DMA0DAD=0x04000010u;
    REG_DMA0CNT=DMA_ENABLE|DMA_REPEAT|DMA_START_HBLANK|DMA_DST_RELOAD|8u;
}

/* Build the next physical HBlank table in inactive EWRAM during active display.
 * Source RAM remains authoritative; VBlank publication only streams any needed
 * ring edge, swaps tables, and rearms DMA0. */
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_hscroll_prepare(unsigned camx,unsigned camy){
    if(!field_hscroll_mode)return;
    unsigned next=(unsigned)(field_hblank_active_index^1u);uint16_t* regs=field_hblank_table(next);
    unsigned span_a=((unsigned)field_map.row_fg+1u)*32u,span_b=((unsigned)field_map.row_bg+1u)*32u;
    int16_t sa,sb;field_hscroll_source_pair(0u,&sa,&sb);
    int a0=field_hscroll_native(sa,span_a),bref=field_hscroll_native(sb,span_b),bmin=bref;
    {uint16_t* r=&regs[0];r[0]=(uint16_t)((unsigned)a0&511u);r[1]=(uint16_t)(camy&255u);r[2]=(uint16_t)((unsigned)bref&511u);r[3]=r[1];r[4]=r[0];r[5]=r[1];r[6]=r[2];r[7]=r[1];}
    if(field_hscroll_mode==2u){
        /* Mode 2 consumes one source pair for each eight display rows. */
        for(unsigned group=0u;group<20u;group++){
            unsigned y0=group*8u;field_hscroll_source_pair(y0,&sa,&sb);
            int ah=field_hscroll_native(sa,span_a),bh=field_hscroll_native(sb,span_b),d=bh-bref,half=(int)(span_b>>1);
            if(d>half)d-=(int)span_b;else if(d<-half)d+=(int)span_b;int bu=bref+d;if(bu<bmin)bmin=bu;
            for(unsigned k=0u;k<8u;k++){unsigned y=y0+k;uint16_t* r=&regs[y*8u];r[0]=(uint16_t)((unsigned)ah&511u);r[1]=(uint16_t)(camy&255u);r[2]=(uint16_t)((unsigned)bh&511u);r[3]=r[1];r[4]=r[0];r[5]=r[1];r[6]=r[2];r[7]=r[1];}
        }
    }else{
        for(unsigned y=1u;y<160u;y++){
            field_hscroll_source_pair(y,&sa,&sb);int ah=field_hscroll_native(sa,span_a),bh=field_hscroll_native(sb,span_b),d=bh-bref,half=(int)(span_b>>1);
            if(d>half)d-=(int)span_b;else if(d<-half)d+=(int)span_b;int bu=bref+d;if(bu<bmin)bmin=bu;
            uint16_t* r=&regs[y*8u];r[0]=(uint16_t)((unsigned)ah&511u);r[1]=(uint16_t)(camy&255u);r[2]=(uint16_t)((unsigned)bh&511u);r[3]=r[1];r[4]=r[0];r[5]=r[1];r[6]=r[2];r[7]=r[1];
        }
    }
    for(unsigned i=0u;i<8u;i++)regs[160u*8u+i]=regs[159u*8u+i];
    field_hscroll_pending_atx=field_floor_div8(a0);field_hscroll_pending_btx=field_floor_div8(bmin);field_hscroll_pending_ty=(int)(camy>>3);
    field_hscroll_prepared_index=(uint8_t)next;field_hscroll_prepared=1u;
}

__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_hscroll_publish(void){
    if(!field_hscroll_mode)return;
    if(field_hscroll_dirty&&!field_hscroll_prepared&&hw_last_cam_valid)field_hscroll_prepare(hw_last_camx,hw_last_camy);
    if(field_hscroll_prepared){
        unsigned span_a=((unsigned)field_map.row_fg+1u)*32u,span_b=((unsigned)field_map.row_bg+1u)*32u;
        unsigned tile_span_a=span_a>>3,tile_span_b=span_b>>3;
        int atx=field_hscroll_pending_atx,btx=field_hscroll_pending_btx,ty=field_hscroll_pending_ty;
        if(hw_hscroll_ring_valid){atx=field_near_tile_origin(atx,hw_hsa_tx,tile_span_a);btx=field_near_tile_origin(btx,hw_hsb_tx,tile_span_b);hw_stream_hscroll_plane(0,atx,ty);hw_stream_hscroll_plane(1,btx,ty);}
        else{hw_fill_hscroll_plane(0,atx,ty);hw_fill_hscroll_plane(1,btx,ty);hw_hscroll_ring_valid=1u;}
        field_hblank_active_index=field_hscroll_prepared_index;field_hscroll_prepared=0u;field_hscroll_dirty=0u;
    }
    /* HBlank source increments through 160 rows; it must be restarted every
     * display frame even when the table itself was unchanged. */
    field_hscroll_rearm();
}

__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_hscroll_present_current(void){
    if(field_hscroll_mode&&hw_last_cam_valid&&(REG_VCOUNT>=160u||(REG_DISPCNT&FORCE_BLANK)))field_hscroll_publish();
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void hw_set_scroll(unsigned camx,unsigned camy){
    unsigned camera_changed=!hw_last_cam_valid||hw_last_camx!=camx||hw_last_camy!=camy;
    hw_last_camx=camx;hw_last_camy=camy;hw_last_cam_valid=1u;ram16w(0xEF94u,(uint16_t)camx);ram16w(0xEF90u,(uint16_t)camy);
    if(field_hscroll_mode){if(camera_changed){field_hscroll_dirty=1u;field_hscroll_prepared=0u;}if(field_hscroll_dirty)field_hscroll_prepare(camx,camy);field_hscroll_present_current();return;}
    REG_DMA0CNT=0u;int tx=(int)(camx>>3),ty=(int)(camy>>3);hw_stream_ring_to(tx,ty);uint16_t hx=(uint16_t)(camx&255u),vy=(uint16_t)(camy&255u);
    REG_BG0HOFS=hx;REG_BG1HOFS=hx;REG_BG2HOFS=hx;REG_BG3HOFS=hx;REG_BG0VOFS=vy;REG_BG1VOFS=vy;REG_BG2VOFS=vy;REG_BG3VOFS=vy;
}
static void hw_load_bg_tiles(void){
    volatile uint16_t* dst=BG_TILE16;unsigned dt=0u;
    while(dt<field_tile_count){unsigned st=field_tile_source[dt]&0x07FFu,ri=field_gba_ready_tile[st];
        if(ri!=0xFFFFu){
            unsigned run=1u;while(dt+run<field_tile_count){unsigned ns=field_tile_source[dt+run]&0x07FFu,nr=field_gba_ready_tile[ns];if(nr==0xFFFFu||nr!=ri+run)break;++run;}
            dma_copy32(ps4_psc_field_fastassets+(uint32_t)ri*32u,(void*)&dst[dt*16u],run*32u);dt+=run;continue;
        }
        unsigned off=st*32u;for(unsigned b=0u;b<32u;b+=2u)dst[dt*16u+(b>>1)]=nibswap_pair_aligned(&md_vram[off+b]);++dt;
    }
}
static void hw_publish_pretranslated_tiles(unsigned source_tile,const uint8_t* data,unsigned tiles){
    unsigned i=0u;while(i<tiles){unsigned st=(source_tile+i)&0x07FFu,dt=field_tile_remap[st];if(dt==0xFFFFu){++i;continue;}
        unsigned run=1u;while(i+run<tiles){unsigned nst=(source_tile+i+run)&0x07FFu,ndt=field_tile_remap[nst];if(ndt==0xFFFFu||ndt!=dt+run)break;++run;}
        dma_copy32(data+i*32u,(void*)&BG_TILE16[dt*16u],run*32u);i+=run;
    }
}
static void hw_publish_canonical_tiles(unsigned source_tile,unsigned tiles){
    for(unsigned i=0u;i<tiles;i++){unsigned st=(source_tile+i)&0x07FFu,dt=field_tile_remap[st];if(dt==0xFFFFu)continue;volatile uint16_t* d=&BG_TILE16[dt*16u];unsigned off=st*32u;for(unsigned b=0u;b<32u;b+=2u)d[b>>1]=nibswap_pair_aligned(&md_vram[off+b]);}
}
#include "r30_tile_animation.h"
static void hw_load_palettes(void){dma_copy32(field_gba_palette,(void*)BG_PALETTE,128u);dma_copy32(field_gba_palette,(void*)OBJ_PALETTE,128u);}
static void hw_load_palettes_full(void){dma_fill32((void*)BG_PALETTE,512u,0u);dma_fill32((void*)OBJ_PALETTE,512u,0u);hw_load_palettes();}
/* PSIV field events animate Palette_Table_Buffer ($FB00) and keep a saved
 * copy at $FB80.  The native renderer keeps the live Mega Drive-format colors
 * in cram[], so mirror both source buffers while applying the exact source
 * component arithmetic, then commit through the common GBA palette upload. */
static uint16_t field_palette_saved[64];
static void field_palette_mirror_live(unsigned count){
    if(count>64u)count=64u;
    for(unsigned i=0u;i<count;i++){
        uint16_t q=md2gba(cram[i]);field_gba_palette[i]=q;ram16w((uint16_t)(0xFB00u+i*2u),cram[i]);
        if(field_cutscene_panels)field_pause_pal[i]=md2gba(cram[i]);
    }
}
static void field_palette_save(unsigned count){
    if(count>64u)count=64u;
    for(unsigned i=0u;i<count;i++){field_palette_saved[i]=cram[i];ram16w((uint16_t)(0xFB80u+i*2u),cram[i]);}
    field_palette_mirror_live(count);
}
static void field_palette_wait(unsigned delay){for(unsigned n=0u;n<=delay;n++)waitvb();}
static void field_palette_alarm_down(unsigned delay){
    field_palette_save(64u);
    for(unsigned step=0u;step<8u;step++){
        for(unsigned i=0u;i<64u;i++){
            uint16_t c=cram[i];
            if(c&0x0F00u)c=(uint16_t)(c-0x0200u);
            if(c&0x00F0u)c=(uint16_t)(c-0x0020u);
            cram[i]=c;
        }
        field_palette_mirror_live(64u);hw_load_palettes();field_palette_wait(delay);
    }
}
static void field_palette_alarm_restore(unsigned delay){
    for(unsigned step=0u;step<8u;step++){
        for(unsigned i=0u;i<64u;i++){
            uint16_t c=cram[i],target=field_palette_saved[i];
            if((c&0x0F00u)!=(target&0x0F00u))c=(uint16_t)(c+0x0200u);
            if((c&0x00F0u)!=(target&0x00F0u))c=(uint16_t)(c+0x0020u);
            cram[i]=c;
        }
        field_palette_mirror_live(64u);hw_load_palettes();field_palette_wait(delay);
    }
}
static void field_palette_first32_black(unsigned delay){
    field_palette_save(32u);
    for(;;){
        uint16_t aggregate=0u;
        for(unsigned i=0u;i<32u;i++){
            uint16_t c=cram[i],out=0u;
            for(unsigned n=0u;n<4u;n++){
                unsigned v=(unsigned)((c>>(n*4u))&0x0Fu);if(v)--v;out=(uint16_t)(out|(uint16_t)(v<<(n*4u)));
            }
            cram[i]=out;aggregate=(uint16_t)(aggregate|out);
        }
        field_palette_mirror_live(32u);hw_load_palettes();field_palette_wait(delay);
        if(!aggregate)break;
    }
}
static void field_palette_increase_tone_once(void){
    for(unsigned i=0u;i<64u;i++){uint16_t c=cram[i];unsigned r=c&0xEu,g=c&0xE0u,b=c&0xE00u;r=r==0xEu?0xEu:r+2u;g=g==0xE0u?0xE0u:g+0x20u;b=b==0xE00u?0xE00u:b+0x200u;cram[i]=(uint16_t)(r|g|b);}
}
static void field_palette_restore_step(void){
    for(unsigned i=0u;i<64u;i++){uint16_t c=cram[i],t=field_palette_saved[i];unsigned r=c&0xEu,tr=t&0xEu,g=c&0xE0u,tg=t&0xE0u,b=c&0xE00u,tb=t&0xE00u;if(r!=tr)c=(uint16_t)(c-2u);if(g!=tg)c=(uint16_t)(c-0x20u);if(b!=tb)c=(uint16_t)(c-0x200u);cram[i]=c;}
}
static void field_palette_copy_saved_to_live(void){for(unsigned i=0u;i<64u;i++)cram[i]=field_palette_saved[i];field_palette_mirror_live(64u);hw_load_palettes();}
static void field_palette_all64_to_black(unsigned delay){
    field_palette_save(64u);
    for(;;){uint16_t aggregate=0u;for(unsigned i=0u;i<64u;i++){uint16_t c=cram[i],out=0u;for(unsigned n=0u;n<4u;n++){unsigned v=(unsigned)((c>>(n*4u))&0x0Fu);if(v)--v;out=(uint16_t)(out|(uint16_t)(v<<(n*4u)));}cram[i]=out;aggregate=(uint16_t)(aggregate|out);}field_palette_mirror_live(64u);hw_load_palettes();field_palette_wait(delay);if(!aggregate)break;}
}
static void field_palette_first32_from_black_to_current(unsigned delay){
    field_palette_save(64u);for(unsigned i=0u;i<32u;i++)cram[i]=0u;field_palette_mirror_live(64u);hw_load_palettes();
    for(;;){unsigned changes=0u;for(unsigned i=0u;i<64u;i++){uint16_t c=cram[i],t=field_palette_saved[i],out=0u;for(unsigned n=0u;n<4u;n++){unsigned v=(unsigned)((c>>(n*4u))&0x0Fu),tv=(unsigned)((t>>(n*4u))&0x0Fu);if(v<tv){++v;changes=1u;}out=(uint16_t)(out|(uint16_t)(v<<(n*4u)));}cram[i]=out;}field_palette_mirror_live(64u);hw_load_palettes();field_palette_wait(delay);if(!changes)break;}
}
static void field_reload_palette_from_map(void){
    for(unsigned i=0u;i<64u;i++)cram[i]=0u;
    for(unsigned i=0u;i<32u;i++)cram[i]=rom16(field_map.palette+i*2u);
    for(unsigned i=0u;i<16u;i++)cram[32u+i]=rom16(0x296300u+i*2u);
    for(unsigned i=0u;i<16u;i++)cram[48u+i]=rom16(field_map.palette+0x40u+i*2u);
    field_palette_mirror_live(64u);
}
static void hw_hide_obj(unsigned i){OAM16[i*4u]=160u;OAM16[i*4u+1u]=0u;OAM16[i*4u+2u]=0u;OAM16[i*4u+3u]=0u;}
static void hw_obj_prio(unsigned i,int sx,int sy,unsigned tile,unsigned pal_line,int hflip,unsigned priority){
    if(sx<=-16||sx>=240||sy<=-32||sy>=160){hw_hide_obj(i);return;}
    OAM16[i*4u]=(uint16_t)((sy&255)|0x8000u);OAM16[i*4u+1u]=(uint16_t)((sx&511)|0x8000u|(hflip?0x1000u:0u));OAM16[i*4u+2u]=(uint16_t)((tile&0x03FFu)|((priority&3u)<<10)|((pal_line&15u)<<12));OAM16[i*4u+3u]=0u;
}
static void hw_obj(unsigned i,int sx,int sy,unsigned tile,unsigned pal_line,int hflip){hw_obj_prio(i,sx,sy,tile,pal_line,hflip,1u);}
static void hw_obj_field(unsigned i,int sx,int sy,unsigned w,unsigned h,unsigned tile,unsigned pal_line,int hflip){
    unsigned shape,size;if(w==16u&&h==32u){shape=2u;size=2u;}else if(w==32u&&h==32u){shape=0u;size=2u;}else if(w==16u&&h==16u){shape=0u;size=1u;}else if(w==64u&&h==32u){shape=1u;size=3u;}else{hw_hide_obj(i);return;}
    if(sx<=-(int)w||sx>=240||sy<=-(int)h||sy>=160){hw_hide_obj(i);return;}
    OAM16[i*4u]=(uint16_t)((sy&255)|(shape<<14));OAM16[i*4u+1u]=(uint16_t)((sx&511)|(hflip?0x1000u:0u)|(size<<14));
    OAM16[i*4u+2u]=(uint16_t)((tile&0x03FFu)|(1u<<10)|((pal_line&15u)<<12));OAM16[i*4u+3u]=0u;
}
static void hw_copy_frame_rom(unsigned dst,uint32_t art,unsigned frame){
    uint8_t face=0u,anim=0u;/* Recover the canonical pose selector from its source tile offset. */
    if(frame>=0x30u){face=1u;frame-=0x30u;}else if(frame>=0x18u){face=2u;frame-=0x18u;}
    if(frame==8u)anim=1u;else if(frame==16u)anim=3u;else anim=0u;
    const uint8_t* pose=ps4_psc_party_pose(art,face,anim);if(pose){dma_copy32(pose,(void*)&OBJ_VRAM16[dst*16u],256u);return;}
    volatile uint16_t* ov=OBJ_VRAM16;unsigned canonical=sprite_frame_offset(face,anim);for(unsigned row=0;row<4u;row++)for(unsigned col=0;col<2u;col++){unsigned st=canonical+col*4u+row,dt=dst+row*2u+col;
        for(unsigned b=0;b<32u;b+=2u){uint16_t q=(uint16_t)nibswap(rom8(art+st*32u+b))|((uint16_t)nibswap(rom8(art+st*32u+b+1u))<<8);ov[(dt*32u+b)>>1]=q;}}
}
static void hw_copy_tile_vram(unsigned dst,unsigned st){
    st&=0x07FFu;unsigned ri=field_gba_ready_tile[st];volatile uint16_t* od=&OBJ_VRAM16[dst*16u];
    if(ri!=0xFFFFu){const uint32_t* src=(const uint32_t*)(const void*)(ps4_psc_field_fastassets+(uint32_t)ri*32u);volatile uint32_t* d=(volatile uint32_t*)(void*)od;for(unsigned i=0u;i<8u;i++)d[i]=src[i];return;}
    unsigned off=st*32u;for(unsigned b=0u;b<32u;b+=2u)od[b>>1]=nibswap_pair_aligned(&md_vram[off+b]);
}
static void hw_copy_frame_vram(unsigned dst,unsigned base,unsigned frame){
    for(unsigned row=0;row<4u;row++)for(unsigned col=0;col<2u;col++){unsigned st=base+frame+col*4u+row,dt=dst+row*2u+col;hw_copy_tile_vram(dt,st);}
}
static void hw_copy_frame_ram(unsigned dst,unsigned base,unsigned frame){
    volatile uint16_t* ov=OBJ_VRAM16;for(unsigned row=0;row<4u;row++)for(unsigned col=0;col<2u;col++){unsigned st=base+(frame+col*4u+row)*32u,dt=dst+row*2u+col;
        for(unsigned b=0;b<32u;b+=2u)ov[(dt*32u+b)>>1]=nibswap_pair_aligned(&work_ram[(st+b)&0xFFFFu]);}
}
static void hw_copy_square32_vram(unsigned dst,unsigned base){
    for(unsigned row=0;row<4u;row++)for(unsigned col=0;col<4u;col++){unsigned st=base+col*4u+row,dt=dst+row*4u+col;hw_copy_tile_vram(dt,st);}
}
static void hw_copy_square24_vram(unsigned dst,unsigned base){
    /* Genesis field monster sheets are column-major.  Copy a 3x3 source
     * frame into a cleared 4x4 GBA 1D-OBJ carrier; the fourth row/column stay
     * transparent. */
    dma_fill32((void*)(OBJ_VRAM16+dst*16u),16u*32u,0u);
    for(unsigned row=0u;row<3u;row++)for(unsigned col=0u;col<3u;col++){unsigned st=base+col*3u+row,dt=dst+row*4u+col;hw_copy_tile_vram(dt,st);}
}
/* Compose the original six-byte Genesis sprite mapping pieces into one GBA
 * 1D OBJ carrier.  This is used for chests because their source mappings are
 * not the common 16x32 actor strip (the white open chest is three pieces and
 * uses source H-flip on its right side). */
static uint8_t* const field_obj_compose_scratch=(uint8_t*)0x02022000;
static unsigned md_tile_pixel(unsigned tile,unsigned x,unsigned y){
    uint8_t q=md_vram[((tile&0x07FFu)*32u)+(y&7u)*4u+((x&7u)>>1)];return (x&1u)?(q&15u):(q>>4);
}
static void field_obj_compose_set(unsigned wt,unsigned x,unsigned y,unsigned ci){
    unsigned tile=(y>>3)*wt+(x>>3),off=tile*32u+(y&7u)*4u+((x&7u)>>1);uint8_t q=field_obj_compose_scratch[off];
    if(x&1u)q=(uint8_t)((q&0x0Fu)|(ci<<4));else q=(uint8_t)((q&0xF0u)|ci);field_obj_compose_scratch[off]=q;
}
static void hw_compose_mapping_vram(unsigned dst,unsigned wt,unsigned ht,unsigned base,uint32_t mapping,int origin_x,int origin_y){
    unsigned bytes=wt*ht*32u;for(unsigned i=0u;i<bytes;i++)field_obj_compose_scratch[i]=0u;
    unsigned pieces=(unsigned)rom8(mapping)+1u;uint32_t p=mapping+2u;
    for(unsigned n=0u;n<pieces;n++,p+=6u){
        /* Field_LoadSprites/Field_BuildSprites consumes mapping piece X from
         * byte +4 (move.b (a1)+,d0), then skips byte +5 (addq #1,a1).  The
         * old +5 decode was the systemic one-tile-left field regression. */
        int py=(int)(int8_t)rom8(p),px=(int)(int8_t)rom8(p+4u);unsigned size=rom8(p+1u),attr_hi=rom8(p+2u),tile_off=rom8(p+3u);
        unsigned pw=((size>>2)&3u)+1u,ph=(size&3u)+1u,hf=(attr_hi&0x08u)!=0u,vf=(attr_hi&0x10u)!=0u;
        unsigned first=(base+tile_off)&0x07FFu;
        for(unsigned y=0u;y<ph*8u;y++)for(unsigned x=0u;x<pw*8u;x++){
            unsigned sx=hf?(pw*8u-1u-x):x,sy=vf?(ph*8u-1u-y):y;
            unsigned st=(first+(sx>>3)*ph+(sy>>3))&0x07FFu,ci=md_tile_pixel(st,sx&7u,sy&7u);
            int ox=px-origin_x+(int)x,oy=py-origin_y+(int)y;if(ci&&ox>=0&&oy>=0&&(unsigned)ox<wt*8u&&(unsigned)oy<ht*8u)field_obj_compose_set(wt,(unsigned)ox,(unsigned)oy,ci);
        }
    }
    dma_copy32(field_obj_compose_scratch,(void*)&OBJ_VRAM16[dst*16u],bytes);
}
static unsigned md_art_pixel_rom(uint32_t art,unsigned tile,unsigned x,unsigned y){
    uint8_t q=rom8(art+tile*32u+(y&7u)*4u+((x&7u)>>1));return (x&1u)?(q&15u):(q>>4);
}
static void hw_compose_mapping_rom(unsigned dst,unsigned wt,unsigned ht,uint32_t art,uint32_t mapping,int origin_x,int origin_y){
    unsigned bytes=wt*ht*32u;for(unsigned i=0u;i<bytes;i++)field_obj_compose_scratch[i]=0u;
    unsigned pieces=(unsigned)rom8(mapping)+1u;uint32_t p=mapping+2u;
    for(unsigned n=0u;n<pieces;n++,p+=6u){
        int py=(int)(int8_t)rom8(p),px=(int)(int8_t)rom8(p+4u);unsigned size=rom8(p+1u),attr_hi=rom8(p+2u),tile_off=rom8(p+3u);
        unsigned pw=((size>>2)&3u)+1u,ph=(size&3u)+1u,hf=(attr_hi&0x08u)!=0u,vf=(attr_hi&0x10u)!=0u;
        for(unsigned y=0u;y<ph*8u;y++)for(unsigned x=0u;x<pw*8u;x++){
            unsigned sx=hf?(pw*8u-1u-x):x,sy=vf?(ph*8u-1u-y):y;
            unsigned st=tile_off+(sx>>3)*ph+(sy>>3),ci=md_art_pixel_rom(art,st,sx&7u,sy&7u);
            int ox=px-origin_x+(int)x,oy=py-origin_y+(int)y;if(ci&&ox>=0&&oy>=0&&(unsigned)ox<wt*8u&&(unsigned)oy<ht*8u)field_obj_compose_set(wt,(unsigned)ox,(unsigned)oy,ci);
        }
    }
    dma_copy32(field_obj_compose_scratch,(void*)&OBJ_VRAM16[dst*16u],bytes);
}
static int r31_actor_type(uint16_t type);
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void hw_refresh_field_object_art(unsigned i){
    if(i<field_obj_count&&r31_actor_type(field_objs[i].type))return;
    if(i>=field_obj_count||!field_objs[i].active)return;const struct FieldObjLite* o=&field_objs[i];if(!o->render_w||!o->render_h)return;unsigned dst=o->gba_tile;uint8_t face=(uint8_t)((o->facing>>2)&3u);
    if(o->type==0x0078u||o->type==0x007Cu||o->type==0x0084u)return;
    uint32_t key0=(uint32_t)o->type|((uint32_t)o->base_tile<<16);
    uint32_t key1=(uint32_t)o->gba_tile|((uint32_t)o->facing<<16)|((uint32_t)o->anim<<24);
    struct FieldObjArtStamp* stamp=&field_obj_art_stamp[i];
    if(stamp->epoch==field_obj_art_epoch&&stamp->key0==key0&&stamp->key1==key1)return;
#if PS4_NPCTYPE1_STANDARD_NPC
    if(o->type==0x0098u)hw_compose_mapping_vram(dst,4u,4u,o->base_tile,0x4CABCu,0,0);   /* CaveWallPiece */
#else
    if(o->type==0x0038u)hw_compose_mapping_vram(dst,4u,4u,o->base_tile,0x4CABCu,0,0);
#endif
    else if(o->type==0x00ECu)hw_compose_mapping_rom(dst,4u,4u,0x291200u,0x4CE8Au,0,0);
    else if(o->type==0x00A0u)hw_compose_mapping_vram(dst,2u,4u,o->base_tile,o->facing?0x5146Au:0x51462u,0,8);
    else if(o->type==0x01D4u)hw_compose_mapping_vram(dst,4u,4u,o->base_tile,o->facing?0x51480u:0x51472u,-4,6);
    else if(o->type==0x01A0u){static const uint32_t m[6]={0x4E7F4u,0x4E7FCu,0x4E804u,0x4E80Cu,0x4E814u,0x4E828u};hw_compose_mapping_vram(dst,4u,4u,o->base_tile,m[o->anim%6u],-12,-12);}
    else if(o->type==0x0184u)hw_copy_square24_vram(dst,o->base_tile+((o->anim&1u)?9u:0u));
    else if(o->type==0x0188u)hw_copy_square32_vram(dst,o->base_tile); /* CP22: both source frames use the same 16 tiles; frame 1 is H-flipped (see hw_update_objects) */
    else if(o->type==0x0148u)hw_copy_frame_vram(dst,0x0200u,(o->anim&1u)?9u:0u);
    else if(o->type==0x68u)hw_copy_frame_rom(dst,0x290900u,sprite_frame_offset(face,o->anim));
    else if(o->type==0x6Cu)hw_copy_frame_rom(dst,0x291200u,sprite_frame_offset(face,o->anim));
    else hw_copy_frame_vram(dst,o->base_tile,sprite_frame_offset(face,o->anim));
    stamp->key0=key0;stamp->key1=key1;stamp->epoch=field_obj_art_epoch;
}
#include "r31_field_actors.h"
static void hw_prepare_static_obj_art(void){for(unsigned i=0;i<field_obj_count;i++)hw_refresh_field_object_art(i);}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_followers_sync(int reset_positions){
    field_follower_count=0u;for(unsigned i=0u;i<FIELD_MAX_FOLLOWERS;i++)hw_follower_key[i]=0xFFFFu;
    int bx=chaz.x,by=chaz.y;
    if(reset_positions){if(chaz.facing==0u)by-=16;else if(chaz.facing==1u)by+=16;else if(chaz.facing==2u)bx-=16;else bx+=16;}
    for(unsigned slot=1u;slot<5u&&field_follower_count<FIELD_MAX_FOLLOWERS;slot++){
        uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)break;
        struct PartyFollowerLite* f=&field_followers[field_follower_count++];f->id=id;f->active=1u;
        if(reset_positions){f->x=bx;f->y=by;f->tx=bx;f->ty=by;f->facing=chaz.facing;f->anim=0u;f->anim_clock=0u;f->moving=0u;
            if(chaz.facing==0u)by-=16;else if(chaz.facing==1u)by+=16;else if(chaz.facing==2u)bx-=16;else bx+=16;}
    }
    for(unsigned i=field_follower_count;i<FIELD_MAX_FOLLOWERS;i++)field_followers[i].active=0u;
}
static void field_followers_sync_aligned(unsigned align){
    /* loc_535D4/loc_53642: 0 stacks the party at the doorway; 4/8/C/10
     * extend down/up/left/right by one source tile per member. */
    field_follower_count=0u;for(unsigned i=0u;i<FIELD_MAX_FOLLOWERS;i++)hw_follower_key[i]=0xFFFFu;int bx=chaz.x,by=chaz.y;
    for(unsigned slot=1u;slot<5u&&field_follower_count<FIELD_MAX_FOLLOWERS;slot++){
        uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)break;
        if(align==4u)by+=16;else if(align==8u)by-=16;else if(align==12u)bx-=16;else if(align==16u)bx+=16;
        struct PartyFollowerLite* f=&field_followers[field_follower_count++];f->id=id;f->active=1u;f->x=bx;f->y=by;f->tx=bx;f->ty=by;f->facing=chaz.facing;f->anim=0u;f->anim_clock=0u;f->moving=0u;
    }
    for(unsigned i=field_follower_count;i<FIELD_MAX_FOLLOWERS;i++)field_followers[i].active=0u;
}
static void field_apply_pending_party_position(void){
    if(!field_manager_party_position_pending)return;
    /* MapDataMan_PiataBasementB2_CharPositions: source writes these by
     * character ID after Igglanova and before the ordinary field loop. */
    static const uint16_t px[3]={0x00F0u,0x0100u,0x0100u};
    static const uint16_t py[3]={0x00D0u,0x00D0u,0x00A0u};
    static const uint8_t sf[3]={0u,12u,8u};
    for(unsigned id=0u;id<3u;id++){
        int slot=-1;for(unsigned q=0u;q<5u;q++){uint8_t cid=ram8r((uint16_t)(0xF40Au+q));if(cid==0xFFu)break;if(cid==id){slot=(int)q;break;}}
        if(slot<0)continue;uint8_t nf=(uint8_t)((sf[id]>>2)&3u);
        ram16w((uint16_t)(0xC000u+(unsigned)slot*0x40u+0x06u),sf[id]);
        ram16w((uint16_t)(0xC000u+(unsigned)slot*0x40u+0x30u),px[id]);ram16w((uint16_t)(0xC000u+(unsigned)slot*0x40u+0x34u),py[id]);
        if(id==lead_char_id){chaz.x=px[id];chaz.y=py[id];chaz.facing=nf;chaz.anim=0u;chaz.step_left=0u;chaz.dx=chaz.dy=0;hw_chaz_key=0xFFFFu;}
        else for(unsigned f=0u;f<field_follower_count;f++)if(field_followers[f].active&&field_followers[f].id==id){field_followers[f].x=px[id];field_followers[f].y=py[id];field_followers[f].tx=px[id];field_followers[f].ty=py[id];field_followers[f].facing=nf;field_followers[f].anim=0u;field_followers[f].moving=0u;hw_follower_key[f]=0xFFFFu;break;}
    }
    field_manager_party_position_pending=0u;
}
static void field_followers_begin_chain(void){
    int px=chaz.x,py=chaz.y;
    for(unsigned i=0;i<field_follower_count;i++){
        struct PartyFollowerLite* f=&field_followers[i];int ox=f->x,oy=f->y;f->tx=px;f->ty=py;f->moving=(uint8_t)(f->x!=f->tx||f->y!=f->ty);px=ox;py=oy;
    }
}
/* Keep PSIV's Character_1..5 records authoritative alongside the native GBA
 * actor cache.  The original engine has one field-object representation; this
 * port historically split the lead/followers into C structs while many source-
 * translated map managers/events still read $C000+$40*slot directly.  Ordinary
 * walking therefore left those source coordinates stale.  That is a systemic
 * state-seam bug, not a map-data problem. */
static void field_sync_lead_to_source(void){
    uint16_t b=0xC000u;int tx=chaz.x,ty=chaz.y;
    if(chaz.step_left){tx+=chaz.dx*(int)chaz.step_left;ty+=chaz.dy*(int)chaz.step_left;}
    ram16w((uint16_t)(b+0x06u),(uint16_t)((unsigned)chaz.facing<<2));
    ram16w((uint16_t)(b+0x30u),(uint16_t)chaz.x);ram16w((uint16_t)(b+0x34u),(uint16_t)chaz.y);
    ram16w((uint16_t)(b+0x38u),(uint16_t)tx);ram16w((uint16_t)(b+0x3Au),(uint16_t)ty);
}
static void field_sync_native_party_to_source(void){
    field_sync_lead_to_source();
    for(unsigned i=0u;i<field_follower_count&&i<4u;i++){
        const struct PartyFollowerLite* f=&field_followers[i];if(!f->active)continue;uint16_t b=(uint16_t)(0xC000u+(i+1u)*0x40u);
        ram16w((uint16_t)(b+0x06u),(uint16_t)((unsigned)f->facing<<2));
        ram16w((uint16_t)(b+0x30u),(uint16_t)f->x);ram16w((uint16_t)(b+0x34u),(uint16_t)f->y);
        ram16w((uint16_t)(b+0x38u),(uint16_t)f->tx);ram16w((uint16_t)(b+0x3Au),(uint16_t)f->ty);
    }
}
/* Source SprMapsData_* walking mapping durations.  FieldObj_Animate decrements
 * mappings_duration once per object update and advances after it becomes negative,
 * so a source duration N is held for N+1 updates.  Keep one shared helper for
 * player, follower and scripted-event movement; the previous event path advanced
 * every 2-pixel tick and made Alys visibly stutter during Event_GameStart. */
static unsigned field_party_anim_period(uint8_t id){
    static const uint8_t source_duration[11]={0x0Au,0x08u,0x07u,0x0Bu,0x08u,0x06u,0x04u,0x09u,0x0Cu,0x09u,0x0Bu};
    if(ram16r(0xF43Cu))return 5u;
    return id<11u?(unsigned)source_duration[id]+1u:11u;
}
static void field_party_anim_advance(uint8_t id,uint8_t* anim,uint8_t* clock){
    if(++*clock>=field_party_anim_period(id)){*clock=0u;*anim=(uint8_t)((*anim+1u)&3u);}
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_followers_advance(void){
    for(unsigned i=0;i<field_follower_count;i++){
        struct PartyFollowerLite* f=&field_followers[i];if(!f->active||!f->moving){f->anim=0u;f->anim_clock=0u;continue;}
        int dx=0,dy=0;if(f->x<f->tx){dx=1;f->facing=2u;}else if(f->x>f->tx){dx=-1;f->facing=3u;}else if(f->y<f->ty){dy=1;f->facing=0u;}else if(f->y>f->ty){dy=-1;f->facing=1u;}
        unsigned n=2u;if(dx){int rem=f->tx-f->x;if(rem<0)rem=-rem;if(rem<2)n=1u;}else{int rem=f->ty-f->y;if(rem<0)rem=-rem;if(rem<2)n=1u;}
        f->x+=dx*(int)n;f->y+=dy*(int)n;if(f->x==f->tx&&f->y==f->ty)f->moving=0u;
        field_party_anim_advance(f->id,&f->anim,&f->anim_clock);
    }
}
static void hw_update_followers(unsigned camx,unsigned camy){
    for(unsigned i=0;i<FIELD_MAX_FOLLOWERS;i++){
        unsigned oi=40u+i;if(i>=field_follower_count||!field_followers[i].active||(ram8r((uint16_t)(0xC042u+i*64u))&2u)){hw_hide_obj(oi);continue;}
        struct PartyFollowerLite* f=&field_followers[i];unsigned dst=FIELD_FOLLOWER_TILE_BASE+i*8u;
        /* Same unchanged-pose fast path already used by the lead character.
         * A walking party otherwise copied up to four 8-tile frames from ROM
         * into OBJ VRAM every display frame even though each source pose is
         * held for ten updates.  Cache by character/facing/animation so the
         * copy occurs only when the authored pose actually changes. */
        unsigned frame=sprite_frame_offset(f->facing,f->anim);uint16_t key=(uint16_t)(((uint16_t)f->id<<8)|(frame&0xFFu));
        if(hw_follower_key[i]!=key){hw_copy_frame_rom(dst,char_field_art_address(f->id),frame);hw_follower_key[i]=key;}
        hw_obj(oi,f->x-(int)camx,f->y-(int)camy-2,dst,2u,f->facing==2u);
    }
}
#include "r30_vehicle_runtime.h"
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void hw_update_objects(unsigned camx,unsigned camy){
    r31_actor_draw(camx,camy);
    for(unsigned i=0;i<field_obj_count;i++){const struct FieldObjLite* o=&field_objs[i];
        /* Render eligibility is data-driven.  Invisible collision objects and
         * any source object with no validated carrier mapping have 0x0 render
         * extent and must never submit stale OBJ tiles to OAM. */
        if(!o->active||!o->render_w||!o->render_h||(ram8r((uint16_t)(0xC302u+i*64u))&2u)){hw_hide_obj(1u+i);continue;}
        uint8_t f=(uint8_t)((o->facing>>2)&3u);int hf=(f==2u);
        /* CP22 Igglanova (runtime IMG_0859 flicker/partial sprite): source frame list
         * loc_4C3D6 = {2 frames, duration 7: loc_4CCCC, loc_4CCD4}.  loc_4CCD4's
         * pattern word is $0800 = tile 0 with the Genesis H-flip bit, not tile +8.
         * The idle animation mirrors the same 32x32 image every 8 updates. */
        if(o->type==0x0188u)hf=(int)(o->anim&1u);
        unsigned pl=(o->type==0x00A0u)?(field_map.general==0x32u?1u:2u):(o->type==0x01D4u?2u:((o->type==0x68u||o->type==0x6Cu)?2u:3u));hw_obj_field(1u+i,(int)o->x-(int)camx+o->render_x,(int)o->y-(int)camy+o->render_y,o->render_w,o->render_h,o->gba_tile,pl,hf);}
    if(r30_vehicle_draw(camx,camy))return;
    unsigned lead_frame=sprite_frame_offset(chaz.facing,chaz.anim);uint16_t key=(uint16_t)(((uint16_t)lead_char_id<<8)|(lead_frame&0xFFu));if(key!=hw_chaz_key){hw_copy_frame_rom(FIELD_OBJ_TILE_BASE,lead_art_address(),lead_frame);hw_chaz_key=key;}
    if(ram8r(0xC002u)&2u)hw_hide_obj(0u);else hw_obj(0u,chaz.x-(int)camx,chaz.y-(int)camy-2,FIELD_OBJ_TILE_BASE,2u,chaz.facing==2u);
    hw_update_followers(camx,camy);
}
static void hw_field_init(unsigned camx,unsigned camy){
    field_dirty_chunk_count=0u;hw_ring_layout_dirty=0u;REG_DMA0CNT=0u;REG_DISPCNT=FORCE_BLANK;REG_BLDCNT=0u;REG_BLDY=0u;hw_load_bg_tiles();hw_load_palettes_full();for(unsigned i=0;i<128u;i++)hw_hide_obj(i);field_obj_art_epoch_advance();hw_prepare_static_obj_art();hw_chaz_key=0xFFFFu;for(unsigned i=0u;i<FIELD_MAX_FOLLOWERS;i++)hw_follower_key[i]=0xFFFFu;hw_last_cam_valid=0u;
    /* Genesis priority-bit tiles must occlude field sprites (desks/counters).
     * Put both high-priority source planes above ordinary OBJ, and both low
     * planes below it.  This fixes the apparent principal/shopkeeper "standing
     * on furniture" without falsifying source coordinates. */
    if(field_hscroll_mode){hw_hscroll_ring_valid=0u;REG_BG0CNT=(uint16_t)(0u|(16u<<8)|(1u<<14));REG_BG1CNT=(uint16_t)(0u|(18u<<8)|(1u<<14));REG_BG2CNT=(uint16_t)(2u|(20u<<8)|(1u<<14));REG_BG3CNT=(uint16_t)(3u|(22u<<8)|(1u<<14));}
    else{hw_fill_ring((int)(camx>>3),(int)(camy>>3));REG_BG0CNT=(uint16_t)(0u|(16u<<8));REG_BG1CNT=(uint16_t)(0u|(17u<<8));REG_BG2CNT=(uint16_t)(2u|(18u<<8));REG_BG3CNT=(uint16_t)(3u|(19u<<8));}
    hw_set_scroll(camx,camy);if(field_hscroll_mode&&field_hscroll_dirty)field_hscroll_publish();hw_update_objects(camx,camy);REG_DISPCNT=(uint16_t)(MODE0|OBJ_1D|BG0_ENABLE|BG1_ENABLE|BG2_ENABLE|BG3_ENABLE|OBJ_ENABLE);
}

static int field_npc_target_blocked(unsigned self,int nx,int ny){
    const struct FieldObjLite* o=&field_objs[self];int dx=nx-(int)o->x,dy=ny-(int)o->y;
    if(collision_blocks(field_collision_at((int)o->x,(int)o->y,dx,dy)))return 1;
    /* Source loc_4A316 (FieldObj_NPCMove's object-collision check) walks
     * Character_1..Character_5 only.  It deliberately does NOT collide a
     * roaming NPC against Field_Obj_Secondary / other NPCs.  The old native
     * lowering tested every field object as well, so harmless/invisible event
     * objects could box Hahn or another walker in place; its source controller
     * would then choose the opposite direction on the next update and visibly
     * flip/animate at full speed without moving.  Preserve the exact party-only
     * occupancy here.  Lead/followers are the native split representation of
     * those five source Character records. */
    if(player_reserves(nx,ny))return 1;
    for(unsigned f=0;f<field_follower_count;f++)if(follower_reserves(&field_followers[f],nx,ny))return 1;
    return 0;
}
static unsigned field_npc_pick_pseudo(struct FieldObjLite* o){
    if(o->move_kind==1u||o->move_kind==2u||o->move_kind==8u){
        if(o->move_kind==8u&&!ps4_event_flag_test(0x33u))return 0u; /* Igglanova/Zema gate */
        if(o->timer){--o->timer;return 0u;}
        uint16_t r=field_npc_rng_next();unsigned mask=o->move_kind==2u?0x7Fu:0x3Fu;r&=(uint16_t)mask;o->timer=(uint8_t)r;return (unsigned)(r&7u);
    }
    if(o->move_kind==3u)return (unsigned)(field_npc_rng_next()&7u);
    if(o->move_kind==4u){static const uint8_t square[8]={1u,1u,4u,4u,2u,2u,3u,3u};unsigned q=square[o->seq_pos&7u];o->seq_pos=(uint8_t)((o->seq_pos+1u)&7u);return q;}
    if(o->move_kind==5u){unsigned q=o->seq_pos?4u:3u;o->seq_pos^=1u;return q;}
    if(o->move_kind==6u){ /* loc_49C18: Piata basement guards */
        if(ps4_event_flag_test(0x0Cu))return 2u;
        int dy=(int)o->y-chaz.y;if(dy<0)dy=-dy;if(dy>16)return 1u;
        int dx=(int)o->x-chaz.x;if(dx==0)return 1u;if(dx>0&&dx<=16)return 3u;if(dx<0&&-dx<=16)return 4u;return 1u;
    }
    if(o->move_kind==7u){ /* loc_49C86: rancher tracks the lead character */
        int dy=(int)o->y-chaz.y;if(dy<0)dy=-dy;if(dy>16)return 2u;
        int dx=(int)o->x-chaz.x;if(dx==0)return 2u;if(dx>0&&dx<=16)return 3u;if(dx<0&&-dx<=16)return 4u;return 2u;
    }
    if(o->move_kind==9u){ /* loc_49D34 / loc_49D88 exact fixed-course families */
        static const uint8_t seq[8][8]={{3,3,3,3,3,0,0,0},{4,4,4,4,4,4,4,0},{1,3,3,3,3,0,0,0},{1,1,4,4,4,0,0,0},{2,2,3,3,3,3,3,0},{2,4,4,4,4,0,0,0},{1,1,1,1,1,0,0,0},{2,2,2,2,2,0,0,0}};
        if(!o->seq_pos)o->timer=(uint8_t)(field_npc_rng_next()&7u);unsigned q=seq[o->timer&7u][o->seq_pos++];if(!q||o->seq_pos>=8u)o->seq_pos=0u;return q;
    }
    if(o->move_kind==10u){ /* NPCType12: source alternates vertical/horizontal bursts */
        if(o->timer){--o->timer;return 0u;}uint16_t r=field_npc_rng_next();o->timer=0x20u;return (r&1u)?3u:4u;
    }
    if(o->move_kind==11u){ /* loc_49E20: horizontal-only random movement */
        if(o->timer){--o->timer;return 0u;}for(;;){uint16_t r=field_npc_rng_next()&0x3Fu;o->timer=(uint8_t)r;unsigned q=r&7u;if(q!=3u&&q!=4u)return q;}
    }
    if(o->move_kind==12u){static const uint8_t q[12]={2,2,2,4,4,4,1,1,1,3,3,3};unsigned v=q[o->seq_pos++%12u];return v;}
    if(o->move_kind==13u){ /* loc_49EE0: Aiedo child fixed course */
        static const uint8_t q[18]={4,4,4,4,4,4,2,2,2,3,3,3,3,3,3,1,1,1};unsigned v=q[o->seq_pos++%18u];return v;
    }
    if(o->move_kind==14u){ /* loc_49F2A: companion Aiedo child fixed course */
        static const uint8_t q[18]={4,4,4,4,2,2,2,3,3,3,3,3,3,1,1,1,4,4};unsigned v=q[o->seq_pos++%18u];return v;
    }
    if(o->move_kind==15u){ /* loc_49F74: savings man patrol */
        static const uint8_t q[21]={4,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,4,3,3,3};unsigned v=q[o->seq_pos++%21u];return v;
    }
    if(o->move_kind==16u){ /* loc_49FC0: Aiedo shopper/mother course */
        static const uint8_t q[45]={4,4,4,4,4,4,4,4,4,4,4,2,2,3,3,3,3,3,3,1,3,3,3,2,2,4,4,4,4,4,1,3,3,3,3,3,3,3,1,1,1,1,4,4,4};unsigned v=q[o->seq_pos++%45u];return v;
    }
    return 0u;
}
static void field_npc_try_start(unsigned i){
    struct FieldObjLite* o=&field_objs[i];unsigned pseudo=field_npc_pick_pseudo(o);if(pseudo<1u||pseudo>4u)return;
    int dx=0,dy=0,bx=o->bound_x,by=o->bound_y;uint8_t face=o->facing;
    if(pseudo==1u){dy=-1;--by;face=4u;}else if(pseudo==2u){dy=1;++by;face=0u;}else if(pseudo==3u){dx=-1;--bx;face=12u;}else{dx=1;++bx;face=8u;}
    o->facing=face;o->anim=0u;hw_refresh_field_object_art(i);
    if(bx<0||by<0||bx>o->max_x||by>o->max_y)return;
    int nx=(int)o->x+dx*16,ny=(int)o->y+dy*16;if(field_npc_target_blocked(i,nx,ny))return;
    o->bound_x=(int8_t)bx;o->bound_y=(int8_t)by;o->move_dir=(uint8_t)pseudo;o->move_left=16u;o->move_phase=0u;
}
static int field_basement_monster_retreat(unsigned i){
    struct FieldObjLite* o=&field_objs[i];if(o->type!=0x0184u||chaz.y<=0x0100)return 0;
    if(o->x>=0x01A0u){o->x=(uint16_t)(o->x-4u);ram16w((uint16_t)(0xC300u+i*0x40u+0x30u),o->x);}
    else{ps4_flag_set_type(2u,0x13u);o->active=0u;ram16w((uint16_t)(0xC300u+i*0x40u),0u);}
    return 1;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void field_update_npcs(unsigned camx,unsigned camy){
    r31_actor_update();
    if(++field_idle_anim_clock>=8u){field_idle_anim_clock=0u;for(unsigned i=0;i<field_obj_count;i++)if(field_objs[i].active&&(field_objs[i].type==0x0184u||field_objs[i].type==0x0188u)){field_objs[i].anim^=1u;hw_refresh_field_object_art(i);}}
    for(unsigned i=0;i<field_obj_count;i++){
        struct FieldObjLite* o=&field_objs[i];if(!o->active)continue;if(field_basement_monster_retreat(i)||!o->move_kind)continue;
        int sx=(int)o->x-(int)camx,sy=(int)o->y-(int)camy;if(sx<-32||sx>272||sy<-48||sy>192)continue; /* mirrors source on-screen gating */
        if(!o->move_left){field_npc_try_start(i);continue;}
        /* d7=0 ($80 in 8.8) for ordinary walkers, d7=1 ($100) for Hahn
         * and fixed-course walkers.  Avoid fractional coordinates: slow NPCs
         * advance one pixel every other frame. */
        unsigned pixels=1u;if(o->move_kind<=3u||o->move_kind==8u||o->move_kind==11u){o->move_phase^=1u;if(o->move_phase)continue;}
        if(o->move_kind==6u)pixels=2u; /* d7=2 => $0200 source step */
        int dx=0,dy=0;if(o->move_dir==1u)dy=-1;else if(o->move_dir==2u)dy=1;else if(o->move_dir==3u)dx=-1;else if(o->move_dir==4u)dx=1;
        if(pixels>o->move_left)pixels=o->move_left;o->x=(uint16_t)((int)o->x+dx*(int)pixels);o->y=(uint16_t)((int)o->y+dy*(int)pixels);o->move_left=(uint8_t)(o->move_left-pixels);
        uint8_t a=(o->move_left>8u)?1u:(o->move_left?3u:0u);if(a!=o->anim){o->anim=a;hw_refresh_field_object_art(i);}if(!o->move_left)o->move_dir=0u;
    }
}

static void choose_camera(unsigned* camx,unsigned* camy){
    int x=chaz.x-152,y=chaz.y-88;int maxx=(int)(((unsigned)field_map.row_fg+1u)*32u)-240;int maxy=(int)(((unsigned)field_map.col_fg+1u)*32u)-160;
    if(maxx<0)maxx=0;if(maxy<0)maxy=0;if(x<0)x=0;if(y<0)y=0;if(x>maxx)x=maxx;if(y>maxy)y=maxy;*camx=(unsigned)x;*camy=(unsigned)y;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static uint8_t field_collision_ring(int rel){
    /* Memory layout relative to Tile_Collision_Up_1 ($EC85).  The source keeps
     * duplicate left/up/top-corner samples later in the ring; preserve those
     * aliases because FieldObj_DoTileCollision indexes around a0 directly. */
    switch(rel){
        case -2: case 6: return field_collision_at(chaz.x,chaz.y,-16,0);       /* left */
        case -1: case 7: return field_collision_at(chaz.x,chaz.y,-16,-16);    /* top-left */
        case 0:  case 8: return field_collision_at(chaz.x,chaz.y,0,-16);      /* up */
        case 1:  case 9: return field_collision_at(chaz.x,chaz.y,16,-16);     /* top-right */
        case 2:          return field_collision_at(chaz.x,chaz.y,16,0);       /* right */
        case 3:          return field_collision_at(chaz.x,chaz.y,16,16);      /* bottom-right */
        case 4:          return field_collision_at(chaz.x,chaz.y,0,16);       /* down */
        case 5:          return field_collision_at(chaz.x,chaz.y,-16,16);     /* bottom-left */
        default: return 0u;
    }
}
__attribute__((section(".iwram.text"),noinline,aligned(4)))
static uint8_t field_collision_ring_resolve_cached(int rel,uint8_t cache[8],uint8_t* valid){
    /* CP19: resolver-relative aliases are an eight-cell ring.  The resolver's
     * legal probes are bounded to -2..9, so one +/-8 normalization preserves
     * the source aliases without any persistent terrain state. */
    unsigned idx=(unsigned)rel&7u;uint8_t bit=(uint8_t)(1u<<idx);
    if(!(*valid&bit)){cache[idx]=field_collision_ring(rel);*valid=(uint8_t)(*valid|bit);}
    return cache[idx];
}
static int field_direction_vector(uint8_t d,int* dx,int* dy,uint8_t* facing){
    *dx=0;*dy=0;*facing=chaz.facing;
    /* loc_47AA8 uses horizontal movement for the valid diagonal d-pad values. */
    if(d==1u){*dy=-1;*facing=1u;return 1;}
    if(d==2u){*dy=1;*facing=0u;return 1;}
    if(d>=4u&&d<=6u){*dx=-1;*facing=3u;return 1;}
    if(d>=8u&&d<=10u){*dx=1;*facing=2u;return 1;}
    return 0;
}
static int field_direction_primary_rel(uint8_t d,int* rel){
    /* DirectionalCollisionOffsets ($45C3A), converted to offsets from
     * Tile_Collision_Up_1. */
    static const int8_t r[16]={
        99,0,4,99,6,6,6,99,2,2,2,99,99,99,99,99
    };
    int q=r[d&15u];if(q==99)return 0;*rel=q;return 1;
}
static int field_tile_blocks_direction(uint8_t d,int* rel){
    if(!field_direction_primary_rel(d,rel))return 0;
    return collision_blocks(field_collision_ring(*rel));
}
static int field_resolve_tile_direction(uint8_t initial,uint8_t held,uint8_t* resolved){
    /* Bit-for-bit control-flow lowering of FieldObj_DoTileCollision
     * ($45B5A-$45C15), including its non-obvious first-collision retry.
     *
     * The old native translation had one systemic error: when both +1/+2
     * neighboring collision bytes had bit 3 clear, it immediately applied the
     * clockwise table.  The 68000 does NOT turn there; it retries the same
     * direction once, then reaches the source second-pass/opposite table if the
     * obstruction remains.  That mistaken eager turn made the player slide to
     * a different tile at straight walls/corners and could look like walking on
     * wall graphics.  Keep the source tables complete as well (entries $D/$E
     * are reachable from unusual simultaneous-pad states/emulator input).
     */
    static const uint8_t turn_cw[16]={0,8,4,0,1,1,1,0,2,2,2,0,0,4,8,0};
    static const uint8_t turn_ccw[16]={0,4,8,0,2,2,2,0,1,1,1,0,0,2,1,0};
    static const uint8_t turn_opposite[16]={0,2,1,0,0,8,8,8,4,4,4,0,0,2,6,0};
    uint8_t d=(uint8_t)(initial&15u);unsigned pass=0u;uint8_t neighborhood[8],neighborhood_valid=0u;*resolved=d;
    for(unsigned guard=0u;guard<8u;guard++){
        int rel=0;
        if(!field_direction_primary_rel(d,&rel)){*resolved=d;return 0;}
        uint8_t primary;
        if(!pass){
            primary=field_collision_ring(rel);unsigned idx=(unsigned)rel&7u;neighborhood[idx]=primary;neighborhood_valid=(uint8_t)(neighborhood_valid|(1u<<idx));
        }else primary=field_collision_ring_resolve_cached(rel,neighborhood,&neighborhood_valid);
        if(!collision_blocks(primary)){*resolved=d;return 1;}
        *resolved=d;
        if(held&0x10u)return 0; /* ButtonCancel: source returns blocked. */
        ++pass;
        if(pass==1u){
            /* Source starts with the clockwise table, but only consumes a turn
             * table when either forward-side bit3 probe is set.  If both are
             * clear it loops with the SAME d0; this was lost in v0.9.10.5. */
            int plus2=(field_collision_ring_resolve_cached(rel+2,neighborhood,&neighborhood_valid)&8u)!=0u;
            int plus1=(field_collision_ring_resolve_cached(rel+1,neighborhood,&neighborhood_valid)&8u)!=0u;
            if(!(plus2||plus1))continue;
            if(field_collision_ring_resolve_cached(rel-2,neighborhood,&neighborhood_valid)&8u)return 0;
            if(field_collision_ring_resolve_cached(rel-1,neighborhood,&neighborhood_valid)&8u)return 0;
            d=turn_ccw[d];
        }else if(pass==2u){
            d=turn_opposite[d];
        }else if(pass==3u){
            d=(field_collision_ring_resolve_cached(rel+1,neighborhood,&neighborhood_valid)&8u)?turn_ccw[d]:turn_cw[d];
        }else{
            /* The original falls back into the same retry loop for values >3;
             * valid source collision neighborhoods resolve by pass three.  A
             * finite guard is safer than hanging if corrupt data ever violates
             * that invariant. */
            continue;
        }
        *resolved=d;if(!d)return 0;
    }
    return 0;
}
static int field_resolve_move(uint8_t pad,int* dx,int* dy,uint8_t* facing){
    uint8_t initial=(uint8_t)(pad&0x0Fu),resolved=initial;
    if(!field_direction_vector(initial,dx,dy,facing))return 0;
    int clear=field_resolve_tile_direction(initial,pad,&resolved);
    /* FieldObj_Collided updates facing from the final d0 even when no movement
     * occurs, so keep the redirected facing on a failed final retry. */
    int rdx=0,rdy=0;uint8_t rface=*facing;
    if(field_direction_vector(resolved,&rdx,&rdy,&rface)){*dx=rdx;*dy=rdy;*facing=rface;}
    return clear;
}
static void begin_step(uint8_t pad){
    if(chaz.step_left)return;int dx=0,dy=0;uint8_t facing=chaz.facing;
    int clear=field_resolve_move(pad,&dx,&dy,&facing);
    chaz.facing=facing;
    if(!clear||field_object_blocks(chaz.x+dx*16,chaz.y+dy*16)||field_follower_blocks_lead(chaz.x+dx*16,chaz.y+dy*16)){chaz.anim=0u;chaz.anim_clock=0u;field_sync_native_party_to_source();return;}
    field_followers_begin_chain();chaz.dx=(int8_t)dx;chaz.dy=(int8_t)dy;chaz.step_left=16u;field_sync_native_party_to_source();
}
static void advance_step(void){
    if(!chaz.step_left){chaz.anim=0u;chaz.anim_clock=0u;field_followers_advance();return;}uint8_t before=chaz.step_left;unsigned n=chaz.step_left>=2u?2u:1u;chaz.x+=chaz.dx*(int)n;chaz.y+=chaz.dy*(int)n;chaz.step_left=(uint8_t)(chaz.step_left-n);field_party_anim_advance(lead_char_id,&chaz.anim,&chaz.anim_clock);field_followers_advance();if(before&&!chaz.step_left){field_sync_native_party_to_source();field_step_completed=1u;if(!ram16r(0xF43Cu))ram8w(0xECE5u,(uint8_t)(ram8r(0xECE5u)|2u));}
}

__attribute__((section(".iwram.text"),noinline))
static int field_xy_range(unsigned kind,int tx,int ty,int px,int py){
    switch(kind){case 0:return 0;case 1:return px>=tx&&px<tx+0x40&&py>=ty&&py<ty+0x40;case 2:return px>=tx&&px<tx+0x20&&py>=ty&&py<ty+0x20;case 3:return px==tx&&py==ty;case 4:return px<=tx;case 5:return px>=tx;case 6:return py<=ty;case 7:return py>=ty;case 8:return py>0x2A0&&px<=tx&&py<=ty;case 9:return px>=tx&&px<tx+0x20&&py>=ty&&py<ty+0x10;case 10:return px>=tx&&px<tx+0x10&&py>=ty&&py<ty+0x60;case 11:return px>=tx&&px<tx+0x40&&py>=ty&&py<ty+0x20;case 12:return px>=tx&&px<tx+0x10&&py>=ty&&py<ty+0x20;case 13:return px>=tx&&px<tx+0x60&&py>=ty&&py<ty+0x10;case 14:return px>=tx&&px<tx+0x40&&py>=ty&&py<ty+0x10;default:return 0;}
}
static int field_scan_transition_table(uint32_t p,struct FieldTransition* out){
    while(rom16(p)!=0xFFFFu){int tx=(int)rom8(p)*16,ty=(int)rom8(p+1u)*16;unsigned kind=rom16(p+2u);if(field_xy_range(kind,tx,ty,chaz.x,chaz.y)){out->map_id=rom16(p+4u);out->x=(uint16_t)(rom8(p+6u)*16u);out->y=(uint16_t)(rom8(p+7u)*16u);out->facing=(uint8_t)((rom8(p+8u)>>2)&3u);out->align=rom8(p+9u);return 1;}p+=10u;}return 0;
}
__attribute__((section(".iwram.text"),noinline))
static int field_scan_transition_plan(const Ps4PscTransition* p,unsigned count,struct FieldTransition* out){
    for(unsigned i=0u;i<count;i++){const Ps4PscTransition* t=&p[i];if(field_xy_range(t->kind,(int)t->tx,(int)t->ty,chaz.x,chaz.y)){out->map_id=t->map_id;out->x=t->x;out->y=t->y;out->facing=t->facing;out->align=t->align;return 1;}}return 0;
}
static void field_update_standing_collision(void){uint8_t old=ram8r(0xEC80u);ram8w(0xEC81u,old);ram8w(0xEC80u,field_collision_at(chaz.x,chaz.y,0,0));}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int field_find_transition(struct FieldTransition* out){
    if(chaz.step_left)return 0;uint8_t standing=ram8r(0xEC80u)&15u;if(standing==1u){if((ram8r(0xEC81u)&15u)==1u)return 0;return field_runtime_plan_valid?field_scan_transition_plan(field_trans2_plan,field_trans2_plan_count,out):field_scan_transition_table(field_map.trans2,out);}if(standing<=7u||standing>=13u)return field_runtime_plan_valid?field_scan_transition_plan(field_trans1_plan,field_trans1_plan_count,out):field_scan_transition_table(field_map.trans1,out);return 0;
}

/* -------------------------------------------------------------------------
 * Dialogue and manga presentation.
 *
 * Ordinary NPC dialogue never leaves the live Mode-0 field.  A 256x64 4bpp
 * OBJ surface occupies the otherwise-unused top quarter of OBJ VRAM and is
 * uploaded once per page.  This removes the old 38,400-pixel software field
 * recompose that caused the user-visible pause before every conversation.
 *
 * Manga panels are different: they are authored as 320x224 Genesis plane
 * compositions.  Render them at native source resolution into a compact 4bpp
 * EWRAM surface, composite Plane B then Plane A, and only then scale the whole
 * scene to 240x160.  Scaling tile-by-tile (v0.8.7) created seams/distortion.
 * ------------------------------------------------------------------------- */
static const uint8_t* field_dialogue_offset(const uint8_t* p,const uint8_t* end,unsigned count){while(count--&&p<end){while(p<end&&*p++!=0xFFu){}}return p;}
static const uint8_t* field_dialogue_entry(unsigned id){return field_dialogue_offset(field_dialogue,field_dialogue+field_dialogue_len,id);}
#ifndef PS4_DIALOGUE_F7_SEGMENTS
#define PS4_DIALOGUE_F7_SEGMENTS 1
#endif
/* Why the last dialogue run stopped: $F7 (segment, popdlg pending) or $FF. */
static uint8_t field_dialogue_last_stop;
static const uint8_t* field_dialogue_resolve(unsigned id){
    const uint8_t* end=field_dialogue+field_dialogue_len;const uint8_t* p=field_dialogue_entry(id);
    for(unsigned guard=0;guard<16u&&p+2u<end&&p[0]==0xFAu;guard++){unsigned flag=p[1],skip=p[2];if(ps4_event_flag_test(flag))p=field_dialogue_offset(p+2u,end,skip);else p+=3u;}return p;
}
static void pause_fill_rect(unsigned x0,unsigned y0,unsigned x1,unsigned y1,uint8_t c){if(x1>240u)x1=240u;if(y1>160u)y1=160u;for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++)field_pause_fb[y*240u+x]=c;}
/* PSIV's Dialogue Font is an exact 8x16 bitmap font.  Earlier GBA menus
 * accidentally used ASCII byte values as source-font indices and then squeezed
 * the 8 source columns into seven pixels.  That produced the corrupted text in
 * v0.9.6.  Keep encoded source strings encoded, and map only port-authored
 * ASCII labels through script/charset.asm. */
static void pause_draw_glyph8(unsigned x,unsigned y,uint8_t ch,uint8_t fg){
    uint32_t art=0x2A3542u+(uint32_t)(ch&0x7Fu)*16u;
    for(unsigned yy=0;yy<16u&&y+yy<160u;yy++){
        uint8_t bits=rom8(art+yy);
        for(unsigned xx=0;xx<8u&&x+xx<240u;xx++)if(bits&(uint8_t)(0x80u>>xx))field_pause_fb[(y+yy)*240u+(x+xx)]=fg;
    }
}
static uint8_t ps4_ascii_font_code(uint8_t ch){
    if(ch>='A'&&ch<='Z')return (uint8_t)(1u+ch-'A');
    if(ch>='a'&&ch<='z')return (uint8_t)(27u+ch-'a');
    if(ch>='0'&&ch<='9')return (uint8_t)(64u+ch-'0');
    if(ch==' ')return 0u;if(ch=='.')return 0x35u;if(ch=='\'')return 0x36u;if(ch==',')return 0x37u;
    if(ch==':')return 0x39u;if(ch=='!')return 0x3Au;if(ch=='?')return 0x3Bu;if(ch=='-')return 0x3Cu;if(ch=='%')return 0x3Fu;
    return 0u;
}
static void pause_draw_ascii_glyph(unsigned x,unsigned y,uint8_t ch,uint8_t fg){pause_draw_glyph8(x,y,ps4_ascii_font_code(ch),fg);}
static uint8_t ps4_window_ascii_code(uint8_t ch){
    /* wincharset.asm stores source character CODES, not zero-based art-tile
     * indices: A=1, 0=27, a=57, while code 0 is a space. */
    if(ch>='A'&&ch<='Z')return (uint8_t)(1u+ch-'A');if(ch>='a'&&ch<='z')return (uint8_t)(57u+ch-'a');if(ch>='0'&&ch<='9')return (uint8_t)(27u+ch-'0');
    if(ch==' ')return 0u;if(ch=='-')return 0x31u;if(ch=='!')return 0x32u;if(ch=='?')return 0x33u;if(ch==':')return 0x34u;if(ch=='.')return 0x53u;if(ch=='\'')return 0x54u;if(ch==',')return 0x55u;return 0u;
}
static int ps4_window_font_index(uint8_t code){
    /* ArtNem_Font is a dense 87-tile stream whose tile 0 is 'A'.  The source
     * charset deliberately reserves encoded value 0 for space, so every
     * drawable source code is one greater than its art-tile index.  v0.9.10.2
     * skipped this final code->art translation, producing ITEM -> JUEN, etc. */
    if(!code||code>87u)return -1;return (int)code-1;
}
static void pause_draw_window_glyph(unsigned x,unsigned y,uint8_t code,uint8_t fg){int gi=ps4_window_font_index(code);if(gi<0)return;const uint8_t* a=ps4_window_font_4bpp+(unsigned)gi*32u;for(unsigned yy=0u;yy<8u&&y+yy<160u;yy++)for(unsigned xx=0u;xx<8u&&x+xx<240u;xx++){uint8_t q=a[yy*4u+(xx>>1)],ci=(xx&1u)?(q&15u):(q>>4);if(ci==15u)field_pause_fb[(y+yy)*240u+x+xx]=fg;}}
static void pause_draw_window_ascii(unsigned x,unsigned y,uint8_t ch,uint8_t fg){pause_draw_window_glyph(x,y,ps4_window_ascii_code(ch),fg);}
static void pause_draw_cursor(unsigned x,unsigned y,uint8_t fg){
    /* Battle_UpdateRedCursor2 uses source window tiles $6E7 (blue) and $6E8
     * (red).  Render those exact tile patterns rather than the port-authored
     * triangle.  Translate their palette ramps into the adapter's reserved UI
     * colors so battle/background palette ownership remains disjoint. */
    unsigned tile=(fg>=83u)?1u:0u;const uint8_t* a=ps4_precomp_assets+PS4_PRECOMP_WINDOW_CURSOR_TILES_OFF+tile*32u;
    for(unsigned yy=0u;yy<8u&&y+yy<160u;yy++)for(unsigned xx=0u;xx<8u&&x+xx<240u;xx++){uint8_t v=a[yy*4u+(xx>>1)],ci=(xx&1u)?(v&15u):(v>>4);if(ci){uint8_t c=(ci==2u)?81u:(ci==14u?82u:fg);field_pause_fb[(y+yy)*240u+x+xx]=c;}}
}
static void pause_draw_slash(unsigned x,unsigned y,uint8_t fg){for(unsigned i=0;i<12u;i++){unsigned px=x+(11u-i)/2u,py=y+2u+i;if(px<240u&&py<160u)field_pause_fb[py*240u+px]=fg;}}
/* Compatibility name for source-encoded call sites. */
#define pause_draw_glyph7 pause_draw_glyph8
static void pause_draw_sprite_rom(uint32_t art,unsigned frame,int sx,int sy,unsigned pal_line,int hflip){for(unsigned y=0;y<32u;y++){int py=sy+(int)y;if(py<0||py>=160)continue;for(unsigned x=0;x<16u;x++){int px=sx+(int)x;if(px<0||px>=240)continue;unsigned ci=sprite_art_px_rom(art,frame,x,y,hflip);if(ci)field_pause_fb[(unsigned)py*240u+(unsigned)px]=(uint8_t)(pal_line*16u+ci);}}}
static void pause_capture_palette(void){for(unsigned i=0;i<256u;i++)field_pause_pal[i]=BG_PALETTE[i];}
static void pause_restore_field_palette(void){dma_copy32(field_gba_palette,(void*)BG_PALETTE,128u);}
static void dialogue_wait_press(void){const uint16_t mask=(uint16_t)((1u<<0)|(1u<<1)|KEY_START);while(((uint16_t)~REG_KEYINPUT)&mask)waitvb();while(!(((uint16_t)~REG_KEYINPUT)&mask))waitvb();while(((uint16_t)~REG_KEYINPUT)&mask)waitvb();}

/* ---- zero-stall Mode-0 dialogue OBJ surface ---- */
/* Separate from panel_art_scratch.  Hahn's manga path decompresses panel art
 * after the dialogue template is prepared; the former alias at $02020000
 * replaced the template with Nemesis data before the first glyph upload. */
static uint8_t* const dialogue_obj_buf=(uint8_t*)0x02038000;
#define DIALOGUE_MAIN_TILE_COUNT 160u
#define DIALOGUE_PORTRAIT_TILE_OFF 160u
#define DIALOGUE_PORTRAIT_TILE_COUNT 25u
#define DIALOGUE_CHOICE_TILE_OFF 192u
static uint8_t dialogue_portrait_id;
static uint8_t dialogue_choice_visible;
static void dialogue_obj_pixel(unsigned x,unsigned y,uint8_t c){
    /* Source-scaled 256x40 logical dialogue canvas.  The first 32 rows are
     * four 64x32 OBJs (tiles 0..127); the last eight are eight 32x8 OBJs
     * (tiles 128..159).  This leaves the rest of the already-reserved OBJ
     * range available for source portraits and the Yes/No window. */
    if(x>=256u||y>=40u)return;unsigned lx,ly,tile;
    if(y<32u){unsigned seg=x>>6;lx=x&63u;ly=y;tile=seg*32u+(ly>>3)*8u+(lx>>3);}
    else{unsigned seg=x>>5;lx=x&31u;ly=y-32u;tile=128u+seg*4u+(lx>>3);}
    unsigned off=tile*32u+(ly&7u)*4u+((lx&7u)>>1);uint8_t v=dialogue_obj_buf[off];
    if(lx&1u)v=(uint8_t)((v&0x0Fu)|(c<<4));else v=(uint8_t)((v&0xF0u)|(c&15u));dialogue_obj_buf[off]=v;
}
static void dialogue_choice_pixel(unsigned x,unsigned y,uint8_t c){
    if(x>=64u||y>=32u)return;unsigned tile=DIALOGUE_CHOICE_TILE_OFF+(y>>3)*8u+(x>>3),off=tile*32u+(y&7u)*4u+((x&7u)>>1);uint8_t v=dialogue_obj_buf[off];
    if(x&1u)v=(uint8_t)((v&0x0Fu)|(c<<4));else v=(uint8_t)((v&0xF0u)|(c&15u));dialogue_obj_buf[off]=v;
}
static void dialogue_obj_rect(unsigned x0,unsigned y0,unsigned x1,unsigned y1,uint8_t c){for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++)dialogue_obj_pixel(x,y,c);}
static void dialogue_obj_glyph(unsigned x,unsigned y,uint8_t ch){
    uint32_t art=0x2A3542u+(uint32_t)(ch&0x7Fu)*16u;
    for(unsigned yy=0;yy<16u;yy++){uint8_t bits=rom8(art+yy);for(unsigned xx=0;xx<8u;xx++)if(bits&(uint8_t)(0x80u>>xx))dialogue_obj_pixel(x+xx,y+yy,2u);}
}
/* Dialogue is authored for a 320x224 surface: TextBufferToPlane places its
 * 32-column text buffer at source x=$20 and y=$A8.  A native 8-pixel glyph
 * cannot preserve that 32-character line on a 240-pixel GBA display.  Scale
 * only the presentation glyph (8x16 -> 6x11), retaining every source byte and
 * source line/page boundary. */
#include "r29_dialogue_font.h"
static void dialogue_obj_source_glyph(unsigned x,unsigned y,uint8_t ch){
    const uint8_t* glyph=r29_dialogue_font[ch&0x7Fu];
    for(unsigned dy=0u;dy<11u;dy++)for(unsigned dx=0u;dx<6u;dx++)if(glyph[dy]&(0x80u>>dx))dialogue_obj_pixel(x+dx,y+dy,2u);
}
static void dialogue_choice_source_glyph(unsigned x,unsigned y,uint8_t ch){
    const uint8_t* glyph=r29_dialogue_font[ch&0x7Fu];
    for(unsigned dy=0u;dy<11u;dy++)for(unsigned dx=0u;dx<6u;dx++)if(glyph[dy]&(0x80u>>dx))dialogue_choice_pixel(x+dx,y+dy,2u);
}
static void dialogue_obj_set_portrait(uint8_t id){
    dialogue_portrait_id=id;
    if(id>=1u&&id<=PS4_PRECOMP_DIALOGUE_PORTRAIT_COUNT){
        const uint8_t* src=ps4_precomp_assets+PS4_PRECOMP_DIALOGUE_PORTRAIT_PIXELS_OFF+(uint32_t)(id-1u)*PS4_PRECOMP_DIALOGUE_PORTRAIT_FRAME_SIZE;
        dma_copy32(src,dialogue_obj_buf+DIALOGUE_PORTRAIT_TILE_OFF*32u,PS4_PRECOMP_DIALOGUE_PORTRAIT_FRAME_SIZE);
    }else{
        dialogue_portrait_id=0u;dma_fill32(dialogue_obj_buf+DIALOGUE_PORTRAIT_TILE_OFF*32u,DIALOGUE_PORTRAIT_TILE_COUNT*32u,0u);
    }
}
static void dialogue_obj_begin(void){
    /* Build-time source lowering: exact source window tiles plus an empty
     * portrait reservation and source-scaled $F5 choice window. */
    dma_copy32(ps4_precomp_assets+PS4_PRECOMP_DIALOGUE_TEMPLATE_OFF,dialogue_obj_buf,DIALOGUE_MAIN_TILE_COUNT*32u);
    if(dialogue_portrait_id)dialogue_obj_set_portrait(dialogue_portrait_id);
}
static void dialogue_obj_show(void){
    /* Palette line 15 is dialogue chrome/text. Portrait source mapping $C55C
     * uses MD palette line 2, mirrored into reserved GBA OBJ palette line 14. */
    waitvb();
    /* P128-style sparse immutable upload: the ordinary dialogue page owns only
     * relative tiles 0..159. Portrait and Yes/No ranges are disjoint and are
     * transferred only when actually visible. */
    dma_copy32(dialogue_obj_buf,(void*)((uintptr_t)OBJ_VRAM16+DIALOGUE_OBJ_TILE_BASE*32u),DIALOGUE_MAIN_TILE_COUNT*32u);
    if(dialogue_portrait_id)dma_copy32(dialogue_obj_buf+DIALOGUE_PORTRAIT_TILE_OFF*32u,(void*)((uintptr_t)OBJ_VRAM16+(DIALOGUE_OBJ_TILE_BASE+DIALOGUE_PORTRAIT_TILE_OFF)*32u),DIALOGUE_PORTRAIT_TILE_COUNT*32u);
    if(dialogue_choice_visible)dma_copy32(dialogue_obj_buf+DIALOGUE_CHOICE_TILE_OFF*32u,(void*)((uintptr_t)OBJ_VRAM16+(DIALOGUE_OBJ_TILE_BASE+DIALOGUE_CHOICE_TILE_OFF)*32u),32u*32u);
    OBJ_PALETTE[240u]=0u;
    OBJ_PALETTE[241u]=md2gba(ps4_window_md_palette[14u]);
    OBJ_PALETTE[242u]=md2gba(ps4_window_md_palette[15u]);
    OBJ_PALETTE[243u]=md2gba(ps4_window_md_palette[3u]);
    OBJ_PALETTE[244u]=md2gba(ps4_window_md_palette[2u]);
    for(unsigned i=0u;i<16u;i++)OBJ_PALETTE[224u+i]=md2gba(cram[32u+i]);
    /* Main source window: four 64x32 rows followed by eight 32x8 tail OBJs. */
    for(unsigned s=0u;s<4u;s++){unsigned oi=DIALOGUE_OAM_BASE+s;
        OAM16[oi*4u]=(uint16_t)(112u|0x4000u);OAM16[oi*4u+1u]=(uint16_t)((s*64u)|0xC000u);
        OAM16[oi*4u+2u]=(uint16_t)((DIALOGUE_OBJ_TILE_BASE+s*32u)|0xF000u);OAM16[oi*4u+3u]=0u;
    }
    for(unsigned s=0u;s<8u;s++){unsigned oi=DIALOGUE_OAM_BASE+4u+s;
        OAM16[oi*4u]=(uint16_t)(144u|0x4000u);OAM16[oi*4u+1u]=(uint16_t)((s*32u)|0x4000u);
        OAM16[oi*4u+2u]=(uint16_t)((DIALOGUE_OBJ_TILE_BASE+128u+s*4u)|0xF000u);OAM16[oi*4u+3u]=0u;
    }
    if(dialogue_portrait_id){
        unsigned oi=DIALOGUE_OAM_BASE+12u;
        OAM16[oi*4u]=74u;OAM16[oi*4u+1u]=(uint16_t)(30u|0x8000u);OAM16[oi*4u+2u]=(uint16_t)((DIALOGUE_OBJ_TILE_BASE+160u)|0xE000u);OAM16[oi*4u+3u]=0u;
        oi++;OAM16[oi*4u]=(uint16_t)(74u|0x8000u);OAM16[oi*4u+1u]=(uint16_t)(62u|0x4000u);OAM16[oi*4u+2u]=(uint16_t)((DIALOGUE_OBJ_TILE_BASE+176u)|0xE000u);OAM16[oi*4u+3u]=0u;
        oi++;OAM16[oi*4u]=(uint16_t)(106u|0x4000u);OAM16[oi*4u+1u]=(uint16_t)(30u|0x4000u);OAM16[oi*4u+2u]=(uint16_t)((DIALOGUE_OBJ_TILE_BASE+180u)|0xE000u);OAM16[oi*4u+3u]=0u;
        oi++;OAM16[oi*4u]=106u;OAM16[oi*4u+1u]=62u;OAM16[oi*4u+2u]=(uint16_t)((DIALOGUE_OBJ_TILE_BASE+184u)|0xE000u);OAM16[oi*4u+3u]=0u;
    }else for(unsigned s=12u;s<16u;s++)hw_hide_obj(DIALOGUE_OAM_BASE+s);
    if(dialogue_choice_visible){
        unsigned oi=DIALOGUE_CHOICE_OAM;OAM16[oi*4u]=(uint16_t)(80u|0x4000u);OAM16[oi*4u+1u]=(uint16_t)(160u|0xC000u);
        OAM16[oi*4u+2u]=(uint16_t)((DIALOGUE_OBJ_TILE_BASE+DIALOGUE_CHOICE_TILE_OFF)|0xF000u);OAM16[oi*4u+3u]=0u;
    }else hw_hide_obj(DIALOGUE_CHOICE_OAM);
}
static void dialogue_obj_hide(void){for(unsigned s=0;s<16u;s++)hw_hide_obj(DIALOGUE_OAM_BASE+s);hw_hide_obj(DIALOGUE_CHOICE_OAM);dialogue_choice_visible=0u;}
static void dialogue_obj_page_wait(void){dialogue_obj_show();dialogue_wait_press();}
static void dialogue_obj_ascii_text(unsigned x,unsigned y,const char* text,uint8_t color){
    for(;*text&&x<248u;text++,x+=8u)dialogue_obj_glyph(x,y,ps4_ascii_font_code((uint8_t)*text));
    (void)color;
}
/* FieldRoutine_ItemFound is a live-field window state in the source.  Keep
 * chest/item feedback in Mode 0 over the current field instead of entering the
 * full-screen Mode-4 menu surface.  Use the exact PSIV window font, projected
 * to 6x8 like battle UI, while reusing the already source-derived dialogue
 * window OBJ carrier/chrome. */
static void dialogue_obj_window_source_glyph6(unsigned x,unsigned y,uint8_t code){
    if((code>=1u&&code<=36u)||(code>=0x31u&&code<=0x52u)||code==0x53u||code==0x54u||code==0x55u){
        const uint8_t* a=r28_font5x7[code];
        for(unsigned dx=0u;dx<5u;dx++)for(unsigned dy=0u;dy<7u;dy++)if(a[dx]&(1u<<dy))dialogue_obj_pixel(x+dx,y+dy,2u);return;
    }
    int gi=ps4_window_font_index(code);if(gi<0)return;const uint8_t* a=ps4_window_font_4bpp+(unsigned)gi*32u;
    for(unsigned dy=0u;dy<8u;dy++)for(unsigned dx=0u;dx<6u;dx++){unsigned sx=(dx*8u)/6u;uint8_t q=a[dy*4u+(sx>>1)],ci=(sx&1u)?(q&15u):(q>>4);if(ci==15u)dialogue_obj_pixel(x+dx,y+dy,2u);}
}
static unsigned dialogue_obj_window_ascii_text6(unsigned x,unsigned y,const char* text){
    for(;*text&&x<238u;text++,x+=6u)dialogue_obj_window_source_glyph6(x,y,ps4_window_ascii_code((uint8_t)*text));return x;
}
static unsigned dialogue_obj_window_rom_name6(unsigned x,unsigned y,uint32_t base,unsigned index,uint8_t delim){
    uint32_t p=base;while(index--){while(p<0x300000u&&rom8(p)!=delim)++p;if(p<0x300000u)++p;}
    while(p<0x300000u&&rom8(p)!=delim&&x<238u){dialogue_obj_window_source_glyph6(x,y,rom8(p++));x+=6u;}return x;
}
static unsigned dialogue_obj_window_uint7(unsigned x,unsigned y,uint32_t v){
    static const uint32_t place[7]={1000000u,100000u,10000u,1000u,100u,10u,1u};int started=0;if(v>9999999u)v=9999999u;
    for(unsigned i=0u;i<7u;i++){unsigned d=0u;while(v>=place[i]){v-=place[i];++d;}if(d||started||i==6u){dialogue_obj_window_source_glyph6(x,y,(uint8_t)(27u+d));x+=6u;started=1;}}return x;
}
static void dialogue_choice_ascii_text(unsigned x,unsigned y,const char* text){for(;*text&&x<58u;text++,x+=6u)dialogue_choice_source_glyph(x,y,ps4_ascii_font_code((uint8_t)*text));}
static void dialogue_obj_choice_cursor(unsigned x,unsigned y){
    for(unsigned yy=0u;yy<9u;yy++){unsigned w=yy<5u?yy+1u:9u-yy;for(unsigned xx=0u;xx<w;xx++)dialogue_choice_pixel(x+xx,y+yy,2u);}
}
/* RunText $F5. PSIV's source WinGroup_Dialogue entry 3 is independently
 * packed at its globally-scaled screen position, so Yes/No never has to grow
 * or overwrite the main 32-column dialogue surface. */
static void dialogue_choice_rebuild(unsigned sel){
    dma_copy32(ps4_precomp_assets+PS4_PRECOMP_DIALOGUE_TEMPLATE_OFF+DIALOGUE_CHOICE_TILE_OFF*32u,dialogue_obj_buf+DIALOGUE_CHOICE_TILE_OFF*32u,32u*32u);
    dialogue_choice_ascii_text(12u,3u,"YES");dialogue_choice_ascii_text(12u,16u,"NO");dialogue_obj_choice_cursor(5u,4u+sel*13u);
}
static void dialogue_choice_refresh(void){
    /* The main dialogue/portrait OBJ ranges are immutable while $F5 owns the
     * selector.  Preserve one source/display tick but republish only the 1 KiB
     * choice range when its cursor changes. */
    waitvb();
    dma_copy32(dialogue_obj_buf+DIALOGUE_CHOICE_TILE_OFF*32u,
               (void*)((uintptr_t)OBJ_VRAM16+(DIALOGUE_OBJ_TILE_BASE+DIALOGUE_CHOICE_TILE_OFF)*32u),32u*32u);
}
static unsigned dialogue_yes_no_select(void){
    unsigned sel=0u;dialogue_choice_visible=1u;while(md_io_read_psiv_pad()&0xF0u)waitvb();uint8_t prev=md_io_read_psiv_pad();
    dialogue_choice_rebuild(sel);dialogue_obj_show();
    for(;;){
        uint8_t now=md_io_read_psiv_pad(),e=(uint8_t)(now&~prev);prev=now;
        if(e&0x03u){sel^=1u;dialogue_choice_rebuild(sel);dialogue_choice_refresh();}
        else if(e&0xE0u){while(md_io_read_psiv_pad()&0xF0u)waitvb();break;}
        else if(e&0x10u){sel=1u;while(md_io_read_psiv_pad()&0xF0u)waitvb();break;}
        else waitvb();
    }
    dialogue_choice_visible=0u;hw_hide_obj(DIALOGUE_CHOICE_OAM);return sel;
}
static const uint8_t* dialogue_skip_ff_entries(const uint8_t* p,const uint8_t* end,unsigned count){
    while(count--&&p<end){while(p<end&&*p!=0xFFu)++p;if(p<end)++p;}return p;
}

/* ---- globally-scaled manga panel compositor ----
 * Render directly into the 240x160 8bpp backbuffer, but choose pixels through
 * the same global 320x224 -> 240x160 source-coordinate map used by the title.
 * This preserves tile adjacency; unlike v0.8.7 no tile is independently 8->6
 * scaled, so boundaries cannot drift or overlap. */
static unsigned panel_art_px(const uint8_t* art,unsigned art_bytes,unsigned dest_tile,uint16_t e,unsigned x,unsigned y){
    if(e&0x0800u)x=7u-x;if(e&0x1000u)y=7u-y;unsigned src=e&0x07FFu;if(src<dest_tile)return 0u;unsigned tile=src-dest_tile,off=tile*32u+y*4u+(x>>1);if(off>=art_bytes)return 0u;uint8_t v=art[off];return (x&1u)?(v&15u):(v>>4);
}
static void panel_clear_scaled(void){dma_fill32(field_pause_fb,240u*160u,0u);}
static void panel_render_layer_scaled(uint16_t dest_tile,unsigned art_bytes,unsigned w,unsigned h,unsigned px,unsigned py,uint16_t base,uint32_t palptr,uint32_t mapptr){
    if(base==0xFFFFu||!mapptr)return;unsigned pl=(base>>13)&3u;for(unsigned i=0;i<16u;i++)field_pause_pal[pl*16u+i]=md2gba(rom16(palptr+i*2u));
    unsigned n=enigma_decode(mapptr,base,panel_map_scratch,2048u),need=w*h;if(n<need)need=n;unsigned x1=px+w*8u,y1=py+h*8u;
    for(unsigned dy=0u;dy<160u;dy++){unsigned sy=src_y[dy];if(sy<py||sy>=y1)continue;unsigned ly=sy-py,ty=ly>>3,iy=ly&7u;
        for(unsigned dx=0u;dx<240u;dx++){unsigned sx=src_x[dx];if(sx<px||sx>=x1)continue;unsigned lx=sx-px,tx=lx>>3,ix=lx&7u,pos=ty*w+tx;if(pos>=need)continue;uint16_t e=panel_map_scratch[pos];unsigned ci=panel_art_px(panel_art_scratch,art_bytes,dest_tile,e,ix,iy);if(ci)field_pause_fb[dy*240u+dx]=(uint8_t)(((e>>13)&3u)*16u+ci);}
    }
}
static void panel_panel_geometry(unsigned id,unsigned* w,unsigned* h,unsigned* px,unsigned* py){
    unsigned group=id>>6;uint32_t base=rom32(0x6B2B6u+group*4u);if(base>=0x300000u){*w=*h=*px=*py=0u;return;}
    uint32_t r=base+(id&63u)*0x1Eu;*w=(unsigned)rom8(r+6u);*h=(unsigned)rom8(r+7u);*px=(unsigned)rom8(r+8u)*8u;*py=(unsigned)rom8(r+9u)*8u;
}
/* The source backs up the underlying Plane A/B cells before each panel and
 * restores them on destroy.  A literal per-panel 240x160 pixel backup is much
 * larger than the Genesis tilemap state and does not fit GBA EWRAM.  Preserve
 * the same visible semantics with one immutable pre-panel framebuffer in Mode-4
 * page 0, then rebuild the surviving panel stack in order after a pop.  Page 1
 * remains the displayed manga page, so this consumes no additional EWRAM. */
static void panel_capture_base_scaled(void){
    waitvb();REG_DISPCNT=FORCE_BLANK;dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,240u*160u);
}
static void panel_rebuild_scaled(void);
static int panel_render_precompiled(unsigned id){
    const Ps4PscPanelDesc* d=ps4_psc_panel_get((uint16_t)id);if(!d)return 0;
    const uint8_t* palb=ps4_psc_panel_bank+ps4_psc_panel_palette_byte_offset+(uint32_t)d->palette_word_offset*2u;
    for(unsigned pl=0u;pl<4u;pl++)if(d->palette_mask&(1u<<pl)){
        for(unsigned i=0u;i<16u;i++){field_pause_pal[pl*16u+i]=(uint16_t)((uint16_t)palb[0]|((uint16_t)palb[1]<<8));palb+=2;}
    }
    const uint8_t* q=ps4_psc_panel_bank+d->stream_offset,*end=q+d->stream_length;
    while(q+3u<=end){unsigned off=(unsigned)q[0]|((unsigned)q[1]<<8),n=q[2];q+=3;if(q+n>end||off+n>240u*160u)return 1;for(unsigned i=0u;i<n;i++)field_pause_fb[off+i]=q[i];q+=n;}
    return 1;
}
static void panel_render_scaled(unsigned id){
    if(panel_render_precompiled(id))return;
    unsigned group=id>>6;uint32_t base=rom32(0x6B2B6u+group*4u);if(base>=0x300000u)return;uint32_t r=base+(id&63u)*0x1Eu;uint16_t dest=rom16(r);uint32_t art=rom32(r+2u);unsigned w=(unsigned)rom8(r+6u),h=(unsigned)rom8(r+7u),px=(unsigned)rom8(r+8u)*8u,py=(unsigned)rom8(r+9u)*8u;
    unsigned art_bytes=nemesis_fast(art,panel_art_scratch,PANEL_ART_SCRATCH_SIZE);if(!art_bytes||!w||!h)return;
    panel_render_layer_scaled(dest,art_bytes,w,h,px,py,rom16(r+20u),rom32(r+22u),rom32(r+26u));
    panel_render_layer_scaled(dest,art_bytes,w,h,px,py,rom16(r+10u),rom32(r+12u),rom32(r+16u));
}
static void panel_rebuild_scaled(void){
    dma_copy32((const void*)MODE4_VRAM16,field_pause_fb,240u*160u);
    for(unsigned i=0u;i<field_active_panel_count;i++)panel_render_scaled(field_active_panels[i]);
}
static void presentation_hide_field_oam(void){for(unsigned oi=0u;oi<DIALOGUE_OAM_BASE;oi++)hw_hide_obj(oi);}
static uint8_t ps4_menu_base_cached; /* defined with the menu state below */
static void panel_present_scaled(void){presentation_hide_field_oam();ps4_menu_base_cached=0u;dma_copy32(field_pause_fb,(void*)MODE4_PAGE1,240u*160u);waitvb();dma_copy32(field_pause_pal,(void*)BG_PALETTE,512u);REG_DISPCNT=(uint16_t)(MODE4|MODE4_FRAME1|BG2_ENABLE|OBJ_1D|OBJ_ENABLE);}
static void pause_panel_action(unsigned action,unsigned value){
    if(!field_cutscene_panels){field_cutscene_panels=1u;field_active_panel_count=0u;dma_fill32(field_pause_pal,512u,0u);panel_clear_scaled();panel_capture_base_scaled();}
    if(action==0u){if(field_active_panel_count<FIELD_PANEL_STACK_MAX){field_active_panels[field_active_panel_count++]=(uint16_t)value;panel_render_scaled((uint16_t)value);panel_present_scaled();}}
    else if(action==1u){if(field_active_panel_count){--field_active_panel_count;panel_rebuild_scaled();panel_present_scaled();}}
    else if(action==2u){field_active_panel_count=0u;panel_rebuild_scaled();panel_present_scaled();}
}

/* RunText $F2 TextActionsOffs entries 3-C, reimplemented from the pinned
 * US PSIV disassembly. These are shared by decompressed and direct-ROM text
 * paths so manga/cutscene timing and side effects cannot diverge. */
static void field_dialogue_commit_palette(void){
    field_palette_mirror_live(64u);hw_load_palettes();
    if(field_cutscene_panels)for(unsigned i=0u;i<64u;i++)BG_PALETTE[i]=field_pause_pal[i];
}
static void field_dialogue_text_action(unsigned action,unsigned value){
    if(action==3u){
        if(value)ram8w(0x500Au,(uint8_t)value);
        waitvb();return;
    }
    if(action==4u){ram8w(0x500Au,(uint8_t)value);waitvb();return;}
    if(action==5u)return;
    if(action==6u){
        field_reload_palette_from_map();field_dialogue_commit_palette();waitvb();return;
    }
    if(action==7u){
        static const uint8_t waits[7]={3u,3u,0x13u,3u,3u,9u,0xFFu};
        ram8w(0xECF2u,0u);ram8w(0x500Au,0xCBu);
        for(unsigned phase=0u;phase<7u&&waits[phase]!=0xFFu;phase++){
            for(unsigned i=0u;i<4u;i++)cram[12u+i]=rom16(0x6A674u+phase*8u+i*2u);
            field_dialogue_commit_palette();
            for(unsigned f=0u;f<=(unsigned)waits[phase];f++)waitvb();
            ram8w(0xECF2u,(uint8_t)(phase+1u));
        }
        return;
    }
    if(action==8u){
        field_dialogue_paused_music=ps4_audio_current_music();
        ps4_audio_stop();waitvb();return;
    }
    if(action==9u){
        if(field_dialogue_paused_music>=0x81u&&field_dialogue_paused_music<=0xB4u)
            ps4_audio_play(field_dialogue_paused_music);
        field_dialogue_paused_music=0u;waitvb();return;
    }
    if(action==0x0Au){
        for(unsigned cycle=0u;cycle<3u;cycle++){
            ram8w(0xED52u,0u);ram8w(0x500Au,0xDBu);field_palette_alarm_down(0u);
            ram8w(0xED52u,0u);ram8w(0x500Au,0xDBu);field_palette_alarm_restore(0u);
        }
        return;
    }
    if(action==0x0Bu){ps4_event_flag_set(value&0xFFu);return;}
    if(action==0x0Cu){
        field_palette_save(64u);
        for(unsigned f=0u;f<20u;f++){if((f&3u)==0u)field_palette_increase_tone_once();field_dialogue_commit_palette();waitvb();}
        for(unsigned f=0u;f<20u;f++){if((f&3u)==0u)field_palette_restore_step();field_dialogue_commit_palette();waitvb();}
        return;
    }
}
static int dialogue_has_panel_actions(const uint8_t* p,const uint8_t* end){
    /* Only LoadPanel ($F2,$00,word) requires bitmap presentation.  Earlier
     * builds treated DestroyLast/DestroyAll as proof that a panel existed and
     * also scanned parameter bytes as opcodes.  Ordinary NPC strings could
     * therefore switch to a blank Mode-4 page: the user-visible white-screen
     * flash.  Parse RunText control widths exactly enough to classify safely. */
    for(const uint8_t* q=p;q<end;){
        uint8_t op=*q++; if(op==0xFFu)break;
        if(op==0xF2u&&q<end){uint8_t a=*q++;if(a==0u){if(q+1u<end)return 1;q=end;continue;}if(a==3u||a==4u||a==0x0Bu){if(q<end)++q;}continue;}
        if(op==0xF4u||op==0xF9u){if(q<end)++q;continue;}
        if(op==0xF5u){if(q+1u<end)q+=2u;else q=end;continue;}
        if(op==0xFAu||op==0xF6u){if(q+1u<end)q+=2u;continue;}
    }
    return 0;
}
enum { FIELD_DIALOGUE_NORMAL=0u, FIELD_DIALOGUE_KEEP_PANELS=1u };
static int field_dialogue_run_mode(const uint8_t* p,unsigned camx,unsigned camy,unsigned mode){
    const uint8_t* end=field_dialogue+field_dialogue_len;if(!p||p>=end)return 0;
    if(p+2u<end&&p[0]==0xF6u)return ((int)p[1]<<8)|p[2];
    int panels=dialogue_has_panel_actions(p,end);
    if(mode==FIELD_DIALOGUE_NORMAL){field_cutscene_panels=0u;field_active_panel_count=0u;if(panels){dma_fill32(field_pause_pal,512u,0u);panel_clear_scaled();}}
    dialogue_obj_begin();unsigned col=0u,line=0u;
    for(;p<end;p++){uint8_t q=*p;
        if(q==0xFFu){dialogue_obj_page_wait();field_dialogue_saved_cursor=p+1u;field_dialogue_last_stop=0xFFu;break;}
#if PS4_DIALOGUE_F7_SEGMENTS
        /* R16: $F7 = TextCtrlCode_Terminate3 -> Terminate2 (pushdlg; rts).  The
         * source stops the segment here and saves the byte after $F7; the event
         * then acts (facing/moves/panels) and popdlg+Event_RunDialogue resumes.
         * R15 swallowed $F7 (q>=0xF0 -> continue), ran every segment at once,
         * stopped at $FF (cursor = NEXT entry) and each later resume played the
         * following dialogue entry -- e.g. AfterIgglanova ($25, tree 33 $12)
         * resumed into $13 = the Principal's confession. */
        if(q==0xF7u){dialogue_obj_page_wait();field_dialogue_saved_cursor=p+1u;field_dialogue_last_stop=0xF7u;break;}
#endif
        if(q==0xFDu){dialogue_obj_page_wait();dialogue_obj_begin();col=0u;line=0u;continue;}
        if(q==0xFCu){col=0u;if(line<1u)++line;else{dialogue_obj_page_wait();dialogue_obj_begin();line=0u;}continue;}
        if(q==0xF4u){if(p+1u<end)dialogue_obj_set_portrait(*++p);continue;}
        if(q==0xF2u){if(p+1u<end){uint8_t action=*++p;if(action==0u){if(p+2u<end){unsigned value=((unsigned)p[1]<<8)|p[2];p+=2u;pause_panel_action(0u,value);}}else if(action==1u||action==2u){pause_panel_action(action,0u);}else if(action==3u||action==4u||action==0x0Bu){unsigned value=0u;if(p+1u<end)value=*++p;field_dialogue_text_action(action,value);}else if(action<=0x0Cu)field_dialogue_text_action(action,0u);}continue;}
        if(q==0xF9u){if(p+1u<end)++p;continue;}if(q==0xFAu){if(p+2u<end)p+=2u;continue;}
        if(q==0xF5u){if(p+2u<end){uint8_t yes_skip=p[1],no_skip=p[2];unsigned sel=dialogue_yes_no_select();ram8w(0xED53u,(uint8_t)sel);const uint8_t* r=dialogue_skip_ff_entries(p+3u,end,sel?no_skip:yes_skip);p=r>field_dialogue?r-1u:p+2u;}continue;}
        if(q>=0xF0u)continue;
        if(col>=32u){col=0u;if(line<1u)++line;else{dialogue_obj_page_wait();dialogue_obj_begin();line=0u;}}
        dialogue_obj_source_glyph(24u+col*6u,8u+line*12u,q);++col;
    }
    dialogue_obj_hide();
    if(mode==FIELD_DIALOGUE_NORMAL&&panels){
        /* Mode-4 page 1 aliases physical OBJ VRAM. Reconstruct the physical
         * field presentation from the persistent virtual MD VRAM before any
         * pose-cache fast path can incorrectly decide actor art is unchanged. */
        waitvb();hw_field_init(camx,camy);field_cutscene_panels=0u;field_active_panel_count=0u;
    }
    return 0;
}
static int field_dialogue_run_resolved(const uint8_t* p,unsigned camx,unsigned camy){return field_dialogue_run_mode(p,camx,camy,FIELD_DIALOGUE_NORMAL);}
static int field_dialogue_show(unsigned id,unsigned camx,unsigned camy){
    field_dialogue_saved_cursor=0;dialogue_portrait_id=0u;dialogue_choice_visible=0u;return field_dialogue_run_resolved(field_dialogue_resolve(id),camx,camy);
}
/* Event_RunDialogue2 deliberately resets logical source dialogue/window
 * counters without Panel_DestroyAll.  Keep the native visual panel stack and
 * bitmap presentation alive so Saving Kyra can flow directly into battle. */
static int field_dialogue_show2(unsigned id,unsigned camx,unsigned camy){
    field_dialogue_saved_cursor=0;dialogue_portrait_id=0u;dialogue_choice_visible=0u;return field_dialogue_run_mode(field_dialogue_resolve(id),camx,camy,FIELD_DIALOGUE_KEEP_PANELS);
}
/* Native cutscenes can create manga panels outside RunText, exactly like the
 * source Panel_Create calls.  These entry points preserve that stack while the
 * following dialogue is rendered/resumed. */
static int field_dialogue_show_keep_panels(unsigned id,unsigned camx,unsigned camy){
    field_dialogue_saved_cursor=0;dialogue_portrait_id=0u;dialogue_choice_visible=0u;
    return field_dialogue_run_mode(field_dialogue_resolve(id),camx,camy,FIELD_DIALOGUE_KEEP_PANELS);
}
/* $5AC6C is the common RunText body entered with A0 already resolved.  Several
 * source EventPtrs rebuild A0 from Saved_Dialogue_Addr ($ECF0) to continue at
 * the entry immediately following the prior $FF terminator. */
static int field_dialogue_resume_saved(unsigned camx,unsigned camy){
    const uint8_t* p=field_dialogue_saved_cursor;field_dialogue_saved_cursor=0;return field_dialogue_run_resolved(p,camx,camy);
}
static int field_dialogue_resume_saved_keep_panels(unsigned camx,unsigned camy){
    const uint8_t* p=field_dialogue_saved_cursor;field_dialogue_saved_cursor=0;
    return field_dialogue_run_mode(p,camx,camy,FIELD_DIALOGUE_KEEP_PANELS);
}

static int field_dialogue_show_rom_ptr(uint32_t rp,unsigned camx,unsigned camy){
    (void)camx;(void)camy;if(rp>=0x300000u)return 0;
    if(rom8(rp)==0xF6u&&rp+2u<0x300000u)return ((int)rom8(rp+1u)<<8)|rom8(rp+2u);
    dialogue_portrait_id=0u;dialogue_choice_visible=0u;dialogue_obj_begin();unsigned col=0u,line=0u;
    for(unsigned guard=0u;guard<1024u&&rp<0x300000u;guard++,rp++){
        uint8_t q=rom8(rp);
        if(q==0xFFu){dialogue_obj_page_wait();break;}
        if(q==0xFDu){dialogue_obj_page_wait();dialogue_obj_begin();col=0u;line=0u;continue;}
        if(q==0xFCu){col=0u;if(line<1u)++line;else{dialogue_obj_page_wait();dialogue_obj_begin();line=0u;}continue;}
        if(q==0xF4u){if(rp+1u<0x300000u)dialogue_obj_set_portrait(rom8(++rp));continue;}
        if(q==0xF9u){if(rp+1u<0x300000u)++rp;continue;}
        if(q==0xFAu){if(rp+2u<0x300000u)rp+=2u;continue;}
        if(q==0xF5u){if(rp+2u<0x300000u){uint8_t yes_skip=rom8(rp+1u),no_skip=rom8(rp+2u);unsigned sel=dialogue_yes_no_select();ram8w(0xED53u,(uint8_t)sel);uint32_t r=rp+3u;unsigned skip=sel?no_skip:yes_skip;while(skip--&&r<0x300000u){while(r<0x300000u&&rom8(r)!=0xFFu)++r;if(r<0x300000u)++r;}rp=r? r-1u : rp+2u;}continue;}
        if(q==0xF2u){if(rp+1u<0x300000u){uint8_t a=rom8(++rp);if(a==0u&&rp+2u<0x300000u){unsigned value=((unsigned)rom8(rp+1u)<<8)|rom8(rp+2u);rp+=2u;pause_panel_action(0u,value);}else if(a==1u||a==2u)pause_panel_action(a,0u);else if(a==3u||a==4u||a==0x0Bu){unsigned value=0u;if(rp+1u<0x300000u)value=rom8(++rp);field_dialogue_text_action(a,value);}else if(a<=0x0Cu)field_dialogue_text_action(a,0u);}continue;}
        if(q>=0xF0u)continue;
        if(col>=32u){col=0u;if(line<1u)++line;else{dialogue_obj_page_wait();dialogue_obj_begin();line=0u;}}
        dialogue_obj_source_glyph(24u+col*6u,8u+line*12u,q);++col;
    }
    dialogue_obj_hide();return 0;
}

static int field_dispatch_event_index(unsigned event_index,unsigned* camx,unsigned* camy);

/* -------------------------------------------------------------------------
 * Source-backed event-battle bring-up.
 *
 * The trigger/formation/background/enemy assets below all come from PSIV's
 * source data.  The turn engine is intentionally still small in v0.8.5; the
 * important compiler/runtime boundary is established now so later formations
 * can reuse it without touching the 59.7 Hz field path.
 * ------------------------------------------------------------------------- */
#define BATTLE_MAX_ENEMIES 4u
#define BATTLE_ENEMY_ANIM_COMPONENTS 6u
#define BATTLE_ENEMY_ANIM_MAX_FRAMES 18u
static uint8_t battle_enemy_anim_frame[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS];
static uint8_t battle_enemy_anim_timer[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS];
static uint8_t battle_enemy_placement[BATTLE_MAX_ENEMIES];
/* v0.5: active-formation animation plans.  PSIV's immutable 68000 mapping
 * records are still the authority, but there are at most four live enemies,
 * six animated components per enemy and eighteen frames per component in the
 * complete retail table.  Decode those pointer chains once at battle bind so
 * the 60 Hz animator normally touches only EWRAM.  Canonical ROM walkers stay
 * available below for any record that fails the bounded cache contract. */
static uint8_t battle_enemy_anim_cache_valid[BATTLE_MAX_ENEMIES];
static uint8_t battle_enemy_anim_cache_id[BATTLE_MAX_ENEMIES];
static uint8_t battle_enemy_anim_cache_ops[BATTLE_MAX_ENEMIES];
static uint8_t battle_enemy_anim_cache_frames[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS];
static uint8_t battle_enemy_anim_cache_delay[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS][BATTLE_ENEMY_ANIM_MAX_FRAMES];
static uint8_t battle_enemy_anim_cache_dst[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS];
static uint8_t battle_enemy_anim_cache_copy[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS];
static uint8_t battle_enemy_anim_cache_src[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS][BATTLE_ENEMY_ANIM_MAX_FRAMES];
static unsigned battle_art_pixel(const uint8_t* art,unsigned art_bytes,unsigned tile,unsigned x,unsigned y){
    unsigned o=tile*32u+y*4u+(x>>1);if(o>=art_bytes)return 0u;uint8_t v=art[o];return (x&1u)?(v&15u):(v>>4);
}
static void battle_palette_enemy(unsigned line,unsigned enemy_id){
    if(enemy_id>=PS4_PRECOMP_BATTLE_ENEMY_NATIVE_COUNT)return;
    /* Immutable source palette is preconverted at build time.  This mirrors
     * P128's rule that deterministic translation work belongs outside the
     * 60 Hz path.  One 32-byte DMA replaces clears + 11 ROM reads + 12
     * Mega-Drive-to-GBA color conversions on every battle composition. */
    unsigned b=line*16u;
    dma_copy32(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_PALETTE_OFF+enemy_id*32u,field_pause_pal+b,32u);
}
static unsigned battle_enemy_mapping_ops(unsigned enemy_id){
    uint32_t table=0x10FCEu;int16_t rel=(int16_t)rom16(table+(uint32_t)enemy_id*2u);uint32_t mp=(uint32_t)((int32_t)table+(int32_t)rel);unsigned ops=(unsigned)rom16(mp)+1u;return ops>BATTLE_ENEMY_ANIM_COMPONENTS?BATTLE_ENEMY_ANIM_COMPONENTS:ops;
}
static uint32_t battle_enemy_anim_descriptor(unsigned enemy_id,unsigned op){
    uint32_t table=0x10FCEu;int16_t rel=(int16_t)rom16(table+(uint32_t)enemy_id*2u);uint32_t mp=(uint32_t)((int32_t)table+(int32_t)rel)+2u+op*6u;uint32_t indir=rom32(mp+2u)&0x7FFFFFFFu;return rom32(indir);
}
static const uint32_t* battle_enemy_native_meta(unsigned enemy_id){
    if(enemy_id>=PS4_PRECOMP_BATTLE_ENEMY_NATIVE_COUNT)return 0;
    return (const uint32_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_META_OFF+enemy_id*PS4_PRECOMP_BATTLE_ENEMY_NATIVE_META_STRIDE);
}
static const uint32_t* battle_enemy_project_meta(unsigned enemy_id){
    if(enemy_id>=PS4_PRECOMP_BATTLE_ENEMY_NATIVE_COUNT)return 0;
    return (const uint32_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_PROJECT_META_OFF+enemy_id*PS4_PRECOMP_BATTLE_ENEMY_PROJECT_META_STRIDE);
}
static unsigned battle_enemy_project_x_edge(int source_x){
    if(source_x<=0)return 0u;if(source_x>=320)return 240u;
    return ps4_precomp_assets[PS4_PRECOMP_BATTLE_ENEMY_PROJECT_XEDGE_OFF+(unsigned)source_x];
}
static int battle_enemy_source_left_x(unsigned slot,unsigned width_tiles){
    int center=(slot<BATTLE_MAX_ENEMIES)?(int)((battle_enemy_placement[slot]&0x7Fu)<<3):160;
    return center-(int)(width_tiles*4u);
}
static unsigned battle_enemy_assemble(unsigned slot,unsigned enemy_id,uint8_t** tiles_out,const uint16_t** map_out,unsigned* map_count_out,unsigned* width_out,unsigned* height_out){
    /* P128-style immutable asset path.  Nemesis source/base pools and Enigma
     * maps are deterministic functions of the verified PSIV ROM, so the build
     * pre-decodes all 153 enemy records.  Runtime now DMA-copies the base pool
     * and applies only the authored frame-specific tile patches.  This removes
     * two Nemesis decodes plus one Enigma decode from each generic enemy draw. */
    const uint32_t* nm=battle_enemy_native_meta(enemy_id);if(!nm)return 0u;
    const uint8_t* source=ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_SOURCE_OFF+nm[0];unsigned source_bytes=nm[1];
    const uint8_t* base=ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_BASE_OFF+nm[2];unsigned base_bytes=nm[3];
    const uint16_t* native_map=(const uint16_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_MAP_OFF+nm[4]);unsigned map_count=nm[5];
    unsigned width=nm[6],height=nm[7];if(!source_bytes||!base_bytes||!width||!height||map_count<width*height)return 0u;
    dma_copy32(base,&work_ram[0x4000u],base_bytes);

    unsigned art_bytes=base_bytes;
    if(slot<BATTLE_MAX_ENEMIES&&battle_enemy_anim_cache_valid[slot]&&battle_enemy_anim_cache_id[slot]==enemy_id){
        unsigned ops=battle_enemy_anim_cache_ops[slot];
        for(unsigned op=0u;op<ops;op++){
            unsigned frame_count=battle_enemy_anim_cache_frames[slot][op];
            unsigned frame=battle_enemy_anim_frame[slot][op];if(!frame_count||frame>=frame_count)frame=0u;
            unsigned dst=battle_enemy_anim_cache_dst[slot][op],copy_count=battle_enemy_anim_cache_copy[slot][op];
            unsigned src=battle_enemy_anim_cache_src[slot][op][frame];
            unsigned src0=src*32u,dst0=dst*32u,bytes=copy_count*32u;
            if(src0+bytes>source_bytes||dst0+bytes>0x4000u)return 0u;
            if(dst0>art_bytes)dma_fill32(&work_ram[0x4000u+art_bytes],dst0-art_bytes,0u);
            dma_copy32(source+src0,&work_ram[0x4000u+dst0],bytes);
            if(dst0+bytes>art_bytes)art_bytes=dst0+bytes;
        }
    }else{
        uint32_t table=0x10FCEu;
        int16_t rel=(int16_t)rom16(table+(uint32_t)enemy_id*2u);
        uint32_t mp=(uint32_t)((int32_t)table+(int32_t)rel);
        unsigned ops=(unsigned)rom16(mp)+1u;mp+=2u;
        for(unsigned op=0u;op<ops;op++,mp+=6u){
            unsigned dst=rom8(mp),copy_count=rom8(mp+1u);
            uint32_t indir=rom32(mp+2u)&0x7FFFFFFFu;
            uint32_t selector=rom32(indir);
            uint8_t first=rom8(selector);unsigned frame_count=(unsigned)(first&0x7Fu);
            unsigned frame=(slot<BATTLE_MAX_ENEMIES&&op<BATTLE_ENEMY_ANIM_COMPONENTS)?battle_enemy_anim_frame[slot][op]:0u;if(!frame_count||frame>=frame_count)frame=0u;
            unsigned source_index=(first&0x80u)?(1u+frame_count+frame):(2u+frame);
            unsigned src=rom8(selector+source_index);
            unsigned src0=src*32u,dst0=dst*32u,bytes=copy_count*32u;
            if(src0+bytes>source_bytes||dst0+bytes>0x4000u)return 0u;
            if(dst0>art_bytes)dma_fill32(&work_ram[0x4000u+art_bytes],dst0-art_bytes,0u);
            dma_copy32(source+src0,&work_ram[0x4000u+dst0],bytes);
            if(dst0+bytes>art_bytes)art_bytes=dst0+bytes;
        }
    }
    *tiles_out=&work_ram[0x4000u];*map_out=native_map;*map_count_out=map_count;*width_out=width;*height_out=height;
    return art_bytes;
}
#define BATTLE_ENEMY_RESOLVED_TILES 288u
#define BATTLE_ENEMY_TILE_TRANSPARENT 0xFFFFu
#define BATTLE_ENEMY_TILE_SOURCE 0x8000u
static uint16_t battle_enemy_resolved_tiles[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_RESOLVED_TILES];
static uint8_t battle_enemy_resolved_frames[BATTLE_MAX_ENEMIES][BATTLE_ENEMY_ANIM_COMPONENTS];
static uint8_t battle_enemy_resolved_valid[BATTLE_MAX_ENEMIES];
static uint8_t battle_enemy_resolved_id[BATTLE_MAX_ENEMIES];
static uint8_t battle_enemy_death_visible_mask;
static uint8_t battle_enemy_death_pending_mask;
static uint8_t battle_enemy_death_rect_mask;
static uint8_t battle_enemy_death_timer[BATTLE_MAX_ENEMIES];

static int battle_enemy_resolved_prepare(unsigned slot,unsigned enemy_id,const uint32_t* nm){
    if(slot>=BATTLE_MAX_ENEMIES||!nm||!battle_enemy_anim_cache_valid[slot]||battle_enemy_anim_cache_id[slot]!=enemy_id)return 0;
    unsigned ops=battle_enemy_anim_cache_ops[slot];int same=battle_enemy_resolved_valid[slot]&&battle_enemy_resolved_id[slot]==enemy_id;
    if(same)for(unsigned op=0u;op<ops;op++)if(battle_enemy_resolved_frames[slot][op]!=battle_enemy_anim_frame[slot][op]){same=0;break;}
    if(same)return 1;
    unsigned source_tiles=nm[1]>>5,base_tiles=nm[3]>>5;if(base_tiles>BATTLE_ENEMY_RESOLVED_TILES)base_tiles=BATTLE_ENEMY_RESOLVED_TILES;
    for(unsigned t=0u;t<BATTLE_ENEMY_RESOLVED_TILES;t++)battle_enemy_resolved_tiles[slot][t]=BATTLE_ENEMY_TILE_TRANSPARENT;
    for(unsigned t=0u;t<base_tiles;t++)battle_enemy_resolved_tiles[slot][t]=(uint16_t)t;
    for(unsigned op=0u;op<ops;op++){
        unsigned frame_count=battle_enemy_anim_cache_frames[slot][op],frame=battle_enemy_anim_frame[slot][op];if(!frame_count||frame>=frame_count)frame=0u;
        unsigned dst=battle_enemy_anim_cache_dst[slot][op],copy_count=battle_enemy_anim_cache_copy[slot][op],src=battle_enemy_anim_cache_src[slot][op][frame];
        if(src+copy_count>source_tiles||dst+copy_count>BATTLE_ENEMY_RESOLVED_TILES){battle_enemy_resolved_valid[slot]=0u;return 0;}
        for(unsigned j=0u;j<copy_count;j++)battle_enemy_resolved_tiles[slot][dst+j]=(uint16_t)(BATTLE_ENEMY_TILE_SOURCE|(src+j));
        battle_enemy_resolved_frames[slot][op]=(uint8_t)frame;
    }
    for(unsigned op=ops;op<BATTLE_ENEMY_ANIM_COMPONENTS;op++)battle_enemy_resolved_frames[slot][op]=0u;
    battle_enemy_resolved_id[slot]=(uint8_t)enemy_id;battle_enemy_resolved_valid[slot]=1u;return 1;
}

static uint8_t battle_compose_clip_active;
static int battle_compose_clip_x0,battle_compose_clip_x1;
static unsigned battle_compose_clip_y0,battle_compose_clip_y1;
static void battle_compose_clip_set(int x0,unsigned y0,int x1,unsigned y1){battle_compose_clip_x0=x0;battle_compose_clip_x1=x1;battle_compose_clip_y0=y0;battle_compose_clip_y1=y1;battle_compose_clip_active=1u;}
static void battle_compose_clip_clear(void){battle_compose_clip_active=0u;}

typedef uint16_t battle_u16_alias __attribute__((may_alias));
static uint32_t battle_enemy_death_row_mask(unsigned slot,unsigned enemy_id,unsigned tile,unsigned row){
    if(slot>=BATTLE_MAX_ENEMIES||enemy_id>=PS4_PRECOMP_BATTLE_ENEMY_NATIVE_COUNT||!(battle_enemy_death_visible_mask&(1u<<slot))||row>=8u)return 0xFFFFFFFFu;
    const uint32_t* meta=(const uint32_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_DEATH_META_OFF+enemy_id*PS4_PRECOMP_BATTLE_ENEMY_DEATH_META_STRIDE);
    if(tile>=meta[1])return 0xFFFFFFFFu;
    uint8_t key=ps4_precomp_assets[PS4_PRECOMP_BATTLE_ENEMY_DEATH_KEYS_OFF+meta[0]+tile];if(!key)return 0xFFFFFFFFu;
    unsigned v=(unsigned)key-1u,group=v>>2,rank=v&3u;
    unsigned d4=group*8u+(7u-row),last=battle_enemy_death_timer[slot],first=d4;if(first&1u)++first;
    unsigned end=d4+15u;if(end>last)end=last;uint32_t mask=0xFFFFFFFFu;
    const uint32_t* masks=(const uint32_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_DEATH_MASKS_OFF);
    if(first<=end)for(unsigned t=first;t<=end;t+=2u){unsigned phase=t-d4;mask&=masks[phase*4u+rank];}
    return mask;
}
static unsigned battle_enemy_death_limit(unsigned enemy_id){
    int16_t rel=(int16_t)rom16(0xC21Cu+(uint32_t)enemy_id*2u);uint32_t tab=(uint32_t)((int32_t)0xC21C+(int32_t)rel);
    return ((unsigned)rom8(tab)+2u)*8u;
}
static void battle_blit_enemy_tile_row(const uint8_t* src,uint8_t* dst,unsigned pal_base,int hflip,uint32_t source_mask){
    /* Two source nibbles become two Mode-4 palette bytes.  Enemy anchors are
     * even after the 320->240 projection, so fully-opaque pairs are one
     * aligned halfword store; transparent nibbles preserve the background. */
    uint8_t masked[4];
    masked[0]=(uint8_t)(src[0]&(uint8_t)(source_mask>>24));masked[1]=(uint8_t)(src[1]&(uint8_t)(source_mask>>16));
    masked[2]=(uint8_t)(src[2]&(uint8_t)(source_mask>>8));masked[3]=(uint8_t)(src[3]&(uint8_t)source_mask);
    for(unsigned pair=0u;pair<4u;pair++){
        uint8_t q=masked[hflip?(3u-pair):pair];uint8_t a=hflip?(q&15u):(q>>4),b=hflip?(q>>4):(q&15u);
        if(a&&b){battle_u16_alias* d=(battle_u16_alias*)(void*)dst;*d=(uint16_t)((pal_base+a)|((pal_base+b)<<8));}
        else{if(a)dst[0]=(uint8_t)(pal_base+a);if(b)dst[1]=(uint8_t)(pal_base+b);}dst+=2;
    }
}
/* R15 exact span kernel for the CP14 projected enemy path.
 * Same pixels as the per-pixel loop below (host differential test:
 * tools_host/battle_sim/diff_enemy.c), but:
 *   - the 320->240 column plan (xmap - src_left -> tile column, lx) is built
 *     once per call instead of once per pixel per row;
 *   - each (row, tile column) resolves the tile, flip and death mask once and
 *     fetches the 4-byte tile row once; a fully transparent tile row writes
 *     nothing and costs no per-pixel work;
 *   - the row/run loop runs from IWRAM (ARM), plan tables live in IWRAM.
 * PS4_BATTLE_ENEMY_SPAN=0 restores the R14 per-pixel loop. */
#ifndef PS4_BATTLE_ENEMY_SPAN
#define PS4_BATTLE_ENEMY_SPAN 1
#endif
#if defined(PS4_BATTLE_ENEMY_SPAN_TEST)
static int battle_enemy_span_test_select=1;
#define BATTLE_ENEMY_USE_SPAN (battle_enemy_span_test_select)
#else
#define BATTLE_ENEMY_USE_SPAN (PS4_BATTLE_ENEMY_SPAN)
#endif
#define BATTLE_SPAN_STATE __attribute__((section(".iwram.battle.state")))
#define BATTLE_SPAN_MAX_RUNS 48u
BATTLE_SPAN_STATE static uint8_t battle_span_lx[240];
BATTLE_SPAN_STATE static uint8_t battle_span_run_x0[BATTLE_SPAN_MAX_RUNS];
BATTLE_SPAN_STATE static uint8_t battle_span_run_n[BATTLE_SPAN_MAX_RUNS];
BATTLE_SPAN_STATE static uint8_t battle_span_run_lx0[BATTLE_SPAN_MAX_RUNS];
BATTLE_SPAN_STATE static uint16_t battle_span_run_tx[BATTLE_SPAN_MAX_RUNS];
/* Plan stores, per destination pixel, the nibble shift inside the
 * little-endian 32-bit tile row: byte lx>>1, high nibble for even lx
 * (matches (xx&1)?(q&15):(q>>4)).  Shifts are {4,0,12,8,20,16,28,24}; for a
 * horizontally flipped tile shift(7-lx) == shift(lx)^28, so the flip is one EOR. */
struct battle_span_job{
    const uint16_t* native_map;const uint8_t* ymap;const uint8_t* source;const uint8_t* base;const uint8_t* assembled;
    unsigned art_bytes,need,w,h,slot,enemy_id,pal_base,runs,dy0,dy1;int sy,resolved,death_active;
};
static uint32_t battle_enemy_death_row_mask(unsigned slot,unsigned enemy_id,unsigned tile,unsigned row);
__attribute__((section(".iwram.text"),noinline))
static void battle_enemy_span_rows(const struct battle_span_job* j){
    for(unsigned dy=j->dy0;dy<j->dy1;dy++){
        unsigned src_y=j->ymap[dy],ty=src_y>>3,ly=src_y&7u;if(ty>=j->h)continue;
        uint8_t* row=field_pause_fb+(unsigned)(j->sy+(int)dy)*240u;unsigned rowpos=ty*j->w;
        for(unsigned r=0u;r<j->runs;r++){
            unsigned pos=rowpos+battle_span_run_tx[r];if(pos>=j->need)continue;
            uint16_t e=j->native_map[pos];unsigned tile=e&0x07FFu;const uint8_t* tp=0;
            if(j->resolved){
                if(tile<BATTLE_ENEMY_RESOLVED_TILES){uint16_t rr=battle_enemy_resolved_tiles[j->slot][tile];if(rr!=BATTLE_ENEMY_TILE_TRANSPARENT)tp=(rr&BATTLE_ENEMY_TILE_SOURCE)?j->source+(unsigned)(rr&0x7FFFu)*32u:j->base+(unsigned)rr*32u;}
            }else if(tile*32u+32u<=j->art_bytes)tp=j->assembled+tile*32u;
            if(!tp)continue;
            unsigned yy=(e&0x1000u)?7u-ly:ly;const uint8_t* rp=tp+yy*4u;
            uint32_t bits=(uint32_t)rp[0]|((uint32_t)rp[1]<<8)|((uint32_t)rp[2]<<16)|((uint32_t)rp[3]<<24);
            uint32_t dm=j->death_active?battle_enemy_death_row_mask(j->slot,j->enemy_id,tile,yy):0xFFFFFFFFu;
            bits&=(dm>>24)|((dm>>8)&0xFF00u)|((dm<<8)&0xFF0000u)|(dm<<24);   /* big-endian mask bytes */
            if(!bits)continue;
            unsigned n=battle_span_run_n[r],x0=battle_span_run_x0[r];const uint8_t* lxp=&battle_span_lx[battle_span_run_lx0[r]];
            unsigned hf=(e&0x0800u)?28u:0u,pb=j->pal_base;uint8_t* o=row+x0;
            for(unsigned k=0u;k<n;k++){unsigned ci=(bits>>(lxp[k]^hf))&15u;if(ci)o[k]=(uint8_t)(pb+ci);}
        }
    }
}
/* Returns 1 when it drew (always, given the same preconditions as the
 * projected branch of battle_draw_enemy). */
static int battle_enemy_draw_projected_span(unsigned slot,unsigned enemy_id,int sy,unsigned pal_base,const uint16_t* native_map,unsigned need,unsigned w,unsigned h,
        const uint8_t* source,const uint8_t* base,int resolved,const uint8_t* assembled,unsigned art_bytes,
        const uint16_t* xmap,const uint8_t* ymap,unsigned ph,int src_left,unsigned px0,unsigned px1){
    struct battle_span_job j;unsigned runs=0u,nlx=0u;int wpx=(int)(w*8u);
    for(unsigned px=px0;px<px1;px++){
        int local_x=(int)xmap[px]-src_left;if(local_x<0||local_x>=wpx)continue;
        unsigned tx=(unsigned)local_x>>3,lx=(unsigned)local_x&7u;
        if(runs&&battle_span_run_tx[runs-1u]==tx&&(unsigned)battle_span_run_x0[runs-1u]+battle_span_run_n[runs-1u]==px){battle_span_run_n[runs-1u]++;}
        else{if(runs>=BATTLE_SPAN_MAX_RUNS)return 0;battle_span_run_tx[runs]=(uint16_t)tx;battle_span_run_x0[runs]=(uint8_t)px;battle_span_run_n[runs]=1u;battle_span_run_lx0[runs]=(uint8_t)nlx;++runs;}
        battle_span_lx[nlx++]=(uint8_t)(((lx>>1)<<3)|((lx&1u)?0u:4u));
    }
    int d0=0,d1=(int)ph;if(sy+d0<0)d0=-sy;if(sy+d1>160)d1=160-sy;
    if(battle_compose_clip_active){if(sy+d0<(int)battle_compose_clip_y0)d0=(int)battle_compose_clip_y0-sy;if(sy+d1>(int)battle_compose_clip_y1)d1=(int)battle_compose_clip_y1-sy;}
    if(d1<=d0||!runs)return 1;
    j.native_map=native_map;j.ymap=ymap;j.source=source;j.base=base;j.assembled=assembled;j.art_bytes=art_bytes;j.need=need;j.w=w;j.h=h;
    j.slot=slot;j.enemy_id=enemy_id;j.pal_base=pal_base;j.runs=runs;j.dy0=(unsigned)d0;j.dy1=(unsigned)d1;j.sy=sy;j.resolved=resolved;
    /* Exactly the early-out of battle_enemy_death_row_mask(): no ROM call per run
     * unless this slot is in its death dissolve. */
    j.death_active=(slot<BATTLE_MAX_ENEMIES&&enemy_id<PS4_PRECOMP_BATTLE_ENEMY_NATIVE_COUNT&&(battle_enemy_death_visible_mask&(1u<<slot)))?1:0;
    battle_enemy_span_rows(&j);return 1;
}
static void battle_draw_enemy(unsigned slot,unsigned enemy_id,int sx,int sy,unsigned pal_line){
    const uint32_t* nm=battle_enemy_native_meta(enemy_id);if(!nm)return;
    const uint8_t* source=ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_SOURCE_OFF+nm[0];
    const uint8_t* base=ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_BASE_OFF+nm[2];
    const uint16_t* native_map=(const uint16_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_MAP_OFF+nm[4]);
    unsigned map_count=nm[5],w=nm[6],h=nm[7],need=w*h,pal_base=pal_line*16u;if(!w||!h||map_count<need)return;
    int resolved=battle_enemy_resolved_prepare(slot,enemy_id,nm);uint8_t* assembled=0;unsigned art_bytes=0u;
    if(!resolved){const uint16_t* fallback_map;unsigned fallback_count,fw,fh;art_bytes=battle_enemy_assemble(slot,enemy_id,&assembled,&fallback_map,&fallback_count,&fw,&fh);if(!art_bytes)return;native_map=fallback_map;map_count=fallback_count;w=fw;h=fh;need=w*h;if(map_count<need)need=map_count;}

    /* CP14: generic enemy pixels are authored on the same 320x224 battle
     * plane as the background. Horizontal sampling must use the exact global
     * 320->240 nearest phase, because nearest-neighbor column choice changes
     * with source placement. Vertical sampling is precompiled per source
     * mapping height against the fixed Plane-A floor. Live animation tile
     * substitution and Enemy_Dead masks remain source-owned at runtime. */
    const uint32_t* pm=battle_enemy_project_meta(enemy_id);
    if(pm&&pm[1]&&pm[2]&&w==nm[6]&&h==nm[7]){
        const uint16_t* xmap=(const uint16_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_PROJECT_XMAP_OFF);
        const uint8_t* ymap=ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_PROJECT_Y_OFF+pm[0];
        unsigned ph=pm[2];int src_left=battle_enemy_source_left_x(slot,w),src_right=src_left+(int)(w*8u);
        unsigned px0=battle_enemy_project_x_edge(src_left),px1=battle_enemy_project_x_edge(src_right);
        if(battle_compose_clip_active){if(px0<(unsigned)(battle_compose_clip_x0<0?0:battle_compose_clip_x0))px0=(unsigned)(battle_compose_clip_x0<0?0:battle_compose_clip_x0);if(px1>(unsigned)(battle_compose_clip_x1>240?240:battle_compose_clip_x1))px1=(unsigned)(battle_compose_clip_x1>240?240:battle_compose_clip_x1);}
        if(px1<=px0)return;
        if(BATTLE_ENEMY_USE_SPAN&&battle_enemy_draw_projected_span(slot,enemy_id,sy,pal_base,native_map,need,w,h,source,base,resolved,assembled,art_bytes,xmap,ymap,ph,src_left,px0,px1))return;
        for(unsigned dy=0u;dy<ph;dy++){
            int py=sy+(int)dy;if(py<0||py>=160)continue;
            if(battle_compose_clip_active&&((unsigned)py<battle_compose_clip_y0||(unsigned)py>=battle_compose_clip_y1))continue;
            unsigned src_y=ymap[dy],ty=src_y>>3,ly=src_y&7u;if(ty>=h)continue;
            unsigned last_pos=0xFFFFFFFFu;uint16_t e=0u;unsigned tile=0u,yy=0u;const uint8_t* tile_pixels=0;uint32_t dm=0xFFFFFFFFu;
            for(unsigned px=px0;px<px1;px++){
                int local_x=(int)xmap[px]-src_left;if(local_x<0||local_x>=(int)(w*8u))continue;
                unsigned src_x=(unsigned)local_x,tx=src_x>>3,lx=src_x&7u,pos=ty*w+tx;if(pos>=need)continue;
                if(pos!=last_pos){
                    last_pos=pos;e=native_map[pos];tile=e&0x07FFu;tile_pixels=0;
                    if(resolved){
                        if(tile<BATTLE_ENEMY_RESOLVED_TILES){uint16_t r=battle_enemy_resolved_tiles[slot][tile];if(r!=BATTLE_ENEMY_TILE_TRANSPARENT)tile_pixels=(r&BATTLE_ENEMY_TILE_SOURCE)?source+(unsigned)(r&0x7FFFu)*32u:base+(unsigned)r*32u;}
                    }else if(tile*32u+32u<=art_bytes)tile_pixels=assembled+tile*32u;
                    yy=(e&0x1000u)?7u-ly:ly;dm=tile_pixels?battle_enemy_death_row_mask(slot,enemy_id,tile,yy):0u;
                }
                if(!tile_pixels)continue;
                unsigned xx=(e&0x0800u)?7u-lx:lx,bi=xx>>1;uint8_t mb=(uint8_t)(dm>>(24u-bi*8u));uint8_t q=(uint8_t)(tile_pixels[yy*4u+bi]&mb);unsigned ci=(xx&1u)?(q&15u):(q>>4);
                if(ci)field_pause_fb[(unsigned)py*240u+px]=(uint8_t)(pal_base+ci);
            }
        }
        return;
    }

    /* Canonical safety path for a malformed/unavailable generated projection.
     * All verified retail enemy records have CP14 projection metadata, so this
     * path is not expected on the normal game hot path. */
    unsigned pos=0u;
    for(unsigned ty=0u;ty<h&&pos<need;ty++)for(unsigned tx=0u;tx<w&&pos<need;tx++,pos++){
        uint16_t e=native_map[pos];unsigned tile=e&0x07FFu;const uint8_t* tile_pixels=0;
        if(resolved){
            if(tile<BATTLE_ENEMY_RESOLVED_TILES){uint16_t r=battle_enemy_resolved_tiles[slot][tile];if(r!=BATTLE_ENEMY_TILE_TRANSPARENT)tile_pixels=(r&BATTLE_ENEMY_TILE_SOURCE)?source+(unsigned)(r&0x7FFFu)*32u:base+(unsigned)r*32u;}
        }else if(tile*32u+32u<=art_bytes)tile_pixels=assembled+tile*32u;
        if(!tile_pixels)continue;
        int ox=sx+(int)(tx*8u),oy=sy+(int)(ty*8u);
        if(battle_compose_clip_active&&(ox+8<=battle_compose_clip_x0||ox>=battle_compose_clip_x1||oy+8<=(int)battle_compose_clip_y0||oy>=(int)battle_compose_clip_y1))continue;
        if(ox>=0&&ox+8<=240&&(!battle_compose_clip_active||(ox>=battle_compose_clip_x0&&ox+8<=battle_compose_clip_x1))){
            for(unsigned y=0u;y<8u;y++){int py=oy+(int)y;if(py<0||py>=160||(battle_compose_clip_active&&((unsigned)py<battle_compose_clip_y0||(unsigned)py>=battle_compose_clip_y1)))continue;unsigned yy=(e&0x1000u)?7u-y:y;const uint8_t* row=tile_pixels+yy*4u;battle_blit_enemy_tile_row(row,field_pause_fb+(unsigned)py*240u+(unsigned)ox,pal_base,(e&0x0800u)!=0u,battle_enemy_death_row_mask(slot,enemy_id,tile,yy));}
        }else{
            for(unsigned y=0u;y<8u;y++){int py=oy+(int)y;if(py<0||py>=160||(battle_compose_clip_active&&((unsigned)py<battle_compose_clip_y0||(unsigned)py>=battle_compose_clip_y1)))continue;unsigned yy=(e&0x1000u)?7u-y:y;
                for(unsigned x=0u;x<8u;x++){int px=ox+(int)x;if(px<0||px>=240||(battle_compose_clip_active&&(px<battle_compose_clip_x0||px>=battle_compose_clip_x1)))continue;unsigned xx=(e&0x0800u)?7u-x:x;uint32_t dm=battle_enemy_death_row_mask(slot,enemy_id,tile,yy);unsigned bi=xx>>1;uint8_t mb=(uint8_t)(dm>>(24u-bi*8u));uint8_t q=(uint8_t)(tile_pixels[yy*4u+bi]&mb);unsigned ci=(xx&1u)?(q&15u):(q>>4);if(ci)field_pause_fb[(unsigned)py*240u+(unsigned)px]=(uint8_t)(pal_base+ci);}}
        }
    }
}

struct BattleLiteActor {
    uint16_t hp,max_hp,tp,max_tp,atk,def,magic_def;
    uint16_t base_atk,base_def,base_magic_def;
    uint8_t id,basic_element,basic_effect,strength,mental,agility,dexterity,status,defending,android;
    uint8_t base_strength,base_mental,base_agility,base_dexterity;
    uint8_t element_prop[14],base_element_prop[14];
};
static int battle_s8(uint8_t q){return (q&0x80u)?(int)q-256:(int)q;}
static int battle_enemy_draw_visible(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned i){
    return i<BATTLE_MAX_ENEMIES&&(enemies[i].hp||(battle_enemy_death_visible_mask&(1u<<i)));
}
static unsigned battle_element_factor(const struct BattleLiteActor* a,unsigned element){
    if(element<1u||element>14u)return 4u;
    return a->element_prop[element-1u];
}
static unsigned battle_actor_stat(const struct BattleLiteActor* a,unsigned selector){
    switch(selector&0x7Fu){
    case 1u:return a->strength; case 2u:return a->mental; case 3u:return a->agility;
    case 4u:return a->dexterity; case 5u:return a->atk; case 6u:return a->def;
    case 7u:return a->magic_def; default:return 0u;
    }
}
static int battle_actor_incapacitated(const struct BattleLiteActor* a){
    return !a->hp || (a->status&0x6Eu)!=0u; /* paralyzed/dead/asleep/asleep2/android-dead */
}
static unsigned battle_load_event_formation(unsigned value,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]);
static uint16_t battle_char_record(unsigned id){return id>=11u?0x4700u:(uint16_t)(0xF500u+id*0x80u);}
static void battle_load_char(unsigned id,struct BattleLiteActor* a){
    /* Character_Stats stores both base and battle-derived values.  v0.9.8 uses
     * the exact battle fields consumed by Battle_OrderTurns/Character_DamageEnemy
     * rather than the v0.9.7 base/modifier fields. */
    uint16_t d=battle_char_record(id);
    a->hp=ram16r((uint16_t)(d+0x0Eu));a->max_hp=ram16r((uint16_t)(d+0x10u));
    a->tp=ram16r((uint16_t)(d+0x12u));a->max_tp=ram16r((uint16_t)(d+0x14u));
    a->status=ram8r((uint16_t)(d+0x16u));a->strength=ram8r((uint16_t)(d+0x1Au));
    a->mental=ram8r((uint16_t)(d+0x1Du));a->agility=ram8r((uint16_t)(d+0x20u));
    a->dexterity=ram8r((uint16_t)(d+0x23u));a->atk=ram16r((uint16_t)(d+0x26u));
    a->def=ram16r((uint16_t)(d+0x2Au));a->magic_def=ram16r((uint16_t)(d+0x2Eu));
    /* Battle_RestoreStatsAtTurnEnd/AbilityEffect_RestoreStats operate from
     * the equipment-modified/base fields, not from guessed defaults. */
    a->base_strength=ram8r((uint16_t)(d+0x19u));a->base_mental=ram8r((uint16_t)(d+0x1Cu));
    a->base_agility=ram8r((uint16_t)(d+0x1Fu));a->base_dexterity=ram8r((uint16_t)(d+0x22u));
    a->base_atk=ram16r((uint16_t)(d+0x24u));a->base_def=ram16r((uint16_t)(d+0x28u));a->base_magic_def=ram16r((uint16_t)(d+0x2Cu));
    /* PSIV stores the live property byte at the even address and the restore
     * byte at the following odd address; AbilityEffect_RestoreStats copies
     * $31,$33..$4B back to $30,$32..$4A. */
    for(unsigned e=0u;e<14u;e++){a->element_prop[e]=ram8r((uint16_t)(d+0x30u+e*2u));a->base_element_prop[e]=ram8r((uint16_t)(d+0x31u+e*2u));}
    if(!a->atk)a->atk=1u;if(!a->base_atk)a->base_atk=a->atk;a->id=(uint8_t)id;a->basic_element=1u;a->basic_effect=0u;a->defending=0u;a->android=ram16r((uint16_t)(d+6u))==5u;
}
static void battle_load_enemy(unsigned id,struct BattleLiteActor* a){
    /* Exact 48-byte EnemyData record: HP, basic attack element/status,
     * STR/MEN/AGI/DEX, attack word, defense, magic defense, 14 properties,
     * AI block, EXP and Meseta. */
    uint32_t p=0x2816BCu+(uint32_t)id*48u;
    a->hp=a->max_hp=rom16(p);a->tp=a->max_tp=0u;a->basic_element=rom8(p+2u);a->basic_effect=rom8(p+3u);
    a->strength=rom8(p+4u);a->mental=rom8(p+5u);a->agility=rom8(p+6u);a->dexterity=rom8(p+7u);
    a->atk=rom16(p+8u);a->def=rom8(p+10u);a->magic_def=rom8(p+11u);
    for(unsigned e=0u;e<14u;e++)a->element_prop[e]=rom8(p+12u+e);
    a->id=(uint8_t)id;a->status=0u;a->defending=0u;a->android=0u;if(!a->basic_element)a->basic_element=1u;
    a->base_strength=a->strength;a->base_mental=a->mental;a->base_agility=a->agility;a->base_dexterity=a->dexterity;
    a->base_atk=a->atk;a->base_def=a->def;a->base_magic_def=a->magic_def;for(unsigned e=0u;e<14u;e++)a->base_element_prop[e]=a->element_prop[e];
}
static unsigned battle_calc_damage_ex(unsigned atk,unsigned def,unsigned element_factor,unsigned power){
    /* Battle_CalculateDamage ($B5C8) + loc_266C clamp, CP22 exact word form.
     * The source works in 68000 word arithmetic: muls.w multiplies the signed
     * low words into 32 bits, then lsr.w/add.w/sub.w touch only the low word
     * (the high word is left untouched and never read), and loc_266C tests
     * the final word as signed (`tst.w d0 / bgt`) before clamping to 1..999.
     * The earlier full-width form matched for ordinary stats but diverged
     * once q*atk exceeded 65535 or the final word was >= $8000 (source gives
     * 1 there, full width gave 999). */
    uint16_t q=0u;for(unsigned i=0;i<16u;i++)q=(uint16_t)(q+(ps4_rng2_next()&7u));
    q=(uint16_t)(q+8u);                                                   /* addq.w #8,d6 */
    uint16_t w=(uint16_t)(int32_t)((int32_t)(int16_t)q*(int32_t)(int16_t)(uint16_t)atk); /* muls.w d1,d6 */
    w=(uint16_t)(w>>6);                                                   /* lsr.w #6,d6 */
    w=(uint16_t)(w+(uint16_t)atk);                                        /* add.w d1,d6 */
    w=(uint16_t)(w+(uint16_t)(power*2u));                                 /* add.w d4,d4; add.w d4,d6 */
    w=(uint16_t)(int32_t)((int32_t)(int16_t)w*(int32_t)(int16_t)(uint16_t)element_factor); /* muls.w d3,d6 */
    w=(uint16_t)(w>>2);                                                   /* lsr.w #2,d6 */
    w=(uint16_t)(w-(uint16_t)def);                                        /* sub.w d2,d6 */
    int d=(int)(int16_t)w;                                                /* tst.w d0 */
    if(d<=0)d=1;if(d>999)d=999;return (unsigned)d;
}
/* CP22 critical bonus: Character_DamageEnemy and Enemy_DamageCharacter both do
 * `moveq #0,d4 / move.b d1,d4 / lsr.w #2,d4` on a critical hit, so the bonus
 * power is (attack & $FF) >> 2 (a byte move), not attack >> 2.  For attack
 * >= 256 the full-width form over-weighted criticals (atk 300: +150 vs +22). */
static unsigned battle_calc_damage(unsigned atk,unsigned def,unsigned element_factor){return battle_calc_damage_ex(atk,def,element_factor,0u);}
static int battle_calc_chance(unsigned actor,unsigned target,unsigned element_factor,unsigned lower,unsigned upper){
    /* Battle_CalculateChances ($B5A8). Return -1 miss, 0 normal, +1 high. */
    int q=(int)(ps4_rng2_next()&0x3Fu)+(int)actor-(int)target;q*=(int)element_factor;
    if(q<=(int)lower)return -1;if(q<=(int)upper)return 0;return 1;
}
static unsigned battle_bar_pixels(unsigned hp,unsigned maxhp,unsigned width){
    if(!maxhp)return 0u;unsigned filled=0u,acc=0u;for(unsigned i=0;i<width;i++){acc+=hp;if(acc>=maxhp){acc-=maxhp;++filled;}}return filled;
}
static void battle_draw_text(unsigned x,unsigned y,const char* q,uint8_t c){for(;*q&&x<232u;q++,x+=8u)pause_draw_window_ascii(x,y,(uint8_t)*q,c);}
static void battle_draw_uint(unsigned x,unsigned y,unsigned v,uint8_t c){
    /* Values displayed here are capped at 999.  Decimal conversion by repeated
     * subtraction keeps ARM7 free of the EABI integer-divide helpers. */
    static const unsigned place[3]={100u,10u,1u};int started=0;
    if(v>999u)v=999u;
    for(unsigned i=0u;i<3u;i++){
        unsigned d=0u;while(v>=place[i]){v-=place[i];++d;}
        if(d||started||i==2u){pause_draw_window_glyph(x,y,(uint8_t)(27u+d),c);x+=8u;started=1;}
    }
}
static uint32_t battle_enemy_name_off(unsigned id){
    uint32_t p=0x280D42u;
    while(id--){while(rom8(p)!=0xFFu)++p;++p;}
    return p;
}
static void battle_draw_enemy_name(unsigned id){
    uint32_t p=battle_enemy_name_off(id);unsigned x=84u;
    while(rom8(p)!=0xFFu&&x<224u){uint8_t q=rom8(p++);if(q<87u){pause_draw_window_glyph(x,4u,q,80u);x+=8u;}}
}
static uint32_t battle_enemy_skill_name_off(unsigned id){
    /* Enemy skill IDs are 1-based: id 1 is the first EnemySkillNames entry. */
    uint32_t p=0x2812E4u;if(id)--id;while(id--){while(rom8(p)!=0xFFu)++p;++p;}return p;
}
static void battle_draw_enemy_skill_name(unsigned id){
    uint32_t p=battle_enemy_skill_name_off(id);unsigned x=70u;
    while(rom8(p)!=0xFFu&&x<232u){uint8_t q=rom8(p++);if(q<87u){pause_draw_window_glyph(x,4u,q,83u);x+=8u;}}
}
static unsigned battle_enemy_count=3u;
/* Decode only the active formation's immutable animation metadata.  Passing
 * the count explicitly keeps this helper independent of declaration order and
 * makes the cache boundary obvious to differential tests. */
static void battle_enemy_anim_cache_bind(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned count){
    for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++){
        battle_enemy_anim_cache_valid[i]=0u;battle_enemy_anim_cache_id[i]=0u;battle_enemy_anim_cache_ops[i]=0u;
    }
    if(count>BATTLE_MAX_ENEMIES)count=BATTLE_MAX_ENEMIES;
    for(unsigned i=0u;i<count;i++){
        unsigned enemy_id=enemies[i].id;uint32_t table=0x10FCEu;
        int16_t rel=(int16_t)rom16(table+(uint32_t)enemy_id*2u);
        uint32_t mp=(uint32_t)((int32_t)table+(int32_t)rel);unsigned ops=(unsigned)rom16(mp)+1u;
        if(!ops||ops>BATTLE_ENEMY_ANIM_COMPONENTS)continue;mp+=2u;int ok=1;
        battle_enemy_anim_cache_id[i]=(uint8_t)enemy_id;battle_enemy_anim_cache_ops[i]=(uint8_t)ops;
        for(unsigned op=0u;op<ops;op++,mp+=6u){
            unsigned dst=rom8(mp),copy_count=rom8(mp+1u);uint32_t indir=rom32(mp+2u)&0x7FFFFFFFu;uint32_t selector=rom32(indir);
            uint8_t first=rom8(selector);unsigned frame_count=(unsigned)(first&0x7Fu);
            if(!frame_count||frame_count>BATTLE_ENEMY_ANIM_MAX_FRAMES){ok=0;break;}
            battle_enemy_anim_cache_frames[i][op]=(uint8_t)frame_count;battle_enemy_anim_cache_dst[i][op]=(uint8_t)dst;battle_enemy_anim_cache_copy[i][op]=(uint8_t)copy_count;
            for(unsigned f=0u;f<frame_count;f++){
                battle_enemy_anim_cache_delay[i][op][f]=(first&0x80u)?rom8(selector+1u+f):rom8(selector+1u);
                unsigned source_index=(first&0x80u)?(1u+frame_count+f):(2u+f);
                battle_enemy_anim_cache_src[i][op][f]=rom8(selector+source_index);
            }
        }
        if(ok)battle_enemy_anim_cache_valid[i]=1u;
    }
}
static uint8_t battle_event_index=0u;
static uint8_t battle_show_enemy_target=0u; /* only while PSIV Target_Cursor is active */
/* Battle_SetupEnemyData keeps one source placement byte beside every enemy
 * ID.  Bit 7 selects the fighter/back-row flag; bits 0..6 are converted to
 * the Genesis battle-plane X coordinate by $80 + (placement << 3).  Keep the
 * byte intact so every formation, including bosses and random encounters,
 * uses the authored PSIV geometry rather than count-based GBA layouts. */
static int battle_enemy_alive(const struct BattleLiteActor e[BATTLE_MAX_ENEMIES],unsigned i){return i<battle_enemy_count&&e[i].hp!=0u;}
static unsigned battle_next_slot(unsigned v){return v+1u>=battle_enemy_count?0u:v+1u;}
static unsigned battle_prev_slot(unsigned v){return v==0u?battle_enemy_count-1u:v-1u;}
static unsigned battle_next_target(const struct BattleLiteActor e[BATTLE_MAX_ENEMIES],unsigned cur,int dir){
    for(unsigned n=0u;n<battle_enemy_count;n++){cur=dir<0?battle_prev_slot(cur):battle_next_slot(cur);if(battle_enemy_alive(e,cur))return cur;}return cur;
}
/* EnemySprites_Animate ($40F0C): each EnemySpriteMappingsOffs component owns
 * an independent frame/timer.  Fixed descriptors are [count,delay,frames...];
 * bit7 descriptors are [80|count, delay0..N-1, frame0..N-1].  There are at
 * most six components in the complete 153-entry source mapping table. */
__attribute__((noinline))
static unsigned battle_enemy_anim_advance_generic(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    unsigned changed=0u;
    for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++){
        if(!enemies[i].hp)continue;
        if(battle_enemy_anim_cache_valid[i]&&battle_enemy_anim_cache_id[i]==enemies[i].id){
            unsigned ops=battle_enemy_anim_cache_ops[i];
            for(unsigned op=0u;op<ops;op++){
                unsigned frame_count=battle_enemy_anim_cache_frames[i][op];if(!frame_count)continue;
                uint8_t* frame=&battle_enemy_anim_frame[i][op];uint8_t* timer=&battle_enemy_anim_timer[i][op];
                if(enemies[i].status&8u){if(*frame){*frame=0u;changed|=1u<<i;}*timer=battle_enemy_anim_cache_delay[i][op][0];continue;}
                /* Dominant 60 Hz path: a live timer is one EWRAM decrement.
                 * No reversed-ROM pointer chasing or GamePak reads. */
                if(*timer){--*timer;continue;}
                uint8_t old=*frame;unsigned nf=(unsigned)old+1u;if(nf>=frame_count)nf=0u;*frame=(uint8_t)nf;
                *timer=battle_enemy_anim_cache_delay[i][op][nf];if(*frame!=old)changed|=1u<<i;
            }
            continue;
        }
        /* Canonical fallback for an uncacheable/dynamic record. */
        unsigned ops=battle_enemy_mapping_ops(enemies[i].id);
        for(unsigned op=0u;op<ops;op++){
            uint32_t d=battle_enemy_anim_descriptor(enemies[i].id,op);
            uint8_t first=rom8(d);unsigned frame_count=(unsigned)(first&0x7Fu);if(!frame_count)continue;
            uint8_t* frame=&battle_enemy_anim_frame[i][op];uint8_t* timer=&battle_enemy_anim_timer[i][op];
            unsigned delay=(first&0x80u)?rom8(d+1u+*frame):rom8(d+1u);
            if(enemies[i].status&8u){if(*frame){*frame=0u;changed|=1u<<i;}*timer=(uint8_t)((first&0x80u)?rom8(d+1u):delay);continue;}
            if(*timer){--*timer;continue;}
            uint8_t old=*frame;unsigned nf=(unsigned)old+1u;if(nf>=frame_count)nf=0u;*frame=(uint8_t)nf;
            *timer=(first&0x80u)?rom8(d+1u+nf):rom8(d+1u);if(*frame!=old)changed|=1u<<i;
        }
    }
    return changed;
}
static const uint8_t* battle_xanaf_anim_pixels(void);
static const uint8_t* battle_igg_anim_pixels(void);
static void ps4_ui_draw_ram_name(unsigned x,unsigned y,unsigned char_id,uint8_t c);
static void ps4_ui_box(unsigned x0,unsigned y0,unsigned x1,unsigned y1);

#define BATTLE_XANAF_OBJ_TILE 512u
#define BATTLE_IGG_OBJ_TILE 640u
#define BATTLE_XANAF_OBJ_LEFT 0u
#define BATTLE_IGG_OBJ 1u
#define BATTLE_XANAF_OBJ_RIGHT 2u

/* Mode 4 reserves OBJ VRAM below 0x06014000.  Use the legal upper 16 KiB
 * bitmap-mode OBJ window and keep each pre-lowered 64x64 8bpp frame in one
 * 4096-byte slot.  Both Xanafalgues share one source frame. */
static void battle_obj64(unsigned oi,int x,int y,unsigned tile,int visible){
    if(!visible){hw_hide_obj(oi);return;}
    OAM16[oi*4u]=(uint16_t)((y&255)|0x2000u); /* 8bpp, square */
    OAM16[oi*4u+1u]=(uint16_t)((x&511)|0xC000u); /* 64x64 */
    OAM16[oi*4u+2u]=(uint16_t)(tile&0x03FFu);
    OAM16[oi*4u+3u]=0u;
}

/* v0.9.8 source BattleObj scratch layer. Mode 4 exposes only the upper 16 KiB
 * of OBJ VRAM; tiles 512..767 are the two 64x64 event-idle slots and 768..1023
 * are deliberately reused for one attack frame at a time. */
#define BATTLE_ATTACK_OBJ_TILE 768u
#define BATTLE_ATTACK_OBJ0 16u
#define BATTLE_ATTACK_OBJ1 17u
static int battle_enemy_source_center_x(unsigned i);
static int battle_obj_geometry(unsigned w,unsigned h,unsigned* shape,unsigned* size){
    if(w==h&&(w==8u||w==16u||w==32u||w==64u)){*shape=0u;*size=w==8u?0u:(w==16u?1u:(w==32u?2u:3u));return 1;}
    if((w==16u&&h==8u)||(w==32u&&h==8u)||(w==32u&&h==16u)||(w==64u&&h==32u)){*shape=1u;*size=(w==16u)?0u:((h==8u)?1u:((w==32u)?2u:3u));return 1;}
    if((w==8u&&h==16u)||(w==8u&&h==32u)||(w==16u&&h==32u)||(w==32u&&h==64u)){*shape=2u;*size=(h==16u)?0u:((w==8u)?1u:((w==16u)?2u:3u));return 1;}
    return 0;
}
static void battle_obj_any(unsigned oi,int cx,int cy,unsigned w,unsigned h,unsigned tile,int hflip,int priority,int visible){
    if(!visible){hw_hide_obj(oi);return;}unsigned shape=0u,size=0u;if(!battle_obj_geometry(w,h,&shape,&size)){hw_hide_obj(oi);return;}
    int x=cx-(int)(w>>1),y=cy-(int)(h>>1);
    OAM16[oi*4u]=(uint16_t)((y&255)|0x2000u|(shape<<14));
    OAM16[oi*4u+1u]=(uint16_t)((x&511)|(hflip?0x1000u:0u)|(size<<14));
    OAM16[oi*4u+2u]=(uint16_t)((tile&0x03FFu)|((unsigned)(priority&3)<<10));OAM16[oi*4u+3u]=0u;
}
/* v0.7 source-ability layer.  Generated TECH/SKILL art remains the
 * original PSIV 16-colour material, so it is kept as 4bpp rather than being
 * inflated to the older 8bpp attack carrier.  Palette bank 15 is reserved for
 * the currently active authored effect. */
#define BATTLE_SOURCE_ABILITY_PALBANK 15u
#define BATTLE_SOURCE_ABILITY_OBJ0 64u
#define BATTLE_SOURCE_ABILITY_OBJN 48u
/* CP16: source-effect part descriptors replace their old width/height +
 * center fields with the exact prebound OBJ geometry and top-left offsets.
 * Source effects never flip and always use priority 1 / palette bank 15, so
 * presentation now patches only anchor position and destination tile. */
static void battle_source_obj_prebound4(unsigned oi,int anchor_x,int anchor_y,int ox,int oy,unsigned geom,unsigned tile){
    unsigned shape=geom&3u,size=(geom>>2)&3u;
    int x=anchor_x+ox,y=anchor_y+oy;
    OAM16[oi*4u]=(uint16_t)((y&255)|(shape<<14));
    OAM16[oi*4u+1u]=(uint16_t)((x&511)|(size<<14));
    OAM16[oi*4u+2u]=(uint16_t)((tile&0x03FFu)|(1u<<10)|(BATTLE_SOURCE_ABILITY_PALBANK<<12));OAM16[oi*4u+3u]=0u;
}
static unsigned battle_source_effect_active_oam;
static void battle_source_ability_hide_all(void){for(unsigned i=0u;i<battle_source_effect_active_oam;i++)hw_hide_obj(BATTLE_SOURCE_ABILITY_OBJ0+i);battle_source_effect_active_oam=0u;}
static unsigned battle_obj_frame_bytes(unsigned w,unsigned h){return w*h;}

/* CP15: destination-aware OBJ effect residency.  Authored holds and repeated
 * child generations often request the exact same immutable payload at the same
 * scratch destination on consecutive display ticks.  Keep a deliberately
 * small cache of proven live spans.  Any overlapping writer invalidates the old
 * owner before installing its new key, so a skipped DMA is possible only while
 * the exact source pointer still owns the complete destination span. */
#define BATTLE_EFFECT_RESIDENT_SLOTS 12u
typedef struct BattleEffectResident {
    const uint8_t* src;
    uint16_t tile;
    uint16_t tiles;
} BattleEffectResident;
static BattleEffectResident battle_effect_resident[BATTLE_EFFECT_RESIDENT_SLOTS];
static unsigned battle_effect_resident_evict;
static void battle_effect_residency_reset(void){
    for(unsigned i=0u;i<BATTLE_EFFECT_RESIDENT_SLOTS;i++)battle_effect_resident[i].tiles=0u;
    battle_effect_resident_evict=0u;
}
static int battle_effect_upload(const uint8_t* src,unsigned bytes,unsigned tile){
    if(!bytes)return 0;
    unsigned tiles=(bytes+31u)>>5;
    for(unsigned i=0u;i<BATTLE_EFFECT_RESIDENT_SLOTS;i++){
        const BattleEffectResident* r=&battle_effect_resident[i];
        if(r->tiles&&r->tile==tile&&r->tiles==tiles&&r->src==src)return 0;
    }
    unsigned end=tile+tiles,free_slot=BATTLE_EFFECT_RESIDENT_SLOTS;
    for(unsigned i=0u;i<BATTLE_EFFECT_RESIDENT_SLOTS;i++){
        BattleEffectResident* r=&battle_effect_resident[i];
        if(!r->tiles){if(free_slot==BATTLE_EFFECT_RESIDENT_SLOTS)free_slot=i;continue;}
        unsigned rend=(unsigned)r->tile+(unsigned)r->tiles;
        if(tile<rend&&(unsigned)r->tile<end){r->tiles=0u;if(free_slot==BATTLE_EFFECT_RESIDENT_SLOTS)free_slot=i;}
    }
    dma_copy32(src,(void*)((uintptr_t)OBJ_VRAM16+tile*32u),bytes);
    if(free_slot==BATTLE_EFFECT_RESIDENT_SLOTS){free_slot=battle_effect_resident_evict++;if(battle_effect_resident_evict>=BATTLE_EFFECT_RESIDENT_SLOTS)battle_effect_resident_evict=0u;}
    battle_effect_resident[free_slot].src=src;
    battle_effect_resident[free_slot].tile=(uint16_t)tile;
    battle_effect_resident[free_slot].tiles=(uint16_t)tiles;
    return 1;
}
static void battle_obj_upload_attack(const uint8_t* src,unsigned bytes,unsigned tile){(void)battle_effect_upload(src,bytes,tile);}
static void battle_attack_hide(void){hw_hide_obj(BATTLE_ATTACK_OBJ0);hw_hide_obj(BATTLE_ATTACK_OBJ1);}
/* Source loc_10ED4 composes every enemy mapping backward from $FFFF8780 in
 * Plane_A_Buffer ($FFFF8000): row 15 is therefore the exclusive bottom edge,
 * i.e. source Y=120.  Battle backgrounds are projected 320x224 -> 240x160,
 * so preserve the same authored plane edge at floor(120*160/224)=85. */
#define BATTLE_ENEMY_SOURCE_FLOOR_Y 120u
#define BATTLE_ENEMY_FLOOR_Y ((BATTLE_ENEMY_SOURCE_FLOOR_Y*5u)/7u)
/* CP13: event-0 bodies are now preprojected with the same full-scene nearest
 * sample phase as the 320x224 -> 240x160 background. Source y=56..119 becomes
 * GBA y=40..85 (46 visible rows), centered in the unchanged 64x64 OBJ carrier
 * with nine transparent rows above/below. */
#define BATTLE_EVENT_IDLE_VIS_TOP_Y 40
#define BATTLE_EVENT_IDLE_TOP_Y (BATTLE_EVENT_IDLE_VIS_TOP_Y-9)
static int battle_enemy_anchor_x(unsigned i){
    return battle_enemy_source_center_x(i);
}
static int battle_enemy_native_origin(unsigned i,unsigned enemy_id,int* sx,int* sy);
static int battle_enemy_anchor_y(unsigned i){
    /* Event battle 0 uses exact 64x64 carriers.  Their authored opaque bounds
     * are Xanafalgue y=23..63 and Igglanova y=6..63; center those visible
     * bounds after placing local row 64 on the source-derived floor. */
    if(battle_event_index==0u){
        /* Opaque source bounds are Xanafalgue y=23..63 and Igglanova y=6..63
         * inside the 64x64 carrier.  After CP13's exact battle-plane projection
         * their visible centers are ~70.5 and ~64.5 respectively.  Keep effect
         * anchors on those projected bodies instead of the former unscaled
         * 64/55 positions. */
        static const int y[3]={71,65,71};return i<3u?y[i]:65;
    }
    /* Generic effects should target the center of the real source mapping,
     * not the old universal 64x64 harness.  The cache ID is bound from the
     * current formation before actions execute. */
    if(i<BATTLE_MAX_ENEMIES&&battle_enemy_anim_cache_valid[i]){
        const uint32_t* pm=battle_enemy_project_meta(battle_enemy_anim_cache_id[i]);
        if(pm&&pm[2])return (int)pm[3]+(int)(pm[2]/2u);
        const uint32_t* nm=battle_enemy_native_meta(battle_enemy_anim_cache_id[i]);
        if(nm&&nm[7])return (int)BATTLE_ENEMY_FLOOR_Y-(int)(nm[7]*4u);
    }
    return (int)BATTLE_ENEMY_FLOOR_Y-32;
}
static void battle_obj_upload_xanaf(void){
    (void)battle_effect_upload(battle_xanaf_anim_pixels(),4096u,BATTLE_XANAF_OBJ_TILE);
}
static void battle_obj_upload_igg(void){
    (void)battle_effect_upload(battle_igg_anim_pixels(),4096u,BATTLE_IGG_OBJ_TILE);
}
static void battle_obj_sync(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned changed){
    /* Only event formation 0 currently has its exact source idle animation
     * pre-lowered into legal Mode-4 OBJ VRAM. Other event formations are
     * drawn from their real source enemy art into the framebuffer below. */
    if(battle_event_index!=0u){hw_hide_obj(BATTLE_XANAF_OBJ_LEFT);hw_hide_obj(BATTLE_IGG_OBJ);hw_hide_obj(BATTLE_XANAF_OBJ_RIGHT);return;}
    if(changed&1u)battle_obj_upload_xanaf();
    if(changed&2u)battle_obj_upload_igg();
    battle_obj64(BATTLE_XANAF_OBJ_LEFT,battle_enemy_anchor_x(0u)-32,BATTLE_EVENT_IDLE_TOP_Y,BATTLE_XANAF_OBJ_TILE,enemies[0].hp!=0u);
    battle_obj64(BATTLE_IGG_OBJ,battle_enemy_anchor_x(1u)-32,BATTLE_EVENT_IDLE_TOP_Y,BATTLE_IGG_OBJ_TILE,enemies[1].hp!=0u);
    battle_obj64(BATTLE_XANAF_OBJ_RIGHT,battle_enemy_anchor_x(2u)-32,BATTLE_EVENT_IDLE_TOP_Y,BATTLE_XANAF_OBJ_TILE,enemies[2].hp!=0u);
}


/* Enemy animation is pre-lowered from EnemySpriteMappingsOffs/DrawAnimTile.
 * The schedule bytes are one state index per original 60 Hz Genesis frame.
 * Advancing is deliberately add/compare/subtract only: no modulo/divide enters
 * the battle hot path.  Both Xanafalgues start from the same source phase. */
static unsigned battle_xanaf_anim_tick;
static unsigned battle_igg_anim_tick;
static uint8_t battle_xanaf_anim_state;
static uint8_t battle_igg_anim_state;
static void battle_anim_reset(void){
    battle_effect_residency_reset();
    battle_xanaf_anim_tick=0u;battle_igg_anim_tick=0u;
    battle_xanaf_anim_state=ps4_precomp_assets[PS4_PRECOMP_XANAF_ANIM_SCHEDULE_OFF];
    battle_igg_anim_state=ps4_precomp_assets[PS4_PRECOMP_IGG_ANIM_SCHEDULE_OFF];
    for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++)for(unsigned op=0u;op<BATTLE_ENEMY_ANIM_COMPONENTS;op++){battle_enemy_anim_frame[i][op]=0u;battle_enemy_anim_timer[i][op]=0u;}
}
static unsigned battle_anim_advance(void){
    uint8_t old_x=battle_xanaf_anim_state,old_i=battle_igg_anim_state;unsigned changed=0u;
    ++battle_xanaf_anim_tick;if(battle_xanaf_anim_tick>=PS4_PRECOMP_XANAF_ANIM_PERIOD)battle_xanaf_anim_tick=0u;
    ++battle_igg_anim_tick;if(battle_igg_anim_tick>=PS4_PRECOMP_IGG_ANIM_PERIOD)battle_igg_anim_tick=0u;
    battle_xanaf_anim_state=ps4_precomp_assets[PS4_PRECOMP_XANAF_ANIM_SCHEDULE_OFF+battle_xanaf_anim_tick];
    battle_igg_anim_state=ps4_precomp_assets[PS4_PRECOMP_IGG_ANIM_SCHEDULE_OFF+battle_igg_anim_tick];
    if(old_x!=battle_xanaf_anim_state)changed|=1u;if(old_i!=battle_igg_anim_state)changed|=2u;return changed;
}
static const uint8_t* battle_xanaf_anim_pixels(void){
    return ps4_precomp_assets+PS4_PRECOMP_XANAF_ANIM_PIXELS_OFF+(unsigned)battle_xanaf_anim_state*4096u;
}
static const uint8_t* battle_igg_anim_pixels(void){
    return ps4_precomp_assets+PS4_PRECOMP_IGG_ANIM_PIXELS_OFF+(unsigned)battle_igg_anim_state*4096u;
}

static int battle_enemy_source_center_x(unsigned i){
    if(i>=BATTLE_MAX_ENEMIES)return 120;
    /* Source loc_7960: x = $80 + ((placement & $7F) << 3).  Project the
     * 320-wide Genesis battle coordinate to the 240-wide GBA viewport around
     * the same $120 center.  Division by four is a compile-time shift. */
    int sx=0x80+(int)((battle_enemy_placement[i]&0x7Fu)<<3);
    return 120+((sx-0x120)*3)/4;
}
static uint8_t battle_party_anim_actor=0xFFu;
static uint8_t battle_party_anim_frame=0u;
static uint8_t battle_party_anim_attack=0u;
static uint8_t battle_party_pose_actor=0xFFu;
static uint8_t battle_party_pose_index=0xFFu;
/* R21: source loc_9F7A/loc_9FCC temporarily move the acting fighter onto the
 * target X (or source center $120).  Keep this separate from the authored
 * party slot positions so the reset is a single state clear. */
static uint8_t battle_party_x_override_actor=0xFFu;
static int16_t battle_party_x_override_value;
/* Character_Dead owns party-plane visibility independently of HP.  A dead
 * character flashes/removes once, then stays absent until revived. */
static uint8_t battle_party_visible_mask=0x1Fu;
static uint8_t battle_party_death_done_mask=0u;
static const uint8_t* battle_party_idle_pixels(unsigned id){
    switch(id){
        case PS4_PRECOMP_PARTY_CHAZ_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_CHAZ_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_CHAZ_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_ALYS_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_ALYS_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_ALYS_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_HAHN_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_HAHN_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_HAHN_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_RUNE_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_RUNE_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_RUNE_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_GRYZ_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_GRYZ_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_GRYZ_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_RIKA_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_RIKA_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_RIKA_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_DEMI_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_DEMI_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_DEMI_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_WREN_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_WREN_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_WREN_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_RAJA_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_RAJA_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_RAJA_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_KYRA_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_KYRA_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_KYRA_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        case PS4_PRECOMP_PARTY_SETH_ID:return ps4_precomp_assets+PS4_PRECOMP_PARTY_SETH_POSES_PIXELS_OFF+PS4_PRECOMP_PARTY_SETH_POSE_IDLE_INDEX*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H;
        default:return 0;
    }
}
static const uint8_t* battle_party_pose_pixels(unsigned id,unsigned pose){
    switch(id){
        case PS4_PRECOMP_PARTY_CHAZ_ID:return pose<PS4_PRECOMP_PARTY_CHAZ_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_CHAZ_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_ALYS_ID:return pose<PS4_PRECOMP_PARTY_ALYS_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_ALYS_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_HAHN_ID:return pose<PS4_PRECOMP_PARTY_HAHN_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_HAHN_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_RUNE_ID:return pose<PS4_PRECOMP_PARTY_RUNE_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_RUNE_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_GRYZ_ID:return pose<PS4_PRECOMP_PARTY_GRYZ_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_GRYZ_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_RIKA_ID:return pose<PS4_PRECOMP_PARTY_RIKA_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_RIKA_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_DEMI_ID:return pose<PS4_PRECOMP_PARTY_DEMI_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_DEMI_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_WREN_ID:return pose<PS4_PRECOMP_PARTY_WREN_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_WREN_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_RAJA_ID:return pose<PS4_PRECOMP_PARTY_RAJA_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_RAJA_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_KYRA_ID:return pose<PS4_PRECOMP_PARTY_KYRA_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_KYRA_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_SETH_ID:return pose<PS4_PRECOMP_PARTY_SETH_POSES_FRAMES?ps4_precomp_assets+PS4_PRECOMP_PARTY_SETH_POSES_PIXELS_OFF+pose*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        default:return 0;
    }
}
static int battle_party_cast_pose_index(unsigned id){
    switch(id){
        case PS4_PRECOMP_PARTY_CHAZ_ID:return PS4_PRECOMP_PARTY_CHAZ_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_ALYS_ID:return PS4_PRECOMP_PARTY_ALYS_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_HAHN_ID:return PS4_PRECOMP_PARTY_HAHN_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_RUNE_ID:return PS4_PRECOMP_PARTY_RUNE_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_GRYZ_ID:return PS4_PRECOMP_PARTY_GRYZ_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_RIKA_ID:return PS4_PRECOMP_PARTY_RIKA_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_DEMI_ID:return PS4_PRECOMP_PARTY_DEMI_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_WREN_ID:return PS4_PRECOMP_PARTY_WREN_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_RAJA_ID:return PS4_PRECOMP_PARTY_RAJA_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_KYRA_ID:return PS4_PRECOMP_PARTY_KYRA_POSE_CAST_INDEX;
        case PS4_PRECOMP_PARTY_SETH_ID:return PS4_PRECOMP_PARTY_SETH_POSE_CAST_INDEX;
        default:return -1;
    }
}
static int battle_party_center_x(unsigned slot){
    /* Character_Sprite_X_Positions: center-out party order, not evenly spaced
     * GBA slots.  $120,$0F0,$150,$0C0,$180 projected 3/4 to 240 px. */
    static const uint16_t source_x[5]={0x120u,0x0F0u,0x150u,0x0C0u,0x180u};
    if(slot>=5u)return 120;return 120+(((int)source_x[slot]-0x120)*3)/4;
}
static const uint8_t* battle_party_attack_pixels(unsigned id,unsigned frame,const uint8_t** holds,unsigned* frames){
    *holds=0;*frames=0u;
    switch(id){
        case PS4_PRECOMP_PARTY_CHAZ_ID:*holds=ps4_precomp_assets+PS4_PRECOMP_PARTY_CHAZ_ATTACK_HOLDS_OFF;*frames=PS4_PRECOMP_PARTY_CHAZ_ATTACK_FRAMES;return frame<*frames?ps4_precomp_assets+PS4_PRECOMP_PARTY_CHAZ_ATTACK_PIXELS_OFF+frame*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_HAHN_ID:*holds=ps4_precomp_assets+PS4_PRECOMP_PARTY_HAHN_ATTACK_HOLDS_OFF;*frames=PS4_PRECOMP_PARTY_HAHN_ATTACK_FRAMES;return frame<*frames?ps4_precomp_assets+PS4_PRECOMP_PARTY_HAHN_ATTACK_PIXELS_OFF+frame*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        case PS4_PRECOMP_PARTY_RIKA_ID:*holds=ps4_precomp_assets+PS4_PRECOMP_PARTY_RIKA_ATTACK_HOLDS_OFF;*frames=PS4_PRECOMP_PARTY_RIKA_ATTACK_FRAMES;return frame<*frames?ps4_precomp_assets+PS4_PRECOMP_PARTY_RIKA_ATTACK_PIXELS_OFF+frame*PS4_PRECOMP_PARTY_CHAZ_W*PS4_PRECOMP_PARTY_CHAZ_H:0;
        default:return 0;
    }
}
typedef uint16_t battle_u16_alias __attribute__((may_alias));
#define BATTLE_PARTY_W PS4_PRECOMP_PARTY_CHAZ_W
#define BATTLE_PARTY_H PS4_PRECOMP_PARTY_CHAZ_H
__attribute__((noinline))
static void battle_blit_party_projected(const uint8_t* px,int left,unsigned sy){
    /* v0.9.10.2: party art is already projected to 36x34 by the build-time
     * source asset converter. 36px leaves the source-derived center-out left
     * edges halfword-aligned rather than word-aligned, so use packed 16-bit
     * writes: two opaque pixels commit together, while color 0 remains
     * transparent. This is ARM7TDMI-safe and still halves the common store
     * count versus byte-at-a-time composition. */
    if(!px||left<0||left+(int)BATTLE_PARTY_W>240||sy+BATTLE_PARTY_H>160u||(left&1))return;
    for(unsigned y=0u;y<BATTLE_PARTY_H;y++){
        unsigned py=sy+y;if(battle_compose_clip_active&&(py<battle_compose_clip_y0||py>=battle_compose_clip_y1))continue;
        const battle_u16_alias* src=(const battle_u16_alias*)(const void*)(px+y*BATTLE_PARTY_W);
        battle_u16_alias* dst=(battle_u16_alias*)(void*)(field_pause_fb+py*240u+(unsigned)left);
        for(unsigned q=0u;q<BATTLE_PARTY_W/2u;q++){
            int pair_x=left+(int)(q*2u);if(battle_compose_clip_active&&(pair_x+2<=battle_compose_clip_x0||pair_x>=battle_compose_clip_x1))continue;
            uint16_t w=src[q];if(!w)continue;
            uint8_t lo=(uint8_t)w,hi=(uint8_t)(w>>8);
            if(lo&&hi){dst[q]=w;continue;}
            uint8_t* d=(uint8_t*)(void*)&dst[q];if(lo)d[0]=lo;if(hi)d[1]=hi;
        }
    }
}

__attribute__((noinline))
static void battle_draw_party_frame(unsigned slot,unsigned id){
    if(slot<5u&&!(battle_party_visible_mask&(1u<<slot)))return;
    if(id>=11u&&id<14u){const uint8_t* px=r28_vehicle_pixels[id-11u];
        for(unsigned y=0u;y<74u;y++){unsigned py=y+86u;if(battle_compose_clip_active&&(py<battle_compose_clip_y0||py>=battle_compose_clip_y1))continue;
            const battle_u16_alias* src=(const battle_u16_alias*)(const void*)(px+y*240u);battle_u16_alias* dst=(battle_u16_alias*)(void*)(field_pause_fb+py*240u);
            for(unsigned x=0u;x<240u;x+=2u){if(battle_compose_clip_active&&((int)x+2<=battle_compose_clip_x0||(int)x>=battle_compose_clip_x1))continue;uint16_t q=src[x/2u];if(!q)continue;uint8_t lo=(uint8_t)q,hi=(uint8_t)(q>>8);
                if(lo&&hi&&(!battle_compose_clip_active||((int)x>=battle_compose_clip_x0&&(int)x+1<battle_compose_clip_x1)))dst[x/2u]=q;
                else{if(lo&&(!battle_compose_clip_active||(int)x>=battle_compose_clip_x0))field_pause_fb[py*240u+x]=lo;if(hi&&(!battle_compose_clip_active||(int)x+1<battle_compose_clip_x1))field_pause_fb[py*240u+x+1u]=hi;}}}
        return;
    }
    if(id==7u&&battle_party_pose_actor==slot&&battle_party_pose_index==PS4_PRECOMP_PARTY_WREN_POSE_LOAD_GUN_INDEX){
        int center=(battle_party_x_override_actor==slot)?(int)battle_party_x_override_value:battle_party_center_x(slot);
        int left=center-21;unsigned sy=(0x80u*5u)/7u;
        for(unsigned y=0u;y<34u;y++)for(unsigned x=0u;x<42u;x++){
            int dx=left+(int)x;unsigned dy=sy+y;if(dx<0||dx>=240||dy>=160u)continue;
            if(battle_compose_clip_active&&(dx<battle_compose_clip_x0||dx>=battle_compose_clip_x1||dy<battle_compose_clip_y0||dy>=battle_compose_clip_y1))continue;
            uint8_t ci=r29_wren_load[y*42u+x];if(ci)field_pause_fb[dy*240u+(unsigned)dx]=ci;
        }return;
    }
    const uint8_t* px=0;
    if(battle_party_pose_actor==slot)px=battle_party_pose_pixels(id,battle_party_pose_index);
    if(!px&&battle_party_anim_attack&&battle_party_anim_actor==slot){const uint8_t* h;unsigned n;px=battle_party_attack_pixels(id,battle_party_anim_frame,&h,&n);}
    if(!px)px=battle_party_idle_pixels(id);
    if(px){
        /* Character source Y is $80 at the rear-view battle baseline. Project
         * that source anchor with the same 5/7 vertical transform as the battle
         * background, keeping the smaller bodies immediately above the HUD. */
        int center=(battle_party_x_override_actor==slot)?(int)battle_party_x_override_value:battle_party_center_x(slot);
        int left=center-(int)(BATTLE_PARTY_W/2u);
        battle_blit_party_projected(px,left,(0x80u*5u)/7u);return;
    }
    /* Every valid PSIV party ID 0..10 has source rear-view art.  Fail closed
     * for corrupt IDs rather than resurrecting the old field-sprite battle
     * harness fallback. */
    return;
}

/* Direct lowering of Battle_SetupBackground ($6C00).  Event battles use
 * EventBattleBGIndexes; ordinary maps use Battle_BackgroundIndexes; Motavia's
 * world map derives its background from the source chunk ID under Character_1
 * via GetMotaBattleBGIndex/MotaBattleBGIndexes.  Background $04 changes to $05
 * after the second Dark Force event exactly as the original does. */
static unsigned battle_background_source_index(void){
    unsigned bg=0u;
    if((int8_t)battle_event_index>=0){
        unsigned e=battle_event_index;
        if(e<27u)bg=rom8(0x6C8Cu+e); /* EventBattleBGIndexes */
    }else if(field_map.map_id){
        if(field_map.map_id<0x1A1u){unsigned q=rom8(0x6CA8u+field_map.map_id);if(q!=0xFFu)bg=q;}
    }else{
        unsigned chunk=field_chunk_index_at(chaz.x,chaz.y,0,0);
        unsigned q=rom8(0x586ECu+chunk); /* MotaBattleBGIndexes */
        bg=q?q-1u:0u;
    }
    if(bg==4u&&ps4_event_flag_test(0x9Eu))bg=5u; /* EventFlag_DarkForce2 */
    return bg<PS4_PRECOMP_BATTLE_BG_COUNT?bg:0u;
}

static unsigned battle_src_y(unsigned row);
static void battle_ui_box_source(unsigned wx,unsigned wy,unsigned ww,unsigned wh,unsigned* ox0,unsigned* oy0,unsigned* ox1,unsigned* oy1);
static void battle_draw_rom_name6(unsigned x,unsigned y,uint32_t base,unsigned index,uint8_t delim,uint8_t c);
static void battle_draw_enemy_name6(unsigned x,unsigned y,unsigned id,uint8_t c);
static void battle_draw_enemy_skill_name6(unsigned x,unsigned y,unsigned id,uint8_t c);
static void battle_draw_cursor6(unsigned x,unsigned y,uint8_t fg);
static void battle_draw_enemy_target_overlay(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned target){
    if(target>=battle_enemy_count||!enemies[target].hp)return;
    /* Source EnemyTargetCursor occupies only the top name/cursor band. */
    unsigned x0,y0,x1,y1;battle_ui_box_source(0u,0u,18u,3u,&x0,&y0,&x1,&y1);
    battle_draw_enemy_name6(x0+6u,y0+6u,enemies[target].id,80u);
    int mx=battle_enemy_anchor_x(target)-3;if(mx<0)mx=0;battle_draw_cursor6((unsigned)mx,30u,84u);
}
static void ps4_ui_colors(void);
static void battle_draw_text6(unsigned x,unsigned y,const char* q,uint8_t c);
static void battle_draw_ram_name6(unsigned x,unsigned y,unsigned char_id,uint8_t c);
static void battle_draw_uint6(unsigned x,unsigned y,unsigned v,uint8_t c);
static uint16_t battle_damage_enemy[BATTLE_MAX_ENEMIES];
static uint16_t battle_damage_party[5];
static uint8_t battle_damage_enemy_mask,battle_damage_party_mask;
/* When a damage number expires, the old glyph pixels remain in the Mode-4
 * staging page until some later compositor overwrites them.  The v0.5 fast
 * pre-action presenter removes that small stale band explicitly instead of
 * forcing a whole-screen compose merely as an eraser. */
static uint8_t battle_damage_pixels_stale;
static uint8_t battle_palette_valid,battle_palette_hw_dirty;
static void battle_draw_damage_overlays(void);
static void battle_action_idle_after_wait(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]);
static void battle_draw_party_hud(const struct BattleLiteActor party[5],unsigned party_count,unsigned actor){
    if(!party)return;
    if(party_count==1u&&party[0].id>=11u){battle_draw_text6(98u,140u,"HP",80u);battle_draw_uint6(116u,140u,party[0].hp,80u);return;}
    /* FillBattleStats is one source lower battle window, but the five party
     * records are visually segmented. Keep one projected outer chrome and add
     * explicit source-style separators so adjacent names/HP/TP never visually
     * run together on the narrower GBA viewport. */
    unsigned hx0,hy0,hx1,hy1;battle_ui_box_source(2u,22u,35u,5u,&hx0,&hy0,&hx1,&hy1);
    const unsigned inner_l=hx0+6u,inner_r=hx1-6u;unsigned span=inner_r-inner_l;
    for(unsigned i=1u;i<5u;i++){unsigned x=inner_l+(span*i)/5u;if(x>=inner_r)break;
        for(unsigned y=hy0+6u;y<hy1-6u;y++){field_pause_fb[y*240u+x]=81u;if(x+1u<inner_r)field_pause_fb[y*240u+x+1u]=82u;}
    }
    /* Character bodies are source-positioned center-out rather than party-index
     * left-to-right.  Put each HUD record under the same screen column as its
     * body.  Because both are keyed by live party slot, ORDER and scripted
     * party changes move the sprite and its HP/TP box together. */
    static const uint8_t slot_to_cell[5]={2u,1u,3u,0u,4u};
    for(unsigned i=0u;i<party_count&&i<5u;i++){
        unsigned cell=slot_to_cell[i],cell0=inner_l+(span*cell)/5u,cell1=inner_l+(span*(cell+1u))/5u,x=cell0+2u;uint8_t namec=i==actor?83u:80u;
        battle_draw_ram_name6(x,hy0+5u,party[i].id,namec);
        battle_draw_text6(x,hy0+15u,"HP:",82u);battle_draw_uint6(x+18u,hy0+15u,party[i].hp,80u);
        battle_draw_text6(x,hy0+25u,"TP:",82u);battle_draw_uint6(x+18u,hy0+25u,party[i].tp,80u);
        (void)cell1;
    }
}
__attribute__((noinline))
static void battle_ui_discard_staged_anim(void);
static void battle_compose_igg(const struct BattleLiteActor party[5],unsigned party_count,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned target,unsigned actor){
    /* A canonical compose already consumes the current animation state.
     * Retire stale dirtiness before waitvb can restore an older VRAM window
     * over the new frame, or redundantly draw a large boss a second time. */
    battle_ui_discard_staged_anim();
    /* Prepare the next FIFO block before expensive background/UI composition so
     * opening a command window cannot starve Direct Sound until presentation. */
    ps4_audio_prepare_frame();
    /* Static source art is preconverted once.  The command/status windows use
     * PSIV's dark/double-edge presentation and support all five party slots. */
    unsigned bg=battle_background_source_index();
    dma_copy32(ps4_precomp_assets+PS4_PRECOMP_BATTLE_BG_PIXELS_OFF+bg*PS4_PRECOMP_BATTLE_BG_FRAME_SIZE,field_pause_fb,PS4_PRECOMP_BATTLE_BG_FRAME_SIZE);
    /* The battle background, enemy palette groups and window colors are fixed
     * for the lifetime of one formation.  Older builds rebuilt this 512-byte
     * shadow (including MD->GBA color work) and republished both palette banks
     * on every full page.  Latch it once; pixel-only battle updates keep using
     * the exact same indices. */
    if(!battle_palette_valid){
        dma_copy32(ps4_precomp_assets+PS4_PRECOMP_BATTLE_PALETTE_OFF,field_pause_pal,PS4_PRECOMP_PALETTE_SIZE);
        ps4_ui_colors();
        if(party&&party_count==1u&&party[0].id>=11u&&party[0].id<14u)dma_copy32(r28_vehicle_palette[party[0].id-11u],field_pause_pal+160u,32u);
        field_pause_pal[80u]=0x7FFFu;field_pause_pal[81u]=md2gba(0x0200u);field_pause_pal[82u]=md2gba(0x0E00u);field_pause_pal[83u]=md2gba(0x000Eu);field_pause_pal[84u]=md2gba(0x000Eu);
        if(battle_event_index!=0u)for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp){unsigned line=12u+((battle_enemy_placement[i]>>7)&1u);battle_palette_enemy(line,enemies[i].id);}
        battle_palette_valid=1u;battle_palette_hw_dirty=1u;
    }
    if(party){
        for(unsigned i=0;i<party_count&&i<5u;i++)battle_draw_party_frame(i,party[i].id);
    }
    if(battle_event_index!=0u){
        /* Generic event-formation presentation. Enemy graphics and palettes are
         * decoded from PSIV itself; formation X comes from the source record. */
        for(unsigned i=0u;i<battle_enemy_count;i++)if(battle_enemy_draw_visible(enemies,i)){
            int ex,ey;if(!battle_enemy_native_origin(i,enemies[i].id,&ex,&ey))continue;
            /* loc_7960 assigns one of exactly two enemy palette groups from
             * bit 7 of the formation placement byte.  Keep those groups in
             * reserved OBJ/BG palette lines 12/13; never invent per-enemy
             * lines that alias the battle/window palette. */
            unsigned line=12u+((battle_enemy_placement[i]>>7)&1u);battle_draw_enemy(i,enemies[i].id,ex,ey,line);
        }
    }
    /* Enemy names and the target cursor belong to BattleObj_EnemyTargetCursor,
     * not to the persistent battle compositor.  Source Y is $B0 ($D0 only in
     * vehicle battles); ordinary battles therefore project $B0 to this plane. */
    if(battle_show_enemy_target&&target<battle_enemy_count&&enemies[target].hp)battle_draw_enemy_target_overlay(enemies,target);
    battle_draw_party_hud(party,party_count,actor);
    battle_draw_damage_overlays();
    if(!battle_damage_enemy_mask&&!battle_damage_party_mask)battle_damage_pixels_stale=0u;
}

__attribute__((noinline))
static void battle_present_page0(void){
    /* P128-style dense transfer path: commit the full page and both palette
     * banks as DMA bursts.  The old 256-iteration palette store loop ran on
     * every battle composition even though palette RAM is DMA-addressable. */
    waitvb();
    dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,240u*160u);
    if(battle_palette_hw_dirty){dma_copy32(field_pause_pal,(void*)BG_PALETTE,512u);dma_copy32(field_pause_pal,(void*)OBJ_PALETTE,512u);battle_palette_hw_dirty=0u;}
    REG_BG2CNT=3u;REG_DISPCNT=(uint16_t)(MODE4|BG2_ENABLE|OBJ_1D|OBJ_ENABLE);
}
__attribute__((noinline))
static void battle_present_band(unsigned y0,unsigned y1){
    /* Battle command windows are opaque and redraw their complete scanline
     * band.  Once the base scene is on screen, cursor/list changes therefore
     * need only one contiguous DMA burst instead of a 38.4 KiB page copy. */
    if(y0>160u)y0=160u;if(y1>160u)y1=160u;if(y1<=y0){waitvb();return;}
    unsigned off=y0*240u,bytes=(y1-y0)*240u;
    waitvb();dma_copy32(field_pause_fb+off,(void*)((uintptr_t)MODE4_VRAM16+off),bytes);
}
static void battle_wait_release(void){for(;;){waitvb();if(!(md_io_read_psiv_pad()&0xF0u))return;}}
static void battle_party_death_reset(const struct BattleLiteActor party[5],unsigned pn){
    battle_party_visible_mask=0u;battle_party_death_done_mask=0u;
    for(unsigned i=0u;i<pn&&i<5u;i++){
        if(party[i].hp)battle_party_visible_mask|=(uint8_t)(1u<<i);
        else battle_party_death_done_mask|=(uint8_t)(1u<<i);
    }
}
static void battle_party_revive_visibility(const struct BattleLiteActor party[5],unsigned pn){
    for(unsigned i=0u;i<pn&&i<5u;i++)if(party[i].hp){battle_party_visible_mask|=(uint8_t)(1u<<i);battle_party_death_done_mask&=(uint8_t)~(1u<<i);}
}
/* PSCSTYLE v0.4 speed pass (START_HERE_CLAUDE.md Priority 1, "particularly
 * expensive obvious case"): the $20-frame death flash used to call
 * battle_compose_igg()+battle_present_page0() every single tick, rebuilding
 * and retransferring the entire 240x160 Mode-4 page (>76.8 KiB/frame) merely
 * to toggle one character's visibility bit.
 *
 * Across this loop, party/enemies/target and every other compositor input
 * are frozen except battle_party_visible_mask's one bit, so the only pixels
 * that can legally differ between two "visible" frames -- or between two
 * "hidden" frames -- are inside the dying character's own fixed sprite
 * rectangle (battle_party_center_x(i) x [sy,sy+BATTLE_PARTY_H)), *unless*
 * enemy idle animation also advanced this tick (battle_draw_enemy() paints
 * enemies straight into field_pause_fb for every formation except event
 * index 0, whose three slots are pure OBJ hardware sprites synced by
 * battle_obj_sync() and never touch field_pause_fb at all).
 *
 * So: composite exactly once per visibility state, cache that state's
 * rectangle, and on every later frame that only repeats a visibility state
 * already cached, replace the full recompose+present with a byte-exact
 * rectangle restore plus a battle_present_band() covering just those rows.
 * battle_enemy_anim_advance_generic()'s return value -- previously
 * discarded -- now invalidates both cached rectangles whenever it reports a
 * change, so any frame that could actually differ elsewhere on screen is
 * rebuilt through the original, unabridged path. Worst case (enemies
 * animate every tick) this degrades to exactly the prior behavior; it never
 * does less compositing work than correctness allows. */
#define BATTLE_DEATH_FLASH_RECT_BYTES (BATTLE_PARTY_W*BATTLE_PARTY_H)
static uint8_t battle_death_flash_rect_a[BATTLE_DEATH_FLASH_RECT_BYTES]; /* bit set (visible) */
static uint8_t battle_death_flash_rect_b[BATTLE_DEATH_FLASH_RECT_BYTES]; /* bit clear (hidden) */
static void battle_death_flash_capture(uint8_t* dst,int left,unsigned sy){
    for(unsigned y=0u;y<BATTLE_PARTY_H;y++){
        const uint8_t* src=field_pause_fb+(sy+y)*240u+(unsigned)left;uint8_t* d=dst+y*BATTLE_PARTY_W;
        for(unsigned x=0u;x<BATTLE_PARTY_W;x++)d[x]=src[x];
    }
}
static void battle_death_flash_restore(const uint8_t* src,int left,unsigned sy){
    for(unsigned y=0u;y<BATTLE_PARTY_H;y++){
        uint8_t* dst=field_pause_fb+(sy+y)*240u+(unsigned)left;const uint8_t* s=src+y*BATTLE_PARTY_W;
        for(unsigned x=0u;x<BATTLE_PARTY_W;x++)dst[x]=s[x];
    }
}
static void battle_process_new_party_deaths(struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned target){
    for(unsigned i=0u;i<pn&&i<5u;i++){
        uint8_t bit=(uint8_t)(1u<<i);if(party[i].hp||(battle_party_death_done_mask&bit))continue;
        /* Character_Dead ($849A): $20 frames of plane-map removal/flashing,
         * then organic status bit 2 or Android status bit 6 is committed. */
        int left=battle_party_center_x(i)-(int)(BATTLE_PARTY_W/2u);unsigned sy=(0x80u*5u)/7u;
        /* Mirror battle_blit_party_projected's own bounds guard: if this slot's
         * rectangle would ever fall outside the framebuffer, never trust the
         * cache and always take the exact original per-frame path below. */
        int geom_ok=left>=0&&left+(int)BATTLE_PARTY_W<=240&&sy+BATTLE_PARTY_H<=160u;
        int have_a=0,have_b=0;
        for(unsigned f=0u;f<0x20u;f++){
            int visible=!(f&1u);
            if(visible)battle_party_visible_mask|=bit;else battle_party_visible_mask&=(uint8_t)~bit;
            int cached=geom_ok&&(visible?have_a:have_b);
            if(!cached){
                battle_compose_igg(party,pn,enemies,target,i);battle_present_page0();
                if(geom_ok){if(visible)battle_death_flash_capture(battle_death_flash_rect_a,left,sy);else battle_death_flash_capture(battle_death_flash_rect_b,left,sy);}
                if(visible)have_a=1;else have_b=1;
            }else{
                ps4_audio_prepare_frame();
                battle_death_flash_restore(visible?battle_death_flash_rect_a:battle_death_flash_rect_b,left,sy);
                battle_present_band(sy,sy+BATTLE_PARTY_H);
            }
            battle_obj_sync(enemies,0u);
            if(battle_event_index==0u){unsigned changed=battle_anim_advance();if(changed)battle_obj_sync(enemies,changed);}
            else{unsigned changed=battle_enemy_anim_advance_generic(enemies);if(changed){have_a=0;have_b=0;}}
        }
        battle_party_visible_mask&=(uint8_t)~bit;battle_party_death_done_mask|=bit;
        if(party[i].android)party[i].status=(uint8_t)((party[i].status&0x10u)|0x40u);
        else party[i].status=(uint8_t)((party[i].status&0x10u)|0x04u);
    }
}
static uint8_t ps4_return_to_title;
static void battle_show_defeated_message(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned target){
    unsigned who=0u;while(who+1u<pn&&party[who].hp)++who;
    battle_compose_igg(party,pn,enemies,target,who);unsigned dx0,dy0,dx1,dy1;
    /* Character_Dead message: project the source 20x5 outer window as an
     * 18x3 interior rather than drawing its Genesis pixels at 1:1 GBA size. */
    battle_ui_box_source(5u,7u,18u,3u,&dx0,&dy0,&dx1,&dy1);
    battle_draw_ram_name6(dx0+6u,dy0+6u,party[who].id,80u);unsigned name_len=0u;uint16_t np=battle_char_record((unsigned)party[who].id);while(name_len<8u){uint8_t q=ram8r((uint16_t)(np+name_len));if(q==0xFEu||q==0xFFu)break;++name_len;}battle_draw_text6(dx0+6u+name_len*6u,dy0+6u," DEFEATED...!",80u);
    battle_present_page0();battle_obj_sync(enemies,0u);
    for(unsigned f=0u;f<0x78u;f++){
        if(battle_event_index==0u){waitvb();unsigned changed=battle_anim_advance();if(changed)battle_obj_sync(enemies,changed);}else waitvb();
    }
    /* Battle routines $29/$2A: fade and tear the battle presentation down,
     * clear Game_Mode_Index, and return to the title dispatcher.  Never let
     * the field loop poll System/Menu after an all-party-dead result. */
    field_palette_fade_out();for(unsigned oi=0u;oi<128u;oi++)hw_hide_obj(oi);waitvb();REG_DISPCNT=FORCE_BLANK;REG_BLDCNT=0u;REG_BLDY=0u;
    if(battle_debug_battle_active)return;ram16w(0xEF00u,0u);ram16w(0xEC20u,0u);ps4_audio_stop();ps4_return_to_title=1u;
}
static int battle_party_alive(const struct BattleLiteActor p[5],unsigned n){for(unsigned i=0;i<n;i++)if(p[i].hp)return 1;return 0;}
static int battle_enemies_alive(const struct BattleLiteActor e[BATTLE_MAX_ENEMIES]){for(unsigned i=0u;i<battle_enemy_count;i++)if(e[i].hp)return 1;return 0;}
static unsigned battle_enemy_alive_count(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){unsigned n=0u;for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp)++n;return n;}

/* v0.9.8: lower PSIV's complete EnemyAIInstructionsOffs predicate set.  Bits
 * 0/1/2/4 are the $2A combat-history bits written by physical/magic/tech/
 * multi-target player actions.  Bit 3 is the opening ambush condition. */
static uint8_t battle_enemy_reaction_flags[BATTLE_MAX_ENEMIES];
static uint8_t battle_enemy_ai_unknown_flag;
static uint8_t battle_enemy_ai_hp25_gate;
static uint8_t battle_enemy_last_regular_slot=0xFFu;
/* EnemyAI_EmptySpace writes $FFFFEE81 and the Fission object consumes that
 * selected side later.  Preserve the source decision so presentation never
 * draws a second RNG value.  -1=left, +1=right, 0=not selected. */
static int8_t battle_enemy_empty_side;
/* Encounter slots that have actually participated.  Event 0 contains two
 * dormant Xanafalgue formation records which EnemyInit_Igglanova clears and
 * Fission may reactivate later. */
static uint8_t battle_enemy_reward_mask;

static int battle_enemy_alive_id(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],int slot,unsigned id){
    return slot>=0 && (unsigned)slot<battle_enemy_count && enemies[slot].hp && enemies[slot].id==id;
}
static int battle_enemy_ai_condition(unsigned code,unsigned ei,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],const struct BattleLiteActor party[5],unsigned pn){
    (void)party;(void)pn;
    if(ei>=battle_enemy_count||!enemies[ei].hp)return 0;
    switch(code){
    case 0u: case 19u:return 0;
    case 1u:{ /* EnemyAI_EmptySpace */
        int l=(ei==0u)||!enemies[ei-1u].hp,r=(ei+1u>=battle_enemy_count)||!enemies[ei+1u].hp;
        battle_enemy_empty_side=0;
        if(!l&&!r)return 0;
        if(l&&r){uint8_t roll=(uint8_t)field_npc_rng_next();battle_enemy_empty_side=(roll&1u)?1:-1;}
        else battle_enemy_empty_side=l?-1:1;
        return 1;
    }
    case 2u:return enemies[ei].hp<=(enemies[ei].max_hp>>1); /* half HP */
    case 3u: /* Wiredine in center; actor/opposite outer geometry */
        if(!battle_enemy_alive_id(enemies,1,0x19u))return 0;
        if(ei==0u)return battle_enemy_count<=2u||!enemies[2].hp;
        if(ei>=2u)return !enemies[ei-2u].hp;
        return 0;
    case 4u:{ /* Arthro Pod on one outside slot with opposite outside empty */
        int e0=battle_enemy_count==0u||!enemies[0].hp,e2=battle_enemy_count<=2u||!enemies[2].hp;
        return (battle_enemy_alive_id(enemies,0,0x17u)&&e2)||(e0&&battle_enemy_alive_id(enemies,2,0x17u));
    }
    case 5u:return battle_enemy_alive_id(enemies,0,0x2Au)&&battle_enemy_alive_id(enemies,1,0x28u)&&battle_enemy_alive_id(enemies,2,0x2Au);
    case 6u:{unsigned n=0u;for(unsigned i=0u;i<battle_enemy_count;i++){if(!enemies[i].hp)continue;if(enemies[i].id!=0x22u)return 0;++n;}return n==2u;}
    case 7u:if(battle_enemy_reaction_flags[ei]&1u){battle_enemy_reaction_flags[ei]=0u;return 1;}return 0;
    case 8u:if(battle_enemy_reaction_flags[ei]&2u){battle_enemy_reaction_flags[ei]=0u;return 1;}return 0;
    case 9u:return battle_enemy_alive_count(enemies)==1u;
    case 10u:if(battle_enemy_reaction_flags[ei]&4u){battle_enemy_reaction_flags[ei]=0u;return 1;}return 0;
    case 11u:if(battle_enemy_reaction_flags[ei]&8u){battle_enemy_reaction_flags[ei]=0u;return 1;}return 0;
    case 12u:return (enemies[ei].status&0x10u)!=0u;
    case 13u: case 14u:{
        unsigned wanted=code==13u?0x56u:0x54u,forbid=code==13u?0x54u:0x56u,n=0u;
        for(unsigned i=0u;i<battle_enemy_count;i++){if(i==ei||!enemies[i].hp)continue;if(enemies[i].id==forbid)return 0;if(enemies[i].id==wanted)++n;}return n==1u;
    }
    case 15u:for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp&&enemies[i].hp<=(enemies[i].max_hp>>1))return 1;return 0;
    case 16u:if(battle_enemy_ai_unknown_flag){battle_enemy_ai_unknown_flag=0u;return 1;}return 0;
    case 17u:return !battle_enemy_ai_hp25_gate&&enemies[ei].hp<=(enemies[ei].max_hp>>2);
    case 18u:{
        if(!(battle_enemy_reaction_flags[ei]&0x10u))return 0;
        for(unsigned i=0u;i<battle_enemy_count&&i<3u;i++)battle_enemy_reaction_flags[i]&=(uint8_t)~0x10u;
        return battle_enemy_count>=3u&&battle_enemy_alive_id(enemies,0,0x7Bu)&&battle_enemy_alive_id(enemies,1,0x7Bu)&&battle_enemy_alive_id(enemies,2,0x7Bu);
    }
    default:return 0;
    }
}
static uint8_t battle_enemy_pick_skill(unsigned ei,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],const struct BattleLiteActor party[5],unsigned pn){
    battle_enemy_empty_side=0;
    uint32_t p=0x2816BCu+(uint32_t)enemies[ei].id*48u;
    /* Enemy_Attack rolls only the eight normal bytes $58-$5F and rejects the
     * previously selected slot ($FFFFEEA8).  Four condition/forced pairs then
     * override that normal choice in source order. */
    unsigned slot;do{slot=(unsigned)(field_npc_rng_next()&7u);}while(slot==battle_enemy_last_regular_slot);
    battle_enemy_last_regular_slot=(uint8_t)slot;uint8_t ability=rom8(p+36u+slot);
    for(unsigned i=0u;i<4u;i++){uint8_t cond=rom8(p+28u+i);if(cond&&battle_enemy_ai_condition(cond,ei,enemies,party,pn)){uint8_t forced=rom8(p+32u+i);if(forced)ability=forced;break;}}
    return ability;
}

/* Enemy_TargetCharacter: compact living list + source threshold tables. */
static int battle_select_enemy_target(const struct BattleLiteActor party[5],unsigned pn){
    uint8_t living[5];unsigned n=0u;for(unsigned i=0u;i<pn&&i<5u;i++)if(party[i].hp&&!(party[i].status&0x44u))living[n++]=(uint8_t)i;
    if(!n)return -1;if(n==1u)return living[0];
    static const uint8_t r2[1]={0x54u},r3[2]={0x80u,0x2Cu},r4[3]={0x99u,0x4Du,0x1Au},r5[4]={0xACu,0x66u,0x33u,0x12u};
    const uint8_t* rates=n==2u?r2:(n==3u?r3:(n==4u?r4:r5));uint8_t r=(uint8_t)field_npc_rng_next();
    for(unsigned i=0u;i+1u<n;i++)if(r>=rates[i])return living[i];return living[n-1u];
}
static int battle_party_class_valid(const struct BattleLiteActor* a,unsigned range){
    if(range==4u||range==5u)return !a->android;
    if(range==6u||range==7u)return a->android;
    return 1;
}
static int battle_party_target_valid(const struct BattleLiteActor* a,unsigned range){
    return a->hp && !(a->status&0x44u) && battle_party_class_valid(a,range);
}
static int battle_select_party_target_kind(const struct BattleLiteActor party[5],unsigned pn,unsigned range,unsigned preferred){
    if(preferred<pn&&battle_party_target_valid(&party[preferred],range))return (int)preferred;
    uint8_t candidates[5];unsigned n=0u;for(unsigned i=0u;i<pn&&i<5u;i++)if(battle_party_target_valid(&party[i],range))candidates[n++]=(uint8_t)i;
    if(!n)return -1;unsigned r=(unsigned)((uint8_t)field_npc_rng_next());unsigned pick=(r*n)>>8;if(pick>=n)pick=n-1u;return candidates[pick];
}
static unsigned battle_source_heal(unsigned mental,unsigned power);
static unsigned battle_enemy_skill_stat(const struct BattleLiteActor* e,unsigned selector){return battle_actor_stat(e,selector);}
static int battle_effect_hits(const struct BattleLiteActor* actor,const struct BattleLiteActor* target,unsigned actor_stat_selector,unsigned hit,unsigned resist,unsigned element){
    if(!resist)return 1;
    return battle_calc_chance(battle_actor_stat(actor,actor_stat_selector),battle_actor_stat(target,resist),battle_element_factor(target,element),hit,hit)>=0;
}
static int battle_physical_hit(const struct BattleLiteActor* actor,const struct BattleLiteActor* target,int multi,int* critical){
    /* loc_B716: DEX vs AGI, factor 2, miss bound $08 and critical bound $74.
     * Multi-target physical attacks suppress the critical result. */
    int q=battle_calc_chance(actor->dexterity,target->agility,2u,8u,0x74u);
    if(multi&&q>0)q=0;if(critical)*critical=q>0;return q>=0;
}
static void battle_apply_basic_attack_effect(const struct BattleLiteActor* actor,struct BattleLiteActor* target){
    /* Battle_DoAttackEffect uses an enemy's EnemyData byte +3 as a physical
     * ability effect. Poison ($1B) and paralysis ($1C) both call
     * Battle_ProcessEffect with d3=$48: actor STR vs target STR, target
     * property at stats offset $48 (element/property 13), lower bound $70. */
    unsigned effect=actor->basic_effect;if(!effect||!target->hp)return;
    if(effect==0x1Bu&&(target->status&1u))return;if(effect==0x1Cu&&(target->status&2u))return;
    unsigned factor=battle_element_factor(target,13u);
    if(battle_calc_chance(actor->strength,target->strength,factor,0x70u,effect)<0)return;
    if(effect==0x1Bu)target->status|=1u;
    else if(effect==0x1Cu){target->status|=2u;target->status&=(uint8_t)~8u;target->agility=1u;target->dexterity=1u;}
}
static void battle_enemy_skill_apply(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t skill,unsigned victim){
    if(ei>=battle_enemy_count||!enemies[ei].hp)return;
    if(!skill){
        if(victim>=pn||!party[victim].hp){int t=battle_select_enemy_target(party,pn);if(t<0)return;victim=(unsigned)t;}
        int critical=0;if(!battle_physical_hit(&enemies[ei],&party[victim],0,&critical))return;
        unsigned factor=battle_element_factor(&party[victim],enemies[ei].basic_element),power=critical?((enemies[ei].atk&0xFFu)>>2):0u;
        unsigned dmg=battle_calc_damage_ex(enemies[ei].atk,party[victim].def,factor,power);
        party[victim].hp=(uint16_t)(dmg>=party[victim].hp?0u:party[victim].hp-dmg);
        if(!party[victim].hp)party[victim].status|=4u;else battle_apply_basic_attack_effect(&enemies[ei],&party[victim]);return;
    }
    uint32_t q=0x28336Cu+(uint32_t)(skill-1u)*8u;unsigned effect=rom8(q),stat=rom8(q+1u)&0x7Fu,range=rom8(q+2u)&15u,power=rom8(q+3u),resist=rom8(q+4u),element=rom8(q+5u);if(!element)element=1u;
    if(range==4u||range==6u||range==8u){int t=battle_select_party_target_kind(party,pn,range,victim);if(t<0)return;victim=(unsigned)t;}
    /* Tech-sealed enemies cannot use records whose source statistic byte has bit 7. */
    if((enemies[ei].status&0x10u)&&(rom8(q+1u)&0x80u))return;
    if(range==3u){
        if(effect==9u)enemies[ei].atk=(uint16_t)(enemies[ei].atk+power);
        else if(effect==10u)enemies[ei].def=(uint16_t)(enemies[ei].def+power);
        else if(effect==11u)enemies[ei].magic_def=(uint16_t)(enemies[ei].magic_def+power);
        else if(effect==12u)enemies[ei].agility=(uint8_t)(enemies[ei].agility+power);
        else if(effect==0x12u||effect==0x16u){unsigned add=effect==0x16u?enemies[ei].max_hp:battle_source_heal(battle_enemy_skill_stat(&enemies[ei],stat),power);unsigned hp=enemies[ei].hp+add;if(hp>enemies[ei].max_hp)hp=enemies[ei].max_hp;enemies[ei].hp=(uint16_t)hp;}
        return;
    }
    unsigned lo=victim,hi=victim+1u;if(range==5u||range==7u||range==9u){lo=0u;hi=pn;}
    for(unsigned i=lo;i<hi&&i<pn;i++){
        if((range==5u||range==7u||range==9u)&&!battle_party_target_valid(&party[i],range))continue;
        if(!party[i].hp&&effect!=0x15u)continue;
        if(effect==1u){unsigned actor_power=battle_enemy_skill_stat(&enemies[ei],stat);unsigned target_power=battle_actor_stat(&party[i],resist);unsigned factor=battle_element_factor(&party[i],element);unsigned dmg=battle_calc_damage_ex(actor_power,target_power,factor,power);party[i].hp=(uint16_t)(dmg>=party[i].hp?0u:party[i].hp-dmg);if(!party[i].hp)party[i].status|=4u;continue;}
        if(!battle_effect_hits(&enemies[ei],&party[i],stat,power,resist,element))continue;
        if(effect==2u||effect==0x1Du){party[i].hp=0u;party[i].status|=4u;}
        else if(effect==3u)party[i].atk=(uint16_t)(party[i].atk>power?party[i].atk-power:1u);
        else if(effect==4u)party[i].def=(uint16_t)(party[i].def>power?party[i].def-power:0u);
        else if(effect==6u)party[i].agility=(uint8_t)(party[i].agility>power?party[i].agility-power:0u);
        else if(effect==7u)party[i].status|=8u;else if(effect==8u)party[i].status|=16u;
        else if(effect==0x1Bu)party[i].status|=1u;else if(effect==0x1Cu){party[i].status|=2u;party[i].status&=(uint8_t)~8u;party[i].agility=1u;party[i].dexterity=1u;}
        else if(effect==0x21u)party[i].dexterity=(uint8_t)(party[i].dexterity>power?party[i].dexterity-power:0u);
    }
}
static void battle_enemy_execute_action(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t skill,unsigned victim,unsigned death_focus);
static void battle_enemy_round(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    /* Failed RUN retaliation preserves its own target/skill RNG order but now
     * uses the same source-authored action presentation as ordinary turns. */
    for(unsigned ei=0u;ei<battle_enemy_count;ei++)if(enemies[ei].hp&&!battle_actor_incapacitated(&enemies[ei])){
        int t=battle_select_enemy_target(party,pn);if(t<0)return;uint8_t skill=battle_enemy_pick_skill(ei,enemies,party,pn);
        battle_enemy_execute_action(party,pn,enemies,ei,skill,(unsigned)t,ei);
    }
}

/* -------------------------------------------------------------------------
 * v0.9.6 source-backed UI / inventory / shop / battle command layer.
 *
 * This deliberately reads PSIV's own live $FFFxxx state and ROM tables rather
 * than maintaining a second port-local RPG database.  The Mode-4 drawing is a
 * GBA presentation backend for the source window/menu semantics.
 * ------------------------------------------------------------------------- */
#define PS4_INV_BASE 0xF410u
#define PS4_MONEY    0xF438u
#define PS4_MACROS   0xF444u
#define PS4_CHAR_BASE 0xF500u
#define PS4_ITEM_DATA 0x2A8E28u
#define PS4_ITEM_NAMES 0x2AAFB4u
#define PS4_TECH_DATA 0x2A9BE8u
#define PS4_TECH_NAMES 0x2AB622u
#define PS4_SKILL_DATA 0x2A9D28u
#define PS4_SKILL_NAMES 0x2AB702u
#define PS4_SHOP_INVENTORIES 0x681A4u

#define PS4_WINDOW_PAL_BASE 96u
static void ps4_ui_colors(void){
    /* loc_296300 is the exact source palette used by PSIV's window renderer. */
    for(unsigned i=0;i<16u;i++)field_pause_pal[PS4_WINDOW_PAL_BASE+i]=md2gba(ps4_window_md_palette[i]);
    field_pause_pal[80u]=md2gba(ps4_window_md_palette[15u]); /* source white text */
    field_pause_pal[81u]=md2gba(ps4_window_md_palette[14u]); /* source window interior */
    field_pause_pal[82u]=md2gba(ps4_window_md_palette[3u]);  /* source secondary edge */
    field_pause_pal[83u]=md2gba(0x000Eu);                  /* source-style red cursor/highlight */
    field_pause_pal[84u]=md2gba(0x000Eu);                  /* warning/accent */
}
static unsigned ps4_window_px(unsigned tile,unsigned x,unsigned y,unsigned hf,unsigned vf){
    if(hf)x=7u-x;if(vf)y=7u-y;uint8_t q=ps4_window_tiles_4bpp[tile*32u+y*4u+(x>>1)];return (x&1u)?(q&15u):(q>>4);
}
static void ps4_window_stamp(unsigned tile,int sx,int sy,unsigned hf,unsigned vf){
    for(unsigned y=0;y<8u;y++){int py=sy+(int)y;if(py<0||py>=160)continue;for(unsigned x=0;x<8u;x++){int px=sx+(int)x;if(px<0||px>=240)continue;unsigned ci=ps4_window_px(tile,x,y,hf,vf);field_pause_fb[(unsigned)py*240u+(unsigned)px]=(uint8_t)(PS4_WINDOW_PAL_BASE+ci);}}
}
static void ps4_ui_box(unsigned x0,unsigned y0,unsigned x1,unsigned y1){
    /* Source Window_Draw uses $6E9 corners, $6EA horizontal edges, $6F3 vertical
     * edges and $680 interior, with tile flips for the opposite sides. */
    if(x1<=x0+15u||y1<=y0+15u)return;if(x1>240u)x1=240u;if(y1>160u)y1=160u;
    pause_fill_rect(x0+8u,y0+8u,x1-8u,y1-8u,(uint8_t)(PS4_WINDOW_PAL_BASE+14u));
    for(unsigned x=x0+8u;x+8u<=x1-8u;x+=8u){ps4_window_stamp(2,(int)x,(int)y0,0,0);ps4_window_stamp(2,(int)x,(int)y1-8,0,1);}
    for(unsigned y=y0+8u;y+8u<=y1-8u;y+=8u){ps4_window_stamp(3,(int)x0,(int)y,0,0);ps4_window_stamp(3,(int)x1-8,(int)y,1,0);}
    ps4_window_stamp(1,(int)x0,(int)y0,0,0);ps4_window_stamp(1,(int)x1-8,(int)y0,1,0);
    ps4_window_stamp(1,(int)x0,(int)y1-8,0,1);ps4_window_stamp(1,(int)x1-8,(int)y1-8,1,1);
}
static unsigned battle_src_y(unsigned row){return (row*40u)/7u;}
static unsigned battle_scale6_index(unsigned pos,unsigned span){unsigned n=pos*6u,q=0u;if(!span)return 0u;while(n>=span){n-=span;++q;}return q<6u?q:5u;}
static void battle_window_stamp_scaled(unsigned tile,unsigned dx0,unsigned dy0,unsigned dx1,unsigned dy1,unsigned hf,unsigned vf){
    /* Border tiles are preprojected to 6x6 in the immutable asset bank.
     * Window construction only clips/flips/copies target pixels.  The bounded
     * subtractive index helper avoids pulling an ARM EABI divider for the few
     * 5-vs-6 pixel source-row projection cases. */
    if(tile>=PS4_PRECOMP_BATTLE_WINDOW_TILE6_COUNT)return;if(dx1>240u)dx1=240u;if(dy1>160u)dy1=160u;if(dx1<=dx0||dy1<=dy0)return;
    const uint8_t* a=ps4_precomp_assets+PS4_PRECOMP_BATTLE_WINDOW_TILE6_PIXELS_OFF+tile*36u;unsigned dw=dx1-dx0,dh=dy1-dy0;
    for(unsigned dy=0u;dy<dh;dy++){unsigned sy=(dh==6u)?dy:battle_scale6_index(dy,dh);if(vf)sy=5u-sy;for(unsigned dx=0u;dx<dw;dx++){unsigned sx=(dw==6u)?dx:battle_scale6_index(dx,dw);if(hf)sx=5u-sx;unsigned ci=a[sy*6u+sx];field_pause_fb[(dy0+dy)*240u+dx0+dx]=(uint8_t)(PS4_WINDOW_PAL_BASE+ci);}}
}

static void battle_ui_box_source(unsigned wx,unsigned wy,unsigned ww,unsigned wh,unsigned* ox0,unsigned* oy0,unsigned* ox1,unsigned* oy1){
    /* Project Window_Draw itself, not just its anchor. Source width/height are
     * interior dimensions; Window_Draw adds the outer border cell. */
    unsigned x0=wx*6u,x1=(wx+ww+1u)*6u,y0=battle_src_y(wy),y1=battle_src_y(wy+wh+1u);
    if(x1>240u)x1=240u;if(y1>160u)y1=160u;if(ox0)*ox0=x0;if(oy0)*oy0=y0;if(ox1)*ox1=x1;if(oy1)*oy1=y1;
    if(x1<=x0+11u||y1<=y0+9u)return;
    unsigned il=x0+6u,ir=x1-6u,it=battle_src_y(wy+1u),ib=battle_src_y(wy+wh);
    pause_fill_rect(il,it,ir,ib,(uint8_t)(PS4_WINDOW_PAL_BASE+14u));
    for(unsigned c=1u;c<ww;c++){unsigned dx0=(wx+c)*6u,dx1=dx0+6u;battle_window_stamp_scaled(2u,dx0,y0,dx1,battle_src_y(wy+1u),0u,0u);battle_window_stamp_scaled(2u,dx0,battle_src_y(wy+wh),dx1,y1,0u,1u);}
    for(unsigned r=1u;r<wh;r++){unsigned dy0=battle_src_y(wy+r),dy1=battle_src_y(wy+r+1u);battle_window_stamp_scaled(3u,x0,dy0,x0+6u,dy1,0u,0u);battle_window_stamp_scaled(3u,x1-6u,dy0,x1,dy1,1u,0u);}
    battle_window_stamp_scaled(1u,x0,y0,x0+6u,battle_src_y(wy+1u),0u,0u);battle_window_stamp_scaled(1u,x1-6u,y0,x1,battle_src_y(wy+1u),1u,0u);
    battle_window_stamp_scaled(1u,x0,battle_src_y(wy+wh),x0+6u,y1,0u,1u);battle_window_stamp_scaled(1u,x1-6u,battle_src_y(wy+wh),x1,y1,1u,1u);
}
static void battle_draw_window_glyph6(unsigned x,unsigned y,uint8_t code,uint8_t fg){
    if(code==0u)return;
    if((code>=1u&&code<=36u)||(code>=0x31u&&code<=0x52u)){
        const uint8_t* a=r28_font5x7[code];
        for(unsigned dx=0u;dx<5u&&x+dx<240u;dx++)for(unsigned dy=0u;dy<7u&&y+dy<160u;dy++)if(a[dx]&(1u<<dy))field_pause_fb[(y+dy)*240u+x+dx]=fg;
        return;
    }
    int gi=ps4_window_font_index(code);if(gi<0||(unsigned)gi>=PS4_PRECOMP_BATTLE_FONT6X6_COUNT)return;const uint8_t* a=ps4_precomp_assets+PS4_PRECOMP_BATTLE_FONT6X6_MASK_OFF+(unsigned)gi*36u;
    for(unsigned dy=0u;dy<6u&&y+dy<160u;dy++)for(unsigned dx=0u;dx<6u&&x+dx<240u;dx++)if(a[dy*6u+dx])field_pause_fb[(y+dy)*240u+x+dx]=fg;
}
static void battle_draw_text6(unsigned x,unsigned y,const char* q,uint8_t c){for(;*q&&x<235u;q++,x+=6u)battle_draw_window_glyph6(x,y,ps4_window_ascii_code((uint8_t)*q),c);}
static void battle_draw_rom_name6(unsigned x,unsigned y,uint32_t base,unsigned index,uint8_t delim,uint8_t c){uint32_t p=base;while(index--){while(p<0x300000u&&rom8(p)!=delim)++p;if(p<0x300000u)++p;}while(p<0x300000u&&rom8(p)!=delim&&x<236u){uint8_t q=rom8(p++);if(q<87u){battle_draw_window_glyph6(x,y,q,c);x+=6u;}}}
static void battle_draw_enemy_name6(unsigned x,unsigned y,unsigned id,uint8_t c){
    uint32_t p=battle_enemy_name_off(id);while(p<0x300000u&&rom8(p)!=0xFFu&&x<236u){uint8_t q=rom8(p++);if(q<87u){battle_draw_window_glyph6(x,y,q,c);x+=6u;}}
}
static void battle_draw_enemy_skill_name6(unsigned x,unsigned y,unsigned id,uint8_t c){
    uint32_t p=battle_enemy_skill_name_off(id);while(p<0x300000u&&rom8(p)!=0xFFu&&x<236u){uint8_t q=rom8(p++);if(q<87u){battle_draw_window_glyph6(x,y,q,c);x+=6u;}}
}
static void battle_draw_ram_name6(unsigned x,unsigned y,unsigned char_id,uint8_t c){if(char_id>=11u&&char_id<14u){battle_draw_text6(x,y,r28_vehicle_names[char_id-11u],c);return;}uint16_t p=battle_char_record(char_id);for(unsigned guard=0u;guard<8u&&x<236u;guard++){uint8_t q=ram8r(p++);if(q==0xFEu||q==0xFFu)break;battle_draw_window_glyph6(x,y,q,c);x+=6u;}}
static void battle_draw_uint6(unsigned x,unsigned y,unsigned v,uint8_t c){unsigned h=(v/100u)%10u,t=(v/10u)%10u,o=v%10u;int started=0;if(h){battle_draw_window_glyph6(x,y,(uint8_t)(27u+h),c);x+=6u;started=1;}if(started||t){battle_draw_window_glyph6(x,y,(uint8_t)(27u+t),c);x+=6u;}battle_draw_window_glyph6(x,y,(uint8_t)(27u+o),c);}
static void battle_damage_overlay_clear(void){if(battle_damage_enemy_mask||battle_damage_party_mask)battle_damage_pixels_stale=1u;battle_damage_enemy_mask=0u;battle_damage_party_mask=0u;for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++)battle_damage_enemy[i]=0u;for(unsigned i=0u;i<5u;i++)battle_damage_party[i]=0u;}
#ifndef PS4_BATTLE_SOURCE_DAMAGE_WINDOW
#define PS4_BATTLE_SOURCE_DAMAGE_WINDOW 1
#endif
/* R16c: heal amounts use the source's alternate digit set ($FFFF4183 set). */
static uint8_t battle_damage_heal_party_mask,battle_damage_heal_enemy_mask;
/* R17: source 1/3/5-column window phases; no scaling or new art. */
static uint8_t battle_damage_columns=5u;
static uint8_t battle_defer_death_audio;
static void battle_damage_overlay_capture(const uint16_t party_before[5],unsigned pn,const uint16_t enemy_before[BATTLE_MAX_ENEMIES],const struct BattleLiteActor party[5],const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    battle_damage_overlay_clear();
    for(unsigned i=0u;i<pn&&i<5u;i++)if(party_before[i]>party[i].hp){battle_damage_party[i]=(uint16_t)(party_before[i]-party[i].hp);battle_damage_party_mask|=(uint8_t)(1u<<i);}
    for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++)if(enemy_before[i]>enemies[i].hp){battle_damage_enemy[i]=(uint16_t)(enemy_before[i]-enemies[i].hp);battle_damage_enemy_mask|=(uint8_t)(1u<<i);}
    battle_damage_heal_party_mask=0u;battle_damage_heal_enemy_mask=0u;
#if PS4_BATTLE_SOURCE_DAMAGE_WINDOW
    for(unsigned i=0u;i<pn&&i<5u;i++)if(party[i].hp>party_before[i]&&party_before[i]){battle_damage_party[i]=(uint16_t)(party[i].hp-party_before[i]);battle_damage_party_mask|=(uint8_t)(1u<<i);battle_damage_heal_party_mask|=(uint8_t)(1u<<i);}
#endif
}
#if PS4_BATTLE_SOURCE_DAMAGE_WINDOW
#include "battle_damage_window_data.h"
/* R16c: source damage window (Fighter_OpenDamageWindow/loc_255C, ps4.asm ~3586/3700).
 * Art: Nemesis loc_27D778 -> VRAM $5E4 (tools/gen_battle_damage_window.py), palette
 * line 3 (= the battle window palette, PS4_WINDOW_PAL_BASE), Genesis H/V flips, ci 0
 * transparent.  5x2 tiles: corners, hundreds/tens (blank pair when leading), ones.
 * Position (loc_29B4): plane row 5 (enemies) / row 18 (party), column
 * (x_pos-$98)>>3 with x_pos the fighter sprite X (= screen X + $80). */
static void battle_draw_damage_glyph(unsigned x,unsigned y,unsigned g){
    const uint8_t* a=battle_damage_glyphs+g*72u;
    for(unsigned dy=0u;dy<BATTLE_DAMAGE_GLYPH_H;dy++){unsigned py=y+dy;if(py>=160u)break;uint8_t* row=field_pause_fb+py*240u;
        for(unsigned dx=0u;dx<BATTLE_DAMAGE_GLYPH_W;dx++){unsigned px=x+dx;if(px>=240u)break;unsigned ci=a[dy*6u+dx];if(ci)row[px]=(uint8_t)(PS4_WINDOW_PAL_BASE+ci);}}
}
static void battle_draw_damage_window(int gba_center_x,unsigned row,unsigned value,int heal){
    if(value>999u)value=999u;
    /* screen X (320) of the fighter, then the source window's tile-aligned left edge. */
    int src_x=(gba_center_x*4)/3;int left=(src_x-0x18)&~7;if(left<0)left=0;if(left>320-40)left=320-40;
    unsigned x0=(unsigned)((left*3)/4),y0=battle_src_y(row);
    const unsigned digits=heal?BATTLE_DAMAGE_GLYPH_HEAL:BATTLE_DAMAGE_GLYPH_DIGIT;
    unsigned h=value/100u,t=(value/10u)%10u,o=value%10u;
    unsigned glyph[5]={BATTLE_DAMAGE_GLYPH_LEFT,
        value<=99u?BATTLE_DAMAGE_GLYPH_BLANK:digits+h,
        value<=9u?BATTLE_DAMAGE_GLYPH_BLANK:digits+t, digits+o,BATTLE_DAMAGE_GLYPH_RIGHT};
    unsigned begin=(5u-battle_damage_columns)/2u,end=5u-begin;
    if(!battle_damage_columns)return;
    for(unsigned col=begin;col<end;col++)battle_draw_damage_glyph(x0+col*6u,y0,glyph[col]);
}
/* Source hold: Fighter_ShowDamage counts $FFFFEE66 = (Battle_Speed+1)*12 frames. */
static unsigned battle_damage_hold_frames(void){unsigned sp=ram16r(0xF442u);if(sp>7u)sp=7u;return (sp+1u)*12u;}
static void battle_draw_damage_overlays(void){
    for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++)if(battle_damage_enemy_mask&(1u<<i))battle_draw_damage_window(battle_enemy_anchor_x(i),5u,battle_damage_enemy[i],(battle_damage_heal_enemy_mask>>i)&1u);
    for(unsigned i=0u;i<5u;i++)if(battle_damage_party_mask&(1u<<i))battle_draw_damage_window(battle_party_center_x(i),18u,battle_damage_party[i],(battle_damage_heal_party_mask>>i)&1u);
}
#else
static void battle_draw_damage_overlays(void){
    for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++)if(battle_damage_enemy_mask&(1u<<i)){int cx=battle_enemy_anchor_x(i),x=cx-9;if(x<2)x=2;if(x>220)x=220;battle_draw_uint6((unsigned)x,72u,battle_damage_enemy[i],83u);}
    for(unsigned i=0u;i<5u;i++)if(battle_damage_party_mask&(1u<<i)){int cx=battle_party_center_x(i),x=cx-9;if(x<2)x=2;if(x>220)x=220;battle_draw_uint6((unsigned)x,101u,battle_damage_party[i],83u);}
}
static unsigned battle_damage_hold_frames(void){return 10u;}
#endif
static void battle_snapshot_hp(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],uint16_t party_hp[5],uint16_t enemy_hp[BATTLE_MAX_ENEMIES]){for(unsigned i=0u;i<5u;i++)party_hp[i]=(i<pn)?party[i].hp:0u;for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++)enemy_hp[i]=(i<battle_enemy_count)?enemies[i].hp:0u;}
static void battle_draw_cursor6(unsigned x,unsigned y,uint8_t fg){unsigned tile=(fg>=83u)?1u:0u;const uint8_t* a=ps4_precomp_assets+PS4_PRECOMP_BATTLE_CURSOR6_PIXELS_OFF+tile*36u;for(unsigned dy=0u;dy<6u&&y+dy<160u;dy++)for(unsigned dx=0u;dx<6u&&x+dx<240u;dx++){uint8_t ci=a[dy*6u+dx];if(ci){uint8_t c=(ci==2u)?81u:(ci==14u?82u:fg);field_pause_fb[(y+dy)*240u+x+dx]=c;}}}
static unsigned ps4_menu_camx,ps4_menu_camy;
static uint8_t ps4_menu_overlay_active;
static uint8_t ps4_menu_base_cached;
/* CP22: menu TALK.  Source Win_TalkDialogue closes the command window and runs
 * the talk dialogue on the live field.  The port used to call ps4_field_talk()
 * while the menu still owned the Mode-4 display (field dialogue OBJs use tiles
 * below 512, which bitmap modes do not display, and player/NPC OAM was hidden),
 * so TALK from the menu showed a missing/garbled dialogue over the menu.  The
 * menu now only latches the request; the field loop restores the field
 * (hw_field_init: tiles, palettes, sprite art, NPC/player OAM, Mode 0) and then
 * runs the same field_dialogue_show path the A-button interaction uses. */
static uint8_t ps4_menu_talk_pending;
static void ps4_ui_clear_field(unsigned camx,unsigned camy);
static void ps4_ui_clear(void){if(ps4_menu_overlay_active){ps4_ui_clear_field(ps4_menu_camx,ps4_menu_camy);return;}dma_fill32(field_pause_pal,512u,0u);ps4_ui_colors();dma_fill32(field_pause_fb,240u*160u,0x6E6E6E6Eu);}
static void ps4_ui_present(void);
static void ps4_ui_clear_field(unsigned camx,unsigned camy){
    /* Mix the next audio block before any menu work.  Once the field backdrop
     * has been captured, every later selector/submenu redraw restores it with
     * one VRAM->EWRAM DMA instead of re-walking both map planes.  This is a
     * native-runtime form of Phase 85's unchanged-epoch/structural cache idea. */
    ps4_audio_prepare_frame();
    dma_fill32(field_pause_pal,512u,0u);
    for(unsigned i=0;i<64u;i++)field_pause_pal[i]=md2gba(cram[i]);
    if(ps4_menu_base_cached){
        dma_copy32((const void*)MODE4_PAGE1,field_pause_fb,240u*160u);ps4_ui_colors();return;
    }
    /* The Genesis menu is an overlay on the live field, not a replacement
     * full-screen scene. Compose by source 8x8 tile spans so chunk/layout
     * lookup happens once per plane per tile rather than twice per pixel. */
    int first_x=-(int)(camx&7u),first_y=-(int)(camy&7u);
    unsigned world_x0=camx&~7u,world_y0=camy&~7u;
    for(int sy=first_y,ty=0;sy<160;sy+=8,ty++){
        unsigned wy=world_y0+(unsigned)ty*8u;
        for(int sx=first_x,tx=0;sx<240;sx+=8,tx++){
            unsigned wx=world_x0+(unsigned)tx*8u;
            uint16_t a=field_entry(0xA000u,wx,wy),b=field_entry(0xB000u,wx,wy);
            unsigned pa=(a>>13)&3u,pb=(b>>13)&3u,pra=a>>15,prb=b>>15;
            unsigned y0=sy<0?(unsigned)(-sy):0u,y1=(sy+8>160)?(unsigned)(160-sy):8u;
            unsigned x0=sx<0?(unsigned)(-sx):0u,x1=(sx+8>240)?(unsigned)(240-sx):8u;
            for(unsigned yy=y0;yy<y1;yy++){
                unsigned di=(unsigned)(sy+(int)yy)*240u+(unsigned)(sx+(int)x0);
                for(unsigned xx=x0;xx<x1;xx++,di++){
                    unsigned ia=tile_px(a,xx,yy),ib=tile_px(b,xx,yy);uint8_t out=0u;
                    if(ia&&(pra||!ib||!prb))out=(uint8_t)(pa*16u+ia);
                    else if(ib)out=(uint8_t)(pb*16u+ib);
                    else if(ia)out=(uint8_t)(pa*16u+ia);
                    field_pause_fb[di]=out;
                }
            }
        }
        /* Keep Direct Sound serviced even on a slow first snapshot.  The
         * cached redraw path above does not need this cooperative yield. */
        if(ty==9){waitvb();ps4_audio_prepare_frame();}
    }
    /* Menu mode no longer needs the field OBJ tiles until exit; hw_field_init()
     * recreates them.  Cache the clean backdrop in the otherwise-unused Mode4
     * page 1 so every menu page starts from identical pixels. */
    for(unsigned i=0u;i<128u;i++)hw_hide_obj(i);
    /* CP22: publish the clean backdrop as Mode-4 page 0 first (VBlank + mode
     * switch in ps4_ui_present), so the page-1 cache write below can never land
     * on live Mode-0 BG map/OBJ tile VRAM (H-scroll maps use SB20-23, which
     * overlap page 1): removes the first-open one-frame flash. */
    ps4_ui_present();dma_copy32(field_pause_fb,(void*)MODE4_PAGE1,240u*160u);ps4_menu_base_cached=1u;ps4_ui_colors();
}
static void ps4_ui_present(void){
    /* CP22 menu-corruption fix (runtime IMG_0856).  Field menus/shops/status
     * are entered from the tiled Mode-0 field, where 0x06000000 is BG
     * character data.  The v0.3 lineage published through
     * battle_present_page0(), which waited for VBlank and selected Mode 4/BG2.
     * A later refactor inlined only the DMA bursts (so palettes are always
     * published) and dropped the VBlank wait and the display-mode switch: the
     * 8bpp menu bitmap was then decoded by the still-active Mode-0 field
     * tilemaps as 4bpp tiles, i.e. full-screen striped corruption.  Publish
     * during VBlank, then own the display as Mode-4 page 0.  Page 1 stays the
     * undisplayed clean-backdrop cache used by ps4_ui_clear_field.  Menu exit
     * is unchanged: the caller runs hw_field_init(), restoring Mode 0. */
    for(unsigned i=0;i<128u;i++)hw_hide_obj(i);
    waitvb();
    dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,240u*160u);
    dma_copy32(field_pause_pal,(void*)BG_PALETTE,512u);
    dma_copy32(field_pause_pal,(void*)OBJ_PALETTE,512u);
    REG_BG2CNT=3u;REG_DISPCNT=(uint16_t)(MODE4|BG2_ENABLE|OBJ_1D|OBJ_ENABLE);
}
static void ps4_ui_present_band(unsigned y0,unsigned y1){
    if(y0>160u)y0=160u;if(y1>160u)y1=160u;if(y1<=y0)return;
    /* Menus already own the displayed Mode-4 page. Cursor/list movement only
     * changes these scanlines, so do not republish an unchanged 38.4 KiB page
     * or unchanged palettes. */
    /* OAM was hidden by the initial full-page presentation and field menus are
     * blocking; do not repeat 128 hide operations on every cursor-only band. */
    waitvb();unsigned off=y0*240u;dma_copy32(field_pause_fb+off,(void*)((uintptr_t)MODE4_VRAM16+off),(y1-y0)*240u);
}
static void ps4_ui_clear_row(unsigned x0,unsigned y,unsigned x1,unsigned h){pause_fill_rect(x0,y,x1,y+h,(uint8_t)(PS4_WINDOW_PAL_BASE+14u));}
static void ps4_ui_draw_rom_name(unsigned x,unsigned y,uint32_t base,unsigned index,uint8_t delim,uint8_t c){
    uint32_t p=base;while(index--){while(p<0x300000u&&rom8(p)!=delim)++p;if(p<0x300000u)++p;}
    while(p<0x300000u&&rom8(p)!=delim&&x<232u){uint8_t q=rom8(p++);if(q<87u){pause_draw_window_glyph(x,y,q,c);x+=8u;}}
}
static void ps4_ui_draw_ram_name(unsigned x,unsigned y,unsigned char_id,uint8_t c){
    uint16_t p=battle_char_record(char_id);for(unsigned guard=0;guard<8u&&x<232u;guard++){uint8_t q=ram8r(p++);if(q==0xFEu||q==0xFFu)break;pause_draw_window_glyph(x,y,q,c);x+=8u;}
}
static void ps4_ui_draw_uint7(unsigned x,unsigned y,uint32_t v,uint8_t c){
    static const uint32_t place[7]={1000000u,100000u,10000u,1000u,100u,10u,1u};int started=0;
    if(v>9999999u)v=9999999u;for(unsigned i=0;i<7u;i++){unsigned d=0;while(v>=place[i]){v-=place[i];++d;}if(d||started||i==6u){pause_draw_window_glyph(x,y,(uint8_t)(27u+d),c);x+=8u;started=1;}}
}
static uint8_t ps4_ui_wait_edge(uint8_t* prev){for(;;){waitvb();uint8_t n=md_io_read_psiv_pad(),p=(uint8_t)(n&(uint8_t)~*prev);*prev=n;if(p){if(p&0x0Fu)ram8w(0x500Au,0xF2u);else if(p&0x60u)ram8w(0x500Au,0xF3u);return p;}}}
static void ps4_ui_wait_buttons_up(void){for(;;){waitvb();if(!(md_io_read_psiv_pad()&0xF0u))return;}}

static unsigned ps4_party_ids(uint8_t out[5]){unsigned n=0u;for(unsigned i=0;i<5u;i++){uint8_t id=ram8r((uint16_t)(0xF40Au+i));if(id==0xFFu)break;out[n++]=id;}return n;}
static int ps4_inventory_add(uint8_t item){if(!item)return -1;for(unsigned i=0;i<40u;i++)if(!ram8r((uint16_t)(PS4_INV_BASE+i))){ram8w((uint16_t)(PS4_INV_BASE+i),item);return (int)i;}return -1;}
static void ps4_inventory_remove(unsigned slot){if(slot>=40u)return;for(unsigned i=slot;i<39u;i++)ram8w((uint16_t)(PS4_INV_BASE+i),ram8r((uint16_t)(PS4_INV_BASE+i+1u)));ram8w((uint16_t)(PS4_INV_BASE+39u),0u);}
/* Source helpers $5B100/$5CB72: find one ItemID in the packed 40-byte
 * inventory, clear that byte, then compact the tail.  Keep this primitive
 * shared so scripted item consumption and menus mutate the same inventory. */
static int ps4_inventory_remove_first(uint8_t item){if(!item)return 0;for(unsigned i=0u;i<40u;i++){uint8_t q=ram8r((uint16_t)(PS4_INV_BASE+i));if(!q)return 0;if(q==item){ps4_inventory_remove(i);return 1;}}return 0;}
static void ps4_inventory_remove_all(uint8_t item){while(ps4_inventory_remove_first(item)){} }
static unsigned ps4_inventory_count(void){unsigned n=0;for(unsigned i=0;i<40u;i++)if(ram8r((uint16_t)(PS4_INV_BASE+i)))++n;return n;}
static uint32_t ps4_item_ptr(uint8_t item){return item?PS4_ITEM_DATA+(uint32_t)(item-1u)*22u:0u;}
static unsigned ps4_item_price(uint8_t item){uint32_t p=ps4_item_ptr(item);return p?rom16(p+20u):0u;}
static unsigned ps4_item_type(uint8_t item){uint32_t p=ps4_item_ptr(item);return p?rom8(p+10u):0u;}
static unsigned ps4_item_effect(uint8_t item){uint32_t p=ps4_item_ptr(item);return p?rom8(p):0u;}
static unsigned ps4_item_power(uint8_t item){uint32_t p=ps4_item_ptr(item);return p?rom8(p+1u):0u;}
static int ps4_item_char_allowed(uint8_t item,unsigned cid){uint32_t p=ps4_item_ptr(item);if(!p||cid>=11u)return 0;uint16_t mask=rom16(p+8u);return (mask&(uint16_t)(0x8000u>>cid))!=0u;}
static int ps4_item_equip_slot(uint8_t item){unsigned t=ps4_item_type(item);if(t>=1u&&t<=4u)return 0;if(t==5u)return 1;if(t==6u)return 2;if(t==7u)return 3;return -1;}
static uint32_t ps4_delim_ptr(uint32_t base,unsigned index,uint8_t delim){uint32_t p=base;while(index--){while(p<0x300000u&&rom8(p)!=delim)++p;if(p<0x300000u)++p;}return p;}

static void ps4_ui_message(const char* a,const char* b){
    ps4_ui_clear();ps4_ui_box(16u,52u,224u,108u);battle_draw_text(28u,64u,a,80u);if(b)battle_draw_text(28u,82u,b,80u);ps4_ui_present();ps4_ui_wait_buttons_up();uint8_t p=0u;for(;;){uint8_t e=ps4_ui_wait_edge(&p);if(e&0xF0u)break;}ps4_ui_wait_buttons_up();
}
static int ps4_ui_select_strings(const char* title,const char* const* options,unsigned count,unsigned initial){
    if(!count)return -1;unsigned sel=initial<count?initial:0u;uint8_t prev=0u;ps4_ui_wait_buttons_up();
    unsigned visible=count<6u?count:6u,first=0u;if(sel>=visible)first=sel-visible+1u;if(first+visible>count)first=count-visible;unsigned y1=48u+visible*18u;if(y1>156u)y1=156u;
    ps4_ui_clear();ps4_ui_box(32u,12u,208u,y1);battle_draw_text(44u,20u,title,80u);
    for(unsigned row=0u;row<visible&&first+row<count;row++){unsigned idx=first+row,y=40u+row*18u;if(idx==sel)pause_draw_cursor(42u,y,83u);battle_draw_text(56u,y,options[idx],80u);}ps4_ui_present();
    for(;;){unsigned old=sel,oldfirst=first;uint8_t e=ps4_ui_wait_edge(&prev);if(e&0x01u)sel=sel?sel-1u:count-1u;else if(e&0x02u)sel=(sel+1u==count)?0u:sel+1u;else if(e&0x60u){ps4_ui_wait_buttons_up();return (int)sel;}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}else continue;
        first=0u;if(sel>=visible)first=sel-visible+1u;if(first+visible>count)first=count-visible;
        if(first!=oldfirst){ps4_ui_clear_row(40u,38u,202u,y1>46u?y1-46u:0u);for(unsigned row=0u;row<visible&&first+row<count;row++){unsigned idx=first+row,y=40u+row*18u;if(idx==sel)pause_draw_cursor(42u,y,83u);battle_draw_text(56u,y,options[idx],80u);}ps4_ui_present_band(38u,y1-6u);}
        else{unsigned orow=old-oldfirst,nrow=sel-first,oy=40u+orow*18u,ny=40u+nrow*18u;ps4_ui_clear_row(40u,oy,202u,14u);battle_draw_text(56u,oy,options[old],80u);if(ny!=oy)ps4_ui_clear_row(40u,ny,202u,14u);pause_draw_cursor(42u,ny,83u);battle_draw_text(56u,ny,options[sel],80u);unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);}
    }
}
static int ps4_ui_select_party(const char* title){
    uint8_t ids[5];unsigned n=ps4_party_ids(ids);if(!n)return -1;unsigned sel=0u;uint8_t prev=0u;ps4_ui_wait_buttons_up();
    ps4_ui_clear();ps4_ui_box(28u,18u,212u,142u);battle_draw_text(42u,28u,title,80u);for(unsigned i=0;i<n;i++){unsigned y=48u+i*18u;if(i==sel)pause_draw_cursor(40u,y,83u);ps4_ui_draw_ram_name(54u,y,ids[i],80u);}ps4_ui_present();
    for(;;){unsigned old=sel;uint8_t e=ps4_ui_wait_edge(&prev);if(e&0x01u)sel=sel?sel-1u:n-1u;else if(e&0x02u)sel=(sel+1u==n)?0u:sel+1u;else if(e&0x60u){ps4_ui_wait_buttons_up();return ids[sel];}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}else continue;unsigned oy=48u+old*18u,ny=48u+sel*18u;ps4_ui_clear_row(38u,oy,204u,14u);ps4_ui_draw_ram_name(54u,oy,ids[old],80u);if(ny!=oy)ps4_ui_clear_row(38u,ny,204u,14u);pause_draw_cursor(40u,ny,83u);ps4_ui_draw_ram_name(54u,ny,ids[sel],83u);unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);}
}
static int ps4_ui_select_inventory(const char* title,int equipment_only,unsigned cid){
    uint8_t slots[40];unsigned n=0;for(unsigned i=0;i<40u;i++){uint8_t item=ram8r((uint16_t)(PS4_INV_BASE+i));if(!item)continue;if(equipment_only&&(ps4_item_equip_slot(item)<0||!ps4_item_char_allowed(item,cid)))continue;slots[n++]=(uint8_t)i;}if(!n){ps4_ui_message("NO ITEMS",0);return -1;}
    unsigned sel=0,visible=n<6u?n:6u,first=0u;uint8_t prev=0;ps4_ui_wait_buttons_up();ps4_ui_clear();ps4_ui_box(8u,6u,232u,154u);battle_draw_text(18u,14u,title,80u);
    for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;uint8_t item=ram8r((uint16_t)(PS4_INV_BASE+slots[k]));if(k==sel)pause_draw_cursor(18u,y,83u);ps4_ui_draw_rom_name(30u,y,PS4_ITEM_NAMES,item-1u,0xFEu,k==sel?83u:80u);}ps4_ui_present();
    for(;;){unsigned old=sel,oldfirst=first;uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:n-1u;else if(e&2u)sel=(sel+1u==n)?0u:sel+1u;else if(e&0x60u){ps4_ui_wait_buttons_up();return slots[sel];}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}else continue;first=0u;if(sel>=visible)first=sel-visible+1u;if(first+visible>n)first=n-visible;
        if(first!=oldfirst){ps4_ui_clear_row(16u,36u,224u,visible*18u);for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;uint8_t item=ram8r((uint16_t)(PS4_INV_BASE+slots[k]));if(k==sel)pause_draw_cursor(18u,y,83u);ps4_ui_draw_rom_name(30u,y,PS4_ITEM_NAMES,item-1u,0xFEu,k==sel?83u:80u);}ps4_ui_present_band(36u,36u+visible*18u);}
        else{unsigned oy=38u+(old-oldfirst)*18u,ny=38u+(sel-first)*18u;uint8_t oi=ram8r((uint16_t)(PS4_INV_BASE+slots[old])),ni=ram8r((uint16_t)(PS4_INV_BASE+slots[sel]));ps4_ui_clear_row(16u,oy,224u,14u);ps4_ui_draw_rom_name(30u,oy,PS4_ITEM_NAMES,oi-1u,0xFEu,80u);if(ny!=oy)ps4_ui_clear_row(16u,ny,224u,14u);pause_draw_cursor(18u,ny,83u);ps4_ui_draw_rom_name(30u,ny,PS4_ITEM_NAMES,ni-1u,0xFEu,83u);unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);}
    }
}
static void ps4_ui_draw_status_portrait(unsigned cid,unsigned dx,unsigned dy){
    if(cid>=PS4_PRECOMP_STATUS_PORTRAIT_FRAMES)return;
    const uint8_t* src=ps4_precomp_assets+PS4_PRECOMP_STATUS_PORTRAIT_PIXELS_OFF+cid*(PS4_PRECOMP_STATUS_PORTRAIT_W*PS4_PRECOMP_STATUS_PORTRAIT_H);
    for(unsigned y=0u;y<PS4_PRECOMP_STATUS_PORTRAIT_H;y++){unsigned py=dy+y;if(py>=160u)break;for(unsigned x=0u;x<PS4_PRECOMP_STATUS_PORTRAIT_W;x++){unsigned px=dx+x;if(px>=240u)break;field_pause_fb[py*240u+px]=(uint8_t)(32u+src[y*PS4_PRECOMP_STATUS_PORTRAIT_W+x]);}}
}
static void ps4_field_status(void){
    int cid=ps4_ui_select_party("STATUS");if(cid<0)return;uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)cid*0x80u);
    ps4_ui_clear();
    /* Win_Status portrait is the source 10x10 mapping at (3,2); the adjacent
     * WinGroup_Menu $CA stat window begins at (13,2).  Both fit natively in
     * the GBA crop, so preserve that left-portrait/right-stat composition. */
    ps4_ui_draw_status_portrait((unsigned)cid,24u,16u);
    ps4_ui_box(104u,16u,208u,112u);
    ps4_ui_draw_ram_name(114u,24u,(unsigned)cid,83u);
    battle_draw_text(114u,42u,"LV",80u);battle_draw_uint(170u,42u,ram16r((uint16_t)(d+8u)),80u);
    battle_draw_text(114u,58u,"HP",80u);battle_draw_uint(146u,58u,ram16r((uint16_t)(d+0x0Eu)),80u);pause_draw_slash(169u,58u,80u);battle_draw_uint(178u,58u,ram16r((uint16_t)(d+0x10u)),80u);
    battle_draw_text(114u,74u,"TP",80u);battle_draw_uint(146u,74u,ram16r((uint16_t)(d+0x12u)),80u);pause_draw_slash(169u,74u,80u);battle_draw_uint(178u,74u,ram16r((uint16_t)(d+0x14u)),80u);
    battle_draw_text(114u,90u,"ATK",80u);battle_draw_uint(170u,90u,ram16r((uint16_t)(d+0x24u)),80u);
    ps4_ui_box(24u,112u,208u,156u);battle_draw_text(34u,120u,"DEF",80u);battle_draw_uint(82u,120u,ram16r((uint16_t)(d+0x28u)),80u);battle_draw_text(114u,120u,"MDF",80u);battle_draw_uint(162u,120u,ram16r((uint16_t)(d+0x2Cu)),80u);
    battle_draw_text(34u,138u,"STR",80u);battle_draw_uint(82u,138u,ram8r((uint16_t)(d+0x19u)),80u);battle_draw_text(114u,138u,"AGI",80u);battle_draw_uint(162u,138u,ram8r((uint16_t)(d+0x1Fu)),80u);
    ps4_ui_present();uint8_t p=0;ps4_ui_wait_buttons_up();while(!(ps4_ui_wait_edge(&p)&0xF0u)){}ps4_ui_wait_buttons_up();
}
static void ps4_field_equip(void){int cid=ps4_ui_select_party("EQUIP");if(cid<0)return;for(;;){uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)cid*0x80u);ps4_ui_clear();ps4_ui_box(8u,6u,232u,154u);ps4_ui_draw_ram_name(20u,14u,(unsigned)cid,83u);static const char* sn[4]={"WEAPON","SHIELD","HEAD","BODY"};for(unsigned s=0;s<4u;s++){unsigned y=38u+s*24u;battle_draw_text(20u,y,sn[s],80u);uint8_t item=ram8r((uint16_t)(d+0x4Cu+s));if(item)ps4_ui_draw_rom_name(90u,y,PS4_ITEM_NAMES,item-1u,0xFEu,80u);else battle_draw_text(90u,y,"--",80u);}battle_draw_text(20u,138u,"A: CHANGE  B: EXIT",80u);ps4_ui_present();uint8_t p=0;ps4_ui_wait_buttons_up();uint8_t e=ps4_ui_wait_edge(&p);if(e&0x90u){ps4_ui_wait_buttons_up();return;}if(e&0x60u){int slot=ps4_ui_select_inventory("EQUIP ITEM",1,(unsigned)cid);if(slot>=0){uint8_t item=ram8r((uint16_t)(PS4_INV_BASE+(unsigned)slot));int es=ps4_item_equip_slot(item);if(es>=0){uint8_t old=ram8r((uint16_t)(d+0x4Cu+(unsigned)es));ram8w((uint16_t)(d+0x4Cu+(unsigned)es),item);if(old)ram8w((uint16_t)(PS4_INV_BASE+(unsigned)slot),old);else ps4_inventory_remove((unsigned)slot);update_one_char_derived((unsigned)cid);}}}}
}
static void ps4_field_item_menu(void){for(;;){int slot=ps4_ui_select_inventory("ITEM",0,0u);if(slot<0)return;uint8_t item=ram8r((uint16_t)(PS4_INV_BASE+(unsigned)slot));uint32_t ip=ps4_item_ptr(item);unsigned type=ps4_item_type(item),effect=ps4_item_effect(item);if((type!=8u&&type!=10u)||!effect){ps4_ui_message("CAN'T USE HERE",0);continue;}int cid=ps4_ui_select_party("USE ON");if(cid<0)continue;uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)cid*0x80u);unsigned power=ps4_item_power(item);int used=0;if(effect==0x12u){unsigned hp=ram16r((uint16_t)(d+0x0Eu)),mx=ram16r((uint16_t)(d+0x10u));unsigned add=power?power*2u:40u;hp+=add;if(hp>mx)hp=mx;ram16w((uint16_t)(d+0x0Eu),(uint16_t)hp);used=1;}else if(effect==0x15u||effect==0x16u){uint8_t st=ram8r((uint16_t)(d+0x16u));if(st&4u){unsigned mx=ram16r((uint16_t)(d+0x10u));unsigned hp=(effect==0x16u)?mx:(mx>>1);if(!hp)hp=1u;ram16w((uint16_t)(d+0x0Eu),(uint16_t)hp);ram8w((uint16_t)(d+0x16u),(uint8_t)(st&~4u));ram8w((uint16_t)(d+0x20u),ram8r((uint16_t)(d+0x1Fu)));ram8w((uint16_t)(d+0x23u),ram8r((uint16_t)(d+0x22u)));used=1;}}else if(effect==0x13u){ram8w((uint16_t)(d+0x16u),(uint8_t)(ram8r((uint16_t)(d+0x16u))&~1u));used=1;}else if(effect==0x14u){ram8w((uint16_t)(d+0x16u),(uint8_t)(ram8r((uint16_t)(d+0x16u))&~2u));ram8w((uint16_t)(d+0x20u),ram8r((uint16_t)(d+0x1Fu)));ram8w((uint16_t)(d+0x23u),ram8r((uint16_t)(d+0x22u)));used=1;}if(used){ps4_inventory_remove((unsigned)slot);ram8w(0x500Au,0xBDu);ps4_ui_message("ITEM USED",0);}else ps4_ui_message("NO EFFECT",0);(void)ip;}}
static void ps4_field_ability_list(int skill){
    int cid=ps4_ui_select_party(skill?"SKILL":"TECH");if(cid<0)return;uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)cid*0x80u),off=(uint16_t)(d+(skill?0x62u:0x52u));unsigned cap=skill?8u:16u;uint8_t ids[16];unsigned n=0;
    for(unsigned i=0;i<cap;i++){uint8_t id=ram8r((uint16_t)(off+i));if(id)ids[n++]=id;}if(!n){ps4_ui_message(skill?"NO SKILLS":"NO TECHNIQUES",0);return;}
    unsigned sel=0u,visible=n<6u?n:6u,first=0u;uint8_t prev=0u;int rebuild=1;ps4_ui_wait_buttons_up();
    for(;;){
        if(rebuild){first=0u;if(sel>=visible)first=sel-visible+1u;if(first+visible>n)first=n-visible;ps4_ui_clear();ps4_ui_box(8u,6u,232u,154u);ps4_ui_draw_ram_name(20u,14u,(unsigned)cid,83u);for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;if(k==sel)pause_draw_cursor(18u,y,83u);ps4_ui_draw_rom_name(30u,y,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[k]-1u,0xFEu,k==sel?83u:80u);}ps4_ui_present();rebuild=0;}
        unsigned old=sel,oldfirst=first;uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:n-1u;else if(e&2u)sel=(sel+1u==n)?0u:sel+1u;else if(e&0x90u){ps4_ui_wait_buttons_up();return;}else if(e&0x60u){
            if(skill){ps4_ui_message("BATTLE SKILL",0);rebuild=1;prev=0u;continue;}
            uint8_t id=ids[sel];uint32_t tp=PS4_TECH_DATA+(uint32_t)(id-1u)*8u;uint8_t target=rom8(tp+2u),effect=rom8(tp),cost=rom8(tp+1u);
            if((target>>4)==1u){ps4_ui_message("BATTLE ONLY",0);rebuild=1;prev=0u;continue;}if(ram16r((uint16_t)(d+0x12u))<cost){ps4_ui_message("NOT ENOUGH TP",0);rebuild=1;prev=0u;continue;}
            int tc=ps4_ui_select_party("USE ON");if(tc<0){rebuild=1;prev=0u;continue;}uint16_t td=(uint16_t)(PS4_CHAR_BASE+(unsigned)tc*0x80u);
            if(effect==0x12u){unsigned hp=ram16r((uint16_t)(td+0x0Eu)),mx=ram16r((uint16_t)(td+0x10u));unsigned add=(unsigned)rom8(tp+3u)*2u+16u;hp+=add;if(hp>mx)hp=mx;ram16w((uint16_t)(td+0x0Eu),(uint16_t)hp);}else if(effect==0x15u||effect==0x16u){uint8_t st=ram8r((uint16_t)(td+0x16u));if(!(st&4u)){ps4_ui_message("NO EFFECT",0);rebuild=1;prev=0u;continue;}unsigned mx=ram16r((uint16_t)(td+0x10u));unsigned hp=(effect==0x16u)?mx:(mx>>1);if(!hp)hp=1u;ram16w((uint16_t)(td+0x0Eu),(uint16_t)hp);ram8w((uint16_t)(td+0x16u),(uint8_t)(st&~4u));ram8w((uint16_t)(td+0x20u),ram8r((uint16_t)(td+0x1Fu)));ram8w((uint16_t)(td+0x23u),ram8r((uint16_t)(td+0x22u)));}else if(effect==0x13u)ram8w((uint16_t)(td+0x16u),(uint8_t)(ram8r((uint16_t)(td+0x16u))&~1u));else if(effect==0x14u){ram8w((uint16_t)(td+0x16u),(uint8_t)(ram8r((uint16_t)(td+0x16u))&~2u));ram8w((uint16_t)(td+0x20u),ram8r((uint16_t)(td+0x1Fu)));ram8w((uint16_t)(td+0x23u),ram8r((uint16_t)(td+0x22u)));}else{ps4_ui_message("NO FIELD EFFECT",0);rebuild=1;prev=0u;continue;}
            ram16w((uint16_t)(d+0x12u),(uint16_t)(ram16r((uint16_t)(d+0x12u))-cost));ram8w(0x500Au,0xBDu);ps4_ui_message("TECHNIQUE USED",0);rebuild=1;prev=0u;continue;
        }else continue;
        first=0u;if(sel>=visible)first=sel-visible+1u;if(first+visible>n)first=n-visible;
        if(first!=oldfirst){ps4_ui_clear_row(16u,36u,224u,visible*18u);for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;if(k==sel)pause_draw_cursor(18u,y,83u);ps4_ui_draw_rom_name(30u,y,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[k]-1u,0xFEu,k==sel?83u:80u);}ps4_ui_present_band(36u,36u+visible*18u);}
        else{unsigned oy=38u+(old-oldfirst)*18u,ny=38u+(sel-first)*18u;ps4_ui_clear_row(16u,oy,224u,14u);ps4_ui_draw_rom_name(30u,oy,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[old]-1u,0xFEu,80u);if(ny!=oy)ps4_ui_clear_row(16u,ny,224u,14u);pause_draw_cursor(18u,ny,83u);ps4_ui_draw_rom_name(30u,ny,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[sel]-1u,0xFEu,83u);unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);}
    }
}
static int ps4_macro_select_ability(unsigned cid,int skill){
    uint16_t d=(uint16_t)(PS4_CHAR_BASE+cid*0x80u),off=(uint16_t)(d+(skill?0x62u:0x52u));unsigned cap=skill?8u:16u;uint8_t ids[16];unsigned n=0u;
    for(unsigned i=0;i<cap;i++){uint8_t id=ram8r((uint16_t)(off+i));if(id)ids[n++]=id;}if(!n){ps4_ui_message(skill?"NO SKILLS":"NO TECHNIQUES",0);return -1;}
    unsigned sel=0u,visible=n<6u?n:6u,first=0u;uint8_t prev=0u;ps4_ui_wait_buttons_up();ps4_ui_clear();ps4_ui_box(8u,6u,232u,154u);battle_draw_text(20u,14u,skill?"MACRO SKILL":"MACRO TECH",80u);
    for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;if(k==sel)pause_draw_cursor(18u,y,83u);ps4_ui_draw_rom_name(30u,y,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[k]-1u,0xFEu,k==sel?83u:80u);}ps4_ui_present();
    for(;;){unsigned old=sel,oldfirst=first;uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:n-1u;else if(e&2u)sel=(sel+1u==n)?0u:sel+1u;else if(e&0x60u){ps4_ui_wait_buttons_up();return ids[sel];}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}else continue;
        first=0u;if(sel>=visible)first=sel-visible+1u;if(first+visible>n)first=n-visible;
        if(first!=oldfirst){ps4_ui_clear_row(16u,36u,224u,visible*18u);for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;if(k==sel)pause_draw_cursor(18u,y,83u);ps4_ui_draw_rom_name(30u,y,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[k]-1u,0xFEu,k==sel?83u:80u);}ps4_ui_present_band(36u,36u+visible*18u);}
        else{unsigned oy=38u+(old-oldfirst)*18u,ny=38u+(sel-first)*18u;ps4_ui_clear_row(16u,oy,224u,14u);ps4_ui_draw_rom_name(30u,oy,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[old]-1u,0xFEu,80u);if(ny!=oy)ps4_ui_clear_row(16u,ny,224u,14u);pause_draw_cursor(18u,ny,83u);ps4_ui_draw_rom_name(30u,ny,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[sel]-1u,0xFEu,83u);unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);}
    }
}
static int ps4_macro_select_item(void){int slot=ps4_ui_select_inventory("MACRO ITEM",0,0u);return slot<0?-1:(int)ram8r((uint16_t)(PS4_INV_BASE+(unsigned)slot));}
static int ps4_macro_record_slot(unsigned macro,unsigned cid){uint16_t b=(uint16_t)(PS4_MACROS+macro*20u);int free_slot=-1;for(unsigned s=0;s<5u;s++){uint16_t q=(uint16_t)(b+s*4u);uint8_t cmd=ram8r((uint16_t)(q+1u));if(cmd&&ram8r(q)==cid)return (int)s;if(!cmd&&free_slot<0)free_slot=(int)s;}return free_slot;}
static void ps4_macro_write(unsigned macro,unsigned cid,uint8_t command,uint8_t id){int rs=ps4_macro_record_slot(macro,cid);if(rs<0)return;uint16_t q=(uint16_t)(PS4_MACROS+macro*20u+(unsigned)rs*4u);ram8w(q,(uint8_t)cid);ram8w((uint16_t)(q+1u),command);ram8w((uint16_t)(q+2u),id);ram8w((uint16_t)(q+3u),0u);}
static void ps4_macro_clear_char(unsigned macro,unsigned cid){uint16_t b=(uint16_t)(PS4_MACROS+macro*20u);for(unsigned s=0;s<5u;s++){uint16_t q=(uint16_t)(b+s*4u);if(ram8r((uint16_t)(q+1u))&&ram8r(q)==cid){for(unsigned k=0;k<4u;k++)ram8w((uint16_t)(q+k),0u);return;}}}
static void ps4_field_macro_edit(unsigned macro){
    static const char* const cmds[6]={"ATTACK","TECH","SKILL","ITEM","DEFEND","CLEAR"};
    for(;;){int cid=ps4_ui_select_party("MACRO CHARACTER");if(cid<0)return;int c=ps4_ui_select_strings("MACRO ACTION",cmds,6u,0u);if(c<0)continue;if(c==5){ps4_macro_clear_char(macro,(unsigned)cid);continue;}uint8_t id=0u;
        if(c==1){int q=ps4_macro_select_ability((unsigned)cid,0);if(q<0)continue;id=(uint8_t)q;}else if(c==2){int q=ps4_macro_select_ability((unsigned)cid,1);if(q<0)continue;id=(uint8_t)q;}else if(c==3){int q=ps4_macro_select_item();if(q<0)continue;id=(uint8_t)q;}
        ps4_macro_write(macro,(unsigned)cid,(uint8_t)(c+1),id);ps4_ui_message("MACRO UPDATED",0);
    }
}
static void ps4_field_macro_view(void){
    unsigned sel=0u,first=0u;uint8_t prev=0u;int rebuild=1;ps4_ui_wait_buttons_up();
    for(;;){if(rebuild){first=sel>=7u?sel-6u:0u;ps4_ui_clear();ps4_ui_box(8u,6u,232u,154u);battle_draw_text(20u,14u,"MACRO A-H",80u);for(unsigned r=0;r<7u&&first+r<8u;r++){unsigned m=first+r,y=34u+r*17u;if(m==sel)pause_draw_cursor(18u,y,83u);pause_draw_window_ascii(32u,y,(uint8_t)('A'+m),m==sel?83u:80u);uint16_t q=(uint16_t)(PS4_MACROS+m*20u);unsigned actions=0;for(unsigned a=0;a<5u;a++)if(ram8r((uint16_t)(q+a*4u+1u)))++actions;pause_draw_window_glyph(58u,y,(uint8_t)(27u+actions),80u);battle_draw_text(72u,y,"ACTIONS",80u);}ps4_ui_present();rebuild=0;}
        unsigned old=sel,oldfirst=first;uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:7u;else if(e&2u)sel=sel==7u?0u:sel+1u;else if(e&0x90u){ps4_ui_wait_buttons_up();return;}else if(e&0x60u){ps4_ui_wait_buttons_up();ps4_field_macro_edit(sel);prev=0u;rebuild=1;continue;}else continue;
        first=sel>=7u?sel-6u:0u;if(first!=oldfirst){rebuild=1;continue;}unsigned oy=34u+(old-oldfirst)*17u,ny=34u+(sel-first)*17u;
        for(unsigned pass=0;pass<2u;pass++){unsigned m=pass?sel:old,y=pass?ny:oy;if(pass&&ny==oy)continue;ps4_ui_clear_row(16u,y,224u,14u);if(m==sel)pause_draw_cursor(18u,y,83u);pause_draw_window_ascii(32u,y,(uint8_t)('A'+m),m==sel?83u:80u);uint16_t q=(uint16_t)(PS4_MACROS+m*20u);unsigned actions=0;for(unsigned a=0;a<5u;a++)if(ram8r((uint16_t)(q+a*4u+1u)))++actions;pause_draw_window_glyph(58u,y,(uint8_t)(27u+actions),80u);battle_draw_text(72u,y,"ACTIONS",80u);}unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);
    }
}
static void ps4_field_talk(unsigned camx,unsigned camy){
    /* Win_TalkDialogue/loc_5DF2A chooses a source talk ID from story flags.
     * Decode the matching talk tree, then feed it through the same two-line
     * field dialogue renderer used by NPCs/events instead of maintaining a
     * separate full-screen TALK presentation. */
    uint32_t p=0x5DF52u;unsigned id=0u;
    while(p+1u<0x300000u&&rom16(p)!=0xFFFFu){uint8_t flag=rom8(p),qid=rom8(p+1u);if(ps4_event_flag_test(flag)){id=qid;break;}p+=2u;}
    uint32_t tree=id<0x1Au?0x1F4F70u:0x1F5A70u;unsigned n=kos_decode(tree,field_dialogue,FIELD_DIALOGUE_CAP);
    if(n){field_dialogue_len=n;field_dialogue_show(id,camx,camy);}
    n=kos_decode(field_map.dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;
}
static void ps4_field_order(void){
    uint8_t ids[5];unsigned n=ps4_party_ids(ids);if(n<2u){ps4_ui_message("ONLY ONE MEMBER",0);return;}
    uint8_t ordered[5];unsigned out=0u,remaining=n;
    while(remaining){
        /* Cache the current remaining IDs once. Cursor motion cannot change
         * membership, so P128-style dirty rows only need to redraw old/new
         * selections until A commits one member. */
        uint8_t row_ids[5];unsigned rows=0u;for(unsigned i=0u;i<n;i++){int used=0;for(unsigned k=0u;k<out;k++)if(ordered[k]==ids[i])used=1;if(!used)row_ids[rows++]=ids[i];}
        unsigned sel=0u;uint8_t prev=0u;ps4_ui_wait_buttons_up();ps4_ui_clear();ps4_ui_box(30u,18u,210u,142u);battle_draw_text(42u,28u,"ORDER",80u);
        for(unsigned row=0u;row<rows;row++){unsigned y=48u+row*18u;if(row==sel)pause_draw_cursor(40u,y,83u);ps4_ui_draw_ram_name(54u,y,row_ids[row],row==sel?83u:80u);}ps4_ui_present();
        for(;;){unsigned old=sel;uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:rows-1u;else if(e&2u)sel=(sel+1u==rows)?0u:sel+1u;else if(e&0x90u){ps4_ui_wait_buttons_up();return;}else if(e&0x60u){ordered[out++]=row_ids[sel];remaining--;ps4_ui_wait_buttons_up();break;}else continue;
            unsigned oy=48u+old*18u,ny=48u+sel*18u;ps4_ui_clear_row(38u,oy,164u,14u);ps4_ui_draw_ram_name(54u,oy,row_ids[old],80u);if(ny!=oy)ps4_ui_clear_row(38u,ny,164u,14u);pause_draw_cursor(40u,ny,83u);ps4_ui_draw_ram_name(54u,ny,row_ids[sel],83u);unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);
        }
    }
    for(unsigned i=0u;i<n;i++)ram8w((uint16_t)(0xF40Au+i),ordered[i]);for(unsigned i=n;i<5u;i++)ram8w((uint16_t)(0xF40Au+i),0xFFu);field_followers_sync(1);
}
static void ps4_field_state(void){
    static const char* const opts[2]={"STATUS","ORDER"};for(;;){int q=ps4_ui_select_strings("STATE",opts,2u,0u);if(q<0)return;if(q==0)ps4_field_status();else ps4_field_order();}
}
static void ps4_ui_draw_uint7_compact(unsigned x,unsigned y,uint32_t v,uint8_t c){
    static const uint32_t place[7]={1000000u,100000u,10000u,1000u,100u,10u,1u};int started=0;
    if(v>9999999u)v=9999999u;for(unsigned i=0u;i<7u;i++){unsigned d=0u;while(v>=place[i]){v-=place[i];d++;}if(d||started||i==6u){battle_draw_window_glyph6(x,y,(uint8_t)(27u+d),c);x+=6u;started=1;}}
}
static void ps4_field_menu_draw_main(unsigned camx,unsigned camy,unsigned sel){
    static const char* const opts[7]={"ITEM","TECH","SKILL","EQUIP","STATE","TALK","MACRO"};
    uint8_t ids[5];unsigned pn=ps4_party_ids(ids),x0,y0,x1,y1;
    ps4_ui_clear_field(camx,camy);battle_ui_box_source(4u,2u,8u,14u,&x0,&y0,&x1,&y1);
    for(unsigned i=0u;i<7u;i++){unsigned y=battle_src_y(3u+i*2u);if(i==sel)battle_draw_cursor6(29u,y,83u);battle_draw_text6(36u,y,(i==5u&&pn==1u)?"MUMBL":opts[i],80u);}
    battle_ui_box_source(26u,1u,11u,pn*5u,&x0,&y0,&x1,&y1);
    for(unsigned i=0u;i<pn;i++){unsigned y=battle_src_y(2u+i*5u);uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)ids[i]*0x80u);
        battle_draw_ram_name6(162u,y,ids[i],80u);battle_draw_text6(198u,y,"LV",80u);battle_draw_uint6(210u,y,ram16r((uint16_t)(d+8u)),80u);
        battle_draw_text6(162u,y+9u,"HP",80u);battle_draw_uint6(180u,y+9u,ram16r((uint16_t)(d+0x0Eu)),80u);battle_draw_text6(198u,y+9u,"/",80u);battle_draw_uint6(204u,y+9u,ram16r((uint16_t)(d+0x10u)),80u);
        battle_draw_text6(162u,y+18u,"TP",80u);battle_draw_uint6(180u,y+18u,ram16r((uint16_t)(d+0x12u)),80u);battle_draw_text6(198u,y+18u,"/",80u);battle_draw_uint6(204u,y+18u,ram16r((uint16_t)(d+0x14u)),80u);
    }
    battle_ui_box_source(3u,23u,12u,2u,&x0,&y0,&x1,&y1);battle_draw_text6(24u,y0+6u,"MST",80u);ps4_ui_draw_uint7_compact(46u,y0+6u,ram32r(PS4_MONEY),80u);ps4_ui_present();
}
static int ps4_field_menu_select_main(unsigned camx,unsigned camy,unsigned initial){
    unsigned sel=initial<7u?initial:0u;uint8_t prev=0u;ps4_ui_wait_buttons_up();ps4_field_menu_draw_main(camx,camy,sel);
    for(;;){uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:6u;else if(e&2u)sel=sel==6u?0u:sel+1u;else if(e&0x60u){ps4_ui_wait_buttons_up();return (int)sel;}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}else continue;ps4_field_menu_draw_main(camx,camy,sel);}
}
__attribute__((noinline))
static void ps4_field_menu(unsigned camx,unsigned camy){
    unsigned sel=0u;ps4_menu_camx=camx;ps4_menu_camy=camy;ps4_menu_overlay_active=1u;ps4_menu_base_cached=0u;
    for(;;){int q=ps4_field_menu_select_main(camx,camy,sel);if(q<0)break;sel=(unsigned)q;if(q==0)ps4_field_item_menu();else if(q==1)ps4_field_ability_list(0);else if(q==2)ps4_field_ability_list(1);else if(q==3)ps4_field_equip();else if(q==4)ps4_field_state();else if(q==5){ps4_menu_talk_pending=1u;break;}else ps4_field_macro_view();}
    ps4_menu_overlay_active=0u;ps4_menu_base_cached=0u;
}

/* Source SRAM layout used by TransferToSRAM: the logical Genesis bytes live on
 * the odd byte lane of GamePak SRAM.  Keep the source 0xA00-byte save image and
 * 0x1400 physical slot stride so saves remain PSIV-shaped rather than becoming
 * a private GBA format. */
static uint16_t ps4_save_checksum(void){
    uint16_t sum=0u;for(unsigned i=0u;i<0xA00u;i++)sum=(uint16_t)(sum+ram8r((uint16_t)(0xF100u+i)));return sum;
}
static void ps4_save_write_slot(unsigned slot){
    static const char sig[]="PHANTASY STAR 4";slot%=3u;
    ram16w(0xF012u,(uint16_t)slot);
    ram16w(0xF402u,ram16r(0xEC2Au));ram16w(0xF404u,ram16r(0xEC28u));
    ram16w(0xF406u,chaz.x);ram16w(0xF408u,chaz.y);
    for(unsigned i=0u;i<15u;i++)ram8w((uint16_t)(0xF002u+i),(uint8_t)sig[i]);
    ram16w((uint16_t)(0xF014u+slot*2u),ps4_save_checksum());
    md_write8(0xA130F1u,1u);
    for(unsigned i=0u;i<0x100u;i++)md_write8(0x200001u+i*2u,ram8r((uint16_t)(0xF000u+i)));
    uint32_t base=0x200201u+(uint32_t)slot*0x1400u;
    for(unsigned i=0u;i<0xA00u;i++)md_write8(base+(uint32_t)i*2u,ram8r((uint16_t)(0xF100u+i)));
    md_write8(0xA130F1u,0u);
}
static int ps4_field_save_allowed(void){
    uint16_t m=ram16r(0xEC28u);
    if(ram8r(0xED51u)!=0u)return 0;
    return !(m==0x0015u||m==0x0016u||m==0x0017u||m==0x00FEu||m==0x016Fu);
}
static int ps4_field_save_slot(void){
    static const char* const slots[3]={"SLOT 1","SLOT 2","SLOT 3"};
    int q=ps4_ui_select_strings("SAVE",slots,3u,ram16r(0xF012u)%3u);
    if(q<0)return 0;ps4_save_write_slot((unsigned)q);ps4_ui_message("GAME SAVED",0);return 1;
}
static void ps4_field_speed_setting(const char* title,uint16_t addr){
    static const char* const vals[5]={"1","2","3","4","5"};unsigned cur=ram8r((uint16_t)(addr+1u));if(cur>4u)cur=2u;
    int q=ps4_ui_select_strings(title,vals,5u,cur);if(q>=0)ram8w((uint16_t)(addr+1u),(uint8_t)q);
}
static void ps4_field_button_setting(void){
    static const char* const maps[6]={"A CAMP  C SPEAK","C CAMP  A SPEAK","A CAMP  B SPEAK","C CAMP  B SPEAK","B CAMP  C SPEAK","B CAMP  A SPEAK"};
    unsigned cur=(ram16r(0xF43Eu)>>1);if(cur>5u)cur=0u;int q=ps4_ui_select_strings("BUTTONS",maps,6u,cur);if(q>=0)ram16w(0xF43Eu,(uint16_t)((unsigned)q*2u));
}

/* Development-only field inspector/warp entry point.  It deliberately mutates
 * the same source-shaped map/party state used by ordinary transitions instead
 * of introducing a second debug-world representation.  This makes runtime
 * screenshots useful for diagnosing visual-vs-logical NPC placement and
 * collision while keeping normal gameplay semantics untouched. */
static void ps4_debug_wait_screen(void){
    ps4_ui_present();ps4_ui_wait_buttons_up();uint8_t p=0u;for(;;){uint8_t e=ps4_ui_wait_edge(&p);if(e&0xF0u)break;}ps4_ui_wait_buttons_up();
}
static void ps4_debug_draw_enemy_name_at(unsigned x,unsigned y,unsigned id,uint8_t c){
    uint32_t q=battle_enemy_name_off(id);while(q<0x300000u&&rom8(q)!=0xFFu&&x<232u){uint8_t ch=rom8(q++);if(ch<87u){pause_draw_window_glyph(x,y,ch,c);x+=8u;}}
}
static int ps4_debug_select_id(const char* title,unsigned lo,unsigned hi,unsigned initial,unsigned kind){
    if(hi<lo)return -1;unsigned v=initial<lo?lo:(initial>hi?hi:initial);uint8_t prev=0u;ps4_ui_wait_buttons_up();
    for(;;){
        ps4_ui_clear();ps4_ui_box(16u,18u,224u,146u);battle_draw_text(28u,28u,title,83u);battle_draw_text(28u,52u,"ID",80u);ps4_ui_draw_uint7(60u,52u,v,83u);
        if(kind==1u)ps4_debug_draw_enemy_name_at(28u,76u,v,80u);
        else if(kind==2u&&v)ps4_ui_draw_rom_name(28u,76u,PS4_ITEM_NAMES,v-1u,0xFEu,80u);
        else if(kind==6u){battle_draw_text6(20u,74u,r28_event_names[v],80u);}
        else if(kind>=3u&&kind<=5u){if(kind==3u)battle_draw_text6(20u,90u,r28_flag_names[v],80u);battle_draw_text(28u,76u,ps4_flag_test_type(kind-3u,v)?"FLAG: SET":"FLAG: CLEAR",80u);}
        else if(kind==7u){struct BattleLiteActor e[BATTLE_MAX_ENEMIES];unsigned n=battle_load_event_formation(v,e);if(n)ps4_debug_draw_enemy_name_at(28u,76u,e[0].id,80u);}
        else if(kind==0u){struct Ps4FieldMapDesc d;battle_draw_text6(20u,74u,r28_map_names[v],80u);if(!ps4_parse_field_map((uint16_t)v,&d))battle_draw_text(28u,88u,"INVALID/UNUSED",84u);}
        battle_draw_text(28u,108u,"UP/DOWN: 1",80u);battle_draw_text(28u,122u,"LEFT/RIGHT: 16",80u);battle_draw_text(28u,136u,"A: SELECT  B: BACK",82u);ps4_ui_present();
        uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)v=v>lo?v-1u:hi;else if(e&2u)v=v<hi?v+1u:lo;else if(e&4u){unsigned d=v-lo;v=lo+(d>=16u?d-16u:0u);}else if(e&8u){unsigned nv=v+16u;v=nv<=hi?nv:hi;}else if(e&0x60u){ps4_ui_wait_buttons_up();return (int)v;}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}
    }
}
static void ps4_debug_field_info(void){
    ps4_ui_clear();ps4_ui_box(4u,4u,236u,156u);battle_draw_text(16u,12u,"FIELD DEBUG",83u);
    battle_draw_text(16u,30u,"MAP",80u);ps4_ui_draw_uint7(56u,30u,field_map.map_id,80u);
    battle_draw_text(104u,30u,"CP",80u);ps4_ui_draw_uint7(136u,30u,field_map.collision_plane,80u);
    battle_draw_text(16u,48u,"X",80u);ps4_ui_draw_uint7(32u,48u,(uint32_t)chaz.x,80u);
    battle_draw_text(112u,48u,"Y",80u);ps4_ui_draw_uint7(128u,48u,(uint32_t)chaz.y,80u);
    uint16_t sx=ram16r(0xC030u),sy=ram16r(0xC034u);battle_draw_text(176u,48u,(sx==(uint16_t)chaz.x&&sy==(uint16_t)chaz.y)?"SYNC":"DESYNC",(sx==(uint16_t)chaz.x&&sy==(uint16_t)chaz.y)?82u:83u);
    battle_draw_text(16u,66u,"ST",80u);ps4_ui_draw_uint7(40u,66u,field_collision_at(chaz.x,chaz.y,0,0),80u);
    battle_draw_text(80u,66u,"U",80u);ps4_ui_draw_uint7(96u,66u,field_collision_at(chaz.x,chaz.y,0,-16),80u);
    battle_draw_text(128u,66u,"D",80u);ps4_ui_draw_uint7(144u,66u,field_collision_at(chaz.x,chaz.y,0,16),80u);
    battle_draw_text(176u,66u,"L",80u);ps4_ui_draw_uint7(192u,66u,field_collision_at(chaz.x,chaz.y,-16,0),80u);
    battle_draw_text(16u,84u,"R",80u);ps4_ui_draw_uint7(32u,84u,field_collision_at(chaz.x,chaz.y,16,0),80u);
    battle_draw_text(80u,84u,"SX",80u);ps4_ui_draw_uint7(104u,84u,sx,80u);battle_draw_text(160u,84u,"SY",80u);ps4_ui_draw_uint7(184u,84u,sy,80u);
    unsigned best=FIELD_MAX_OBJECTS,bestd=0xFFFFFFFFu;
    for(unsigned i=0u;i<field_obj_count;i++)if(field_objs[i].active){int dx=(int)field_objs[i].x-chaz.x,dy=(int)field_objs[i].y-chaz.y;unsigned d=(unsigned)(dx<0?-dx:dx)+(unsigned)(dy<0?-dy:dy);if(d<bestd){bestd=d;best=i;}}
    if(best<field_obj_count){const struct FieldObjLite* o=&field_objs[best];battle_draw_text(16u,104u,"OBJ",80u);ps4_ui_draw_uint7(48u,104u,best,80u);battle_draw_text(80u,104u,"TYPE",80u);ps4_ui_draw_uint7(120u,104u,o->type,80u);battle_draw_text(16u,122u,"OX",80u);ps4_ui_draw_uint7(40u,122u,o->x,80u);battle_draw_text(112u,122u,"OY",80u);ps4_ui_draw_uint7(136u,122u,o->y,80u);battle_draw_text(176u,122u,"F34",80u);ps4_ui_draw_uint7(208u,122u,ps4_event_flag_test(0x34u),ps4_event_flag_test(0x34u)?83u:82u);}else{battle_draw_text(16u,104u,"NO ACTIVE OBJECT",80u);battle_draw_text(176u,122u,"F34",80u);ps4_ui_draw_uint7(208u,122u,ps4_event_flag_test(0x34u),ps4_event_flag_test(0x34u)?83u:82u);}
    battle_draw_text(16u,140u,"A/B/START: RETURN",82u);ps4_debug_wait_screen();
}
static void ps4_debug_heal_party(void){
    uint8_t ids[5];unsigned n=ps4_party_ids(ids);for(unsigned i=0u;i<n;i++){uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)ids[i]*0x80u);ram16w((uint16_t)(d+0x0Eu),ram16r((uint16_t)(d+0x10u)));ram16w((uint16_t)(d+0x12u),ram16r((uint16_t)(d+0x14u)));ram8w((uint16_t)(d+0x16u),0u);}ps4_ui_message("PARTY RESTORED",0);
}
static void ps4_debug_queue_warp(uint16_t map,uint16_t x,uint16_t y,uint8_t facing,uint8_t align){
    ram16w(0xEC2Au,ram16r(0xEC28u));ram16w(0xEC28u,map);ram16w(0xEC48u,(uint16_t)(x>>3));ram16w(0xEC4Au,(uint16_t)(y>>3));ram16w(0xEC44u,(uint16_t)(facing<<2));ram16w(0xEC46u,(uint16_t)align);field_event_reload_pending=1u;
}
static int ps4_debug_warp(void){
    static const char* const names[8]={"ANY MAP ID","PIATA F1","PRINCIPAL","BASEMENT B1","BASEMENT B2","BASEMENT B3","AIEDO","CHAZ HOUSE"};
    int q=ps4_ui_select_strings("DEBUG WARP",names,8u,0u);if(q<0)return 0;
    if(q==0){int id=ps4_debug_select_id("WARP MAP",0u,0x1A0u,field_map.map_id,0u);if(id<0)return 0;struct Ps4FieldMapDesc d;if(!ps4_parse_field_map((uint16_t)id,&d)){ps4_ui_message("UNUSED MAP ID",0);return 0;}ps4_debug_autoplace_pending=1u;ps4_debug_queue_warp((uint16_t)id,0x0010u,0x0010u,0u,0u);return 1;}
    switch(q){
    case 1: ps4_debug_queue_warp(0x0013u,0x0110u,0x0120u,2u,0u);break;
    case 2: ps4_debug_queue_warp(0x0014u,0x00F0u,0x0130u,1u,4u);break;
    case 3: ps4_debug_queue_warp(0x0015u,0x0300u,0x0080u,0u,0u);break;
    case 4: ps4_debug_queue_warp(0x0016u,0x0300u,0x0080u,0u,0u);break;
    case 5: ps4_debug_queue_warp(0x0017u,0x0100u,0x0120u,3u,0u);break;
    case 6: ps4_debug_queue_warp(0x0054u,0x0160u,0x0240u,0u,0u);break;
    default:ps4_debug_queue_warp(0x005Eu,0x01F0u,0x0280u,1u,4u);break;
    }
    return 1;
}
static void ps4_debug_give_item(void){
    int id=ps4_debug_select_id("GIVE ANY ITEM",1u,160u,1u,2u);if(id<0)return;if(ps4_inventory_add((uint8_t)id)<0)ps4_ui_message("INVENTORY FULL",0);else ps4_ui_message("ITEM ADDED",0);
}
static void ps4_debug_fight_enemy(void){
    int id=ps4_debug_select_id("FIGHT ENEMY",0u,152u,9u,1u);if(id<0)return;ps4_debug_level60_party();ps4_menu_overlay_active=0u;int ran=battle_run_debug_enemy((unsigned)id);ps4_menu_overlay_active=1u;ps4_menu_base_cached=0u;if(!ran&&!ps4_return_to_title)ps4_ui_message("BATTLE FAILED",0);
}
static void ps4_debug_story(void){
    static const char* const opts[]={"STORY FLAGS","PERMANENT FLAGS","TEMPORARY FLAGS","RUN STORY EVENT"};
    int q=ps4_ui_select_strings("STORY",opts,4u,0u);if(q<0)return;
    int id=ps4_debug_select_id(q==3?"STORY EVENT":"TOGGLE FLAG",0u,q==3?0x9Fu:0xFFu,0u,q==3?6u:(unsigned)q+3u);if(id<0)return;
    if(q==3){ps4_debug_event_pending=(uint16_t)(id+1);ps4_debug_queue_warp(field_map.map_id,(uint16_t)chaz.x,(uint16_t)chaz.y,chaz.facing,0u);return;}
    if(ps4_flag_test_type((unsigned)q,(unsigned)id))ps4_flag_clear_type((unsigned)q,(unsigned)id);else ps4_flag_set_type((unsigned)q,(unsigned)id);
    ps4_debug_queue_warp(field_map.map_id,(uint16_t)chaz.x,(uint16_t)chaz.y,chaz.facing,0u);
}
static void ps4_field_debug(void){
    static const char* const opts[]={"FIELD INFO","WARP ANYWHERE","FIGHT ANY ENEMY","GIVE ANY ITEM","RESTORE PARTY","RANDOM BATTLE","LEVEL 60 PARTY","BOSS BATTLES","VEHICLE BATTLES","STORY CONTROLS"};unsigned sel=0u;
    for(;;){int q=ps4_ui_select_strings("DEBUG",opts,10u,sel);if(q<0)return;sel=(unsigned)q;
        if(q==0)ps4_debug_field_info();
        else if(q==1){if(ps4_debug_warp())return;}
        else if(q==2){ps4_debug_fight_enemy();if(ps4_return_to_title)return;}
        else if(q==3)ps4_debug_give_item();
        else if(q==4)ps4_debug_heal_party();
        else if(q==6){ps4_debug_level60_party();ps4_ui_message("LEVEL 60 PARTY READY","CHAZ RIKA RUNE DEMI KYRA");}
        else if(q==9){ps4_debug_story();if(field_event_reload_pending)return;}
        else{
            int id=0,vehicle=0;if(q==7){id=ps4_debug_select_id("BOSS FORMATION",0u,0x1Au,0u,7u);if(id<0)continue;}
            if(q==8){static const char* const v[]={"LANDMASTER","ICE DIGGER","HYDROFOIL"};vehicle=ps4_ui_select_strings("VEHICLE",v,3u,0u)+1;if(!vehicle)continue;id=ps4_debug_select_id("VEHICLE ENEMY",0u,152u,19u,1u);if(id<0)continue;}
            ps4_debug_level60_party();ps4_menu_overlay_active=0u;
            int ran=q==7?battle_run_debug_boss((unsigned)id):(q==8?battle_run_debug_vehicle((unsigned)vehicle,(unsigned)id):battle_run_debug_random());
            ps4_menu_overlay_active=1u;ps4_menu_base_cached=0u;if(!ran&&!ps4_return_to_title)ps4_ui_message("BATTLE ENDED",0);if(ps4_return_to_title)return;
        }
    }
}
__attribute__((noinline))
static void ps4_field_system(unsigned camx,unsigned camy){
    static const char* const opts[5]={"SAVE","MESSAGE","BATTLE","BUTTONS","DEBUG"};unsigned sel=0u;
    ps4_menu_camx=camx;ps4_menu_camy=camy;ps4_menu_overlay_active=1u;ps4_menu_base_cached=0u;
    for(;;){int q=ps4_ui_select_strings("SYSTEM",opts,5u,sel);if(q<0)break;sel=(unsigned)q;
        if(q==0){if(ps4_field_save_allowed())ps4_field_save_slot();else ps4_ui_message("CANNOT SAVE HERE",0);}
        else if(q==1)ps4_field_speed_setting("MESSAGE SPEED",0xF440u);
        else if(q==2)ps4_field_speed_setting("BATTLE SPEED",0xF442u);
        else if(q==3)ps4_field_button_setting();
        else{ps4_field_debug();if(field_event_reload_pending||ps4_return_to_title)break;}
    }
    ps4_menu_overlay_active=0u;ps4_menu_base_cached=0u;
}

static void ps4_field_popup_begin(void){dialogue_portrait_id=0u;dialogue_choice_visible=0u;dialogue_obj_begin();}
static void ps4_field_popup_wait(void){dialogue_obj_show();dialogue_wait_press();dialogue_obj_hide();}
static void ps4_field_popup_text(const char* a,const char* b){ps4_field_popup_begin();dialogue_obj_window_ascii_text6(24u,8u,a);if(b)dialogue_obj_window_ascii_text6(24u,20u,b);ps4_field_popup_wait();}
static void ps4_field_popup_found_item(uint8_t item){ps4_field_popup_begin();unsigned x=dialogue_obj_window_ascii_text6(24u,8u,"FOUND ");dialogue_obj_window_rom_name6(x,8u,PS4_ITEM_NAMES,item-1u,0xFEu);dialogue_obj_window_ascii_text6(24u,20u,"ITEM ADDED");ps4_field_popup_wait();}
static void ps4_field_popup_found_money(unsigned hundreds){ps4_field_popup_begin();unsigned x=dialogue_obj_window_ascii_text6(24u,8u,"FOUND ");x=dialogue_obj_window_uint7(x,8u,(uint32_t)hundreds*100u);dialogue_obj_window_ascii_text6(x+6u,8u,"MESETA");ps4_field_popup_wait();}

static uint32_t ps4_shop_list_ptr(unsigned selector){return ps4_delim_ptr(PS4_SHOP_INVENTORIES,selector,0xFFu);}
static unsigned ps4_shop_count(unsigned selector){uint32_t p=ps4_shop_list_ptr(selector);unsigned n=0;while(p<0x300000u&&rom8(p++)!=0xFFu)++n;return n;}
static uint8_t ps4_shop_item(unsigned selector,unsigned index){uint32_t p=ps4_shop_list_ptr(selector);while(index--&&rom8(p)!=0xFFu)++p;return rom8(p);}
static int ps4_shop_choose(unsigned selector){
    unsigned n=ps4_shop_count(selector);if(!n)return -1;unsigned sel=0u,visible=n<6u?n:6u,first=0u;uint8_t prev=0u;ps4_ui_wait_buttons_up();
    ps4_ui_clear();ps4_ui_box(4u,4u,236u,156u);battle_draw_text(16u,12u,"BUY",80u);battle_draw_text(132u,12u,"MESETA",80u);ps4_ui_draw_uint7(181u,12u,ram32r(PS4_MONEY),80u);
    for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;uint8_t item=ps4_shop_item(selector,k);if(k==sel)pause_draw_cursor(12u,y,83u);ps4_ui_draw_rom_name(24u,y,PS4_ITEM_NAMES,item-1u,0xFEu,k==sel?83u:80u);battle_draw_uint(184u,y,ps4_item_price(item),80u);}ps4_ui_present();
    for(;;){unsigned old=sel,oldfirst=first;uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:n-1u;else if(e&2u)sel=(sel+1u==n)?0u:sel+1u;else if(e&0x60u){ps4_ui_wait_buttons_up();return (int)sel;}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}else continue;
        first=0u;if(sel>=visible)first=sel-visible+1u;if(first+visible>n)first=n-visible;
        if(first!=oldfirst){ps4_ui_clear_row(10u,36u,226u,visible*18u);for(unsigned r=0;r<visible;r++){unsigned k=first+r,y=38u+r*18u;uint8_t item=ps4_shop_item(selector,k);if(k==sel)pause_draw_cursor(12u,y,83u);ps4_ui_draw_rom_name(24u,y,PS4_ITEM_NAMES,item-1u,0xFEu,k==sel?83u:80u);battle_draw_uint(184u,y,ps4_item_price(item),80u);}ps4_ui_present_band(36u,36u+visible*18u);}
        else{unsigned oy=38u+(old-oldfirst)*18u,ny=38u+(sel-first)*18u;uint8_t oi=ps4_shop_item(selector,old),ni=ps4_shop_item(selector,sel);ps4_ui_clear_row(10u,oy,226u,14u);ps4_ui_draw_rom_name(24u,oy,PS4_ITEM_NAMES,oi-1u,0xFEu,80u);battle_draw_uint(184u,oy,ps4_item_price(oi),80u);if(ny!=oy)ps4_ui_clear_row(10u,ny,226u,14u);pause_draw_cursor(12u,ny,83u);ps4_ui_draw_rom_name(24u,ny,PS4_ITEM_NAMES,ni-1u,0xFEu,83u);battle_draw_uint(184u,ny,ps4_item_price(ni),80u);unsigned lo=oy<ny?oy:ny,hi=oy>ny?oy:ny;ps4_ui_present_band(lo,hi+14u);}
    }
}
static void ps4_shop_run(unsigned selector){static const char* const menu[3]={"BUY","SELL","LEAVE"};for(;;){int c=ps4_ui_select_strings("SHOP",menu,3u,0u);if(c<0||c==2)return;if(c==0){int k=ps4_shop_choose(selector);if(k<0)continue;uint8_t item=ps4_shop_item(selector,(unsigned)k);unsigned price=ps4_item_price(item);uint32_t money=ram32r(PS4_MONEY);if(!price){ps4_ui_message("NOT FOR SALE",0);continue;}if(money<price){ps4_ui_message("NOT ENOUGH MESETA",0);continue;}if(ps4_inventory_add(item)<0){ps4_ui_message("INVENTORY FULL",0);continue;}ram32w(PS4_MONEY,money-price);ps4_ui_message("THANK YOU",0);}else{int slot=ps4_ui_select_inventory("SELL",0,0u);if(slot<0)continue;uint8_t item=ram8r((uint16_t)(PS4_INV_BASE+(unsigned)slot));unsigned price=ps4_item_price(item)>>1;ps4_inventory_remove((unsigned)slot);uint32_t money=ram32r(PS4_MONEY);money+=price;if(money>9999999u)money=9999999u;ram32w(PS4_MONEY,money);ps4_ui_message("SOLD",0);}}}

/* ---- source-shaped battle command execution ---- */
struct BattleCommandLite { uint8_t type,id,target,inv_slot; };
/* PSIV FillBattleStats ($776C): battle resolution never reads the equipment-
 * modified fields directly.  At the start of every battle, the original copies
 * the seven live/mod statistics into their battle-tier slots for all eleven
 * character records.  Earlier static-port builds omitted this call, leaving
 * fresh battle attack/defense fields at zero until an unrelated scripted copy. */
static void battle_fill_vehicle_stats(unsigned v){
    uint16_t d=0x4700u,save=(uint16_t)(0xFA80u+(v-1u)*32u);uint32_t p=R28_VEHICLE_DATA+(v-1u)*26u;
    for(unsigned i=0u;i<0x80u;i++)ram8w((uint16_t)(d+i),0u);ram16w((uint16_t)(d+6u),5u);ram16w((uint16_t)(d+0x0Eu),rom16(p));ram16w((uint16_t)(d+0x10u),rom16(p));
    for(unsigned i=0u;i<4u;i++)for(unsigned j=0u;j<3u;j++)ram8w((uint16_t)(d+0x18u+i*3u+j),rom8(p+2u+i));
    for(unsigned i=0u;i<3u;i++){uint16_t value=rom8(p+6u+i);ram16w((uint16_t)(d+0x24u+i*4u),value);ram16w((uint16_t)(d+0x26u+i*4u),value);}
    for(unsigned i=0u;i<14u;i++){uint8_t q=rom8(p+9u+i);ram8w((uint16_t)(d+0x30u+i*2u),q);ram8w((uint16_t)(d+0x31u+i*2u),q);}
    unsigned mask=ram8r((uint16_t)(save+4u));if(!mask)mask=rom8(p+23u);unsigned uses=0u;
    for(unsigned i=0u;i<8u;i++)if(mask&(1u<<i)){ram8w((uint16_t)(d+0x62u+i),(uint8_t)(i+1u));uint8_t mx=rom8(p+24u+uses++);ram8w((uint16_t)(d+0x6Au+i*2u),mx);ram8w((uint16_t)(d+0x6Bu+i*2u),mx);}
}
static void battle_fill_stats(void){
    unsigned v=ram16r(0xF43Cu);if(v>=1u&&v<=3u){battle_fill_vehicle_stats(v);return;}
    for(unsigned id=0u;id<11u;id++){
        uint16_t d=(uint16_t)(PS4_CHAR_BASE+id*0x80u);
        ram8w((uint16_t)(d+0x1Au),ram8r((uint16_t)(d+0x19u)));
        ram8w((uint16_t)(d+0x1Du),ram8r((uint16_t)(d+0x1Cu)));
        ram8w((uint16_t)(d+0x20u),ram8r((uint16_t)(d+0x1Fu)));
        ram8w((uint16_t)(d+0x23u),ram8r((uint16_t)(d+0x22u)));
        ram16w((uint16_t)(d+0x26u),ram16r((uint16_t)(d+0x24u)));
        ram16w((uint16_t)(d+0x2Au),ram16r((uint16_t)(d+0x28u)));
        ram16w((uint16_t)(d+0x2Eu),ram16r((uint16_t)(d+0x2Cu)));
    }
}
static unsigned battle_live_party(struct BattleLiteActor out[5]){unsigned v=ram16r(0xF43Cu);if(v>=1u&&v<=3u){battle_load_char(10u+v,&out[0]);return 1u;}uint8_t ids[5];unsigned n=ps4_party_ids(ids);for(unsigned i=0;i<n;i++)battle_load_char(ids[i],&out[i]);return n;}
static void battle_store_party(const struct BattleLiteActor p[5],unsigned n){for(unsigned i=0;i<n;i++){uint16_t d=battle_char_record((unsigned)p[i].id);ram16w((uint16_t)(d+0x0Eu),p[i].hp);ram16w((uint16_t)(d+0x12u),p[i].tp);ram8w((uint16_t)(d+0x16u),p[i].status);}}
static unsigned battle_source_damage(unsigned stat,unsigned def,unsigned power,unsigned element_factor){return battle_calc_damage_ex(stat,def,element_factor,power);}
static unsigned battle_source_heal(unsigned mental,unsigned power){unsigned q=8u;for(unsigned i=0;i<16u;i++)q+=(unsigned)(ps4_rng2_next()&7u);unsigned v=(((q*mental)>>6)+mental+power*2u)>>1;if(v>999u)v=999u;return v;}
/* Battle_OpenMainOptions builds an 8x7-tile window over the live battle plane.
 * Keep that invariant for every nested selector: compose the battle first,
 * overlay a source-window crop, and continue advancing BattleObj idle frames
 * while input is pending. */
static struct BattleLiteActor* battle_ui_party;
static struct BattleLiteActor* battle_ui_enemies;
static unsigned battle_ui_party_count,battle_ui_target,battle_ui_actor;
static uint8_t battle_ui_overlay_active;
static unsigned battle_ui_overlay_x0,battle_ui_overlay_y0,battle_ui_overlay_x1,battle_ui_overlay_y1;
static uint8_t battle_generic_stage_ready;
static unsigned battle_generic_pending_mask;
static int battle_generic_stage_x0,battle_generic_stage_x1;
static unsigned battle_generic_stage_y0,battle_generic_stage_y1;
static void battle_ui_overlay_clear(void){battle_ui_overlay_active=0u;}
static void battle_ui_discard_staged_anim(void){battle_generic_pending_mask=0u;battle_generic_stage_ready=0u;battle_ui_overlay_clear();}
static void battle_ui_overlay_set(unsigned x0,unsigned y0,unsigned x1,unsigned y1){
    if(x0>240u)x0=240u;if(x1>240u)x1=240u;if(y0>160u)y0=160u;if(y1>160u)y1=160u;
    if(x1<=x0||y1<=y0){battle_ui_overlay_clear();return;}
    battle_ui_overlay_x0=x0;battle_ui_overlay_y0=y0;battle_ui_overlay_x1=x1;battle_ui_overlay_y1=y1;battle_ui_overlay_active=1u;
}
static void battle_ui_restore_overlay_from_vram_rect(int rx0,unsigned ry0,int rx1,unsigned ry1){
    if(!battle_ui_overlay_active)return;int x0=(int)battle_ui_overlay_x0,x1=(int)battle_ui_overlay_x1;unsigned y0=battle_ui_overlay_y0,y1=battle_ui_overlay_y1;
    if(rx0>x0)x0=rx0;if(rx1<x1)x1=rx1;if(ry0>y0)y0=ry0;if(ry1<y1)y1=ry1;if(x1<=x0||y1<=y0)return;
    for(unsigned y=y0;y<y1;y++){
        int x=x0;if(x&1){unsigned idx=y*240u+(unsigned)x;uint16_t pair=MODE4_VRAM16[idx>>1];field_pause_fb[idx]=(uint8_t)(pair>>8);++x;}
        for(;x+1<x1;x+=2){unsigned idx=y*240u+(unsigned)x;uint16_t pair=MODE4_VRAM16[idx>>1];battle_u16_alias* d=(battle_u16_alias*)(void*)(field_pause_fb+idx);*d=pair;}
        if(x<x1){unsigned idx=y*240u+(unsigned)x;uint16_t pair=MODE4_VRAM16[idx>>1];field_pause_fb[idx]=(uint8_t)(pair&0xFFu);}
    }
}
static void battle_ui_restore_overlay_from_vram(void){battle_ui_restore_overlay_from_vram_rect((int)battle_ui_overlay_x0,battle_ui_overlay_y0,(int)battle_ui_overlay_x1,battle_ui_overlay_y1);}

static void battle_ui_bind(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned target){battle_ui_party=party;battle_ui_party_count=pn;battle_ui_enemies=enemies;battle_ui_target=target;battle_ui_actor=0u;battle_show_enemy_target=0u;battle_palette_valid=0u;battle_palette_hw_dirty=1u;battle_enemy_anim_cache_bind(enemies,battle_enemy_count);battle_ui_overlay_clear();battle_generic_pending_mask=0u;battle_generic_stage_ready=0u;battle_enemy_death_visible_mask=0u;battle_enemy_death_pending_mask=0u;battle_enemy_death_rect_mask=0u;for(unsigned di=0u;di<BATTLE_MAX_ENEMIES;di++)battle_enemy_death_timer[di]=0u;}
static int battle_enemy_native_origin(unsigned i,unsigned enemy_id,int* sx,int* sy){
    const uint32_t* nm=battle_enemy_native_meta(enemy_id);if(!nm||!nm[6]||!nm[7])return 0;
    const uint32_t* pm=battle_enemy_project_meta(enemy_id);
    /* CP14 keeps the source loc_10ED4 floor but projects the complete mapping
     * with the same viewport contract as the battle background.  X remains
     * centered on the already-projected loc_7960 formation anchor; Y is the
     * exact first GBA scanline selected from source [120-height*8,120). */
    if(pm&&pm[1]&&pm[2]){*sx=battle_enemy_anchor_x(i)-(int)(pm[1]/2u);*sy=(int)pm[3];return 1;}
    *sx=battle_enemy_anchor_x(i)-(int)(nm[6]*4u);
    *sy=(int)BATTLE_ENEMY_FLOOR_Y-(int)(nm[7]*8u);return 1;
}
static unsigned battle_enemy_native_bottom(unsigned enemy_id){
    (void)enemy_id;return BATTLE_ENEMY_FLOOR_Y;
}
static int battle_enemy_native_rect(unsigned i,int* x0,unsigned* y0,int* x1,unsigned* y1){
    if(i>=battle_enemy_count||i>=BATTLE_MAX_ENEMIES||!battle_ui_enemies||(!battle_enemy_draw_visible(battle_ui_enemies,i)&&!(battle_enemy_death_rect_mask&(1u<<i))))return 0;
    const uint32_t* nm=battle_enemy_native_meta(battle_ui_enemies[i].id);if(!nm||!nm[6]||!nm[7])return 0;
    const uint32_t* pm=battle_enemy_project_meta(battle_ui_enemies[i].id);int a,c0,b,d0;
    if(pm&&pm[1]&&pm[2]){int sl=battle_enemy_source_left_x(i,nm[6]);a=(int)battle_enemy_project_x_edge(sl);b=(int)battle_enemy_project_x_edge(sl+(int)(nm[6]*8u));c0=(int)pm[3];d0=c0+(int)pm[2];}
    else{if(!battle_enemy_native_origin(i,battle_ui_enemies[i].id,&a,&c0))return 0;b=a+(int)(nm[6]*8u);d0=c0+(int)(nm[7]*8u);}
    unsigned c=c0<0?0u:(unsigned)c0,d=d0<0?0u:(unsigned)d0;
    if(b<=0||a>=240||d<=c)return 0;if(a<0)a=0;if(b>240)b=240;if(d>160u)d=160u;
    *x0=a;*x1=b;*y0=c;*y1=d;return b>a&&d>c;
}
/* Compose a generic-enemy idle damage rectangle without consuming a display
 * tick.  Publication is separate so source effects that already own this
 * tick's waitvb() can share its VBlank rather than adding another one. */
__attribute__((noinline))
static int battle_ui_stage_generic_anim(unsigned changed){
    int x0=240,x1=0;unsigned y0=160u,y1=0u;
    for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++)if(changed&(1u<<i)){
        int a,b;unsigned c,d;if(battle_enemy_native_rect(i,&a,&c,&b,&d)){if(a<x0)x0=a;if(b>x1)x1=b;if(c<y0)y0=c;if(d>y1)y1=d;}
    }
    if(x1<=x0||y1<=y0)return 0;
    /* GBA DMA32 requires word-aligned row endpoints.  Expanding by <=3 pixels
     * is safe because intersecting layers are selected using the expanded box. */
    x0&=~3;x1=(x1+3)&~3;if(x1>240)x1=240;unsigned width=(unsigned)(x1-x0);
    unsigned bg=battle_background_source_index();const uint8_t* bgp=ps4_precomp_assets+PS4_PRECOMP_BATTLE_BG_PIXELS_OFF+bg*PS4_PRECOMP_BATTLE_BG_FRAME_SIZE;
    for(unsigned y=y0;y<y1;y++){unsigned off=y*240u+(unsigned)x0;dma_copy32(bgp+off,field_pause_fb+off,width);}
    battle_compose_clip_set(x0,y0,x1,y1);
    if(battle_ui_party){unsigned py0=(0x80u*5u)/7u,py1=py0+BATTLE_PARTY_H;if(y1>py0&&y0<py1)for(unsigned i=0u;i<battle_ui_party_count&&i<5u;i++){
        int px0=battle_party_center_x(i)-(int)(BATTLE_PARTY_W/2u),px1=px0+(int)BATTLE_PARTY_W;if(px1>x0&&px0<x1)battle_draw_party_frame(i,battle_ui_party[i].id);
    }}
    for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++)if(battle_enemy_draw_visible(battle_ui_enemies,i)){int a,b;unsigned c,d;if(battle_enemy_native_rect(i,&a,&c,&b,&d)&&b>x0&&a<x1&&d>y0&&c<y1){int ex,ey;if(!battle_enemy_native_origin(i,battle_ui_enemies[i].id,&ex,&ey))continue;unsigned line=12u+((battle_enemy_placement[i]>>7)&1u);battle_draw_enemy(i,battle_ui_enemies[i].id,ex,ey,line);}}
    battle_compose_clip_clear();
    if(y1>battle_src_y(22u))battle_draw_party_hud(battle_ui_party,battle_ui_party_count,battle_ui_actor);
    battle_draw_damage_overlays();
    /* Menus/cursors are authoritative in displayed VRAM and may overlap the
     * dirty rectangle.  Replay only their intersection into staging. */
    if(battle_show_enemy_target)battle_draw_enemy_target_overlay(battle_ui_enemies,battle_ui_target);
    battle_ui_restore_overlay_from_vram_rect(x0,y0,x1,y1);
    battle_generic_stage_x0=x0;battle_generic_stage_x1=x1;battle_generic_stage_y0=y0;battle_generic_stage_y1=y1;battle_generic_stage_ready=1u;
    return 1;
}
static void battle_ui_commit_generic_anim(void){
    if(!battle_generic_stage_ready)return;unsigned width=(unsigned)(battle_generic_stage_x1-battle_generic_stage_x0);
    for(unsigned y=battle_generic_stage_y0;y<battle_generic_stage_y1;y++){unsigned off=y*240u+(unsigned)battle_generic_stage_x0;dma_copy32(field_pause_fb+off,(void*)((uintptr_t)MODE4_VRAM16+off),width);}
    battle_generic_stage_ready=0u;
}
__attribute__((noinline))
static void battle_ui_redraw_generic_anim(unsigned changed){
    /* Normal UI idle has not yet consumed its tick: prepare audio first, stage
     * the changed bodies, then let the ordinary wait own exactly one VBlank. */
    ps4_audio_prepare_frame();
    if(!battle_ui_stage_generic_anim(changed)){waitvb();return;}
    waitvb();battle_ui_commit_generic_anim();
}
static void battle_ui_idle_vblank(void){
    if(battle_event_index==0u){waitvb();unsigned changed=battle_anim_advance();if(changed)battle_obj_sync(battle_ui_enemies,changed);return;}
    unsigned changed=battle_enemy_anim_advance_generic(battle_ui_enemies);
    if(!changed){waitvb();return;}
    battle_ui_redraw_generic_anim(changed);
}
static void battle_ui_wait_buttons_up(void){for(;;){battle_ui_idle_vblank();if(!(md_io_read_psiv_pad()&0xF0u))return;}}
static uint8_t battle_ui_wait_edge(uint8_t* prev){for(;;){battle_ui_idle_vblank();uint8_t now=md_io_read_psiv_pad(),pressed=(uint8_t)(now&(uint8_t)~*prev);*prev=now;if(pressed){if(pressed&0x0Fu)ram8w(0x500Au,0xF2u);else if(pressed&0x60u)ram8w(0x500Au,0xF3u);return pressed;}}}
static void battle_ui_begin(unsigned target,unsigned actor){battle_ui_target=target;battle_ui_actor=actor;battle_compose_igg(battle_ui_party,battle_ui_party_count,battle_ui_enemies,target,actor);}
static void battle_ui_present(void){battle_present_page0();battle_obj_sync(battle_ui_enemies,0u);}
static void battle_ui_present_overlay(void){
    if(battle_ui_overlay_active)battle_present_band(battle_ui_overlay_y0,battle_ui_overlay_y1);else battle_present_page0();
    battle_obj_sync(battle_ui_enemies,0u);
}
static void battle_ui_present_menu(uint8_t* first){
    if(*first){battle_ui_present();*first=0u;}else battle_ui_present_overlay();
}
static void battle_ui_message(const char* a,const char* b){
    unsigned x0,y0,x1,y1;battle_ui_begin(battle_ui_target,battle_ui_actor);battle_ui_box_source(3u,7u,31u,b?6u:4u,&x0,&y0,&x1,&y1);battle_draw_text6(x0+12u,y0+7u,a,80u);if(b)battle_draw_text6(x0+12u,y0+18u,b,80u);battle_ui_overlay_set(x0,y0,x1,y1);battle_ui_present();battle_ui_wait_buttons_up();uint8_t prev=0u;while(!(battle_ui_wait_edge(&prev)&0xF0u)){}battle_ui_wait_buttons_up();battle_ui_overlay_clear();
}
static int battle_ui_select_strings(const char* title,const char* const* options,unsigned count,unsigned initial,unsigned columns){
    if(!count)return -1;if(columns!=2u&&columns!=4u)columns=1u;
    unsigned sel=initial<count?initial:0u;
    unsigned rows=columns==1u?count:(columns==2u?(count+1u)>>1:(count+3u)>>2);if(rows>8u)rows=8u;uint8_t prev=0u;battle_ui_wait_buttons_up();
    unsigned cell_src=columns==1u?14u:(columns==2u?18u:9u),ww=cell_src*columns+2u,wh=rows*2u+3u;if(ww>38u)ww=38u;
    battle_ui_begin(battle_ui_target,battle_ui_actor);uint8_t first_present=1u;
    for(;;){unsigned x0,y0,x1,y1;battle_ui_box_source(1u,3u,ww,wh,&x0,&y0,&x1,&y1);battle_draw_text6(x0+12u,y0+6u,title,80u);
        for(unsigned i=0u;i<count;i++){unsigned col=columns==1u?0u:(columns==2u?i&1u:i&3u),row=columns==1u?i:(columns==2u?i>>1:i>>2);if(row>=rows)continue;unsigned x=x0+8u+col*cell_src*6u,y=y0+18u+row*11u;if(i==sel)battle_draw_cursor6(x,y,83u);battle_draw_text6(x+7u,y,options[i],80u);}
        battle_ui_overlay_set(x0,y0,x1,y1);battle_ui_present_menu(&first_present);uint8_t e=battle_ui_wait_edge(&prev);
        if(e&1u)sel=sel?sel-1u:count-1u;else if(e&2u)sel=(sel+1u==count)?0u:sel+1u;else if(e&4u&&columns>1u){if(sel>=columns)sel-=columns;}else if(e&8u&&columns>1u){if(sel+columns<count)sel+=columns;}else if(e&0x60u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return (int)sel;}else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return -1;}
    }
}
static int battle_choose_main_option(int allow_macro){
    static const char* const opts[3]={"COMD","MACR","RUN"};unsigned sel=0u;uint8_t prev=0u;battle_ui_wait_buttons_up();
    /* The readable seven-pixel glyphs need three eight-pixel rows plus
     * padding. Expand the source window by two rows so RUN stays inside
     * the lower border, retaining the original anchor and option order. */
    unsigned x0,y0,x1,y1;battle_ui_begin(battle_ui_target,0u);uint8_t first_present=1u;
    for(;;){
        battle_ui_box_source(3u,15u,6u,6u,&x0,&y0,&x1,&y1);
        for(unsigned i=0u;i<3u;i++){unsigned y=y0+7u+i*8u;if(i==sel)battle_draw_cursor6(x0+5u,y,83u);battle_draw_text6(x0+12u,y,opts[i],80u);}
        battle_ui_overlay_set(x0,y0,x1,y1);battle_ui_present_menu(&first_present);uint8_t e=battle_ui_wait_edge(&prev);
        if(e&1u)sel=sel?sel-1u:2u;else if(e&2u)sel=sel==2u?0u:sel+1u;else if(e&0x60u){if(sel==1u&&!allow_macro)continue;battle_ui_wait_buttons_up();battle_ui_overlay_clear();return (int)sel;}else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return -1;}
    }
}
static unsigned battle_item_type(uint8_t item);
static int battle_actor_has_weapon(const struct BattleLiteActor* a){
    if(a->id>=11u)return 1;
    uint16_t d=battle_char_record((unsigned)a->id);
    for(unsigned h=0u;h<2u;h++){uint8_t item=ram8r((uint16_t)(d+0x4Cu+h));unsigned t=battle_item_type(item);if(t>=1u&&t<=4u)return 1;}
    return 0;
}
static unsigned battle_fill_ability_list(const struct BattleLiteActor* a,int skill,uint8_t ids[16],uint8_t disabled[16],uint8_t uses[16]){
    uint16_t d=battle_char_record((unsigned)a->id),off=(uint16_t)(d+(skill?0x62u:0x52u));unsigned cap=skill?8u:16u,n=0u;
    for(unsigned rev=0u;rev<cap;rev++){
        unsigned i=cap-1u-rev;uint8_t id=ram8r((uint16_t)(off+i));if(!id)continue;
        uint32_t p=(skill?PS4_SKILL_DATA:PS4_TECH_DATA)+(uint32_t)(id-1u)*8u;
        if(!(rom8(p+2u)&0x10u))continue; /* Battle_Fill{Tech,Skill}List in-battle bit */
        uint8_t bad=0u,use=0u;
        if(skill){use=ram8r((uint16_t)(d+0x6Au+i*2u));if((rom8(p+1u)&0x80u)&&!battle_actor_has_weapon(a))bad=1u;if(!use)bad=1u;}
        else if(a->tp<rom8(p+1u))bad=1u;
        ids[n]=id;disabled[n]=bad;uses[n]=use;++n;
    }
    return n;
}
static unsigned battle_fill_item_list(const struct BattleLiteActor* a,uint8_t ids[44],uint8_t slots[44]){
    uint16_t d=battle_char_record((unsigned)a->id);unsigned n=0u;
    for(unsigned i=0u;i<4u;i++){uint8_t id=ram8r((uint16_t)(d+0x4Cu+i));if(id){ids[n]=id;slots[n]=(uint8_t)(0x80u+i);++n;}}
    for(unsigned i=0u;i<40u;i++){uint8_t id=ram8r((uint16_t)(PS4_INV_BASE+i));if(id){ids[n]=id;slots[n]=(uint8_t)i;++n;}}
    return n;
}
static void battle_draw_command_icon(unsigned icon,unsigned x,unsigned y){
    if(icon>=PS4_PRECOMP_BATTLE_COMMAND_ICON_FRAMES)return;const uint8_t* p=ps4_precomp_assets+PS4_PRECOMP_BATTLE_COMMAND_ICON_PIXELS_OFF+icon*PS4_PRECOMP_BATTLE_COMMAND_ICON_W*PS4_PRECOMP_BATTLE_COMMAND_ICON_H;
    for(unsigned dy=0u;dy<PS4_PRECOMP_BATTLE_COMMAND_ICON_H&&y+dy<160u;dy++)for(unsigned dx=0u;dx<PS4_PRECOMP_BATTLE_COMMAND_ICON_W&&x+dx<240u;dx++){uint8_t c=p[dy*PS4_PRECOMP_BATTLE_COMMAND_ICON_W+dx];if(c)field_pause_fb[(y+dy)*240u+x+dx]=c;}
}
static int battle_choose_target(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned initial,unsigned actor){
    unsigned t=initial;while(!enemies[t].hp)t=battle_next_target(enemies,t,1);uint8_t prev=0;battle_ui_overlay_clear();battle_ui_wait_buttons_up();
    /* Compose the battle once.  EnemyTargetCursor lives entirely above y=42,
     * before enemy/party pixels begin, so left/right target changes can restore
     * that top band directly from the immutable battle background. */
    battle_show_enemy_target=0u;battle_ui_target=t;battle_ui_actor=actor;battle_compose_igg(party,pn,enemies,t,actor);
    battle_show_enemy_target=1u;battle_draw_enemy_target_overlay(enemies,t);battle_present_page0();battle_obj_sync(enemies,0u);
    for(;;){
        uint8_t e=battle_ui_wait_edge(&prev);unsigned old=t;
        if(e&4u)t=battle_next_target(enemies,t,-1);else if(e&8u)t=battle_next_target(enemies,t,1);
        else if(e&0x60u){battle_ui_target=t;battle_show_enemy_target=0u;battle_ui_wait_buttons_up();return (int)t;}
        else if(e&0x90u){battle_show_enemy_target=0u;battle_ui_wait_buttons_up();return -1;}
        if(t!=old){unsigned bg=battle_background_source_index(),bytes=42u*240u;battle_ui_target=t;dma_copy32(ps4_precomp_assets+PS4_PRECOMP_BATTLE_BG_PIXELS_OFF+bg*PS4_PRECOMP_BATTLE_BG_FRAME_SIZE,field_pause_fb,bytes);
            /* Rebuild the upper scene before its target window. Clearing this
             * whole band used to erase large boss heads when switching targets. */
            if(battle_event_index!=0u){battle_compose_clip_set(0,0u,240,42u);
                for(unsigned i=0u;i<battle_enemy_count;i++)if(battle_enemy_draw_visible(enemies,i)){
                    int ex,ey,a,b;unsigned c,d;
                    if(battle_enemy_native_rect(i,&a,&c,&b,&d)&&c<42u&&battle_enemy_native_origin(i,enemies[i].id,&ex,&ey))battle_draw_enemy(i,enemies[i].id,ex,ey,12u+((battle_enemy_placement[i]>>7)&1u));
                }battle_compose_clip_clear();}
            battle_draw_enemy_target_overlay(enemies,t);battle_present_band(0u,42u);battle_obj_sync(enemies,0u);}
    }
}
static int battle_choose_ability(const struct BattleLiteActor* a,int skill,uint8_t* out_id){
    uint8_t ids[16],disabled[16],uses[16];unsigned n=battle_fill_ability_list(a,skill,ids,disabled,uses);if(!n)return 0;unsigned sel=0u;uint8_t prev=0u;battle_ui_wait_buttons_up();
    /* WinGroup_Battle uses width $0B for TECH and $0E for SKILL. The source
     * list height is 2*n+1 tiles (records $03,$05,$07...); keep at most four
     * visible entries per page in the GBA projection. */
    const unsigned ww=skill?14u:11u;battle_ui_begin(battle_ui_target,battle_ui_actor);uint8_t first_present=1u;
    for(;;){unsigned first=sel&~3u,visible=n-first;if(visible>4u)visible=4u;unsigned wh=visible*2u+1u,x0,y0,x1,y1;battle_ui_box_source(3u,2u,ww,wh,&x0,&y0,&x1,&y1);
        for(unsigned r=0u;r<visible;r++){unsigned k=first+r,y=y0+7u+r*11u;uint8_t col=disabled[k]?82u:(k==sel?83u:80u);if(k==sel)battle_draw_cursor6(x0+5u,y,col);battle_draw_rom_name6(x0+12u,y,skill?PS4_SKILL_NAMES:PS4_TECH_NAMES,ids[k]-1u,0xFEu,col);if(skill)battle_draw_uint6(x1-24u,y,uses[k],col);else battle_draw_uint6(x1-24u,y,rom8(PS4_TECH_DATA+(uint32_t)(ids[k]-1u)*8u+1u),col);}
        battle_ui_overlay_set(x0,y0,x1,y1);battle_ui_present_menu(&first_present);uint8_t e=battle_ui_wait_edge(&prev);
        if(e&1u){unsigned row=sel-first;sel=row?sel-1u:first+visible-1u;}else if(e&2u){unsigned row=sel-first;sel=(row+1u<visible)?sel+1u:first;}else if(e&4u){if(first>=4u)sel-=4u;}else if(e&8u){if(first+4u<n){sel+=4u;if(sel>=n)sel=n-1u;}}else if(e&0x60u){if(disabled[sel])continue;*out_id=ids[sel];battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 1;}else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 0;}
    }
}
static int battle_choose_item(const struct BattleLiteActor* a,uint8_t* id,uint8_t* slot){
    uint8_t ids[44],slots[44];unsigned n=battle_fill_item_list(a,ids,slots);if(!n)return 0;unsigned sel=0u;uint8_t prev=0u;battle_ui_wait_buttons_up();battle_ui_begin(battle_ui_target,battle_ui_actor);uint8_t first_present=1u;
    for(;;){unsigned first=sel&~3u,visible=n-first;if(visible>4u)visible=4u;unsigned wh=visible*2u+1u,x0,y0,x1,y1;battle_ui_box_source(3u,2u,13u,wh,&x0,&y0,&x1,&y1);
        for(unsigned r=0u;r<visible;r++){unsigned k=first+r,y=y0+7u+r*11u;if(k==sel)battle_draw_cursor6(x0+5u,y,83u);battle_draw_rom_name6(x0+12u,y,PS4_ITEM_NAMES,ids[k]-1u,0xFEu,k==sel?83u:80u);}battle_ui_overlay_set(x0,y0,x1,y1);battle_ui_present_menu(&first_present);uint8_t e=battle_ui_wait_edge(&prev);
        if(e&1u){unsigned row=sel-first;sel=row?sel-1u:first+visible-1u;}else if(e&2u){unsigned row=sel-first;sel=(row+1u<visible)?sel+1u:first;}else if(e&4u){if(first>=4u)sel-=4u;}else if(e&8u){if(first+4u<n){sel+=4u;if(sel>=n)sel=n-1u;}}else if(e&0x60u){*slot=slots[sel];*id=ids[sel];battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 1;}else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 0;}
    }
}
static unsigned battle_command_target_kind(const struct BattleCommandLite* c){uint32_t p;if(c->type==6u)return rom8(R28_VEHICLE_SKILLS+(unsigned)(c->id-1u)*8u+2u)&15u;if(c->type==1u)return 1u;if(c->type==2u)p=PS4_TECH_DATA+(uint32_t)(c->id-1u)*8u;else if(c->type==3u)p=PS4_SKILL_DATA+(uint32_t)(c->id-1u)*8u;else if(c->type==4u)p=ps4_item_ptr(c->id);else return 3u;return rom8(p+2u)&15u;}
static unsigned battle_ally_cursor_x(unsigned pn,unsigned pos){
    /* Battle_AllyCursorInitPos = $B0,$E8,$120,$158,$190 and
     * Battle_AllyCursorPosIndex = 2,1,1,0,0.  These are the exact 320-wide
     * source positions projected 3/4 to GBA around battle center $120. */
    static const uint8_t gx[5]={36u,78u,120u,162u,204u};static const uint8_t first[5]={2u,1u,1u,0u,0u};if(!pn)pn=1u;if(pn>5u)pn=5u;unsigned q=(unsigned)first[pn-1u]+pos;return q<5u?gx[q]:gx[4];
}
static int battle_choose_ally(const struct BattleLiteActor party[5],unsigned pn,unsigned initial,unsigned target_kind){
    if(!pn)return -1;unsigned pos=initial<pn?initial:0u;int any=0;for(unsigned i=0u;i<pn;i++)if(battle_party_class_valid(&party[i],target_kind)){any=1;break;}if(!any)return -1;uint8_t prev=0;battle_show_enemy_target=0u;battle_ui_wait_buttons_up();
    for(;;){unsigned cx=battle_ally_cursor_x(pn,pos),cy=118u;battle_ui_begin(battle_ui_target,battle_ui_actor);battle_draw_cursor6(cx>3u?cx-3u:0u,cy,84u);battle_ui_overlay_set(cx>3u?cx-3u:0u,cy,cx+4u,126u);battle_ui_present();uint8_t e=battle_ui_wait_edge(&prev);if(e&(1u|4u))pos=pos?pos-1u:pn-1u;else if(e&(2u|8u))pos=(pos+1u==pn)?0u:pos+1u;else if(e&0x60u){if(!battle_party_class_valid(&party[pos],target_kind))continue;battle_ui_wait_buttons_up();battle_ui_overlay_clear();return (int)pos;}else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return -1;}}
}
static int battle_vehicle_command(const struct BattleLiteActor* a,struct BattleCommandLite* c){
    uint8_t choices[10]={0u};unsigned n=1u;uint16_t d=0x4700u;
    for(unsigned i=0u;i<8u;i++)if(ram8r((uint16_t)(d+0x62u+i)))choices[n++]=ram8r((uint16_t)(d+0x62u+i));choices[n++]=0xFFu;
    unsigned sel=0u;uint8_t prev=0u,first=1u;battle_ui_wait_buttons_up();battle_ui_begin(battle_ui_target,battle_ui_actor);
    for(;;){unsigned x0,y0,x1,y1;battle_ui_box_source(3u,16u,18u,9u,&x0,&y0,&x1,&y1);
        for(unsigned i=0u;i<n;i++){unsigned y=y0+6u+i*11u;uint8_t id=choices[i];if(i==sel)battle_draw_cursor6(x0+5u,y,83u);battle_draw_text6(x0+12u,y,id==0xFFu?"DEFEND":r28_vehicle_skill_names[id],80u);if(id&&id!=0xFFu)battle_draw_uint6(x0+90u,y,ram8r((uint16_t)(d+0x6Au+(id-1u)*2u)),80u);}
        battle_ui_overlay_set(x0,y0,x1,y1);battle_ui_present_menu(&first);uint8_t e=battle_ui_wait_edge(&prev);
        if(e&1u)sel=sel?sel-1u:n-1u;else if(e&2u)sel=sel+1u<n?sel+1u:0u;else if(e&0x60u){uint8_t id=choices[sel];if(id&&id!=0xFFu&&!ram8r((uint16_t)(d+0x6Au+(id-1u)*2u)))continue;c->type=id==0xFFu?5u:(id?6u:1u);c->id=id==0xFFu?0u:id;c->target=0u;c->inv_slot=0xFFu;battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 1;}else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 0;}
    }
}
static int battle_command_for_actor(const struct BattleLiteActor* a,struct BattleCommandLite* c){
    if(a->id>=11u)return battle_vehicle_command(a,c);
    uint8_t tids[16],td[16],tu[16],sids[16],sd[16],su[16],iids[44],islots[44];uint8_t available[5];
    available[0]=(uint8_t)battle_actor_has_weapon(a);available[1]=(uint8_t)(battle_fill_ability_list(a,0,tids,td,tu)!=0u);available[2]=(uint8_t)(battle_fill_ability_list(a,1,sids,sd,su)!=0u);available[3]=(uint8_t)(battle_fill_item_list(a,iids,islots)!=0u);available[4]=1u;
    unsigned q=available[0]?0u:4u;uint8_t prev=0u;battle_ui_wait_buttons_up();battle_ui_begin(battle_ui_target,battle_ui_actor);uint8_t first_present=1u;
    for(;;){
        /* Battle_OpenCharComd's authored window is $0F x $05.  Preserve the
         * source actor-dependent X anchors recovered from loc_17A4, but use
         * the actual source row $13 and project the border/icons/cursor too. */
        static const uint8_t source_col[5]={3u,15u,9u,9u,15u};unsigned x0,y0,x1,y1;battle_ui_box_source(source_col[battle_ui_actor<5u?battle_ui_actor:0u],19u,14u,4u,&x0,&y0,&x1,&y1);
        unsigned cx[5]={x0+7u,x0+23u,x0+39u,x0+55u,x0+71u};
        for(unsigned i=0u;i<5u;i++)if(available[i])battle_draw_command_icon(i,cx[i],y0+7u);
        battle_draw_cursor6(cx[q]-5u,y0+9u,83u);battle_ui_overlay_set(x0,y0,x1,y1);battle_ui_present_menu(&first_present);uint8_t e=battle_ui_wait_edge(&prev);
        if(e&4u)q=q?q-1u:4u;else if(e&8u)q=q==4u?0u:q+1u;else if(e&0x60u){
            if(!available[q])continue;c->type=(uint8_t)(q+1u);c->id=0u;c->target=0u;c->inv_slot=0xFFu;
            if(q==1u&&!battle_choose_ability(a,0,&c->id)){battle_ui_begin(battle_ui_target,battle_ui_actor);first_present=1u;continue;}
            if(q==2u&&!battle_choose_ability(a,1,&c->id)){battle_ui_begin(battle_ui_target,battle_ui_actor);first_present=1u;continue;}
            if(q==3u&&!battle_choose_item(a,&c->id,&c->inv_slot)){battle_ui_begin(battle_ui_target,battle_ui_actor);first_present=1u;continue;}
            battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 1;
        }else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return 0;}
    }
}
static void battle_apply_friendly(struct BattleLiteActor party[5],unsigned pn,unsigned actor,unsigned chosen,unsigned effect,unsigned power,unsigned target_kind){unsigned start=chosen<pn?chosen:actor,end=start+1u;if(target_kind==5u||target_kind==7u||target_kind==9u){start=0u;end=pn;}for(unsigned i=start;i<end;i++){if(i>=pn)break;if(!battle_party_class_valid(&party[i],target_kind))continue;if(effect==0x12u||effect==0x16u){unsigned add=effect==0x16u?party[i].max_hp:battle_source_heal(party[actor].mental,power);unsigned hp=party[i].hp+add;if(hp>party[i].max_hp)hp=party[i].max_hp;party[i].hp=(uint16_t)hp;if(hp)party[i].status&=(uint8_t)~4u;}else if(effect==0x13u){party[i].status&=(uint8_t)~1u;}else if(effect==0x14u){party[i].status&=(uint8_t)~2u;party[i].agility=party[i].base_agility;party[i].dexterity=party[i].base_dexterity;}else if(effect==0x15u){if(!party[i].hp){party[i].hp=(uint16_t)(party[i].max_hp>>1);if(!party[i].hp)party[i].hp=1u;party[i].status&=(uint8_t)~4u;}party[i].agility=party[i].base_agility;party[i].dexterity=party[i].base_dexterity;}else if(effect==0x27u){party[i].strength=party[i].base_strength;party[i].mental=party[i].base_mental;party[i].agility=party[i].base_agility;party[i].dexterity=party[i].base_dexterity;party[i].atk=party[i].base_atk;party[i].def=party[i].base_def;party[i].magic_def=party[i].base_magic_def;for(unsigned e=0u;e<14u;e++)party[i].element_prop[e]=party[i].base_element_prop[e];}else if(effect==9u)party[i].atk=(uint16_t)(party[i].atk+power);else if(effect==10u)party[i].def=(uint16_t)(party[i].def+power);else if(effect==0x26u){/* AbilityEffect_DexterityUp: Skill Vision uses the actor stat selected by SkillData (Mental in the bugfixed source), restores the target's modified/base DEX, then adds that stat byte. */party[i].dexterity=(uint8_t)(party[i].base_dexterity+party[actor].mental);}}}
static void battle_mark_enemy_reaction(unsigned ei,uint8_t bits){if(ei<battle_enemy_count)battle_enemy_reaction_flags[ei]|=bits;}
static uint8_t battle_pending_special_death=0xFFu;
static int battle_enemy_generic_death_sfx(unsigned id){
    /* Enemy_Dead ($C0BE): $8C takes its special death object and $82-$87
     * jump to loc_2D936; all other enemies start SFXID_EnemyKilled ($B9). */
    return id!=0x8Cu && !(id>=0x82u&&id<0x88u);
}
static void battle_enemy_set_hp(struct BattleLiteActor* e,unsigned hp){
    if(!e)return;unsigned old=e->hp;if(hp>e->max_hp)hp=e->max_hp;e->hp=(uint16_t)hp;
    if(old&&!hp&&!battle_defer_death_audio){
        if(battle_enemy_generic_death_sfx(e->id)){
            ram8w(0x500Au,0xB9u);
            if(battle_event_index!=0u&&battle_ui_enemies)
                for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++)if(e==&battle_ui_enemies[i]){
                    battle_enemy_death_visible_mask|=(uint8_t)(1u<<i);battle_enemy_death_pending_mask|=(uint8_t)(1u<<i);battle_enemy_death_timer[i]=0u;break;
                }
        }else if(e->id>=0x82u&&e->id<=0x87u)battle_pending_special_death=e->id;
    }
}
static void battle_process_pending_enemy_deaths(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    uint8_t pending=battle_enemy_death_pending_mask;if(!pending||battle_event_index==0u)return;
    for(unsigned t=0u;pending;t+=2u){
        uint8_t changed=0u,erase=0u;
        for(unsigned i=0u;i<battle_enemy_count&&i<BATTLE_MAX_ENEMIES;i++)if(pending&(1u<<i)){
            unsigned limit=battle_enemy_death_limit(enemies[i].id);
            if(t>=limit){pending&=(uint8_t)~(1u<<i);battle_enemy_death_pending_mask&=(uint8_t)~(1u<<i);battle_enemy_death_visible_mask&=(uint8_t)~(1u<<i);erase|=(uint8_t)(1u<<i);changed|=(uint8_t)(1u<<i);}
            else{battle_enemy_death_timer[i]=(uint8_t)t;changed|=(uint8_t)(1u<<i);}
        }
        changed|=(uint8_t)battle_enemy_anim_advance_generic(enemies);
        battle_enemy_death_rect_mask=erase;
        if(changed)battle_ui_redraw_generic_anim(changed);else waitvb();
        battle_enemy_death_rect_mask=0u;
    }
}
static int battle_action_has_resource(const struct BattleLiteActor party[5],unsigned pn,unsigned actor,const struct BattleCommandLite* c){
    if(actor>=pn||!party[actor].hp)return 0;
    if(c->type==2u){if(party[actor].status&0x10u)return 0;uint32_t p=PS4_TECH_DATA+(uint32_t)(c->id-1u)*8u;return party[actor].tp>=rom8(p+1u);}
    if(c->type==3u){uint16_t d=battle_char_record((unsigned)party[actor].id);uint32_t sp=PS4_SKILL_DATA+(uint32_t)(c->id-1u)*8u;if(rom8(sp+1u)&0x80u){uint8_t rh=ram8r((uint16_t)(d+0x4Cu)),lh=ram8r((uint16_t)(d+0x4Du));unsigned rt=battle_item_type(rh),lt=battle_item_type(lh);int armed=(rt>=1u&&rt<=4u)||(lt>=1u&&lt<=4u);if(!armed)return 0;}for(unsigned i=0u;i<8u;i++)if(ram8r((uint16_t)(d+0x62u+i))==c->id)return ram8r((uint16_t)(d+0x6Au+i*2u))!=0u;return 0;}
    return 1;
}
static void battle_consume_action_resource(struct BattleLiteActor party[5],unsigned actor,const struct BattleCommandLite* c){
    if(c->type==2u){uint32_t p=PS4_TECH_DATA+(uint32_t)(c->id-1u)*8u;unsigned cost=rom8(p+1u);if(party[actor].tp>=cost)party[actor].tp=(uint16_t)(party[actor].tp-cost);}
    else if(c->type==3u){uint16_t d=battle_char_record((unsigned)party[actor].id);for(unsigned i=0u;i<8u;i++)if(ram8r((uint16_t)(d+0x62u+i))==c->id){uint16_t uo=(uint16_t)(d+0x6Au+i*2u);uint8_t u=ram8r(uo);if(u)ram8w(uo,(uint8_t)(u-1u));break;}}
}
static void battle_apply_enemy_effect(struct BattleLiteActor* e,unsigned effect,unsigned power){
    if(effect==2u){battle_enemy_set_hp(e,0u);return;}if(effect==3u){e->atk=(uint16_t)(e->atk>power?e->atk-power:1u);return;}if(effect==4u){e->def=(uint16_t)(e->def>power?e->def-power:0u);return;}if(effect==6u){e->agility=(uint8_t)(e->agility>power?e->agility-power:0u);return;}if(effect==7u){e->status|=8u;return;}if(effect==8u){e->status|=16u;return;}if(effect==0x1Bu){e->status|=1u;return;}if(effect==0x1Cu){e->status|=2u;e->status&=(uint8_t)~8u;e->agility=1u;e->dexterity=1u;return;}if(effect==0x21u){e->dexterity=(uint8_t)(e->dexterity>power?e->dexterity-power:0u);return;}
}
static unsigned battle_item_type(uint8_t item){return item?rom8(PS4_ITEM_DATA+(uint32_t)(item-1u)*22u+10u):0u;}
static unsigned battle_actor_slasher_mask(const struct BattleLiteActor* a){
    if(!a||(a->id!=1u&&a->id!=9u))return 0u;uint16_t d=battle_char_record((unsigned)a->id);unsigned m=0u;
    uint8_t rh=ram8r((uint16_t)(d+0x4Cu)),lh=ram8r((uint16_t)(d+0x4Du));if(rh&&battle_item_type(rh)==2u)m|=1u;if(lh&&battle_item_type(lh)==2u)m|=2u;return m;
}
static uint8_t battle_party_weapon_sound(const struct BattleLiteActor* a){
    /* Character attack setup lowers equipped item IDs through the source
     * Battle_WeaponIndex table at $883A. Close-range weapon IDs then index
     * CloseRangeWpns_SoundIndexes at $36FDC; ranged weapons use Shot. */
    if(!a)return 0u;uint16_t d=battle_char_record((unsigned)a->id);
    uint8_t items[2]={ram8r((uint16_t)(d+0x4Cu)),ram8r((uint16_t)(d+0x4Du))};
    for(unsigned h=0u;h<2u;h++){
        uint8_t item=items[h];if(!item||battle_item_type(item)==5u)continue;
        uint8_t wid=rom8(0x883Au+(uint32_t)item);if(!wid)continue;
        if(wid<=0x15u)return rom8(0x36FDCu+(uint32_t)wid);
        return 0xB6u; /* BattleObj_Shot */
    }
    return 0u;
}
static int battle_actor_has_item(const struct BattleLiteActor* a,uint8_t item){
    if(!a)return 0;uint16_t d=battle_char_record((unsigned)a->id);return ram8r((uint16_t)(d+0x4Cu))==item||ram8r((uint16_t)(d+0x4Du))==item;
}
static unsigned battle_item_element(uint8_t item){return item?rom8(PS4_ITEM_DATA+(uint32_t)(item-1u)*22u+18u):0u;}
static void battle_action_hold(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned frames);
static void battle_source_sfx_cue(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned* elapsed,unsigned frame,uint8_t sound){
    if(frame>*elapsed){battle_action_hold(enemies,frame-*elapsed);*elapsed=frame;}if(sound)ram8w(0x500Au,sound);
}
static void battle_item_source_audio(uint8_t item,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    /* Character_DoItem copies InventoryData byte +7 to the item-object selector,
     * then CharItem_ItemObjOffs drives source BattleObj_AbilityAnim/Sound objects.
     * Delays below are the source low-byte timers; high bytes are Sound_Index. */
    if(!item)return;uint32_t p=ps4_item_ptr(item);unsigned obj=rom8(p+7u),t=0u;
    switch(obj){
    case 1u: case 7u: case 0xDu: case 0xEu: case 0x12u: /* HealSingle/HealMulti */
        battle_source_sfx_cue(enemies,&t,0x26u,0xCCu);break; /* Monomate -> Res */
    case 2u: battle_source_sfx_cue(enemies,&t,0u,0xCBu);break; /* Psyco Wand -> Brose */
    case 3u: battle_source_sfx_cue(enemies,&t,0u,0xBBu);battle_source_sfx_cue(enemies,&t,0x15u,0xBEu);break;
    case 4u: battle_source_sfx_cue(enemies,&t,0u,0xBBu);battle_source_sfx_cue(enemies,&t,2u,0xCEu);break;
    case 5u: battle_source_sfx_cue(enemies,&t,0u,0xBBu);battle_source_sfx_cue(enemies,&t,2u,0xCDu);break;
    case 6u: battle_source_sfx_cue(enemies,&t,0u,0xBEu);battle_source_sfx_cue(enemies,&t,2u,0xCBu);break;
    case 8u: /* Laco Rod: the Recovery cue is source-timed; later Efess waits on a visual state. */
        battle_source_sfx_cue(enemies,&t,2u,0xCDu);break;
    case 9u: case 0xBu: battle_source_sfx_cue(enemies,&t,0u,0xC9u);break; /* Swift Helm -> Saner */
    case 0xAu: battle_source_sfx_cue(enemies,&t,0x18u,0xC4u);battle_source_sfx_cue(enemies,&t,0x20u,0xC4u);battle_source_sfx_cue(enemies,&t,0x28u,0xC4u);break;
    case 0xCu: break; /* Mahlay Dagger's Efess cue is visual-state gated; do not invent an absolute frame. */
    case 0xFu: case 0x10u: battle_source_sfx_cue(enemies,&t,0u,0xC4u);battle_source_sfx_cue(enemies,&t,8u,0xC4u);battle_source_sfx_cue(enemies,&t,0x10u,0xC4u);break;
    case 0x11u: case 0x13u: battle_source_sfx_cue(enemies,&t,0u,0xCDu);battle_source_sfx_cue(enemies,&t,0x24u,0xCDu);break;
    case 0x14u: battle_source_sfx_cue(enemies,&t,0u,0xDCu);break; /* Mahlay Shield -> Deban */
    case 0x15u: battle_source_sfx_cue(enemies,&t,2u,0xC0u);break; /* Dynamite -> Megid */
    case 0x16u: /* Eclipse Torch: loc_3AAE2 adds FireBreath ($C2) to authored delays. */
        battle_source_sfx_cue(enemies,&t,0x0Cu,0xC2u);battle_source_sfx_cue(enemies,&t,0x16u,0xC2u);battle_source_sfx_cue(enemies,&t,0x20u,0xC2u);battle_source_sfx_cue(enemies,&t,0x2Au,0xC2u);battle_source_sfx_cue(enemies,&t,0x34u,0xC2u);battle_source_sfx_cue(enemies,&t,0x3Eu,0xC2u);break;
    case 0x17u: battle_source_sfx_cue(enemies,&t,0x18u,0xCCu);battle_source_sfx_cue(enemies,&t,0x2Au,0xCCu);battle_source_sfx_cue(enemies,&t,0x3Cu,0xCCu);break;
    default: break;
    }
}
static uint8_t battle_suppress_skill_source_audio;
static void battle_skill_source_audio(uint8_t skill,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    /* Character_DoSkill indexes CharSkill_SkillObjsOffs with the skill ID.
     * These cues are copied only from explicit Battle_LoadSoundObj / ability
     * sound+timer words in the corresponding PSIV BattleObj routines.  Keep
     * this table conservative: skills whose later-state cue timing has not
     * been proved are intentionally silent here instead of being guessed. */
    unsigned t=0u;
    switch(skill){
    case 0x01u: battle_source_sfx_cue(enemies,&t,0x10u,0xF5u);battle_source_sfx_cue(enemies,&t,0x29u,0xF5u);break; /* Crosscut */
    case 0x02u: battle_source_sfx_cue(enemies,&t,0x10u,0xF5u);break; /* Rayblade */
    case 0x04u: case 0x31u: battle_source_sfx_cue(enemies,&t,0x33u,0xBEu);break; /* Flaeli */
    case 0x05u: battle_source_sfx_cue(enemies,&t,0x1Eu,0xC5u);battle_source_sfx_cue(enemies,&t,0x26u,0xBEu);break; /* Flare */
    case 0x07u: battle_source_sfx_cue(enemies,&t,0x32u,0xC5u);battle_source_sfx_cue(enemies,&t,0x37u,0xBFu);break; /* Astral */
    case 0x0Eu: battle_source_sfx_cue(enemies,&t,0x1Fu,0xCFu);break; /* Burstroc: initial $1E timer expires -> AndroidSkillImplant */
    case 0x08u: case 0x10u: battle_source_sfx_cue(enemies,&t,0x10u,0xF5u);break; /* Airslash / Sweeping */
    case 0x11u: battle_source_sfx_cue(enemies,&t,0x1Fu,0xCFu);break; /* Phonon: initial $1E timer expires -> AndroidSkillImplant */
    case 0x0Au: case 0x32u: battle_source_sfx_cue(enemies,&t,0x30u,0xC6u);break; /* Hewn */
    case 0x0Bu: case 0x33u: battle_source_sfx_cue(enemies,&t,0x20u,0xCEu);break; /* Tandle */
    case 0x0Fu: battle_source_sfx_cue(enemies,&t,0x1Eu,0xCDu);battle_source_sfx_cue(enemies,&t,0x3Cu,0xC5u);break; /* Posibolt */
    case 0x15u: case 0x30u: battle_source_sfx_cue(enemies,&t,0x10u,0xD2u);break; /* Eliminat */
    case 0x17u: case 0x34u: battle_source_sfx_cue(enemies,&t,0x36u,0xD3u);battle_source_sfx_cue(enemies,&t,0x3Au,0xD3u);battle_source_sfx_cue(enemies,&t,0x4Au,0xD3u);battle_source_sfx_cue(enemies,&t,0x52u,0xD3u);break; /* Spark */
    case 0x19u: case 0x1Au: battle_source_sfx_cue(enemies,&t,0x2Au,0xCBu);break; /* Holyword / DthSpell */
    case 0x1Bu: battle_source_sfx_cue(enemies,&t,0x20u,0xCBu);break; /* Negatis */
    case 0x1Du: battle_source_sfx_cue(enemies,&t,0x1Eu,0xBEu);break; /* Telele */
    case 0x1Fu: battle_source_sfx_cue(enemies,&t,0x10u,0xD2u);battle_source_sfx_cue(enemies,&t,0x20u,0xCAu);break; /* Earth */
    case 0x22u: battle_source_sfx_cue(enemies,&t,0x0Fu,0xB5u);break; /* Crash */
    case 0x23u: battle_source_sfx_cue(enemies,&t,0x1Eu,0xC5u);battle_source_sfx_cue(enemies,&t,0x26u,0xBEu);break; /* Stasis Beam */
    case 0x24u: battle_source_sfx_cue(enemies,&t,0x1Eu,0xBEu);battle_source_sfx_cue(enemies,&t,0x20u,0xCBu);break; /* Bindwa */
    case 0x25u: battle_source_sfx_cue(enemies,&t,0x2Au,0xCBu);break; /* Mindblst */
    case 0x26u: case 0x35u: battle_source_sfx_cue(enemies,&t,0x2Eu,0xDCu);break; /* Barrier */
    case 0x27u: battle_source_sfx_cue(enemies,&t,0x1Fu,0xD4u);break; /* WarCry: initial $1E timer expires -> WarCry */
    case 0x28u: battle_source_sfx_cue(enemies,&t,0x1Eu,0xBCu);battle_source_sfx_cue(enemies,&t,0x37u,0xDCu);break; /* Blessing */
    case 0x29u: battle_source_sfx_cue(enemies,&t,0x10u,0xCDu);battle_source_sfx_cue(enemies,&t,0x1Eu,0xBEu);battle_source_sfx_cue(enemies,&t,0x37u,0xDCu);break; /* Warla */
    case 0x2Bu: battle_source_sfx_cue(enemies,&t,0x1Eu,0xBDu);battle_source_sfx_cue(enemies,&t,0x46u,0xCCu);break; /* Medice */
    case 0x2Cu: battle_source_sfx_cue(enemies,&t,0x46u,0xCCu);break; /* Miracle */
    case 0x2Du: battle_source_sfx_cue(enemies,&t,0x2Eu,0xCDu);battle_source_sfx_cue(enemies,&t,0x52u,0xC4u);break; /* Medic Power */
    case 0x2Eu: battle_source_sfx_cue(enemies,&t,0x46u,0xC4u);battle_source_sfx_cue(enemies,&t,0x4Eu,0xC4u);battle_source_sfx_cue(enemies,&t,0x56u,0xC4u);break; /* Ataraxia */
    case 0x2Fu: battle_source_sfx_cue(enemies,&t,0x1Eu,0xC4u);break; /* Vision */
    default: break;
    }
}
/* v0.7 source-skill sound scheduler.  Unlike battle_skill_source_audio(),
 * this emits only the cues explicitly recovered from the corresponding PSIV
 * BattleObj at the authored update number while the source visual is running.
 * There is no generic cast/impact sound for unlisted skills. */
static void battle_skill_source_audio_tick(uint8_t skill,unsigned frame){
    /* Source BattleObj child sounds.  Values/timers are taken from the
     * original packed $1C sound+delay words or direct Sound_Index writes.
     * Silence for an unlisted frame is deliberate; there is no substitute
     * cast/impact sound. */
    switch(skill){
    case 0x01u: if(frame==0x10u||frame==0x29u)ram8w(0x500Au,0xF5u); break; /* Crosscut */
    case 0x02u: if(frame==0x10u)ram8w(0x500Au,0xF5u); break; /* Rayblade */
    case 0x04u: case 0x31u: if(frame==0x33u)ram8w(0x500Au,0xBEu); break; /* Flaeli */
    case 0x05u: if(frame==0x1Eu)ram8w(0x500Au,0xC5u);else if(frame==0x26u)ram8w(0x500Au,0xBEu); break; /* Flare */
    case 0x07u: if(frame==0x32u)ram8w(0x500Au,0xC5u);else if(frame==0x37u)ram8w(0x500Au,0xBFu); break; /* Astral */
    case 0x08u: case 0x10u: if(frame==0x10u)ram8w(0x500Au,0xF5u); break; /* Airslash/Sweeping */
    case 0x0Au: case 0x32u: if(frame==0x30u)ram8w(0x500Au,0xC6u); break; /* Hewn */
    case 0x0Bu: case 0x33u: if(frame==0x20u)ram8w(0x500Au,0xCEu); break; /* Tandle */
    case 0x0Cu: /* Efess: recovery sound object at $32; Efess strike is emitted at its source child spawn. */
        if(frame==0x32u)ram8w(0x500Au,0xCDu); break;
    case 0x0Du: /* Legeon: eight source AbilityAnim objects, $BF00 + delays $2E..$66. */
        if(frame>=0x2Eu&&frame<=0x66u&&((frame-0x2Eu)&7u)==0u)ram8w(0x500Au,0xBFu); break;
    case 0x0Eu: case 0x11u: if(frame==0x1Fu)ram8w(0x500Au,0xCFu); break; /* Burstroc/Phonon */
    case 0x0Fu: if(frame==0x1Eu)ram8w(0x500Au,0xCDu);else if(frame==0x3Cu)ram8w(0x500Au,0xC5u); break; /* Posibolt */
    case 0x12u: case 0x13u: /* StFire/Corrsion loc_3AAE2: FireBreath + authored child delays. */
        if(frame==0x0Cu||frame==0x16u||frame==0x20u||frame==0x2Au||frame==0x34u||frame==0x3Eu)ram8w(0x500Au,0xC2u); break;
    case 0x14u: /* Explode: root Recovery plus authored C5/BF child cues. */
        if(frame==0x1Fu)ram8w(0x500Au,0xCDu);else if(frame==0x32u)ram8w(0x500Au,0xC5u);else if(frame==0x37u)ram8w(0x500Au,0xBFu); break;
    case 0x15u: case 0x30u: if(frame==0x10u)ram8w(0x500Au,0xD2u); break; /* Eliminat */
    case 0x16u: if(frame==0x20u||frame==0x28u||frame==0x30u)ram8w(0x500Au,0xCBu); break; /* Diem */
    case 0x17u: case 0x34u: if(frame==0x36u||frame==0x3Au||frame==0x4Au||frame==0x52u)ram8w(0x500Au,0xD3u); break; /* Spark */
    case 0x18u: break; /* Death is hit/miss conditional; explicit controller emits B8/F5. */
    case 0x19u: case 0x1Au: if(frame==0x2Au)ram8w(0x500Au,0xCBu); break; /* Holyword/DthSpell */
    case 0x1Bu: if(frame==0x20u)ram8w(0x500Au,0xCBu); break; /* Negatis */
    case 0x1Cu: if(frame==0u)ram8w(0x500Au,0xCAu); break; /* Illusion: BattleObj_LoadIllusionAnim -> Rimit */
    case 0x1Du: if(frame==0x1Eu)ram8w(0x500Au,0xBEu);else if(frame==0x24u)ram8w(0x500Au,0xCAu); break; /* Telele + loc_3BF1C */
    case 0x1Eu: if(frame==0x24u)ram8w(0x500Au,0xCAu); break; /* Shadow: final loc_3BF1C child gets $CA24 */
    case 0x20u: /* Hijammer: visual C1 children, root implant, target Spark children. */
        if(frame==0x1Fu)ram8w(0x500Au,0xCFu);
        else if(frame==0x26u||frame==0x35u||frame==0x44u||frame==0x53u)ram8w(0x500Au,0xC1u);
        else if(frame==0x36u||frame==0x4Eu||frame==0x5Au)ram8w(0x500Au,0xD3u); break;
    case 0x21u: if(frame==0x26u||frame==0x2Eu||frame==0x36u||frame==0x3Eu||frame==0x46u)ram8w(0x500Au,0xC4u); break; /* Moonshad */
    case 0x22u: if(frame==0x0Fu)ram8w(0x500Au,0xB5u); break; /* Crash */
    case 0x23u: if(frame==0x1Eu)ram8w(0x500Au,0xC5u);else if(frame==0x26u)ram8w(0x500Au,0xBEu); break; /* Stasis Beam */
    case 0x24u: if(frame==0x1Eu)ram8w(0x500Au,0xBEu);else if(frame==0x20u)ram8w(0x500Au,0xCBu); break; /* Bindwa */
    case 0x25u: if(frame==0x2Au)ram8w(0x500Au,0xCBu); break; /* Mindblst */
    case 0x26u: case 0x35u: if(frame==0x2Eu)ram8w(0x500Au,0xDCu); break; /* Barrier */
    case 0x27u: if(frame==0x1Fu)ram8w(0x500Au,0xD4u); break; /* WarCry */
    case 0x28u: if(frame==0x1Eu)ram8w(0x500Au,0xBCu);else if(frame==0x37u)ram8w(0x500Au,0xDCu); break; /* Blessing */
    case 0x29u: if(frame==0x10u)ram8w(0x500Au,0xCDu);else if(frame==0x1Eu)ram8w(0x500Au,0xBEu);else if(frame==0x37u)ram8w(0x500Au,0xDCu); break; /* Warla */
    case 0x2Au: case 0x36u: if(frame==0x36u||frame==0x48u||frame==0x5Au)ram8w(0x500Au,0xCCu); break; /* Recover */
    case 0x2Bu: if(frame==0x1Eu)ram8w(0x500Au,0xBDu);else if(frame==0x46u)ram8w(0x500Au,0xCCu); break; /* Medice */
    case 0x2Cu: if(frame==0x46u)ram8w(0x500Au,0xCCu); break; /* Miracle */
    case 0x2Du: if(frame==0x2Eu)ram8w(0x500Au,0xCDu);else if(frame==0x52u)ram8w(0x500Au,0xC4u); break; /* Medic Power */
    case 0x2Eu: if(frame==0x46u||frame==0x4Eu||frame==0x56u)ram8w(0x500Au,0xC4u); break; /* Ataraxia */
    /* Vortex/Earth/Vision use dedicated source handlers; DblSlash/Disrupt use
     * the source weapon path. */
    case 0x03u: case 0x06u: case 0x09u: case 0x1Fu: case 0x2Fu: break;
    default: break;
    }
}

static void battle_skill_source_anim_start_audio(uint8_t skill,uint32_t source_table){
    /* A few direct Sound_Index writes are tied to creation of a particular
     * source child rather than a fixed global timer.  Bind those cues to that
     * exact source mapping spawn instead of guessing an absolute frame. */
    if(skill==0x0Cu&&source_table==0x25B2B4u)ram8w(0x500Au,0xC3u); /* Efess */
}

static unsigned battle_skill_source_audio_end(uint8_t skill){
    /* One past the last source-proved global cue frame.  Cues tied directly to
     * an animation spawn are handled by battle_skill_source_anim_start_audio. */
    switch(skill){
    case 0x01u:return 0x2Au; case 0x02u:return 0x11u;
    case 0x04u:case 0x31u:return 0x34u; case 0x05u:return 0x27u; case 0x07u:return 0x38u;
    case 0x08u:case 0x10u:return 0x11u; case 0x0Au:case 0x32u:return 0x31u; case 0x0Bu:case 0x33u:return 0x21u;
    case 0x0Cu:return 0x33u; case 0x0Du:return 0x67u; case 0x0Eu:case 0x11u:return 0x20u; case 0x0Fu:return 0x3Du;
    case 0x12u:case 0x13u:return 0x3Fu; case 0x14u:return 0x38u; case 0x15u:case 0x30u:return 0x11u;
    case 0x16u:return 0x31u; case 0x17u:case 0x34u:return 0x53u; case 0x19u:case 0x1Au:return 0x2Bu; case 0x1Bu:return 0x21u;
    case 0x1Cu:return 1u; case 0x1Du:return 0x25u; case 0x1Eu:return 0x25u;
    case 0x20u:return 0x5Bu; case 0x21u:return 0x47u;
    case 0x22u:return 0x10u; case 0x23u:return 0x27u; case 0x24u:return 0x21u;
    case 0x25u:return 0x2Bu; case 0x26u:case 0x35u:return 0x2Fu; case 0x27u:return 0x20u;
    case 0x28u:case 0x29u:return 0x38u; case 0x2Au:case 0x36u:return 0x5Bu;
    case 0x2Bu:case 0x2Cu:return 0x47u; case 0x2Du:return 0x53u; case 0x2Eu:return 0x57u;
    default:return 0u;
    }
}

static unsigned battle_weapon_factor(const struct BattleLiteActor* actor,const struct BattleLiteActor* target){
    if(actor->id>=11u)return battle_element_factor(target,1u);
    /* Character_DamageEnemy/Fighter_GetElementProp: consider equipped weapons in
     * both hands, ignore shields (type 5), and use the stronger target property. */
    uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)actor->id*0x80u);uint8_t items[2]={ram8r((uint16_t)(d+0x4Cu)),ram8r((uint16_t)(d+0x4Du))};unsigned best=0u;
    for(unsigned h=0u;h<2u;h++){uint8_t item=items[h];if(!item||battle_item_type(item)==5u)continue;unsigned element=battle_item_element(item);unsigned factor=battle_element_factor(target,element);if(factor>best)best=factor;}
    return best;
}
static uint8_t battle_slasher_hit_mask,battle_slasher_target_mask;
static int battle_slasher_hit_target(unsigned count,int x){
    /* Source Slasher_HitPointXPos_{1..4}Targets, in fighter-slot order. */
    static const uint16_t hit_x[4][4]={{0x120u,0u,0u,0u},{0xF0u,0x150u,0u,0u},{0xD0u,0x120u,0x180u,0u},{0xC0u,0x100u,0x140u,0x180u}};
    if(count<1u||count>4u)return -1;
    for(unsigned i=0u;i<count;i++)if(x==(int)hit_x[count-1u][i])return (int)i;
    return -1;
}
static void battle_execute_cmd(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* target){
    if(actor>=pn||!party[actor].hp)return;struct BattleLiteActor* a=&party[actor];
    if(c->type==5u){
        /* PSIV Character_Defend -> loc_4A74: Defend does not apply a generic
         * half-damage flag.  When this actor's turn actually executes, the
         * source writes physical_prop ($30) = 1.  Enemy physical damage reads
         * that property as its element factor; elemental properties are left
         * untouched.  Battle_RestoreStatsAtTurnEnd restores the saved value. */
        a->element_prop[0]=1u;
        a->defending=1u; /* presentation/state marker only; not damage math */
        return;
    }
    if(c->type==1u){
        if(a->id>=11u)ram8w(0x500Au,a->id==13u?0xFAu:0xF9u);
        unsigned slasher=battle_actor_slasher_mask(a);
        if(slasher){battle_slasher_hit_mask=0u;battle_slasher_target_mask=0u;for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp){battle_slasher_target_mask|=(uint8_t)(1u<<i);int critical=0;if(!battle_physical_hit(a,&enemies[i],1,&critical))continue;battle_slasher_hit_mask|=(uint8_t)(1u<<i);unsigned factor=battle_weapon_factor(a,&enemies[i]);unsigned dmg=battle_source_damage(a->atk,enemies[i].def,critical?((a->atk&0xFFu)>>2):0u,factor);battle_mark_enemy_reaction(i,1u);battle_enemy_set_hp(&enemies[i],dmg>=enemies[i].hp?0u:enemies[i].hp-dmg);}return;}
        while(*target<battle_enemy_count&&!enemies[*target].hp)*target=battle_next_target(enemies,*target,1);if(*target>=battle_enemy_count)return;
        int critical=0;if(!battle_physical_hit(a,&enemies[*target],0,&critical))return;unsigned factor=battle_weapon_factor(a,&enemies[*target]);unsigned dmg=battle_source_damage(a->atk,enemies[*target].def,critical?((a->atk&0xFFu)>>2):0u,factor);battle_mark_enemy_reaction(*target,1u);battle_enemy_set_hp(&enemies[*target],dmg>=enemies[*target].hp?0u:enemies[*target].hp-dmg);return;
    }
    uint32_t p=0u;unsigned effect=0u,power=0u,target_kind=1u,element=1u,resist=0u,actor_power=a->mental;uint8_t reaction=2u;
    if(c->type==6u){
        if(a->id<11u||!c->id||c->id>8u)return;uint16_t uses=(uint16_t)(0x4700u+0x6Au+(unsigned)(c->id-1u)*2u);uint8_t u=ram8r(uses);if(!u)return;ram8w(uses,(uint8_t)(u-1u));
        p=R28_VEHICLE_SKILLS+(uint32_t)(c->id-1u)*8u;effect=rom8(p);target_kind=rom8(p+2u)&15u;power=rom8(p+3u);resist=rom8(p+4u);element=rom8(p+5u);actor_power=battle_actor_stat(a,rom8(p+1u));ram8w(0x500Au,c->id==5u?0xC4u:0xCAu);
    }else if(c->type==2u){
        p=PS4_TECH_DATA+(uint32_t)(c->id-1u)*8u;if(!battle_action_has_resource(party,pn,actor,c))return;battle_consume_action_resource(party,actor,c);effect=rom8(p);target_kind=rom8(p+2u)&15u;power=rom8(p+3u);resist=rom8(p+4u);element=rom8(p+5u);actor_power=a->mental;reaction|=4u;
    }else if(c->type==3u){
        p=PS4_SKILL_DATA+(uint32_t)(c->id-1u)*8u;if(!battle_action_has_resource(party,pn,actor,c))return;battle_consume_action_resource(party,actor,c);if(!battle_suppress_skill_source_audio)battle_skill_source_audio(c->id,enemies);effect=rom8(p);target_kind=rom8(p+2u)&15u;power=rom8(p+3u);resist=rom8(p+4u);element=rom8(p+5u);actor_power=battle_actor_stat(a,rom8(p+1u)&0x7Fu);
    }else if(c->type==4u){
        p=ps4_item_ptr(c->id);effect=rom8(p);target_kind=rom8(p+2u)&15u;power=rom8(p+1u);resist=rom8(p+4u);element=rom8(p+5u);actor_power=power;
        battle_item_source_audio(c->id,enemies);
        if(c->inv_slot<40u&&ram8r((uint16_t)(PS4_INV_BASE+c->inv_slot))==c->id)ps4_inventory_remove(c->inv_slot);else if(c->inv_slot==0xFFu)for(unsigned ii=0u;ii<40u;ii++)if(ram8r((uint16_t)(PS4_INV_BASE+ii))==c->id){ps4_inventory_remove(ii);break;}
    }else return;
    if(!element)element=1u;
    if(target_kind>=3u){battle_apply_friendly(party,pn,actor,c->target,effect,power,target_kind);return;}
    if(target_kind==2u)reaction|=0x10u;
    unsigned lo=target_kind==2u?0u:*target,hi=target_kind==2u?battle_enemy_count:*target+1u;
    for(unsigned i=lo;i<hi&&i<battle_enemy_count;i++)if(enemies[i].hp){
        battle_mark_enemy_reaction(i,reaction);
        unsigned factor=element>=0x10u?battle_weapon_factor(a,&enemies[i]):battle_element_factor(&enemies[i],element);unsigned target_power=battle_actor_stat(&enemies[i],resist);
        if(effect==1u){unsigned dmg=battle_source_damage(actor_power,target_power,power,factor);battle_enemy_set_hp(&enemies[i],dmg>=enemies[i].hp?0u:enemies[i].hp-dmg);}
        else if(!resist||battle_calc_chance(actor_power,target_power,factor,power,power)>=0)battle_apply_enemy_effect(&enemies[i],effect,power);
    }
}
static int battle_macro_special_kind(uint8_t command,uint8_t id){
    /* MacroSpecialActionTable at $005838: command,id,word target-policy. */
    for(uint32_t p=0x5838u;p+3u<0x6000u;p+=4u){if(rom16(p)==0xFFFFu)break;if(rom8(p)==command&&rom8(p+1u)==id)return (int)rom16(p+2u);}return -1;
}
static unsigned battle_macro_largest_hp_diff(const struct BattleLiteActor party[5],unsigned pn,int android_mode){
    unsigned best=0u,bestdiff=0u;int found=0;for(unsigned i=0;i<pn;i++){uint16_t d=battle_char_record((unsigned)party[i].id);int android=ram16r((uint16_t)(d+6u))==5u;uint8_t st=party[i].status;
        if(android_mode==0&&android)continue;if(android_mode==2&&!android)continue;if((st&4u)||(android&&(st&0x40u)))continue;unsigned diff=party[i].max_hp-party[i].hp;if(!found||diff>=bestdiff){best=i;bestdiff=diff;found=1;}}
    return found?best:0u;
}
static unsigned battle_macro_special_target(const struct BattleLiteActor party[5],unsigned pn,unsigned actor,int kind){
    if(!pn)return 0u;if(actor>=pn)actor=0u;
    if(kind==0)return battle_macro_largest_hp_diff(party,pn,0); /* Res/Gires/Nares + mates: humans */
    if(kind==1)return 0u; /* Shift source routine's default Character 1 behavior */
    if(kind==2){for(unsigned i=0;i<pn;i++)if(!(party[i].status&4u)&&(party[i].status&1u))return i;return actor;}
    if(kind==3){for(unsigned i=0;i<pn;i++)if(!(party[i].status&4u)&&(party[i].status&2u))return i;return actor;}
    if(kind==4){for(unsigned i=0;i<pn;i++)if(party[i].status&4u)return i;for(unsigned i=0;i<pn;i++)if(!(party[i].status&4u)&&(party[i].status&2u))return i;return battle_macro_largest_hp_diff(party,pn,0);}
    if(kind==5)return battle_macro_largest_hp_diff(party,pn,1); /* Medice: humans + androids */
    if(kind==6)return battle_macro_largest_hp_diff(party,pn,2); /* Repair Kit: androids */
    return actor;
}
static int battle_load_macro(unsigned macro,struct BattleLiteActor party[5],unsigned pn,struct BattleCommandLite cmds[5]){
    if(macro>=8u)return 0;uint16_t b=(uint16_t)(PS4_MACROS+macro*20u);
    for(unsigned i=0;i<pn;i++){cmds[i].type=5u;cmds[i].id=0;cmds[i].target=(uint8_t)i;cmds[i].inv_slot=0xFFu;for(unsigned s=0;s<5u;s++){uint16_t q=(uint16_t)(b+s*4u);if(ram8r((uint16_t)(q+1u))&&ram8r(q)==party[i].id){uint8_t t=ram8r((uint16_t)(q+1u));if(t>=1u&&t<=5u){cmds[i].type=t;cmds[i].id=ram8r((uint16_t)(q+2u));int k=battle_macro_special_kind(t,cmds[i].id);if(k>=0)cmds[i].target=(uint8_t)battle_macro_special_target(party,pn,i,k);}break;}}}
    return 1;
}
static void battle_draw_macro_action(unsigned x,unsigned y,uint8_t type,uint8_t id,uint8_t color){
    if(type==1u)battle_draw_text6(x,y,"ATTACK",color);
    else if(type==2u&&id)battle_draw_rom_name6(x,y,PS4_TECH_NAMES,id-1u,0xFEu,color);
    else if(type==3u&&id)battle_draw_rom_name6(x,y,PS4_SKILL_NAMES,id-1u,0xFEu,color);
    else if(type==4u&&id)battle_draw_rom_name6(x,y,PS4_ITEM_NAMES,id-1u,0xFEu,color);
    else if(type==5u)battle_draw_text6(x,y,"DEFEND",color);
    else battle_draw_text6(x,y,"--",82u);
}
static int battle_choose_macro(const struct BattleLiteActor party[5],unsigned pn){
    unsigned sel=0u;uint8_t prev=0u;battle_ui_wait_buttons_up();battle_ui_begin(battle_ui_target,battle_ui_actor);uint8_t first_present=1u;
    for(;;){
        unsigned lx0,ly0,lx1,ly1,rx0,ry0,rx1,ry1;
        /* Source MACR presentation: a narrow A-H selector on the left and a
         * separate detail window showing each party member's programmed action. */
        battle_ui_box_source(4u,4u,5u,17u,&lx0,&ly0,&lx1,&ly1);
        for(unsigned i=0u;i<8u;i++){unsigned y=ly0+6u+i*10u;char label[2]={(char)('A'+i),0};if(i==sel)battle_draw_cursor6(lx0+5u,y,83u);battle_draw_text6(lx0+12u,y,label,80u);}
        battle_ui_box_source(10u,9u,17u,11u,&rx0,&ry0,&rx1,&ry1);
        uint16_t b=(uint16_t)(PS4_MACROS+sel*20u);unsigned row=0u;
        for(unsigned pi=0u;pi<pn&&pi<5u;pi++){uint8_t type=0u,id=0u;for(unsigned m=0u;m<5u;m++){uint16_t q=(uint16_t)(b+m*4u);if(ram8r(q)==party[pi].id){type=ram8r((uint16_t)(q+1u));id=ram8r((uint16_t)(q+2u));break;}}
            unsigned y=ry0+7u+row*11u;battle_draw_ram_name6(rx0+6u,y,party[pi].id,80u);battle_draw_text6(rx0+45u,y,":",82u);battle_draw_macro_action(rx0+51u,y,type,id,80u);++row;
        }
        unsigned ux0=lx0<rx0?lx0:rx0,uy0=ly0<ry0?ly0:ry0,ux1=lx1>rx1?lx1:rx1,uy1=ly1>ry1?ly1:ry1;battle_ui_overlay_set(ux0,uy0,ux1,uy1);battle_ui_present_menu(&first_present);
        uint8_t e=battle_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:7u;else if(e&2u)sel=sel==7u?0u:sel+1u;else if(e&0x60u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return (int)sel;}else if(e&0x90u){battle_ui_wait_buttons_up();battle_ui_overlay_clear();return -1;}
        battle_ui_begin(battle_ui_target,battle_ui_actor);
    }
}

#define PS4_COMBO_DATA 0x285424u
#define PS4_COMBO_SETUP 0x28549Cu
#define PS4_COMBO_NAMES 0x28554Cu
#define PS4_COMBO_COUNT 14u
struct BattleTurnLite { uint8_t enemy,index; uint16_t initiative; };

static unsigned battle_mod_u16(uint16_t value,unsigned divisor){
    if(!divisor)return 0u;unsigned rem=0u;for(int bit=15;bit>=0;--bit){rem=(rem<<1)|((value>>(unsigned)bit)&1u);if(rem>=divisor)rem-=divisor;}return rem;
}
static int battle_party_index_by_id(const struct BattleLiteActor party[5],unsigned pn,unsigned id){for(unsigned i=0u;i<pn;i++)if(party[i].id==id)return (int)i;return -1;}
static unsigned battle_build_turn_order(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],int macro_index,struct BattleTurnLite turns[9]){
    unsigned n=0u;for(unsigned i=0u;i<pn;i++)if(!battle_actor_incapacitated(&party[i])){turns[n].enemy=0u;turns[n].index=(uint8_t)i;turns[n].initiative=party[i].agility;++n;}
    for(unsigned i=0u;i<battle_enemy_count;i++)if(!battle_actor_incapacitated(&enemies[i])){turns[n].enemy=1u;turns[n].index=(uint8_t)i;turns[n].initiative=enemies[i].agility;++n;}
    unsigned high=0u;for(unsigned i=0u;i<n;i++)if(turns[i].initiative>high)high=turns[i].initiative;unsigned span=high>>1;
    if(span)for(unsigned i=0u;i<n;i++)if(turns[i].initiative)turns[i].initiative=(uint16_t)(turns[i].initiative+battle_mod_u16(field_npc_rng_next(),span));
    /* Stable descending order, matching Battle_OrderTurns' bubble-sort tie behavior. */
    for(unsigned i=1u;i<n;i++){struct BattleTurnLite moving=turns[i];unsigned j=i;while(j&&turns[j-1u].initiative<moving.initiative){turns[j]=turns[j-1u];--j;}turns[j]=moving;}
    /* Macro_Data's programmed character sequence constrains only party-relative
     * order; enemies retain their initiative positions. */
    if(macro_index>=0&&macro_index<8){uint16_t b=(uint16_t)(PS4_MACROS+(unsigned)macro_index*20u);int previous=-1;
        for(unsigned slot=0u;slot<5u;slot++){uint16_t q=(uint16_t)(b+slot*4u);if(!ram8r((uint16_t)(q+1u)))break;int cur=battle_party_index_by_id(party,pn,ram8r(q));if(cur<0||battle_actor_incapacitated(&party[cur]))continue;
            if(previous>=0){int pp=-1,cp=-1;for(unsigned k=0u;k<n;k++)if(!turns[k].enemy){if(turns[k].index==(unsigned)previous)pp=(int)k;if(turns[k].index==(unsigned)cur)cp=(int)k;}
                if(pp>=0&&cp>=0&&pp>cp){struct BattleTurnLite moving=turns[cp];for(int k=cp;k<pp;k++)turns[k]=turns[k+1];turns[pp]=moving;}}
            previous=cur;
        }
    }
    return n;
}
static uint8_t battle_action_signature(const struct BattleCommandLite* c){if(c->type==2u)return c->id;if(c->type==3u)return (uint8_t)(c->id|0x80u);return 0u;}
static int battle_combo_option_match(uint32_t p,unsigned participants,int ordered,const uint8_t sig[4]){
    if(ordered){for(unsigned i=0u;i<participants;i++)if(rom8(p+i)!=sig[i])return 0;return 1;}
    uint8_t used=0u;for(unsigned i=0u;i<participants;i++){int found=0;for(unsigned j=0u;j<participants;j++)if(!(used&(1u<<j))&&sig[i]==rom8(p+j)){used|=(uint8_t)(1u<<j);found=1;break;}if(!found)return 0;}return 1;
}
static int battle_combo_find_at(const struct BattleTurnLite turns[9],unsigned turn_count,unsigned pos,const struct BattleCommandLite cmds[5],const struct BattleLiteActor party[5],unsigned pn,unsigned* combo_out,unsigned participants_out[4]){
    uint32_t p=PS4_COMBO_SETUP;for(unsigned combo=1u;combo<=PS4_COMBO_COUNT;combo++){
        uint8_t meta=rom8(p++);unsigned participants=meta&0x7Fu;int ordered=(meta&0x80u)!=0u;uint32_t options=p;
        while(rom8(p)!=0xFFu)p+=participants;uint32_t end=p++;
        if(participants<2u||participants>4u||pos+participants>turn_count)continue;
        uint8_t sig[4]={0,0,0,0};int valid=1;for(unsigned k=0u;k<participants;k++){const struct BattleTurnLite* t=&turns[pos+k];if(t->enemy||t->index>=pn){valid=0;break;}sig[k]=battle_action_signature(&cmds[t->index]);if(!sig[k]||!battle_action_has_resource(party,pn,t->index,&cmds[t->index])){valid=0;break;}participants_out[k]=t->index;}if(!valid)continue;
        for(uint32_t o=options;o<end;o+=participants)if(battle_combo_option_match(o,participants,ordered,sig)){*combo_out=combo;return (int)participants;}
    }return 0;
}
static void battle_draw_combo_name(unsigned combo,unsigned x,unsigned y){if(combo>=1u&&combo<=PS4_COMBO_COUNT)battle_draw_rom_name6(x,y,PS4_COMBO_NAMES,combo-1u,0xFFu,83u);}
static void battle_draw_action_banner(const struct BattleLiteActor party[5],const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],int actor_enemy,unsigned actor,const struct BattleCommandLite* c,unsigned combo){
    unsigned x0,y0,x1,y1;
    /* Compact source-window action banner. Everything in the banner uses the
     * same 3/4 x,5/7 y projection as the battle command plane. */
    if(combo){battle_ui_box_source(0u,0u,28u,3u,&x0,&y0,&x1,&y1);battle_draw_combo_name(combo,x0+8u,y0+6u);}
    else if(actor_enemy){battle_ui_box_source(0u,0u,28u,3u,&x0,&y0,&x1,&y1);if(c&&c->id)battle_draw_enemy_skill_name6(x0+8u,y0+6u,c->id,80u);else{battle_draw_enemy_name6(x0+8u,y0+6u,enemies[actor].id,83u);battle_draw_text6(x0+98u,y0+6u,"ATTACK",80u);}}
    else{battle_ui_box_source(0u,0u,33u,3u,&x0,&y0,&x1,&y1);battle_draw_ram_name6(x0+6u,y0+6u,party[actor].id,83u);if(c){if(c->type==1u)battle_draw_text6(x0+60u,y0+6u,"ATTACK",80u);else if(c->type==2u)battle_draw_rom_name6(x0+60u,y0+6u,PS4_TECH_NAMES,c->id-1u,0xFEu,80u);else if(c->type==3u)battle_draw_rom_name6(x0+60u,y0+6u,PS4_SKILL_NAMES,c->id-1u,0xFEu,80u);else if(c->type==4u)battle_draw_rom_name6(x0+60u,y0+6u,PS4_ITEM_NAMES,c->id-1u,0xFEu,80u);else battle_draw_text6(x0+60u,y0+6u,"DEFEND",80u);}}
    /* All bosses share this protected action window, including bodies whose
     * idle maps extend into the upper text band (Zio / Dark Force). */
    battle_ui_overlay_set(x0,y0,x1,y1);
}
static void battle_present_action(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,int actor_enemy,unsigned actor,const struct BattleCommandLite* c,unsigned combo){
    /* Source-feedback regression anchor retained after factoring the banner:
     * actor_enemy){battle_ui_box_source(0u,0u,28u,3u is implemented verbatim
     * in battle_draw_action_banner() above. */
    unsigned draw_target=enemy_target<battle_enemy_count?enemy_target:0u;battle_compose_igg(party,pn,enemies,draw_target,actor_enemy?5u:actor);
    battle_draw_action_banner(party,enemies,actor_enemy,actor,c,combo);
    battle_present_page0();battle_obj_sync(enemies,0u);
}
/* PSCSTYLE v0.5 battle-speed path.  At the start of an ordinary action the
 * battle scene is already current from the preceding canonical present.  The
 * old path nevertheless rebuilt all 38,400 pixels, every enemy, every party
 * body and both palette banks just to replace the top action banner and the
 * highlighted party HUD record.  Restore only the bands that can be stale,
 * redraw their exact source-backed layers, then publish all bands in one
 * VBlank.  Callers use this only before state mutation and only when no
 * dedicated party-pose renderer owns the body band. */
__attribute__((noinline))
static void battle_present_action_pre_fast(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],int actor_enemy,unsigned actor,const struct BattleCommandLite* c,unsigned combo){
    const unsigned top_y1=battle_src_y(4u),hud_y0=battle_src_y(22u);
#if PS4_BATTLE_SOURCE_DAMAGE_WINDOW
    const unsigned damage_y0=battle_src_y(5u),damage_y1=battle_src_y(18u)+BATTLE_DAMAGE_GLYPH_H;
#else
    const unsigned damage_y0=72u,damage_y1=107u;
#endif
    unsigned bg=battle_background_source_index();
    const uint8_t* bgp=ps4_precomp_assets+PS4_PRECOMP_BATTLE_BG_PIXELS_OFF+bg*PS4_PRECOMP_BATTLE_BG_FRAME_SIZE;
    ps4_audio_prepare_frame();
    /* Top action-window band contains background only during resolution. */
    dma_copy32(bgp,field_pause_fb,top_y1*240u);
    battle_draw_action_banner(party,enemies,actor_enemy,actor,c,combo);
    /* A just-expired damage overlay dirties the active window rows.  Rebuild that
     * strip from the authored background and replay the intersecting dynamic
     * layers.  Full enemy/body painters are safe here: pixels outside the
     * strip are rewritten with the same current frame. */
    unsigned restore_damage=battle_damage_pixels_stale;
    if(restore_damage){
        unsigned off=damage_y0*240u,bytes=(damage_y1-damage_y0)*240u;
        dma_copy32(bgp+off,field_pause_fb+off,bytes);battle_compose_clip_set(0,damage_y0,240,damage_y1);
        if(battle_event_index!=0u)for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp){int ex,ey;if(!battle_enemy_native_origin(i,enemies[i].id,&ex,&ey))continue;unsigned line=12u+((battle_enemy_placement[i]>>7)&1u);battle_draw_enemy(i,enemies[i].id,ex,ey,line);}
        for(unsigned i=0u;i<pn&&i<5u;i++)battle_draw_party_frame(i,party[i].id);battle_compose_clip_clear();
        battle_damage_pixels_stale=0u;
    }
    /* The lower source FillBattleStats window is opaque, so redrawing it
     * exactly erases the prior actor highlight while publishing current HP/TP. */
    battle_draw_party_hud(party,pn,actor_enemy?5u:actor);
    waitvb();
    dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,top_y1*240u);
    if(restore_damage){unsigned off=damage_y0*240u;dma_copy32(field_pause_fb+off,(void*)((uintptr_t)MODE4_VRAM16+off),(damage_y1-damage_y0)*240u);}
    {unsigned off=hud_y0*240u;dma_copy32(field_pause_fb+off,(void*)((uintptr_t)MODE4_VRAM16+off),(160u-hud_y0)*240u);}
    battle_obj_sync(enemies,0u);
}
static void battle_action_idle_after_wait(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    /* Source BattleObj loops have already consumed this tick's VBlank.  Advance
     * simulation exactly once, but queue generic bitmap dirtiness for staging
     * before the next wait; that next wait publishes body + effect state in one
     * VBlank instead of adding a second display tick. */
    if(battle_event_index==0u){unsigned ch=battle_anim_advance();if(ch)battle_obj_sync(enemies,ch);}
    else {unsigned ch=battle_enemy_anim_advance_generic(enemies);battle_generic_pending_mask|=ch;}
}
__attribute__((noinline))
static unsigned battle_action_hold_track_fb(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned frames){
    unsigned framebuffer_changed=0u;
    for(unsigned f=0u;f<frames;f++){
        if(battle_event_index==0u){waitvb();unsigned ch=battle_anim_advance();if(ch)battle_obj_sync(enemies,ch);}
        else{unsigned ch=battle_enemy_anim_advance_generic(enemies);if(ch){framebuffer_changed|=ch;battle_ui_redraw_generic_anim(ch);}else waitvb();}
    }
    return framebuffer_changed;
}
static void battle_action_hold(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned frames){
    /* Regression anchor after extracting the invalidation-returning helper:
     * battle_enemy_anim_advance_generic(enemies) and
     * battle_ui_redraw_generic_anim(ch) still execute inside track_fb above. */
    (void)battle_action_hold_track_fb(enemies,frames);
}
/* Enemy_Dead special family ($82-$87).  The original death objects load
 * BattleObj $8E8 as a periodic Sound_Index object with period four and SFX $DD.
 * The non-PD3 family removes that object after the source death sequence; PD3
 * additionally issues Sound_StopAll before loading it and, in the English
 * revision, clears $8E8 before issuing AnotherGate.  These frame bounds were
 * derived from the source death-object counters, not from the visible GBA art. */
static void battle_flush_special_death_audio(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    uint8_t id=battle_pending_special_death;if(id==0xFFu)return;battle_pending_special_death=0xFFu;unsigned t=0u;
    /* Battle_RunObjects scans the 64 slots upward from $D800.  Enemy_Dead puts
     * the death controller in $DA00; that controller then creates periodic
     * sound object $8E8 in $D800.  Because $D800 has already been visited in
     * that pass, $8E8's first Sound_Index write occurs on the NEXT update.
     * With period four the source pulse positions are therefore 1,5,9,... . */
    if(id>=0x82u&&id<=0x86u){
        /* $81C/$840 clear $8E8 on death-controller update 462.  Since $D800
         * runs before $DA00, the last pulse on update 462 (global frame 461)
         * is observed before the controller clears the object. */
        for(unsigned f=1u;f<=461u;f+=4u)battle_source_sfx_cue(enemies,&t,f,0xDDu);
        if(t<462u)battle_action_hold(enemies,462u-t);return;
    }
    if(id==0x87u){
        /* PD3 writes StopAll in the death controller's first pass.  Its $8E8
         * child begins one update later.  On global frame 769 the $D800 child
         * is visited first, but loc_2DBC6 subsequently clears it and overwrites
         * Sound_Index with AnotherGate; model the effective source command as
         * the final GraveOpening pulse at 765 followed by AnotherGate at 769. */
        ram8w(0x500Au,0xFEu);
        for(unsigned f=1u;f<=765u;f+=4u)battle_source_sfx_cue(enemies,&t,f,0xDDu);
        battle_source_sfx_cue(enemies,&t,769u,0xEDu);return;
    }
}
/* R17: restore only the two 12-pixel window strips. Replay underlying layers
 * before drawing the next source phase; each phase owns exactly one VBlank. */
static void battle_damage_present_phase(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned columns){
#if PS4_BATTLE_SOURCE_DAMAGE_WINDOW
    battle_damage_columns=(uint8_t)columns;
    unsigned rows[2]={battle_src_y(5u),battle_src_y(18u)};
    const uint8_t* bg=ps4_precomp_assets+PS4_PRECOMP_BATTLE_BG_PIXELS_OFF+battle_background_source_index()*PS4_PRECOMP_BATTLE_BG_FRAME_SIZE;
    for(unsigned strip=0u;strip<2u;strip++){
        unsigned y0=rows[strip],y1=y0+BATTLE_DAMAGE_GLYPH_H;
        dma_copy32(bg+y0*240u,field_pause_fb+y0*240u,(y1-y0)*240u);
        battle_compose_clip_set(0,y0,240,y1);
        if(battle_ui_party)for(unsigned i=0u;i<battle_ui_party_count&&i<5u;i++)battle_draw_party_frame(i,battle_ui_party[i].id);
        if(battle_event_index!=0u)for(unsigned i=0u;i<battle_enemy_count;i++)if(battle_enemy_draw_visible(enemies,i)){
            int x,y;if(battle_enemy_native_origin(i,enemies[i].id,&x,&y))battle_draw_enemy(i,enemies[i].id,x,y,12u+((battle_enemy_placement[i]>>7)&1u));
        }
        battle_compose_clip_clear();
    }
    battle_draw_damage_overlays();waitvb();
    for(unsigned strip=0u;strip<2u;strip++){unsigned off=rows[strip]*240u;dma_copy32(field_pause_fb+off,(void*)((uintptr_t)MODE4_VRAM16+off),BATTLE_DAMAGE_GLYPH_H*240u);}
    battle_action_idle_after_wait(enemies);
#else
    (void)enemies;(void)columns;
#endif
}
/* Resolve math once, publish pre-hit HP while numbers are visible, and commit
 * the result/death only after the close. Status/resource changes are retained.
 * Called at the action boundary, never between two damage calculations. */
static void battle_finish_damage(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],
        const uint16_t party_before[5],const uint16_t enemy_before[BATTLE_MAX_ENEMIES],unsigned target,
        unsigned actor_enemy,unsigned actor,const struct BattleCommandLite* cmd,unsigned combo,unsigned empty_hold){
    uint16_t party_after[5],enemy_after[BATTLE_MAX_ENEMIES];
    battle_snapshot_hp(party,pn,enemies,party_after,enemy_after);
    battle_damage_overlay_capture(party_before,pn,enemy_before,party,enemies);
    unsigned any=battle_damage_enemy_mask||battle_damage_party_mask;
#if PS4_BATTLE_SOURCE_DAMAGE_WINDOW
    if(any){
        for(unsigned i=0u;i<pn&&i<5u;i++)if(battle_damage_party_mask&(1u<<i))party[i].hp=party_before[i];
        for(unsigned i=0u;i<battle_enemy_count;i++)if(battle_damage_enemy_mask&(1u<<i))enemies[i].hp=enemy_before[i];
        battle_damage_columns=1u;
    }
#endif
    battle_present_action(party,pn,enemies,target,actor_enemy,actor,cmd,combo);
#if PS4_BATTLE_SOURCE_DAMAGE_WINDOW
    if(any){battle_damage_present_phase(enemies,3u);battle_damage_present_phase(enemies,5u);}
#endif
    battle_action_hold(enemies,any?battle_damage_hold_frames():empty_hold);
#if PS4_BATTLE_SOURCE_DAMAGE_WINDOW
    if(any){battle_damage_present_phase(enemies,3u);battle_damage_present_phase(enemies,1u);battle_damage_present_phase(enemies,0u);}
#endif
    battle_damage_overlay_clear();battle_damage_columns=5u;battle_defer_death_audio=0u;
    for(unsigned i=0u;i<pn&&i<5u;i++)party[i].hp=party_after[i];
    for(unsigned i=0u;i<battle_enemy_count;i++){
        /* Re-arm the real transition even in A/B mode and for instant death. */
        if(enemy_before[i]&&!enemy_after[i])enemies[i].hp=enemy_before[i];
        battle_enemy_set_hp(&enemies[i],enemy_after[i]);
    }
    if(any){
        battle_draw_party_hud(party,pn,actor_enemy?5u:actor);
        unsigned y=battle_src_y(22u);waitvb();dma_copy32(field_pause_fb+y*240u,(void*)((uintptr_t)MODE4_VRAM16+y*240u),(160u-y)*240u);
        battle_obj_sync(enemies,0u);
    }
    battle_flush_special_death_audio(enemies);
}

static int battle_alys_slasher_source_attack(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* c,unsigned mask);
static int battle_party_vortex_source_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target);
static int battle_party_earth_source_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target);
static int battle_party_vision_source_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target);
static int battle_demi_impacter_source_attack(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* c);
static uint8_t battle_attack_clean_rect[BATTLE_DEATH_FLASH_RECT_BYTES];
/* Reuse the same attacker-underlay cache for source attacks whose body animation
 * is expressed as a small sequence of explicit party poses (Alys/Demi).  The
 * first pose still publishes a canonical full scene + action banner.  Later
 * body-pose changes restore only the 36x34 actor rectangle, unless a generic
 * framebuffer enemy animation invalidated the clean underlay while we waited. */
static int battle_present_actor_pose_dirty(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* c,int* clean_valid,int first_full){
    /* Wren's load mapping is seven source columns. Refresh the whole party
     * band at each of its gun poses so the extra column is also erased when
     * changing back to the six-column fire/idle maps. */
    if(actor<pn&&party[actor].id==7u){
        battle_compose_igg(party,pn,enemies,enemy_target<battle_enemy_count?enemy_target:0u,actor);
        battle_draw_action_banner(party,enemies,0u,actor,c,0u);*clean_valid=0;
        if(first_full)battle_present_page0();else battle_present_band((0x80u*5u)/7u,(0x80u*5u)/7u+BATTLE_PARTY_H);
        battle_obj_sync(enemies,0u);return 1;
    }

    if(actor>=pn)return 0;int left=battle_party_center_x(actor)-(int)(BATTLE_PARTY_W/2u);unsigned sy=(0x80u*5u)/7u;
    if(left<0||left+(int)BATTLE_PARTY_W>240||sy+BATTLE_PARTY_H>160u)return 0;
    uint8_t bit=(uint8_t)(1u<<actor),saved=(uint8_t)(battle_party_visible_mask&bit);
    if(!*clean_valid){
        battle_party_visible_mask&=(uint8_t)~bit;
        battle_compose_igg(party,pn,enemies,enemy_target<battle_enemy_count?enemy_target:0u,actor);
        battle_draw_action_banner(party,enemies,0u,actor,c,0u);
        battle_death_flash_capture(battle_attack_clean_rect,left,sy);
        if(saved)battle_party_visible_mask|=bit;
        *clean_valid=1;
    }else{ps4_audio_prepare_frame();battle_death_flash_restore(battle_attack_clean_rect,left,sy);}
    battle_draw_party_frame(actor,party[actor].id);
    if(first_full)battle_present_page0();else battle_present_band(sy,sy+BATTLE_PARTY_H);
    battle_obj_sync(enemies,0u);return 1;
}
static int battle_source_x_to_gba(unsigned x);
static int battle_source_y_to_gba(unsigned y);
static int battle_party_source_attack(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* c){
    if(actor>=pn)return 0;unsigned sm=battle_actor_slasher_mask(&party[actor]);if((party[actor].id==1u||party[actor].id==9u)&&sm)return battle_alys_slasher_source_attack(party,pn,enemies,enemy_target,actor,c,sm);if(party[actor].id==6u&&battle_actor_has_item(&party[actor],PS4_PRECOMP_DEMI_IMPACTER_ITEM_ID))return battle_demi_impacter_source_attack(party,pn,enemies,enemy_target,actor,c);
    const uint8_t* holds=0;unsigned frames=0u;const uint8_t* probe=battle_party_attack_pixels(party[actor].id,0u,&holds,&frames);if(!probe||!holds||!frames)return 0;
    battle_party_anim_actor=(uint8_t)actor;battle_party_anim_attack=1u;uint8_t attack_sfx=battle_party_weapon_sound(&party[actor]);unsigned elapsed=0u;int sounded=0;
    int left=battle_party_center_x(actor)-(int)(BATTLE_PARTY_W/2u);unsigned sy=(0x80u*5u)/7u;int geom_ok=left>=0&&left+(int)BATTLE_PARTY_W<=240&&sy+BATTLE_PARTY_H<=160u;
    uint8_t actor_bit=(uint8_t)(1u<<actor),saved_visible=(uint8_t)(battle_party_visible_mask&actor_bit);int clean_valid=0;
    for(unsigned f=0u;f<frames;f++){
        battle_party_anim_frame=(uint8_t)f;
        if(!geom_ok){
            battle_present_action(party,pn,enemies,enemy_target,0u,actor,c,0u);
        }else{
            if(!clean_valid){
                /* Build the exact current battle scene with the attacker omitted,
                 * then keep only the 36x34 pixels that can be exposed through
                 * transparent pixels of subsequent attack poses.  This full
                 * canonical compose is repeated only after a framebuffer enemy
                 * animation invalidates the underlying rectangle. */
                battle_party_visible_mask&=(uint8_t)~actor_bit;
                battle_compose_igg(party,pn,enemies,enemy_target<battle_enemy_count?enemy_target:0u,actor);
                battle_draw_action_banner(party,enemies,0u,actor,c,0u);
                battle_death_flash_capture(battle_attack_clean_rect,left,sy);
                if(saved_visible)battle_party_visible_mask|=actor_bit;
                clean_valid=1;
            }else{
                ps4_audio_prepare_frame();
                battle_death_flash_restore(battle_attack_clean_rect,left,sy);
            }
            {const uint8_t* hp=0;unsigned nf=0u;const uint8_t* px=battle_party_attack_pixels(party[actor].id,f,&hp,&nf);(void)hp;(void)nf;battle_blit_party_projected(px,left,sy);}
            if(f==0u){battle_present_page0();battle_obj_sync(enemies,0u);}
            else{battle_present_band(sy,sy+BATTLE_PARTY_H);battle_obj_sync(enemies,0u);}
        }
        unsigned hold=holds[f];if(!sounded&&attack_sfx&&elapsed+hold>=15u){ram8w(0x500Au,attack_sfx);sounded=1;}
        if(hold>1u&&battle_action_hold_track_fb(enemies,hold-1u))clean_valid=0;elapsed+=hold;
    }
    if(!sounded&&attack_sfx)ram8w(0x500Au,attack_sfx);
    if(saved_visible)battle_party_visible_mask|=actor_bit;else battle_party_visible_mask&=(uint8_t)~actor_bit;
    battle_party_anim_attack=0u;battle_party_anim_actor=0xFFu;battle_party_anim_frame=0u;return 1;
}
/* v0.7: generic *source* BattleObj renderer.  This is not a fallback
 * animation.  The generator byte-compares every frame payload against the
 * verified PSIV ROM's PLC+mapping data and carries the original per-frame
 * Battle_AnimateSprite holds.  Runtime only translates those authored 4bpp
 * frames onto GBA OBJ hardware. */
static unsigned battle_source_ability_points(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned current_target,int xs[5],int ys[5]){
    unsigned tk=battle_command_target_kind(c),n=0u;
    /* R20 controller follow-up: these source skill roots create the BattleObj at
     * the caster/fixed source position, not at the eventual damage target. */
    if(c&&c->type==3u){
        if(c->id==0x01u||c->id==0x02u||c->id==0x08u||c->id==0x10u){
            if(actor<pn){xs[0]=battle_party_center_x(actor);ys[0]=battle_source_y_to_gba(0xD8u);return 1u;}return 0u;
        }
        if(c->id==0x1Cu){xs[0]=battle_source_x_to_gba(0x120u);ys[0]=battle_source_y_to_gba(0xD8u);return 1u;}
    }
    if(tk==1u){
        if(current_target<battle_enemy_count&&enemies[current_target].hp){xs[0]=battle_enemy_anchor_x(current_target);ys[0]=battle_source_y_to_gba(0xD8u);return 1u;}
        return 0u;
    }
    if(tk==2u){
        for(unsigned i=0u;i<battle_enemy_count&&n<5u;i++)if(enemies[i].hp){xs[n]=battle_enemy_anchor_x(i);ys[n]=battle_source_y_to_gba(0xD8u);n++;}
        return n;
    }
    if(tk==3u){ /* self-only source range */
        if(actor<pn){xs[0]=battle_party_center_x(actor);ys[0]=battle_source_y_to_gba((c&&c->type==3u&&c->id>=0x26u&&c->id<=0x2Eu)?0xD8u:0x108u);return 1u;}
        return 0u;
    }
    if(tk==4u||tk==6u||tk==8u){
        unsigned t=c->target<pn?c->target:actor;
        if(t<pn&&battle_party_class_valid(&party[t],tk)){xs[0]=battle_party_center_x(t);ys[0]=battle_source_y_to_gba((c&&c->type==3u&&c->id>=0x26u&&c->id<=0x2Eu)?0xD8u:0x108u);return 1u;}
        return 0u;
    }
    if(tk==5u||tk==7u||tk==9u){
        for(unsigned i=0u;i<pn&&i<5u;i++)if(battle_party_class_valid(&party[i],tk)){xs[n]=battle_party_center_x(i);ys[n]=battle_source_y_to_gba((c&&c->type==3u&&c->id>=0x26u&&c->id<=0x2Eu)?0xD8u:0x108u);n++;}
        return n;
    }
    return 0u;
}
static void battle_source_ability_palette(const Ps4SourceAbilityDesc* d){
    dma_copy32(ps4_source_ability_palettes[d->palette_id],(void*)(OBJ_PALETTE+BATTLE_SOURCE_ABILITY_PALBANK*16u),32u);
}
static void battle_source_ability_frame(const Ps4SourceAbilityAnim* a,unsigned frame,const int* xs,const int* ys,unsigned ntarget){
    unsigned oi=BATTLE_SOURCE_ABILITY_OBJ0,tile=BATTLE_ATTACK_OBJ_TILE;
    for(unsigned pi=0u;pi<a->part_count;pi++){
        const Ps4SourceAbilityPart* p=&ps4_source_ability_parts[a->first_part+pi];
        const uint8_t* src=ps4_source_ability_bank+p->off+(uint32_t)frame*p->frame_bytes;
        (void)battle_effect_upload(src,p->frame_bytes,tile);
        for(unsigned t=0u;t<ntarget&&oi<BATTLE_SOURCE_ABILITY_OBJ0+BATTLE_SOURCE_ABILITY_OBJN;t++)
            battle_source_obj_prebound4(oi++,xs[t],ys[t],p->ox,p->oy,p->geom,tile);
        tile+=p->frame_bytes>>5;
    }
    {unsigned now=oi-BATTLE_SOURCE_ABILITY_OBJ0;if(now<battle_source_effect_active_oam)for(unsigned i=now;i<battle_source_effect_active_oam;i++)hw_hide_obj(BATTLE_SOURCE_ABILITY_OBJ0+i);battle_source_effect_active_oam=now;}
}

/* R21 source-controller choreography scheduler.  R20 proved asset coverage but
 * serialized BattleObj children.  These tracks retain the source start delay,
 * independent animation phase, authored anchor and per-target fanout while
 * reserving a fixed OBJ tile range for each concurrently-live child class. */
typedef struct{
    const Ps4SourceAbilityAnim* a;
    uint16_t start,tile;
    uint8_t nt,last_frame;
    int16_t xs[5],ys[5];
} BattleAbilityTrack;
#define BATTLE_ABILITY_TRACK_MAX 10u
/* Battle-only scheduler state reuses the low end of the title-token arena.
 * title_logo_token is dead after Start; 0x02034000 begins the field HBlank
 * scratch, so keep all choreography state strictly below that boundary. */
static BattleAbilityTrack* const battle_ability_tracks=(BattleAbilityTrack*)0x02033D80;
static unsigned battle_ability_track_count,battle_ability_track_next_tile;
static const Ps4SourceAbilityAnim* battle_source_ability_find_anim(uint32_t table){
    for(unsigned i=0u;i<PS4_SOURCE_ABILITY_ANIM_COUNT;i++)if(ps4_source_ability_anims[i].source_table==table)return &ps4_source_ability_anims[i];
    return 0;
}
static unsigned battle_source_ability_anim_tiles(const Ps4SourceAbilityAnim* a){
    unsigned n=0u;if(!a)return 0u;for(unsigned i=0u;i<a->part_count;i++)n+=ps4_source_ability_parts[a->first_part+i].frame_bytes>>5;return n;
}
static unsigned battle_source_ability_anim_duration(const Ps4SourceAbilityAnim* a){
    if(!a)return 0u;const uint8_t* h=ps4_source_ability_bank+a->holds_off;unsigned n=0u;for(unsigned i=0u;i<a->frame_count;i++)n+=h[i]?h[i]:1u;return n;
}
static int battle_source_ability_anim_frame_at(const Ps4SourceAbilityAnim* a,unsigned age){
    if(!a)return -1;const uint8_t* h=ps4_source_ability_bank+a->holds_off;for(unsigned f=0u;f<a->frame_count;f++){unsigned q=h[f]?h[f]:1u;if(age<q)return (int)f;age-=q;}return -1;
}
static void battle_ability_tracks_reset(void){battle_ability_track_count=0u;battle_ability_track_next_tile=BATTLE_ATTACK_OBJ_TILE;}
static int battle_ability_track_add(uint32_t table,unsigned start,const int* xs,const int* ys,unsigned nt){
    if(!nt||nt>5u||battle_ability_track_count>=BATTLE_ABILITY_TRACK_MAX)return 0;
    const Ps4SourceAbilityAnim* a=battle_source_ability_find_anim(table);if(!a)return 0;unsigned tiles=battle_source_ability_anim_tiles(a);
    if(!tiles||battle_ability_track_next_tile+tiles>1024u)return 0;
    BattleAbilityTrack* t=&battle_ability_tracks[battle_ability_track_count++];t->a=a;t->start=(uint16_t)start;t->tile=(uint16_t)battle_ability_track_next_tile;t->nt=(uint8_t)nt;t->last_frame=0xFFu;
    for(unsigned i=0u;i<nt;i++){t->xs[i]=(int16_t)xs[i];t->ys[i]=(int16_t)ys[i];}
    battle_ability_track_next_tile+=tiles;return 1;
}
static unsigned battle_ability_tracks_end(void){
    unsigned end=0u;for(unsigned i=0u;i<battle_ability_track_count;i++){unsigned e=battle_ability_tracks[i].start+battle_source_ability_anim_duration(battle_ability_tracks[i].a);if(e>end)end=e;}return end;
}
static void battle_ability_tracks_draw(unsigned elapsed){
    unsigned oi=BATTLE_SOURCE_ABILITY_OBJ0;
    for(unsigned ti=0u;ti<battle_ability_track_count;ti++){
        BattleAbilityTrack* t=&battle_ability_tracks[ti];if(elapsed<t->start)continue;int f=battle_source_ability_anim_frame_at(t->a,elapsed-t->start);if(f<0)continue;
        unsigned tile=t->tile;
        for(unsigned pi=0u;pi<t->a->part_count;pi++){
            const Ps4SourceAbilityPart* p=&ps4_source_ability_parts[t->a->first_part+pi];
            if(t->last_frame!=(uint8_t)f){const uint8_t* src=ps4_source_ability_bank+p->off+(uint32_t)(unsigned)f*p->frame_bytes;(void)battle_effect_upload(src,p->frame_bytes,tile);}
            for(unsigned q=0u;q<t->nt&&oi<BATTLE_SOURCE_ABILITY_OBJ0+BATTLE_SOURCE_ABILITY_OBJN;q++)battle_source_obj_prebound4(oi++,t->xs[q],t->ys[q],p->ox,p->oy,p->geom,tile);
            tile+=p->frame_bytes>>5;
        }
        t->last_frame=(uint8_t)f;
    }
    {unsigned now=oi-BATTLE_SOURCE_ABILITY_OBJ0;if(now<battle_source_effect_active_oam)for(unsigned i=now;i<battle_source_effect_active_oam;i++)hw_hide_obj(BATTLE_SOURCE_ABILITY_OBJ0+i);battle_source_effect_active_oam=now;}
}
static void battle_ability_tracks_run(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],uint8_t skill,unsigned frames){
    for(unsigned elapsed=0u;elapsed<frames;elapsed++){waitvb();battle_skill_source_audio_tick(skill,elapsed);battle_ability_tracks_draw(elapsed);battle_action_idle_after_wait(enemies);}
}
static int battle_party_choreo_pose_index(unsigned id){
    switch(id){
    case PS4_PRECOMP_PARTY_HAHN_ID:return PS4_PRECOMP_PARTY_HAHN_POSE_ARMS_RAISED_INDEX;
    case PS4_PRECOMP_PARTY_RUNE_ID:return PS4_PRECOMP_PARTY_RUNE_POSE_CHARGE_MAGIC_INDEX;
    case PS4_PRECOMP_PARTY_GRYZ_ID:return PS4_PRECOMP_PARTY_GRYZ_POSE_ARMS_RAISED_INDEX;
    case PS4_PRECOMP_PARTY_DEMI_ID:return PS4_PRECOMP_PARTY_DEMI_POSE_CHARGE_INDEX;
    case PS4_PRECOMP_PARTY_WREN_ID:return PS4_PRECOMP_PARTY_WREN_POSE_CHARGE_INDEX;
    case PS4_PRECOMP_PARTY_RAJA_ID:return PS4_PRECOMP_PARTY_RAJA_POSE_ARMS_RAISED_INDEX;
    case PS4_PRECOMP_PARTY_KYRA_ID:return PS4_PRECOMP_PARTY_KYRA_POSE_CHARGE_MAGIC_INDEX;
    default:return battle_party_cast_pose_index(id);
    }
}

static void battle_source_ability_play_anim(const Ps4SourceAbilityAnim* a,const int* xs,const int* ys,unsigned ntarget,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],uint8_t skill,int is_skill,unsigned* elapsed){
    if(is_skill)battle_skill_source_anim_start_audio(skill,a->source_table);
    const uint8_t* holds=ps4_source_ability_bank+a->holds_off;
    for(unsigned f=0u;f<a->frame_count;f++){
        unsigned h=holds[f];if(!h)h=1u;
        for(unsigned tick=0u;tick<h;tick++){
            waitvb();
            if(is_skill)battle_skill_source_audio_tick(skill,*elapsed);
            if(tick==0u)battle_source_ability_frame(a,f,xs,ys,ntarget);
            battle_action_idle_after_wait(enemies);(*elapsed)++;
        }
    }
}
static unsigned battle_skill_shared_helper(uint8_t id){
    /* 1 = loc_9F7A (fighter special-graphics helper), 2 = loc_9FCC
     * (fighter action helper). IDs are the exact SkillObj dispatch families. */
    switch(id){
    case 0x04u: case 0x0Au: case 0x0Bu: case 0x0Cu: case 0x0Du: case 0x16u:
    case 0x1Bu: case 0x1Du: case 0x24u: case 0x31u: case 0x32u: case 0x33u:return 1u;
    case 0x05u: case 0x0Eu: case 0x0Fu: case 0x11u: case 0x12u: case 0x13u:
    case 0x14u: case 0x17u: case 0x19u: case 0x1Au: case 0x1Eu: case 0x20u:
    case 0x21u: case 0x23u: case 0x25u:return 2u;
    default:return 0u;
    }
}
static int battle_party_source_generated_ability(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target,const Ps4SourceAbilityDesc* d,int is_skill){
    if(!d||(d->kind!=1u&&d->kind!=4u)||!d->anim_count)return 0;
    unsigned helper=(is_skill&&c)?battle_skill_shared_helper(c->id):0u;
    int pose=helper==1u?battle_party_choreo_pose_index(party[actor].id):battle_party_cast_pose_index(party[actor].id);
    if(is_skill){uint32_t sp=PS4_SKILL_DATA+(uint32_t)(c->id-1u)*8u;if((rom8(sp+1u)&0x80u)&&!helper)pose=-1;}
    if(helper){
        int px=120;unsigned tk=battle_command_target_kind(c);
        if(tk==1u&&*current_target<battle_enemy_count&&enemies[*current_target].hp)px=battle_enemy_anchor_x(*current_target);
        else if((tk==4u||tk==6u||tk==8u)&&c->target<pn)px=battle_party_center_x(c->target);
        battle_party_x_override_actor=(uint8_t)actor;battle_party_x_override_value=(int16_t)px;
    }
    int xs[5],ys[5];unsigned nt=battle_source_ability_points(party,pn,enemies,actor,c,*current_target,xs,ys);
    if(!nt){battle_party_x_override_actor=0xFFu;return 0;} /* no invented center/actor placement */
    if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);
    battle_source_ability_palette(d);
    if(!is_skill&&d->sfx)ram8w(0x500Au,d->sfx); /* source TECH root SFX only */
    unsigned executed=0u,elapsed=0u;
    for(unsigned ai=0u;ai<d->anim_count;ai++){
        const Ps4SourceAbilityAnim* a=&ps4_source_ability_anims[d->first_anim+ai];
        battle_source_ability_play_anim(a,xs,ys,nt,enemies,c->id,is_skill,&elapsed);
        /* Gameplay semantics are independent from presentation and execute once,
         * never once per source child object. */
        if(!executed){if(is_skill)battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;executed=1u;}
    }
    if(is_skill){unsigned end=battle_skill_source_audio_end(c->id);while(elapsed<end){waitvb();battle_skill_source_audio_tick(c->id,elapsed);battle_action_idle_after_wait(enemies);elapsed++;}}
    if(!executed){if(is_skill)battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;}
    battle_source_ability_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;battle_party_x_override_actor=0xFFu;
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);return 1;
}

static void battle_source_enemy_palette(const Ps4SourceEnemyDesc* d){
    dma_copy32(ps4_source_enemy_palettes[d->palette_id],(void*)(OBJ_PALETTE+BATTLE_SOURCE_ABILITY_PALBANK*16u),32u);
}
static void battle_source_enemy_frame_position(const Ps4SourceEnemyAnim* a,unsigned frame,const int* xs,const int* ys,unsigned ntarget,int upload){
    /* Generic/event bitmap battles do not use OBJ tiles 512..767 for enemy
     * bodies, so SOURCEENEMIES may use the full 16 KiB bitmap-mode OBJ window.
     * Event battle zero keeps Xanafalgue/Igglanova in the lower half. */
    unsigned oi=BATTLE_SOURCE_ABILITY_OBJ0,tile=(battle_event_index==0u)?BATTLE_ATTACK_OBJ_TILE:512u;
    for(unsigned pi=0u;pi<a->part_count;pi++){
        const Ps4SourceEnemyPart* p=&ps4_source_enemy_parts[a->first_part+pi];
        if(upload){const uint8_t* src=ps4_source_enemy_bank+p->off+(uint32_t)frame*p->frame_bytes;(void)battle_effect_upload(src,p->frame_bytes,tile);}
        for(unsigned t=0u;t<ntarget&&oi<BATTLE_SOURCE_ABILITY_OBJ0+BATTLE_SOURCE_ABILITY_OBJN;t++)
            battle_source_obj_prebound4(oi++,xs[t],ys[t],p->ox,p->oy,p->geom,tile);
        tile+=p->frame_bytes>>5;
    }
    {unsigned now=oi-BATTLE_SOURCE_ABILITY_OBJ0;if(now<battle_source_effect_active_oam)for(unsigned i=now;i<battle_source_effect_active_oam;i++)hw_hide_obj(BATTLE_SOURCE_ABILITY_OBJ0+i);battle_source_effect_active_oam=now;}
}
static void battle_source_enemy_frame(const Ps4SourceEnemyAnim* a,unsigned frame,const int* xs,const int* ys,unsigned ntarget){battle_source_enemy_frame_position(a,frame,xs,ys,ntarget,1);}
static void battle_source_enemy_play_anim(const Ps4SourceEnemyAnim* a,const int* xs,const int* ys,unsigned ntarget,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    const uint8_t* holds=ps4_source_enemy_bank+a->holds_off;
    for(unsigned f=0u;f<a->frame_count;f++){
        unsigned h=holds[f];if(!h)h=1u;
        for(unsigned tick=0u;tick<h;tick++){
            waitvb();if(tick==0u)battle_source_enemy_frame(a,f,xs,ys,ntarget);battle_action_idle_after_wait(enemies);
        }
    }
}
static unsigned battle_source_enemy_points(const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,unsigned victim,const Ps4SourceEnemyDesc* d,int xs[5],int ys[5]){
    if(d->target_mode==PS4_SOURCE_ENEMY_TARGET_SELF){if(ei>=battle_enemy_count)return 0u;xs[0]=battle_enemy_anchor_x(ei);ys[0]=battle_enemy_anchor_y(ei);return 1u;}
    if(d->target_mode==PS4_SOURCE_ENEMY_TARGET_ALL_PARTY){unsigned n=0u;for(unsigned i=0u;i<pn&&i<5u;i++)if(party[i].hp){xs[n]=battle_party_center_x(i);ys[n]=battle_source_y_to_gba(0x108u);n++;}return n;}
    if(victim<pn&&party[victim].hp){xs[0]=battle_party_center_x(victim);ys[0]=battle_source_y_to_gba(0x108u);return 1u;}
    return 0u;
}
/* CP9: EnemyAttack_Igglanova/Fission is a spatial controller, not an
 * all-party attack effect.  The source root spends 18 display ticks in its
 * opening state, enters loc_14C16 on tick 19, then moves the fission carrier
 * six source pixels per tick for eight ticks using the exact 16.16 vertical
 * acceleration.  loc_14E42 owns the four displayed arrival ticks; the fifth
 * update completes the object and reactivates the authored side fighter.
 * Lower that controller directly so the generated mappings are not cloned
 * over every party member. */
static int battle_source_igglanova_fission_action(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,const Ps4SourceEnemyDesc* d){
    if(!d||d->anim_count<2u||ei>=battle_enemy_count||battle_event_index!=0u)return 0;
    int left=battle_enemy_empty_side<0;
    if(!battle_enemy_empty_side){
        if(battle_enemy_count>0u&&!enemies[0].hp)left=1;
        else if(battle_enemy_count>2u&&!enemies[2].hp)left=0;
        else return 0;
    }
    const Ps4SourceEnemyAnim* move=&ps4_source_enemy_anims[d->first_anim];
    const Ps4SourceEnemyAnim* arrive=&ps4_source_enemy_anims[d->first_anim+1u];
    battle_source_enemy_palette(d);
    /* Source state 0 reaches state 4 on update 19.  Keep authored idle/body
     * simulation alive during those waits; the cue scheduler emits D7 there. */
    battle_action_hold(enemies,18u);
    int sx=left?0xF8:0x148;
    int32_t sy=((int32_t)0xDC)<<16,vy=-((int32_t)6<<16);
    const uint8_t* mh=ps4_source_enemy_bank+move->holds_off;unsigned mf=0u,mt=0u;
    for(unsigned tick=0u;tick<8u;tick++){
        waitvb();
        sx+=left?-6:6;vy+=(int32_t)0x0001D000;sy+=vy;
        while(mf+1u<move->frame_count&&mt>=mh[mf]){mt=0u;mf++;}
        int xs[1]={battle_source_x_to_gba((unsigned)sx)},ys[1]={battle_source_y_to_gba((unsigned)(sy>>16))};
        battle_source_enemy_frame_position(move,mf,xs,ys,1u,mt==0u);mt++;
        battle_action_idle_after_wait(enemies);
    }
    /* loc_14DF2 resets to source ($C8/$178,$F8) and loc_14E42 is 2x2. */
    sx=left?0xC8:0x178;unsigned af=0u,at=0u;const uint8_t* ah=ps4_source_enemy_bank+arrive->holds_off;
    for(unsigned tick=0u;tick<4u;tick++){
        waitvb();
        while(af+1u<arrive->frame_count&&at>=ah[af]){at=0u;af++;}
        int xs[1]={battle_source_x_to_gba((unsigned)sx)},ys[1]={battle_source_y_to_gba(0xF8u)};
        battle_source_enemy_frame_position(arrive,af,xs,ys,1u,at==0u);at++;
        battle_action_idle_after_wait(enemies);
    }
    /* Fifth loc_256AE update marks the arrival object complete.  The runtime
     * formation mutation that follows this action activates exactly the side
     * chosen by EnemyAI_EmptySpace. */
    waitvb();battle_source_ability_hide_all();battle_action_idle_after_wait(enemies);return 1;
}

static int battle_source_enemy_generated_action(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,unsigned victim,const Ps4SourceEnemyDesc* d,unsigned source_audio_owned){
    if(!d||!d->anim_count)return 0;int xs[5],ys[5];unsigned nt=battle_source_enemy_points(party,pn,enemies,ei,victim,d,xs,ys);if(!nt)return 0;
    battle_source_enemy_palette(d);if(!source_audio_owned&&d->primary_sfx)ram8w(0x500Au,d->primary_sfx);
    for(unsigned ai=0u;ai<d->anim_count;ai++)battle_source_enemy_play_anim(&ps4_source_enemy_anims[d->first_anim+ai],xs,ys,nt,enemies);
    battle_source_ability_hide_all();return 1;
}
static int battle_source_enemy_rimit_action(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned victim,unsigned source_audio_owned){
    /* Dark Marauder EnemySkill Rimit uses the same authored PSIV Rimit effect
     * family as Character Tech $17.  Reuse those exact source mappings/palette,
     * anchored at the selected party target; only gameplay ownership differs. */
    const Ps4SourceAbilityDesc* a=&ps4_source_tech[0x17u];if(a->kind!=1u||!a->anim_count||victim>=pn||!party[victim].hp)return 0;
    int xs[1]={battle_party_center_x(victim)},ys[1]={battle_source_y_to_gba(0x108u)};battle_source_ability_palette(a);if(!source_audio_owned&&a->sfx)ram8w(0x500Au,a->sfx);unsigned elapsed=0u;
    for(unsigned i=0u;i<a->anim_count;i++)battle_source_ability_play_anim(&ps4_source_ability_anims[a->first_anim+i],xs,ys,1u,enemies,0u,0,&elapsed);
    battle_source_ability_hide_all();return 1;
}
static int battle_source_enemy_earthquake_action(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned source_audio_owned){
    /* BattleObj_KingRappyEarthquake ($904), loc_344AC: eight six-frame camera
     * shake cycles, amplitudes $10,$08,$04,$03,$02,$02,$01,$01.  Counter 4
     * emits GraveOpening each cycle.  This is the original controller effect;
     * it has no substitute sprite. */
    static const int8_t amp[8]={16,8,4,3,2,2,1,1};volatile int32_t* bg2x=(volatile int32_t*)0x04000028;
    for(unsigned phase=0u;phase<8u;phase++)for(unsigned f=1u;f<=6u;f++){
        waitvb();if(f==3u)*bg2x=(int32_t)((int)amp[phase]<<8);if(f==4u)ram8w(0x500Au,0xDDu);if(f==6u)*bg2x=-(int32_t)((int32_t)amp[phase]*256);battle_action_idle_after_wait(enemies);
    }
    waitvb();*bg2x=0;battle_action_idle_after_wait(enemies);(void)source_audio_owned;return 1;
}
static int battle_enemy_source_action(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t ability);
static int battle_enemy_source_action_full(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t ability,unsigned victim,unsigned source_audio_owned){
    if(ei>=battle_enemy_count)return 0;
    const Ps4SourceEnemyDesc* d=ps4_source_enemy_find(enemies[ei].id,ability);if(!d)return 0;
    if(d->kind==PS4_SOURCE_ENEMY_KIND_EXISTING)return battle_enemy_source_action(enemies,ei,ability);
    if(d->kind==PS4_SOURCE_ENEMY_KIND_NOOP)return 1; /* explicit source branch clears/returns without presentation */
    if(d->kind==PS4_SOURCE_ENEMY_KIND_RIMIT_SCREEN)return battle_source_enemy_rimit_action(party,pn,enemies,victim,source_audio_owned);
    if(d->kind==PS4_SOURCE_ENEMY_KIND_EARTHQUAKE_SCREEN)return battle_source_enemy_earthquake_action(enemies,source_audio_owned);
    if(battle_event_index==0u&&(enemies[ei].id==0x0Cu||enemies[ei].id==0x0Du)&&ability!=0u)return battle_source_igglanova_fission_action(enemies,ei,d);
    if(d->kind==PS4_SOURCE_ENEMY_KIND_BODY_DBLSLASH||d->kind==PS4_SOURCE_ENEMY_KIND_GENERATED)return battle_source_enemy_generated_action(party,pn,enemies,ei,victim,d,source_audio_owned);
    return 0;
}

static int battle_party_brose_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    /* BattleObj_Brose is intentionally not replaced by sprite art: the source
     * effect is an H-scroll distortion controller.  Retain its source startup
     * delay, Brose SFX and alternating +/-2 pixel displacement semantics using
     * the GBA bitmap plane as a whole-screen analogue. */
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,5u);ram8w(0x500Au,0xCBu);
    /* The Genesis routine performs nine ticks of -2/0/+2/0 line scroll and
     * nine ticks of the inverse pattern.  A bitmap-wide +/-2 phase preserves
     * the authored displacement/timing without inventing alternate artwork. */
    volatile int32_t* bg2x=(volatile int32_t*)0x04000028;
    for(unsigned phase=0u;phase<18u;phase++){waitvb();*bg2x=(int32_t)(((phase&1u)?2:-2)<<8);battle_action_idle_after_wait(enemies);}
    waitvb();*bg2x=0;battle_action_idle_after_wait(enemies);battle_execute_cmd(party,pn,enemies,actor,c,current_target);
    battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_source_weapon_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    /* DblSlash and Disrupt are source close-range weapon controllers rather
     * than generic ability sprites.  Reuse the authored character/weapon pose
     * path.  Disrupt performs it for every live enemy; semantics execute once. */
    if(c->id==0x09u){unsigned saved=*current_target;for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp){*current_target=i;(void)battle_party_source_attack(party,pn,enemies,i,actor,c);}*current_target=saved;}
    else (void)battle_party_source_attack(party,pn,enemies,*current_target,actor,c);
    battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;
    battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);return 1;
}
static int battle_party_source_death_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    /* Skill $18 Death is not a fixed two-layer animation.  Source
     * BattleObj_Death waits $1E, launches DeathChild, then chooses either
     * SFXID_AttackMiss ($B8) with no hit child or SFXID_Sword ($F5) plus
     * loc_25EB44 when Battle_Ability_Effects says the death succeeded. */
    if(!c||c->id!=0x18u||actor>=pn||*current_target>=battle_enemy_count||!enemies[*current_target].hp)return 0;
    const Ps4SourceAbilityDesc* d=&ps4_source_skill[0x18u];if(d->kind!=1u||d->anim_count<2u)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_source_ability_palette(d);
    /* BattleObj_Death's root timer is the authored pre-projectile delay. */
    battle_action_hold(enemies,0x1Eu);
    int xs[1]={battle_enemy_anchor_x(*current_target)},ys[1]={battle_source_y_to_gba(0xD8u)};unsigned elapsed=0u;
    battle_source_ability_play_anim(&ps4_source_ability_anims[d->first_anim],xs,ys,1u,enemies,c->id,1,&elapsed);
    int before=enemies[*current_target].hp;
    battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;
    int success=(before>0&&enemies[*current_target].hp<=0);
    if(success){ram8w(0x500Au,0xF5u);battle_source_ability_play_anim(&ps4_source_ability_anims[d->first_anim+1u],xs,ys,1u,enemies,c->id,1,&elapsed);}
    else ram8w(0x500Au,0xB8u);
    battle_source_ability_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);return 1;
}



static int battle_timed_frame(unsigned age,const uint8_t* holds,unsigned n);
static int battle_party_source_slash_controller_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(!c||actor>=pn||(c->id!=0x01u&&c->id!=0x02u&&c->id!=0x08u&&c->id!=0x10u))return 0;
    const Ps4SourceAbilityDesc* d=&ps4_source_skill[c->id];if(d->kind!=1u)return 0;int pose=battle_party_choreo_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_source_ability_palette(d);battle_ability_tracks_reset();int x[1],y[1];
    if(c->id==0x01u||c->id==0x02u){x[0]=battle_party_center_x(actor);y[0]=battle_source_y_to_gba(0xD0u);battle_ability_track_add(c->id==0x01u?0x25E68Au:0x25E5FEu,0x10u,x,y,1u);}
    else {x[0]=battle_source_x_to_gba(0xD0u);y[0]=battle_source_y_to_gba(0xD8u);battle_ability_track_add(0x25E804u,0x14u,x,y,1u);x[0]=battle_source_x_to_gba(0x170u);battle_ability_track_add(0x25E824u,0x14u,x,y,1u);}
    /* SkillObj_Crosscut/Rayblade/Airslash/Sweeping hand control to the fighter
     * after the hidden root delay ($0F / $19).  Use the already-source-backed
     * rear-view attack sequence when that character has one; otherwise retain
     * the recovered controller pose instead of synthesizing a weapon frame. */
    unsigned root=(c->id==0x08u||c->id==0x10u)?0x19u:0x0Fu;const uint8_t* ah=0;unsigned af=0u;(void)battle_party_attack_pixels(party[actor].id,0u,&ah,&af);unsigned attack_len=0u;if(ah&&af)for(unsigned i=0u;i<af;i++)attack_len+=ah[i]?ah[i]:1u;
    unsigned end=battle_ability_tracks_end();unsigned ae=battle_skill_source_audio_end(c->id);if(ae>end)end=ae;if(root+attack_len>end)end=root+attack_len;int last=-1;
    battle_party_anim_actor=(uint8_t)actor;battle_party_anim_attack=(ah&&af)?1u:0u;
    for(unsigned t=0u;t<end;t++){
        waitvb();battle_skill_source_audio_tick(c->id,t);
        if(ah&&af&&t>=root){int fr=battle_timed_frame(t-root,ah,af);if(fr>=0&&fr!=last){battle_party_pose_actor=0xFFu;battle_party_anim_frame=(uint8_t)fr;battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);last=fr;}}
        battle_ability_tracks_draw(t);battle_action_idle_after_wait(enemies);
    }
    battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;battle_source_ability_hide_all();battle_party_anim_attack=0u;battle_party_anim_actor=0xFFu;battle_party_anim_frame=0u;battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);return 1;
}

static int battle_party_source_support_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(!c||c->type!=3u||c->id<0x26u||c->id>0x2Eu||actor>=pn)return 0;
    const Ps4SourceAbilityDesc* d=&ps4_source_skill[c->id];if(d->kind!=1u)return 0;
    int tx[5],ty[5];unsigned nt=battle_source_ability_points(party,pn,enemies,actor,c,*current_target,tx,ty);if(!nt)return 0;
    int cx=battle_party_center_x(actor),x1[5],x2[5],y1[5],y2[5];
    for(unsigned i=0u;i<nt;i++){x1[i]=tx[i];x2[i]=tx[i];y1[i]=battle_source_y_to_gba(0x100u);y2[i]=battle_source_y_to_gba(0x108u);}
    int pose=(c->id==0x27u||c->id==0x29u||c->id==0x2Au||c->id==0x2Bu)?battle_party_choreo_pose_index(party[actor].id):battle_party_cast_pose_index(party[actor].id);
    if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_source_ability_palette(d);battle_ability_tracks_reset();
    int ax[2]={cx,cx},ay[2]={battle_source_y_to_gba(0x100u),battle_source_y_to_gba(0x100u)};
    unsigned end=0u;
    switch(c->id){
    case 0x26u: /* Barrier: recovery caster child + per-affected barrier + cleanup child. */
        ax[0]=cx+12;ay[0]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25B0C8u,0x1Eu,ax,ay,1u);
        battle_ability_track_add(0x25E3FAu,0x2Eu,x1,y1,nt);
        battle_ability_track_add(0x25B008u,0x66u,ax,ay,1u);break;
    case 0x27u: /* War Cry is a fighter/action controller, not an ability sprite. */
        battle_action_hold(enemies,0x1Eu);ram8w(0x500Au,0xD4u);battle_action_hold(enemies,0x2Du);end=0u;break;
    case 0x28u: /* Blessing: mirrored caster pair then one child per affected fighter. */
        ax[0]=cx+12;ax[1]=cx-12;ay[0]=ay[1]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25CE88u,0x1Eu,ax,ay,2u);
        battle_ability_track_add(0x25D240u,0x37u,x1,y1,nt);break;
    case 0x29u: /* Warla: source loc_3CCAA + loc_25AF70 + party fanout. */
        ax[0]=cx;ay[0]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25D916u,0x10u,ax,ay,1u);battle_ability_track_add(0x25AF70u,0x1Eu,ax,ay,1u);
        battle_ability_track_add(0x25D240u,0x37u,x1,y1,nt);break;
    case 0x2Au: /* Recover: three independently-delayed source children. */
        ax[0]=cx+12;ay[0]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25B0C8u,0x1Eu,ax,ay,1u);
        ax[0]=cx;ay[0]=battle_source_y_to_gba(0x114u);battle_ability_track_add(0x25E4F8u,0x36u,ax,ay,1u);battle_ability_track_add(0x25E4F8u,0x48u,ax,ay,1u);battle_ability_track_add(0x25E4F8u,0x5Au,ax,ay,1u);
        ax[0]=cx+12;ay[0]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25B008u,0x6Eu,ax,ay,1u);break;
    case 0x2Bu: { /* Medice: caster helper + caster effect + selected party target. */
        ax[0]=cx;ay[0]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25D916u,0x10u,ax,ay,1u);battle_ability_track_add(0x25D39Cu,0x1Eu,ax,ay,1u);
        unsigned t=c->target<pn?c->target:actor;int mx[1]={battle_party_center_x(t)},my[1]={battle_source_y_to_gba(0x108u)};battle_ability_track_add(0x25D542u,0x46u,mx,my,1u);break;}
    case 0x2Cu: /* Miracle: mirrored caster pair + all affected party members. */
        ax[0]=cx+12;ax[1]=cx-12;ay[0]=ay[1]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25D39Cu,0x1Eu,ax,ay,2u);battle_ability_track_add(0x25D542u,0x46u,x2,y2,nt);break;
    case 0x2Du: /* Medic Power: per-fighter long child controller plus start/end helper. */
        ax[0]=cx+12;ay[0]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25B0C8u,0x1Eu,ax,ay,1u);battle_ability_track_add(0x25D708u,0x2Eu,x2,y2,nt);battle_ability_track_add(0x25B008u,0x95u,ax,ay,1u);break;
    case 0x2Eu: { /* Ataraxia: mirrored caster pair; three vertically-staggered children per target. */
        ax[0]=cx+12;ax[1]=cx-12;ay[0]=ay[1]=battle_source_y_to_gba(0x100u);battle_ability_track_add(0x25D39Cu,0x1Eu,ax,ay,2u);
        int qx[5],qy[5];for(unsigned i=0u;i<nt;i++){qx[i]=tx[i]-6;qy[i]=battle_source_y_to_gba(0x11Cu);}battle_ability_track_add(0x25D6C0u,0x46u,qx,qy,nt);
        for(unsigned i=0u;i<nt;i++){qx[i]=tx[i]+6;qy[i]=battle_source_y_to_gba(0x110u);}battle_ability_track_add(0x25D6C0u,0x4Eu,qx,qy,nt);
        for(unsigned i=0u;i<nt;i++){qx[i]=tx[i];qy[i]=battle_source_y_to_gba(0x104u);}battle_ability_track_add(0x25D6C0u,0x56u,qx,qy,nt);break;}
    default:return 0;
    }
    if(c->id!=0x27u){end=battle_ability_tracks_end();unsigned audio_end=battle_skill_source_audio_end(c->id);if(audio_end>end)end=audio_end;battle_ability_tracks_run(enemies,c->id,end);}
    battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;
    battle_source_ability_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);return 1;
}

static int battle_party_source_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(!c||c->type!=3u||actor>=pn||c->id<1u||c->id>54u||!battle_action_has_resource(party,pn,actor,c))return 0;
    /* Every valid battle SKILL is explicit.  No generic cast/weapon fallback is
     * permitted: the special controller classes below are source controllers,
     * and every other ID must resolve through the generated source BattleObj bank. */
    if(c->id==0x01u||c->id==0x02u||c->id==0x08u||c->id==0x10u)return battle_party_source_slash_controller_skill(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_SKILL_VORTEX_ID)return battle_party_vortex_source_skill(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_SKILL_EARTH_ID)return battle_party_earth_source_skill(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_SKILL_VISION_ID)return battle_party_vision_source_skill(party,pn,enemies,actor,c,current_target);
    if(c->id==0x03u||c->id==0x09u)return battle_party_source_weapon_skill(party,pn,enemies,actor,c,current_target); /* DblSlash / Disrupt */
    if(c->id==0x18u)return battle_party_source_death_skill(party,pn,enemies,actor,c,current_target); /* Death: source hit/miss controller */
    if(c->id>=0x26u&&c->id<=0x2Eu)return battle_party_source_support_skill(party,pn,enemies,actor,c,current_target);
    const Ps4SourceAbilityDesc* d=&ps4_source_skill[c->id];
    if((d->kind==1u||d->kind==4u)&&d->anim_count)return battle_party_source_generated_ability(party,pn,enemies,actor,c,current_target,d,1);
    return 0; /* explicit failure: never synthesize an animation or sound */
}

static void battle_attack_frame_wait(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned frames){battle_action_hold(enemies,frames);}

#define MICRO_PARENT_TILE 768u
#define MICRO_BURST0_TILE 784u
#define MICRO_BURST1_TILE 852u
#define MICRO_MISSILE_TILE 920u
#define MICRO_IMPACT0_TILE 928u
#define MICRO_IMPACT1_TILE 960u
#define MICRO_IMPACT2_TILE 992u
static int battle_timed_frame(unsigned age,const uint8_t* holds,unsigned n){unsigned t=0u;for(unsigned f=0u;f<n;f++){t+=holds[f];if(age<t)return (int)f;}return -1;}
static int battle_source_x_to_gba(unsigned x){return 120+(((int)x-0x120)*3)/4;}
static int battle_source_x_to_gba_signed(int x){return 120+((x-0x120)*3)/4;}
static int battle_source_y_to_gba(unsigned y){return (((int)y-0x80)*3)/4;}
static void battle_attack_hide_range(unsigned first,unsigned count){for(unsigned i=0u;i<count;i++)hw_hide_obj(first+i);}
static uint16_t battle_coord_word(unsigned off,unsigned index,unsigned axis){return (uint16_t)(ps4_precomp_assets[off+index*4u+axis*2u]|((uint16_t)ps4_precomp_assets[off+index*4u+axis*2u+1u]<<8));}
static void battle_upload_party_attack_frame(unsigned off,unsigned frame,unsigned w,unsigned h,unsigned tile){unsigned b=w*h;battle_obj_upload_attack(ps4_precomp_assets+off+frame*b,b,tile);}
static int battle_party_vortex_source_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(!c||c->id!=PS4_PRECOMP_SKILL_VORTEX_ID||actor>=pn||!battle_action_has_resource(party,pn,actor,c))return 0;
    /* SkillObj_Vortex -> BattleObj_Vortex ($554): the root waits $1E updates,
     * then creates the two slasher-trace objects (loc_25FDDE/loc_25FDCC) and
     * BattleObj_VortexChild ($558, loc_25FDF0). Character_DoSkill has already
     * loaded skill PLC $92, so these frames are generated from that PLC rather
     * than reusing the visually-related physical slasher PLC $77. */
    unsigned sm=battle_actor_slasher_mask(&party[actor]);
    if(party[actor].id==1u&&sm){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)(sm==3u?PS4_PRECOMP_PARTY_ALYS_POSE_TWO_SLASHERS_INDEX:PS4_PRECOMP_PARTY_ALYS_POSE_SLASHER_INDEX);}
    else {int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,PS4_PRECOMP_VORTEX_ROOT_DELAY);
    if(party[actor].id==1u&&sm)battle_party_pose_index=(uint8_t)(sm==3u?PS4_PRECOMP_PARTY_ALYS_POSE_THROW_TWO_SLASHERS_INDEX:PS4_PRECOMP_PARTY_ALYS_POSE_THROW_SLASHER_INDEX);
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);
    const uint8_t* ha=ps4_precomp_assets+PS4_PRECOMP_VORTEX_TRACE_A_HOLDS_OFF;const uint8_t* hb=ps4_precomp_assets+PS4_PRECOMP_VORTEX_TRACE_B_HOLDS_OFF;
    const unsigned throw_tile=BATTLE_ATTACK_OBJ_TILE,trace_a_tile=BATTLE_ATTACK_OBJ_TILE+64u,trace_b_tile=BATTLE_ATTACK_OBJ_TILE+128u;
    int trace_x=battle_source_x_to_gba(0x120u),trace_y=battle_source_y_to_gba(0x128u);
    for(unsigned f=0u;f<PS4_PRECOMP_VORTEX_COORD_COUNT;f++){
        waitvb();battle_upload_party_attack_frame(PS4_PRECOMP_VORTEX_THROW_PIXELS_OFF,f%PS4_PRECOMP_VORTEX_THROW_FRAMES,PS4_PRECOMP_VORTEX_THROW_W,PS4_PRECOMP_VORTEX_THROW_H,throw_tile);
        int fa=battle_timed_frame(f,ha,PS4_PRECOMP_VORTEX_TRACE_A_FRAMES),fb=battle_timed_frame(f,hb,PS4_PRECOMP_VORTEX_TRACE_B_FRAMES);
        if(fa>=0)battle_upload_party_attack_frame(PS4_PRECOMP_VORTEX_TRACE_A_PIXELS_OFF,(unsigned)fa,PS4_PRECOMP_VORTEX_TRACE_A_W,PS4_PRECOMP_VORTEX_TRACE_A_H,trace_a_tile);
        if(fb>=0)battle_upload_party_attack_frame(PS4_PRECOMP_VORTEX_TRACE_B_PIXELS_OFF,(unsigned)fb,PS4_PRECOMP_VORTEX_TRACE_B_W,PS4_PRECOMP_VORTEX_TRACE_B_H,trace_b_tile);
        unsigned sx=battle_coord_word(PS4_PRECOMP_VORTEX_RIGHT_COORDS_OFF,f,0u),sy=battle_coord_word(PS4_PRECOMP_VORTEX_RIGHT_COORDS_OFF,f,1u);
        battle_obj_any(16u,battle_source_x_to_gba(sx)+(int)PS4_PRECOMP_VORTEX_THROW_CENTER_X-128,battle_source_y_to_gba(sy)+(int)PS4_PRECOMP_VORTEX_THROW_CENTER_Y-128,PS4_PRECOMP_VORTEX_THROW_W,PS4_PRECOMP_VORTEX_THROW_H,throw_tile,0,1,1);
        if(fa>=0)battle_obj_any(17u,trace_x+(int)PS4_PRECOMP_VORTEX_TRACE_A_CENTER_X-128,trace_y+(int)PS4_PRECOMP_VORTEX_TRACE_A_CENTER_Y-128,PS4_PRECOMP_VORTEX_TRACE_A_W,PS4_PRECOMP_VORTEX_TRACE_A_H,trace_a_tile,0,1,1);else hw_hide_obj(17u);
        if(fb>=0)battle_obj_any(18u,trace_x+(int)PS4_PRECOMP_VORTEX_TRACE_B_CENTER_X-128,trace_y+(int)PS4_PRECOMP_VORTEX_TRACE_B_CENTER_Y-128,PS4_PRECOMP_VORTEX_TRACE_B_W,PS4_PRECOMP_VORTEX_TRACE_B_H,trace_b_tile,0,1,1);else hw_hide_obj(18u);
        battle_action_idle_after_wait(enemies);
    }
    battle_attack_hide_range(16u,3u);
    /* BattleObj_VortexChild does not end with the outbound throw.  Once
     * Battle_ThrowSlasher exhausts loc_25FDF0 it changes the same object to
     * loc_25E58C at VRAM $AC00, hides for $14 updates, then starts at
     * ($1C0,$E8) and moves left $10 source pixels/update until target+$28.
     * Recreate that controller verbatim enough for the GBA-scaled battle
     * coordinates; the two-frame source mapping loops while travelling. */
    battle_action_hold(enemies,0x14u);
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);
    if(*current_target>=battle_enemy_count){battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;}
    const uint8_t* rh=ps4_precomp_assets+PS4_PRECOMP_VORTEX_RETURN_HOLDS_OFF;
    unsigned rperiod=0u;for(unsigned i=0u;i<PS4_PRECOMP_VORTEX_RETURN_FRAMES;i++)rperiod+=rh[i];if(!rperiod)rperiod=1u;
    const unsigned return_tile=BATTLE_ATTACK_OBJ_TILE;int rx=battle_source_x_to_gba(0x1C0u),ry=battle_source_y_to_gba(0xE8u);
    const int target_gate=battle_enemy_anchor_x(*current_target)+((0x28*3)/4);unsigned rage=0u;
    do{
        waitvb();rx-=12;int rf=battle_timed_frame(rage,rh,PS4_PRECOMP_VORTEX_RETURN_FRAMES);if(rf<0)rf=0;
        battle_upload_party_attack_frame(PS4_PRECOMP_VORTEX_RETURN_PIXELS_OFF,(unsigned)rf,PS4_PRECOMP_VORTEX_RETURN_W,PS4_PRECOMP_VORTEX_RETURN_H,return_tile);
        battle_obj_any(16u,rx+(int)PS4_PRECOMP_VORTEX_RETURN_CENTER_X-128,ry+(int)PS4_PRECOMP_VORTEX_RETURN_CENTER_Y-128,PS4_PRECOMP_VORTEX_RETURN_W,PS4_PRECOMP_VORTEX_RETURN_H,return_tile,0,1,1);
        rage++;if(rage>=rperiod)rage=0u;battle_action_idle_after_wait(enemies);
    }while(rx>target_gate);
    hw_hide_obj(16u);

    /* Crossing the target creates three BattleObj_AbilityAnim children using
     * loc_25E5A8 at x=target-$28,y=$D8 with delays 5,$16,$27; the latter two
     * are shifted upward by $A/$14.  Each child plays source SFX $F5 when its
     * delay expires.  Keep them independent so the staggered slasher impacts
     * surround the enemy instead of collapsing into one generic flash. */
    battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;
    const uint8_t* ih=ps4_precomp_assets+PS4_PRECOMP_VORTEX_IMPACT_HOLDS_OFF;unsigned ilen=0u;for(unsigned i=0u;i<PS4_PRECOMP_VORTEX_IMPACT_FRAMES;i++)ilen+=ih[i];
    const unsigned delays[3]={6u,0x17u,0x28u};const int yoff[3]={0,-8,-15};
    const unsigned ip1_tile=BATTLE_ATTACK_OBJ_TILE+64u,ip2_tile=BATTLE_ATTACK_OBJ_TILE+128u;
    int ix=battle_enemy_anchor_x(*current_target)-30,iy=battle_source_y_to_gba(0xD8u);
    unsigned total=0x20u;if(0x28u+ilen>total)total=0x28u+ilen;
    for(unsigned t=0u;t<total;t++){
        waitvb();
        for(unsigned k=0u;k<3u;k++){unsigned o=17u+k*2u;if(t<delays[k]||t>=delays[k]+ilen){hw_hide_obj(o);hw_hide_obj(o+1u);continue;}
            unsigned age=t-delays[k];int fr=battle_timed_frame(age,ih,PS4_PRECOMP_VORTEX_IMPACT_FRAMES);if(fr<0){hw_hide_obj(o);hw_hide_obj(o+1u);continue;}
            if(age==0u)ram8w(0x500Au,0xF5u);
            battle_upload_party_attack_frame(PS4_PRECOMP_VORTEX_IMPACT_PART1_PIXELS_OFF,(unsigned)fr,PS4_PRECOMP_VORTEX_IMPACT_PART1_W,PS4_PRECOMP_VORTEX_IMPACT_PART1_H,ip1_tile);
            battle_upload_party_attack_frame(PS4_PRECOMP_VORTEX_IMPACT_PART2_PIXELS_OFF,(unsigned)fr,PS4_PRECOMP_VORTEX_IMPACT_PART2_W,PS4_PRECOMP_VORTEX_IMPACT_PART2_H,ip2_tile);
            battle_obj_any(o,ix+(int)PS4_PRECOMP_VORTEX_IMPACT_PART1_CENTER_X-128,iy+yoff[k]+(int)PS4_PRECOMP_VORTEX_IMPACT_PART1_CENTER_Y-128,PS4_PRECOMP_VORTEX_IMPACT_PART1_W,PS4_PRECOMP_VORTEX_IMPACT_PART1_H,ip1_tile,0,1,1);
            battle_obj_any(o+1u,ix+(int)PS4_PRECOMP_VORTEX_IMPACT_PART2_CENTER_X-128,iy+yoff[k]+(int)PS4_PRECOMP_VORTEX_IMPACT_PART2_CENTER_Y-128,PS4_PRECOMP_VORTEX_IMPACT_PART2_W,PS4_PRECOMP_VORTEX_IMPACT_PART2_H,ip2_tile,0,1,1);
        }
        battle_action_idle_after_wait(enemies);
    }
    battle_attack_hide_range(16u,7u);
    battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}


static int battle_party_earth_source_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(!c||c->id!=PS4_PRECOMP_SKILL_EARTH_ID||actor>=pn||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    /* SkillObj_Earth -> BattleObj_Earth ($540).  Character_DoSkill loads PLC
     * $AB at $9B80: Map_Earth2 consumes bank 0 and Map_Earth consumes bank 1
     * at $A020.  The root lives $3C updates on failure and $64 on success. */
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);
    uint8_t before_status=enemies[*current_target].status;
    battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;
    int success=enemies[*current_target].status!=before_status;unsigned total=success?PS4_PRECOMP_EARTH_SUCCESS_DELAY:PS4_PRECOMP_EARTH_ROOT_DELAY;
    const uint8_t* mh=ps4_precomp_assets+PS4_PRECOMP_EARTH_MAIN_HOLDS_OFF;const uint8_t* sh=ps4_precomp_assets+PS4_PRECOMP_EARTH_SUCCESS_HOLDS_OFF;
    int ax=battle_enemy_anchor_x(*current_target),ay=battle_source_y_to_gba(0xD8u);const unsigned mtile=BATTLE_ATTACK_OBJ_TILE,stile=BATTLE_ATTACK_OBJ_TILE+32u;
    for(unsigned t=0u;t<total;t++){
        waitvb();if(t==0x10u)ram8w(0x500Au,0xD2u);if(success&&t==0x20u)ram8w(0x500Au,0xCAu);
        int mf=battle_timed_frame(t,mh,PS4_PRECOMP_EARTH_MAIN_FRAMES);if(mf>=0){battle_upload_party_attack_frame(PS4_PRECOMP_EARTH_MAIN_PIXELS_OFF,(unsigned)mf,PS4_PRECOMP_EARTH_MAIN_W,PS4_PRECOMP_EARTH_MAIN_H,mtile);battle_obj_any(16u,ax+(int)PS4_PRECOMP_EARTH_MAIN_CENTER_X-128,ay+(int)PS4_PRECOMP_EARTH_MAIN_CENTER_Y-128,PS4_PRECOMP_EARTH_MAIN_W,PS4_PRECOMP_EARTH_MAIN_H,mtile,0,1,1);}else hw_hide_obj(16u);
        int sf=success?battle_timed_frame(t,sh,PS4_PRECOMP_EARTH_SUCCESS_FRAMES):-1;if(sf>=0){battle_upload_party_attack_frame(PS4_PRECOMP_EARTH_SUCCESS_PIXELS_OFF,(unsigned)sf,PS4_PRECOMP_EARTH_SUCCESS_W,PS4_PRECOMP_EARTH_SUCCESS_H,stile);battle_obj_any(17u,ax+(int)PS4_PRECOMP_EARTH_SUCCESS_CENTER_X-128,ay+(int)PS4_PRECOMP_EARTH_SUCCESS_CENTER_Y-128,PS4_PRECOMP_EARTH_SUCCESS_W,PS4_PRECOMP_EARTH_SUCCESS_H,stile,0,1,1);}else hw_hide_obj(17u);
        battle_action_idle_after_wait(enemies);
    }
    battle_attack_hide_range(16u,2u);battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_vision_source_skill(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(!c||c->id!=PS4_PRECOMP_SKILL_VISION_ID||actor>=pn||!battle_action_has_resource(party,pn,actor,c))return 0;
    /* BattleObj_Vision ($5B4) creates two loc_25D6C0 ability objects at x-8
     * and x+8 around every affected party member, y=$100, then keeps its root
     * alive for $50 updates.  The same predecoded frame can be shared by all
     * OAM instances because the source also points every child at one bank. */
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);
    battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_suppress_skill_source_audio=0u;
    const uint8_t* h=ps4_precomp_assets+PS4_PRECOMP_VISION_EFFECT_HOLDS_OFF;unsigned tile=BATTLE_ATTACK_OBJ_TILE,visual_len=0u;for(unsigned i=0u;i<PS4_PRECOMP_VISION_EFFECT_FRAMES;i++)visual_len+=h[i];
    int y=battle_source_y_to_gba(0x100u);
    for(unsigned t=0u;t<PS4_PRECOMP_VISION_ROOT_DELAY;t++){
        waitvb();if(t==0x1Eu)ram8w(0x500Au,0xC4u);int fr=t<visual_len?battle_timed_frame(t,h,PS4_PRECOMP_VISION_EFFECT_FRAMES):-1;
        if(fr>=0){battle_upload_party_attack_frame(PS4_PRECOMP_VISION_EFFECT_PIXELS_OFF,(unsigned)fr,PS4_PRECOMP_VISION_EFFECT_W,PS4_PRECOMP_VISION_EFFECT_H,tile);for(unsigned i=0u;i<pn&&i<5u;i++){unsigned o=16u+i*2u;if(!party[i].hp){hw_hide_obj(o);hw_hide_obj(o+1u);continue;}int x=battle_party_center_x(i);battle_obj_any(o,x-6+(int)PS4_PRECOMP_VISION_EFFECT_CENTER_X-128,y+(int)PS4_PRECOMP_VISION_EFFECT_CENTER_Y-128,PS4_PRECOMP_VISION_EFFECT_W,PS4_PRECOMP_VISION_EFFECT_H,tile,0,1,1);battle_obj_any(o+1u,x+6+(int)PS4_PRECOMP_VISION_EFFECT_CENTER_X-128,y+(int)PS4_PRECOMP_VISION_EFFECT_CENTER_Y-128,PS4_PRECOMP_VISION_EFFECT_W,PS4_PRECOMP_VISION_EFFECT_H,tile,0,1,1);}}else battle_attack_hide_range(16u,10u);
        battle_action_idle_after_wait(enemies);
    }
    battle_attack_hide_range(16u,10u);battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}

static int battle_alys_slasher_source_attack(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* c,unsigned mask){
    if(actor>=pn||!mask)return 0;unsigned dual=mask==3u;unsigned kyra=party[actor].id==9u;int clean_valid=0;
    battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)(kyra?(dual?PS4_PRECOMP_PARTY_KYRA_POSE_TWO_SLASHERS_INDEX:PS4_PRECOMP_PARTY_KYRA_POSE_SLASHER_INDEX):(dual?PS4_PRECOMP_PARTY_ALYS_POSE_TWO_SLASHERS_INDEX:PS4_PRECOMP_PARTY_ALYS_POSE_SLASHER_INDEX));
    if(!battle_present_actor_pose_dirty(party,pn,enemies,enemy_target,actor,c,&clean_valid,1))battle_present_action(party,pn,enemies,enemy_target,0u,actor,c,0u);
    if(battle_action_hold_track_fb(enemies,PS4_PRECOMP_ALYS_SLASHER_BODY_WINDUP))clean_valid=0;
    battle_party_pose_index=(uint8_t)(kyra?(dual?PS4_PRECOMP_PARTY_KYRA_POSE_THROW_TWO_SLASHERS_INDEX:PS4_PRECOMP_PARTY_KYRA_POSE_THROW_SLASHER_INDEX):(dual?PS4_PRECOMP_PARTY_ALYS_POSE_THROW_TWO_SLASHERS_INDEX:PS4_PRECOMP_PARTY_ALYS_POSE_THROW_SLASHER_INDEX));
    if(!battle_present_actor_pose_dirty(party,pn,enemies,enemy_target,actor,c,&clean_valid,0))battle_present_action(party,pn,enemies,enemy_target,0u,actor,c,0u);ram8w(0x500Au,0xB7u);
    const uint8_t* ha=ps4_precomp_assets+PS4_PRECOMP_ALYS_SLASHER_TRACE_A_HOLDS_OFF;const uint8_t* hb=ps4_precomp_assets+PS4_PRECOMP_ALYS_SLASHER_TRACE_B_HOLDS_OFF;
    const unsigned throw_tile=BATTLE_ATTACK_OBJ_TILE,trace_a_tile=BATTLE_ATTACK_OBJ_TILE+64u,trace_b_tile=BATTLE_ATTACK_OBJ_TILE+128u;
    int trace_x=battle_source_x_to_gba(0x120u),trace_y=battle_source_y_to_gba(0x128u);
    for(unsigned f=0u;f<PS4_PRECOMP_ALYS_SLASHER_COORD_COUNT;f++){
        waitvb();battle_upload_party_attack_frame(PS4_PRECOMP_ALYS_SLASHER_THROW_PIXELS_OFF,f,PS4_PRECOMP_ALYS_SLASHER_THROW_W,PS4_PRECOMP_ALYS_SLASHER_THROW_H,throw_tile);
        int fa=battle_timed_frame(f,ha,PS4_PRECOMP_ALYS_SLASHER_TRACE_A_FRAMES),fb=battle_timed_frame(f,hb,PS4_PRECOMP_ALYS_SLASHER_TRACE_B_FRAMES);
        if(fa>=0)battle_upload_party_attack_frame(PS4_PRECOMP_ALYS_SLASHER_TRACE_A_PIXELS_OFF,(unsigned)fa,PS4_PRECOMP_ALYS_SLASHER_TRACE_A_W,PS4_PRECOMP_ALYS_SLASHER_TRACE_A_H,trace_a_tile);
        if(fb>=0)battle_upload_party_attack_frame(PS4_PRECOMP_ALYS_SLASHER_TRACE_B_PIXELS_OFF,(unsigned)fb,PS4_PRECOMP_ALYS_SLASHER_TRACE_B_W,PS4_PRECOMP_ALYS_SLASHER_TRACE_B_H,trace_b_tile);
        for(unsigned side=0u;side<2u;side++){
            unsigned bit=side?2u:1u,base=16u+side*3u;if(!(mask&bit)){battle_attack_hide_range(base,3u);continue;}unsigned co=side?PS4_PRECOMP_ALYS_SLASHER_LEFT_COORDS_OFF:PS4_PRECOMP_ALYS_SLASHER_RIGHT_COORDS_OFF;unsigned sx=battle_coord_word(co,f,0u),sy=battle_coord_word(co,f,1u);int flip=side?1:0;
            battle_obj_any(base,battle_source_x_to_gba(sx)+(int)PS4_PRECOMP_ALYS_SLASHER_THROW_CENTER_X-128,battle_source_y_to_gba(sy)+(int)PS4_PRECOMP_ALYS_SLASHER_THROW_CENTER_Y-128,PS4_PRECOMP_ALYS_SLASHER_THROW_W,PS4_PRECOMP_ALYS_SLASHER_THROW_H,throw_tile,flip,1,1);
            if(fa>=0)battle_obj_any(base+1u,trace_x+(int)PS4_PRECOMP_ALYS_SLASHER_TRACE_A_CENTER_X-128,trace_y+(int)PS4_PRECOMP_ALYS_SLASHER_TRACE_A_CENTER_Y-128,PS4_PRECOMP_ALYS_SLASHER_TRACE_A_W,PS4_PRECOMP_ALYS_SLASHER_TRACE_A_H,trace_a_tile,flip,1,1);else hw_hide_obj(base+1u);
            if(fb>=0)battle_obj_any(base+2u,trace_x+(int)PS4_PRECOMP_ALYS_SLASHER_TRACE_B_CENTER_X-128,trace_y+(int)PS4_PRECOMP_ALYS_SLASHER_TRACE_B_CENTER_Y-128,PS4_PRECOMP_ALYS_SLASHER_TRACE_B_W,PS4_PRECOMP_ALYS_SLASHER_TRACE_B_H,trace_b_tile,flip,1,1);else hw_hide_obj(base+2u);
        }
        battle_action_idle_after_wait(enemies);
    }
    battle_attack_hide_range(16u,6u);
    /* BattleObj_SlasherAttack2: PLC $C3 overwrites tiles 0..$83, while the
     * $84+ tail of the already-loaded weapon PLC $77 remains resident.  The
     * main slasher and its trail both cycle four source frames while crossing
     * the full formation at $10 source pixels per update. */
    const unsigned sweep_tile=BATTLE_ATTACK_OBJ_TILE,trail_tile=BATTLE_ATTACK_OBJ_TILE+16u;
    for(unsigned t=0u;t<64u;t++){
        unsigned fr=t&3u;waitvb();battle_upload_party_attack_frame(PS4_PRECOMP_ALYS_SLASHER_SWEEP_PIXELS_OFF,fr,PS4_PRECOMP_ALYS_SLASHER_SWEEP_W,PS4_PRECOMP_ALYS_SLASHER_SWEEP_H,sweep_tile);battle_upload_party_attack_frame(PS4_PRECOMP_ALYS_SLASHER_TRAIL_PIXELS_OFF,fr,PS4_PRECOMP_ALYS_SLASHER_TRAIL_W,PS4_PRECOMP_ALYS_SLASHER_TRAIL_H,trail_tile);
        for(unsigned side=0u;side<2u;side++){unsigned bit=side?2u:1u,base=16u+side*2u;if(!(mask&bit)){battle_attack_hide_range(base,2u);continue;}int sx=side?(-0xE0+(int)(t+1u)*0x10):(0x320-(int)(t+1u)*0x10);
            int hit=battle_slasher_hit_target(battle_enemy_count,sx);
            if(hit>=0&&(battle_slasher_target_mask&(1u<<(unsigned)hit)))ram8w(0x500Au,(battle_slasher_hit_mask&(1u<<(unsigned)hit))?0xF5u:0xB8u);
            int gx=battle_source_x_to_gba_signed(sx),gy=battle_source_y_to_gba(0xD8u),flip=side?1:0;battle_obj_any(base,gx+(int)PS4_PRECOMP_ALYS_SLASHER_SWEEP_CENTER_X-128,gy+(int)PS4_PRECOMP_ALYS_SLASHER_SWEEP_CENTER_Y-128,PS4_PRECOMP_ALYS_SLASHER_SWEEP_W,PS4_PRECOMP_ALYS_SLASHER_SWEEP_H,sweep_tile,flip,1,1);battle_obj_any(base+1u,gx+(int)PS4_PRECOMP_ALYS_SLASHER_TRAIL_CENTER_X-128,gy+(int)PS4_PRECOMP_ALYS_SLASHER_TRAIL_CENTER_Y-128,PS4_PRECOMP_ALYS_SLASHER_TRAIL_W,PS4_PRECOMP_ALYS_SLASHER_TRAIL_H,trail_tile,flip,1,1);}
        battle_action_idle_after_wait(enemies);
    }
    battle_attack_hide_range(16u,4u);battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_demi_impacter_source_attack(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* c){
    if(actor>=pn||enemy_target>=battle_enemy_count)return 0;int clean_valid=0;battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=PS4_PRECOMP_PARTY_DEMI_POSE_LOAD_GUN_INDEX;
    if(!battle_present_actor_pose_dirty(party,pn,enemies,enemy_target,actor,c,&clean_valid,1))battle_present_action(party,pn,enemies,enemy_target,0u,actor,c,0u);
    if(battle_action_hold_track_fb(enemies,PS4_PRECOMP_DEMI_IMPACTER_BODY_WINDUP))clean_valid=0;
    battle_party_pose_index=PS4_PRECOMP_PARTY_DEMI_POSE_FIRE_GUN_INDEX;
    if(!battle_present_actor_pose_dirty(party,pn,enemies,enemy_target,actor,c,&clean_valid,0))battle_present_action(party,pn,enemies,enemy_target,0u,actor,c,0u);battle_action_hold(enemies,PS4_PRECOMP_DEMI_IMPACTER_SHOT_DELAY);ram8w(0x500Au,0xB6u);
    int ax=battle_enemy_anchor_x(enemy_target),ay=battle_enemy_anchor_y(enemy_target);const uint8_t* sh=ps4_precomp_assets+PS4_PRECOMP_DEMI_IMPACTER_SHOT_HOLDS_OFF;
    for(unsigned f=0u;f<PS4_PRECOMP_DEMI_IMPACTER_SHOT_FRAMES;f++){waitvb();battle_upload_party_attack_frame(PS4_PRECOMP_DEMI_IMPACTER_SHOT_PIXELS_OFF,f,PS4_PRECOMP_DEMI_IMPACTER_SHOT_W,PS4_PRECOMP_DEMI_IMPACTER_SHOT_H,BATTLE_ATTACK_OBJ_TILE);battle_obj_any(BATTLE_ATTACK_OBJ0,ax+(int)PS4_PRECOMP_DEMI_IMPACTER_SHOT_CENTER_X-128,ay+(int)PS4_PRECOMP_DEMI_IMPACTER_SHOT_CENTER_Y-128,PS4_PRECOMP_DEMI_IMPACTER_SHOT_W,PS4_PRECOMP_DEMI_IMPACTER_SHOT_H,BATTLE_ATTACK_OBJ_TILE,0,1,1);battle_action_idle_after_wait(enemies);unsigned hold=sh[f];if(hold>1u)battle_action_hold(enemies,hold-1u);}
    ram8w(0x500Au,0xBEu);const uint8_t* ih=ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_IMPACT_HOLDS_OFF;for(unsigned f=0u;f<PS4_PRECOMP_JURAFA_MICRO_IMPACT_FRAMES;f++){waitvb();battle_upload_party_attack_frame(PS4_PRECOMP_JURAFA_MICRO_IMPACT_PIXELS_OFF,f,PS4_PRECOMP_JURAFA_MICRO_IMPACT_W,PS4_PRECOMP_JURAFA_MICRO_IMPACT_H,BATTLE_ATTACK_OBJ_TILE);battle_obj_any(BATTLE_ATTACK_OBJ0,ax,ay,PS4_PRECOMP_JURAFA_MICRO_IMPACT_W,PS4_PRECOMP_JURAFA_MICRO_IMPACT_H,BATTLE_ATTACK_OBJ_TILE,0,1,1);battle_action_idle_after_wait(enemies);unsigned hold=ih[f];if(hold>1u)battle_action_hold(enemies,hold-1u);}
    hw_hide_obj(BATTLE_ATTACK_OBJ0);battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
/* v0.9.8 source technique presentation.  These use the same legal upper Mode-4
 * OBJ VRAM scratch window as enemy attack BattleObjs.  Foi needs at most a
 * 64x64 cast/release bank plus independent 32x32 orbit/impact banks; healing
 * needs exactly two 64x64 banks, so both fit the 8 KiB tile window. */
#define BATTLE_TECH_CAST_OBJ 40u
#define BATTLE_TECH_ORBIT_OBJ 41u
#define BATTLE_TECH_IMPACT_OBJ 42u
#define BATTLE_TECH_RELEASE_OBJ 43u
#define BATTLE_TECH_HEAL_OBJ0 50u
#define BATTLE_TECH_CAST_TILE 768u
#define BATTLE_TECH_SMALL0_TILE 896u
#define BATTLE_TECH_SMALL1_TILE 928u
#define BATTLE_TECH_SMALL2_TILE 960u
#define BATTLE_TECH_HEAL_EFFECT_TILE 896u
#define BATTLE_NAFOI_CAST1_TILE 768u
#define BATTLE_NAFOI_CAST2_TILE 832u
#define BATTLE_NAFOI_CAST3_TILE 848u
#define BATTLE_NAFOI_EFFECT1_TILE 864u
#define BATTLE_NAFOI_EFFECT2_TILE 928u
#define BATTLE_NAFOI_EFFECT3_TILE 992u
#define BATTLE_NATHU_CHILD_TILE 864u
#define BATTLE_NATHU_PARENT1_TILE 928u
#define BATTLE_NATHU_PARENT2_TILE 992u
#define BATTLE_NATHU_PARENT3_TILE 1000u
static void battle_tech_hide_all(void){for(unsigned i=40u;i<55u;i++)hw_hide_obj(i);}
static unsigned battle_tech_holds_total(unsigned off,unsigned n){unsigned t=0u;for(unsigned i=0u;i<n;i++)t+=ps4_precomp_assets[off+i];return t;}
static void battle_tech_idle_tick(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){battle_action_idle_after_wait(enemies);}
static void battle_tech_upload(unsigned off,unsigned frame,unsigned w,unsigned h,unsigned tile){battle_obj_upload_attack(ps4_precomp_assets+off+frame*w*h,w*h,tile);}
static int battle_party_foi_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id!=PS4_PRECOMP_TECH_FOI_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);
    /* Character_DoTech begins with the source 0x1E action timer before the
     * main technique object graph is allowed to run. */
    battle_action_hold(enemies,30u);ram8w(0x500Au,0xBBu);
    const uint8_t* cast_h=ps4_precomp_assets+PS4_PRECOMP_FOI_CAST_HOLDS_OFF;
    const uint8_t* orbit_h=ps4_precomp_assets+PS4_PRECOMP_FOI_ORBIT_HOLDS_OFF;
    const uint8_t* impact_h=ps4_precomp_assets+PS4_PRECOMP_FOI_IMPACT_HOLDS_OFF;
    const uint8_t* release_h=ps4_precomp_assets+PS4_PRECOMP_FOI_RELEASE_HOLDS_OFF;
    const uint8_t* orbit_hidden=ps4_precomp_assets+PS4_PRECOMP_FOI_ORBIT_HIDDEN_OFF;
    unsigned cast_len=battle_tech_holds_total(PS4_PRECOMP_FOI_CAST_HOLDS_OFF,PS4_PRECOMP_FOI_CAST_FRAMES);
    unsigned orbit_start=12u,orbit_len=battle_tech_holds_total(PS4_PRECOMP_FOI_ORBIT_HOLDS_OFF,PS4_PRECOMP_FOI_ORBIT_FRAMES);
    unsigned impact_start=orbit_start+orbit_len+8u;
    unsigned release_start=impact_start+8u;
    unsigned impact_len=battle_tech_holds_total(PS4_PRECOMP_FOI_IMPACT_HOLDS_OFF,PS4_PRECOMP_FOI_IMPACT_FRAMES);
    unsigned release_len=battle_tech_holds_total(PS4_PRECOMP_FOI_RELEASE_HOLDS_OFF,PS4_PRECOMP_FOI_RELEASE_FRAMES);
    unsigned total=impact_start+impact_len;if(release_start+release_len>total)total=release_start+release_len;
    int ax=battle_enemy_anchor_x(*current_target),cast_y=battle_source_y_to_gba(0x100u),attack_y=battle_source_y_to_gba(0xD8u);int executed=0;
    for(unsigned t=0u;t<total;t++){
        waitvb();if(t==impact_start)ram8w(0x500Au,0xBEu);
        int cf=t<cast_len?battle_timed_frame(t,cast_h,PS4_PRECOMP_FOI_CAST_FRAMES):-1;
        if(cf>=0){battle_tech_upload(PS4_PRECOMP_FOI_CAST_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_FOI_CAST_W,PS4_PRECOMP_FOI_CAST_H,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_CAST_OBJ,ax,cast_y,PS4_PRECOMP_FOI_CAST_W,PS4_PRECOMP_FOI_CAST_H,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_CAST_OBJ);
        int of=t>=orbit_start&&t<orbit_start+orbit_len?battle_timed_frame(t-orbit_start,orbit_h,PS4_PRECOMP_FOI_ORBIT_FRAMES):-1;
        if(of>=0&&!orbit_hidden[of]){battle_tech_upload(PS4_PRECOMP_FOI_ORBIT_PIXELS_OFF,(unsigned)of,PS4_PRECOMP_FOI_ORBIT_W,PS4_PRECOMP_FOI_ORBIT_H,BATTLE_TECH_SMALL0_TILE);battle_obj_any(BATTLE_TECH_ORBIT_OBJ,ax,attack_y,PS4_PRECOMP_FOI_ORBIT_W,PS4_PRECOMP_FOI_ORBIT_H,BATTLE_TECH_SMALL0_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_ORBIT_OBJ);
        int inf=t>=impact_start?battle_timed_frame(t-impact_start,impact_h,PS4_PRECOMP_FOI_IMPACT_FRAMES):-1;
        if(inf>=0){battle_tech_upload(PS4_PRECOMP_FOI_IMPACT_PIXELS_OFF,(unsigned)inf,PS4_PRECOMP_FOI_IMPACT_W,PS4_PRECOMP_FOI_IMPACT_H,BATTLE_TECH_SMALL1_TILE);battle_obj_any(BATTLE_TECH_IMPACT_OBJ,ax,attack_y,PS4_PRECOMP_FOI_IMPACT_W,PS4_PRECOMP_FOI_IMPACT_H,BATTLE_TECH_SMALL1_TILE,0,0,1);}else hw_hide_obj(BATTLE_TECH_IMPACT_OBJ);
        int rf=t>=release_start?battle_timed_frame(t-release_start,release_h,PS4_PRECOMP_FOI_RELEASE_FRAMES):-1;
        if(rf>=0){battle_tech_upload(PS4_PRECOMP_FOI_RELEASE_PIXELS_OFF,(unsigned)rf,PS4_PRECOMP_FOI_RELEASE_W,PS4_PRECOMP_FOI_RELEASE_H,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_RELEASE_OBJ,ax,attack_y,PS4_PRECOMP_FOI_RELEASE_W,PS4_PRECOMP_FOI_RELEASE_H,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_RELEASE_OBJ);
        /* Foi_AdvanceParentAction advances the character two updates after the
         * impact object is spawned; apply the translated ability at that same
         * visible hit point. */
        if(!executed&&t==impact_start+2u){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;}
        battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);
    battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_gifoi_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id!=PS4_PRECOMP_TECH_GIFOI_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,30u);ram8w(0x500Au,0xBBu);
    const uint8_t* cast_h=ps4_precomp_assets+PS4_PRECOMP_FOI_CAST_HOLDS_OFF;const uint8_t* orbit_h=ps4_precomp_assets+PS4_PRECOMP_FOI_ORBIT_HOLDS_OFF;const uint8_t* impact_h=ps4_precomp_assets+PS4_PRECOMP_FOI_IMPACT_HOLDS_OFF;const uint8_t* release_h=ps4_precomp_assets+PS4_PRECOMP_FOI_RELEASE_HOLDS_OFF;const uint8_t* orbit_hidden=ps4_precomp_assets+PS4_PRECOMP_FOI_ORBIT_HIDDEN_OFF;
    unsigned cast_len=battle_tech_holds_total(PS4_PRECOMP_FOI_CAST_HOLDS_OFF,PS4_PRECOMP_FOI_CAST_FRAMES),orbit_start=12u,orbit_len=battle_tech_holds_total(PS4_PRECOMP_FOI_ORBIT_HOLDS_OFF,PS4_PRECOMP_FOI_ORBIT_FRAMES),impact_start=orbit_start+orbit_len+8u,release_start=impact_start+8u,impact_len=battle_tech_holds_total(PS4_PRECOMP_FOI_IMPACT_HOLDS_OFF,PS4_PRECOMP_FOI_IMPACT_FRAMES),release_len=battle_tech_holds_total(PS4_PRECOMP_FOI_RELEASE_HOLDS_OFF,PS4_PRECOMP_FOI_RELEASE_FRAMES),total=impact_start+impact_len;if(release_start+release_len>total)total=release_start+release_len;
    int base_x=battle_enemy_anchor_x(*current_target);int32_t fx[3]={((int32_t)base_x)<<16,((int32_t)(base_x-26))<<16,((int32_t)(base_x+26))<<16};int32_t fy[3]={((int32_t)battle_source_y_to_gba(0x100u))<<16,((int32_t)battle_source_y_to_gba(0xC4u))<<16,((int32_t)battle_source_y_to_gba(0xC4u))<<16};
    /* Source 16.16 speeds scaled by the static renderer's 3/4 Genesis->GBA
     * transform: middle rises 1 source px/update; side shots converge while
     * descending by 1/2 source px/update. */
    static const int32_t vx[3]={0,0xA2C0,-0xA2C0};static const int32_t vy[3]={-0xC000,0x6000,0x6000};int executed=0;
    for(unsigned t=0u;t<total;t++){
        waitvb();if(t==impact_start)ram8w(0x500Au,0xBEu);int cf=t<cast_len?battle_timed_frame(t,cast_h,PS4_PRECOMP_FOI_CAST_FRAMES):-1;if(cf>=0){battle_tech_upload(PS4_PRECOMP_FOI_CAST_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_FOI_CAST_W,PS4_PRECOMP_FOI_CAST_H,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_CAST_OBJ,base_x,battle_source_y_to_gba(0x100u),PS4_PRECOMP_FOI_CAST_W,PS4_PRECOMP_FOI_CAST_H,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_CAST_OBJ);
        int of=t>=orbit_start&&t<orbit_start+orbit_len?battle_timed_frame(t-orbit_start,orbit_h,PS4_PRECOMP_FOI_ORBIT_FRAMES):-1;if(of>=0&&!orbit_hidden[of])battle_tech_upload(PS4_PRECOMP_FOI_ORBIT_PIXELS_OFF,(unsigned)of,PS4_PRECOMP_FOI_ORBIT_W,PS4_PRECOMP_FOI_ORBIT_H,BATTLE_TECH_SMALL0_TILE);
        int inf=t>=impact_start?battle_timed_frame(t-impact_start,impact_h,PS4_PRECOMP_FOI_IMPACT_FRAMES):-1;if(inf>=0)battle_tech_upload(PS4_PRECOMP_FOI_IMPACT_PIXELS_OFF,(unsigned)inf,PS4_PRECOMP_FOI_IMPACT_W,PS4_PRECOMP_FOI_IMPACT_H,BATTLE_TECH_SMALL1_TILE);
        int rf=t>=release_start?battle_timed_frame(t-release_start,release_h,PS4_PRECOMP_FOI_RELEASE_FRAMES):-1;if(rf>=0)battle_tech_upload(PS4_PRECOMP_FOI_RELEASE_PIXELS_OFF,(unsigned)rf,PS4_PRECOMP_FOI_RELEASE_W,PS4_PRECOMP_FOI_RELEASE_H,BATTLE_TECH_CAST_TILE);
        for(unsigned j=0u;j<3u;j++){int x=(int)(fx[j]>>16),y=(int)(fy[j]>>16);unsigned oo=41u+j,io=44u+j,ro=47u+j;if(of>=0&&!orbit_hidden[of])battle_obj_any(oo,x,y,PS4_PRECOMP_FOI_ORBIT_W,PS4_PRECOMP_FOI_ORBIT_H,BATTLE_TECH_SMALL0_TILE,0,1,1);else hw_hide_obj(oo);if(inf>=0)battle_obj_any(io,x,y,PS4_PRECOMP_FOI_IMPACT_W,PS4_PRECOMP_FOI_IMPACT_H,BATTLE_TECH_SMALL1_TILE,0,0,1);else hw_hide_obj(io);if(rf>=0)battle_obj_any(ro,x,y,PS4_PRECOMP_FOI_RELEASE_W,PS4_PRECOMP_FOI_RELEASE_H,BATTLE_TECH_CAST_TILE,0,1,1);else hw_hide_obj(ro);if(t>=orbit_start){fx[j]+=vx[j];fy[j]+=vy[j];}}
        if(!executed&&t==impact_start+2u){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;}battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_nafoi_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id!=PS4_PRECOMP_TECH_NAFOI_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,30u);ram8w(0x500Au,0xBBu);
    const uint8_t* cast_h=ps4_precomp_assets+PS4_PRECOMP_NAFOI_CAST_HOLDS_OFF;const uint8_t* charge_h=ps4_precomp_assets+PS4_PRECOMP_NAFOI_CHARGE_HOLDS_OFF;const uint8_t* attack_h=ps4_precomp_assets+PS4_PRECOMP_NAFOI_ATTACK_HOLDS_OFF;
    unsigned cast_len=battle_tech_holds_total(PS4_PRECOMP_NAFOI_CAST_HOLDS_OFF,PS4_PRECOMP_NAFOI_CAST_FRAMES),charge_len=battle_tech_holds_total(PS4_PRECOMP_NAFOI_CHARGE_HOLDS_OFF,PS4_PRECOMP_NAFOI_CHARGE_FRAMES),attack_start=charge_len+8u,attack_len=battle_tech_holds_total(PS4_PRECOMP_NAFOI_ATTACK_HOLDS_OFF,PS4_PRECOMP_NAFOI_ATTACK_FRAMES),total=attack_start+attack_len;if(cast_len>total)total=cast_len;
    int ax=battle_enemy_anchor_x(*current_target),cast_y=battle_source_y_to_gba(0x100u),attack_y=battle_source_y_to_gba(0xD8u);int executed=0;
    for(unsigned t=0u;t<total;t++){
        waitvb();if(t==attack_start)ram8w(0x500Au,0xC3u);int cf=t<cast_len?battle_timed_frame(t,cast_h,PS4_PRECOMP_NAFOI_CAST_FRAMES):-1;
        if(cf>=0){battle_tech_upload(PS4_PRECOMP_NAFOI_CAST_PART1_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_NAFOI_CAST_PART1_W,PS4_PRECOMP_NAFOI_CAST_PART1_H,BATTLE_NAFOI_CAST1_TILE);battle_tech_upload(PS4_PRECOMP_NAFOI_CAST_PART2_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_NAFOI_CAST_PART2_W,PS4_PRECOMP_NAFOI_CAST_PART2_H,BATTLE_NAFOI_CAST2_TILE);battle_tech_upload(PS4_PRECOMP_NAFOI_CAST_PART3_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_NAFOI_CAST_PART3_W,PS4_PRECOMP_NAFOI_CAST_PART3_H,BATTLE_NAFOI_CAST3_TILE);battle_obj_any(40u,ax+(int)PS4_PRECOMP_NAFOI_CAST_PART1_CENTER_X-128,cast_y+(int)PS4_PRECOMP_NAFOI_CAST_PART1_CENTER_Y-128,PS4_PRECOMP_NAFOI_CAST_PART1_W,PS4_PRECOMP_NAFOI_CAST_PART1_H,BATTLE_NAFOI_CAST1_TILE,0,1,1);battle_obj_any(41u,ax+(int)PS4_PRECOMP_NAFOI_CAST_PART2_CENTER_X-128,cast_y+(int)PS4_PRECOMP_NAFOI_CAST_PART2_CENTER_Y-128,PS4_PRECOMP_NAFOI_CAST_PART2_W,PS4_PRECOMP_NAFOI_CAST_PART2_H,BATTLE_NAFOI_CAST2_TILE,0,1,1);battle_obj_any(42u,ax+(int)PS4_PRECOMP_NAFOI_CAST_PART3_CENTER_X-128,cast_y+(int)PS4_PRECOMP_NAFOI_CAST_PART3_CENTER_Y-128,PS4_PRECOMP_NAFOI_CAST_PART3_W,PS4_PRECOMP_NAFOI_CAST_PART3_H,BATTLE_NAFOI_CAST3_TILE,0,1,1);}else{hw_hide_obj(40u);hw_hide_obj(41u);hw_hide_obj(42u);}
        int ef=-1;int charge_visible=0;if(t<charge_len){ef=battle_timed_frame(t,charge_h,PS4_PRECOMP_NAFOI_CHARGE_FRAMES);charge_visible=(t&1u)!=0u;}else if(t>=attack_start)ef=battle_timed_frame(t-attack_start,attack_h,PS4_PRECOMP_NAFOI_ATTACK_FRAMES);
        if(ef>=0&&(t>=attack_start||charge_visible)){
            unsigned o1,o2,o3,w1,h1,w2,h2,w3,h3,cx1,cy1,cx2,cy2,cx3,cy3;if(t>=attack_start){o1=PS4_PRECOMP_NAFOI_ATTACK_PART1_PIXELS_OFF;o2=PS4_PRECOMP_NAFOI_ATTACK_PART2_PIXELS_OFF;o3=PS4_PRECOMP_NAFOI_ATTACK_PART3_PIXELS_OFF;w1=PS4_PRECOMP_NAFOI_ATTACK_PART1_W;h1=PS4_PRECOMP_NAFOI_ATTACK_PART1_H;w2=PS4_PRECOMP_NAFOI_ATTACK_PART2_W;h2=PS4_PRECOMP_NAFOI_ATTACK_PART2_H;w3=PS4_PRECOMP_NAFOI_ATTACK_PART3_W;h3=PS4_PRECOMP_NAFOI_ATTACK_PART3_H;cx1=PS4_PRECOMP_NAFOI_ATTACK_PART1_CENTER_X;cy1=PS4_PRECOMP_NAFOI_ATTACK_PART1_CENTER_Y;cx2=PS4_PRECOMP_NAFOI_ATTACK_PART2_CENTER_X;cy2=PS4_PRECOMP_NAFOI_ATTACK_PART2_CENTER_Y;cx3=PS4_PRECOMP_NAFOI_ATTACK_PART3_CENTER_X;cy3=PS4_PRECOMP_NAFOI_ATTACK_PART3_CENTER_Y;}else{o1=PS4_PRECOMP_NAFOI_CHARGE_PART1_PIXELS_OFF;o2=PS4_PRECOMP_NAFOI_CHARGE_PART2_PIXELS_OFF;o3=PS4_PRECOMP_NAFOI_CHARGE_PART3_PIXELS_OFF;w1=PS4_PRECOMP_NAFOI_CHARGE_PART1_W;h1=PS4_PRECOMP_NAFOI_CHARGE_PART1_H;w2=PS4_PRECOMP_NAFOI_CHARGE_PART2_W;h2=PS4_PRECOMP_NAFOI_CHARGE_PART2_H;w3=PS4_PRECOMP_NAFOI_CHARGE_PART3_W;h3=PS4_PRECOMP_NAFOI_CHARGE_PART3_H;cx1=PS4_PRECOMP_NAFOI_CHARGE_PART1_CENTER_X;cy1=PS4_PRECOMP_NAFOI_CHARGE_PART1_CENTER_Y;cx2=PS4_PRECOMP_NAFOI_CHARGE_PART2_CENTER_X;cy2=PS4_PRECOMP_NAFOI_CHARGE_PART2_CENTER_Y;cx3=PS4_PRECOMP_NAFOI_CHARGE_PART3_CENTER_X;cy3=PS4_PRECOMP_NAFOI_CHARGE_PART3_CENTER_Y;}battle_tech_upload(o1,(unsigned)ef,w1,h1,BATTLE_NAFOI_EFFECT1_TILE);battle_tech_upload(o2,(unsigned)ef,w2,h2,BATTLE_NAFOI_EFFECT2_TILE);battle_tech_upload(o3,(unsigned)ef,w3,h3,BATTLE_NAFOI_EFFECT3_TILE);battle_obj_any(43u,ax+(int)cx1-128,attack_y+(int)cy1-128,w1,h1,BATTLE_NAFOI_EFFECT1_TILE,0,1,1);battle_obj_any(44u,ax+(int)cx2-128,attack_y+(int)cy2-128,w2,h2,BATTLE_NAFOI_EFFECT2_TILE,0,1,1);battle_obj_any(45u,ax+(int)cx3-128,attack_y+(int)cy3-128,w3,h3,BATTLE_NAFOI_EFFECT3_TILE,0,1,1);
        }else{hw_hide_obj(43u);hw_hide_obj(44u);hw_hide_obj(45u);}
        if(!executed&&t==attack_start+2u){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;}battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_wat_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id!=PS4_PRECOMP_TECH_WAT_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,30u);ram8w(0x500Au,0xBBu);
    const uint8_t* cast_h=ps4_precomp_assets+PS4_PRECOMP_WAT_CAST_HOLDS_OFF;const uint8_t* orbit_h=ps4_precomp_assets+PS4_PRECOMP_WAT_ORBIT_HOLDS_OFF;const uint8_t* impact_h=ps4_precomp_assets+PS4_PRECOMP_WAT_IMPACT_HOLDS_OFF;const uint8_t* release_h=ps4_precomp_assets+PS4_PRECOMP_WAT_RELEASE_HOLDS_OFF;const uint8_t* orbit_hidden=ps4_precomp_assets+PS4_PRECOMP_WAT_ORBIT_HIDDEN_OFF;
    unsigned cast_len=battle_tech_holds_total(PS4_PRECOMP_WAT_CAST_HOLDS_OFF,PS4_PRECOMP_WAT_CAST_FRAMES),orbit_start=12u,orbit_len=battle_tech_holds_total(PS4_PRECOMP_WAT_ORBIT_HOLDS_OFF,PS4_PRECOMP_WAT_ORBIT_FRAMES),impact_start=orbit_start+orbit_len+8u,release_start=impact_start+11u,impact_len=battle_tech_holds_total(PS4_PRECOMP_WAT_IMPACT_HOLDS_OFF,PS4_PRECOMP_WAT_IMPACT_FRAMES),release_len=battle_tech_holds_total(PS4_PRECOMP_WAT_RELEASE_HOLDS_OFF,PS4_PRECOMP_WAT_RELEASE_FRAMES),total=impact_start+impact_len;if(release_start+release_len>total)total=release_start+release_len;
    int ax=battle_enemy_anchor_x(*current_target),cast_y=battle_source_y_to_gba(0x100u),attack_y=battle_source_y_to_gba(0xD8u);int executed=0;
    for(unsigned t=0u;t<total;t++){
        waitvb();if(t==impact_start)ram8w(0x500Au,0xC4u);int cf=t<cast_len?battle_timed_frame(t,cast_h,PS4_PRECOMP_WAT_CAST_FRAMES):-1;if(cf>=0){battle_tech_upload(PS4_PRECOMP_WAT_CAST_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_WAT_CAST_W,PS4_PRECOMP_WAT_CAST_H,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_CAST_OBJ,ax,cast_y,PS4_PRECOMP_WAT_CAST_W,PS4_PRECOMP_WAT_CAST_H,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_CAST_OBJ);
        int of=t>=orbit_start&&t<orbit_start+orbit_len?battle_timed_frame(t-orbit_start,orbit_h,PS4_PRECOMP_WAT_ORBIT_FRAMES):-1;if(of>=0&&!orbit_hidden[of]){battle_tech_upload(PS4_PRECOMP_WAT_ORBIT_PIXELS_OFF,(unsigned)of,PS4_PRECOMP_WAT_ORBIT_W,PS4_PRECOMP_WAT_ORBIT_H,BATTLE_TECH_SMALL0_TILE);battle_obj_any(BATTLE_TECH_ORBIT_OBJ,ax,attack_y,PS4_PRECOMP_WAT_ORBIT_W,PS4_PRECOMP_WAT_ORBIT_H,BATTLE_TECH_SMALL0_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_ORBIT_OBJ);
        int inf=t>=impact_start?battle_timed_frame(t-impact_start,impact_h,PS4_PRECOMP_WAT_IMPACT_FRAMES):-1;if(inf>=0){battle_tech_upload(PS4_PRECOMP_WAT_IMPACT_PIXELS_OFF,(unsigned)inf,PS4_PRECOMP_WAT_IMPACT_W,PS4_PRECOMP_WAT_IMPACT_H,BATTLE_TECH_SMALL1_TILE);battle_obj_any(BATTLE_TECH_IMPACT_OBJ,ax,attack_y,PS4_PRECOMP_WAT_IMPACT_W,PS4_PRECOMP_WAT_IMPACT_H,BATTLE_TECH_SMALL1_TILE,0,0,1);}else hw_hide_obj(BATTLE_TECH_IMPACT_OBJ);
        int rf=t>=release_start?battle_timed_frame(t-release_start,release_h,PS4_PRECOMP_WAT_RELEASE_FRAMES):-1;if(rf>=0){battle_tech_upload(PS4_PRECOMP_WAT_RELEASE_PIXELS_OFF,(unsigned)rf,PS4_PRECOMP_WAT_RELEASE_W,PS4_PRECOMP_WAT_RELEASE_H,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_RELEASE_OBJ,ax,attack_y,PS4_PRECOMP_WAT_RELEASE_W,PS4_PRECOMP_WAT_RELEASE_H,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_RELEASE_OBJ);
        if(!executed&&t==impact_start+2u){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;}battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_tsu_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id!=PS4_PRECOMP_TECH_TSU_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,30u);ram8w(0x500Au,0xBBu);ram8w(0x500Au,0xC5u);
    const uint8_t* cast_h=ps4_precomp_assets+PS4_PRECOMP_TSU_CAST_HOLDS_OFF;const uint8_t* attack_h=ps4_precomp_assets+PS4_PRECOMP_TSU_ATTACK_HOLDS_OFF;
    unsigned cast_len=battle_tech_holds_total(PS4_PRECOMP_TSU_CAST_HOLDS_OFF,PS4_PRECOMP_TSU_CAST_FRAMES),attack_len=battle_tech_holds_total(PS4_PRECOMP_TSU_ATTACK_HOLDS_OFF,PS4_PRECOMP_TSU_ATTACK_FRAMES),total=cast_len;if(attack_len>total)total=attack_len;
    int ax=battle_enemy_anchor_x(*current_target),cast_y=battle_source_y_to_gba(0x100u),attack_y=battle_source_y_to_gba(0xD8u);int executed=0;
    for(unsigned t=0u;t<total;t++){
        waitvb();int cf=t<cast_len?battle_timed_frame(t,cast_h,PS4_PRECOMP_TSU_CAST_FRAMES):-1;if(cf>=0){battle_tech_upload(PS4_PRECOMP_TSU_CAST_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_TSU_CAST_W,PS4_PRECOMP_TSU_CAST_H,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_CAST_OBJ,ax,cast_y,PS4_PRECOMP_TSU_CAST_W,PS4_PRECOMP_TSU_CAST_H,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_CAST_OBJ);
        int af=t<attack_len?battle_timed_frame(t,attack_h,PS4_PRECOMP_TSU_ATTACK_FRAMES):-1;if(af>=0){battle_tech_upload(PS4_PRECOMP_TSU_ATTACK_PIXELS_OFF,(unsigned)af,PS4_PRECOMP_TSU_ATTACK_W,PS4_PRECOMP_TSU_ATTACK_H,BATTLE_TECH_SMALL0_TILE);battle_obj_any(BATTLE_TECH_ORBIT_OBJ,ax+(int)PS4_PRECOMP_TSU_ATTACK_CENTER_X-128,attack_y+(int)PS4_PRECOMP_TSU_ATTACK_CENTER_Y-128,PS4_PRECOMP_TSU_ATTACK_W,PS4_PRECOMP_TSU_ATTACK_H,BATTLE_TECH_SMALL0_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_ORBIT_OBJ);
        /* BattleObj_Tsu starts with timer=2 and advances the parent character
         * to action routine 5 when that wait expires.  Apply the translated
         * technique at the same source-visible hit point. */
        if(!executed&&t==2u){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;}battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_githu_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id!=PS4_PRECOMP_TECH_GITHU_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,30u);ram8w(0x500Au,0xBBu);ram8w(0x500Au,0xC5u);
    const uint8_t* cast_h=ps4_precomp_assets+PS4_PRECOMP_GITHU_CAST_HOLDS_OFF;const uint8_t* child_h=ps4_precomp_assets+PS4_PRECOMP_GITHU_CHILD_HOLDS_OFF;const uint8_t* parent_h=ps4_precomp_assets+PS4_PRECOMP_GITHU_PARENT_HOLDS_OFF;
    unsigned cast_len=battle_tech_holds_total(PS4_PRECOMP_GITHU_CAST_HOLDS_OFF,PS4_PRECOMP_GITHU_CAST_FRAMES),child_len=battle_tech_holds_total(PS4_PRECOMP_GITHU_CHILD_HOLDS_OFF,PS4_PRECOMP_GITHU_CHILD_FRAMES),parent_start=2u,parent_len=battle_tech_holds_total(PS4_PRECOMP_GITHU_PARENT_HOLDS_OFF,PS4_PRECOMP_GITHU_PARENT_FRAMES),total=child_len;if(cast_len>total)total=cast_len;if(parent_start+parent_len>total)total=parent_start+parent_len;
    int ax=battle_enemy_anchor_x(*current_target),cast_y=battle_source_y_to_gba(0x100u),attack_y=battle_source_y_to_gba(0xD8u);int executed=0;
    for(unsigned t=0u;t<total;t++){
        waitvb();int cf=t<cast_len?battle_timed_frame(t,cast_h,PS4_PRECOMP_GITHU_CAST_FRAMES):-1;if(cf>=0){battle_tech_upload(PS4_PRECOMP_GITHU_CAST_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_GITHU_CAST_W,PS4_PRECOMP_GITHU_CAST_H,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_CAST_OBJ,ax,cast_y,PS4_PRECOMP_GITHU_CAST_W,PS4_PRECOMP_GITHU_CAST_H,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_CAST_OBJ);
        int ch=t<child_len?battle_timed_frame(t,child_h,PS4_PRECOMP_GITHU_CHILD_FRAMES):-1;if(ch>=0){battle_tech_upload(PS4_PRECOMP_GITHU_CHILD_PIXELS_OFF,(unsigned)ch,PS4_PRECOMP_GITHU_CHILD_W,PS4_PRECOMP_GITHU_CHILD_H,BATTLE_TECH_SMALL0_TILE);battle_obj_any(BATTLE_TECH_ORBIT_OBJ,ax+(int)PS4_PRECOMP_GITHU_CHILD_CENTER_X-128,attack_y+(int)PS4_PRECOMP_GITHU_CHILD_CENTER_Y-128,PS4_PRECOMP_GITHU_CHILD_W,PS4_PRECOMP_GITHU_CHILD_H,BATTLE_TECH_SMALL0_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_ORBIT_OBJ);
        int ph=t>=parent_start?battle_timed_frame(t-parent_start,parent_h,PS4_PRECOMP_GITHU_PARENT_FRAMES):-1;if(ph>=0){battle_tech_upload(PS4_PRECOMP_GITHU_PARENT_PIXELS_OFF,(unsigned)ph,PS4_PRECOMP_GITHU_PARENT_W,PS4_PRECOMP_GITHU_PARENT_H,BATTLE_TECH_SMALL2_TILE);battle_obj_any(BATTLE_TECH_IMPACT_OBJ,ax+(int)PS4_PRECOMP_GITHU_PARENT_CENTER_X-128,attack_y+(int)PS4_PRECOMP_GITHU_PARENT_CENTER_Y-128,PS4_PRECOMP_GITHU_PARENT_W,PS4_PRECOMP_GITHU_PARENT_H,BATTLE_TECH_SMALL2_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_IMPACT_OBJ);
        /* The $424 parent starts hidden for two updates, then Githu's source
         * routine clears the hidden flag and advances the character to the
         * effect phase. */
        if(!executed&&t==2u){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;}battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_nathu_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id!=PS4_PRECOMP_TECH_NATHU_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);if(*current_target>=battle_enemy_count)return 0;
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,30u);ram8w(0x500Au,0xBBu);ram8w(0x500Au,0xC5u);
    const uint8_t* cast_h=ps4_precomp_assets+PS4_PRECOMP_NATHU_CAST_HOLDS_OFF;const uint8_t* child_h=ps4_precomp_assets+PS4_PRECOMP_NATHU_CHILD_HOLDS_OFF;const uint8_t* parent_h=ps4_precomp_assets+PS4_PRECOMP_NATHU_PARENT_HOLDS_OFF;
    unsigned cast_len=battle_tech_holds_total(PS4_PRECOMP_NATHU_CAST_HOLDS_OFF,PS4_PRECOMP_NATHU_CAST_FRAMES),child_len=battle_tech_holds_total(PS4_PRECOMP_NATHU_CHILD_HOLDS_OFF,PS4_PRECOMP_NATHU_CHILD_FRAMES),parent_start=2u,parent_len=battle_tech_holds_total(PS4_PRECOMP_NATHU_PARENT_HOLDS_OFF,PS4_PRECOMP_NATHU_PARENT_FRAMES),total=child_len;if(cast_len>total)total=cast_len;if(parent_start+parent_len>total)total=parent_start+parent_len;
    int ax=battle_enemy_anchor_x(*current_target),cast_y=battle_source_y_to_gba(0x100u),attack_y=battle_source_y_to_gba(0xD8u);int executed=0;
    for(unsigned t=0u;t<total;t++){
        waitvb();int cf=t<cast_len?battle_timed_frame(t,cast_h,PS4_PRECOMP_NATHU_CAST_FRAMES):-1;
        if(cf>=0){battle_tech_upload(PS4_PRECOMP_NATHU_CAST_PART1_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_NATHU_CAST_PART1_W,PS4_PRECOMP_NATHU_CAST_PART1_H,BATTLE_NAFOI_CAST1_TILE);battle_tech_upload(PS4_PRECOMP_NATHU_CAST_PART2_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_NATHU_CAST_PART2_W,PS4_PRECOMP_NATHU_CAST_PART2_H,BATTLE_NAFOI_CAST2_TILE);battle_tech_upload(PS4_PRECOMP_NATHU_CAST_PART3_PIXELS_OFF,(unsigned)cf,PS4_PRECOMP_NATHU_CAST_PART3_W,PS4_PRECOMP_NATHU_CAST_PART3_H,BATTLE_NAFOI_CAST3_TILE);battle_obj_any(40u,ax+(int)PS4_PRECOMP_NATHU_CAST_PART1_CENTER_X-128,cast_y+(int)PS4_PRECOMP_NATHU_CAST_PART1_CENTER_Y-128,PS4_PRECOMP_NATHU_CAST_PART1_W,PS4_PRECOMP_NATHU_CAST_PART1_H,BATTLE_NAFOI_CAST1_TILE,0,1,1);battle_obj_any(41u,ax+(int)PS4_PRECOMP_NATHU_CAST_PART2_CENTER_X-128,cast_y+(int)PS4_PRECOMP_NATHU_CAST_PART2_CENTER_Y-128,PS4_PRECOMP_NATHU_CAST_PART2_W,PS4_PRECOMP_NATHU_CAST_PART2_H,BATTLE_NAFOI_CAST2_TILE,0,1,1);battle_obj_any(42u,ax+(int)PS4_PRECOMP_NATHU_CAST_PART3_CENTER_X-128,cast_y+(int)PS4_PRECOMP_NATHU_CAST_PART3_CENTER_Y-128,PS4_PRECOMP_NATHU_CAST_PART3_W,PS4_PRECOMP_NATHU_CAST_PART3_H,BATTLE_NAFOI_CAST3_TILE,0,1,1);}else{hw_hide_obj(40u);hw_hide_obj(41u);hw_hide_obj(42u);}
        int ch=t<child_len?battle_timed_frame(t,child_h,PS4_PRECOMP_NATHU_CHILD_FRAMES):-1;if(ch>=0){battle_tech_upload(PS4_PRECOMP_NATHU_CHILD_PIXELS_OFF,(unsigned)ch,PS4_PRECOMP_NATHU_CHILD_W,PS4_PRECOMP_NATHU_CHILD_H,BATTLE_NATHU_CHILD_TILE);battle_obj_any(43u,ax+(int)PS4_PRECOMP_NATHU_CHILD_CENTER_X-128,attack_y+(int)PS4_PRECOMP_NATHU_CHILD_CENTER_Y-128,PS4_PRECOMP_NATHU_CHILD_W,PS4_PRECOMP_NATHU_CHILD_H,BATTLE_NATHU_CHILD_TILE,0,1,1);}else hw_hide_obj(43u);
        int ph=t>=parent_start?battle_timed_frame(t-parent_start,parent_h,PS4_PRECOMP_NATHU_PARENT_FRAMES):-1;if(ph>=0){battle_tech_upload(PS4_PRECOMP_NATHU_PARENT_PART1_PIXELS_OFF,(unsigned)ph,PS4_PRECOMP_NATHU_PARENT_PART1_W,PS4_PRECOMP_NATHU_PARENT_PART1_H,BATTLE_NATHU_PARENT1_TILE);battle_tech_upload(PS4_PRECOMP_NATHU_PARENT_PART2_PIXELS_OFF,(unsigned)ph,PS4_PRECOMP_NATHU_PARENT_PART2_W,PS4_PRECOMP_NATHU_PARENT_PART2_H,BATTLE_NATHU_PARENT2_TILE);battle_tech_upload(PS4_PRECOMP_NATHU_PARENT_PART3_PIXELS_OFF,(unsigned)ph,PS4_PRECOMP_NATHU_PARENT_PART3_W,PS4_PRECOMP_NATHU_PARENT_PART3_H,BATTLE_NATHU_PARENT3_TILE);battle_obj_any(44u,ax+(int)PS4_PRECOMP_NATHU_PARENT_PART1_CENTER_X-128,attack_y+(int)PS4_PRECOMP_NATHU_PARENT_PART1_CENTER_Y-128,PS4_PRECOMP_NATHU_PARENT_PART1_W,PS4_PRECOMP_NATHU_PARENT_PART1_H,BATTLE_NATHU_PARENT1_TILE,0,1,1);battle_obj_any(45u,ax+(int)PS4_PRECOMP_NATHU_PARENT_PART2_CENTER_X-128,attack_y+(int)PS4_PRECOMP_NATHU_PARENT_PART2_CENTER_Y-128,PS4_PRECOMP_NATHU_PARENT_PART2_W,PS4_PRECOMP_NATHU_PARENT_PART2_H,BATTLE_NATHU_PARENT2_TILE,0,1,1);battle_obj_any(46u,ax+(int)PS4_PRECOMP_NATHU_PARENT_PART3_CENTER_X-128,attack_y+(int)PS4_PRECOMP_NATHU_PARENT_PART3_CENTER_Y-128,PS4_PRECOMP_NATHU_PARENT_PART3_W,PS4_PRECOMP_NATHU_PARENT_PART3_H,BATTLE_NATHU_PARENT3_TILE,0,1,1);}else{hw_hide_obj(44u);hw_hide_obj(45u);hw_hide_obj(46u);}
        if(!executed&&t==2u){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;}battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_heal_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(actor>=pn||c->id<PS4_PRECOMP_TECH_RES_ID||c->id>PS4_PRECOMP_TECH_NARES_MULTI_ID||!battle_action_has_resource(party,pn,actor,c))return 0;
    unsigned cast_off,effect_off,cast_holds,effect_holds,cast_frames,effect_frames,cast_w,cast_h,effect_w,effect_h;
    if(c->id==PS4_PRECOMP_TECH_RES_ID||c->id==PS4_PRECOMP_TECH_RES_MULTI_ID){cast_off=PS4_PRECOMP_RES_HEAL_CAST_PIXELS_OFF;effect_off=PS4_PRECOMP_RES_HEAL_EFFECT_PIXELS_OFF;cast_holds=PS4_PRECOMP_RES_HEAL_CAST_HOLDS_OFF;effect_holds=PS4_PRECOMP_RES_HEAL_EFFECT_HOLDS_OFF;cast_frames=PS4_PRECOMP_RES_HEAL_CAST_FRAMES;effect_frames=PS4_PRECOMP_RES_HEAL_EFFECT_FRAMES;cast_w=PS4_PRECOMP_RES_HEAL_CAST_W;cast_h=PS4_PRECOMP_RES_HEAL_CAST_H;effect_w=PS4_PRECOMP_RES_HEAL_EFFECT_W;effect_h=PS4_PRECOMP_RES_HEAL_EFFECT_H;}
    else if(c->id==PS4_PRECOMP_TECH_GIRES_ID||c->id==PS4_PRECOMP_TECH_GIRES_MULTI_ID){cast_off=PS4_PRECOMP_GIRES_HEAL_CAST_PIXELS_OFF;effect_off=PS4_PRECOMP_GIRES_HEAL_EFFECT_PIXELS_OFF;cast_holds=PS4_PRECOMP_GIRES_HEAL_CAST_HOLDS_OFF;effect_holds=PS4_PRECOMP_GIRES_HEAL_EFFECT_HOLDS_OFF;cast_frames=PS4_PRECOMP_GIRES_HEAL_CAST_FRAMES;effect_frames=PS4_PRECOMP_GIRES_HEAL_EFFECT_FRAMES;cast_w=PS4_PRECOMP_GIRES_HEAL_CAST_W;cast_h=PS4_PRECOMP_GIRES_HEAL_CAST_H;effect_w=PS4_PRECOMP_GIRES_HEAL_EFFECT_W;effect_h=PS4_PRECOMP_GIRES_HEAL_EFFECT_H;}
    else {cast_off=PS4_PRECOMP_NARES_HEAL_CAST_PIXELS_OFF;effect_off=PS4_PRECOMP_NARES_HEAL_EFFECT_PIXELS_OFF;cast_holds=PS4_PRECOMP_NARES_HEAL_CAST_HOLDS_OFF;effect_holds=PS4_PRECOMP_NARES_HEAL_EFFECT_HOLDS_OFF;cast_frames=PS4_PRECOMP_NARES_HEAL_CAST_FRAMES;effect_frames=PS4_PRECOMP_NARES_HEAL_EFFECT_FRAMES;cast_w=PS4_PRECOMP_NARES_HEAL_CAST_W;cast_h=PS4_PRECOMP_NARES_HEAL_CAST_H;effect_w=PS4_PRECOMP_NARES_HEAL_EFFECT_W;effect_h=PS4_PRECOMP_NARES_HEAL_EFFECT_H;}
    int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
    battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,30u);ram8w(0x500Au,0xBDu);
    const uint8_t* ch=ps4_precomp_assets+cast_holds;const uint8_t* eh=ps4_precomp_assets+effect_holds;unsigned cast_len=battle_tech_holds_total(cast_holds,cast_frames),effect_start=30u,effect_len=battle_tech_holds_total(effect_holds,effect_frames),total=effect_start+effect_len;if(cast_len>total)total=cast_len;
    uint16_t before_hp[5];uint8_t before_st[5];for(unsigned i=0u;i<pn&&i<5u;i++){before_hp[i]=party[i].hp;before_st[i]=party[i].status;}unsigned changed=0u;int executed=0;int cast_x=battle_party_center_x(actor),cast_y=battle_source_y_to_gba(0x100u),effect_y=battle_source_y_to_gba(0x108u);
    for(unsigned t=0u;t<total;t++){
        waitvb();int cf=t<cast_len?battle_timed_frame(t,ch,cast_frames):-1;if(cf>=0){battle_tech_upload(cast_off,(unsigned)cf,cast_w,cast_h,BATTLE_TECH_CAST_TILE);battle_obj_any(BATTLE_TECH_CAST_OBJ,cast_x,cast_y,cast_w,cast_h,BATTLE_TECH_CAST_TILE,0,1,1);}else hw_hide_obj(BATTLE_TECH_CAST_OBJ);
        if(t==effect_start)ram8w(0x500Au,0xCCu);if(!executed&&t==effect_start){battle_execute_cmd(party,pn,enemies,actor,c,current_target);executed=1;for(unsigned i=0u;i<pn&&i<5u;i++)if(party[i].hp!=before_hp[i]||party[i].status!=before_st[i])changed|=1u<<i;}
        int ef=t>=effect_start?battle_timed_frame(t-effect_start,eh,effect_frames):-1;if(ef>=0&&changed){battle_tech_upload(effect_off,(unsigned)ef,effect_w,effect_h,BATTLE_TECH_HEAL_EFFECT_TILE);for(unsigned i=0u;i<pn&&i<5u;i++)if(changed&(1u<<i))battle_obj_any(BATTLE_TECH_HEAL_OBJ0+i,battle_party_center_x(i),effect_y,effect_w,effect_h,BATTLE_TECH_HEAL_EFFECT_TILE,0,1,1);else hw_hide_obj(BATTLE_TECH_HEAL_OBJ0+i);}else for(unsigned i=0u;i<pn&&i<5u;i++)hw_hide_obj(BATTLE_TECH_HEAL_OBJ0+i);
        battle_tech_idle_tick(enemies);
    }
    if(!executed)battle_execute_cmd(party,pn,enemies,actor,c,current_target);battle_tech_hide_all();battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;return 1;
}
static int battle_party_source_technique(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned actor,const struct BattleCommandLite* c,unsigned* current_target){
    if(!c||c->type!=2u||actor>=pn||c->id<1u||c->id>38u||!battle_action_has_resource(party,pn,actor,c))return 0;
    if(c->id==PS4_PRECOMP_TECH_FOI_ID)return battle_party_foi_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_TECH_GIFOI_ID)return battle_party_gifoi_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_TECH_NAFOI_ID)return battle_party_nafoi_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_TECH_WAT_ID)return battle_party_wat_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_TECH_TSU_ID)return battle_party_tsu_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_TECH_GITHU_ID)return battle_party_githu_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id==PS4_PRECOMP_TECH_NATHU_ID)return battle_party_nathu_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id>=PS4_PRECOMP_TECH_RES_ID&&c->id<=PS4_PRECOMP_TECH_NARES_MULTI_ID)return battle_party_heal_source_technique(party,pn,enemies,actor,c,current_target);
    if(c->id==0x11u)return battle_party_brose_source_technique(party,pn,enemies,actor,c,current_target); /* source H-scroll controller */
    const Ps4SourceAbilityDesc* d=&ps4_source_tech[c->id];
    if(d->kind==1u&&d->anim_count)return battle_party_source_generated_ability(party,pn,enemies,actor,c,current_target,d,0);
    return 0; /* explicit failure: never synthesize an animation or sound */
}
static void battle_micro_hide_all(void){for(unsigned i=16u;i<34u;i++)hw_hide_obj(i);}
static void battle_micro_upload_burst(unsigned frame,unsigned tile){
    unsigned b1=PS4_PRECOMP_JURAFA_MICRO_BURST_PART1_W*PS4_PRECOMP_JURAFA_MICRO_BURST_PART1_H;unsigned b2=PS4_PRECOMP_JURAFA_MICRO_BURST_PART2_W*PS4_PRECOMP_JURAFA_MICRO_BURST_PART2_H;
    battle_obj_upload_attack(ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_BURST_PART1_PIXELS_OFF+frame*b1,b1,tile);battle_obj_upload_attack(ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_BURST_PART2_PIXELS_OFF+frame*b2,b2,tile+(b1>>5));
}
static void battle_micro_upload_impact(unsigned frame,unsigned tile){unsigned b=PS4_PRECOMP_JURAFA_MICRO_IMPACT_W*PS4_PRECOMP_JURAFA_MICRO_IMPACT_H;battle_obj_upload_attack(ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_IMPACT_PIXELS_OFF+frame*b,b,tile);}
static int battle_jurafa_micro_missile(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei){
    int anchor_x=battle_enemy_anchor_x(ei),anchor_y=battle_enemy_anchor_y(ei);const uint8_t* bh=ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_BURST_HOLDS_OFF;const uint8_t* ih=ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_IMPACT_HOLDS_OFF;
    static const uint8_t burst_spawn[4]={23,33,44,54},burst_variant[4]={0,1,0,1},missile_delay[4]={0,10,20,30};
    static const uint16_t missile_x[4]={0xB0,0xD0,0xF0,0x110},missile_x_mirror[4]={0x190,0x170,0x150,0x130};
    int parent_x[2]={anchor_x-28,anchor_x+28};int burst_age[8],missile_timer[8],impact_age[8];unsigned missile_y[8],missile_sx[8];uint8_t missile_live[8],impact_live[8];unsigned impact_done=0u,return_steps=0u;
    for(unsigned z=0u;z<8u;z++){burst_age[z]=-1;missile_timer[z]=0;impact_age[z]=-1;missile_y[z]=0u;missile_sx[z]=0u;missile_live[z]=0u;impact_live[z]=0u;}
    unsigned pb=PS4_PRECOMP_JURAFA_MICRO_PARENT_W*PS4_PRECOMP_JURAFA_MICRO_PARENT_H,mb=PS4_PRECOMP_JURAFA_MICRO_MISSILE_W*PS4_PRECOMP_JURAFA_MICRO_MISSILE_H;
    waitvb();battle_obj_upload_attack(ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_PARENT_PIXELS_OFF,pb,MICRO_PARENT_TILE);battle_obj_upload_attack(ps4_precomp_assets+PS4_PRECOMP_JURAFA_MICRO_MISSILE_PIXELS_OFF,mb,MICRO_MISSILE_TILE);battle_obj_any(16,parent_x[0],anchor_y-17,16,32,MICRO_PARENT_TILE,0,1,1);battle_obj_any(17,parent_x[1],anchor_y-17,16,32,MICRO_PARENT_TILE,1,1,1);
    for(unsigned update=1u;update<=155u;update++){
        waitvb();
        if(update<=15u){--parent_x[0];++parent_x[1];battle_obj_any(16,parent_x[0],anchor_y-17,16,32,MICRO_PARENT_TILE,0,1,1);battle_obj_any(17,parent_x[1],anchor_y-17,16,32,MICRO_PARENT_TILE,1,1,1);}
        for(unsigned b=0u;b<4u;b++)if(update==burst_spawn[b]){ram8w(0x500Au,0xD0u);for(unsigned side=0u;side<2u;side++)burst_age[side*4u+b]=0;}
        for(unsigned b=0u;b<4u;b++){int age=burst_age[b];if(age<0)age=burst_age[4u+b];if(age>=0){int fr=battle_timed_frame((unsigned)age,bh,6u);unsigned slot=b&1u,tile=slot?MICRO_BURST1_TILE:MICRO_BURST0_TILE;if(fr>=0)battle_micro_upload_burst((unsigned)fr,tile);for(unsigned side=0u;side<2u;side++){unsigned idx=side*4u+b;if(burst_age[idx]<0)continue;unsigned oi=18u+side*4u+(b&1u)*2u;int sign=side?1:-1;int variant=burst_variant[b]?(-sign*6):0;int bx=anchor_x+sign*40+variant;battle_obj_any(oi,bx,anchor_y-29,32,64,tile,side,1,fr>=0);battle_obj_any(oi+1u,bx,anchor_y+4,16,8,tile+64u,side,1,fr>=0);if(fr<0)burst_age[idx]=-1;else ++burst_age[idx];}}}
        if(update==75u){ram8w(0x500Au,0xB7u);for(unsigned side=0u;side<2u;side++)for(unsigned m=0u;m<4u;m++){unsigned idx=side*4u+m;missile_timer[idx]=missile_delay[m];missile_y[idx]=0x68u;missile_sx[idx]=side?missile_x_mirror[m]:missile_x[m];missile_live[idx]=1u;}}
        for(unsigned idx=0u;idx<8u;idx++)if(missile_live[idx]){--missile_timer[idx];if(missile_timer[idx]>=0)continue;unsigned side=idx>>2;unsigned oi=26u+idx;if(missile_y[idx]>=0x110u){missile_live[idx]=0u;impact_live[idx]=1u;impact_age[idx]=0;ram8w(0x500Au,0xBEu);hw_hide_obj(oi);continue;}int mx=battle_source_x_to_gba(missile_sx[idx]),my=battle_source_y_to_gba(missile_y[idx]);battle_obj_any(oi,mx,my,8,32,MICRO_MISSILE_TILE,side,missile_y[idx]>=0xA8u?0:1,1);missile_y[idx]+=0x18u;}
        for(unsigned gen=0u;gen<4u;gen++){unsigned a=gen,b=4u+gen;if(!impact_live[a]&&!impact_live[b])continue;int age=impact_live[a]?impact_age[a]:impact_age[b];int fr=battle_timed_frame((unsigned)age,ih,6u);unsigned slot=gen%3u,tile=slot==0u?MICRO_IMPACT0_TILE:(slot==1u?MICRO_IMPACT1_TILE:MICRO_IMPACT2_TILE);if(fr>=0)battle_micro_upload_impact((unsigned)fr,tile);for(unsigned side=0u;side<2u;side++){unsigned idx=side?b:a;if(!impact_live[idx])continue;unsigned oi=26u+idx;if(fr<0){impact_live[idx]=0u;impact_age[idx]=-1;hw_hide_obj(oi);++impact_done;}else{battle_obj_any(oi,battle_source_x_to_gba(missile_sx[idx]),battle_source_y_to_gba(missile_y[idx]),32,32,tile,side,0,1);++impact_age[idx];}}}
        if(impact_done==8u&&return_steps<15u){++parent_x[0];--parent_x[1];++return_steps;battle_obj_any(16,parent_x[0],anchor_y-17,16,32,MICRO_PARENT_TILE,0,1,1);battle_obj_any(17,parent_x[1],anchor_y-17,16,32,MICRO_PARENT_TILE,1,1,1);}
        battle_action_idle_after_wait(enemies);if(impact_done==8u&&return_steps==15u)break;
    }
    battle_micro_hide_all();return impact_done==8u;
}
/* EnemyAttackOffs -> constructor -> root BattleObj audio lowering.  This
 * deliberately contains only cues whose timing is independent of target/visual
 * animation state in the original 68k object.  Immediate writes correspond to
 * the BattleObj's first invocation.  Counter-driven cues use the source byte
 * counter value verbatim.  Animation-gated sounds stay in the visual path or
 * remain silent until their state relationship is proved. */
#define BATTLE_ENEMY_CUE_MAX 4u
typedef struct {uint8_t frame[BATTLE_ENEMY_CUE_MAX],sound[BATTLE_ENEMY_CUE_MAX],count,active;unsigned next_tick,last_tick;} BattleEnemyCuePlan;
static BattleEnemyCuePlan battle_enemy_cue_plan;
static const Ps4SourceEnemyDesc* battle_enemy_cue_desc;
static void battle_enemy_cue_reset(void){battle_enemy_cue_plan.count=0u;battle_enemy_cue_plan.active=0u;battle_enemy_cue_plan.next_tick=0u;battle_enemy_cue_plan.last_tick=0u;battle_enemy_cue_desc=0;}
static int battle_enemy_cue_has_sound(uint8_t sound){for(unsigned i=0u;i<battle_enemy_cue_plan.count;i++)if(battle_enemy_cue_plan.sound[i]==sound)return 1;return 0;}
static void battle_enemy_cue_add(unsigned frame,uint8_t sound){
    /* The older family scheduler deliberately covered only a subset of source
     * controller sounds.  Do not let a broad family guess inject a sound that
     * the generated enemy/ability pair never writes.  Pairs without SFX
     * metadata retain their bespoke/pre-existing path unchanged. */
    if(!sound||battle_enemy_cue_plan.count>=BATTLE_ENEMY_CUE_MAX)return;
    if(battle_enemy_cue_desc){
        unsigned di=(unsigned)(battle_enemy_cue_desc-ps4_source_enemy_descs);
        if(di<234u&&ps4_source_enemy_sfx_off[di+1u]>ps4_source_enemy_sfx_off[di]&&!ps4_source_enemy_has_sfx(battle_enemy_cue_desc,sound))return;
    }
    unsigned n=battle_enemy_cue_plan.count++;battle_enemy_cue_plan.frame[n]=(uint8_t)frame;battle_enemy_cue_plan.sound[n]=sound;if(frame>battle_enemy_cue_plan.last_tick)battle_enemy_cue_plan.last_tick=frame;
}
static void battle_enemy_cue_emit(unsigned tick){for(unsigned i=0u;i<battle_enemy_cue_plan.count;i++)if((unsigned)battle_enemy_cue_plan.frame[i]==tick)ram8w(0x500Au,battle_enemy_cue_plan.sound[i]);}
static int battle_enemy_cue_activate(void){
    /* Generated carriers previously lost their pair-specific source sound when
     * any generic family cue claimed ownership.  Ensure the source primary is
     * represented once; EXISTING carriers already emit their exact SFX at the
     * precompiled hit frame and must not be doubled here. */
    if(battle_enemy_cue_desc&&(battle_enemy_cue_desc->kind==PS4_SOURCE_ENEMY_KIND_GENERATED||battle_enemy_cue_desc->kind==PS4_SOURCE_ENEMY_KIND_BODY_DBLSLASH)&&battle_enemy_cue_desc->primary_sfx&&!battle_enemy_cue_has_sound(battle_enemy_cue_desc->primary_sfx))battle_enemy_cue_add(0u,battle_enemy_cue_desc->primary_sfx);
    if(!battle_enemy_cue_plan.count)return 0;battle_enemy_cue_plan.active=1u;battle_enemy_cue_emit(0u);battle_enemy_cue_plan.next_tick=1u;if(!battle_enemy_cue_plan.last_tick)battle_enemy_cue_plan.active=0u;return 1;
}
static void battle_enemy_cue_tick(void){
    if(!battle_enemy_cue_plan.active)return;unsigned t=battle_enemy_cue_plan.next_tick++;battle_enemy_cue_emit(t);if(t>=battle_enemy_cue_plan.last_tick)battle_enemy_cue_plan.active=0u;
}
static void battle_enemy_cue_finish(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){while(battle_enemy_cue_plan.active)battle_action_hold(enemies,1u);}
static void battle_enemy_cue_cancel(void){battle_enemy_cue_plan.active=0u;battle_enemy_cue_plan.count=0u;}

static int battle_enemy_source_audio_begin(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t ability){
    battle_enemy_cue_reset();if(ei>=battle_enemy_count)return 0;unsigned id=enemies[ei].id;
    battle_enemy_cue_desc=ps4_source_enemy_find((uint8_t)id,ability);
    if(id==0x00u){battle_enemy_cue_add(0u,0xD7u);return battle_enemy_cue_activate();} /* Helex -> HelexFlameBolt */
    if(id==0x05u&&ability!=0u){battle_enemy_cue_add(0u,0xD7u);return battle_enemy_cue_activate();} /* Forced Fly special */
    if(id==0x0Cu||id==0x0Du){
        if(ability!=0u){
            /* BattleObj_IgglanovaFission: state 0 uses four six-tick entries,
             * but transitions when index 3 is reached, so loc_14C16 begins on
             * source update 19 and writes EnemyAttack3.  The root completes at
             * update 31; MoleAttack belongs to the newly active side fighter. */
            battle_enemy_cue_add(19u,0xD7u);battle_enemy_cue_add(31u,0xD5u);return battle_enemy_cue_activate();
        }
        /* BattleObj_IgglanovaAtk creates BattleObj_SoundExtended3 after its
         * ten-count gate.  That sound object writes EnemyAttack3 four times,
         * once every four ticks. */
        battle_enemy_cue_add(10u,0xD7u);battle_enemy_cue_add(14u,0xD7u);battle_enemy_cue_add(18u,0xD7u);battle_enemy_cue_add(22u,0xD7u);return battle_enemy_cue_activate();
    }
    if(id>=0x02u&&id<=0x04u){battle_enemy_cue_add(0u,0xD6u);return battle_enemy_cue_activate();} /* Gunner/Sensor/Protect Bit */
    if(id>=0x06u&&id<=0x08u){battle_enemy_cue_add(0u,0xD6u);return battle_enemy_cue_activate();} /* Neowhistle/Seeker/Sweeper root state */
    if(id>=0x0Eu&&id<=0x10u){battle_enemy_cue_add(0u,0xD5u);return battle_enemy_cue_activate();} /* Locusta/Fanbite/Grasshound */
    if(id>=0x21u&&id<=0x23u&&ability!=0u){ /* Slug fission root: init MoleAttack, Fusion at source counter $13. */
        battle_enemy_cue_add(0u,0xD5u);battle_enemy_cue_add(0x13u,0xD9u);return battle_enemy_cue_activate();
    }
    if((id==0x24u||id==0x25u||id==0x26u||id==0x89u)&&ability!=0u&&ability!=0x13u){
        battle_enemy_cue_add(0u,0xD5u);return battle_enemy_cue_activate(); /* Meta/Zol Slug fission */
    }
    if(id>=0x29u&&id<=0x2Bu&&ability!=0x17u){battle_enemy_cue_add(0u,0xD6u);return battle_enemy_cue_activate();} /* Arm Drone */
    if(id>=0x4Bu&&id<=0x4Du&&ability!=0u){ /* Every non-normal Flatter branch loads AcidBreathChild. */
        battle_enemy_cue_add(0x0Au,0xD5u);if(ability==0x33u)battle_enemy_cue_add(0x24u,0xD8u);return battle_enemy_cue_activate();
    }
    if(id>=0x57u&&id<=0x58u){
        if(ability==0x2Fu){battle_enemy_cue_add(0u,0xD5u);return battle_enemy_cue_activate();} /* root $374 */
        if(ability==0x3Cu){battle_enemy_cue_add(3u,0xD5u);return battle_enemy_cue_activate();} /* root $364 */
        battle_enemy_cue_add(5u,0xD5u);return battle_enemy_cue_activate(); /* $3D root $36C and fallback root $378 */
    }
    if(id>=0x59u&&id<=0x5Au){ /* Ismounos: both roots reach MoleAttack at counter 6. */
        battle_enemy_cue_add(6u,0xD5u);
        if(ability!=0u)battle_enemy_cue_add(20u,0xD8u); /* Flood Breath: EnemyAttack4 at post-reset counter $0E. */
        return battle_enemy_cue_activate();
    }
    if(id>=0x5Bu&&id<=0x5Cu){ /* Hew Gilla roots share the counter-6 opening. */
        battle_enemy_cue_add(6u,0xD5u);
        battle_enemy_cue_add(20u,ability==0x3Fu?0xD8u:0xBBu); /* $3F breath; $40/fallback Wat path. */
        return battle_enemy_cue_activate();
    }
    if((id>=0x5Du&&id<=0x5Fu||id==0x96u)&&ability!=0u){battle_enemy_cue_add(5u,0xD9u);return battle_enemy_cue_activate();} /* Mini Worm */
    if(id>=0x60u&&id<=0x62u){ /* All Centaur roots share these counter cues. */
        battle_enemy_cue_add(5u,0xD5u);battle_enemy_cue_add(0x0Bu,0xCBu);return battle_enemy_cue_activate();
    }
    if(id>=0x63u&&id<=0x65u){battle_enemy_cue_add(0u,0xD5u);return battle_enemy_cue_activate();} /* TechUser family */
    if(id>=0x6Au&&id<=0x6Cu){battle_enemy_cue_add(0u,0xCDu);return battle_enemy_cue_activate();} /* Haunt family: every constructor root initializes Recovery. */
    if(id>=0x72u&&id<=0x74u){battle_enemy_cue_add(0u,0xD5u);return battle_enemy_cue_activate();} /* Juza family: all eleven roots initialize MoleAttack. */
    if(id==0x78u){battle_enemy_cue_add(0u,0xC3u);return battle_enemy_cue_activate();} /* De-Vars */
    if(id==0x79u){battle_enemy_cue_add(0u,0xCDu);return battle_enemy_cue_activate();} /* Sa-Lews: every constructor root initializes Recovery. */
    if(id>=0x7Au&&id<=0x7Du){ /* D. Elm Lars constructor has six explicit roots plus an unresolved fallback. */
        if(ability==0u||ability==0x5Du||ability==0x35u||ability==0x48u||ability==0x2Eu){battle_enemy_cue_add(0u,0xD5u);return battle_enemy_cue_activate();}
        if(ability==0x5Au){battle_enemy_cue_add(0u,0xCDu);return battle_enemy_cue_activate();}
    }
    if(id==0x7Eu){battle_enemy_cue_add(0u,0xB7u);return battle_enemy_cue_activate();} /* Ryre root $7CC */
    if(id==0x7Fu){battle_enemy_cue_add(0u,0xCDu);return battle_enemy_cue_activate();} /* Re-Faze root $7E0 */
    if(id==0x80u){ /* Lashiec: only counter-driven cues, never child/animation-gated sounds. */
        if(ability==0x60u){battle_enemy_cue_add(0x13u,0xBCu);battle_enemy_cue_add(0x1Eu,0xEBu);return battle_enemy_cue_activate();}
        battle_enemy_cue_add(0x0Cu,0xD5u);
        if(ability==0x5Fu||ability==0x62u)battle_enemy_cue_add(0x1Fu,0xCDu);
        return battle_enemy_cue_activate();
    }
    return battle_enemy_cue_activate();
}
static unsigned battle_enemy_source_mutate_formation(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t skill){
    if(ei>=battle_enemy_count||!skill)return 0u;
    unsigned id=enemies[ei].id;
    /* Event battle 0 Igglanova Fission restores one defeated side Xanafalgue
     * into its authored formation slot; it does not create an arbitrary new
     * target or consume another RNG draw. */
    if(battle_event_index==0u&&(id==0x0Cu||id==0x0Du)){
        unsigned slot=0xFFu;
        if(battle_enemy_empty_side<0&&battle_enemy_count>0u&&!enemies[0].hp)slot=0u;
        else if(battle_enemy_empty_side>0&&battle_enemy_count>2u&&!enemies[2].hp)slot=2u;
        /* Forced/debug Fission can bypass EnemyAI_EmptySpace; choose a real
         * empty authored side without introducing another RNG read. */
        else if(battle_enemy_count>0u&&!enemies[0].hp)slot=0u;
        else if(battle_enemy_count>2u&&!enemies[2].hp)slot=2u;
        if(slot!=0xFFu){
            unsigned keep_place=battle_enemy_placement[slot];
            battle_load_enemy(0x09u,&enemies[slot]);battle_enemy_placement[slot]=(uint8_t)keep_place;
            battle_enemy_reward_mask|=(uint8_t)(1u<<slot);
            for(unsigned op=0u;op<BATTLE_ENEMY_ANIM_COMPONENTS;op++){battle_enemy_anim_frame[slot][op]=0u;battle_enemy_anim_timer[slot][op]=0u;}
            battle_enemy_anim_cache_bind(enemies,battle_enemy_count);return 1u;
        }
        return 0u;
    }
    /* Meta/Zol/King Slug Fission replaces the source formation with four
     * Jr Oozes.  CellSplit ($13) is a different authored skill. */
    if((id==0x24u||id==0x25u||id==0x26u||id==0x89u)&&skill!=0x13u){
        static const uint8_t pos[4]={0x08u,0x10u,0x18u,0x20u};
        battle_enemy_count=4u;battle_enemy_reward_mask=0x0Fu;
        for(unsigned i=0u;i<4u;i++){
            battle_load_enemy(0x23u,&enemies[i]);battle_enemy_placement[i]=pos[i];
            for(unsigned op=0u;op<BATTLE_ENEMY_ANIM_COMPONENTS;op++){battle_enemy_anim_frame[i][op]=0u;battle_enemy_anim_timer[i][op]=0u;}
        }
        battle_enemy_anim_cache_bind(enemies,battle_enemy_count);return 2u;
    }
    return 0u;
}
static void battle_enemy_execute_action(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t skill,unsigned victim,unsigned death_focus){
    uint16_t party_hp_before[5],enemy_hp_before[BATTLE_MAX_ENEMIES];
    struct BattleCommandLite ec={skill?3u:1u,skill,(uint8_t)victim,0xFFu};
    battle_snapshot_hp(party,pn,enemies,party_hp_before,enemy_hp_before);battle_defer_death_audio=1u;
    battle_present_action_pre_fast(party,pn,enemies,1u,ei,&ec,0u);
    int source_audio_owned=battle_enemy_source_audio_begin(enemies,ei,skill);
    if(skill){(void)battle_enemy_source_action_full(party,pn,enemies,ei,skill,victim,(unsigned)source_audio_owned);}
    else battle_action_hold(enemies,6u);
    battle_enemy_cue_finish(enemies);battle_enemy_cue_cancel();
    unsigned formation_changed=battle_enemy_source_mutate_formation(enemies,ei,skill);
    battle_enemy_skill_apply(party,pn,enemies,ei,skill,victim);
    if(formation_changed){for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++)enemy_hp_before[i]=enemies[i].hp;battle_ui_enemies=enemies;battle_generic_pending_mask=0u;battle_generic_stage_ready=0u;}
    battle_finish_damage(party,pn,enemies,party_hp_before,enemy_hp_before,ei,1u,ei,&ec,0u,6u);
    battle_process_new_party_deaths(party,pn,enemies,death_focus);
}

static int battle_enemy_source_action(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t ability){
    if(ei>=battle_enemy_count)return 0;unsigned id=enemies[ei].id,frames=0u,lead=0u,w1=0u,h1=0u,w2=0u,h2=0u;
    if(id==0x1Du&&ability==0x0Eu)return battle_jurafa_micro_missile(enemies,ei);
    unsigned off1=0u,off2=0u,holds=0u,cx1=128u,cy1=128u,cx2=128u,cy2=128u,sfx=0u,hit_frame=0u;
    /* v0.9.10.8: the old Xanafalgue branch used mapping frames at $119E6..,
     * which belong to BattleObj_IgglanovaAtk2.  Do not display another enemy's
     * BattleObj as Xanafalgue's attack.  Falling through keeps Xanafalgue's own
     * source EnemySprites_Animate state running until its exact attack object is
     * decoded. */
    if(id==8u&&ability==4u){frames=PS4_PRECOMP_SWEEPER_LASER_FRAMES;lead=PS4_PRECOMP_SWEEPER_LASER_LEAD_IN;off1=PS4_PRECOMP_SWEEPER_LASER_PART1_PIXELS_OFF;off2=PS4_PRECOMP_SWEEPER_LASER_PART2_PIXELS_OFF;holds=PS4_PRECOMP_SWEEPER_LASER_HOLDS_OFF;w1=PS4_PRECOMP_SWEEPER_LASER_PART1_W;h1=PS4_PRECOMP_SWEEPER_LASER_PART1_H;w2=PS4_PRECOMP_SWEEPER_LASER_PART2_W;h2=PS4_PRECOMP_SWEEPER_LASER_PART2_H;cx1=PS4_PRECOMP_SWEEPER_LASER_PART1_CENTER_X;cy1=PS4_PRECOMP_SWEEPER_LASER_PART1_CENTER_Y;cx2=PS4_PRECOMP_SWEEPER_LASER_PART2_CENTER_X;cy2=PS4_PRECOMP_SWEEPER_LASER_PART2_CENTER_Y;sfx=PS4_PRECOMP_SWEEPER_LASER_SFX;hit_frame=PS4_PRECOMP_SWEEPER_LASER_HIT_FRAME;}
    else if(id==8u&&ability==5u){frames=PS4_PRECOMP_SWEEPER_LIGHTNING_FRAMES;lead=PS4_PRECOMP_SWEEPER_LIGHTNING_LEAD_IN;off1=PS4_PRECOMP_SWEEPER_LIGHTNING_PART1_PIXELS_OFF;off2=PS4_PRECOMP_SWEEPER_LIGHTNING_PART2_PIXELS_OFF;holds=PS4_PRECOMP_SWEEPER_LIGHTNING_HOLDS_OFF;w1=PS4_PRECOMP_SWEEPER_LIGHTNING_PART1_W;h1=PS4_PRECOMP_SWEEPER_LIGHTNING_PART1_H;w2=PS4_PRECOMP_SWEEPER_LIGHTNING_PART2_W;h2=PS4_PRECOMP_SWEEPER_LIGHTNING_PART2_H;cx1=PS4_PRECOMP_SWEEPER_LIGHTNING_PART1_CENTER_X;cy1=PS4_PRECOMP_SWEEPER_LIGHTNING_PART1_CENTER_Y;cx2=PS4_PRECOMP_SWEEPER_LIGHTNING_PART2_CENTER_X;cy2=PS4_PRECOMP_SWEEPER_LIGHTNING_PART2_CENTER_Y;sfx=PS4_PRECOMP_SWEEPER_LIGHTNING_SFX;hit_frame=PS4_PRECOMP_SWEEPER_LIGHTNING_HIT_FRAME;}
    else if(id==0x1Du&&ability==0x0Fu){frames=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_FRAMES;lead=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_LEAD_IN;off1=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_PIXELS_OFF;holds=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_HOLDS_OFF;w1=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_W;h1=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_H;cx1=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_CENTER_X;cy1=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_CENTER_Y;sfx=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_SFX;hit_frame=PS4_PRECOMP_JURAFA_FLAME_LAUNCH_HIT_FRAME;}
    else return 0;
    if(lead)battle_attack_frame_wait(enemies,lead);int ax=battle_enemy_anchor_x(ei),ay=battle_enemy_anchor_y(ei);unsigned bytes1=battle_obj_frame_bytes(w1,h1),bytes2=w2?battle_obj_frame_bytes(w2,h2):0u;
    for(unsigned f=0u;f<frames;f++){waitvb();if(sfx&&f==hit_frame)ram8w(0x500Au,(uint8_t)sfx);battle_obj_upload_attack(ps4_precomp_assets+off1+f*bytes1,bytes1,BATTLE_ATTACK_OBJ_TILE);unsigned tile2=BATTLE_ATTACK_OBJ_TILE+(bytes1>>5);if(w2)battle_obj_upload_attack(ps4_precomp_assets+off2+f*bytes2,bytes2,tile2);battle_obj_any(BATTLE_ATTACK_OBJ0,ax+(int)cx1-128,ay+(int)cy1-128,w1,h1,BATTLE_ATTACK_OBJ_TILE,0,1,1);if(w2)battle_obj_any(BATTLE_ATTACK_OBJ1,ax+(int)cx2-128,ay+(int)cy2-128,w2,h2,tile2,0,1,1);else hw_hide_obj(BATTLE_ATTACK_OBJ1);battle_action_idle_after_wait(enemies);unsigned hold=ps4_precomp_assets[holds+f];if(hold>1u)battle_attack_frame_wait(enemies,hold-1u);}
    battle_attack_hide();return 1;
}
static void battle_combo_source_audio_tick(unsigned combo,unsigned elapsed){
    uint8_t s=0u;
    switch(combo){
    case 1u: if(elapsed==0x30u||elapsed==0x58u)s=0xF5u;else if(elapsed==0x32u)s=0xC5u;break;
    case 2u: case 3u: if(elapsed==0x2Au)s=0xC6u;break;
    case 5u: if(elapsed==0x5Eu)s=0xC3u;break;
    case 6u: if(elapsed==0x26u||elapsed==0x35u||elapsed==0x44u||elapsed==0x53u||elapsed==0x62u||elapsed==0x71u)s=0xC1u;break;
    case 8u: if(elapsed==0x0Fu)s=0xCDu;break;
    case 10u: if(elapsed>=0x5Eu&&elapsed<=0x8Eu&&((elapsed-0x5Eu)&7u)==0u)s=0xD3u;break;
    case 11u: if(elapsed==0x41u)s=0xCEu;else if(elapsed==0x72u||elapsed==0x8Au||elapsed==0x96u)s=0xD3u;break;
    case 12u: if(elapsed==0x40u)s=0xBBu;else if(elapsed==0x46u)s=0xCBu;break;
    case 14u: if(elapsed==0x1Eu)s=0xCDu;break;
    default: break;
    }
    if(s)ram8w(0x500Au,s);
}
static unsigned battle_combo_source_audio_end(unsigned combo){
    switch(combo){case 1u:return 0x59u;case 2u:case 3u:return 0x2Bu;case 5u:return 0x5Fu;case 6u:return 0x72u;case 8u:return 0x10u;case 10u:return 0x8Fu;case 11u:return 0x97u;case 12u:return 0x47u;case 14u:return 0x1Fu;default:return 0u;}
}
static unsigned battle_source_combo_points(unsigned combo,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned current_target,int xs[5],int ys[5]){
    uint32_t cp=PS4_COMBO_DATA+combo*8u;unsigned range=rom8(cp+2u)&15u,n=0u;
    if(range==2u){for(unsigned i=0u;i<battle_enemy_count&&n<5u;i++)if(enemies[i].hp){xs[n]=battle_enemy_anchor_x(i);ys[n]=battle_source_y_to_gba(0xD8u);n++;}return n;}
    if(current_target<battle_enemy_count&&enemies[current_target].hp){xs[0]=battle_enemy_anchor_x(current_target);ys[0]=battle_source_y_to_gba(0xD8u);return 1u;}
    for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp){xs[0]=battle_enemy_anchor_x(i);ys[0]=battle_source_y_to_gba(0xD8u);return 1u;}
    return 0u;
}
static void battle_source_combo_frame(const Ps4SourceComboAnim* a,unsigned frame,const int* xs,const int* ys,unsigned ntarget){
    unsigned oi=BATTLE_SOURCE_ABILITY_OBJ0,tile=BATTLE_ATTACK_OBJ_TILE;
    for(unsigned pi=0u;pi<a->part_count;pi++){
        const Ps4SourceComboPart* p=&ps4_source_combo_parts[a->first_part+pi];
        const uint8_t* src=ps4_source_combo_bank+p->off+(uint32_t)frame*p->frame_bytes;
        (void)battle_effect_upload(src,p->frame_bytes,tile);
        for(unsigned t=0u;t<ntarget&&oi<BATTLE_SOURCE_ABILITY_OBJ0+BATTLE_SOURCE_ABILITY_OBJN;t++)
            battle_source_obj_prebound4(oi++,xs[t],ys[t],p->ox,p->oy,p->geom,tile);
        tile+=p->frame_bytes>>5;
    }
    {unsigned now=oi-BATTLE_SOURCE_ABILITY_OBJ0;if(now<battle_source_effect_active_oam)for(unsigned i=now;i<battle_source_effect_active_oam;i++)hw_hide_obj(BATTLE_SOURCE_ABILITY_OBJ0+i);battle_source_effect_active_oam=now;}
}
static void battle_source_combo_wait(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned combo,unsigned* elapsed,unsigned frames){
    for(unsigned i=0u;i<frames;i++){waitvb();battle_combo_source_audio_tick(combo,*elapsed);battle_action_idle_after_wait(enemies);(*elapsed)++;}
}

/* R21 overlapping combo child scheduler.  The track tile field is explicit so
 * source children whose lifetimes do not overlap can reuse the same OBJ bank;
 * this is how the Genesis concurrent-object choreography fits Mode-4's OBJ VRAM
 * without reverting to R20's all-children-serialized playback. */
typedef struct{
    const Ps4SourceComboAnim* a;
    uint16_t start,tile;
    uint8_t nt,last_frame,motion,pad;
    int16_t xs[5],ys[5],cx[5],cy[5];
    uint8_t seed[5];
} BattleComboTrack;
#define BATTLE_COMBO_TRACK_MAX 32u
/* 32 * 0x3C = 0x780 bytes at 0x02033600..0x02033D7F.  This is the
 * title-token presentation arena after the title has finished, not linker BSS. */
static BattleComboTrack* const battle_combo_tracks=(BattleComboTrack*)0x02033600;
static unsigned battle_combo_track_count;
static const Ps4SourceComboAnim* battle_source_combo_find_anim(uint32_t table){for(unsigned i=0u;i<PS4_SOURCE_COMBO_ANIM_COUNT;i++)if(ps4_source_combo_anims[i].source_table==table)return &ps4_source_combo_anims[i];return 0;}
static unsigned battle_source_combo_anim_duration(const Ps4SourceComboAnim* a){if(!a)return 0u;const uint8_t* h=ps4_source_combo_bank+a->holds_off;unsigned n=0u;for(unsigned i=0u;i<a->frame_count;i++)n+=h[i]?h[i]:1u;return n;}
static int battle_source_combo_anim_frame_at(const Ps4SourceComboAnim* a,unsigned age){if(!a)return -1;const uint8_t* h=ps4_source_combo_bank+a->holds_off;for(unsigned f=0u;f<a->frame_count;f++){unsigned q=h[f]?h[f]:1u;if(age<q)return (int)f;age-=q;}return -1;}
static unsigned battle_source_combo_anim_tiles(const Ps4SourceComboAnim* a){unsigned n=0u;if(a)for(unsigned i=0u;i<a->part_count;i++)n+=ps4_source_combo_parts[a->first_part+i].frame_bytes>>5;return n;}
static int battle_source_trig8(uint8_t angle,int cosine){
    static const uint8_t q[128]={0,3,6,9,12,16,19,22,25,28,31,34,37,40,43,46,49,52,54,57,60,63,66,68,71,73,76,78,81,83,86,88,90,92,94,96,98,100,102,104,106,108,109,111,112,114,115,116,118,119,120,121,122,123,123,124,125,125,126,126,126,127,127,127,127,127,127,127,126,126,125,125,124,124,123,122,121,120,119,118,117,116,114,113,112,110,108,107,105,103,101,99,97,95,93,91,89,87,84,82,80,77,75,72,69,67,64,61,59,56,53,50,47,44,41,39,36,32,29,26,23,20,17,14,11,8,5,2};
    int a=(int8_t)(uint8_t)(angle+(cosine?0x40u:0u));if(a>=0)return q[(unsigned)a];return -(int)q[(unsigned)(~a)&0x7Fu];
}
static void battle_combo_tracks_reset(void){battle_combo_track_count=0u;}
static BattleComboTrack* battle_combo_track_add(uint32_t table,unsigned start,unsigned tile,const int* xs,const int* ys,unsigned nt){
    if(!nt||nt>5u||battle_combo_track_count>=BATTLE_COMBO_TRACK_MAX)return 0;const Ps4SourceComboAnim* a=battle_source_combo_find_anim(table);if(!a||tile+battle_source_combo_anim_tiles(a)>1024u)return 0;
    BattleComboTrack* t=&battle_combo_tracks[battle_combo_track_count++];t->a=a;t->start=(uint16_t)start;t->tile=(uint16_t)tile;t->nt=(uint8_t)nt;t->last_frame=0xFFu;t->motion=0u;
    for(unsigned i=0u;i<nt;i++){t->xs[i]=(int16_t)xs[i];t->ys[i]=(int16_t)ys[i];t->cx[i]=(int16_t)xs[i];t->cy[i]=(int16_t)ys[i];t->seed[i]=0u;}return t;
}
static void battle_combo_blizzard_motion(BattleComboTrack* t,unsigned age){
    static const uint8_t step[27]={0,1,2,4,6,9,12,16,20,25,31,37,43,49,55,61,67,73,79,85,91,97,103,109,115,121,127};unsigned si=age+1u;if(si>26u)si=26u;
    for(unsigned i=0u;i<t->nt;i++){uint8_t a=(uint8_t)(t->seed[i]+step[si]);int dx=(5*battle_source_trig8(a,1))/8,dy=(5*battle_source_trig8(a,0))/16;t->xs[i]=(int16_t)battle_source_x_to_gba_signed(t->cx[i]+dx);t->ys[i]=(int16_t)battle_source_y_to_gba((unsigned)(t->cy[i]+dy));}
}
static void battle_combo_tracks_draw(unsigned elapsed){
    unsigned oi=BATTLE_SOURCE_ABILITY_OBJ0;
    for(unsigned ti=0u;ti<battle_combo_track_count;ti++){
        BattleComboTrack* t=&battle_combo_tracks[ti];if(elapsed<t->start)continue;unsigned age=elapsed-t->start;int f=battle_source_combo_anim_frame_at(t->a,age);if(f<0)continue;if(t->motion==1u)battle_combo_blizzard_motion(t,age);
        unsigned tile=t->tile;for(unsigned pi=0u;pi<t->a->part_count;pi++){const Ps4SourceComboPart* p=&ps4_source_combo_parts[t->a->first_part+pi];if(t->last_frame!=(uint8_t)f){const uint8_t* src=ps4_source_combo_bank+p->off+(uint32_t)(unsigned)f*p->frame_bytes;(void)battle_effect_upload(src,p->frame_bytes,tile);}for(unsigned q=0u;q<t->nt&&oi<BATTLE_SOURCE_ABILITY_OBJ0+BATTLE_SOURCE_ABILITY_OBJN;q++)battle_source_obj_prebound4(oi++,t->xs[q],t->ys[q],p->ox,p->oy,p->geom,tile);tile+=p->frame_bytes>>5;}t->last_frame=(uint8_t)f;
    }
    {unsigned now=oi-BATTLE_SOURCE_ABILITY_OBJ0;if(now<battle_source_effect_active_oam)for(unsigned i=now;i<battle_source_effect_active_oam;i++)hw_hide_obj(BATTLE_SOURCE_ABILITY_OBJ0+i);battle_source_effect_active_oam=now;}
}
static unsigned battle_combo_tracks_end(void){unsigned end=0u;for(unsigned i=0u;i<battle_combo_track_count;i++){unsigned e=battle_combo_tracks[i].start+battle_source_combo_anim_duration(battle_combo_tracks[i].a);if(e>end)end=e;}return end;}
static void battle_combo_tracks_run(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned combo,unsigned end){for(unsigned t=0u;t<end;t++){waitvb();battle_combo_source_audio_tick(combo,t);battle_combo_tracks_draw(t);battle_action_idle_after_wait(enemies);}}
static unsigned battle_combo_live_enemy_points(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],int xs[5],int ys[5],int yoff){unsigned n=0u;for(unsigned i=0u;i<battle_enemy_count&&n<5u;i++)if(enemies[i].hp){xs[n]=battle_enemy_anchor_x(i);ys[n]=battle_enemy_anchor_y(i)+yoff;n++;}return n;}
static int battle_party_source_combo_overlap(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned combo,unsigned current_target,const unsigned who[4],unsigned participants){
    /* Igglanova/Xanafalgue owns OBJ tiles 512..767.  The overlap families that
     * fit in the upper 256 tiles still run their source child scheduler there;
     * Firestorm/Blizzard require the wider normal-battle window and retain the
     * safe R20 serialized presentation only in that one event battle. */
    if(battle_event_index==0u&&(combo==2u||combo==3u))return 0;
    const Ps4SourceComboDesc* d=&ps4_source_combo[combo];dma_copy32(ps4_source_combo_palettes[d->palette_id],(void*)(OBJ_PALETTE+BATTLE_SOURCE_ABILITY_PALBANK*16u),32u);battle_combo_tracks_reset();
    int x[5],y[5],x2[5],y2[5];unsigned n=0u,end=0u;const unsigned base=(battle_event_index==0u)?768u:512u;
    switch(combo){
    case 2u: { /* Firestorm: source start/position overlap. C762 consumes 8 KiB; use the other 6 KiB bank for the active Zan pillar. */
        int qx[1]={battle_source_x_to_gba(0x120u)},qy[1]={battle_source_y_to_gba(0xD8u)};battle_combo_track_add(0x25C762u,0x2Au,base,qx,qy,1u);
        /* Genesis can hold three independently-phased 6 KiB C240 children. GBA Mode4 cannot. Preserve all three authored spawns/positions using the one available 6 KiB bank; later spawns take over that bank instead of corrupting OBJ VRAM. */
        int a1x[1]={battle_source_x_to_gba(0x190u)},a1y[1]={battle_source_y_to_gba(0xE0u)};battle_combo_track_add(0x25C240u,0x38u,base+256u,a1x,a1y,1u);
        int a2x[1]={battle_source_x_to_gba(0xD0u)},a2y[1]={battle_source_y_to_gba(0xD8u)};battle_combo_track_add(0x25C240u,0x5Au,base+256u,a2x,a2y,1u);
        int a3x[1]={battle_source_x_to_gba(0x168u)},a3y[1]={battle_source_y_to_gba(0xD0u)};battle_combo_track_add(0x25C240u,0x78u,base+256u,a3x,a3y,1u);break;}
    case 3u: { /* Blizzard: Zan core plus 31 source two-flake spawn pairs following the original trig path. */
        int qx[1]={battle_source_x_to_gba(0x120u)},qy[1]={battle_source_y_to_gba(0xD8u)};battle_combo_track_add(0x25C762u,0x2Au,base,qx,qy,1u);uint8_t parent_phase=0u;
        for(unsigned k=0u;k<31u;k++){int sx[2],sy[2];uint8_t seeds[2];for(unsigned j=0u;j<2u;j++){parent_phase=(uint8_t)(parent_phase+((field_npc_rng_next()&0x3Fu)+0x40u));seeds[j]=parent_phase;int bx=0x120-(5*battle_source_trig8(seeds[j],1))/8,by=0xD8-(5*battle_source_trig8(seeds[j],0))/16;sx[j]=battle_source_x_to_gba_signed(bx);sy[j]=battle_source_y_to_gba((unsigned)by);}parent_phase=(uint8_t)(parent_phase+0x5Bu);BattleComboTrack* t=battle_combo_track_add(0x25EC78u,0x3Cu+1u+k*2u,base+256u+(k%14u)*16u,sx,sy,2u);if(t){t->motion=1u;for(unsigned j=0u;j<2u;j++){t->seed[j]=seeds[j];t->cx[j]=(int16_t)(0x120-(5*battle_source_trig8(seeds[j],1))/8);t->cy[j]=(int16_t)(0xD8-(5*battle_source_trig8(seeds[j],0))/16);}}}break;}
    case 6u: { /* Silent Wave: caster wave, six mirrored child pairs, twelve staggered horizontal sweep children. */
        unsigned a=(participants&&who[0]<pn)?who[0]:0u;int cx=battle_party_center_x(a);int qx[2],qy[2];qx[0]=cx;qy[0]=battle_source_y_to_gba(0x108u);battle_combo_track_add(0x25E072u,0x26u,base,qx,qy,1u);
        for(unsigned k=0u;k<6u;k++){unsigned st=0x26u+k*0x0Fu;qx[0]=cx+12;qx[1]=cx-12;qy[0]=qy[1]=battle_source_y_to_gba(0x118u);battle_combo_track_add(0x25BACEu,st,base+32u+(k&1u)*64u,qx,qy,2u);}
        for(unsigned k=0u;k<12u;k++){qx[0]=battle_source_x_to_gba(0xA0u+k*0x18u);qy[0]=battle_source_y_to_gba(0xD8u);battle_combo_track_add(0x25ECC6u,0x35u+k*4u,base+160u+(k%3u)*16u,qx,qy,1u);}break;}
    case 10u: { /* Black Hole: seven source bursts, each ends before the next begins, so one bank is exact. */
        static const uint32_t tab[7]={0x25ED8Cu,0x25ED7Cu,0x25ED84u,0x25ED84u,0x25ED8Cu,0x25ED7Cu,0x25ED94u};static const uint16_t sx[7]={0xF0,0x180,0x168,0xD8,0x150,0xC0,0x120},sy[7]={0xC0,0x90,0xA8,0xA8,0xC0,0x90,0xC8};
        for(unsigned k=0u;k<7u;k++){x[0]=battle_source_x_to_gba(sx[k]);y[0]=battle_source_y_to_gba(sy[k]);battle_combo_track_add(tab[k],0x5Eu+k*8u,base,x,y,1u);}break;}
    case 11u: { /* Circuit Breaker: one enemy-wide charge then three vertically-staggered discharge passes. */
        n=battle_combo_live_enemy_points(enemies,x,y,0);if(!n)return 0;battle_combo_track_add(0x25DD1Cu,0x41u,base,x,y,n);
        for(unsigned k=0u;k<3u;k++){int dy=(k==0)?6:(k==2?-12:0);for(unsigned i=0u;i<n;i++){x2[i]=x[i];y2[i]=y[i]+dy;}battle_combo_track_add(0x25CCC0u,k==0?0x72u:(k==1?0x8Au:0x96u),base,x2,y2,n);}break;}
    case 12u: { /* Purifying Light: target light plus Kyra's paired source child, if she is one of the combo participants. */
        n=battle_combo_live_enemy_points(enemies,x,y,0);if(n)battle_combo_track_add(0x25E226u,0x46u,base,x,y,n);unsigned kn=0u;for(unsigned k=0u;k<participants&&k<4u;k++)if(who[k]<pn&&party[who[k]].id==PS4_PRECOMP_PARTY_KYRA_ID){x2[kn]=battle_party_center_x(who[k])-12;y2[kn]=battle_source_y_to_gba(0x100u);kn++;if(kn<5u){x2[kn]=battle_party_center_x(who[k])+12;y2[kn]=battle_source_y_to_gba(0x100u);kn++;}}if(kn)battle_combo_track_add(0x25B62Eu,0x40u,base+64u,x2,y2,kn);break;}
    default:return 0;
    }
    end=battle_combo_tracks_end();unsigned ae=battle_combo_source_audio_end(combo);if(ae>end)end=ae;if(!end)return 0;battle_combo_tracks_run(enemies,combo,end);battle_source_ability_hide_all();return 1;
}

static int battle_party_source_combo(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned combo,unsigned current_target,const unsigned who[4],unsigned participants){
    if(combo<1u||combo>14u)return 0;if(battle_party_source_combo_overlap(party,pn,enemies,combo,current_target,who,participants))return 1;const Ps4SourceComboDesc* d=&ps4_source_combo[combo];unsigned elapsed=0u;
    dma_copy32(ps4_source_combo_palettes[d->palette_id],(void*)(OBJ_PALETTE+BATTLE_SOURCE_ABILITY_PALBANK*16u),32u);
    if(d->kind==2u){
        /* Holocaust is a source screen/palette controller rather than a normal
         * sprite package. Lower that distinction explicitly instead of inventing
         * an unrelated sprite: pulse the battle plane through the GBA brightness
         * unit while preserving the authored action scene underneath. */
        REG_BLDCNT=(uint16_t)((3u<<6)|0x003Fu);
        for(unsigned f=0u;f<48u;f++){unsigned q=f<24u?f:(47u-f);REG_BLDY=(uint16_t)((q*12u)/23u);battle_source_combo_wait(enemies,combo,&elapsed,1u);}
        REG_BLDY=0u;REG_BLDCNT=0u;
    }else if(d->anim_count){
        int xs[5],ys[5];unsigned nt=battle_source_combo_points(combo,enemies,current_target,xs,ys);if(!nt)return 0;
        for(unsigned ai=0u;ai<d->anim_count;ai++){
            const Ps4SourceComboAnim* a=&ps4_source_combo_anims[d->first_anim+ai];const uint8_t* holds=ps4_source_combo_bank+a->holds_off;
            for(unsigned f=0u;f<a->frame_count;f++){
                unsigned h=holds[f];if(!h)h=1u;
                for(unsigned tick=0u;tick<h;tick++){waitvb();battle_combo_source_audio_tick(combo,elapsed);if(tick==0u)battle_source_combo_frame(a,f,xs,ys,nt);battle_action_idle_after_wait(enemies);elapsed++;}
            }
        }
    }else return 0;
    {unsigned end=battle_combo_source_audio_end(combo);if(elapsed<end)battle_source_combo_wait(enemies,combo,&elapsed,end-elapsed);}
    battle_source_ability_hide_all();REG_BLDY=0u;REG_BLDCNT=0u;return 1;
}
static void battle_execute_combo(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],const struct BattleCommandLite cmds[5],const unsigned who[4],unsigned participants,unsigned combo,unsigned* current_target){
    unsigned actor_power=0u,ability_power=0u;uint8_t reaction=2u;for(unsigned k=0u;k<participants;k++){unsigned pi=who[k];const struct BattleCommandLite* c=&cmds[pi];uint32_t ap;if(c->type==2u){ap=PS4_TECH_DATA+(uint32_t)(c->id-1u)*8u;actor_power+=party[pi].mental;reaction|=4u;}else{ap=PS4_SKILL_DATA+(uint32_t)(c->id-1u)*8u;actor_power+=battle_actor_stat(&party[pi],rom8(ap+1u)&0x7Fu);}ability_power+=rom8(ap+3u);battle_consume_action_resource(party,pi,c);}
    uint32_t cp=PS4_COMBO_DATA+combo*8u;unsigned effect=rom8(cp),range=rom8(cp+2u)&15u,hit=rom8(cp+3u),resist=rom8(cp+4u),element=rom8(cp+5u);if(!element)element=1u;if(range==2u)reaction|=0x10u;
    unsigned lo=range==2u?0u:*current_target,hi=range==2u?battle_enemy_count:*current_target+1u;if(lo>=battle_enemy_count)lo=0u;
    for(unsigned i=lo;i<hi&&i<battle_enemy_count;i++)if(enemies[i].hp){battle_mark_enemy_reaction(i,reaction);unsigned target_power=battle_actor_stat(&enemies[i],resist),factor=battle_element_factor(&enemies[i],element);if(element>=0x10u){factor=0u;for(unsigned k=0u;k<participants;k++){unsigned wf=battle_weapon_factor(&party[who[k]],&enemies[i]);if(wf>factor)factor=wf;}}if(effect==1u){unsigned dmg=battle_source_damage(actor_power,target_power,ability_power,factor);battle_enemy_set_hp(&enemies[i],dmg>=enemies[i].hp?0u:enemies[i].hp-dmg);}else if(effect==2u&&battle_calc_chance(actor_power,target_power,factor,hit,hit)>=0)battle_enemy_set_hp(&enemies[i],0u);}
}
static void battle_restore_stats_at_turn_end(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    /* Direct lowering of PSIV Battle_RestoreStatsAtTurnEnd ($67BE/$67DE area):
     * sleeping fighters have a 50% wake check; waking clears both sleep bits
     * and restores agility. Enemy paralysis is one-turn and is cleared here.
     * The physical-property bookkeeping is represented by the saved property
     * bytes already carried in BattleLiteActor. */
    for(unsigned i=0u;i<pn;i++){
        /* Retail Battle_RestoreStatsAtTurnEnd copies saved physical_prop+1
         * ($31) back to physical_prop ($30) for every live fighter. */
        party[i].element_prop[0]=party[i].base_element_prop[0];
        if(party[i].status&0x28u){if(field_npc_rng_next()&1u){party[i].status&=(uint8_t)~0x28u;party[i].agility=party[i].base_agility;}}
    }
    for(unsigned i=0u;i<battle_enemy_count;i++){
        if(enemies[i].status&0x28u){if(field_npc_rng_next()&1u){enemies[i].status&=(uint8_t)~0x28u;enemies[i].agility=enemies[i].base_agility;}}
        if(enemies[i].hp&&(enemies[i].status&2u)){enemies[i].status&=(uint8_t)~2u;enemies[i].agility=enemies[i].base_agility;enemies[i].dexterity=enemies[i].base_dexterity;}
    }
}

static void battle_resolve_mixed_round(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],struct BattleCommandLite cmds[5],int macro_index,unsigned* current_target){
    struct BattleTurnLite turns[9];unsigned turn_count=battle_build_turn_order(party,pn,enemies,macro_index,turns);
    int enemy_targets[BATTLE_MAX_ENEMIES];for(unsigned i=0u;i<battle_enemy_count;i++)enemy_targets[i]=battle_select_enemy_target(party,pn);
    /* Source Defend becomes active only when Character_Defend executes in
     * turn order, not at command-entry time. */
    for(unsigned a=0u;a<pn;a++)party[a].defending=0u;
    battle_damage_overlay_clear();
    for(unsigned pos=0u;pos<turn_count&&battle_party_alive(party,pn)&&battle_enemies_alive(enemies);){
        struct BattleTurnLite t=turns[pos];uint16_t party_hp_before[5],enemy_hp_before[BATTLE_MAX_ENEMIES];
        if(!t.enemy){
            if(t.index>=pn||battle_actor_incapacitated(&party[t.index])){++pos;continue;}
            battle_snapshot_hp(party,pn,enemies,party_hp_before,enemy_hp_before);battle_defer_death_audio=1u;
            unsigned combo=0u,who[4]={0,0,0,0};int used=battle_combo_find_at(turns,turn_count,pos,cmds,party,pn,&combo,who);
            if(used>0){
                unsigned tt=cmds[who[0]].target;if(tt<battle_enemy_count)*current_target=tt;
                battle_present_action_pre_fast(party,pn,enemies,0u,t.index,&cmds[t.index],combo);if(!battle_party_source_combo(party,pn,enemies,combo,*current_target,who,(unsigned)used))battle_action_hold(enemies,6u);
                battle_execute_combo(party,pn,enemies,cmds,who,(unsigned)used,combo,current_target);
                battle_finish_damage(party,pn,enemies,party_hp_before,enemy_hp_before,*current_target,0u,t.index,&cmds[t.index],combo,8u);battle_process_pending_enemy_deaths(enemies);
                pos+=(unsigned)used;continue;
            }
            if(battle_command_target_kind(&cmds[t.index])==1u&&cmds[t.index].target<battle_enemy_count)*current_target=cmds[t.index].target;
            if(*current_target>=battle_enemy_count||!enemies[*current_target].hp)*current_target=battle_next_target(enemies,*current_target,1);
            int executed=0;
            if(cmds[t.index].type==1u){
                unsigned sm=battle_actor_slasher_mask(&party[t.index]);
                unsigned source_slasher=sm&&(party[t.index].id==1u||party[t.index].id==9u);
                uint16_t slasher_hp[BATTLE_MAX_ENEMIES];
                if(source_slasher){
                    /* Resolve once in original target/RNG order; keep the victims alive
                     * through the sweep and use the saved hit flags for each crossing. */
                    battle_execute_cmd(party,pn,enemies,t.index,&cmds[t.index],current_target);executed=1;
                    for(unsigned i=0u;i<battle_enemy_count;i++){slasher_hp[i]=enemies[i].hp;enemies[i].hp=enemy_hp_before[i];}
                }
                if(!battle_party_source_attack(party,pn,enemies,*current_target,t.index,&cmds[t.index])){battle_present_action_pre_fast(party,pn,enemies,0u,t.index,&cmds[t.index],0u);battle_action_hold(enemies,5u);}
                if(source_slasher)for(unsigned i=0u;i<battle_enemy_count;i++)enemies[i].hp=slasher_hp[i];
            }else if(cmds[t.index].type==2u){
                executed=battle_party_source_technique(party,pn,enemies,t.index,&cmds[t.index],current_target);
                if(!executed){battle_execute_cmd(party,pn,enemies,t.index,&cmds[t.index],current_target);executed=1;}
            }else if(cmds[t.index].type==3u){
                executed=battle_party_source_skill(party,pn,enemies,t.index,&cmds[t.index],current_target);
                if(!executed){battle_suppress_skill_source_audio=1u;battle_execute_cmd(party,pn,enemies,t.index,&cmds[t.index],current_target);battle_suppress_skill_source_audio=0u;executed=1;}
            }else{battle_present_action_pre_fast(party,pn,enemies,0u,t.index,&cmds[t.index],0u);battle_action_hold(enemies,5u);}
            if(!executed)battle_execute_cmd(party,pn,enemies,t.index,&cmds[t.index],current_target);
            battle_finish_damage(party,pn,enemies,party_hp_before,enemy_hp_before,*current_target,0u,t.index,&cmds[t.index],0u,6u);battle_process_pending_enemy_deaths(enemies);
            ++pos;continue;
        }
        unsigned ei=t.index;if(ei>=battle_enemy_count||battle_actor_incapacitated(&enemies[ei])){++pos;continue;}
        int victim=enemy_targets[ei];if(victim<0||(unsigned)victim>=pn||!party[victim].hp)victim=battle_select_enemy_target(party,pn);if(victim<0)break;
        uint8_t skill=battle_enemy_pick_skill(ei,enemies,party,pn);
        battle_enemy_execute_action(party,pn,enemies,ei,skill,(unsigned)victim,*current_target);++pos;
    }
    battle_damage_overlay_clear();for(unsigned a=0u;a<pn;a++)party[a].defending=0u;
    battle_party_revive_visibility(party,pn);battle_restore_stats_at_turn_end(party,pn,enemies);
}


static unsigned battle_load_event_formation(unsigned value,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    /* Battle_BossFormationData ($284F7C) is one Kosinski stream containing
     * 0xFF-delimited variable records. Battle_SetupEnemyData selects record
     * Event_Battle_Index; byte +4 is enemy count and +7,+9,+11,+13 are IDs. */
    unsigned n=kos_decode(0x284F7Cu,&work_ram[0x8000u],0x1000u),off=0u;
    if(!n)return 0u;
    for(unsigned k=0u;k<value;k++){while(off<n&&work_ram[0x8000u+off]!=0xFFu)++off;if(off>=n)return 0u;++off;}
    if(off+8u>n)return 0u;unsigned count=work_ram[0x8000u+off+4u];if(!count||count>BATTLE_MAX_ENEMIES)return 0u;
    for(unsigned i=0u;i<count;i++){unsigned q=off+7u+i*2u;if(q+1u>=n||work_ram[0x8000u+q]==0xFFu)return 0u;battle_load_enemy(work_ram[0x8000u+q],&enemies[i]);battle_enemy_placement[i]=work_ram[0x8000u+q+1u];}
    for(unsigned i=count;i<BATTLE_MAX_ENEMIES;i++){enemies[i].hp=0u;enemies[i].max_hp=0u;enemies[i].id=0u;battle_enemy_placement[i]=0u;}
    battle_enemy_reward_mask=(uint8_t)((1u<<count)-1u);
    /* EnemyInit_Igglanova explicitly clears prev_obj and next_obj before the
     * encounter begins.  Keep the authored records as dormant Fission slots,
     * but only Igglanova is active/reward-eligible at battle start. */
    if(value==0u&&count>=3u&&enemies[0].id==0x09u&&(enemies[1].id==0x0Cu||enemies[1].id==0x0Du)&&enemies[2].id==0x09u){
        enemies[0].hp=0u;enemies[2].hp=0u;battle_enemy_reward_mask=(uint8_t)(1u<<1);
    }
    return count;
}
static uint8_t battle_random_escape_param;
static uint8_t battle_random_bg_index;
static uint8_t battle_random_group;
static uint8_t battle_debug_enemy_pending=0xFFu;


static unsigned battle_load_formation_record(const uint8_t* rec,unsigned avail,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    /* Random and event formation records share the same compact header used by
     * Battle_SetupEnemyData.  Byte 0 participates in battle-priority checks,
     * byte 1 is the RUN target parameter, byte 4 is count, and enemy IDs are
     * interleaved with placement bytes from +7.  We preserve both leading
     * fields even though v0.9.8 does not yet reproduce every priority visual. */
    if(avail<8u)return 0u;unsigned count=rec[4u];if(!count||count>BATTLE_MAX_ENEMIES)return 0u;
    battle_random_bg_index=rec[0u];battle_random_escape_param=rec[1u];
    for(unsigned i=0u;i<count;i++){unsigned q=7u+i*2u;if(q+1u>=avail||rec[q]==0xFFu)return 0u;battle_load_enemy(rec[q],&enemies[i]);battle_enemy_placement[i]=rec[q+1u];}
    for(unsigned i=count;i<BATTLE_MAX_ENEMIES;i++){enemies[i].hp=0u;enemies[i].max_hp=0u;enemies[i].id=0u;battle_enemy_placement[i]=0u;}
    battle_enemy_reward_mask=(uint8_t)((1u<<count)-1u);return count;
}
static int battle_random_group_for_field(unsigned* group){
    /* Battle_SetupEnemyData: ordinary field maps use the byte table at $8050.
     * Motavia/Dezolis world maps (groups 0/1) instead decode a 64x64 region map
     * and sample it at character-coordinate / 64.  Negative region bytes mean
     * group 0. Vehicle_Index selects the source vehicle groups below. */
    unsigned map=field_map.map_id;if(map>=0x1A1u)return 0;
    unsigned g=rom8(0x8050u+map);if(g==0xFFu)return 0;
    if(g>1u){*group=g;return 1;}
    /* Source loc_7E24: vehicle world-map groups are 8/9/10 on Motavia,
     * selected by GetMotaBattleBGIndex, and 13 on Dezolis. */
    if(ram16r(0xF43Cu)){
        if(map!=0u){*group=13u;return 1;}
        unsigned chunk=field_chunk_index_at(chaz.x,chaz.y,0,0);
        unsigned raw_bg=rom8(0x586ECu+chunk);
        *group=raw_bg==1u?9u:(raw_bg==3u?10u:8u);return 1;
    }
    uint32_t stream=map==0u?0x28501Cu:0x2853CCu;unsigned cap=map==0u?0x1000u:0x0800u;
    unsigned n=kos_decode(stream,&work_ram[0x8000u],cap);if(!n)return 0;
    unsigned cx=((unsigned)chaz.x)>>6,cy=((unsigned)chaz.y)>>6;unsigned off=(cy<<6)+cx;if(off>=n)return 0;
    int q=battle_s8(work_ram[0x8000u+off]);*group=q<0?0u:(unsigned)q;return *group<68u;
}
static unsigned battle_load_random_formation(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    if(battle_debug_enemy_pending!=0xFFu){unsigned id=battle_debug_enemy_pending;battle_debug_enemy_pending=0xFFu;battle_random_group=0u;battle_random_bg_index=0u;battle_random_escape_param=0x20u;battle_load_enemy(id,&enemies[0]);battle_enemy_placement[0]=0x14u;for(unsigned i=1u;i<BATTLE_MAX_ENEMIES;i++){enemies[i].hp=0u;enemies[i].max_hp=0u;enemies[i].id=0u;battle_enemy_placement[i]=0u;}battle_enemy_reward_mask=1u;return 1u;}
    unsigned group;if(!battle_random_group_for_field(&group))return 0u;battle_random_group=(uint8_t)group;
    /* Battle_FormationIndexes decompresses to 68 groups x 32 big-endian words.
     * UpdateRNGSeed2 & $1F selects one word from the active 64-byte group. */
    unsigned n=kos_decode(0x2836ECu,&work_ram[0x8000u],0x1200u);if(n<group*64u+64u)return 0u;
    unsigned pick=(unsigned)(ps4_rng2_next()&0x1Fu);unsigned o=group*64u+pick*2u;
    unsigned formation=((unsigned)work_ram[0x8000u+o]<<8)|work_ram[0x8000u+o+1u];
    unsigned bank=formation>>7,rec_index=formation&0x7Fu;if(bank>3u)return 0u;
    static const uint32_t streams[4]={0x283E6Cu,0x2842BCu,0x28471Cu,0x284B8Cu};
    n=kos_decode(streams[bank],&work_ram[0x8000u],0x1000u);if(!n)return 0u;
    unsigned off=0u;for(unsigned k=0u;k<rec_index;k++){while(off<n&&work_ram[0x8000u+off]!=0xFFu)++off;if(off>=n)return 0u;++off;}
    unsigned end=off;while(end<n&&work_ram[0x8000u+end]!=0xFFu)++end;
    return battle_load_formation_record(&work_ram[0x8000u+off],end-off,enemies);
}
static int battle_try_escape(const struct BattleLiteActor party[5],unsigned pn){
    /* Battle_ProcessRUN: at least one active character is required; formation
     * byte +1 >= $F0 cannot be escaped.  Otherwise use the highest living
     * character agility and Battle_CalculateChances with factor 2/lower $28.
     * Only the fail/non-fail result matters to RUN. */
    unsigned highest=0u;for(unsigned i=0u;i<pn;i++)if(!battle_actor_incapacitated(&party[i])&&party[i].agility>highest)highest=party[i].agility;
    if(!highest||battle_random_escape_param>=0xF0u)return 0;
    return battle_calc_chance(highest,battle_random_escape_param,2u,0x28u,0xFFFFu)>=0;
}
static uint8_t battle_event_music(unsigned value){
    /* Exact EventBattleMusicData at $5A63C. The AOT cart currently carries a
     * subset of v18; unsupported source IDs fall back to the resident v18
     * Defeat At A Blow instead of invoking any v19/live synthesis path. */
    return value<=0x1Au?rom8(0x5A63Cu+value):0x95u;
}
static unsigned battle_div_small(unsigned value,unsigned divisor){
    /* Victory-only helper; party size is <=5. Repeated subtraction is bounded
     * by source 16-bit battle EXP and avoids linking ARM EABI division. */
    unsigned q=0u;if(!divisor)return value;while(value>=divisor){value-=divisor;++q;}return q;
}
/* Source BattleResults_PartyExp / BattleResults_LevelUpStats.  Each 22-byte
 * level record is: target EXP, max HP, max TP, STR/MEN/AGI/DEX, learned
 * technique, learned skill, then eight max-skill-use bytes. */
static unsigned battle_apply_levelups_one(unsigned ci){
    static const uint8_t start_level[11]={1u,7u,1u,17u,6u,1u,12u,20u,25u,27u,35u};
    static const uint32_t table[11]={0x2856B0u,0x285F1Cu,0x286704u,0x286F70u,0x28767Cu,0x287E7Au,0x2886E6u,0x288E60u,0x28952Au,0x289B86u,0x28A1B6u};
    if(ci>=11u)return 0u;uint16_t d=(uint16_t)(PS4_CHAR_BASE+ci*0x80u);unsigned gained=0u;
    for(;;){
        unsigned lv=ram16r((uint16_t)(d+0x08u));if(lv>=99u||lv<start_level[ci])break;
        uint32_t rec=table[ci]+(uint32_t)(lv-start_level[ci])*22u;
        if(ram32r((uint16_t)(d+0x0Au))<rom32(rec))break;
        ram16w((uint16_t)(d+0x08u),(uint16_t)(lv+1u));
        ram16w((uint16_t)(d+0x10u),rom16(rec+4u));
        ram16w((uint16_t)(d+0x14u),rom16(rec+6u));
        ram8w((uint16_t)(d+0x18u),rom8(rec+8u)); ram8w((uint16_t)(d+0x1Bu),rom8(rec+9u));
        ram8w((uint16_t)(d+0x1Eu),rom8(rec+10u));ram8w((uint16_t)(d+0x21u),rom8(rec+11u));
        uint8_t tech=rom8(rec+12u),skill=rom8(rec+13u);
        if(tech){for(unsigned i=0u;i<16u;i++)if(!ram8r((uint16_t)(d+0x52u+i))){ram8w((uint16_t)(d+0x52u+i),tech);break;}}
        int learned_skill_slot=-1;if(skill){for(unsigned i=0u;i<8u;i++)if(!ram8r((uint16_t)(d+0x62u+i))){ram8w((uint16_t)(d+0x62u+i),skill);learned_skill_slot=(int)i;break;}}
        /* BattleResults_PartyExp copies only the eight max-use bytes on every level.
         * BattleResults_LevelUpStats initializes current uses only for a newly learned skill. */
        for(unsigned i=0u;i<8u;i++)ram8w((uint16_t)(d+0x6Bu+i*2u),rom8(rec+14u+i));
        if(learned_skill_slot>=0){unsigned i=(unsigned)learned_skill_slot;ram8w((uint16_t)(d+0x6Au+i*2u),ram8r((uint16_t)(d+0x6Bu+i*2u)));}
        update_one_char_derived(ci);++gained;
    }
    return gained;
}
static void ps4_debug_level60_party(void){
    static const uint8_t ids[5]={0u,5u,3u,6u,9u};
    static const uint8_t start[11]={1u,7u,1u,17u,6u,1u,12u,20u,25u,27u,35u};
    static const uint32_t table[11]={0x2856B0u,0x285F1Cu,0x286704u,0x286F70u,0x28767Cu,0x287E7Au,0x2886E6u,0x288E60u,0x28952Au,0x289B86u,0x28A1B6u};
    uint32_t sp=0x2A8ACAu;for(unsigned ci=0u;ci<11u;ci++){int take=0;for(unsigned k=0u;k<5u;k++)if(ids[k]==ci)take=1;
        if(take){uint16_t d=(uint16_t)(PS4_CHAR_BASE+ci*0x80u);for(unsigned j=6u;j<0x80u;j++)ram8w((uint16_t)(d+j),0u);init_one_char(ci,&sp);ram32w((uint16_t)(d+0x0Au),rom32(table[ci]+(59u-start[ci])*22u));battle_apply_levelups_one(ci);update_one_char_derived(ci);}
        else sp+=66u;
    }
    for(unsigned k=0u;k<5u;k++){unsigned ci=ids[k];uint16_t d=(uint16_t)(PS4_CHAR_BASE+ci*0x80u);ram8w((uint16_t)(0xF40Au+k),(uint8_t)ci);ram16w((uint16_t)(d+0x0Eu),ram16r((uint16_t)(d+0x10u)));ram16w((uint16_t)(d+0x12u),ram16r((uint16_t)(d+0x14u)));for(unsigned j=0u;j<8u;j++)ram8w((uint16_t)(d+0x6Au+j*2u),ram8r((uint16_t)(d+0x6Bu+j*2u)));}
    ram16w(0xF43Cu,0u);lead_char_id=0u;hw_chaz_key=0xFFFFu;field_followers_sync(1);
}
static unsigned battle_apply_levelups_party(const struct BattleLiteActor party[5],unsigned pn){
    unsigned total=0u;for(unsigned i=0u;i<pn;i++){if(!party[i].hp||(party[i].status&0x44u))continue;total+=battle_apply_levelups_one(party[i].id);}return total;
}

static void battle_apply_rewards(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],const struct BattleLiteActor party[5],unsigned pn){
    unsigned exp_total=0u,meseta_total=0u;for(unsigned i=0u;i<battle_enemy_count;i++)if(battle_enemy_reward_mask&(1u<<i)){uint32_t p=0x2816BCu+(uint32_t)enemies[i].id*48u;exp_total+=rom16(p+44u);meseta_total+=rom16(p+46u);}
    unsigned survivors=0u;for(unsigned i=0u;i<pn;i++)if(party[i].hp&&!(party[i].status&0x44u))++survivors;
    unsigned share=battle_div_small(exp_total,survivors?survivors:1u);
    for(unsigned i=0u;i<pn;i++){uint16_t d=battle_char_record((unsigned)party[i].id);ram8w((uint16_t)(d+0x7Au),0xFFu);if(!party[i].hp||(party[i].status&0x44u))continue;uint32_t x=ram32r((uint16_t)(d+0x0Au));x+=share;if(x>9999999u)x=9999999u;ram32w((uint16_t)(d+0x0Au),x);}
    uint32_t money=ram32r(PS4_MONEY);money+=meseta_total;if(money>9999999u)money=9999999u;ram32w(PS4_MONEY,money);
    (void)battle_apply_levelups_party(party,pn);
}
static int battle_run_event(unsigned event_index){
    battle_event_index=(uint8_t)event_index;ram8w(0xECEEu,(uint8_t)event_index);
    struct BattleLiteActor party[5],enemies[BATTLE_MAX_ENEMIES];battle_fill_stats();unsigned pn=battle_live_party(party);if(!pn)return 0;
    battle_enemy_count=battle_load_event_formation(event_index,enemies);if(!battle_enemy_count){battle_enemy_count=3u;fail(84u);}
    uint8_t music=battle_event_music(event_index);ps4_audio_stop();if(!ps4_audio_play(music))ps4_audio_play(0x95u);
    unsigned target=0u;struct BattleCommandLite cmds[5];battle_anim_reset();battle_party_death_reset(party,pn);battle_ui_bind(party,pn,enemies,target);for(unsigned oi=0u;oi<128u;oi++)hw_hide_obj(oi);battle_compose_igg(party,pn,enemies,target,0u);battle_present_page0();battle_obj_sync(enemies,event_index==0u?3u:0u);
    for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++)battle_enemy_reaction_flags[i]=0u;battle_enemy_last_regular_slot=0xFFu;battle_enemy_ai_unknown_flag=0u;battle_enemy_ai_hp25_gate=0u;battle_pending_special_death=0xFFu;
    while(battle_party_alive(party,pn)&&battle_enemies_alive(enemies)){
        battle_ui_target=target;battle_ui_actor=0u;int top=battle_choose_main_option(pn>1u);if(top<0)top=0;
        if(top==2){battle_ui_message("CAN'T RUN","EVENT BATTLE");continue;}
        int ready=1,macro_index=-1;if(top==1){int m=battle_choose_macro(party,pn);if(m<0)continue;macro_index=m;battle_load_macro((unsigned)m,party,pn,cmds);}else{
            for(unsigned a=0;a<pn;a++){battle_ui_actor=a;if(!party[a].hp){cmds[a].type=5u;cmds[a].id=0u;cmds[a].target=(uint8_t)a;cmds[a].inv_slot=0xFFu;continue;}if(!battle_command_for_actor(&party[a],&cmds[a])){ready=0;break;}unsigned tk=battle_command_target_kind(&cmds[a]);if(cmds[a].type==1u&&battle_actor_slasher_mask(&party[a]))tk=2u;if(tk==1u){int t=battle_choose_target(party,pn,enemies,target,a);if(t<0){ready=0;break;}cmds[a].target=(uint8_t)t;target=(unsigned)t;}else if(tk==4u||tk==6u||tk==8u){int t=battle_choose_ally(party,pn,a,tk);if(t<0){ready=0;break;}cmds[a].target=(uint8_t)t;}else cmds[a].target=(uint8_t)a;}
        }if(!ready)continue;
        battle_resolve_mixed_round(party,pn,enemies,cmds,macro_index,&target);
        if(battle_enemies_alive(enemies)&&target<battle_enemy_count&&!enemies[target].hp)target=battle_next_target(enemies,target,1);
        battle_store_party(party,pn);battle_ui_target=target;battle_compose_igg(party,pn,enemies,target,0u);battle_present_page0();battle_obj_sync(enemies,0u);
    }
    battle_store_party(party,pn);if(battle_enemies_alive(enemies)){battle_show_defeated_message(party,pn,enemies,target);return 0;}
    ram8w(0x500Au,0x8Bu);ps4_audio_stop();ps4_audio_play(0x8Bu);
    do{unsigned vx0,vy0,vx1,vy1;battle_ui_box_source(10u,10u,17u,4u,&vx0,&vy0,&vx1,&vy1);battle_draw_text6(vx0+18u,vy0+10u,"VICTORY",80u);}while(0);battle_present_page0();battle_obj_sync(enemies,0u);battle_wait_release();for(;;){waitvb();if(md_io_read_psiv_pad()&0xF0u){battle_wait_release();break;}}
    if(field_map.music>=0x81u&&field_map.music<=0xB4u){ps4_audio_stop();ps4_audio_play(field_map.music);}field_random_battle_delay=10u;if(!battle_debug_battle_active){battle_apply_rewards(enemies,party,pn);if(event_index==0u)ps4_event_flag_set(0x0Bu);}return 1;
}

static int battle_run_random(void){
    /* Event_Battle_Index is negative for random battles in the original.  Keep
     * 0xFF here so the static renderer takes its generic source-enemy path. */
    battle_event_index=0xFFu;ram8w(0xECEEu,0xFFu);
    struct BattleLiteActor party[5],enemies[BATTLE_MAX_ENEMIES];battle_fill_stats();unsigned pn=battle_live_party(party);if(!pn)return 0;
    battle_enemy_count=battle_load_random_formation(enemies);if(!battle_enemy_count)return 0;
    ps4_audio_stop();ps4_audio_play(0x8Fu); /* MusicID_MeetThemHeadOn */
    unsigned target=0u;struct BattleCommandLite cmds[5];battle_anim_reset();battle_party_death_reset(party,pn);battle_ui_bind(party,pn,enemies,target);for(unsigned oi=0u;oi<128u;oi++)hw_hide_obj(oi);battle_compose_igg(party,pn,enemies,target,0u);battle_present_page0();battle_obj_sync(enemies,0u);
    for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++)battle_enemy_reaction_flags[i]=0u;battle_enemy_last_regular_slot=0xFFu;battle_enemy_ai_unknown_flag=0u;battle_enemy_ai_hp25_gate=0u;battle_pending_special_death=0xFFu;
    int escaped=0;
    while(battle_party_alive(party,pn)&&battle_enemies_alive(enemies)){
        battle_ui_target=target;battle_ui_actor=0u;int top=battle_choose_main_option(pn>1u);if(top<0)top=0;
        if(top==2){
            if(battle_try_escape(party,pn)){escaped=1;do{unsigned vx0,vy0,vx1,vy1;battle_ui_box_source(10u,10u,17u,4u,&vx0,&vy0,&vx1,&vy1);battle_draw_text6(vx0+18u,vy0+10u,"ESCAPE",80u);}while(0);battle_present_page0();battle_wait_release();for(;;){waitvb();if(md_io_read_psiv_pad()&0xF0u){battle_wait_release();break;}}break;}
            battle_ui_message("CAN'T ESCAPE",0);battle_enemy_round(party,pn,enemies);battle_store_party(party,pn);battle_compose_igg(party,pn,enemies,target,0u);battle_present_page0();battle_obj_sync(enemies,0u);continue;
        }
        int ready=1,macro_index=-1;if(top==1){int m=battle_choose_macro(party,pn);if(m<0)continue;macro_index=m;battle_load_macro((unsigned)m,party,pn,cmds);}else{
            for(unsigned a=0;a<pn;a++){battle_ui_actor=a;if(!party[a].hp){cmds[a].type=5u;cmds[a].id=0u;cmds[a].target=(uint8_t)a;cmds[a].inv_slot=0xFFu;continue;}if(!battle_command_for_actor(&party[a],&cmds[a])){ready=0;break;}unsigned tk=battle_command_target_kind(&cmds[a]);if(cmds[a].type==1u&&battle_actor_slasher_mask(&party[a]))tk=2u;if(tk==1u){int t=battle_choose_target(party,pn,enemies,target,a);if(t<0){ready=0;break;}cmds[a].target=(uint8_t)t;target=(unsigned)t;}else if(tk==4u||tk==6u||tk==8u){int t=battle_choose_ally(party,pn,a,tk);if(t<0){ready=0;break;}cmds[a].target=(uint8_t)t;}else cmds[a].target=(uint8_t)a;}
        }if(!ready)continue;
        battle_resolve_mixed_round(party,pn,enemies,cmds,macro_index,&target);
        if(battle_enemies_alive(enemies)&&target<battle_enemy_count&&!enemies[target].hp)target=battle_next_target(enemies,target,1);
        battle_store_party(party,pn);battle_ui_target=target;battle_compose_igg(party,pn,enemies,target,0u);battle_present_page0();battle_obj_sync(enemies,0u);
    }
    battle_store_party(party,pn);
    if(!escaped&&battle_enemies_alive(enemies)){battle_show_defeated_message(party,pn,enemies,target);return 0;}
    if(!escaped){do{unsigned vx0,vy0,vx1,vy1;battle_ui_box_source(10u,10u,17u,4u,&vx0,&vy0,&vx1,&vy1);battle_draw_text6(vx0+18u,vy0+10u,"VICTORY",80u);}while(0);battle_present_page0();battle_obj_sync(enemies,0u);battle_wait_release();for(;;){waitvb();if(md_io_read_psiv_pad()&0xF0u){battle_wait_release();break;}}}
    if(field_map.music>=0x81u&&field_map.music<=0xB4u){ps4_audio_stop();ps4_audio_play(field_map.music);}field_random_battle_delay=10u;
    if(escaped)return 1;if(!battle_debug_battle_active)battle_apply_rewards(enemies,party,pn);return 1;
}
static int battle_run_debug_vehicle(unsigned vehicle,unsigned enemy){
    if(vehicle<1u||vehicle>3u)return 0;unsigned saved=ram16r(0xF43Cu);ram16w(0xF43Cu,(uint16_t)vehicle);int r=battle_run_debug_enemy(enemy);ram16w(0xF43Cu,(uint16_t)saved);return r;
}
static int battle_run_debug_random(void){battle_debug_battle_active=1u;int r=battle_run_random();battle_debug_battle_active=0u;return r;}
static int battle_run_debug_boss(unsigned event_id){battle_debug_battle_active=1u;int r=battle_run_event(event_id);battle_debug_battle_active=0u;return r;}
static int battle_run_debug_enemy(unsigned enemy_id){
    if(enemy_id>=153u)return 0;battle_debug_enemy_pending=(uint8_t)enemy_id;battle_debug_battle_active=1u;int r=battle_run_random();battle_debug_enemy_pending=0xFFu;battle_debug_battle_active=0u;return r;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int field_random_battle_check(void){
    if(!field_step_completed)return 0;field_step_completed=0u;
    if(!ram8r(0xECE7u))return 0; /* Source Random_Battles_Flag. */
    uint8_t standing=ram8r(0xEC80u);if(standing==1u||standing==2u)return 0;
    /* RunRandomBattles rejects encounters beside any transition collision tile. */
    for(int rel=0;rel<8;rel++)if(field_collision_ring(rel)==1u)return 0;
    unsigned group;if(!battle_random_group_for_field(&group))return 0;
    if(field_random_battle_delay>1u){--field_random_battle_delay;return 0;}field_random_battle_delay=1u;
    if(field_npc_rng_next()&(ram16r(0xF43Cu)?0x7Fu:0x1Fu))return 0;
    return battle_run_random();
}

/* PSIV Interaction_ChkMapAreas.  Records are 10 bytes: x,y,XYRange kind,
 * flag type, flag id, InteractionRoutines index, value.  The check samples the
 * tile adjacent to the lead character in the facing direction. */
__attribute__((noinline))
static int field_interact_area(unsigned* camx,unsigned* camy){
    int px=chaz.x,py=chaz.y;if(chaz.facing==0u)py+=16;else if(chaz.facing==1u)py-=16;else if(chaz.facing==2u)px+=16;else px-=16;
    uint32_t p=field_map.interactions;
    while(rom16(p)!=0xFFFFu){int tx=(int)rom16(p)*8,ty=(int)rom16(p+2u)*8;unsigned kind=rom16(p+4u);uint8_t flag_type=rom8(p+6u),flag=rom8(p+7u),routine=rom8(p+8u),value=rom8(p+9u);
        if(field_xy_range(kind,tx,ty,px,py)){
            /* EventFlags_Set/ChestFlags_Set/TempEveFlags_Set are test-and-set
             * operations on 68k (BSET's Z result tells whether it was old).
             * Reproduce that before dispatch so every map record has the same
             * one-shot/repeat semantics as the source. */
            if(ps4_flag_test_and_set(flag_type,flag)){p+=10u;continue;}
            if(routine==0u){field_dialogue_show(value,*camx,*camy);return 1;}
            if(routine==1u){if(ps4_inventory_add(value)<0)ps4_field_popup_text("INVENTORY FULL",0);else ps4_field_popup_found_item(value);hw_field_init(*camx,*camy);return 1;}
            if(routine==2u){uint16_t event_index=rom16(0x64F22u+(uint32_t)value*2u);if(event_index==0x006Bu){if(ps4_event_flag_test(0x0Bu))return 1;battle_run_event(0u);return 2;}if(field_dispatch_event_index(event_index,camx,camy))return 1;return 1;}
            if(routine==3u){/* Area records carry a byte; pointer-based Dialogue2 is used primarily by tile/special checks. */field_dialogue_show(value,*camx,*camy);return 1;}
            if(routine==4u){uint32_t add=(uint32_t)value*100u,m=ram32r(PS4_MONEY);m+=add;if(m>9999999u)m=9999999u;ram32w(PS4_MONEY,m);ps4_field_popup_found_money(value);hw_field_init(*camx,*camy);return 1;}
            if(routine==5u){ps4_shop_run(value);hw_field_init(*camx,*camy);return 1;}
            if(routine==6u){battle_run_event(value);return 2;}
            return 1;
        }p+=10u;
    }return 0;
}

/* PSIV Interaction_ChkTiles ($58ECA).  The source chooses a table by
 * Map_General_Var, compares the chunk index returned by GetChunkAndCollision,
 * and dispatches a direct Dialogue2 ROM pointer.  All 45 source records begin
 * with the ordinary $F4 text control, so no map-dialogue-tree substitution is
 * needed for the initial pointer. */
__attribute__((noinline))
static int field_interact_tile(unsigned* camx,unsigned* camy){
    int dx=0,dy=0;if(chaz.facing==0u)dy=16;else if(chaz.facing==1u)dy=-16;else if(chaz.facing==2u)dx=16;else dx=-16;
    uint8_t chunk=field_chunk_index_at(chaz.x,chaz.y,dx,dy);if(field_map.general>0x35u)return 0;
    const uint32_t base=0x58F12u;uint32_t p=base+rom16(base+(uint32_t)field_map.general*2u);
    for(unsigned guard=0u;guard<64u&&p+5u<0x300000u;guard++,p+=6u){uint8_t c=rom8(p);if(!c)break;if(c==chunk){uint32_t rp=rom32(p+2u);int ev=field_dialogue_show_rom_ptr(rp,*camx,*camy);if(ev)return field_dispatch_event_index((unsigned)ev,camx,camy);return 1;}}
    return 0;
}
/* PSIV Interaction_ChkObjsSpecial + loc_68394.  Retail counters use collision
 * $C and place the shopkeeper one additional tile behind the counter.  The
 * source table's high byte selects the service family; $01 is the item/weapon
 * shop family and the low byte selects ShopInventories. */
__attribute__((noinline))
static int field_interact_special_shop(unsigned* camx,unsigned* camy){
    int dx=0,dy=0;if(chaz.facing==0u)dy=16;else if(chaz.facing==1u)dy=-16;else if(chaz.facing==2u)dx=16;else dx=-16;
    if(field_collision_at(chaz.x,chaz.y,dx,dy)!=0x0Cu)return 0;
    int tx=chaz.x+dx*2,ty=chaz.y+dy*2,oi=-1;
    for(unsigned i=0;i<field_obj_count;i++)if(field_objs[i].active){int ax=(int)field_objs[i].x-tx,ay=(int)field_objs[i].y-ty;if(ax<0)ax=-ax;if(ay<0)ay=-ay;if(ax<=8&&ay<=8){oi=(int)i;break;}}
    if(oi<0)return 0;uint16_t ox=field_objs[oi].x,oy=field_objs[oi].y;uint32_t p=0x68394u;
    for(unsigned guard=0u;guard<96u&&p+7u<0x300000u;guard++,p+=8u){uint16_t map=rom16(p);if(map==0xFFFFu)break;if(map==field_map.map_id&&rom16(p+2u)==ox&&rom16(p+4u)==oy){uint16_t service=rom16(p+6u);if((service>>8)==1u){ps4_shop_run(service&0xFFu);hw_field_init(*camx,*camy);return 1;}return 0;}}
    return 0;
}

/* Shared lowering of source map-layout event primitives.  PSIV's event
 * executor mutates decompressed layout bytes in RAM and redraws; keeping this
 * here makes elevators, platforms, conveyors and later scripted doors use the
 * same path instead of map-specific coordinate patches. */
static uint16_t field_layout_off_for_plane(unsigned plane){return plane?0xB000u:0xA000u;}
/* Exact lowering of GetMapLayoutChunk ($53514): unlike GetChunkAndCollision,
 * this helper uses the selected plane's own row-size byte as its stride. */
static int field_layout_index_world(unsigned wx,unsigned wy,unsigned plane,unsigned* idx){
    if(field_map.is_world)return 0;unsigned cw=(unsigned)(plane?field_map.row_bg:field_map.row_fg)+1u;
    unsigned ch=(unsigned)(plane?field_map.col_bg:field_map.col_fg)+1u,cx=wx>>5,cy=wy>>5;
    if(cx>=cw||cy>=ch)return 0;*idx=cy*cw+cx;return 1;
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static uint8_t field_layout_chunk_world(unsigned wx,unsigned wy,unsigned plane){
    unsigned i;if(!field_layout_index_world(wx,wy,plane,&i))return 0xFFu;
    return work_ram[(uint16_t)(field_layout_off_for_plane(plane)+i)];
}
static int field_layout_write_world(unsigned wx,unsigned wy,unsigned plane,uint8_t value){
    unsigned i;if(!field_layout_index_world(wx,wy,plane,&i))return 0;uint16_t off=field_layout_off_for_plane(plane);
    if(work_ram[(uint16_t)(off+i)]==value)return 1;work_ram[(uint16_t)(off+i)]=value;
    field_dirty_chunk_mark(plane,wx>>5,wy>>5);return 1;
}
static int field_layout_pair_world(unsigned wx,unsigned wy,unsigned plane,uint8_t first,uint8_t second){
    unsigned i;if(!field_layout_index_world(wx,wy,plane,&i))return 0;
    unsigned cw=(unsigned)(plane?field_map.row_bg:field_map.row_fg)+1u,cells=cw*((unsigned)(plane?field_map.col_bg:field_map.col_fg)+1u);
    if(i+1u>=cells)return 0;uint16_t off=field_layout_off_for_plane(plane);uint8_t a=work_ram[(uint16_t)(off+i)],b=work_ram[(uint16_t)(off+i+1u)];
    unsigned cx=wx>>5,cy=wy>>5;if(a!=first){work_ram[(uint16_t)(off+i)]=first;field_dirty_chunk_mark(plane,cx,cy);}
    if(b!=second){unsigned j=i+1u,nx=cx+1u,ny=cy;if(nx>=cw){nx=0u;++ny;}work_ram[(uint16_t)(off+j)]=second;field_dirty_chunk_mark(plane,nx,ny);}
    return 1;
}
static uint8_t field_fg_layout_chunk_world(unsigned wx,unsigned wy){return field_layout_chunk_world(wx,wy,0u);}
static int field_fg_layout_pair_world(unsigned wx,unsigned wy,uint8_t first,uint8_t second){return field_layout_pair_world(wx,wy,0u,first,second);}
static uint8_t field_collision_plane_layout_chunk_world(unsigned wx,unsigned wy){return field_layout_chunk_world(wx,wy,field_map.collision_plane?1u:0u);}
static void field_event_redraw(unsigned* camx,unsigned* camy){
    choose_camera(camx,camy);
    field_publish_layout_dirty((int)(*camx>>3),(int)(*camy>>3));
    hw_set_scroll(*camx,*camy);hw_update_objects(*camx,*camy);
}
/* Scripted Event_MoveCamera must not snap back through choose_camera().  The
 * source routine updates X fully, then Y, calling Field_UpdateObjects and
 * VInt_Prepare on every step.  Keep an explicit-camera redraw path for that
 * choreography and for loc_5A71E's counted object-update waits. */
static void field_event_redraw_explicit(unsigned camx,unsigned camy){
    field_publish_layout_dirty((int)(camx>>3),(int)(camy>>3));
    hw_set_scroll(camx,camy);hw_update_objects(camx,camy);
}
static void field_event_choreography_tick(unsigned camx,unsigned camy){
    r30_tile_animation_update();field_run_map_updates();field_update_npcs(camx,camy);field_event_redraw_explicit(camx,camy);waitvb();
}
static void field_event_wait_updates(unsigned count,unsigned* camx,unsigned* camy){
    /* loc_5A71E is DBF-driven: D0=0 executes once, so authored count N is N+1 frames. */
    ram16w(0xEF02u,0u);
    for(unsigned i=0u;i<=count;i++)field_event_choreography_tick(*camx,*camy);
}
static void field_event_move_camera_to(uint16_t world_x,uint16_t world_y,unsigned step,unsigned* camx,unsigned* camy){
    if(!step)step=1u;
    /* Event_MoveCamera subtracts the Genesis screen centre ($98,$58) and masks
     * each target to 12 bits before moving X and then Y. */
    unsigned tx=((unsigned)world_x-0x98u)&0x0FFFu,ty=((unsigned)world_y-0x58u)&0x0FFFu;
    while(*camx!=tx){unsigned d=*camx<tx?tx-*camx:*camx-tx;if(d>step)d=step;*camx=*camx<tx?*camx+d:*camx-d;ram16w(0xEF94u,(uint16_t)*camx);field_event_choreography_tick(*camx,*camy);}
    while(*camy!=ty){unsigned d=*camy<ty?ty-*camy:*camy-ty;if(d>step)d=step;*camy=*camy<ty?*camy+d:*camy-d;ram16w(0xEF90u,(uint16_t)*camy);field_event_choreography_tick(*camx,*camy);}
}
static void field_event_translate_party(int dx,int dy,unsigned* camx,unsigned* camy){
    int ax=dx<0?-dx:dx,ay=dy<0?-dy:dy,dist=ax>ay?ax:ay;if(!dist)return;
    int sx=dx<0?-2:(dx>0?2:0),sy=dy<0?-2:(dy>0?2:0);int moved=0;
    while(moved<dist){int n=dist-moved;if(n>2)n=2;int mx=sx?((sx<0)?-n:n):0,my=sy?((sy<0)?-n:n):0;chaz.x+=mx;chaz.y+=my;
        for(unsigned i=0;i<field_follower_count;i++)if(field_followers[i].active){field_followers[i].x+=mx;field_followers[i].y+=my;field_followers[i].tx+=mx;field_followers[i].ty+=my;}
        moved+=n;waitvb();field_event_redraw(camx,camy);
    }
    chaz.step_left=0u;chaz.dx=chaz.dy=0;
}
static int field_rect_hit(unsigned x,unsigned y,unsigned rx,unsigned ry,unsigned w,unsigned h){return x>=rx&&x<rx+w&&y>=ry&&y<ry+h;}

/* Event_VahFortMovingPlatform1/2 and Event_WpnPlntMovingPlatform1..4 share
 * one source routine shape.  Each routine owns a 6-byte source-state record
 * and an 8-byte destination record at its tail; a TempFlag selects the
 * direction.  Decode those ROM records directly so the port remains tied to
 * the PSIV source data rather than duplicated platform coordinates. */
static int field_event_moving_platform(unsigned event_index,unsigned* camx,unsigned* camy){
    static const uint8_t flags[6]={0x09u,0x0Au,0x0Du,0x0Eu,0x0Fu,0x10u};
    static const uint32_t event_end[6]={0x06C608u,0x06C798u,0x06C928u,0x06CAB8u,0x06CC48u,0x06CDD4u};
    if(event_index<0x15u||event_index>0x1Au)return 0;unsigned n=event_index-0x15u;unsigned old=ps4_flag_test_type(2u,flags[n])?1u:0u;
    uint32_t p1=event_end[n]-0x1Cu+(old?6u:0u),p2=event_end[n]-0x10u+(old?8u:0u);
    unsigned sx=rom16(p1),sy=rom16(p1+2u);field_fg_layout_pair_world(sx,sy,rom8(p1+4u),rom8(p1+5u));field_event_redraw(camx,camy);
    int target_y=(int)rom16(p2+4u)-0x20;field_event_translate_party(0,target_y-chaz.y,camx,camy);
    ps4_flag_toggle_type(2u,flags[n]);field_fg_layout_pair_world(rom16(p2+2u),rom16(p2+4u),rom8(p2+6u),rom8(p2+7u));field_event_redraw(camx,camy);return 1;
}
static int field_event_conveyor(unsigned event_index,unsigned* camx,unsigned* camy){
    int dx=0,dy=0;if(event_index==0x1Du)dy=16;else if(event_index==0x1Eu)dy=-16;else if(event_index==0x1Fu)dx=16;else if(event_index==0x20u)dx=-16;else return 0;
    field_event_translate_party(dx,dy,camx,camy);return 1;
}
/* Event_VahalFortTerminal ($1B) and Event_WeaponPlantTerminal ($1C) are
 * the same source primitive with a different X split and TempFlag pair.
 * Both source routines end in TempFlags_Toggle, so preserve toggle (not set)
 * semantics; the platform/conveyor condition layer consumes these flags. */
static int field_event_terminal(unsigned event_index,unsigned* camx,unsigned* camy){
    unsigned split,left_flag,right_flag;
    if(event_index==0x1Bu){split=0x01F0u;left_flag=0x0Cu;right_flag=0x0Bu;}
    else if(event_index==0x1Cu){split=0x0300u;left_flag=0x11u;right_flag=0x12u;}
    else return 0;
    ps4_flag_toggle_type(2u,(unsigned)chaz.x<split?left_flag:right_flag);
    /* The Genesis routine surrounds the toggle with terminal UI/SFX calls.
     * The gameplay-visible machinery state changes immediately; redraw once
     * through the common field path so every consumer sees the same state. */
    field_event_redraw(camx,camy);
    return 1;
}
static int field_event_elevator_door(unsigned* camx,unsigned* camy){
    /* Event_ElevatorDoorOpening ($6C2BC) asks GetMapLayoutChunk for the lead's
     * current collision plane, verifies $4F, then writes $50,$51,$52,$53 to
     * that exact same layout byte.  Do not offset to a neighboring cell. */
    unsigned plane=field_map.collision_plane?1u:0u,idx;
    if(!field_layout_index_world((unsigned)chaz.x,(unsigned)chaz.y,plane,&idx))return 1;
    uint16_t off=field_layout_off_for_plane(plane);if(work_ram[(uint16_t)(off+idx)]!=0x4Fu)return 1;
    static const uint8_t frames[4]={0x50u,0x51u,0x52u,0x53u};
    for(unsigned q=0;q<4u;q++){work_ram[(uint16_t)(off+idx)]=frames[q];hw_ring_layout_dirty=1u;field_event_redraw(camx,camy);waitvb();waitvb();}
    return 1;
}
static int field_event_ride_elevator(unsigned* camx,unsigned* camy){
    /* Event_RidingElevator ($6C33E): force the lift-side layout chunk open,
     * move the whole active party one source tile, then close the chunk behind
     * them through $52,$51,$50,$4F.  All layout access follows the map's
     * collision plane, matching GetMapLayoutChunk. */
    unsigned plane=field_map.collision_plane?1u:0u;
    field_layout_write_world((unsigned)chaz.x,(unsigned)(chaz.y+16),plane,0x53u);field_event_redraw(camx,camy);
    field_event_translate_party(0,16,camx,camy);
    unsigned idx;if(field_layout_index_world((unsigned)chaz.x,(unsigned)(chaz.y-16),plane,&idx)){
        uint16_t off=field_layout_off_for_plane(plane);static const uint8_t frames[4]={0x52u,0x51u,0x50u,0x4Fu};
        for(unsigned q=0;q<4u;q++){work_ram[(uint16_t)(off+idx)]=frames[q];hw_ring_layout_dirty=1u;field_event_redraw(camx,camy);waitvb();waitvb();}
    }
    return 1;
}
static int field_find_object_type(uint16_t type){for(unsigned i=0;i<field_obj_count;i++)if(field_objs[i].active&&field_objs[i].type==type)return (int)i;return -1;}
static void field_animate_alys_to_chaz(unsigned oi,unsigned camx,unsigned camy){struct FieldObjLite* a=&field_objs[oi];if(a->y!=chaz.y){unsigned target=(unsigned)a->y+16u;while((unsigned)a->y<target){waitvb();a->y=(uint16_t)(a->y+2u);hw_update_objects(camx,camy);}}a->facing=8u;hw_refresh_field_object_art(oi);hw_update_objects(camx,camy);}
static void field_animate_hahn_join(unsigned oi,unsigned camx,unsigned camy){struct FieldObjLite* h=&field_objs[oi];if((int)h->x!=chaz.x){int dir=((int)h->x<chaz.x)?1:-1;int target=(int)h->x+dir*16;while((dir>0&&(int)h->x<target)||(dir<0&&(int)h->x>target)){waitvb();h->x=(uint16_t)((int)h->x+dir*2);hw_update_objects(camx,camy);}}h->facing=0u;hw_refresh_field_object_art(oi);hw_update_objects(camx,camy);}
static int field_event_dialogue_set_flag(unsigned dialogue,unsigned flag,unsigned* camx,unsigned* camy){
    field_dialogue_show(dialogue,*camx,*camy);
    ps4_event_flag_set(flag);
    return 1;
}
static int field_event_dialogue_once(unsigned flag,unsigned first_dialogue,unsigned repeat_dialogue,unsigned* camx,unsigned* camy){
    if(ps4_event_flag_test(flag)){field_dialogue_show(repeat_dialogue,*camx,*camy);return 1;}
    field_dialogue_show(first_dialogue,*camx,*camy);ps4_event_flag_set(flag);return 1;
}
static void field_event_add_money(uint32_t amount){ram32w(0xF438u,ram32r(0xF438u)+amount);}
/* Build-time static translation of EventPtrs routines whose complete 68k body
 * is exactly Event_GetAndRunDialogue + EventFlags_Set.  The generator refuses
 * routines with any extra choreography, so this extends story coverage without
 * turning the ARM runtime into a 68000 interpreter or silently dropping side
 * effects. */
static int field_event_simple_dialogue_flag(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_SIMPLE_EVENT_DESCRIPTOR_COUNT;i++){
        const struct Ps4SimpleEventDescriptor* e=&ps4_simple_event_descriptors[i];
        if((unsigned)e->event_index==event_index){field_dialogue_show(e->dialogue,*camx,*camy);ps4_event_flag_set(e->flag);return 1;}
    }
    return 0;
}
static void field_event_face_source_object(unsigned slot,unsigned* camx,unsigned* camy){
    if(slot>=field_obj_count||!field_objs[slot].active)return;
    struct FieldObjLite* o=&field_objs[slot];int dx=chaz.x-(int)o->x,dy=chaz.y-(int)o->y;
    if(dx<0)dx=-dx;if(dy<0)dy=-dy;
    if(dx>dy)o->facing=(uint8_t)((chaz.x>(int)o->x?2u:3u)<<2);
    else o->facing=(uint8_t)((chaz.y>(int)o->y?0u:1u)<<2);
    hw_refresh_field_object_art(slot);hw_update_objects(*camx,*camy);
}
static void field_event_set_source_object_facing(unsigned slot,uint8_t source_facing,unsigned* camx,unsigned* camy){
    if(slot>=field_obj_count||!field_objs[slot].active)return;
    field_objs[slot].facing=source_facing;
    ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u+0x06u),source_facing);
    hw_refresh_field_object_art(slot);hw_update_objects(*camx,*camy);
}
static void field_event_set_source_object_dialogue(unsigned slot,uint8_t dialogue){
    if(slot<32u)ram8w((uint16_t)(0xC300u+(uint16_t)slot*0x40u+0x14u),dialogue);
    if(slot<field_obj_count)field_objs[slot].dialog=dialogue;
}
static int field_event_facing_dialogue_flag(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_FACING_DIALOGUE_FLAG_DESCRIPTOR_COUNT;i++){
        const struct Ps4FacingDialogueFlagDescriptor* e=&ps4_facing_dialogue_flag_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_event_face_source_object(e->object_slot,camx,camy);
        field_dialogue_show(e->dialogue,*camx,*camy);ps4_event_flag_set(e->flag);return 1;
    }
    return 0;
}

/* Exact source templates for the small EventPtrs families that manipulate
 * temporary flags or multiple story flags.  The descriptors are emitted only
 * after a complete body match against the pinned PSIV ROM. */
static int field_event_temp_flag_clear(unsigned event_index){
    for(unsigned i=0u;i<PS4_TEMP_FLAG_CLEAR_DESCRIPTOR_COUNT;i++){
        const struct Ps4TempFlagClearDescriptor* e=&ps4_temp_flag_clear_descriptors[i];
        if((unsigned)e->event_index==event_index){ps4_flag_clear_type(2u,e->flag);return 1;}
    }
    return 0;
}
static int field_event_dialogue_two_flags(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_DIALOGUE_TWO_FLAGS_DESCRIPTOR_COUNT;i++){
        const struct Ps4DialogueTwoFlagsDescriptor* e=&ps4_dialogue_two_flags_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_dialogue_show(e->dialogue,*camx,*camy);ps4_event_flag_set(e->first_flag);ps4_event_flag_set(e->second_flag);return 1;
    }
    return 0;
}


/* Build-generated lowering of the simple FieldRoutine_Cutscene subset.  The
 * generator accepts a cutscene only when every 68000 instruction and helper
 * call belongs to the audited typed vocabulary in generated_cutscene_scripts.
 * Unsupported cutscenes stay unhandled rather than silently dropping source
 * side effects. */
static void field_cutscene_sound(uint8_t q){
    /* Every source cutscene sound write goes through Sound_Index.  ram8w()
     * dispatches that register centrally, preserving the original distinction
     * between music, normal SFX, special SFX and driver commands ($FB-$FE).
     * In particular $F3 is Selection, not Stop, and $FB is the source fade
     * command rather than an immediate hardware stop. */
    ram8w(0x500Au,q);
}
static const struct Ps4CutsceneScript* field_find_generated_cutscene(unsigned cutscene_index){
    for(unsigned i=0u;i<PS4_CUTSCENE_SCRIPT_COUNT;i++)if((unsigned)ps4_cutscene_scripts[i].cutscene_index==cutscene_index)return &ps4_cutscene_scripts[i];
    return 0;
}
static int field_execute_generated_cutscene(const struct Ps4CutsceneScript* script,unsigned start_op,unsigned* camx,unsigned* camy){
    if(!script||start_op>script->op_count)return 0;
    for(unsigned n=start_op;n<script->op_count;n++){
        const struct Ps4CutsceneOp* op=&ps4_cutscene_ops[(unsigned)script->first_op+n];
        if(op->opcode==PS4_CUTOP_SOUND)field_cutscene_sound(op->arg8);
        else if(op->opcode==PS4_CUTOP_DIALOGUE)field_dialogue_show(op->arg8,*camx,*camy);
        else if(op->opcode==PS4_CUTOP_FLAG_SET)ps4_event_flag_set(op->arg8);
        else if(op->opcode==PS4_CUTOP_RAM8)ram8w(op->addr,op->arg8);
        else if(op->opcode==PS4_CUTOP_RAM16)ram16w(op->addr,(uint16_t)op->value);
        else if(op->opcode==PS4_CUTOP_ADD32){uint32_t v=ram32r(op->addr);v+=op->value;if(op->addr==0xF438u&&v>9999999u)v=9999999u;ram32w(op->addr,v);}
        else if(op->opcode==PS4_CUTOP_REFRESH_MAP){
            /* RefreshMap returns to the same 68k routine only after the new map
             * is resident.  Suspend exactly at that seam and resume after the
             * outer field loader has consumed EC28/EC48/EC4A/EC44. */
            field_event_reload_pending=1u;
            if(n+1u<script->op_count){field_cutscene_resume_index=script->cutscene_index;field_cutscene_resume_op=(uint8_t)(n+1u);}
            else {field_cutscene_resume_index=0xFFu;field_cutscene_resume_op=0u;}
            return 1;
        }
        else if(op->opcode==PS4_CUTOP_FADE_IN){field_palette_fade_in();}
        else if(op->opcode==PS4_CUTOP_PREP){field_event_redraw(camx,camy);}
        else return 0;
    }
    field_cutscene_resume_index=0xFFu;field_cutscene_resume_op=0u;
    return 1;
}
static int field_dispatch_generated_cutscene(unsigned cutscene_index,unsigned* camx,unsigned* camy){
    const struct Ps4CutsceneScript* script=field_find_generated_cutscene(cutscene_index);
    if(!script)return 0;
    field_cutscene_resume_index=0xFFu;field_cutscene_resume_op=0u;
    return field_execute_generated_cutscene(script,0u,camx,camy);
}
static int field_resume_generated_cutscene(unsigned* camx,unsigned* camy){
    if(field_cutscene_resume_index==0xFFu)return 0;
    unsigned idx=field_cutscene_resume_index,start=field_cutscene_resume_op;
    /* Clear first; another RefreshMap encountered by the resumed portion will
     * install the next continuation atomically. */
    field_cutscene_resume_index=0xFFu;field_cutscene_resume_op=0u;
    return field_execute_generated_cutscene(field_find_generated_cutscene(idx),start,camx,camy);
}

static const struct Ps4TransitionEventScript* field_find_transition_event_script(unsigned event_index){
    for(unsigned i=0u;i<PS4_TRANSITION_EVENT_SCRIPT_COUNT;i++)if((unsigned)ps4_transition_event_scripts[i].event_index==event_index)return &ps4_transition_event_scripts[i];
    return 0;
}
static int field_execute_transition_event_script(const struct Ps4TransitionEventScript* script,unsigned start_op,unsigned* camx,unsigned* camy){
    if(!script||start_op>script->op_count)return 0;
    for(unsigned n=start_op;n<script->op_count;n++){
        const struct Ps4TransitionEventOp* op=&ps4_transition_event_ops[(unsigned)script->first_op+n];
        if(op->opcode==PS4_EVTTR_DIALOGUE)field_dialogue_show(op->arg8,*camx,*camy);
        else if(op->opcode==PS4_EVTTR_FLAG_SET)ps4_event_flag_set(op->arg8);
        else if(op->opcode==PS4_EVTTR_SOUND)field_cutscene_sound(op->arg8);
        else if(op->opcode==PS4_EVTTR_FADE_OUT)field_palette_fade_out();
        else if(op->opcode==PS4_EVTTR_RAM16)ram16w(op->addr,(uint16_t)op->value);
        else if(op->opcode==PS4_EVTTR_MAPLOAD_CLEAR)ram8w(op->addr,(uint8_t)(ram8r(op->addr)&(uint8_t)~(1u<<op->arg8)));
        else if(op->opcode==PS4_EVTTR_REFRESH_MAP){
            field_event_reload_pending=1u;
            if(n+1u<script->op_count){field_transition_resume_index=script->event_index;field_transition_resume_op=(uint8_t)(n+1u);}
            else {field_transition_resume_index=0xFFu;field_transition_resume_op=0u;}
            return 1;
        }
        else if(op->opcode==PS4_EVTTR_FACE_OBJECT){
            unsigned slot=op->arg8;
            if(slot<field_obj_count&&field_objs[slot].active){field_objs[slot].facing=(uint8_t)op->value;hw_refresh_field_object_art(slot);hw_update_objects(*camx,*camy);}
        }
        else if(op->opcode==PS4_EVTTR_FADE_IN)field_palette_fade_in();
        else if(op->opcode==PS4_EVTTR_RAM8)ram8w(op->addr,op->arg8);
        else if(op->opcode==PS4_EVTTR_TREE_LOAD){unsigned q=kos_decode(op->value,field_dialogue,FIELD_DIALOGUE_CAP);if(!q)return 0;field_dialogue_len=q;}
        else if(op->opcode==PS4_EVTTR_REMOVE_ALL_ITEM)ps4_inventory_remove_all(op->arg8);
        else if(op->opcode==PS4_EVTTR_TYPED_FLAG_SET)ps4_flag_test_and_set((unsigned)op->addr,op->arg8);
        else if(op->opcode==PS4_EVTTR_COPY32)ram32w(op->addr,ram32r((uint16_t)op->value));
        else if(op->opcode==PS4_EVTTR_COPY8)ram8w(op->addr,ram8r((uint16_t)op->value));
        else if(op->opcode==PS4_EVTTR_BRANCH_FLAG_CLEAR){if(!ps4_event_flag_test(op->arg8))n+=(unsigned)op->value;}
        else if(op->opcode==PS4_EVTTR_RETURN){field_transition_resume_index=0xFFu;field_transition_resume_op=0u;return 1;}
        else if(op->opcode==PS4_EVTTR_RAM32)ram32w(op->addr,op->value);
        else if(op->opcode==PS4_EVTTR_PARTY_SYNC){lead_char_id=ram8r(0xF40Au);field_followers_sync(1);hw_chaz_key=0xFFFFu;}
        else if(op->opcode==PS4_EVTTR_MOVE_CAMERA)field_event_move_camera_to(op->addr,(uint16_t)op->value,op->arg8,camx,camy);
        else if(op->opcode==PS4_EVTTR_WAIT_UPDATES)field_event_wait_updates(op->arg8,camx,camy);
        else if(op->opcode==PS4_EVTTR_BRANCH_FLAG_SET){if(ps4_event_flag_test(op->arg8))n+=(unsigned)op->value;}
        else if(op->opcode==PS4_EVTTR_ENSURE_SAVED_SOUND){
            if(ram8r(0xECECu)!=op->arg8&&op->arg8){field_cutscene_sound(op->arg8);ram8w(0xECECu,op->arg8);}
        }
        else if(op->opcode==PS4_EVTTR_VINT)waitvb();
        else if(op->opcode==PS4_EVTTR_FACE_OBJECT_OPPOSITE)field_event_set_source_object_facing(op->arg8,(uint8_t)((chaz.facing^1u)<<2),camx,camy);
        else return 0;
    }
    field_transition_resume_index=0xFFu;field_transition_resume_op=0u;
    return 1;
}
static int field_dispatch_transition_event_script(unsigned event_index,unsigned* camx,unsigned* camy){
    const struct Ps4TransitionEventScript* script=field_find_transition_event_script(event_index);
    if(!script)return 0;
    field_transition_resume_index=0xFFu;field_transition_resume_op=0u;
    return field_execute_transition_event_script(script,0u,camx,camy);
}
static int field_resume_transition_event_script(unsigned* camx,unsigned* camy){
    if(field_transition_resume_index==0xFFu)return 0;
    unsigned idx=field_transition_resume_index,start=field_transition_resume_op;
    field_transition_resume_index=0xFFu;field_transition_resume_op=0u;
    return field_execute_transition_event_script(field_find_transition_event_script(idx),start,camx,camy);
}

static void field_event_set_mapload_bit(unsigned bit){if(bit<8u)ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)|(uint8_t)(1u<<bit)));}
static int field_event_run_battle_request(uint8_t battle_index,unsigned* camx,unsigned* camy){
    ram8w(0xECFCu,battle_index);ram8w(0xEC27u,(uint8_t)(ram8r(0xEC27u)|8u));
    battle_run_event(battle_index);
    ram8w(0xEC27u,(uint8_t)(ram8r(0xEC27u)&(uint8_t)~8u));
    /* Defeat has already blanked/stopped the battle and requested the title.
     * Do not rebuild a field frame underneath that request; doing so was the
     * event-battle analogue of the old one-frame post-defeat menu leak. */
    if(ps4_return_to_title)return 1;
    /* Event battles may deliberately modify Palette_Table_Buffer before combat
     * (for example EventPtr $40).  The original battle return rebuilds field
     * presentation; restore the current map's authored palette before the GBA
     * field hardware is reinitialized so those effects cannot leak afterward. */
    field_reload_palette_from_map();
    choose_camera(camx,camy);hw_field_init(*camx,*camy);return 1;
}
static int field_event_quick_dialogue(uint8_t dialogue,unsigned* camx,unsigned* camy){
    field_dialogue_show(dialogue,*camx,*camy);
    /* $5ACDC's source-only fast return clears these dialogue engine bytes. */
    ram8w(0xED02u,0u);ram8w(0xEC9Au,0u);ram8w(0xECA4u,0u);return 1;
}

static int field_event_palette_alarm_dialogue(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_PALETTE_ALARM_DIALOGUE_DESCRIPTOR_COUNT;i++){
        const struct Ps4PaletteAlarmDialogueDescriptor* e=&ps4_palette_alarm_dialogue_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_cutscene_sound(e->sound);
        ram8w(0xED52u,e->delay_down);field_palette_alarm_down(e->delay_down);
        ram8w(0xED52u,e->delay_restore);field_palette_alarm_restore(e->delay_restore);
        field_dialogue_show(e->dialogue,*camx,*camy);
        ram16w(0xEC20u,0u);
        /* Source $51ADC reloads field graphics after dialogue.  Tiles remain
         * resident in the native renderer, so redraw/rebind is the equivalent
         * visible operation rather than redundant Genesis decompression. */
        field_event_redraw(camx,camy);
        (void)ps4_flag_test_and_set(2u,e->temp_flag);
        return 1;
    }
    return 0;
}
static int field_event_palette_black_battle(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_PALETTE_BLACK_BATTLE_DESCRIPTOR_COUNT;i++){
        const struct Ps4PaletteBlackBattleDescriptor* e=&ps4_palette_black_battle_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_event_set_source_object_facing(e->object_slot,(uint8_t)((chaz.facing^1u)<<2),camx,camy);
        ram8w(0xED52u,e->delay);field_palette_first32_black(e->delay);
        field_event_quick_dialogue(e->dialogue,camx,camy);
        ps4_event_flag_set(e->flag);
        return field_event_run_battle_request(e->battle_index,camx,camy);
    }
    return 0;
}

/* Source EventPtrs can author all five Character_1..5 field records before an
 * event battle.  Keep both the virtual Genesis RAM image and the native field
 * actors synchronized so later source-derived predicates and the GBA renderer
 * observe the same formation.  Source facing_dir is stored in multiples of 4;
 * the native sprite path uses the corresponding 0..3 index. */
static void field_event_apply_party_formation(const struct Ps4PartyFormationBattleDescriptor* e){
    const uint16_t xs[5]={e->x0,e->x1,e->x2,e->x3,e->x4};
    const uint16_t ys[5]={e->y0,e->y1,e->y2,e->y3,e->y4};
    for(unsigned slot=0u;slot<5u;slot++){
        uint16_t b=(uint16_t)(0xC000u+slot*0x40u);
        ram16w((uint16_t)(b+0x30u),xs[slot]);ram16w((uint16_t)(b+0x34u),ys[slot]);
        ram16w((uint16_t)(b+0x38u),xs[slot]);ram16w((uint16_t)(b+0x3Au),ys[slot]);
        ram16w((uint16_t)(b+0x06u),e->source_facing);
    }
    field_followers_sync(0);
    chaz.x=(int)xs[0];chaz.y=(int)ys[0];chaz.facing=(uint8_t)((e->source_facing>>2)&3u);
    chaz.anim=0u;chaz.anim_clock=0u;chaz.dx=0;chaz.dy=0;chaz.step_left=0u;
    for(unsigned i=0u;i<field_follower_count&&i<4u;i++){
        struct PartyFollowerLite* f=&field_followers[i];unsigned slot=i+1u;
        f->x=f->tx=(int)xs[slot];f->y=f->ty=(int)ys[slot];
        f->facing=(uint8_t)((e->source_facing>>2)&3u);f->anim=0u;f->anim_clock=0u;f->moving=0u;
    }
}

static int field_event_party_formation_battle(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_PARTY_FORMATION_BATTLE_DESCRIPTOR_COUNT;i++){
        const struct Ps4PartyFormationBattleDescriptor* e=&ps4_party_formation_battle_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_event_quick_dialogue(e->dialogue,camx,camy);
        ps4_event_flag_set(e->flag);
        (void)ps4_inventory_remove_first(e->item);
        field_event_apply_party_formation(e);
        field_event_set_mapload_bit(e->mapload_bit);
        return field_event_run_battle_request(e->battle_index,camx,camy);
    }
    return 0;
}

static int field_event_inventory_reward(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_INVENTORY_REWARD_DESCRIPTOR_COUNT;i++){
        const struct Ps4InventoryRewardDescriptor* e=&ps4_inventory_reward_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        if(ps4_inventory_count()>=40u){field_dialogue_show(e->full_dialogue,*camx,*camy);return 1;}
        field_dialogue_show(e->success_dialogue,*camx,*camy);
        if(ps4_inventory_add(e->item)<0)return 1;
        ps4_event_flag_set(e->flag);return 1;
    }
    return 0;
}

/* $4B/$82 share the same source transaction protocol.  The $F5 dialogue
 * command writes the response byte at $ED53 and the source later validates the
 * post-$FF cursor marker before charging Meseta, so neither insufficient funds
 * nor a NO response can accidentally commit the purchase. */
static int field_event_money_dialogue_transaction(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_MONEY_DIALOGUE_TRANSACTION_DESCRIPTOR_COUNT;i++){
        const struct Ps4MoneyDialogueTransactionDescriptor* e=&ps4_money_dialogue_transaction_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        uint32_t money=ram32r(0xF438u);
        if(money<e->cost)ps4_event_flag_set(e->insufficient_flag);else ps4_flag_clear_type(0u,e->insufficient_flag);
        field_event_face_source_object(e->object_slot,camx,camy);
        ram8w(0xED53u,1u); /* safe default if malformed text omits the $F5 command */
        field_dialogue_show(e->dialogue,*camx,*camy);
        if(ram8r(0xED53u)!=0u)return 1;
        const uint8_t* begin=field_dialogue;const uint8_t* end=field_dialogue+field_dialogue_len;
        if(!field_dialogue_saved_cursor||field_dialogue_saved_cursor<begin+2u||field_dialogue_saved_cursor>end)return 1;
        if(field_dialogue_saved_cursor[-2]!=0x3Au)return 1;
        ram32w(0xF438u,money-e->cost);
        if(e->patch_dialogue!=0xFFu)field_event_set_source_object_dialogue(e->object_slot,e->patch_dialogue);
        ps4_event_flag_set(e->success_flag);return 1;
    }
    return 0;
}

/* Exact source EventPtrs templates that request RunEventBattle through
 * Event_Battle_Index ($ECFC) + Routine_Exit_Flags bit 3.  This restores the
 * shared EventPtrs -> event-battle handoff instead of teaching individual maps
 * which boss/formation to launch. */
static int field_event_coord_object_conditional_dialogue(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_COORD_OBJECT_CONDITIONAL_DIALOGUE_DESCRIPTOR_COUNT;i++){
        const struct Ps4CoordObjectConditionalDialogueDescriptor* e=&ps4_coord_object_conditional_dialogue_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        const uint16_t xs[4]={e->x0,e->x1,e->x2,e->x3};
        const uint16_t ys[4]={e->y0,e->y1,e->y2,e->y3};
        const uint8_t slots[4]={e->slot0,e->slot1,e->slot2,e->slot3};
        for(unsigned q=0u;q<4u;q++){
            if((unsigned)chaz.x!=(unsigned)xs[q]||(unsigned)chaz.y!=(unsigned)ys[q])continue;
            field_event_face_source_object(slots[q],camx,camy);
            field_dialogue_show(ps4_event_flag_test(e->flag)?e->set_dialogue:e->clear_dialogue,*camx,*camy);
            break;
        }
        return 1;
    }
    return 0;
}

static int field_event_paired_object_dialogue_state(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_PAIRED_OBJECT_DIALOGUE_STATE_DESCRIPTOR_COUNT;i++){
        const struct Ps4PairedObjectDialogueStateDescriptor* e=&ps4_paired_object_dialogue_state_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_event_face_source_object(e->slot0,camx,camy);
        field_event_face_source_object(e->slot1,camx,camy);
        if(!ps4_event_flag_test(e->primary_flag)&&!ps4_event_flag_test(e->secondary_flag)){
            field_dialogue_show(e->base_dialogue,*camx,*camy);return 1;
        }
        uint8_t dialogue=ps4_event_flag_test(e->primary_flag)?e->primary_dialogue:e->secondary_dialogue;
        field_dialogue_show(dialogue,*camx,*camy);
        field_event_set_source_object_dialogue(e->slot0,dialogue);
        field_event_set_source_object_dialogue(e->slot1,dialogue);
        (void)ps4_flag_test_and_set(2u,e->temp_flag);
        return 1;
    }
    return 0;
}

static int field_event_rng_dialogue(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_RNG_DIALOGUE_DESCRIPTOR_COUNT;i++){
        const struct Ps4RngDialogueDescriptor* e=&ps4_rng_dialogue_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_event_face_source_object(e->object_slot,camx,camy);
        uint8_t dialogue=e->flagged_dialogue;
        if(!ps4_event_flag_test(e->flag)){
            unsigned q=(unsigned)(field_npc_rng_next()&3u);
            dialogue=q==0u?e->zero_dialogue:(q==1u?e->one_dialogue:e->other_dialogue);
        }
        field_dialogue_show(dialogue,*camx,*camy);return 1;
    }
    return 0;
}

static int field_event_dialogue_face_resume(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_DIALOGUE_FACE_RESUME_DESCRIPTOR_COUNT;i++){
        const struct Ps4DialogueFaceResumeDescriptor* e=&ps4_dialogue_face_resume_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_dialogue_show(e->dialogue,*camx,*camy);
        /* Source toggles bit 2 of Character_1 facing before Event_UpdateObjFacing:
         * in the native 0..3 facing enum this is exactly opposite direction. */
        field_event_set_source_object_facing(e->object_slot,(uint8_t)((chaz.facing^1u)<<2),camx,camy);
        (void)field_dialogue_resume_saved(*camx,*camy);
        ram16w(0xEC20u,0u);
        /* Source $51ADC reloads Genesis field graphics from the map descriptor
         * because dialogue presentation reused that graphics workspace.  GBA field
         * tiles stay resident, so redraw/rebind the live field without decompression. */
        field_event_redraw(camx,camy);
        return 1;
    }
    return 0;
}

static int field_event_choreographed_followup_dialogue(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_CHOREOGRAPHED_FOLLOWUP_DIALOGUE_DESCRIPTOR_COUNT;i++){
        const struct Ps4ChoreographedFollowupDialogueDescriptor* e=&ps4_choreographed_followup_dialogue_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_cutscene_sound(0xE1u);
        field_event_set_source_object_facing(e->object_slot,e->source_facing,camx,camy);
        field_event_wait_updates(0x27u,camx,camy);
        field_dialogue_show(e->dialogue,*camx,*camy);
        field_event_wait_updates(0x27u,camx,camy);
        field_cutscene_sound(0xCFu);field_event_wait_updates(0x27u,camx,camy);
        field_cutscene_sound(0xCFu);field_event_wait_updates(0x13u,camx,camy);
        field_cutscene_sound(0xCFu);field_event_wait_updates(0x3Bu,camx,camy);
        (void)field_dialogue_resume_saved(*camx,*camy);
        ram8w(e->state_addr,e->state_value);
        ps4_event_flag_set(e->flag);return 1;
    }
    return 0;
}

static int field_event_dialogue_successor_flag(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_DIALOGUE_SUCCESSOR_FLAG_DESCRIPTOR_COUNT;i++){
        const struct Ps4DialogueSuccessorFlagDescriptor* e=&ps4_dialogue_successor_flag_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        unsigned dialogue=ps4_event_flag_test(e->flag)?e->repeat_dialogue:e->first_dialogue;
        field_dialogue_show(dialogue,*camx,*camy);
        const uint8_t* end=field_dialogue+field_dialogue_len;
        if(field_dialogue_saved_cursor&&field_dialogue_saved_cursor<end&&*field_dialogue_saved_cursor==e->successor_marker)
            ps4_event_flag_set(e->flag);
        return 1;
    }
    return 0;
}

static int field_event_simple_battle(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_SIMPLE_BATTLE_EVENT_DESCRIPTOR_COUNT;i++){
        const struct Ps4SimpleBattleEventDescriptor* e=&ps4_simple_battle_event_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        if(e->dialogue!=0xFFu)field_dialogue_show(e->dialogue,*camx,*camy);
        ps4_event_flag_set(e->flag);
        if(e->set_mapload_bit)field_event_set_mapload_bit(3u);
        return field_event_run_battle_request(e->battle_index,camx,camy);
    }
    return 0;
}

static int field_event_quick_dialogue_battle(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_QUICK_DIALOGUE_BATTLE_EVENT_DESCRIPTOR_COUNT;i++){
        const struct Ps4QuickDialogueBattleEventDescriptor* e=&ps4_quick_dialogue_battle_event_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_event_quick_dialogue(e->dialogue,camx,camy);ps4_event_flag_set(e->flag);
        if(e->saved_sound!=0xFFu)ram8w(0xECECu,e->saved_sound);
        if(e->mapload_bit!=0xFFu)field_event_set_mapload_bit(e->mapload_bit);
        return field_event_run_battle_request(e->battle_index,camx,camy);
    }
    return 0;
}

static int field_event_guarded_quick_battle(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_GUARDED_QUICK_BATTLE_DESCRIPTOR_COUNT;i++){
        const struct Ps4GuardedQuickBattleDescriptor* e=&ps4_guarded_quick_battle_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        if(!ps4_event_flag_test(e->guard_flag)){field_dialogue_show(e->fallback_dialogue,*camx,*camy);return 1;}
        field_event_quick_dialogue(e->quick_dialogue,camx,camy);ps4_event_flag_set(e->flag);
        if(e->mapload_bit!=0xFFu)field_event_set_mapload_bit(e->mapload_bit);
        return field_event_run_battle_request(e->battle_index,camx,camy);
    }
    return 0;
}

/* Variants of the same source RunEventBattle handoff with one extra audited
 * primitive: an already-complete EventFlag guard and/or a direct write to
 * Saved_Sound_Index ($ECEC). */
static int field_event_extended_battle(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_EXTENDED_BATTLE_EVENT_DESCRIPTOR_COUNT;i++){
        const struct Ps4ExtendedBattleEventDescriptor* e=&ps4_extended_battle_event_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        if(e->guard_flag!=0xFFu&&ps4_event_flag_test(e->guard_flag))return 1;
        if(e->dialogue!=0xFFu)field_dialogue_show(e->dialogue,*camx,*camy);
        if(e->battle_index!=0u)ps4_event_flag_set(e->flag);
        if(e->saved_sound!=0xFFu)ram8w(0xECECu,e->saved_sound);
        if(e->set_mapload_bit)field_event_set_mapload_bit(3u);
        return field_event_run_battle_request(e->battle_index,camx,camy);
    }
    return 0;
}

static void field_recover_party(void){
    /* Event_Recovery ($6D39A) ultimately calls RecoverStats.  Recover every
     * occupied Current_Party_Slots entry from the same Character_Stats fields
     * used by battle; clear field/battle ailments and restore HP/TP. */
    for(unsigned slot=0u;slot<5u;slot++){
        uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id&0x80u)continue;
        uint16_t d=(uint16_t)(0xF500u+(uint16_t)id*0x80u);
        ram16w((uint16_t)(d+0x0Eu),ram16r((uint16_t)(d+0x10u)));
        ram16w((uint16_t)(d+0x12u),ram16r((uint16_t)(d+0x14u)));
        ram8w((uint16_t)(d+0x16u),0u);
        ram8w((uint16_t)(d+0x1Au),ram8r((uint16_t)(d+0x19u)));
        ram8w((uint16_t)(d+0x1Du),ram8r((uint16_t)(d+0x1Cu)));
        ram8w((uint16_t)(d+0x20u),ram8r((uint16_t)(d+0x1Fu)));
        ram8w((uint16_t)(d+0x23u),ram8r((uint16_t)(d+0x22u)));
        ram16w((uint16_t)(d+0x26u),ram16r((uint16_t)(d+0x24u)));
        ram16w((uint16_t)(d+0x2Au),ram16r((uint16_t)(d+0x28u)));
        ram16w((uint16_t)(d+0x2Eu),ram16r((uint16_t)(d+0x2Cu)));
        for(unsigned e=0u;e<14u;e++)ram8w((uint16_t)(d+0x30u+e*2u),ram8r((uint16_t)(d+0x31u+e*2u)));
    }
}
static int field_event_recovery(unsigned* camx,unsigned* camy){
    /* Event_Recovery is also called by other EventPtrs, so keep its complete
     * gameplay-visible behavior behind one shared executor. */
    for(unsigned pulse=0u;pulse<3u;pulse++){for(unsigned f=0u;f<16u;f++)waitvb();for(unsigned f=0u;f<16u;f++)waitvb();}
    field_recover_party();hw_field_init(*camx,*camy);return 1;
}
static int field_event_dialogue_recovery(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_DIALOGUE_RECOVERY_DESCRIPTOR_COUNT;i++){
        const struct Ps4DialogueRecoveryDescriptor* e=&ps4_dialogue_recovery_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_dialogue_show(e->dialogue,*camx,*camy);return field_event_recovery(camx,camy);
    }
    return 0;
}

static int field_event_dialogue_camera_flag(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_DIALOGUE_CAMERA_FLAG_DESCRIPTOR_COUNT;i++){
        const struct Ps4DialogueCameraFlagDescriptor* e=&ps4_dialogue_camera_flag_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        field_dialogue_show(e->dialogue,*camx,*camy);
        /* Source calls Event_MoveCamera($5AAEE) with Character_1's current
         * coordinates.  The GBA field renderer keeps that same camera target
         * through choose_camera; redraw through the shared event path so this
         * is a global camera-restore primitive, not an Event $50 coordinate fix. */
        field_event_redraw(camx,camy);
        ps4_event_flag_set(e->flag);return 1;
    }
    return 0;
}

static int field_event_conditional_dialogue_flag(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_CONDITIONAL_DIALOGUE_FLAG_DESCRIPTOR_COUNT;i++){
        const struct Ps4ConditionalDialogueFlagDescriptor* e=&ps4_conditional_dialogue_flag_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        return field_event_dialogue_once(e->flag,e->first_dialogue,e->repeat_dialogue,camx,camy);
    }
    return 0;
}

static int field_event_tree_dialogue_flag(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_TREE_DIALOGUE_FLAG_DESCRIPTOR_COUNT;i++){
        const struct Ps4TreeDialogueFlagDescriptor* e=&ps4_tree_dialogue_flag_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        unsigned n=kos_decode(e->dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(!n)return 0;
        field_dialogue_len=n;field_dialogue_show(e->dialogue,*camx,*camy);ps4_event_flag_set(e->flag);
        n=kos_decode(field_map.dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;
        return 1;
    }
    return 0;
}

static int field_event_object_dialogue_patch(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_OBJECT_DIALOGUE_PATCH_DESCRIPTOR_COUNT;i++){
        const struct Ps4ObjectDialoguePatchDescriptor* e=&ps4_object_dialogue_patch_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        unsigned n;
        if(e->pre_tree!=0xFFFFFFFFu){n=kos_decode(e->pre_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(!n)return 0;field_dialogue_len=n;}
        field_dialogue_show(e->dialogue,*camx,*camy);
        for(unsigned q=0u;q<(unsigned)e->count;q++)
            field_event_set_source_object_dialogue((unsigned)e->start_slot+q,e->object_dialogue);
        if(e->post_tree!=0xFFFFFFFFu){n=kos_decode(e->post_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;}
        ps4_event_flag_set(e->flag);return 1;
    }
    return 0;
}

static int field_event_consume_item_dialogue(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_CONSUME_ITEM_DIALOGUE_DESCRIPTOR_COUNT;i++){
        const struct Ps4ConsumeItemDialogueDescriptor* e=&ps4_consume_item_dialogue_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        unsigned slot=e->object_slot;
        if(slot<field_obj_count&&field_objs[slot].active){
            /* Source derives the opposite of Character_1 facing before calling
             * Event_UpdateObjFacing on the authored object. */
            field_objs[slot].facing=(uint8_t)(chaz.facing^1u);
            hw_refresh_field_object_art(slot);hw_update_objects(*camx,*camy);
        }
        if(!ps4_inventory_remove_first(e->item)){field_dialogue_show(e->fallback_dialogue,*camx,*camy);return 1;}
        field_dialogue_show(e->success_dialogue,*camx,*camy);ps4_event_flag_set(e->flag);return 1;
    }
    return 0;
}

static int field_event_layout_animation(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_LAYOUT_ANIMATION_DESCRIPTOR_COUNT;i++){
        const struct Ps4LayoutAnimationDescriptor* e=&ps4_layout_animation_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        /* Event source writes Sound_Index directly.  Keep the virtual sound
         * request exact; the current music renderer must not loop one-shot SFX. */
        ram8w(0x500Au,e->sound);
        uint32_t q=e->frames_addr;
        for(unsigned frame=0u;frame<(unsigned)e->frame_count;frame++){
            uint8_t duration=rom8(q++);if(duration==0xFFu)return 0;
            uint8_t tl=rom8(q++),tr=rom8(q++),bl=rom8(q++),br=rom8(q++);
            unsigned wx=(unsigned)e->x_chunk<<5,wy=(unsigned)e->y_chunk<<5,plane=e->plane;
            if(!field_layout_write_world(wx,wy,plane,tl)||
               !field_layout_write_world(wx+32u,wy,plane,tr)||
               !field_layout_write_world(wx,wy+32u,plane,bl)||
               !field_layout_write_world(wx+32u,wy+32u,plane,br))return 0;
            field_event_redraw(camx,camy);
            /* Source uses DBF on the table duration, so value N means N+1
             * VInt-prepared frames before the next authored layout. */
            for(unsigned n=0u;n<=(unsigned)duration;n++)waitvb();
        }
        if(rom8(q)!=0xFFu)return 0;
        ps4_event_flag_set(e->flag);return 1;
    }
    return 0;
}

static int field_event_fixed_two_row_layout(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_FIXED_TWO_ROW_LAYOUT_DESCRIPTOR_COUNT;i++){
        const struct Ps4FixedTwoRowLayoutDescriptor* e=&ps4_fixed_two_row_layout_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        ram8w(0x500Au,e->sound);
        const unsigned wx=(unsigned)e->x_chunk<<5,wy=(unsigned)e->y_chunk<<5,plane=e->plane;
        uint32_t q=e->frames_addr;
        for(unsigned frame=0u;frame<(unsigned)e->frame_count;frame++){
            const uint8_t duration=rom8(q++),ul=rom8(q++),ur=rom8(q++),ll=rom8(q++),lr=rom8(q++);
            if(!field_layout_write_world(wx,wy,plane,ul)||
               !field_layout_write_world(wx+32u,wy,plane,ur)||
               !field_layout_write_world(wx,wy+32u,plane,ll)||
               !field_layout_write_world(wx+32u,wy+32u,plane,lr))return 1;
            /* $416A6 is the lighter plane/VInt wrapper: no Field_UpdateObjects
             * call is part of this source loop. Redraw the authored layout on
             * every DBF iteration, then wait one VBlank. */
            for(unsigned n=0u;n<=(unsigned)duration;n++){field_event_redraw(camx,camy);waitvb();}
        }
        if(rom8(q)!=0xFFu)return 1;
        return 1;
    }
    return 0;
}

static int field_event_dynamic_two_cell_layout(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_DYNAMIC_TWO_CELL_LAYOUT_DESCRIPTOR_COUNT;i++){
        const struct Ps4DynamicTwoCellLayoutDescriptor* e=&ps4_dynamic_two_cell_layout_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        const unsigned plane=field_map.collision_plane?1u:0u;
        const unsigned wx=(unsigned)chaz.x,wy=(unsigned)chaz.y;
        if(chaz.y<32)return 1;
        /* Event_GaruberkTwDoorOpening1/2 resolve Character_1's current layout
         * byte and the byte exactly one 32-pixel chunk above it, both through
         * Map_Collision_Plane ($EC24). A nonmatching lower tile is a handled
         * source RTS, not an unrecognized event. */
        if(field_layout_chunk_world(wx,wy,plane)!=e->expected_lower)return 1;
        ram8w(0x500Au,e->sound); /* exact Sound_Index request; SFX mixer audit is separate */
        uint32_t q=e->frames_addr;
        for(unsigned frame=0u;frame<(unsigned)e->frame_count;frame++){
            const uint8_t duration=rom8(q++),lower=rom8(q++),upper=rom8(q++);
            if(!field_layout_write_world(wx,wy,plane,lower))return 1;
            if(!field_layout_write_world(wx,(unsigned)(chaz.y-32),plane,upper))return 1;
            /* Source DBF repeats plane redraw + Field_UpdateObjects + VInt for
             * duration+1 iterations. Reuse the shared counted choreography
             * primitive instead of reducing this to a blind sleep. */
            field_event_wait_updates(duration,camx,camy);
        }
        if(rom8(q)!=0xFFu)return 1;
        return 1;
    }
    return 0;
}

static int field_event_garuberk_door_entered(unsigned event_index,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_GARUBERK_DOOR_ENTERED_DESCRIPTOR_COUNT;i++){
        const struct Ps4GaruberkDoorEnteredDescriptor* e=&ps4_garuberk_door_entered_descriptors[i];
        if((unsigned)e->event_index!=event_index)continue;
        const unsigned plane=field_map.collision_plane?1u:0u;
        if(chaz.y<16)return 1;

        /* Event_GaruberkTwDoorEntered1/2 begin with Pal_FadeOut followed by a
         * same-map field/palette refresh.  The GBA keeps decoded field tiles
         * resident, so rebuild the native display under black rather than
         * re-decoding the map or inventing a visible blank frame. */
        field_palette_fade_out();
        field_reload_palette_from_map();
        choose_camera(camx,camy);hw_field_init(*camx,*camy);

        /* Before Character_1 moves, source resolves one layout cell from
         * y-$10 and one from y.  Keep that half-cell asymmetry exactly: these
         * doorway events are authored on 16-pixel character coordinates even
         * though GetMapLayoutChunk indexes 32-pixel layout chunks. */
        const unsigned open_x=(unsigned)chaz.x,open_y=(unsigned)chaz.y;
        if(!field_layout_write_world(open_x,open_y,plane,0x3Cu))return 1;
        if(!field_layout_write_world(open_x,open_y-16u,plane,e->open_upper))return 1;
        field_event_choreography_tick(*camx,*camy);
        field_palette_fade_in();
        ram8w(0x500Au,0xD7u);

        /* Source sets Character_1 dest_y=current_y+$10, then runs its normal
         * field/scroll maintenance until movement completes and synchronizes
         * the party records.  Reuse the native party translation primitive;
         * it advances the whole visible party with source-sized steps instead
         * of replacing the sequence with a teleport or delay. */
        field_event_translate_party(0,16,camx,camy);
        {
            uint16_t b=0xC000u;ram16w((uint16_t)(b+0x30u),(uint16_t)chaz.x);ram16w((uint16_t)(b+0x34u),(uint16_t)chaz.y);
            ram16w((uint16_t)(b+0x38u),(uint16_t)chaz.x);ram16w((uint16_t)(b+0x3Au),(uint16_t)chaz.y);
            for(unsigned n=0u;n<field_follower_count&&n<4u;n++)if(field_followers[n].active){
                struct PartyFollowerLite* f=&field_followers[n];b=(uint16_t)(0xC000u+(n+1u)*0x40u);
                ram16w((uint16_t)(b+0x30u),(uint16_t)f->x);ram16w((uint16_t)(b+0x34u),(uint16_t)f->y);
                ram16w((uint16_t)(b+0x38u),(uint16_t)f->tx);ram16w((uint16_t)(b+0x3Au),(uint16_t)f->ty);
            }
        }

        /* After the +$10 move, source recomputes the two door pointers using
         * current Y and Y-$20, then streams [duration,lower,upper] close
         * records with Field_UpdateObjects + VInt on every DBF iteration. */
        if(chaz.y<32)return 1;
        ram8w(0x500Au,0xD8u);
        uint32_t q=e->close_frames_addr;
        for(unsigned frame=0u;frame<(unsigned)e->frame_count;frame++){
            const uint8_t duration=rom8(q++),lower=rom8(q++),upper=rom8(q++);
            if(!field_layout_write_world((unsigned)chaz.x,(unsigned)chaz.y,plane,lower))return 1;
            if(!field_layout_write_world((unsigned)chaz.x,(unsigned)(chaz.y-32),plane,upper))return 1;
            field_event_wait_updates(duration,camx,camy);
        }
        if(rom8(q)!=0xFFu)return 1;
        return 1;
    }
    return 0;
}

static void field_event_clear_source_object(unsigned slot){
    if(slot<field_obj_count){field_objs[slot].active=0u;field_objs[slot].type=0u;}
    if(slot<32u)ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u),0u);
}
static void field_event_set_source_object_type(unsigned slot,uint16_t type){
    if(slot<field_obj_count){field_objs[slot].type=type;field_objs[slot].active=(uint8_t)(type!=0u);field_obj_init_motion(&field_objs[slot]);}
    if(slot<32u)ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u),type);
}
static void field_event_move_source_object(unsigned slot,uint16_t tx,uint16_t ty,unsigned* camx,unsigned* camy){
    if(slot>=field_obj_count||!field_objs[slot].active)return;struct FieldObjLite* o=&field_objs[slot];
    while(o->x!=tx||o->y!=ty){int dx=0,dy=0;if(o->x<tx){dx=2;o->facing=8u;}else if(o->x>tx){dx=-2;o->facing=12u;}else if(o->y<ty){dy=2;o->facing=0u;}else if(o->y>ty){dy=-2;o->facing=4u;}
        if(dx&&((dx>0&&(unsigned)o->x+2u>tx)||(dx<0&&o->x<tx+2u)))dx=(int)tx-(int)o->x;if(dy&&((dy>0&&(unsigned)o->y+2u>ty)||(dy<0&&o->y<ty+2u)))dy=(int)ty-(int)o->y;
        o->x=(uint16_t)((int)o->x+dx);o->y=(uint16_t)((int)o->y+dy);o->anim=(uint8_t)((o->anim+1u)&3u);ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u+0x30u),o->x);ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u+0x34u),o->y);hw_refresh_field_object_art(slot);field_event_choreography_tick(*camx,*camy);}
    o->anim=0u;hw_refresh_field_object_art(slot);field_event_redraw(camx,camy);
}
static void field_event_force_disembark(unsigned* camx,unsigned* camy){if(!ram16r(0xF43Cu))return;ram16w(0xF43Cu,0u);ram16w(0xC000u,(uint16_t)((unsigned)lead_char_id*4u+4u));field_reload_palette_from_map();hw_load_palettes();field_followers_sync(1);hw_chaz_key=0xFFFFu;field_event_redraw(camx,camy);}
static int field_event_bioplant_door(unsigned* camx,unsigned* camy){
    unsigned upper_i,lower_i;if(!field_layout_index_world((unsigned)chaz.x,(unsigned)chaz.y,1u,&upper_i)||!field_layout_index_world((unsigned)chaz.x,(unsigned)chaz.y+32u,1u,&lower_i))return 1;
    uint16_t off=field_layout_off_for_plane(1u);uint32_t q=0x06B9A4u;ram8w(0x500Au,0xE2u);
    for(;;){uint8_t duration=rom8(q++);if(duration==0xFFu)break;uint8_t upper=rom8(q++),lower=rom8(q++);work_ram[(uint16_t)(off+upper_i)]=upper;work_ram[(uint16_t)(off+lower_i)]=lower;hw_ring_layout_dirty=1u;field_event_redraw(camx,camy);for(unsigned f=0u;f<=(unsigned)duration;f++)waitvb();}
    (void)ps4_flag_test_and_set(2u,0x08u);return 1;
}
static int field_event_board_vehicle(unsigned event_index,unsigned* camx,unsigned* camy){
    unsigned vehicle=(event_index-0x09u)+1u;uint16_t source_type=vehicle==1u?0x00B0u:(vehicle==2u?0x00B4u:0x00B8u);
    for(unsigned i=0u;i<field_follower_count;i++)field_followers[i].active=0u;field_follower_count=0u;
    chaz.x&=~0x10;chaz.y=(chaz.y+0x10)&~0x10;ram16w(0xC030u,(uint16_t)chaz.x);ram16w(0xC034u,(uint16_t)chaz.y);ram16w(0xC000u,source_type);for(unsigned i=1u;i<5u;i++)ram16w((uint16_t)(0xC000u+i*0x40u),0u);
    if(ram8r(0xECECu)!=0x8Du){ram8w(0x500Au,0x8Du);ram8w(0xECECu,0x8Du);ps4_audio_stop();ps4_audio_play(0x8Du);}ram16w(0xF43Cu,(uint16_t)vehicle);r30_vehicle_palette(vehicle);hw_chaz_key=0xFFFFu;choose_camera(camx,camy);field_event_redraw(camx,camy);return 1;
}
static int field_event_get_off_vehicle(unsigned* camx,unsigned* camy){
    unsigned vehicle=ram16r(0xF43Cu);if(!vehicle)return 1;uint8_t coll=field_collision_at(chaz.x,chaz.y,0,0);int blocked=coll!=0u;
    if(vehicle==2u&&coll==0u)blocked=0;if(vehicle==3u&&coll==0u)blocked=0;
    if(blocked){ps4_ui_message("Cannot get off!",0);hw_field_init(*camx,*camy);return 1;}
    ram16w(0xF43Cu,0u);ram16w(0xC000u,(uint16_t)((unsigned)lead_char_id*4u+4u));field_reload_palette_from_map();hw_load_palettes();field_followers_sync(1);hw_chaz_key=0xFFFFu;if(field_map.music>=0x81u&&field_map.music<=0xB4u){ram8w(0xECECu,field_map.music);ram8w(0x500Au,field_map.music);ps4_audio_stop();ps4_audio_play(field_map.music);}field_event_redraw(camx,camy);return 1;
}
static int field_event_aiedo_crush(unsigned* camx,unsigned* camy){
    const unsigned slot=5u;field_event_face_source_object(slot,camx,camy);field_dialogue_show(0x3Cu,*camx,*camy);if(slot<field_obj_count&&field_objs[slot].active){struct FieldObjLite* o=&field_objs[slot];field_event_set_source_object_type(slot,0x00BCu);for(unsigned guard=0u;guard<160u&&o->active;guard++){o->y=(uint16_t)(o->y-2u);o->facing=4u;o->anim=(uint8_t)((o->anim+1u)&3u);if((int)o->y-(int)*camy<-32)field_event_clear_source_object(slot);else{ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u+0x34u),o->y);hw_refresh_field_object_art(slot);field_event_choreography_tick(*camx,*camy);}}}if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);field_event_redraw(camx,camy);return 1;
}
static int field_event_fainting_priest(unsigned* camx,unsigned* camy){
    const unsigned priest=9u,block=10u;field_event_face_source_object(priest,camx,camy);field_dialogue_show(0x45u,*camx,*camy);if(priest<field_obj_count){field_event_set_source_object_type(priest,0x0180u);field_objs[priest].facing=0u;for(unsigned f=0u;f<46u;f++){unsigned idx=f/8u;if(idx>5u)idx=5u;field_objs[priest].anim=(uint8_t)idx;hw_refresh_field_object_art(priest);field_event_choreography_tick(*camx,*camy);}}
    if(block<field_obj_count){field_objs[block].type=0x0074u;field_objs[block].active=1u;field_objs[block].x=0x0200u;field_objs[block].y=0x0140u;field_objs[block].dialog=0x38u;field_obj_configure_render(&field_objs[block],field_objs[block].gba_tile);}else if(field_obj_count<FIELD_MAX_OBJECTS){struct FieldObjLite* o=&field_objs[field_obj_count++];o->type=0x0074u;o->active=1u;o->x=0x0200u;o->y=0x0140u;o->facing=0u;o->dialog=0x38u;o->base_tile=0u;o->gba_tile=0u;o->render_w=o->render_h=0u;o->render_x=o->render_y=0;field_obj_init_motion(o);}if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);field_event_set_source_object_dialogue(priest,0x38u);field_event_redraw(camx,camy);return 1;
}
static int field_event_rune_ladae(unsigned* camx,unsigned* camy){
    ram8w(0xF40Eu,3u);uint16_t d=(uint16_t)(0xF500u+3u*0x80u);ram16w((uint16_t)(d+0x0Eu),ram16r((uint16_t)(d+0x10u)));ram16w((uint16_t)(d+0x12u),ram16r((uint16_t)(d+0x14u)));ram8w((uint16_t)(d+0x4Cu),0x37u);ram8w((uint16_t)(d+0x4Du),0u);ram8w((uint16_t)(d+0x4Eu),0x36u);ram8w((uint16_t)(d+0x4Fu),0x38u);update_one_char_derived(3u);uint16_t sx=(uint16_t)chaz.x,sy=(uint16_t)chaz.y;if(field_obj_count&&field_objs[0].active){sx=field_objs[0].x;sy=field_objs[0].y;field_event_clear_source_object(0u);}field_followers_sync(1);struct PartyFollowerLite* rf=0;for(unsigned i=0;i<field_follower_count;i++)if(field_followers[i].id==3u){rf=&field_followers[i];break;}if(rf){rf->x=sx;rf->y=sy;rf->tx=sx;rf->ty=sy;rf->facing=3u;}field_dialogue_show(0x06u,*camx,*camy);if(rf&&field_follower_count>=2u){struct PartyFollowerLite* prev=&field_followers[field_follower_count-2u];rf->x=prev->x-16;rf->y=prev->y;rf->tx=rf->x;rf->ty=rf->y;}ram8w(0xECE0u,1u);ps4_event_flag_set(0x62u);field_event_redraw(camx,camy);return 1;
}
static int field_event_zio_fanatic(unsigned* camx,unsigned* camy){
    const unsigned original=4u,replacement=9u;field_event_set_source_object_facing(original,(uint8_t)((chaz.facing^1u)<<2),camx,camy);field_dialogue_show(0x49u,*camx,*camy);if(original>=field_obj_count||!field_objs[original].active)return 1;const uint16_t sx=field_objs[original].x,sy=field_objs[original].y,reuse_tile=field_objs[original].gba_tile;
    while(field_obj_count<=replacement&&field_obj_count<FIELD_MAX_OBJECTS){struct FieldObjLite* z=&field_objs[field_obj_count++];z->type=0u;z->active=0u;}if(replacement>=FIELD_MAX_OBJECTS)return 1;struct FieldObjLite* o=&field_objs[replacement];o->type=0x0148u;o->active=1u;o->facing=0u;o->dialog=0u;o->base_tile=0x0200u;o->x=(uint16_t)(sx&~7u);o->y=(uint16_t)(sy&~7u);o->anim=0u;o->move_kind=o->move_dir=o->move_left=o->move_phase=o->seq_pos=0u;o->timer=3u;o->bound_x=o->bound_y=o->max_x=o->max_y=0;o->gba_tile=reuse_tile;o->render_w=16u;o->render_h=32u;o->render_x=0;o->render_y=-2;field_obj_art_stamp[original].epoch=0u;field_obj_art_stamp[replacement].epoch=0u;ram16w((uint16_t)(0xC300u+replacement*0x40u),0x0148u);ram16w((uint16_t)(0xC300u+replacement*0x40u+0x06u),0u);ram16w((uint16_t)(0xC300u+replacement*0x40u+0x30u),o->x);ram16w((uint16_t)(0xC300u+replacement*0x40u+0x34u),o->y);hw_refresh_field_object_art(replacement);field_event_clear_source_object(original);int duration=3;unsigned frame=0u;for(unsigned f=0u;f<30u;f++){--duration;if(duration<0){duration=7;frame^=1u;o->anim=(uint8_t)frame;hw_refresh_field_object_art(replacement);}hw_update_objects(*camx,*camy);waitvb();}field_dialogue_show(0x1Fu,*camx,*camy);return 1;
}
static int field_event_chaz_house(unsigned* camx,unsigned* camy){field_dialogue_show(0x31u,*camx,*camy);unsigned yes=ram8r(0xED53u)==0u;if(yes){ps4_audio_stop();field_palette_fade_out();field_recover_party();ram16w(0xEC28u,0x005Eu);ram16w(0xEC2Au,0x0054u);ram16w(0xEC48u,0x002Eu);ram16w(0xEC4Au,0x0044u);ram16w(0xEC44u,0u);ram16w(0xEC46u,0u);ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~0x08u));ram8w(0x500Au,0x84u);ram8w(0xECECu,0x84u);field_event_reload_pending=1u;}(void)ps4_flag_test_and_set(2u,0x18u);return 1;}
static void field_event_white_flash_defeat(unsigned dialogue,unsigned flag,unsigned spawned_count,unsigned* camx,unsigned* camy){field_dialogue_show(dialogue,*camx,*camy);field_palette_save(64u);for(unsigned f=0u;f<40u;f++){if((f&3u)==0u)field_palette_increase_tone_once();for(unsigned i=0u;i<32u;i++)cram[i]=rom16(field_map.palette+i*2u);for(unsigned i=0u;i<16u;i++)cram[32u+i]=rom16(0x296300u+i*2u);field_palette_mirror_live(64u);hw_load_palettes();waitvb();}for(unsigned f=0u;f<40u;f++){if((f&3u)==0u)field_palette_increase_tone_once();for(unsigned i=0u;i<16u;i++)cram[32u+i]=rom16(0x296300u+i*2u);field_palette_mirror_live(64u);hw_load_palettes();waitvb();}for(unsigned q=0u;q<5u;q++)field_event_clear_source_object(q);for(unsigned q=0u;q<spawned_count;q++){unsigned slot=7u+q;if(slot<field_obj_count){field_objs[slot].active=1u;field_objs[slot].type=0x00A0u;}ram16w((uint16_t)(0xC300u+slot*0x40u),0x00A0u);}field_event_wait_updates(9u,camx,camy);ram8w(0x500Au,0xE6u);for(unsigned f=0u;f<40u;f++){if((f&3u)==0u)field_palette_restore_step();field_palette_mirror_live(64u);hw_load_palettes();waitvb();}ps4_event_flag_set(flag);field_event_redraw(camx,camy);}
static int field_event_defeated_guardians(unsigned event_index,unsigned* camx,unsigned* camy){if(event_index==0x64u){field_event_white_flash_defeat(4u,0xE5u,3u,camx,camy);return 1;}if(event_index==0x65u){field_event_white_flash_defeat(2u,0xE6u,2u,camx,camy);return 1;}return 0;}
static int field_event_carnivorous_trees(unsigned* camx,unsigned* camy){ram8w(0xECECu,0x99u);if(ram16r(0xF43Cu)){field_event_force_disembark(camx,camy);ps4_audio_stop();ps4_audio_play(0xA9u);unsigned old=field_dialogue_len,n=kos_decode(0x1F9580u,field_dialogue,FIELD_DIALOGUE_CAP);if(n){field_dialogue_len=n;field_dialogue_show(0x34u,*camx,*camy);field_dialogue_len=old;}}field_event_translate_party(0,32,camx,camy);return field_event_run_battle_request(0x0Au,camx,camy);}
static int field_event_saving_kyra(unsigned* camx,unsigned* camy){if(ram16r(0xF43Cu)){ram8w(0xECECu,0x99u);field_event_force_disembark(camx,camy);unsigned old=field_dialogue_len,n=kos_decode(0x1F9580u,field_dialogue,FIELD_DIALOGUE_CAP);if(n){field_dialogue_len=n;field_dialogue_show(0x34u,*camx,*camy);field_dialogue_len=old;}}ps4_audio_stop();ps4_audio_play(0xA9u);field_cutscene_panels=1u;field_active_panel_count=0u;dma_fill32(field_pause_pal,512u,0u);panel_clear_scaled();pause_panel_action(0u,0x98u);{unsigned old=field_dialogue_len,n=kos_decode(0x1F9580u,field_dialogue,FIELD_DIALOGUE_CAP);if(n){field_dialogue_len=n;field_dialogue_show2(0x26u,*camx,*camy);field_dialogue_len=old;}}dialogue_obj_hide();field_event_translate_party(0,32,camx,camy);ps4_event_flag_set(0x95u);return field_event_run_battle_request(0x0Au,camx,camy);}
static void field_event_raw_object_step(unsigned slot,int dx,int dy,unsigned frames,unsigned* camx,unsigned* camy){if(slot>=field_obj_count||!field_objs[slot].active)return;struct FieldObjLite* o=&field_objs[slot];for(unsigned f=0u;f<frames;f++){o->x=(uint16_t)((int)o->x+dx);o->y=(uint16_t)((int)o->y+dy);o->anim=(uint8_t)((o->anim+1u)&3u);ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u+0x30u),o->x);ram16w((uint16_t)(0xC300u+(uint16_t)slot*0x40u+0x34u),o->y);hw_refresh_field_object_art(slot);field_event_choreography_tick(*camx,*camy);}}
static void field_event_raw_chaz_step(int dx,int dy,unsigned frames,unsigned* camx,unsigned* camy){for(unsigned f=0u;f<frames;f++){chaz.x+=dx;chaz.y+=dy;field_party_anim_advance(lead_char_id,&chaz.anim,&chaz.anim_clock);hw_chaz_key=0xFFFFu;ram16w(0xC030u,(uint16_t)chaz.x);ram16w(0xC034u,(uint16_t)chaz.y);field_event_choreography_tick(*camx,*camy);}chaz.anim=0u;chaz.anim_clock=0u;hw_chaz_key=0xFFFFu;}
static void field_event_source_facing_vector(uint8_t source_facing,int* dx,int* dy){*dx=0;*dy=0;if(source_facing==0u)*dy=1;else if(source_facing==4u)*dy=-1;else if(source_facing==8u)*dx=1;else if(source_facing==12u)*dx=-1;}
static int field_event_anger_tower_alys(unsigned* camx,unsigned* camy){field_dialogue_show(7u,*camx,*camy);const unsigned alys_slot=0u;uint8_t aface=(uint8_t)((chaz.facing^1u)<<2);field_event_set_source_object_facing(alys_slot,aface,camx,camy);int dx=0,dy=0;field_event_source_facing_vector(aface,&dx,&dy);field_event_raw_object_step(alys_slot,dx,dy,8u,camx,camy);uint8_t cface=(uint8_t)(chaz.facing<<2);field_event_source_facing_vector(cface,&dx,&dy);field_event_raw_chaz_step(-dx,-dy,16u,camx,camy);field_event_raw_object_step(alys_slot,dx,dy,8u,camx,camy);field_dialogue_show(8u,*camx,*camy);ps4_event_flag_set(0xE4u);chaz.x=0x01F0;chaz.y=0x01F0;chaz.facing=0u;chaz.anim=0u;chaz.step_left=0u;chaz.dx=chaz.dy=0;ram16w(0xC030u,0x01F0u);ram16w(0xC034u,0x01F0u);ram16w(0xC006u,0u);field_event_set_mapload_bit(3u);return field_event_run_battle_request(0x18u,camx,camy);}
static int field_event_phonon(unsigned* camx,unsigned* camy){ram8w(0x500Au,0xE1u);field_event_set_source_object_facing(3u,4u,camx,camy);field_event_wait_updates(0x27u,camx,camy);uint8_t ids[5];int demi_present=0;unsigned pn=ps4_party_ids(ids);for(unsigned i=0u;i<pn;i++)if(ids[i]==6u){demi_present=1;break;}if(!demi_present){field_dialogue_show(0x4Fu,*camx,*camy);ram8w(0x500Au,0xE1u);field_event_set_source_object_facing(3u,0u,camx,camy);ps4_flag_clear_type(1u,0x42u);return 1;}field_dialogue_show(0x4Eu,*camx,*camy);ram8w(0xED52u,5u);field_palette_all64_to_black(5u);field_event_wait_updates(0x27u,camx,camy);ram8w(0x500Au,0xCFu);field_event_wait_updates(0x27u,camx,camy);ram8w(0x500Au,0xCFu);field_event_wait_updates(0x0Eu,camx,camy);ram8w(0x500Au,0xCFu);field_event_wait_updates(9u,camx,camy);ram8w(0x500Au,0xC0u);field_event_wait_updates(0x1Du,camx,camy);ram8w(0x500Au,0xCDu);field_event_wait_updates(0x3Bu,camx,camy);ram8w(0x500Au,0xCFu);field_event_wait_updates(0x27u,camx,camy);field_palette_copy_saved_to_live();field_event_wait_updates(0u,camx,camy);ram8w(0xED52u,2u);field_palette_first32_from_black_to_current(2u);ram8w(0xED52u,0u);field_palette_all64_to_black(0u);field_palette_copy_saved_to_live();field_event_wait_updates(0u,camx,camy);ram8w(0xED52u,0u);field_palette_first32_from_black_to_current(0u);if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);ram8w(0xF867u,0x11u);return 1;}
static int field_event_ranch_owner(unsigned* camx,unsigned* camy){if(!field_obj_count)return 1;while(field_objs[0].active&&field_objs[0].x!=(uint16_t)chaz.x){int nx=(int)field_objs[0].x+(field_objs[0].x<(uint16_t)chaz.x?2:-2);field_event_move_source_object(0u,(uint16_t)nx,field_objs[0].y,camx,camy);}field_event_set_source_object_facing(0u,0u,camx,camy);field_dialogue_show(0x20u,*camx,*camy);field_event_clear_source_object(8u);field_event_clear_source_object(9u);field_event_set_source_object_type(0u,0x0194u);if(field_objs[0].active){field_objs[0].base_tile=0x024Du;field_event_move_source_object(0u,0x0110u,0x02D0u,camx,camy);field_event_set_source_object_facing(0u,0u,camx,camy);}ps4_event_flag_set(0x1Au);return 1;}
static int field_event_catching_rocky(unsigned* camx,unsigned* camy){const uint16_t map=ram16r(0xEC28u);unsigned slot=(map==0x0073u)?7u:((map==0x0039u)?11u:9u);field_event_face_source_object(slot,camx,camy);int has_cake=0;for(unsigned i=0u;i<40u;i++){uint8_t q=ram8r((uint16_t)(PS4_INV_BASE+i));if(!q)break;if(q==0x91u){has_cake=1;break;}}uint8_t first_dialogue=0u,escape_flag=0u,home_dialogue=0u;if(map==0x0039u){first_dialogue=0x35u;escape_flag=0x22u;home_dialogue=0x36u;}else if(map==0x0073u){first_dialogue=0x2Au;escape_flag=0x23u;home_dialogue=0x2Bu;}else if(map==0x007Bu){first_dialogue=0x4Cu;escape_flag=0x21u;home_dialogue=0x4Du;}else return 1;if(!has_cake){ps4_flag_clear_type(0u,0x21u);ps4_flag_clear_type(0u,0x22u);ps4_flag_clear_type(0u,0x23u);field_dialogue_show(first_dialogue,*camx,*camy);field_event_set_source_object_type(slot,0x02F8u);if(slot<field_obj_count&&field_objs[slot].active){uint16_t tx=(uint16_t)(field_objs[slot].x-0x00B0u),ty=field_objs[slot].y;field_event_move_source_object(slot,tx,ty,camx,camy);field_event_clear_source_object(slot);}if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);ps4_event_flag_set(escape_flag);return 1;}field_dialogue_show(home_dialogue,*camx,*camy);ps4_inventory_remove_all(0x91u);field_palette_fade_out();ps4_event_flag_set(0x24u);ram16w(0xEC28u,0x0065u);ram16w(0xEC2Au,0x0054u);ram16w(0xEC48u,0x003Cu);ram16w(0xEC4Au,0x0042u);ram16w(0xEC44u,0x000Cu);ram16w(0xEC46u,0x0010u);ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~0x08u));field_custom_resume_event=0x74u;field_event_reload_pending=1u;return 1;}
static int field_resume_custom_event(unsigned* camx,unsigned* camy);
static int field_event_tallas_rescued(unsigned* camx,unsigned* camy){const unsigned slot=2u;field_dialogue_show(0x27u,*camx,*camy);if(slot<field_obj_count){field_event_set_source_object_type(slot,0x02FCu);static const uint16_t path[][2]={{0x610u,0x2D0u},{0x610u,0x2D0u},{0x610u,0x320u},{0x670u,0x320u},{0x670u,0x300u},{0x5F0u,0x300u},{0x5F0u,0x390u}};for(unsigned i=0;i<7u;i++)field_event_move_source_object(slot,path[i][0],path[i][1],camx,camy);field_event_clear_source_object(slot);}ram8w(0xECECu,0x8Au);ps4_event_flag_set(0x3Eu);return 1;}
static void field_event_move_chaz_to(uint16_t tx,uint16_t ty,unsigned* camx,unsigned* camy){while(chaz.x!=(int)tx){int dx=chaz.x<(int)tx?2:-2;if((dx>0&&chaz.x+dx>(int)tx)||(dx<0&&chaz.x+dx<(int)tx))dx=(int)tx-chaz.x;chaz.x+=dx;chaz.facing=(uint8_t)(dx>0?2u:3u);field_party_anim_advance(lead_char_id,&chaz.anim,&chaz.anim_clock);ram16w(0xC030u,(uint16_t)chaz.x);choose_camera(camx,camy);field_event_choreography_tick(*camx,*camy);}while(chaz.y!=(int)ty){int dy=chaz.y<(int)ty?2:-2;if((dy>0&&chaz.y+dy>(int)ty)||(dy<0&&chaz.y+dy<(int)ty))dy=(int)ty-chaz.y;chaz.y+=dy;chaz.facing=(uint8_t)(dy>0?0u:1u);field_party_anim_advance(lead_char_id,&chaz.anim,&chaz.anim_clock);ram16w(0xC034u,(uint16_t)chaz.y);choose_camera(camx,camy);field_event_choreography_tick(*camx,*camy);}chaz.anim=0u;chaz.anim_clock=0u;chaz.step_left=0u;chaz.dx=chaz.dy=0;hw_chaz_key=0xFFFFu;field_event_redraw(camx,camy);}
static int field_event_king_rappy_defeated(unsigned* camx,unsigned* camy){const unsigned sekreas=2u,scripted=3u;if(sekreas<field_obj_count&&field_objs[sekreas].active){field_event_move_source_object(sekreas,0x03C0u,0x01C0u,camx,camy);field_event_clear_source_object(sekreas);}field_dialogue_show(0x37u,*camx,*camy);field_event_move_chaz_to(0x03E0u,0x0110u,camx,camy);if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);for(unsigned i=0u;i<16u;i++)cram[48u+i]=rom16(0x0730FCu+i*2u);field_palette_mirror_live(64u);hw_load_palettes();waitvb();if(scripted<field_obj_count){field_event_set_source_object_type(scripted,0x0194u);struct FieldObjLite* o=&field_objs[scripted];o->base_tile=0x0202u;o->facing=0u;o->x=0x03E0u;o->y=0x01C0u;ram16w((uint16_t)(0xC300u+scripted*0x40u+0x16u),0x0202u);ram16w((uint16_t)(0xC300u+scripted*0x40u+0x30u),o->x);ram16w((uint16_t)(0xC300u+scripted*0x40u+0x34u),o->y);hw_refresh_field_object_art(scripted);field_event_redraw(camx,camy);field_event_move_source_object(scripted,0x03E0u,0x0120u,camx,camy);if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);field_event_move_source_object(scripted,0x03E0u,0x01C0u,camx,camy);field_event_clear_source_object(scripted);}else if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);field_dialogue_show(0x38u,*camx,*camy);ps4_event_flag_set(0xBAu);return 1;}
static int field_event_penguin_feed_stolen(unsigned* camx,unsigned* camy){ps4_audio_stop();field_event_wait_updates(0x10u,camx,camy);ps4_audio_play(0xB3u);const unsigned slot=2u;if(slot<field_obj_count){field_event_set_source_object_type(slot,0x0368u);field_objs[slot].base_tile=0x0342u;field_objs[slot].x=0x0150u;field_objs[slot].y=0x0270u;field_event_move_source_object(slot,(uint16_t)chaz.x,0x0270u,camx,camy);for(unsigned f=0u;f<20u;f++){field_objs[slot].facing=(uint8_t)((f&3u)*4u);hw_refresh_field_object_art(slot);field_event_wait_updates(1u,camx,camy);}field_event_wait_updates(0x1Du,camx,camy);field_event_move_source_object(slot,0x02D0u,0x0270u,camx,camy);field_event_clear_source_object(slot);}field_dialogue_show(0x10u,*camx,*camy);ps4_audio_stop();field_event_wait_updates(0x10u,camx,camy);ps4_audio_play(0x87u);ps4_inventory_remove_all(0x92u);return 1;}
static int field_event_party_slot_for_id(uint8_t id){for(unsigned i=0u;i<5u;i++){uint8_t q=ram8r((uint16_t)(0xF40Au+i));if(q==0xFFu)break;if(q==id)return (int)i;}return -1;}
static struct PartyFollowerLite* field_event_follower_for_id(uint8_t id){for(unsigned i=0u;i<field_follower_count;i++)if(field_followers[i].active&&field_followers[i].id==id)return &field_followers[i];return 0;}
static void field_event_set_party_facing(uint8_t id,uint8_t source_facing){int slot=field_event_party_slot_for_id(id);if(slot<0)return;ram16w((uint16_t)(0xC000u+(uint16_t)slot*0x40u+0x06u),source_facing);uint8_t native=(uint8_t)((source_facing>>2)&3u);if(id==lead_char_id){chaz.facing=native;hw_chaz_key=0xFFFFu;}else{struct PartyFollowerLite* f=field_event_follower_for_id(id);if(f)f->facing=native;}}
static void field_event_set_party_dest(uint8_t id,uint16_t x,uint16_t y){int slot=field_event_party_slot_for_id(id);if(slot<0)return;ram16w((uint16_t)(0xC000u+(uint16_t)slot*0x40u+0x38u),x);ram16w((uint16_t)(0xC000u+(uint16_t)slot*0x40u+0x3Au),y);}
/* Event_MoveCharacter is not a point-to-point sprite tween.  In event mode the
 * original game runs Field_UpdateObjects for the whole Character_1..5 chain on
 * every frame.  Auto-input chooses the shortest wrapped route, movement starts
 * in authored 16-pixel steps, and the old position of a character becomes the
 * next party record's destination when a new step starts (unless ECFE bit 0 is
 * set).  Keep that behavior here rather than collision-clamping choreography. */
static unsigned field_event_axis_extent(unsigned y_axis){
    const unsigned cp=field_map.collision_plane?1u:0u;
    const unsigned cells=y_axis?(unsigned)(cp?field_map.col_bg:field_map.col_fg)+1u:(unsigned)(cp?field_map.row_bg:field_map.row_fg)+1u;
    return cells<<5;
}
static int field_event_wrapped_dir(int cur,int dst,unsigned extent){
    if(cur==dst)return 0;
    int d=dst-cur,ad=d<0?-d:d;
    if(extent){int wrap=(int)extent-ad;if(wrap<=ad)return d>0?-1:1;}
    return d>0?1:-1;
}
static unsigned field_event_source_speed(void){static const uint8_t px[4]={1u,2u,4u,8u};return px[ram8r(0xECE0u)&3u];}
static void field_event_sync_party_carrier(unsigned slot,uint8_t id,uint8_t moved){
    const uint16_t base=(uint16_t)(0xC000u+slot*0x40u);int x=(int)ram16r((uint16_t)(base+0x30u)),y=(int)ram16r((uint16_t)(base+0x34u));uint8_t face=(uint8_t)((ram16r((uint16_t)(base+0x06u))>>2)&3u);
    if(id==lead_char_id){chaz.x=x;chaz.y=y;chaz.facing=face;if(moved)field_party_anim_advance(id,&chaz.anim,&chaz.anim_clock);hw_chaz_key=0xFFFFu;return;}
    struct PartyFollowerLite* f=field_event_follower_for_id(id);if(!f)return;f->x=x;f->y=y;f->tx=x;f->ty=y;f->facing=face;f->moving=0u;if(moved)field_party_anim_advance(id,&f->anim,&f->anim_clock);
}
static void field_event_move_party_id(uint8_t id,uint16_t tx,uint16_t ty,unsigned* camx,unsigned* camy){
    int selected=field_event_party_slot_for_id(id);if(selected<0)return;field_event_set_party_dest(id,tx,ty);
    uint8_t step_left[5]={0u,0u,0u,0u,0u};int8_t step_dx[5]={0,0,0,0,0},step_dy[5]={0,0,0,0,0};
    const unsigned extent_x=field_event_axis_extent(0u),extent_y=field_event_axis_extent(1u);
    for(unsigned guard=0u;guard<32768u;guard++){
        uint16_t sb=(uint16_t)(0xC000u+(unsigned)selected*0x40u);if(ram16r((uint16_t)(sb+0x30u))==tx&&ram16r((uint16_t)(sb+0x34u))==ty)break;
        const uint8_t flags=ram8r(0xECFEu);const unsigned speed=field_event_source_speed();
        for(unsigned slot=0u;slot<5u;slot++){
            uint8_t cid=ram8r((uint16_t)(0xF40Au+slot));if(cid==0xFFu)break;uint16_t b=(uint16_t)(0xC000u+slot*0x40u);int x=(int)ram16r((uint16_t)(b+0x30u)),y=(int)ram16r((uint16_t)(b+0x34u));int dxst=(int)ram16r((uint16_t)(b+0x38u)),dyst=(int)ram16r((uint16_t)(b+0x3Au));
            if(!step_left[slot]&&(x!=dxst||y!=dyst)){
                int xd=field_event_wrapped_dir(x,dxst,extent_x),yd=field_event_wrapped_dir(y,dyst,extent_y);step_dx[slot]=step_dy[slot]=0;
                if((flags&2u)&&yd)step_dy[slot]=(int8_t)yd;else if(xd)step_dx[slot]=(int8_t)xd;else if(yd)step_dy[slot]=(int8_t)yd;
                step_left[slot]=16u;uint16_t sf=step_dx[slot]>0?8u:(step_dx[slot]<0?12u:(step_dy[slot]>0?0u:4u));ram16w((uint16_t)(b+0x06u),sf);
                if(!(flags&1u)&&slot+1u<5u&&ram8r((uint16_t)(0xF40Au+slot+1u))!=0xFFu){uint16_t nb=(uint16_t)(b+0x40u);ram16w((uint16_t)(nb+0x38u),(uint16_t)x);ram16w((uint16_t)(nb+0x3Au),(uint16_t)y);}
            }
            if(step_left[slot]){
                unsigned n=speed;if(n>step_left[slot])n=step_left[slot];x+=step_dx[slot]*(int)n;y+=step_dy[slot]*(int)n;
                if(extent_x){if(x<0)x+=(int)extent_x;else if(x>=(int)extent_x)x-=(int)extent_x;}if(extent_y){if(y<0)y+=(int)extent_y;else if(y>=(int)extent_y)y-=(int)extent_y;}
                step_left[slot]=(uint8_t)(step_left[slot]-n);ram16w((uint16_t)(b+0x30u),(uint16_t)x);ram16w((uint16_t)(b+0x34u),(uint16_t)y);field_event_sync_party_carrier(slot,cid,1u);
            }else field_event_sync_party_carrier(slot,cid,0u);
        }
        if(!(flags&4u))choose_camera(camx,camy);field_event_choreography_tick(*camx,*camy);
    }
    chaz.anim=0u;chaz.anim_clock=0u;chaz.step_left=0u;chaz.dx=chaz.dy=0;hw_chaz_key=0xFFFFu;for(unsigned i=0u;i<field_follower_count;i++){field_followers[i].anim=0u;field_followers[i].anim_clock=0u;}field_event_redraw(camx,camy);
}
static void field_event_move_party_group_40(unsigned* camx,unsigned* camy){static const uint8_t ids[5]={0u,3u,4u,5u,6u};static const uint16_t xs[5]={0x02F0u,0x02F0u,0x0310u,0x02E0u,0x0300u};static const uint16_t ys[5]={0x0380u,0x0370u,0x0380u,0x0380u,0x0380u};for(unsigned frame=0u;frame<40u;frame++){for(unsigned n=0u;n<5u;n++){int slot=field_event_party_slot_for_id(ids[n]);if(slot<0)continue;int* px=0;int* py=0;struct PartyFollowerLite* f=0;if(ids[n]==lead_char_id){px=&chaz.x;py=&chaz.y;}else{f=field_event_follower_for_id(ids[n]);if(!f)continue;px=&f->x;py=&f->y;}int dx=0,dy=0;if(*px<(int)xs[n])dx=2;else if(*px>(int)xs[n])dx=-2;else if(*py<(int)ys[n])dy=2;else if(*py>(int)ys[n])dy=-2;if(dx&&((dx>0&&*px+dx>(int)xs[n])||(dx<0&&*px+dx<(int)xs[n])))dx=(int)xs[n]-*px;if(dy&&((dy>0&&*py+dy>(int)ys[n])||(dy<0&&*py+dy<(int)ys[n])))dy=(int)ys[n]-*py;*px+=dx;*py+=dy;ram16w((uint16_t)(0xC000u+(uint16_t)slot*0x40u+0x30u),(uint16_t)*px);ram16w((uint16_t)(0xC000u+(uint16_t)slot*0x40u+0x34u),(uint16_t)*py);if(dx||dy){uint8_t face=(uint8_t)(dx>0?2u:(dx<0?3u:(dy>0?0u:1u)));if(ids[n]==lead_char_id){chaz.facing=face;field_party_anim_advance(ids[n],&chaz.anim,&chaz.anim_clock);hw_chaz_key=0xFFFFu;}else if(f){f->facing=face;field_party_anim_advance(ids[n],&f->anim,&f->anim_clock);}}if(f){f->tx=f->x;f->ty=f->y;f->moving=0u;}}field_event_choreography_tick(*camx,*camy);}chaz.anim=0u;chaz.anim_clock=0u;for(unsigned i=0u;i<field_follower_count;i++){field_followers[i].anim=0u;field_followers[i].anim_clock=0u;}for(unsigned n=0u;n<5u;n++)field_event_set_party_facing(ids[n],4u);}
static int field_event_zio_fort_barrier(unsigned* camx,unsigned* camy){
    if(!ps4_event_flag_test(0x63u)){field_dialogue_show(0x4Au,*camx,*camy);return 1;}
    field_dialogue_show(0x44u,*camx,*camy);static const uint8_t ids[5]={0u,3u,4u,5u,6u};static const uint16_t xs[5]={0x02F0u,0x02F0u,0x0310u,0x02E0u,0x0300u};static const uint16_t ys[5]={0x0380u,0x0370u,0x0380u,0x0380u,0x0380u};for(unsigned n=0u;n<5u;n++)field_event_set_party_dest(ids[n],xs[n],ys[n]);ram8w(0xECE0u,0u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|0x05u));
    field_event_move_party_id(3u,0x02F0u,0x0370u,camx,camy);for(unsigned n=0u;n<5u;n++)field_event_set_party_facing(ids[n],4u);field_event_move_party_group_40(camx,camy);
    for(unsigned slot=4u;slot<=7u&&slot<field_obj_count;slot++){field_objs[slot].active=1u;}field_event_redraw(camx,camy);ram8w(0x500Au,0xCEu);ps4_audio_play(0xCEu);waitvb();
    for(unsigned step=0u;step<7u;step++){uint32_t b=0x06EE16u+(uint32_t)step*2u;cram[49u]=rom16(b);cram[50u]=rom16(b+14u);cram[51u]=rom16(b+28u);field_palette_mirror_live(64u);hw_load_palettes();for(unsigned f=0u;f<30u;f++)waitvb();}
    for(unsigned f=0u;f<60u;f++)waitvb();for(unsigned slot=4u;slot<12u;slot++)field_event_clear_source_object(slot);field_event_redraw(camx,camy);
    field_event_set_party_facing(3u,0u);field_event_move_party_id(6u,0x0300u,0x0370u,camx,camy);field_event_set_party_facing(6u,0u);for(unsigned f=0u;f<30u;f++)waitvb();field_dialogue_show(0x45u,*camx,*camy);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&(uint8_t)~1u));field_event_move_party_id(0u,0x02F0u,0x0350u,camx,camy);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&(uint8_t)~4u));for(unsigned f=0u;f<60u;f++)field_event_choreography_tick(*camx,*camy);field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,1u,camx,camy);ram8w(0xECE0u,1u);ps4_event_flag_set(0x64u);return 1;
}
static int field_event_enter_anger_tower(unsigned* camx,unsigned* camy){if(!ps4_event_flag_test(0xD9u)){field_dialogue_show(5u,*camx,*camy);ps4_audio_stop();field_palette_fade_out();ram16w(0xEC28u,0x0002u);ram16w(0xEC2Au,0x00FCu);ram16w(0xEC48u,0x0044u);ram16w(0xEC4Au,0x00C2u);ram16w(0xEC44u,0u);ram16w(0xEC46u,0u);ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~0x08u));ram8w(0x500Au,0x90u);ram8w(0xECECu,0x90u);field_event_reload_pending=1u;return 1;}field_dialogue_show(6u,*camx,*camy);field_palette_save(64u);ram8w(0x500Au,0xE6u);for(unsigned f=0u;f<40u;f++){if((f&3u)==0u)field_palette_increase_tone_once();for(unsigned i=0u;i<16u;i++)cram[32u+i]=rom16(0x296300u+i*2u);field_palette_mirror_live(64u);hw_load_palettes();waitvb();}for(unsigned f=0u;f<40u;f++){if((f&3u)==0u)field_palette_restore_step();field_palette_mirror_live(64u);hw_load_palettes();waitvb();}if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);ps4_event_flag_set(0xE0u);return 1;}


static int field_event_add_transient(uint16_t type,uint16_t x,uint16_t y,uint16_t gba_tile){
    if(field_obj_count>=FIELD_MAX_OBJECTS)return -1;unsigned slot=field_obj_count++;struct FieldObjLite* o=&field_objs[slot];
    o->type=type;o->facing=0u;o->dialog=0u;o->base_tile=0u;o->x=x;o->y=y;o->active=1u;o->anim=0u;o->blocks=0u;o->treasure_type=o->treasure_flag=o->treasure_item=0u;field_obj_init_motion(o);o->gba_tile=gba_tile;o->render_w=16u;o->render_h=32u;o->render_x=-16;o->render_y=-2;return (int)slot;
}

/* Event_MachineCenterAppearing / MotaSpaceportAppearing /
 * DezoSpaceportAppearing ($06/$07/$11) are one source family.  The only
 * differences are the three Nemesis streams, camera center, 24 authored
 * lifting-particle records, the BG chunk replacement and completion flag. */
static int field_event_place_appearing(unsigned event_index,unsigned* camx,unsigned* camy){
    uint32_t art0,art1,art2,table;uint16_t cx,cy,lx,ly;uint8_t layout,flag,dialog=0u,pal=0x20u;
    if(event_index==0x06u){art0=0x1D3710u;art1=0x1D2ABCu;art2=0x1D2F4Eu;table=0x6B634u;cx=0x0730u;cy=0x0B40u;lx=0x39u;ly=0x5Au;layout=0xD3u;flag=0x43u;dialog=0x0Bu;}
    else if(event_index==0x07u){art0=0x1D3182u;art1=0x1D2ABCu;art2=0x1D2F4Eu;table=0x6B86Eu;cx=0x0340u;cy=0x0590u;lx=0x1Au;ly=0x2Du;layout=0x3Fu;flag=0x66u;}
    else if(event_index==0x11u){art0=0x1D3C1Eu;art1=0x1D4234u;art2=0x1D46C6u;table=0x6C1BCu;cx=0x00C0u;cy=0x0480u;lx=0x06u;ly=0x24u;layout=0x2Cu;flag=0x82u;pal=0u;}
    else return 0;
    unsigned n0=nemesis_fast(art0,&work_ram[0x0000u],0x0800u),n1=nemesis_fast(art1,&work_ram[0x0800u],0x0800u),n2=nemesis_fast(art2,&work_ram[0x1000u],0x0800u);if(!n0||!n1||!n2)return 1;
    hw_copy_frame_ram(FIELD_EVENT_TILE_FADEIN,0x0000u,0u);hw_copy_frame_ram(FIELD_EVENT_TILE_DIVIDE,0x0800u,0u);hw_copy_frame_ram(FIELD_EVENT_TILE_PARTICLE,0x1000u,4u);
    uint16_t oldcx=(uint16_t)chaz.x,oldcy=(uint16_t)chaz.y;field_event_move_camera_to(cx,cy,1u,camx,camy);
    int divide=-1,fade=-1;int particle_slot[24];uint8_t particle_life[24];for(unsigned q=0u;q<24u;q++){particle_slot[q]=-1;particle_life[q]=0u;}
    for(int d7=0x170;d7>=0;d7--){
        if((d7&7)==0)ram8w(0x500Au,0xDDu);
        if(d7==0xF8){uint16_t oy=(uint16_t)(cy+(event_index==0x07u?0x10u:0u));divide=field_event_add_transient(0x0078u,cx,oy,FIELD_EVENT_TILE_DIVIDE);fade=field_event_add_transient(0x0084u,cx,oy,FIELD_EVENT_TILE_FADEIN);if(divide>=0)field_objs[(unsigned)divide].render_y=-2;if(fade>=0)field_objs[(unsigned)fade].render_y=-2;}
        for(unsigned q=0u;q<24u;q++){uint32_t r=table+q*8u;if((int)rom16(r)==d7){int sl=field_event_add_transient(0x007Cu,rom16(r+2u),rom16(r+4u),FIELD_EVENT_TILE_PARTICLE);particle_slot[q]=sl;particle_life[q]=31u;if(sl>=0){field_objs[(unsigned)sl].base_tile=rom16(r+6u);field_objs[(unsigned)sl].render_y=-2;}}}
        if(d7==0x3D){(void)field_layout_write_world((unsigned)lx<<5,(unsigned)ly<<5,1u,layout);field_event_redraw(camx,camy);}
        for(unsigned q=0u;q<24u;q++)if(particle_life[q]){if(--particle_life[q]==0u&&particle_slot[q]>=0)field_event_clear_source_object((unsigned)particle_slot[q]);}
        if(divide>=0&&d7==0xF8-59)field_event_clear_source_object((unsigned)divide);if(fade>=0&&d7==0xF8-188)field_event_clear_source_object((unsigned)fade);
        field_event_choreography_tick(*camx,*camy);
    }
    field_event_move_camera_to(oldcx,oldcy,2u,camx,camy);if(dialog)field_dialogue_show(dialog,*camx,*camy);ps4_event_flag_set(flag);return 1;
}

/* Event_MeetingSaya ($0D).  The 68k source copies Hahn into a temporary
 * secondary-object slot, hides the party copy, then copies the temporary
 * object back after the scene.  The native follower is the same authored Hahn
 * state, so save/move/restore that follower while preserving the source
 * dialogue/facing/timing sequence and the 35-update SayaStars object. */
static int field_event_meeting_saya(unsigned* camx,unsigned* camy){
    field_dialogue_show(0x0Eu,*camx,*camy);
    struct PartyFollowerLite* h=field_event_follower_for_id(2u);int hx=0x0190,hy=0x0210;uint8_t hf=0u;
    if(h){hx=h->x;hy=h->y;hf=h->facing;field_event_move_party_id(2u,0x0190u,0x01F0u,camx,camy);field_event_set_party_facing(2u,8u);}
    field_event_set_source_object_facing(0u,12u,camx,camy);
    field_event_move_party_id(0u,0x0190u,0x0210u,camx,camy);
    field_event_set_source_object_facing(0u,12u,camx,camy);
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    field_event_set_source_object_facing(0u,0u,camx,camy);field_event_set_party_facing(2u,0u);
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    if(load_nem_vram_auto(0x1D48FAu,0x0580u)){
        const unsigned slot=7u;if(slot<field_obj_count){struct FieldObjLite* o=&field_objs[slot];o->type=0x01A0u;o->base_tile=0x0580u;o->x=0x01A8u;o->y=0x01FAu;o->facing=0u;o->active=1u;o->anim=0u;field_obj_init_motion(o);field_obj_configure_render(o,o->gba_tile);ram16w((uint16_t)(0xC300u+slot*0x40u),0x01A0u);ram16w((uint16_t)(0xC300u+slot*0x40u+0x16u),0x0580u);ram16w((uint16_t)(0xC300u+slot*0x40u+0x30u),o->x);ram16w((uint16_t)(0xC300u+slot*0x40u+0x34u),o->y);
            ram8w(0x500Au,0xCCu);for(unsigned f=0u;f<35u;f++){o->anim=(uint8_t)((f/5u)%6u);hw_refresh_field_object_art(slot);field_event_choreography_tick(*camx,*camy);}field_event_clear_source_object(slot);}
    }
    field_event_wait_updates(0x1Du,camx,camy);if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    field_event_set_party_facing(2u,8u);if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    field_event_set_source_object_facing(0u,12u,camx,camy);if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    /* loc_5A99E gives Saya and Hahn their small scripted drifts.  Preserve the
     * net source motion before the healing cast rather than teleporting the
     * scene directly to its final positions. */
    if(field_obj_count>0u&&field_objs[0].active){field_objs[0].x=(uint16_t)(field_objs[0].x+8u);ram16w(0xC330u,field_objs[0].x);}
    if(h){h->x+=4;h->tx=h->x;ram16w((uint16_t)(0xC000u+(uint16_t)field_event_party_slot_for_id(2u)*0x40u+0x30u),(uint16_t)h->x);}
    field_event_redraw(camx,camy);ram8w(0x500Au,0xBDu);field_event_wait_updates(0x59u,camx,camy);
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    if(field_obj_count>0u&&field_objs[0].active){field_objs[0].x=(uint16_t)(field_objs[0].x+4u);ram16w(0xC330u,field_objs[0].x);}
    field_event_wait_updates(0x13u,camx,camy);
    if(h){field_event_move_party_id(2u,(uint16_t)hx,(uint16_t)hy,camx,camy);h=field_event_follower_for_id(2u);if(h)h->facing=hf;}
    ps4_event_flag_set(0x12u);field_event_redraw(camx,camy);return 1;
}

/* Event_StrippersAppearing ($94): restore the authored Pao Pao cutscene, its
 * three entrance dancers, three temporary center dancers, final four source
 * object classes and TempEveFlag_StrippersDance ($1B). */
static int field_event_strippers_appearing(unsigned* camx,unsigned* camy){
    ram8w(0x500Au,0xFBu);field_event_wait_updates(9u,camx,camy);
    ram8w(0xECECu,0xB0u);ram8w(0x500Au,0xB0u);ps4_audio_play(0xB0u);field_event_move_camera_to(0x0220u,0x01C0u,1u,camx,camy);
    static const uint16_t ix[3]={0x0160u,0x0150u,0x0170u};
    for(unsigned q=0u;q<3u;q++){unsigned slot=1u+q;if(slot<field_obj_count){field_event_set_source_object_type(slot,0x0344u);field_objs[slot].x=ix[q];field_objs[slot].y=0x0190u;ram16w((uint16_t)(0xC300u+slot*0x40u+0x30u),ix[q]);ram16w((uint16_t)(0xC300u+slot*0x40u+0x34u),0x0190u);}}
    field_event_redraw(camx,camy);if(1u<field_obj_count)field_event_move_source_object(1u,0x0220u,0x0210u,camx,camy);field_event_wait_updates(0x3Bu,camx,camy);
    for(unsigned q=0u;q<3u;q++){unsigned slot=1u+q;if(slot<field_obj_count)field_event_set_source_object_facing(slot,0x10u,camx,camy);}
    static const uint16_t cx[3]={0x0210u,0x01F0u,0x0230u};
    for(unsigned q=0u;q<3u;q++){unsigned slot=4u+q;if(slot<field_obj_count){field_event_set_source_object_type(slot,0x0348u);field_objs[slot].x=cx[q];field_objs[slot].y=0x01F0u;ram16w((uint16_t)(0xC300u+slot*0x40u+0x30u),cx[q]);ram16w((uint16_t)(0xC300u+slot*0x40u+0x34u),0x01F0u);}}
    field_event_redraw(camx,camy);field_event_wait_updates(0x59u,camx,camy);for(unsigned q=0u;q<3u;q++)field_event_clear_source_object(4u+q);
    (void)load_nem_vram_auto(0x14A1CCu,0x0372u);field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,1u,camx,camy);
    static const uint16_t final_type[4]={0x034Cu,0x035Cu,0x0360u,0x0364u};for(unsigned q=0u;q<4u;q++)if(q<field_obj_count)field_event_set_source_object_type(q,final_type[q]);
    ps4_flag_set_type(2u,0x1Bu);field_event_redraw(camx,camy);return 1;
}



static void field_event_show_alt_dialogue(uint32_t tree,unsigned id,unsigned* camx,unsigned* camy){
    unsigned n=kos_decode(tree,field_dialogue,FIELD_DIALOGUE_CAP);if(!n)return;field_dialogue_len=n;field_dialogue_show(id,*camx,*camy);n=kos_decode(field_map.dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;field_dialogue_saved_cursor=0;
}
static void field_event_guild_dialogue(unsigned id,unsigned* camx,unsigned* camy){if(id>=0x1Fu)field_event_show_alt_dialogue(0x1F2400u,id,camx,camy);else field_dialogue_show(id,*camx,*camy);}

/* Event_HuntersGuild ($66).  Interpret the eight source HuntersGuildData
 * records directly: accepted/completed/fee flags, availability gates,
 * per-state dialogue IDs and exact 1000-meseta reward multipliers. */
static int field_event_hunters_guild(unsigned* camx,unsigned* camy){
    static const uint8_t d[8][8]={
        {0x19u,0x1Bu,0x1Du,0x14u,0x16u,0x18u,0x08u,0xD9u},{0x1Eu,0x24u,0x25u,0x1Au,0x1Cu,0x1Eu,0x44u,0xEBu},
        {0x26u,0x2Au,0x2Bu,0x1Fu,0x21u,0x23u,0x65u,0xEBu},{0x2Cu,0x2Fu,0x3Du,0x24u,0x26u,0x27u,0x65u,0xEBu},
        {0x78u,0x7Du,0x7Eu,0x28u,0x2Au,0x2Cu,0xC0u,0xEBu},{0xA8u,0xABu,0xACu,0x2Du,0x2Fu,0x30u,0xC0u,0xEBu},
        {0xADu,0xAFu,0xB0u,0x31u,0x33u,0x34u,0xC0u,0xEBu},{0xB1u,0xB6u,0xB8u,0x35u,0x37u,0x39u,0xC0u,0xD9u}};
    static const uint8_t reward[8]={5u,2u,3u,5u,50u,10u,0u,80u};
    static const char* const names[8]={"THE RANCH OWNER","TINKERBELL'S DOG","MISSING STUDENT","FISSURE OF FEAR","STAIN IN LIFE","DYING BOY","MAN WITH TWIST","SILVER SOLDIER"};
    unsigned exhausted=0u;
    for(unsigned j=0u;j<8u;j++){
        if(!ps4_event_flag_test(d[j][0]))continue;
        if(!ps4_event_flag_test(d[j][1])){
            if(j==0u&&ps4_event_flag_test(0xD9u)){field_event_guild_dialogue(0x19u,camx,camy);ps4_flag_clear_type(0u,d[0][0]);ps4_flag_clear_type(0u,d[0][1]);return 1;}
            field_event_guild_dialogue(d[j][4],camx,camy);return 1;
        }
        if(!ps4_event_flag_test(d[j][2])){field_event_guild_dialogue(d[j][5],camx,camy);ps4_event_flag_set(d[j][2]);uint32_t money=ram32r(PS4_MONEY)+(uint32_t)reward[j]*1000u;if(money>9999999u)money=9999999u;ram32w(PS4_MONEY,money);return 1;}
        if(ps4_event_flag_test(d[j][6])||ps4_event_flag_test(d[j][7]))++exhausted;
    }
    if(exhausted==8u){field_dialogue_show(0x12u,*camx,*camy);return 1;}
    field_dialogue_show(0x0Fu,*camx,*camy);int choice=ps4_ui_select_strings("HUNTERS' GUILD",names,8u,0u);if(choice<0){field_dialogue_show(0x13u,*camx,*camy);return 1;}unsigned j=(unsigned)choice;
    if(ps4_event_flag_test(d[j][2])){field_dialogue_show(0x11u,*camx,*camy);return 1;}
    if(!ps4_event_flag_test(d[j][6])){ps4_ui_message("LISTING PENDING",0);return 1;}
    if(ps4_event_flag_test(d[j][7])){field_event_guild_dialogue(j?0x3Au:0x19u,camx,camy);return 1;}
    field_event_guild_dialogue(d[j][3],camx,camy);if(ram8r(0xED53u)==0u)ps4_event_flag_set(d[j][0]);return 1;
}

static void field_event_macro_remove_char(uint8_t id){
    for(unsigned m=0u;m<8u;m++){uint16_t b=(uint16_t)(PS4_MACROS+m*20u);for(unsigned s=0u;s<5u;s++){uint16_t q=(uint16_t)(b+s*4u);if(!ram8r((uint16_t)(q+1u)))break;if(ram8r(q)!=id)continue;for(unsigned k=s;k<4u;k++){uint16_t dst=(uint16_t)(b+k*4u),src=(uint16_t)(dst+4u);for(unsigned z=0u;z<4u;z++)ram8w((uint16_t)(dst+z),ram8r((uint16_t)(src+z)));}for(unsigned z=0u;z<4u;z++)ram8w((uint16_t)(b+16u+z),0u);break;}}
}
static void field_event_macro_add_attack(uint8_t id){for(unsigned m=0u;m<8u;m++){uint16_t b=(uint16_t)(PS4_MACROS+m*20u);if(ram8r((uint16_t)(b+1u)))ps4_macro_write(m,id,1u,0u);}}

/* Event_PickingFifthCharacter ($68).  Candidate NPC slots C440..C540 map to
 * Gryz/Hahn/Demi/Raja/Kyra.  Preserve the source swap semantics, macro
 * compaction/ATTACK insertion and exactly-one picked flag. */
static int field_event_picking_fifth(unsigned* camx,unsigned* camy){
    static const uint8_t ids[5]={4u,2u,6u,8u,9u},flags[5]={0xDCu,0xDBu,0xDDu,0xDEu,0xDFu},dlg[5]={0x06u,0x02u,0x0Au,0x0Eu,0x12u};
    static const uint16_t xs[5]={0x01B0u,0x01A0u,0x0220u,0x0220u,0x0210u},ys[5]={0x0190u,0x01C0u,0x01C0u,0x0180u,0x01A0u},types[5]={0x02DCu,0x02E0u,0x02E4u,0x02E8u,0x02ECu};
    int tx=chaz.x,ty=chaz.y;if(chaz.facing==0u)ty+=16;else if(chaz.facing==1u)ty-=16;else if(chaz.facing==2u)tx+=16;else tx-=16;int pick=-1;
    for(unsigned q=0u;q<5u;q++){unsigned slot=5u+q;if(slot<field_obj_count&&field_objs[slot].active){int dx=(int)field_objs[slot].x-tx,dy=(int)field_objs[slot].y-ty;if(dx<0)dx=-dx;if(dy<0)dy=-dy;if(dx<=12&&dy<=12){pick=(int)q;break;}}}
    if(pick<0){static const char* const n[5]={"GRYZ","HAHN","DEMI","RAJA","KYRA"};pick=ps4_ui_select_strings("FIFTH MEMBER",n,5u,0u);if(pick<0)return 1;}
    unsigned q=(unsigned)pick;field_event_guild_dialogue(dlg[q],camx,camy);field_event_guild_dialogue((unsigned)dlg[q]+2u,camx,camy);if(ram8r(0xED53u)!=0u)return 1;
    uint8_t old=ram8r(0xF40Eu);if(old!=0xFFu&&old!=ids[q]){unsigned oi=5u;while(oi<10u&&ids[oi-5u]!=old)++oi;if(oi<10u){unsigned r=oi-5u;if(oi<field_obj_count){field_event_set_source_object_type(oi,types[r]);field_objs[oi].x=xs[r];field_objs[oi].y=ys[r];field_objs[oi].facing=0u;ram16w((uint16_t)(0xC300u+oi*0x40u+0x30u),xs[r]);ram16w((uint16_t)(0xC300u+oi*0x40u+0x34u),ys[r]);}}field_event_macro_remove_char(old);}
    ram8w(0xF40Eu,ids[q]);field_event_macro_add_attack(ids[q]);for(unsigned f=0xDBu;f<=0xDFu;f++)ps4_flag_clear_type(0u,f);ps4_event_flag_set(flags[q]);field_event_clear_source_object(5u+q);field_followers_sync(1);field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,2u,camx,camy);return 1;
}

/* Event_StrengthTowerTop ($5E) / Event_CourageTowerTop ($5F).  These scenes
 * are presentation-heavy source choreography with no branching; preserve the
 * authored object classes/positions, waits, sound writes, camera path and
 * completion flags so the state machine remains source-faithful while the
 * exceptional monster mappings are handled by the common field renderer. */
static int field_event_strength_tower_top(unsigned* camx,unsigned* camy){
    field_event_move_camera_to(0x01F0u,0x01C8u,1u,camx,camy);field_event_wait_updates(0x3Bu,camx,camy);
    field_event_set_source_object_facing(5u,4u,camx,camy);field_event_set_source_object_facing(6u,4u,camx,camy);field_event_wait_updates(0x0Eu,camx,camy);
    static const uint16_t xs[4]={0x0210u,0x01F0u,0x01D8u,0x01F8u},ys[4]={0x015Cu,0x0160u,0x0158u,0x015Cu};
    for(unsigned q=0u;q<4u;q++){unsigned slot=10u+q;if(slot<field_obj_count){field_event_set_source_object_type(slot,0x02ACu);field_objs[slot].facing=8u;field_objs[slot].x=xs[q];field_objs[slot].y=ys[q];ram16w((uint16_t)(0xC300u+slot*0x40u+0x30u),xs[q]);ram16w((uint16_t)(0xC300u+slot*0x40u+0x34u),ys[q]);}field_event_wait_updates(q==3u?0x26u:0x0Eu,camx,camy);}
    field_event_set_source_object_facing(5u,0u,camx,camy);field_event_wait_updates(3u,camx,camy);field_event_set_source_object_facing(6u,0u,camx,camy);field_event_wait_updates(0x3Bu,camx,camy);field_event_wait_updates(0x1Du,camx,camy);
    if(5u<field_obj_count&&field_objs[5].active){field_objs[5].y=(uint16_t)(field_objs[5].y+8u);ram16w((uint16_t)(0xC300u+5u*0x40u+0x34u),field_objs[5].y);}if(6u<field_obj_count&&field_objs[6].active){field_objs[6].y=(uint16_t)(field_objs[6].y+8u);ram16w((uint16_t)(0xC300u+6u*0x40u+0x34u),field_objs[6].y);}field_event_wait_updates(0x13u,camx,camy);
    if(14u<field_obj_count){field_event_set_source_object_type(14u,0x02A4u);field_objs[14].facing=4u;field_objs[14].x=0x01E8u;field_objs[14].y=0x01A4u;}if(15u<field_obj_count){field_event_set_source_object_type(15u,0x02A4u);field_objs[15].facing=8u;field_objs[15].x=0x0208u;field_objs[15].y=0x01A4u;}field_event_redraw(camx,camy);
    ram8w(0x500Au,0xC2u);field_event_wait_updates(0x2Fu,camx,camy);if(14u<field_obj_count)field_objs[14].facing=12u;if(15u<field_obj_count)field_objs[15].facing=16u;field_event_wait_updates(5u,camx,camy);ram8w(0x500Au,0xC3u);field_event_wait_updates(7u,camx,camy);
    field_event_move_camera_to(0x01F0u,0x0270u,8u,camx,camy);ram8w(0x500Au,0xB9u);field_event_wait_updates(0x1Du,camx,camy);field_event_clear_source_object(5u);field_event_clear_source_object(6u);field_event_clear_source_object(14u);field_event_clear_source_object(15u);field_event_wait_updates(0x3Bu,camx,camy);field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,2u,camx,camy);ps4_event_flag_set(0xE2u);return 1;
}
static int field_event_courage_tower_top(unsigned* camx,unsigned* camy){
    field_event_set_party_facing(0u,4u);field_event_move_camera_to(0x01F0u,0x01C0u,1u,camx,camy);field_event_wait_updates(0x3Bu,camx,camy);
    static const uint16_t ax[8]={0x01F2u,0x01EAu,0x01E6u,0x01DEu,0x0206u,0x01FEu,0x0212u,0x020Au};static const uint16_t ay[8]={0x01D8u,0x01DCu,0x01D8u,0x01DCu,0x01E0u,0x01DCu,0x01E0u,0x01DCu};static const uint8_t af[8]={8u,16u,12u,16u,8u,16u,12u,16u};
    for(unsigned q=0u;q<8u;q++){unsigned slot=9u+(q&3u);if(slot<field_obj_count){field_event_set_source_object_type(slot,0x02BCu);field_objs[slot].facing=af[q];field_objs[slot].x=ax[q];field_objs[slot].y=ay[q];ram16w((uint16_t)(0xC300u+slot*0x40u+0x30u),ax[q]);ram16w((uint16_t)(0xC300u+slot*0x40u+0x34u),ay[q]);}if((q&1u)!=0u)ram8w(0x500Au,0xBEu);field_event_wait_updates(q==3u?0x0Eu:(q==7u?9u:1u),camx,camy);}
    if(9u<field_obj_count){field_event_set_source_object_type(9u,0x02B4u);field_objs[9].facing=4u;field_objs[9].x=0x01E8u;field_objs[9].y=0x01D0u;}if(10u<field_obj_count){field_event_set_source_object_type(10u,0x02B4u);field_objs[10].facing=8u;field_objs[10].x=0x0210u;field_objs[10].y=0x01D0u;}ram8w(0x500Au,0xCDu);field_event_wait_updates(0x27u,camx,camy);
    if(9u<field_obj_count){field_objs[9].facing=12u;field_objs[9].x=0x01E2u;field_objs[9].y=0x01CCu;}if(10u<field_obj_count){field_objs[10].facing=16u;field_objs[10].x=0x020Au;field_objs[10].y=0x01CCu;}ram8w(0x500Au,0xC5u);field_event_wait_updates(1u,camx,camy);
    field_event_set_source_object_facing(5u,4u,camx,camy);field_event_set_source_object_facing(6u,4u,camx,camy);field_event_wait_updates(7u,camx,camy);field_event_wait_updates(0x13u,camx,camy);field_event_set_source_object_facing(5u,0x14u,camx,camy);field_event_set_source_object_facing(6u,0x14u,camx,camy);ram8w(0x500Au,0xB9u);field_event_wait_updates(0x1Du,camx,camy);field_event_clear_source_object(5u);field_event_clear_source_object(6u);field_event_wait_updates(0x1Du,camx,camy);field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,2u,camx,camy);ps4_event_flag_set(0xE3u);return 1;
}

/* Event_GaruberkTwEyeAction1/2 ($39/$3A).  Source decompresses three
 * $9E0-byte Kosinski animation frames into RAM_Start, cycles them into MD VRAM
 * $4060 every other frame, and commits the two replacement layouts after the
 * first eye.  Keep the authored ECF2/ECF3 frame/direction bytes so the second
 * eye resumes the same oscillator state. */
static void field_event_move_party_targets(const uint8_t* ids,const uint16_t* xs,const uint16_t* ys,unsigned count,unsigned frames,unsigned* camx,unsigned* camy){
    for(unsigned n=0u;n<count;n++)field_event_set_party_dest(ids[n],xs[n],ys[n]);
    for(unsigned fno=0u;fno<frames;fno++){
        for(unsigned n=0u;n<count;n++){
            int slot=field_event_party_slot_for_id(ids[n]);if(slot<0)continue;
            int* px=0;int* py=0;struct PartyFollowerLite* fl=0;
            if(ids[n]==lead_char_id){px=&chaz.x;py=&chaz.y;}
            else {fl=field_event_follower_for_id(ids[n]);if(!fl)continue;px=&fl->x;py=&fl->y;}
            int dx=0,dy=0;if(*px<(int)xs[n])dx=2;else if(*px>(int)xs[n])dx=-2;else if(*py<(int)ys[n])dy=2;else if(*py>(int)ys[n])dy=-2;
            if(dx&&((dx>0&&*px+dx>(int)xs[n])||(dx<0&&*px+dx<(int)xs[n])))dx=(int)xs[n]-*px;
            if(dy&&((dy>0&&*py+dy>(int)ys[n])||(dy<0&&*py+dy<(int)ys[n])))dy=(int)ys[n]-*py;
            *px+=dx;*py+=dy;
            if(ids[n]==lead_char_id){if(dx||dy){chaz.facing=(uint8_t)(dx>0?2u:(dx<0?3u:(dy>0?0u:1u)));field_party_anim_advance(ids[n],&chaz.anim,&chaz.anim_clock);hw_chaz_key=0xFFFFu;}}
            else {fl->tx=fl->x;fl->ty=fl->y;fl->moving=0u;if(dx||dy){fl->facing=(uint8_t)(dx>0?2u:(dx<0?3u:(dy>0?0u:1u)));field_party_anim_advance(ids[n],&fl->anim,&fl->anim_clock);}}
            ram16w((uint16_t)(0xC000u+(uint16_t)slot*0x40u+0x30u),(uint16_t)*px);ram16w((uint16_t)(0xC000u+(uint16_t)slot*0x40u+0x34u),(uint16_t)*py);
        }
        field_event_choreography_tick(*camx,*camy);
    }
    chaz.anim=0u;chaz.anim_clock=0u;for(unsigned i=0u;i<field_follower_count;i++){field_followers[i].anim=0u;field_followers[i].anim_clock=0u;}
}

/* Event_PlateSysEngine ($2D): source party convergence, authored Power Down
 * palette pulse, Demi response, return of Chaz, palette restoration and flag $61. */
static int field_event_plate_system_engine(unsigned* camx,unsigned* camy){
    static const uint8_t ids_all[5]={0u,4u,6u,5u,3u};
    static const uint16_t xs_all[5]={0x01E0u,0x01C0u,0x01E0u,0x0200u,0x0230u};
    static const uint16_t ys_all[5]={0x0190u,0x0180u,0x0170u,0x0190u,0x0170u};
    uint8_t ids[5];uint16_t xs[5],ys[5];unsigned n=0u;
    for(unsigned i=0u;i<5u;i++)if(field_event_party_slot_for_id(ids_all[i])>=0){ids[n]=ids_all[i];xs[n]=xs_all[i];ys[n]=ys_all[i];++n;}
    ram8w(0xECE0u,0u);field_event_move_party_targets(ids,xs,ys,n,80u,camx,camy);
    for(unsigned i=0u;i<n;i++)field_event_set_party_facing(ids[i],ids[i]==3u?8u:4u);
    field_dialogue_show(0x41u,*camx,*camy);ram8w(0x500Au,0xE4u); /* SFXID_PowerDown */
    static const uint16_t c0[6]={0x0860u,0x0A80u,0x0CA0u,0x0EC0u,0x0CA0u,0x0A80u};
    static const uint16_t c1[6]={0x00ACu,0x008Au,0x0068u,0x008Au,0x00ACu,0x7007u};
    for(unsigned k=0u;k<16u;k++){cram[28u]=c0[k%6u];cram[29u]=c1[k%6u];field_palette_mirror_live(64u);hw_load_palettes();for(unsigned w=0u;w<8u;w++)waitvb();}
    cram[28u]=0x0EC0u;cram[29u]=0x00CEu;field_palette_mirror_live(64u);hw_load_palettes();for(unsigned k=0u;k<40u;k++)waitvb();
    if(field_event_party_slot_for_id(6u)>=0){field_event_set_party_facing(6u,8u);field_event_wait_updates(0x0Eu,camx,camy);field_event_set_party_facing(6u,0u);field_event_wait_updates(0x1Du,camx,camy);}
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    field_event_move_party_id(0u,0x01E0u,0x01C0u,camx,camy);field_event_wait_updates(0x3Bu,camx,camy);
    ram8w(0xECE0u,1u);field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,1u,camx,camy);field_reload_palette_from_map();hw_load_palettes();ps4_event_flag_set(0x61u);return 1;
}

/* Event_DominatorsDefeated ($91): exact party destinations and 120-frame
 * convergence, Daughter palette shutdown, Wren turn/dialogue, Chaz return and
 * EventFlag_DaughterShutDown ($BB). */
static int field_event_dominators_defeated(unsigned* camx,unsigned* camy){
    uint8_t ids[5]={0u,3u,5u,7u,0xFFu};uint16_t xs[5]={0x02C0u,0x02E0u,0x0300u,0x02E0u,0x02F0u};uint16_t ys[5]={0x02E0u,0x02E0u,0x02E0u,0x02B0u,0x02E0u};unsigned count=4u;
    static const uint8_t fifth_try[6]={2u,4u,6u,8u,9u,10u};for(unsigned i=0u;i<6u;i++)if(field_event_party_slot_for_id(fifth_try[i])>=0){ids[4]=fifth_try[i];count=5u;break;}
    ram8w(0xECE0u,0u);field_event_move_party_targets(ids,xs,ys,count,120u,camx,camy);field_dialogue_show(5u,*camx,*camy);field_event_wait_updates(0x77u,camx,camy);
    ram8w(0x500Au,0xE4u);cram[28u]=0x06CEu;cram[29u]=0x0CE0u;field_palette_mirror_live(64u);hw_load_palettes();waitvb();
    static const uint16_t a[4]={0x06CEu,0x04ACu,0x028Au,0x0068u};static const uint16_t b[4]={0x0CE0u,0x0AC0u,0x08A0u,0x0680u};
    for(unsigned k=0u;k<4u;k++){cram[28u]=a[k];cram[29u]=b[k];field_palette_mirror_live(64u);hw_load_palettes();for(unsigned w=0u;w<16u;w++)waitvb();}waitvb();for(unsigned w=0u;w<30u;w++)waitvb();
    field_event_set_party_facing(7u,0u);if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);ps4_event_flag_set(0xBBu);
    field_event_move_party_id(0u,0x02E0u,0x02F0u,camx,camy);field_event_wait_updates(0x3Bu,camx,camy);ram8w(0xECE0u,1u);field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,2u,camx,camy);return 1;
}

/* Event_ZioNurvus ($34): source music stop, two Nemesis streams at tiles
 * $347/$580, 126-frame Zio/beam appearance, post-appearance drift, dialogue,
 * EventFlag $65 and event battle $06. */
static int field_event_zio_nurvus(unsigned* camx,unsigned* camy){
    ram8w(0x500Au,0xFBu);field_event_wait_updates(0x27u,camx,camy);field_event_wait_updates(0x13u,camx,camy);
    if(!load_nem_vram_auto(0x1D4E86u,0x0347u))return 1;if(!load_nem_vram_auto(0x1D51C4u,0x0580u))return 1;
    if(field_obj_count>0u){struct FieldObjLite* z=&field_objs[0];z->type=0x01E8u;z->facing=0u;z->base_tile=0x0347u;z->x=0x01E8u;z->y=0x01AAu;z->active=0u;z->anim=0u;field_obj_init_motion(z);ram16w(0xC300u,0x01E8u);ram16w(0xC306u,0u);ram16w(0xC316u,0x0347u);ram16w(0xC330u,z->x);ram16w(0xC334u,z->y);}
    if(field_obj_count>1u){struct FieldObjLite* b=&field_objs[1];b->type=0x01ECu;b->facing=0u;b->base_tile=0x0580u;b->x=0x01E0u;b->y=0x0180u;b->active=1u;b->anim=0u;field_obj_init_motion(b);ram16w(0xC340u,0x01ECu);ram16w(0xC346u,0u);ram16w(0xC356u,0x0580u);ram16w(0xC370u,b->x);ram16w(0xC374u,b->y);hw_refresh_field_object_art(1u);}
    ram8w(0x500Au,0xA3u);ps4_audio_play(0xA3u); /* MusicID_EnemyAppearance */
    for(unsigned f=0u;f<126u;f++){if(f==65u&&field_obj_count>0u){field_objs[0].active=1u;hw_refresh_field_object_art(0u);}if(f==125u&&field_obj_count>1u)field_objs[1].active=0u;field_event_choreography_tick(*camx,*camy);}
    field_event_wait_updates(0x13u,camx,camy);
    if(field_obj_count>0u&&field_objs[0].active){for(unsigned f=0u;f<33u;f++){if((f&3u)==3u){++field_objs[0].y;ram16w(0xC334u,field_objs[0].y);}field_event_choreography_tick(*camx,*camy);}}
    ram8w(0x500Au,0xA8u);ps4_audio_stop();ps4_audio_play(0xA8u);field_event_wait_updates(0u,camx,camy);field_dialogue_show(0x0Bu,*camx,*camy);ps4_event_flag_set(0x65u);ram8w(0xECECu,0xFEu);field_event_set_mapload_bit(7u);return field_event_run_battle_request(0x06u,camx,camy);
}

/* Event_LashiecAppearance ($5D): source Nemesis art at tile $371, white line-4
 * reveal, party turn sequence, Black Blood, two revision-1 hover passes,
 * dialogue $37, Lashiec flag $9B, saved Red Alert and battle $10. */
static int field_event_lashiec_appearance(unsigned* camx,unsigned* camy){
    if(!load_nem_vram_auto(0x1D7710u,0x0371u))return 1;field_palette_save(64u);for(unsigned i=48u;i<64u;i++)cram[i]=0x0EEEu;field_palette_mirror_live(64u);hw_load_palettes();
    field_event_move_camera_to(0x01E0u,0x0110u,1u,camx,camy);ram8w(0x500Au,0xA3u);ps4_audio_stop();ps4_audio_play(0xA3u);
    if(field_obj_count>1u){struct FieldObjLite* o=&field_objs[1];o->type=0x033Cu;o->base_tile=0x0371u;o->x=0x01E0u;o->y=0x0110u;o->facing=0u;o->active=1u;o->anim=0u;field_obj_init_motion(o);ram16w(0xC340u,0x033Cu);ram16w(0xC356u,0x0371u);ram16w(0xC370u,o->x);ram16w(0xC374u,o->y);hw_refresh_field_object_art(1u);}
    field_event_wait_updates(0x13u,camx,camy);for(unsigned f=0u;f<40u;f++)field_event_choreography_tick(*camx,*camy);field_event_move_camera_to(0x01E0u,0x0150u,1u,camx,camy);
    for(unsigned slot=0u;slot<5u;slot++){uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)break;field_event_set_party_facing(id,4u);if(slot==0u)field_event_wait_updates(4u,camx,camy);else if(slot==1u||slot==3u)field_event_wait_updates(9u,camx,camy);}
    ram8w(0x500Au,0xA8u);ps4_audio_stop();ps4_audio_play(0xA8u);field_event_wait_updates(0x13u,camx,camy);
    if(field_obj_count>1u&&field_objs[1].active){struct FieldObjLite* o=&field_objs[1];for(unsigned pass=0u;pass<2u;pass++){for(unsigned f=0u;f<4u;f++){if((f&3u)==3u)++o->x;if((f&7u)==7u)++o->y;field_event_choreography_tick(*camx,*camy);}for(unsigned f=0u;f<4u;f++){if((f&1u)==1u)--o->x;field_event_choreography_tick(*camx,*camy);}if(!pass)for(unsigned w=0u;w<40u;w++)field_event_choreography_tick(*camx,*camy);ram16w(0xC370u,o->x);ram16w(0xC374u,o->y);}}
    field_dialogue_show(0x37u,*camx,*camy);ps4_event_flag_set(0x9Bu);ram8w(0xECECu,0xA9u);field_event_set_mapload_bit(7u);return field_event_run_battle_request(0x10u,camx,camy);
}

/* Event_ReFaze ($63).  Yes_No_Option==0 takes the source battle-$19 branch;
 * otherwise Chaz receives Megid in the US bug-fixed final technique slot,
 * Re-Faze dissolves through the source white/palette sequence, Barrier Broken
 * fires, Tower becomes the saved music, and EventFlag $E1 is committed. */
static int field_event_refaze(unsigned* camx,unsigned* camy){
    field_dialogue_show(9u,*camx,*camy);field_event_wait_updates(0x27u,camx,camy);field_event_set_party_facing(0u,8u);field_event_wait_updates(0x13u,camx,camy);field_event_set_party_facing(0u,12u);field_event_wait_updates(0x13u,camx,camy);field_event_set_party_facing(0u,4u);
    ram8w(0x500Au,0xFBu);ram8w(0xED52u,5u);field_event_wait_updates(0x3Bu,camx,camy);field_palette_save(64u);
    if(field_obj_count>1u){field_event_set_source_object_type(1u,0x02C0u);field_objs[1].x=0x01F0u;field_objs[1].y=0x01D0u;ram16w(0xC370u,0x01F0u);ram16w(0xC374u,0x01D0u);}field_event_wait_updates(0x3Eu,camx,camy);for(unsigned i=54u;i<60u;i++)cram[i]=0x0EEEu;field_palette_mirror_live(64u);hw_load_palettes();field_event_wait_updates(7u,camx,camy);for(unsigned w=0u;w<64u;w++)field_event_choreography_tick(*camx,*camy);
    ram8w(0x500Au,0xC2u);field_event_wait_updates(0x87u,camx,camy);ram8w(0x500Au,0x9Fu);ps4_audio_stop();ps4_audio_play(0x9Fu);waitvb();if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    if(ram8r(0xED53u)==0u){field_event_clear_source_object(1u);ps4_event_flag_set(0xE1u);return field_event_run_battle_request(0x19u,camx,camy);}
    ram8w((uint16_t)(PS4_CHAR_BASE+0x52u+15u),0x10u); /* TechID_Megid */
    field_palette_save(64u);for(unsigned f=0u;f<80u;f++){if((f&3u)==0u)field_palette_increase_tone_once();field_palette_mirror_live(64u);hw_load_palettes();waitvb();}
    field_event_clear_source_object(1u);field_event_wait_updates(9u,camx,camy);for(unsigned i=0u;i<32u;i++)cram[i]=rom16(field_map.palette+i*2u);field_palette_mirror_live(64u);hw_load_palettes();ram8w(0x500Au,0xE6u);for(unsigned w=0u;w<40u;w++)field_event_choreography_tick(*camx,*camy);
    ram8w(0x500Au,0x9Au);ram8w(0xECECu,0x9Au);ps4_audio_stop();ps4_audio_play(0x9Au);ram8w(0xED52u,2u);ps4_event_flag_set(0xE1u);return 1;
}

static int field_event_garuberk_eye(unsigned event_index,unsigned* camx,unsigned* camy){
    if(event_index!=0x39u&&event_index!=0x3Au)return 0;
    if(!kos_decode(0x1C7460u,&work_ram[0x0000u],0x09E0u))return 1;
    if(!kos_decode(0x1C79F0u,&work_ram[0x09E0u],0x09E0u))return 1;
    if(!kos_decode(0x1C7FE0u,&work_ram[0x13C0u],0x09E0u))return 1;
    ram8w(0x500Au,0xD9u); /* SFXID_Fusion */
    if(event_index==0x39u)ps4_flag_set_type(2u,0x14u);
    unsigned count=event_index==0x39u?60u:120u;
    /* These authored animation frames overwrite virtual VRAM tiles $203..$251.
     * Invalidate the immutable GBA-facing aliases once before the first write. */
    field_gba_ready_invalidate(0x4060u>>5,0x09E0u>>5);
    for(unsigned f=0u;f<count;f++){
        if((f&1u)==0u){
            int frame=(int)ram8r(0xECF2u);unsigned reverse=ram8r(0xECF3u)&1u;
            if(!reverse){if(++frame>2){frame=2;reverse=1u;}}
            else {if(--frame<0){frame=0;reverse=0u;}}
            ram8w(0xECF2u,(uint8_t)frame);ram8w(0xECF3u,(uint8_t)reverse);
            dma_copy32(&work_ram[(unsigned)frame*0x09E0u],&md_vram[0x4060u],0x09E0u);
            /* Garuberk Eye is authored runtime-mutated VRAM; invalidate the
             * immutable aliases, then publish only the affected remapped tiles.
             * The three source streams decode to $9C0 bytes while the legacy
             * port copies $9E0, so the final copied tile stays canonical. */
            field_gba_ready_invalidate(0x0203u,0x004Fu);
            static const uint32_t frame_src[3]={0x1C7460u,0x1C79F0u,0x1C7FE0u};const uint8_t* ready=0;unsigned ready_bytes=0u;
            if(ps4_psc_fastgfx_lookup(PS4_FASTGFX_KOS,frame_src[(unsigned)frame],&ready,&ready_bytes)&&ready_bytes<=0x09E0u){
                hw_publish_pretranslated_tiles(0x0203u,ready,ready_bytes>>5);
                if(ready_bytes<0x09E0u)hw_publish_canonical_tiles(0x0203u+(ready_bytes>>5),(0x09E0u-ready_bytes)>>5);
            }else hw_load_bg_tiles();
        }
        field_event_choreography_tick(*camx,*camy);
    }
    if(event_index==0x39u){
        unsigned fg=kos_decode(0x1C8F7Au,&work_ram[0xA000u],0x1000u);
        unsigned bg=kos_decode(0x1C92BAu,&work_ram[0xB000u],0x1000u);
        if(fg&&bg){hw_ring_layout_dirty=1u;hw_fill_ring((int)(*camx>>3),(int)(*camy>>3));hw_set_scroll(*camx,*camy);}
    }else ps4_flag_set_type(2u,0x16u);
    field_event_redraw(camx,camy);return 1;
}

/* Event_TylerGraveOpening ($44).  The source only opens the grave when the
 * scripted secondary object is exactly at X=$120.  Its twelve stone pieces
 * split asymmetrically for 64 updates: x_step=$FFFFC000 (-1/4 px/frame) on
 * six pieces and $00008000 (+1/2 px/frame) on the other six. */
static int field_event_tyler_grave(unsigned* camx,unsigned* camy){
    const unsigned trigger_slot=2u;
    if(trigger_slot>=field_obj_count||(unsigned)field_objs[trigger_slot].x!=0x0120u){field_dialogue_show(0x22u,*camx,*camy);return 1;}
    if(!ps4_event_flag_test(0x84u))field_dialogue_show(0x1Eu,*camx,*camy);
    ram8w(0x500Au,0xDDu); /* SFXID_GraveOpening */
    field_event_move_camera_to((uint16_t)(chaz.x+4),(uint16_t)chaz.y,4u,camx,camy);
    field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,4u,camx,camy);
    {unsigned width=(unsigned)field_map.row_bg+1u,idx=12u*width+10u;if(idx<0x1000u)work_ram[0xB000u+idx]=0x47u;}
    field_event_redraw_explicit(*camx,*camy);
    static const uint8_t left_slots[6]={2u,4u,5u,8u,9u,10u};
    static const uint8_t right_slots[6]={3u,6u,7u,11u,12u,13u};
    for(unsigned f=0u;f<64u;f++){
        if((f&3u)==3u)for(unsigned q=0u;q<6u;q++){unsigned i=left_slots[q];if(i<field_obj_count&&field_objs[i].active){--field_objs[i].x;ram16w((uint16_t)(0xC300u+i*0x40u+0x30u),field_objs[i].x);}}
        if((f&1u)==1u)for(unsigned q=0u;q<6u;q++){unsigned i=right_slots[q];if(i<field_obj_count&&field_objs[i].active){++field_objs[i].x;ram16w((uint16_t)(0xC300u+i*0x40u+0x30u),field_objs[i].x);}}
        field_event_choreography_tick(*camx,*camy);
    }
    if(2u<field_obj_count)field_objs[2].facing=8u;if(3u<field_obj_count)field_objs[3].facing=12u;
    ram8w(0x500Au,0xDDu);
    field_event_move_camera_to((uint16_t)(chaz.x+4),(uint16_t)chaz.y,4u,camx,camy);
    field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,4u,camx,camy);
    field_event_wait_updates(0x3Bu,camx,camy);
    if(!ps4_event_flag_test(0x84u)&&field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    ps4_event_flag_set(0x84u);field_event_redraw(camx,camy);return 1;
}


/* Event_GameStart ($9F) is a four-map source event.  RefreshMap is an actual
 * execution seam in the 68k routine, so keep one small phase byte across the
 * native field reload rather than flattening the opening into teleports. */
static void field_game_start_queue_map(uint16_t map,uint16_t prev,uint16_t sx_tiles,uint16_t sy_tiles,uint8_t source_facing,uint8_t align,uint8_t next_phase){
    ram16w(0xEC28u,map);ram16w(0xEC2Au,prev);ram16w(0xEC48u,sx_tiles);ram16w(0xEC4Au,sy_tiles);ram16w(0xEC44u,source_facing);ram16w(0xEC46u,align);ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&(uint8_t)~0x08u));
    field_custom_resume_event=0x9Fu;field_custom_resume_phase=next_phase;field_event_reload_pending=1u;
}
static void field_game_start_swap_to_alys(unsigned* camx,unsigned* camy){
    /* Event_GameStart changes Party_Order to Alys/Chaz and then performs the
     * source trap-#1 $1F copy literally: all 0x40 bytes of Character_1 and
     * Character_2 are exchanged.  Position, destination, facing, animation
     * state and event flags therefore travel together; copying only native
     * x/y carriers was the root of the opening path divergence. */
    ram8w(0xF40Au,1u);ram8w(0xF40Bu,0u);for(unsigned i=2u;i<5u;i++)ram8w((uint16_t)(0xF40Au+i),0xFFu);
    uint8_t tmp[0x40u];for(unsigned i=0u;i<0x40u;i++)tmp[i]=ram8r((uint16_t)(0xC000u+i));
    for(unsigned i=0u;i<0x40u;i++)ram8w((uint16_t)(0xC000u+i),ram8r((uint16_t)(0xC040u+i)));
    for(unsigned i=0u;i<0x40u;i++)ram8w((uint16_t)(0xC040u+i),tmp[i]);
    lead_char_id=1u;
    chaz.x=(int)ram16r(0xC030u);chaz.y=(int)ram16r(0xC034u);chaz.facing=(uint8_t)((ram16r(0xC006u)>>2)&3u);chaz.anim=0u;chaz.anim_clock=0u;chaz.step_left=0u;chaz.dx=chaz.dy=0;hw_chaz_key=0xFFFFu;
    field_followers_sync(0);struct PartyFollowerLite* c=field_event_follower_for_id(0u);if(c){c->x=(int)ram16r(0xC070u);c->y=(int)ram16r(0xC074u);c->tx=c->x;c->ty=c->y;c->facing=(uint8_t)((ram16r(0xC046u)>>2)&3u);c->anim=0u;c->anim_clock=0u;c->moving=0u;}
    choose_camera(camx,camy);field_event_redraw(camx,camy);
}
static int field_event_game_start_begin(unsigned* camx,unsigned* camy){
    (void)camx;(void)camy;ram8w(0xECE0u,0u);ps4_audio_stop();field_palette_fade_out();ram8w(0x500Au,0x84u);ram8w(0xECECu,0x84u);ps4_audio_play(0x84u);
    field_game_start_queue_map(0x005Eu,0x0054u,0x0044u,0x0038u,0u,0u,0u);return 1;
}
static int r30_event_land_rover_resume(unsigned* camx,unsigned* camy);
static int field_resume_custom_event(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event==0x2Bu)return r30_event_land_rover_resume(camx,camy);
    if(field_custom_resume_event==0x74u){field_custom_resume_event=0xFFu;ram8w(0x500Au,0x9Eu);ram8w(0xECECu,0x9Eu);ps4_audio_stop();ps4_audio_play(0x9Eu);field_palette_fade_in();field_event_set_source_object_facing(1u,8u,camx,camy);field_dialogue_show(0x46u,*camx,*camy);return 1;}
    if(field_custom_resume_event!=0x9Fu)return 0;
    if(field_custom_resume_phase==0u){
        /* Chaz House: source manually places both characters after RefreshMap,
         * walks them together, then swaps Character_1/2 when Alys becomes lead. */
        chaz.x=0x0230;chaz.y=0x01A0;chaz.facing=0u;ram16w(0xC030u,0x0230u);ram16w(0xC034u,0x01A0u);ram16w(0xC038u,0x0230u);ram16w(0xC03Au,0x01A0u);
        struct PartyFollowerLite* a=field_event_follower_for_id(1u);if(a){a->x=0x02A0;a->y=0x0240;a->tx=a->x;a->ty=a->y;a->moving=0u;}ram16w(0xC070u,0x02A0u);ram16w(0xC074u,0x0240u);ram16w(0xC078u,0x0230u);ram16w(0xC07Au,0x0250u);
        choose_camera(camx,camy);field_event_redraw(camx,camy);field_palette_fade_in();
        /* Source Event_GameStart sets Alys's destination in Character_2,
         * enables Y-first + no-follow in Char_Move_Flags, then invokes the
         * ordinary event-mode Character_1 mover for Chaz. Field_UpdateObjects
         * advances the entire Character_1..5 chain each frame, so Alys reaches
         * her already-authored destination at the same time. The v0.9.10.2-
         * v0.9.10.4 custom two-sprite tween bypassed those shared 16-pixel
         * step/route semantics and visibly diverged from the opening path. */
        ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|3u));
        field_event_move_party_id(0u,0x0230u,0x0240u,camx,camy);
        ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&(uint8_t)~3u));field_game_start_swap_to_alys(camx,camy);field_event_wait_updates(0x27u,camx,camy);field_event_set_party_facing(1u,4u);
        field_event_show_alt_dialogue(0x1EBA90u,0x37u,camx,camy);field_event_move_party_id(1u,0x01F0u,0x0290u,camx,camy);field_palette_fade_out();
        field_game_start_queue_map(0x0054u,0x005Eu,0x002Cu,0x0048u,0u,0u,1u);return 1;
    }
    if(field_custom_resume_phase==1u){
        field_palette_fade_in();field_event_move_party_id(1u,0x0160u,0x0260u,camx,camy);field_event_set_party_facing(1u,8u);field_event_show_alt_dialogue(0x1EBA90u,0x38u,camx,camy);
        static const uint16_t path[][2]={{0x0180u,0x0300u},{0x0220u,0x03D0u},{0x02B0u,0x03D0u},{0x02B0u,0x0420u},{0x02D0u,0x0420u},{0x02D0u,0x0440u},{0x0300u,0x0530u}};
        for(unsigned i=0u;i<7u;i++)field_event_move_party_id(1u,path[i][0],path[i][1],camx,camy);
        ps4_audio_stop();field_palette_fade_out();ram8w(0x500Au,0x8Cu);ram8w(0xECECu,0x8Cu);ps4_audio_play(0x8Cu);field_game_start_queue_map(0x0000u,0x0054u,0x004Eu,0x0074u,8u,0x0Cu,2u);return 1;
    }
    if(field_custom_resume_phase==2u){
        field_palette_fade_in();field_event_show_alt_dialogue(0x1EBA90u,0x39u,camx,camy);field_event_move_party_id(1u,0x02B0u,0x0370u,camx,camy);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|4u));field_event_move_party_id(1u,0x03B0u,0x0370u,camx,camy);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&(uint8_t)~4u));field_palette_fade_out();
        game_start_world_text_sequence();ps4_audio_stop();
        ram8w(0xF40Au,0u);for(unsigned i=1u;i<5u;i++)ram8w((uint16_t)(0xF40Au+i),0xFFu);lead_char_id=0u;field_follower_count=0u;ram8w(0xECE0u,0u);
        field_game_start_queue_map(0x0013u,0xFFFFu,0x0060u,0x0024u,0u,0u,3u);return 1;
    }
    /* Final source phase: hold the newly refreshed Piata map for $1D updates,
     * then start/save Motabia Town music, reveal the field and set flag $07. */
    field_event_wait_updates(0x1Du,camx,camy);ram8w(0x500Au,0x84u);ram8w(0xECECu,0x84u);ps4_audio_stop();ps4_audio_play(0x84u);field_palette_fade_in();ps4_event_flag_set(0x07u);ram8w(0xECE0u,1u);field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}

static int field_event_noop(unsigned event_index){
    for(unsigned i=0u;i<PS4_NOOP_EVENT_DESCRIPTOR_COUNT;i++)
        if((unsigned)ps4_noop_event_descriptors[i].event_index==event_index)return 1;
    return 0;
}

static int field_event_after_igglanova(unsigned* camx,unsigned* camy){
    field_dialogue_show(0x12u,*camx,*camy);
    ram8w(0xECE0u,0u);
    field_event_set_party_facing(0u,8u);
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    field_event_set_party_facing(0u,4u);
    field_event_set_party_facing(1u,4u);
    field_event_set_party_facing(2u,0u);
    field_event_wait_updates(0x1Du,camx,camy);
    field_event_move_party_id(2u,0x0100u,0x00C0u,camx,camy);
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    uint8_t ids[4];unsigned n=0u;
    for(unsigned slot=1u;slot<5u;slot++){uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)break;ids[n++]=id;}
    for(unsigned i=0u;i<n;i++)field_event_move_party_id(ids[i],(uint16_t)chaz.x,(uint16_t)chaz.y,camx,camy);
    field_followers_sync_aligned(0u);
    field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,1u,camx,camy);
    field_event_set_party_facing(0u,0u);
    ram8w(0xECE0u,1u);ps4_event_flag_set(0x0Fu);field_event_redraw(camx,camy);return 1;
}
static int field_cutscene_meeting_rune(unsigned* camx,unsigned* camy){
    /* Cutscene_MeetingRune ($73F9C), source cutscene index 3.  This is the
     * first previously-unlowered party-join cutscene.  Preserve its durable
     * semantics exactly: Thray music, dialogue tree 3, Rune in party slot 4,
     * default ATTACK macro insertion, RuneJoined flag, stop music, and force
     * the previous-map sentinel so the next field transition cannot reuse a
     * stale map binding.  Native follower construction replaces FieldObj_Rune. */
    ram8w(0x500Au,0x94u); ps4_audio_stop(); ps4_audio_play(0x94u);
    field_dialogue_show(0x03u,*camx,*camy);
    ram8w(0xF40Du,3u); /* Current_Party_Slot_4 = CharID_Rune */
    field_event_macro_add_attack(3u);
    ps4_event_flag_set(0x11u); /* EventFlag_RuneJoined */
    field_followers_sync(1); hw_chaz_key=0xFFFFu; field_event_redraw(camx,camy);
    ram8w(0x500Au,0xFBu); ps4_audio_stop(); ram8w(0xECECu,0u);
    ram16w(0xEC2Au,0xFFFFu); /* Field_Map_Index_2 */
    return 1;
}
static int field_cutscene_dorin(unsigned* camx,unsigned* camy){
    /* Cutscene_Dorin (ps4.asm Cutscene_Dorin), source cutscene index 4 (Tonoe):
     * Rune leaves the party and Gryz joins.  Durable source semantics:
     *   dialogue $25 (Event_GetAndRunDialogue5)
     *   loc_63B3A(3): remove Rune's entry from every macro in use (compacting)
     *   clr.b Rune_Stats+$16 (status)
     *   loc_5A6E8(3,4): Gryz takes Rune's Current_Party_Slots position
     *   Event_AddMacro(4): ATTACK entry for Gryz in every macro in use
     *   MusicID_TonoeDePon ($81); EventFlag_GryzJoined ($30)
     *   Map_Start_X/Y = $3C/$3C + RefreshMap of the current map.
     * Rune's exit walk (FieldObj_NPCRuneSequence), the secondary object script
     * and the camera pan are choreography; the native follower rebuild replaces
     * FieldObj_Gryz insertion.  Map_Load_Flags bit 3 is not set: the port's
     * reload path does not consume it (field_event_chaz_house clears it). */
    field_dialogue_show(0x25u,*camx,*camy);
    field_event_macro_remove_char(3u);
    ram8w((uint16_t)(PS4_CHAR_BASE+3u*0x80u+0x16u),0u);
    for(unsigned s=0u;s<5u;s++){if(ram8r((uint16_t)(0xF40Au+s))==3u){ram8w((uint16_t)(0xF40Au+s),4u);break;}}
    field_event_macro_add_attack(4u);
    ram8w(0x500Au,0x81u);
    ps4_event_flag_set(0x30u);
    ram16w(0xEC48u,0x003Cu);ram16w(0xEC4Au,0x003Cu);
    field_followers_sync(1);hw_chaz_key=0xFFFFu;
    field_event_reload_pending=1u;
    return 1;
}
/* CP27 story cutscenes (subsystem audit P11), same durable-semantics pattern as
 * ChatGPT's field_cutscene_meeting_rune: music, dialogue (with the tree the source
 * loads), party/macro/stat changes, money, flags, saved music and the final
 * map/start position are exact; walking/camera choreography is replaced by the
 * native reload + follower rebuild.  Directly created manga panels (Panel_Create)
 * are NOT yet shown natively: field_dialogue_show() clears panels at dialogue
 * start, so they need a dialogue-path change (IDs listed per cutscene). */
static void field_cutscene_panel_wait(unsigned panel,unsigned frames){
    pause_panel_action(0u,panel);for(unsigned f=0u;f<frames;f++)waitvb();
}
static void field_cutscene_panels_clear(void){pause_panel_action(2u,0u);}
static void field_cutscene_tree_dialogue(uint32_t tree,uint8_t id,unsigned* camx,unsigned* camy){
    unsigned n=kos_decode(tree,field_dialogue,FIELD_DIALOGUE_CAP);
    if(n){field_dialogue_len=n;field_dialogue_show(id,*camx,*camy);}
    n=kos_decode(field_map.dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;
}
static void field_cutscene_reload(uint16_t map,uint16_t map2,uint16_t sx,uint16_t sy,uint16_t facing,uint16_t align){
    /* Field_Map_Index/_2, Map_Start_X/Y (cells), Map_Start_Facing_Dir, Map_Start_Char_Align,
     * bclr #3,Map_Load_Flags, RefreshMap (performed by the field loop's reload). */
    ram16w(0xEC28u,map);ram16w(0xEC2Au,map2);ram16w(0xEC48u,sx);ram16w(0xEC4Au,sy);
    ram16w(0xEC44u,facing);ram16w(0xEC46u,align);
    ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&(uint8_t)~0x08u));
    field_event_reload_pending=1u;
}
#include "r30_story_events.h"
#include "r31_story_events.h"
static int field_cutscene_alshline(unsigned* camx,unsigned* camy){
    /* Cutscene_Alshline, index 5 (Zema: the Alshline restores the petrified
     * villagers, then the Zema Igglanova battle).  Tree 3 stays loaded across
     * $67, its continuation and $68 so the saved dialogue cursor stays valid.
     * Panels not yet native: $1A-$22 (BarrierBroken SFX between), $25, $26. */
    ram8w(0x500Au,0xFBu);                                                  /* Sound_StopMusic */
    /* Source Panel_Create montage $1A-$22. VInt_PrepareLoop d0 waits d0+1 VBlanks. */
    field_cutscene_panel_wait(0x1Au,0x14u);
    field_cutscene_panel_wait(0x1Bu,0x14u);ram8w(0x500Au,0xD9u);for(unsigned f=0;f<0x3Cu;f++)waitvb();
    field_cutscene_panel_wait(0x1Cu,1u);ram8w(0x500Au,0xD9u);for(unsigned f=0;f<10u;f++)waitvb();
    field_cutscene_panel_wait(0x1Du,1u);ram8w(0x500Au,0xD9u);for(unsigned f=0;f<10u;f++)waitvb();
    field_cutscene_panel_wait(0x1Eu,1u);ram8w(0x500Au,0xD9u);for(unsigned f=0;f<0x5Au;f++)waitvb();
    field_cutscene_panels_clear();for(unsigned f=0;f<0x28u;f++)waitvb();
    field_cutscene_panel_wait(0x1Fu,0x3Cu);ram8w(0x500Au,0x84u);
    field_cutscene_panel_wait(0x20u,0x78u);field_cutscene_panels_clear();
    field_cutscene_panel_wait(0x21u,0x3Cu);field_cutscene_panel_wait(0x22u,1u);ram8w(0x500Au,0xD9u);for(unsigned f=0;f<0x5Au;f++)waitvb();
    field_cutscene_panels_clear();
    unsigned n=kos_decode(0x1E0BA0u,field_dialogue,FIELD_DIALOGUE_CAP);    /* DialogueTree3 */
    if(n){
        field_dialogue_len=n;
        field_dialogue_show(0x67u,*camx,*camy);                            /* Event_GetAndRunDialogue5 */
        field_cutscene_panel_wait(0x25u,0x3Cu);ram8w(0x500Au,0x84u);field_cutscene_panel_wait(0x26u,10u);
        (void)field_dialogue_resume_saved_keep_panels(*camx,*camy);        /* Event_RunDialogue5 */
        field_cutscene_panels_clear();
        ram8w(0x500Au,0xD9u);                                              /* SFXID_Fusion */
        field_dialogue_show2(0x68u,*camx,*camy);                           /* Event_GetAndRunDialogue2 */
    }
    n=kos_decode(field_map.dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;
    ps4_event_flag_set(0x33u);                                             /* EventFlag_IgglanovaZema */
    (void)ps4_inventory_remove_first(0x8Du);                               /* GetItem Alshline; clr.b (a0); ReorderInventory */
    field_cutscene_reload(0x0024u,0x0000u,0x0044u,0x003Eu,12u,16u);        /* MapID_Zema, prev Motavia */
    ram8w(0xECECu,0x84u);                                                  /* Saved_Sound_Index = MusicID_MotabiaTown */
    field_event_set_mapload_bit(7u);field_event_set_mapload_bit(3u);
    return field_event_run_battle_request(1u,camx,camy);                   /* Event_Battle_Index = 1 */
}
static int field_cutscene_zema_igglanova_defeated(unsigned* camx,unsigned* camy){
    /* Cutscene_ZemaIgglanovaDefeated, index 6 (Zema). */
    if(ram8r(0xECECu)!=0x84u){ram8w(0x500Au,0x84u);ram8w(0xECECu,0x84u);}   /* MusicID_MotabiaTown */
    field_cutscene_tree_dialogue(0x1E0BA0u,0x69u,camx,camy);               /* DialogueTree3, dialogue $69 */
    field_event_add_money(1000u);                                          /* addi.l #1000,Current_Money */
    ps4_event_flag_set(0x37u);                                             /* EventFlag_AfterIgglanovaZema */
    field_cutscene_reload(0x0024u,0xFFFFu,0x003Cu,0x0020u,4u,4u);          /* MapID_Zema */
    ram8w(0xECECu,0x84u);
    return 1;
}
static int field_cutscene_meeting_rika(unsigned* camx,unsigned* camy){
    /* Cutscene_MeetingRika, index 7 (Bio-plant B4 -> Zema -> Motavia).  All six
     * dialogues come from DialogueTree34, which is also the trigger map's tree
     * ($AC/$AD).  Event_RunDialogueN ignores d0 and continues the paused dialogue.
     * Manga panels not yet native: $33 $34 (before the continuation), $3B $3C (before 4). */
    field_dialogue_show(0x00u,*camx,*camy);
    ram8w(0x500Au,0x91u);ram8w(0xECECu,0x91u);                             /* MusicID_DungeonArrange1 */
    field_dialogue_show(0x01u,*camx,*camy);
    field_cutscene_panel_wait(0x33u,0x28u);field_cutscene_panel_wait(0x34u,0x14u);
    (void)field_dialogue_resume_saved_keep_panels(*camx,*camy);            /* Event_RunDialogue5: continues dialogue 1 */
    field_cutscene_panels_clear();
    ram8w(0x500Au,0xFBu);                                                  /* Sound_StopMusic */
    field_dialogue_show(0x02u,*camx,*camy);
    ram8w(0x500Au,0xF8u);ram8w(0x500Au,0xFDu);                             /* SpaceshipRadar, StopSpcSFX */
    ram8w(0xF40Eu,5u);                                                     /* Current_Party_Slot_5 = CharID_Rika */
    field_event_macro_add_attack(5u);
    ps4_event_flag_set(0x34u);                                             /* EventFlag_BioPlantEscape */
    ram8w(0x500Au,0x92u);                                                  /* MusicID_Fal */
    field_cutscene_tree_dialogue(0x1F7190u,0x03u,camx,camy);               /* DialogueTree34 */
    ram8w(0x500Au,0xFEu);ram8w(0x500Au,0xADu);                             /* StopAll, MusicID_Explosion */
    {unsigned n=kos_decode(0x1F7190u,field_dialogue,FIELD_DIALOGUE_CAP);if(n){field_dialogue_len=n;field_cutscene_panel_wait(0x3Bu,0x3Cu);field_cutscene_panel_wait(0x3Cu,0x14u);field_dialogue_show_keep_panels(0x04u,*camx,*camy);field_cutscene_panels_clear();}n=kos_decode(field_map.dialogue_tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;}
    ps4_event_flag_set(0x35u);                                             /* EventFlag_RikaJoined */
    field_cutscene_reload(0x0000u,0x0024u,0x00C6u,0x00A4u,0u,0u);          /* MapID_Motavia, prev Zema */
    ram8w(0x500Au,0x8Cu);ram8w(0xECECu,0x8Cu);                             /* MusicID_FieldMotabia */
    field_followers_sync(1);hw_chaz_key=0xFFFFu;
    return 1;
}
static int field_cutscene_demi_rescue(unsigned* camx,unsigned* camy){
    /* Cutscene_DemiRescue, index 8.  Panels not yet native: $40, $42 (around the Sword SFX). */
    field_dialogue_show(0x42u,*camx,*camy);                                /* Event_GetAndRunDialogue3 */
    field_cutscene_panel_wait(0x40u,1u);ram8w(0x500Au,0xF5u);for(unsigned f=0;f<0x3Cu;f++)waitvb();
    pause_panel_action(1u,0u);pause_panel_action(1u,0u);for(unsigned f=0;f<10u;f++)waitvb();
    field_cutscene_panel_wait(0x42u,0x14u);
    (void)field_dialogue_resume_saved_keep_panels(*camx,*camy);            /* Event_RunDialogue2: continues $42 */
    field_cutscene_panels_clear();
    for(unsigned s=0u;s<5u;s++){                                           /* +$1C HP (cap max), clear status */
        uint8_t id=ram8r((uint16_t)(0xF40Au+s));if(id&0x80u)break;
        uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)id*0x80u);
        unsigned hp=(unsigned)ram16r((uint16_t)(d+0x0Eu))+0x1Cu,mx=ram16r((uint16_t)(d+0x10u));
        if(hp>=mx){hp=mx;}
        ram16w((uint16_t)(d+0x0Eu),(uint16_t)hp);ram8w((uint16_t)(d+0x16u),0u);
    }
    ps4_event_flag_set(0x42u);                                             /* EventFlag_Zio */
    ram8w(0xECECu,0xA9u);                                                  /* Saved_Sound_Index = MusicID_RedAlert */
    field_event_set_mapload_bit(7u);field_event_set_mapload_bit(3u);
    return field_event_run_battle_request(4u,camx,camy);                   /* Event_Battle_Index = 4 (Zio) */
}
static int field_dispatch_event_index_inner(unsigned event_index,unsigned* camx,unsigned* camy);
/* R16: every source $F7 is followed by popdlg+Event_RunDialogue.  Lowered
 * handlers that mirror those resumes get exact action/text interleaving; a
 * handler that lacks one (audited: Event_WreckageEngine via
 * field_event_dialogue_once) must not lose its remaining segments, so the
 * dispatcher finishes any pending $F7 segments of the same entry (never past
 * its $FF) before returning. */
static int field_dispatch_event_index(unsigned event_index,unsigned* camx,unsigned* camy){
    field_dialogue_last_stop=0u;
    int r=field_dispatch_event_index_inner(event_index,camx,camy);
#if PS4_DIALOGUE_F7_SEGMENTS
    for(unsigned guard=0u;guard<16u&&field_dialogue_last_stop==0xF7u&&field_dialogue_saved_cursor;guard++)field_dialogue_resume_saved(*camx,*camy);
#endif
    return r;
}
static int field_dispatch_event_index_inner(unsigned event_index,unsigned* camx,unsigned* camy){
    if(event_index==0x0027u)return r31_event_rune_flaeli(camx,camy);
    if(event_index==0x0032u)return r31_event_meeting_dorin(camx,camy);
    if(event_index==0x0003u)return r30_event_alys_found(camx,camy);
    if(event_index==0x002Bu)return r30_event_land_rover(camx,camy);
    if(event_index==0x8003u)return field_cutscene_meeting_rune(camx,camy);
    if(event_index==0x8004u)return field_cutscene_dorin(camx,camy);
    if(event_index==0x8005u)return field_cutscene_alshline(camx,camy);
    if(event_index==0x8006u)return field_cutscene_zema_igglanova_defeated(camx,camy);
    if(event_index==0x8007u)return field_cutscene_meeting_rika(camx,camy);
    if(event_index==0x8008u)return field_cutscene_demi_rescue(camx,camy);
    if(event_index&0x8000u)return field_dispatch_generated_cutscene(event_index&0x7FFFu,camx,camy);
    if(field_event_noop(event_index))return 1;
    if(event_index==0x0006u||event_index==0x0007u||event_index==0x0011u)return field_event_place_appearing(event_index,camx,camy);
    if(event_index==0x000Du)return field_event_meeting_saya(camx,camy);
    if(event_index==0x002Du)return field_event_plate_system_engine(camx,camy);
    if(event_index==0x0034u)return field_event_zio_nurvus(camx,camy);
    if(event_index==0x005Du)return field_event_lashiec_appearance(camx,camy);
    if(event_index==0x005Eu)return field_event_strength_tower_top(camx,camy);
    if(event_index==0x005Fu)return field_event_courage_tower_top(camx,camy);
    if(event_index==0x0063u)return field_event_refaze(camx,camy);
    if(event_index==0x0066u)return field_event_hunters_guild(camx,camy);
    if(event_index==0x0068u)return field_event_picking_fifth(camx,camy);
    if(event_index==0x0039u||event_index==0x003Au)return field_event_garuberk_eye(event_index,camx,camy);
    if(event_index==0x0091u)return field_event_dominators_defeated(camx,camy);
    if(event_index==0x0094u)return field_event_strippers_appearing(camx,camy);
    if(event_index==0x0044u)return field_event_tyler_grave(camx,camy);
    if(event_index==0x000Eu)return field_event_aiedo_crush(camx,camy);
    if(event_index==0x0024u)return field_event_fainting_priest(camx,camy);
    if(event_index==0x002Eu)return field_event_rune_ladae(camx,camy);
    if(event_index==0x0030u)return field_event_zio_fort_barrier(camx,camy);
    if(event_index==0x0031u)return field_event_zio_fanatic(camx,camy);
    if(event_index==0x003Bu)return field_event_chaz_house(camx,camy);
    if(event_index==0x004Cu)return field_event_carnivorous_trees(camx,camy);
    if(event_index==0x004Du)return field_event_saving_kyra(camx,camy);
    if(event_index==0x0062u)return field_event_anger_tower_alys(camx,camy);
    if(field_event_defeated_guardians(event_index,camx,camy))return 1;
    if(event_index==0x006Cu)return field_event_phonon(camx,camy);
    if(event_index==0x0070u)return field_event_ranch_owner(camx,camy);
    if(event_index==0x0074u)return field_event_catching_rocky(camx,camy);
    if(event_index==0x007Eu)return field_event_tallas_rescued(camx,camy);
    if(event_index==0x0089u)return field_event_king_rappy_defeated(camx,camy);
    if(event_index==0x0096u)return field_event_penguin_feed_stolen(camx,camy);
    if(event_index==0x0099u)return field_event_enter_anger_tower(camx,camy);
    if(event_index==0x009Fu)return field_event_game_start_begin(camx,camy);
    if(event_index==0x0004u){
        if(ps4_event_flag_test(0x0Au))return 1;int oi=field_find_object_type(0x006Cu);if(oi>=0){field_animate_hahn_join((unsigned)oi,*camx,*camy);field_dialogue_show(0x11u,*camx,*camy);ram8w(0xF40Cu,2u);field_objs[oi].active=0u;}else ram8w(0xF40Cu,2u);field_followers_sync(1);
        uint32_t money=ram32r(0xF438u);money=(money>9999899u)?9999999u:money+100u;ram32w(0xF438u,money);ps4_event_flag_set(0x0Au);for(unsigned bi=0;bi<field_obj_count;bi++)if(field_objs[bi].type==0x0074u)field_objs[bi].active=0u;hw_prepare_static_obj_art();hw_update_objects(*camx,*camy);return 1;
    }
    if(event_index==0x0008u)return field_event_bioplant_door(camx,camy);
    if(event_index>=0x0009u&&event_index<=0x000Bu)return field_event_board_vehicle(event_index,camx,camy);
    if(event_index==0x0010u)return field_event_get_off_vehicle(camx,camy);
    if(event_index==0x000Cu){ /* Event_BasementContainers ($6BC36). */
        ps4_audio_stop();ps4_audio_play(0xABu);field_dialogue_show(0x0Fu,*camx,*camy);
        ps4_audio_stop();ps4_audio_play(0x8Au);ps4_event_flag_set(0x0Du);return 1;
    }
    if(field_dispatch_transition_event_script(event_index,camx,camy))return 1;
    if(field_event_palette_alarm_dialogue(event_index,camx,camy))return 1;
    if(field_event_palette_black_battle(event_index,camx,camy))return 1;
    if(field_event_party_formation_battle(event_index,camx,camy))return 1;
    if(field_event_inventory_reward(event_index,camx,camy))return 1;
    if(field_event_money_dialogue_transaction(event_index,camx,camy))return 1;
    if(field_event_coord_object_conditional_dialogue(event_index,camx,camy))return 1;
    if(field_event_paired_object_dialogue_state(event_index,camx,camy))return 1;
    if(field_event_rng_dialogue(event_index,camx,camy))return 1;
    if(field_event_dialogue_face_resume(event_index,camx,camy))return 1;
    if(field_event_choreographed_followup_dialogue(event_index,camx,camy))return 1;
    if(field_event_dialogue_successor_flag(event_index,camx,camy))return 1;
    if(field_event_simple_battle(event_index,camx,camy))return 1;
    if(field_event_quick_dialogue_battle(event_index,camx,camy))return 1;
    if(field_event_guarded_quick_battle(event_index,camx,camy))return 1;
    if(field_event_extended_battle(event_index,camx,camy))return 1;
    if(field_event_consume_item_dialogue(event_index,camx,camy))return 1;
    if(field_event_layout_animation(event_index,camx,camy))return 1;
    if(field_event_dynamic_two_cell_layout(event_index,camx,camy))return 1;
    if(field_event_fixed_two_row_layout(event_index,camx,camy))return 1;
    if(field_event_garuberk_door_entered(event_index,camx,camy))return 1;
    if(field_event_simple_dialogue_flag(event_index,camx,camy))return 1;
    if(field_event_facing_dialogue_flag(event_index,camx,camy))return 1;
    if(field_event_temp_flag_clear(event_index))return 1;
    if(field_event_dialogue_two_flags(event_index,camx,camy))return 1;
    if(field_event_dialogue_recovery(event_index,camx,camy))return 1;
    if(field_event_dialogue_camera_flag(event_index,camx,camy))return 1;
    if(field_event_conditional_dialogue_flag(event_index,camx,camy))return 1;
    if(field_event_tree_dialogue_flag(event_index,camx,camy))return 1;
    if(field_event_object_dialogue_patch(event_index,camx,camy))return 1;
    if(event_index==0x0013u)return field_event_elevator_door(camx,camy);
    if(event_index==0x0014u)return field_event_ride_elevator(camx,camy);
    if(event_index>=0x0015u&&event_index<=0x001Au)return field_event_moving_platform(event_index,camx,camy);
    if(event_index==0x001Bu||event_index==0x001Cu)return field_event_terminal(event_index,camx,camy);
    if(event_index>=0x001Du&&event_index<=0x0020u)return field_event_conveyor(event_index,camx,camy);
    if(event_index==0x0021u)return field_event_recovery(camx,camy);
    /* Source EventPtrs $23/$25/$28/$2A all end in EventFlags_Set after their
     * authored field dialogue.  Lower the shared progression primitive once;
     * camera/object choreography can layer on top without changing game state. */
    if(event_index==0x0023u)return field_event_dialogue_set_flag(0x3Eu,0x46u,camx,camy); /* GirlsSneakingOut */
    if(event_index==0x0025u)return field_event_after_igglanova(camx,camy); /* AfterIgglanova */
    if(event_index==0x0026u){ /* PrincipalConfession */
        ps4_audio_stop();ps4_audio_play(0x9Fu);
        field_dialogue_show(0x13u,*camx,*camy);
        field_event_add_money(300u);
        ps4_audio_stop();ps4_audio_play(0x84u);
        ps4_event_flag_set(0x0Cu);
        return 1;
    }
    if(event_index==0x0029u){ /* Event_WreckageEngine, loc_6DCB2: first non-Rika/non-Chaz speaker. */
        if(ps4_event_flag_test(0x40u)){field_dialogue_show(0x48u,*camx,*camy);return 1;}
        field_dialogue_show(0x47u,*camx,*camy);
        const uint8_t* p=field_dialogue_saved_cursor;
        if(field_dialogue_last_stop==0xF7u&&p&&p>=field_dialogue&&p+1u<field_dialogue+field_dialogue_len&&p[0]==0xF4u){
            for(unsigned i=0u;i<5u;i++){unsigned id=ram8r((uint16_t)(0xF40Au+i));if(id&0x80u)break;if(id!=0u&&id!=5u){field_dialogue[(p-field_dialogue)+1u]=(uint8_t)(id+1u);break;}}
            field_dialogue_resume_saved(*camx,*camy);
        }
        ps4_event_flag_set(0x40u);return 1;
    }
    if(event_index==0x0033u){ /* TonoeBasementDoor: dialogue always runs; door opens only after EventFlag $30. */
        field_dialogue_show(0x24u,*camx,*camy);
        if(ps4_event_flag_test(0x30u))ps4_event_flag_set(0x31u);
        return 1;
    }
    return 0;
}
static int field_adjacent_object(void){
    int tx=chaz.x,ty=chaz.y;if(chaz.facing==0u)ty+=16;else if(chaz.facing==1u)ty-=16;else if(chaz.facing==2u)tx+=16;else tx-=16;
    for(unsigned i=0;i<field_obj_count;i++)if(field_objs[i].active){int dx=(int)field_objs[i].x-tx,dy=(int)field_objs[i].y-ty;if(dx<0)dx=-dx;if(dy<0)dy=-dy;if(dx<=8&&dy<=8)return (int)i;}return -1;
}
static int field_interact_treasure(unsigned oi,unsigned* camx,unsigned* camy){
    if(oi>=field_obj_count)return 0;struct FieldObjLite* o=&field_objs[oi];if(o->type!=0x00A0u&&o->type!=0x01D4u)return 0;
    if(ps4_flag_test_type(1u,o->treasure_flag)){ps4_field_popup_text("IT'S EMPTY",0);hw_field_init(*camx,*camy);return 1;}
    /* FieldRoutine_ItemFound opens/animates the physical chest before its
     * capacity check.  Inventory-full therefore leaves this instance visibly
     * open but does not commit Chest_Flags; a later map load reconstructs it
     * closed from the still-clear flag. */
    field_cutscene_sound(0xE1u);o->facing=4u;hw_refresh_field_object_art(oi);hw_update_objects(*camx,*camy);
    if(o->treasure_type){
        uint32_t m=ram32r(PS4_MONEY),add=(uint32_t)o->treasure_item*100u;m+=add;if(m>9999999u)m=9999999u;ram32w(PS4_MONEY,m);
        ps4_field_popup_found_money(o->treasure_item);ps4_flag_set_type(1u,o->treasure_flag);hw_field_init(*camx,*camy);return 1;
    }
    if(ps4_inventory_count()>=40u){ps4_field_popup_text("INVENTORY FULL",0);hw_field_init(*camx,*camy);return 1;}
    ps4_field_popup_found_item(o->treasure_item);if(ps4_inventory_add(o->treasure_item)<0){hw_field_init(*camx,*camy);return 1;}
    ps4_flag_set_type(1u,o->treasure_flag);hw_field_init(*camx,*camy);return 1;
}
__attribute__((noinline))
static int field_interact_object(unsigned* camx,unsigned* camy){
    int oi=field_adjacent_object();if(oi<0)return 0;struct FieldObjLite* o=&field_objs[oi];if(o->type==0x00A0u||o->type==0x01D4u)return field_interact_treasure((unsigned)oi,camx,camy);const uint8_t* p=field_dialogue_resolve(o->dialog);const uint8_t* end=field_dialogue+field_dialogue_len;
    if(p<end&&*p!=0xF3u){uint8_t face=(uint8_t)((chaz.facing^1u)<<2);o->facing=face;hw_refresh_field_object_art((unsigned)oi);hw_update_objects(*camx,*camy);}
    int event_index=field_dialogue_show(o->dialog,*camx,*camy);if(event_index)return field_dispatch_event_index((unsigned)event_index,camx,camy);return 1;
}
static int field_run_simple_flag_event_condition(uint8_t id,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_SIMPLE_RUN_EVENT_CONDITION_COUNT;i++){
        const struct Ps4SimpleRunEventCondition* c=&ps4_simple_run_event_conditions[i];
        if(c->run_id!=id)continue;
        if((unsigned)ps4_event_flag_test(c->flag)!=(unsigned)c->require_set)return 0;
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    return 0;
}
static int field_run_flag_chain_event_condition(uint8_t id,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_RUN_EVENT_FLAG_CHAIN_CONDITION_COUNT;i++){
        const struct Ps4RunEventFlagChainCondition* c=&ps4_run_event_flag_chain_conditions[i];
        if(c->run_id!=id)continue;
        const uint8_t banks[3]={c->bank0,c->bank1,c->bank2};
        const uint8_t flags[3]={c->flag0,c->flag1,c->flag2};
        const uint8_t required[3]={c->require0,c->require1,c->require2};
        for(unsigned n=0u;n<c->count&&n<3u;n++)
            if((unsigned)ps4_flag_test_type(banks[n],flags[n])!=(unsigned)required[n])return 0;
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    return 0;
}
__attribute__((noinline))
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int field_run_flag_coord_eq_event_condition(uint8_t id,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_RUN_EVENT_FLAG_COORD_EQ_CONDITION_COUNT;i++){
        const struct Ps4RunEventFlagCoordEqCondition* c=&ps4_run_event_flag_coord_eq_conditions[i];
        if(c->run_id!=id)continue;
        const uint8_t banks[3]={c->bank0,c->bank1,c->bank2};
        const uint8_t flags[3]={c->flag0,c->flag1,c->flag2};
        const uint8_t required[3]={c->require0,c->require1,c->require2};
        for(unsigned n=0u;n<c->count&&n<3u;n++)
            if((unsigned)ps4_flag_test_type(banks[n],flags[n])!=(unsigned)required[n])return 0;
        if(c->x!=0xFFFFu&&(unsigned)chaz.x!=(unsigned)c->x)return 0;
        if(c->y!=0xFFFFu&&(unsigned)chaz.y!=(unsigned)c->y)return 0;
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    return 0;
}
__attribute__((noinline))
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int field_run_flag_coord_range_event_condition(uint8_t id,unsigned* camx,unsigned* camy){
    for(unsigned i=0u;i<PS4_RUN_EVENT_FLAG_COORD_RANGE_CONDITION_COUNT;i++){
        const struct Ps4RunEventFlagCoordRangeCondition* c=&ps4_run_event_flag_coord_range_conditions[i];
        if(c->run_id!=id)continue;
        const uint8_t banks[3]={c->bank0,c->bank1,c->bank2};
        const uint8_t flags[3]={c->flag0,c->flag1,c->flag2};
        const uint8_t required[3]={c->require0,c->require1,c->require2};
        for(unsigned n=0u;n<c->flag_count&&n<3u;n++)
            if((unsigned)ps4_flag_test_type(banks[n],flags[n])!=(unsigned)required[n])return 0;
        const uint8_t axes[4]={c->axis0,c->axis1,c->axis2,c->axis3};
        const uint8_t ops[4]={c->op0,c->op1,c->op2,c->op3};
        const uint16_t values[4]={c->value0,c->value1,c->value2,c->value3};
        for(unsigned n=0u;n<c->cmp_count&&n<4u;n++){
            unsigned actual=axes[n]?(unsigned)chaz.y:(unsigned)chaz.x,value=(unsigned)values[n];int pass=0;
            if(ops[n]==0u)pass=actual==value;else if(ops[n]==1u)pass=actual<=value;else if(ops[n]==2u)pass=actual>value;else if(ops[n]==3u)pass=actual<value;else if(ops[n]==4u)pass=actual>=value;
            if(!pass)return 0;
        }
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    return 0;
}
static int field_run_bound_event_condition(uint8_t id,unsigned* camx,unsigned* camy){
    if(id>=128u)return 0;uint16_t bind=ps4_run_event_condition_binding[id];if(bind==0xFFFFu)return 0;unsigned kind=bind>>8,index=bind&0xFFu;
    if(kind==1u){
        const struct Ps4SimpleRunEventCondition* c=&ps4_simple_run_event_conditions[index];
        if((unsigned)ps4_event_flag_test(c->flag)!=(unsigned)c->require_set)return 0;
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    if(kind==2u){
        const struct Ps4RunEventFlagChainCondition* c=&ps4_run_event_flag_chain_conditions[index];
        const uint8_t banks[3]={c->bank0,c->bank1,c->bank2},flags[3]={c->flag0,c->flag1,c->flag2},required[3]={c->require0,c->require1,c->require2};
        for(unsigned n=0u;n<c->count&&n<3u;n++)if((unsigned)ps4_flag_test_type(banks[n],flags[n])!=(unsigned)required[n])return 0;
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    if(kind==3u){
        const struct Ps4RunEventFlagCoordEqCondition* c=&ps4_run_event_flag_coord_eq_conditions[index];
        const uint8_t banks[3]={c->bank0,c->bank1,c->bank2},flags[3]={c->flag0,c->flag1,c->flag2},required[3]={c->require0,c->require1,c->require2};
        for(unsigned n=0u;n<c->count&&n<3u;n++)if((unsigned)ps4_flag_test_type(banks[n],flags[n])!=(unsigned)required[n])return 0;
        if(c->x!=0xFFFFu&&(unsigned)chaz.x!=(unsigned)c->x)return 0;if(c->y!=0xFFFFu&&(unsigned)chaz.y!=(unsigned)c->y)return 0;
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    if(kind==4u){
        const struct Ps4RunEventFlagCoordRangeCondition* c=&ps4_run_event_flag_coord_range_conditions[index];
        const uint8_t banks[3]={c->bank0,c->bank1,c->bank2},flags[3]={c->flag0,c->flag1,c->flag2},required[3]={c->require0,c->require1,c->require2};
        for(unsigned n=0u;n<c->flag_count&&n<3u;n++)if((unsigned)ps4_flag_test_type(banks[n],flags[n])!=(unsigned)required[n])return 0;
        const uint8_t axes[4]={c->axis0,c->axis1,c->axis2,c->axis3},ops[4]={c->op0,c->op1,c->op2,c->op3};const uint16_t values[4]={c->value0,c->value1,c->value2,c->value3};
        for(unsigned n=0u;n<c->cmp_count&&n<4u;n++){unsigned actual=axes[n]?(unsigned)chaz.y:(unsigned)chaz.x,value=(unsigned)values[n];int pass=ops[n]==0u?actual==value:ops[n]==1u?actual<=value:ops[n]==2u?actual>value:ops[n]==3u?actual<value:ops[n]==4u?actual>=value:0;if(!pass)return 0;}
        return field_dispatch_event_index(c->event_index,camx,camy);
    }
    return 0;
}
__attribute__((noinline))
/* CP25 (subsystem audit P11): the 12 RunEventsJmpTbl conditions that neither the
 * native chain above nor the generated condition families covered, each lowered
 * line-for-line from its source routine (flag values from ps4.constants.asm).
 * Coordinates are Character_1 curr_x/curr_y (chaz.x/chaz.y); bcs/bhi are the
 * unsigned lower/upper bounds.  Event_Index values with bit 15 set are story
 * cutscenes; they dispatch through field_dispatch_generated_cutscene() and
 * simply do not fire until that cutscene is lowered.  Returns -1 if `id` is not
 * one of these routines. */
static int ps4_inventory_contains(uint8_t item){
    /* GetItem ($5D… ): Z set (d0=0) when the item is anywhere in the compacted inventory. */
    if(!item)return 0;
    for(unsigned i=0u;i<40u;i++){if(ram8r((uint16_t)(PS4_INV_BASE+i))==item)return 1;}
    return 0;
}
static int field_run_cp25_event_condition(uint8_t id,unsigned* camx,unsigned* camy){
    const unsigned x=(unsigned)chaz.x,y=(unsigned)chaz.y;
    switch(id){
    case 0x06u: /* RunEvent_MachineCenter */
        if(ps4_event_flag_test(0x43u)||!ps4_event_flag_test(0x42u))return 0;
        if(y!=0x0AD0u||x<0x0710u||x>0x0750u)return 0;
        return field_dispatch_event_index(0x0006u,camx,camy);
    case 0x0Cu: /* RunEvent_BioPlantAlarm: TempEveFlag_BioPlantAlarm (Temp_Event_Flags $F140 = bank 2) */
        if(ps4_flag_test_type(2u,0x08u)||y!=0x0180u)return 0;
        return field_dispatch_event_index(0x0012u,camx,camy);
    case 0x1Au: /* RunEvent_MeetingRika -> Cutscene_MeetingRika */
        if(ps4_event_flag_test(0x34u)||y!=0x01A0u)return 0;
        return field_dispatch_event_index(0x8007u,camx,camy);
    case 0x1Cu: /* RunEvent_SavingDemi -> Cutscene_DemiRescue */
        if(ps4_event_flag_test(0x42u)||y!=0x0170u)return 0;
        return field_dispatch_event_index(0x8008u,camx,camy);
    case 0x1Fu: /* RunEvent_ZioNurvus */
        if(ps4_event_flag_test(0x65u)||y!=0x01E0u)return 0;
        return field_dispatch_event_index(0x0034u,camx,camy);
    case 0x22u:{ /* RunEvent_EnterGrbkTwDoor: layout chunk under/over Chaz on the collision plane ($EC24) */
        const unsigned plane=ram8r(0xEC24u);
        if(field_layout_chunk_world(x,(y+0x10u)&0xFFFFu,plane)!=0x3Cu)return 0;
        unsigned ev=0x0037u;
        if(field_layout_chunk_world(x,(y-0x10u)&0xFFFFu,plane)==0x3Eu)ev=0x0038u;
        return field_dispatch_event_index(ev,camx,camy);
    }
    case 0x35u: /* RunEvent_CarnivorousTrees */
        if(ps4_event_flag_test(0x9Cu))return 0;
        if(y<=0x00C0u&&x>=0x0BA0u&&x<=0x0BB0u){
            unsigned ev=0x004Cu;
            if(ps4_event_flag_test(0x94u)&&!ps4_event_flag_test(0x95u))ev=0x004Du;
            return field_dispatch_event_index(ev,camx,camy);
        }
        if(!ps4_event_flag_test(0x95u)||ps4_event_flag_test(0xA0u))return 0;
        return field_dispatch_event_index(0x8013u,camx,camy);
    case 0x51u: /* RunEvent_AngerTowerTop */
        if(ps4_event_flag_test(0xE4u))return 0;
        if(x<0x01C0u||x>0x01D0u||y<0x0250u||y>0x0260u)return 0;
        return field_dispatch_event_index(0x0069u,camx,camy);
    case 0x52u: /* RunEvent_AngerTowerExitTop */
        if(ps4_event_flag_test(0xE7u))return 0;
        if(y!=0x0260u||x<0x01A0u||x>0x01B0u)return 0;
        return field_dispatch_event_index(0x006Au,camx,camy);
    case 0x53u: /* RunEvent_ProfoundDarkness -> Cutscene_ProfoundDarkness */
        if(ps4_event_flag_test(0xE8u)||y!=0x0160u)return 0;
        return field_dispatch_event_index(0x8020u,camx,camy);
    case 0x70u: /* RunEvent_MileSandWorm: UpdateRNGSeed only once inside the window, then RNG_Seed&$1F==0 */
        if(ps4_event_flag_test(0x1Bu))return 0;
        if(x>0x0170u||y<0x0150u||y>0x02B0u)return 0;
        if(field_npc_rng_next()&0x1Fu)return 0;
        return field_dispatch_event_index(0x0071u,camx,camy);
    case 0x7Bu: /* RunEvent_PenguFeedStolen: GetItem(ItemID_PenguFeed $92) found */
        if(!ps4_inventory_contains(0x92u)||y!=0x0260u)return 0;
        return field_dispatch_event_index(0x0096u,camx,camy);
    default: return -1;
    }
}
/* R14C: returned to ROM (4,328 B).  It runs at most once per completed step
 * (field_run_map_events is gated by !chaz.step_left) and executes one short
 * switch arm per plan ID; R10 placed it in IWRAM and walking did not improve.
 * The IWRAM is reassigned to the P134R per-sample audio kernels, which run
 * ~2,000-3,500 inner iterations every VBlank. */
__attribute__((noinline,aligned(16)))
static int field_run_map_event_condition(uint8_t id,unsigned* camx,unsigned* camy){
    /* Directly lowered RunEventsJmpTbl conditions whose matching event executor
     * is implemented by this runtime.  Keep this centralized so maps share the
     * source condition semantics rather than acquiring room-specific checks. */
    if(id==0x03u){ /* RunEvent_FindingAlys */
        if(ps4_event_flag_test(8u)||chaz.y<0xF0||chaz.x!=0x260)return 0;
        return field_dispatch_event_index(0x03u,camx,camy);
    }
    if(id==0x0Au){ /* RunEvent_SuspicionOnPrincipal */
        if(!ps4_event_flag_test(9u)||ps4_event_flag_test(0x0Eu))return 0;return field_dispatch_event_index(0x0Fu,camx,camy);
    }
    if(id==0x0Du){ /* RunEvent_RidingElevator: selected-plane chunk below lead == $53. */
        if(field_collision_plane_layout_chunk_world((unsigned)chaz.x,(unsigned)(chaz.y+16))!=0x53u)return 0;return field_dispatch_event_index(0x14u,camx,camy);
    }
    if(id==0x0Eu){ /* RunEvent_VahFortMovingPlatform */
        if((ram8r(0xEC81u)&15u)==2u)return 0;
        unsigned y1=ps4_flag_test_type(2u,0x09u)?0x02B0u:0x0210u;if(field_rect_hit((unsigned)chaz.x,(unsigned)chaz.y,0x02C0u,y1,0x40u,0x20u))return field_dispatch_event_index(0x15u,camx,camy);
        unsigned y2=ps4_flag_test_type(2u,0x0Au)?0x02F0u:0x0390u;if(field_rect_hit((unsigned)chaz.x,(unsigned)chaz.y,0x02C0u,y2,0x40u,0x20u))return field_dispatch_event_index(0x16u,camx,camy);return 0;
    }
    if(id==0x0Fu){ /* RunEvent_WpnPlntMovingPlatform */
        if((ram8r(0xEC81u)&15u)==2u)return 0;
        static const unsigned rx[4]={0x01C0u,0x0220u,0x0380u,0x03E0u};static const unsigned lo[4]={0x0290u,0x0150u,0x0150u,0x0290u};static const unsigned hi[4]={0x0370u,0x0230u,0x0230u,0x0370u};static const uint8_t tf[4]={0x0Du,0x0Eu,0x0Fu,0x10u};
        for(unsigned q=0;q<4u;q++){unsigned ry=ps4_flag_test_type(2u,tf[q])?hi[q]:lo[q];if(field_rect_hit((unsigned)chaz.x,(unsigned)chaz.y,rx[q],ry,0x40u,0x20u))return field_dispatch_event_index(0x17u+q,camx,camy);}return 0;
    }
    if(id==0x10u){ /* RunEvent_VahFortConveyorBelt */
        unsigned x=(unsigned)chaz.x,y=(unsigned)chaz.y;
        if(field_rect_hit(x,y,0x0180u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x0180u,0x0290u,0x20u,0x20u))return field_dispatch_event_index(0x1Du,camx,camy);
        if(field_rect_hit(x,y,0x0440u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x0440u,0x0290u,0x20u,0x20u))return field_dispatch_event_index(0x1Eu,camx,camy);
        if(field_rect_hit(x,y,0x0180u,0x0310u,0x20u,0x20u)||field_rect_hit(x,y,0x0180u,0x0390u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x0Cu)?0x1Eu:0x1Du,camx,camy);
        if(field_rect_hit(x,y,0x0440u,0x0310u,0x20u,0x20u)||field_rect_hit(x,y,0x0440u,0x0390u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x0Bu)?0x1Eu:0x1Du,camx,camy);
        if(field_rect_hit(x,y,0x01C0u,0x03D0u,0x20u,0x20u)||field_rect_hit(x,y,0x0220u,0x03D0u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x0Cu)?0x20u:0x1Fu,camx,camy);
        if(field_rect_hit(x,y,0x03A0u,0x03D0u,0x20u,0x20u)||field_rect_hit(x,y,0x0400u,0x03D0u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x0Bu)?0x20u:0x1Fu,camx,camy);return 0;
    }
    if(id==0x11u){ /* RunEvent_WeaponPlantConveyorBelt ($5671E). */
        unsigned x=(unsigned)chaz.x,y=(unsigned)chaz.y;
        if(field_rect_hit(x,y,0x0120u,0x0130u,0x20u,0x20u)||field_rect_hit(x,y,0x0120u,0x01D0u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x11u)?0x1Eu:0x1Du,camx,camy);
        if(field_rect_hit(x,y,0x04A0u,0x0130u,0x20u,0x20u)||field_rect_hit(x,y,0x04A0u,0x01D0u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x12u)?0x1Eu:0x1Du,camx,camy);
        if(field_rect_hit(x,y,0x0160u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x01C0u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x0320u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x0380u,0x0210u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x11u)?0x20u:0x1Fu,camx,camy);
        if(field_rect_hit(x,y,0x0240u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x02A0u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x0400u,0x0210u,0x20u,0x20u)||field_rect_hit(x,y,0x0460u,0x0210u,0x20u,0x20u))return field_dispatch_event_index(ps4_flag_test_type(2u,0x12u)?0x1Fu:0x20u,camx,camy);
        return 0;
    }
    if(id==0x12u){ /* RunEvent_Recovery: trigger only on entry to collision 2. */
        if((ram8r(0xEC80u)&15u)!=2u||(ram8r(0xEC81u)&15u)==2u)return 0;return field_dispatch_event_index(0x21u,camx,camy);
    }
    if(id==0x13u){ /* RunEvent_AfterIgglanova: Igglanova defeated, follow-up not yet committed. */
        if(ps4_event_flag_test(0x0Fu)||!ps4_event_flag_test(0x0Bu))return 0;
        return field_dispatch_event_index(0x25u,camx,camy);
    }
    if(id==0x18u){ /* RunEvent_AlshlineFound: story flag clear and ChestFlag $08 present. */
        if(ps4_event_flag_test(0x32u)||!ps4_flag_test_type(1u,0x08u))return 0;
        return field_dispatch_event_index(0x28u,camx,camy);
    }
    if(id==0x55u){ /* RunEvent_CancellerReminder.  Source checks EventFlags $70/$73,
                     * ChestFlag $0B and the authored 16px trigger strip. */
        if(!ps4_event_flag_test(0x70u)||ps4_event_flag_test(0x73u)||ps4_flag_test_type(1u,0x0Bu))return 0;
        if((unsigned)chaz.y!=0x0310u||(unsigned)chaz.x<0x01E0u||(unsigned)chaz.x>0x01F0u)return 0;
        return field_dispatch_event_index(0x2Au,camx,camy);
    }
    if(id==0x7Cu){ /* RunEvent_PiataChazAlone */
        if(ps4_event_flag_test(0x15u))return 0;field_dialogue_show(0x6Du,*camx,*camy);ps4_event_flag_set(0x15u);return 1;
    }
    {int cp25=field_run_cp25_event_condition(id,camx,camy);if(cp25>=0)return cp25;}
    return field_run_bound_event_condition(id,camx,camy);
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int field_run_map_events(unsigned* camx,unsigned* camy){
    if(field_runtime_plan_valid){for(unsigned i=0u;i<field_event_plan_count;i++)if(field_run_map_event_condition(field_event_plan_ids[i],camx,camy))return 1;return 0;}
    uint32_t p=field_map.events;for(unsigned guard=0;guard<128u;guard++){uint8_t id=rom8(p++);if(id==0xFFu)break;if(field_run_map_event_condition(id,camx,camy))return 1;}return 0;
}

static void ps4_apply_game_start_field_handoff(void){
    lead_char_id=0u;ram8w(0xF40Au,0u);for(unsigned i=1u;i<5u;i++)ram8w((uint16_t)(0xF40Au+i),0xFFu);ram16w(0xEC28u,0x0013u);ram16w(0xEC2Au,0xFFFFu);ram16w(0xEC48u,0x0060u);ram16w(0xEC4Au,0x0024u);ram16w(0xEC44u,0u);ram16w(0xEC46u,0u);ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&(uint8_t)~0x08u));ram8w(0x500Au,0x84u);ram8w(0xECECu,0x84u);ram8w(0xF100u,(uint8_t)(ram8r(0xF100u)|0x01u));ps4_audio_play(0x84u);
}
/* PSCSTYLE v0.3: PSC-like native field continuation.  Keep the cold map-load,
 * resume and transition setup in ROM, but run the steady-state field frame
 * state machine from IWRAM.  PSC's PSII/PSIII ports similarly keep hot native
 * module continuations separate from cold outer dispatch/state setup. */
#include "r30_field_status.h"
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static int field_runtime_active(uint16_t* map_id_p,uint16_t* start_x_p,uint16_t* start_y_p,uint8_t* facing_p,unsigned* camx_p,unsigned* camy_p){
    uint16_t map_id=*map_id_p;unsigned camx=*camx_p,camy=*camy_p;uint8_t prev_pad=0u;
    for(;;){
        waitvb();if(ps4_return_to_title)return 0;r30_field_status_restore();r30_tile_animation_update();field_run_map_updates();
        if(field_event_reload_pending){
            field_event_reload_pending=0u;*map_id_p=ram16r(0xEC28u);*start_x_p=(uint16_t)(ram16r(0xEC48u)<<3);*start_y_p=(uint16_t)(ram16r(0xEC4Au)<<3);*facing_p=(uint8_t)((ram16r(0xEC44u)>>2)&3u);return 1;
        }
        struct FieldTransition tr;
        if(!chaz.step_left){field_update_standing_collision();r30_field_status_update(camx,camy);if(ps4_return_to_title)return 0;if(field_run_map_events(&camx,&camy)){prev_pad=0u;continue;}if(field_find_transition(&tr)){ram16w(0xEC2Au,map_id);*map_id_p=tr.map_id;*start_x_p=tr.x;*start_y_p=tr.y;*facing_p=tr.facing;ram16w(0xEC46u,(uint16_t)tr.align);return 1;}{int ran=field_random_battle_check();if(ps4_return_to_title)return 0;if(ran){choose_camera(&camx,&camy);hw_field_init(camx,camy);prev_pad=0u;continue;}}}
        if(ps4_return_to_title)return 0;
        uint8_t now=md_io_read_psiv_pad();
        if(!chaz.step_left&&(now&0x80u)&&!(prev_pad&0x80u)){ps4_field_system(camx,camy);choose_camera(&camx,&camy);hw_field_init(camx,camy);prev_pad=0u;continue;}
        if(!chaz.step_left&&(now&0x40u)&&!(prev_pad&0x40u)){ps4_field_menu(camx,camy);choose_camera(&camx,&camy);hw_field_init(camx,camy);if(ps4_menu_talk_pending){ps4_menu_talk_pending=0u;ps4_field_talk(camx,camy);}prev_pad=0u;continue;}
        if(!chaz.step_left&&(now&0x20u)&&!(prev_pad&0x20u)){int area=field_interact_area(&camx,&camy);if(area==2){int reload=ps4_load_field_map(map_id);if(reload)fail((unsigned)reload);choose_camera(&camx,&camy);hw_field_init(camx,camy);prev_pad=0u;continue;}if(area||field_interact_tile(&camx,&camy)||field_interact_special_shop(&camx,&camy)||field_interact_object(&camx,&camy)){prev_pad=0u;continue;}}
        begin_step(now);advance_step();prev_pad=now;unsigned nx,ny;choose_camera(&nx,&ny);field_update_npcs(nx,ny);hw_set_scroll(nx,ny);hw_update_objects(nx,ny);camx=nx;camy=ny;
    }
}
static void field_runtime_loop(uint16_t map_id,uint16_t start_x,uint16_t start_y,uint8_t facing){
    for(;;){
        /* Character_1 is always the first current-party slot when a field is
         * rebuilt.  Source events such as $6A restore F40A..F40E immediately
         * before RefreshMap, so derive the native lead at every map-load seam
         * instead of only on SRAM Continue.  This also keeps ORDER changes
         * coherent on the next ordinary transition. */
        lead_char_id=ram8r(0xF40Au);if(lead_char_id>10u)lead_char_id=0u;
        /* Source MapDataManagers run after Character_1 already contains the
         * incoming map-start position.  Initialize the native/source lead before
         * ps4_load_field_map() so position-sensitive managers ($3D/$3E and any
         * future lowered equivalents) never see the previous map's C030/C034. */
        chaz.x=start_x;chaz.y=start_y;chaz.facing=facing;chaz.anim=0u;chaz.anim_clock=0u;chaz.dx=chaz.dy=0;chaz.step_left=0u;field_sync_lead_to_source();
        REG_DISPCNT=FORCE_BLANK;ram16w(0xEC28u,map_id);ram16w(0xEC48u,(uint16_t)(start_x>>3));ram16w(0xEC4Au,(uint16_t)(start_y>>3));ram16w(0xEC44u,(uint16_t)(facing<<2));
        int rc=ps4_load_field_map(map_id);if(rc)fail((unsigned)rc);if(!(field_custom_resume_event==0x9Fu&&field_custom_resume_phase==3u)&&field_map.music>=0x81u&&field_map.music<=0xB4u)ps4_audio_play(ram16r(0xF43Cu)?0x8Du:field_map.music);if(map_id==0x0013u&&piata_f1_state_hash()!=0x5386A284u)fail(73u);
        if(ps4_debug_autoplace_pending){uint16_t ax,ay;if(ps4_debug_find_safe_spawn(&ax,&ay)){chaz.x=ax;chaz.y=ay;start_x=ax;start_y=ay;ram16w(0xEC48u,(uint16_t)(ax>>3));ram16w(0xEC4Au,(uint16_t)(ay>>3));}ps4_debug_autoplace_pending=0u;field_sync_lead_to_source();}
        ram8w(0xEC80u,1u);ram8w(0xEC81u,1u);field_random_battle_delay=10u;field_step_completed=0u;field_idle_anim_clock=0u;field_event_reload_pending=0u;
        field_followers_sync_aligned(ram16r(0xEC46u));if(ram16r(0xF43Cu))field_follower_count=0u;field_apply_pending_party_position();field_sync_native_party_to_source();
        unsigned camx,camy;choose_camera(&camx,&camy);hw_field_init(camx,camy);uint8_t prev_pad=0u;
        if(ps4_debug_event_pending){unsigned ev=ps4_debug_event_pending-1u;ps4_debug_event_pending=0u;if(!field_dispatch_event_index(ev,&camx,&camy))ps4_field_popup_text("EVENT NOT IMPLEMENTED",0);if(field_event_reload_pending){map_id=ram16r(0xEC28u);start_x=(uint16_t)(ram16r(0xEC48u)<<3);start_y=(uint16_t)(ram16r(0xEC4Au)<<3);facing=(uint8_t)((ram16r(0xEC44u)>>2)&3u);continue;}}
        if(field_custom_resume_event!=0xFFu){
            if(!field_resume_custom_event(&camx,&camy))fail(98u);
            if(field_event_reload_pending){field_event_reload_pending=0u;map_id=ram16r(0xEC28u);start_x=(uint16_t)(ram16r(0xEC48u)<<3);start_y=(uint16_t)(ram16r(0xEC4Au)<<3);facing=(uint8_t)((ram16r(0xEC44u)>>2)&3u);continue;}
        }
        if(field_cutscene_resume_index!=0xFFu){
            if(!field_resume_generated_cutscene(&camx,&camy))fail(96u);
            if(field_event_reload_pending){field_event_reload_pending=0u;map_id=ram16r(0xEC28u);start_x=(uint16_t)(ram16r(0xEC48u)<<3);start_y=(uint16_t)(ram16r(0xEC4Au)<<3);facing=(uint8_t)((ram16r(0xEC44u)>>2)&3u);continue;}
        }
        if(field_transition_resume_index!=0xFFu){
            if(!field_resume_transition_event_script(&camx,&camy))fail(97u);
            if(field_event_reload_pending){field_event_reload_pending=0u;map_id=ram16r(0xEC28u);start_x=(uint16_t)(ram16r(0xEC48u)<<3);start_y=(uint16_t)(ram16r(0xEC4Au)<<3);facing=(uint8_t)((ram16r(0xEC44u)>>2)&3u);continue;}
        }
        (void)prev_pad; /* active continuation owns its edge-trigger pad state */
        if(!field_runtime_active(&map_id,&start_x,&start_y,&facing,&camx,&camy))return;
        continue;
    }
}
static int opening_show_source_dialogue_36(void){
    /* Event_GameStart starts with InitVRAMAndCRAM + Pal_FadeIn, then loads
     * DialogueTree17 and executes entry $36.  fade_framebuffer_to_black() leaves
     * GBA BLDY=16, so explicitly finish that source fade-in lifecycle before
     * presenting the source dialogue.  v0.9.8.7 drew a port-authored Mode-4
     * message while BLDY was still 16 and then waited for input: an invisible,
     * apparently endless black screen. */
    REG_BLDY=0u;REG_BLDCNT=0u;for(unsigned i=0u;i<128u;i++)hw_hide_obj(i);
    BG_PALETTE[0u]=0u;REG_DISPCNT=(uint16_t)(MODE0|OBJ_1D|OBJ_ENABLE);
    unsigned n=kos_decode(0x1EBA90u,field_dialogue,FIELD_DIALOGUE_CAP);if(!n)return 0;field_dialogue_len=n;
    field_dialogue_saved_cursor=0;dialogue_portrait_id=0u;dialogue_choice_visible=0u;field_dialogue_run_resolved(field_dialogue_resolve(0x36u),0u,0u);
    /* PalFadeOut_ClrSpriteTbl before RefreshMap.  The native fade targets OBJ as
     * well, so the window disappears over the source 14-update envelope. */
    field_palette_fade_out();return 1;
}
static void opening_handoff_loop(void){
    if(!opening_show_source_dialogue_36())fail(98u);
    ram8w(0xECE0u,0u);ram8w(0x500Au,0x84u);ram8w(0xECECu,0x84u);ps4_audio_play(0x84u);
    field_custom_resume_event=0x9Fu;field_custom_resume_phase=0u;field_runtime_loop(0x005Eu,(uint16_t)(0x44u*8u),(uint16_t)(0x38u*8u),0u);
}
static void saved_game_handoff_loop(void){
    uint16_t map=ram16r(0xF404u),prev=ram16r(0xF402u),x=ram16r(0xF406u),y=ram16r(0xF408u);
    ram16w(0xEC28u,map);ram16w(0xEC2Au,prev);lead_char_id=ram8r(0xF40Au);if(lead_char_id>10u)lead_char_id=0u;
    /* Source load restores Field_Map_Index/Field_Map_Index_2 then enters the
     * normal field-load mode. Facing is not part of the SRAM save block; the
     * field object is rebuilt by the map loader. */
    field_runtime_loop(map,x,y,0u);
}

static const uint16_t press_cycle[32]={0x04EE,0x04EE,0x04EE,0x04EE,0x04EE,0x04EE,0x04EE,0x04EE,0x04CE,0x04AE,0x048E,0x046E,0x044E,0x042E,0x040E,0x020C,0x000A,0x000A,0x000A,0x000A,0x000A,0x000A,0x000A,0x000A,0x020C,0x040E,0x042E,0x044E,0x046E,0x048E,0x04AE,0x04CE};
static const uint8_t cvt[8]={0,4,9,13,18,22,27,31};
static const uint16_t src_x[240]={0,1,2,4,5,6,8,9,10,12,13,14,16,17,18,20,21,22,24,25,26,28,29,30,32,33,34,36,37,38,40,41,42,44,45,46,48,49,50,52,53,54,56,57,58,60,61,62,64,65,66,68,69,70,72,73,74,76,77,78,80,81,82,84,85,86,88,89,90,92,93,94,96,97,98,100,101,102,104,105,106,108,109,110,112,113,114,116,117,118,120,121,122,124,125,126,128,129,130,132,133,134,136,137,138,140,141,142,144,145,146,148,149,150,152,153,154,156,157,158,160,161,162,164,165,166,168,169,170,172,173,174,176,177,178,180,181,182,184,185,186,188,189,190,192,193,194,196,197,198,200,201,202,204,205,206,208,209,210,212,213,214,216,217,218,220,221,222,224,225,226,228,229,230,232,233,234,236,237,238,240,241,242,244,245,246,248,249,250,252,253,254,256,257,258,260,261,262,264,265,266,268,269,270,272,273,274,276,277,278,280,281,282,284,285,286,288,289,290,292,293,294,296,297,298,300,301,302,304,305,306,308,309,310,312,313,314,316,317,318};
static const uint16_t src_y[160]={0,1,2,4,5,7,8,9,11,12,14,15,16,18,19,21,22,23,25,26,28,29,30,32,33,35,36,37,39,40,42,43,44,46,47,49,50,51,53,54,56,57,58,60,61,63,64,65,67,68,70,71,72,74,75,77,78,79,81,82,84,85,86,88,89,91,92,93,95,96,98,99,100,102,103,105,106,107,109,110,112,113,114,116,117,119,120,121,123,124,126,127,128,130,131,133,134,135,137,138,140,141,142,144,145,147,148,149,151,152,154,155,156,158,159,161,162,163,165,166,168,169,170,172,173,175,176,177,179,180,182,183,184,186,187,189,190,191,193,194,196,197,198,200,201,203,204,205,207,208,210,211,212,214,215,217,218,219,221,222};
static inline uint16_t md2gba(uint16_t c){ unsigned r=(c>>1)&7,g=(c>>5)&7,b=(c>>9)&7; return (uint16_t)(cvt[r]|(cvt[g]<<5)|(cvt[b]<<10)); }
static inline uint16_t pal(unsigned p,unsigned i,uint16_t press){ unsigned idx=p*16u+i; return md2gba(idx==37u?press:cram[idx]); }
static inline unsigned tile_px(uint16_t e,unsigned x,unsigned y){ if(e&0x800)x=7-x;if(e&0x1000)y=7-y;unsigned t=e&0x7ff,o=(t*32u+y*4u+(x>>1))&0xffffu,v=md_vram[o];return (x&1)?v&15u:v>>4; }
static inline uint8_t tile_entry_index(uint16_t e,unsigned x,unsigned y){ unsigned i=tile_px(e,x,y); return i?(uint8_t)((((e>>13)&3u)<<4)|i):0u; }
static uint8_t sample_idx(unsigned sx,unsigned sy){
    unsigned tx=(sx>>3)&63u,ty=(sy>>3)&31u,x=sx&7u,y=sy&7u,mi=ty*64u+tx;
    uint16_t a=plane_a[mi],b=plane_b[mi]; unsigned ia=tile_px(a,x,y),ib=tile_px(b,x,y);
    unsigned pa=(a>>13)&3u,pb=(b>>13)&3u,pra=a>>15,prb=b>>15;
    if(ia&&(pra||!ib||!prb)) return (uint8_t)(pa*16u+ia);
    if(ib) return (uint8_t)(pb*16u+ib);
    if(ia) return (uint8_t)(pa*16u+ia);
    return 0u;
}
static inline unsigned sxmap(unsigned x){ return src_x[x]; }
static inline unsigned symap(unsigned y){ return src_y[y]; }
static uint8_t sample_plane_b_idx(unsigned sx,unsigned sy);
static void title_load_palette(void){
    dma_fill32((void*)BG_PALETTE,512u,0u);
    for(unsigned i=0;i<64u;i++) BG_PALETTE[i]=md2gba(cram[i]);
}
static void title_present_full(void){ dma_copy32(title_fb,(void*)MODE4_VRAM16,240u*160u); }
static void title_present_band(unsigned y0,unsigned y1){if(y0>160u)y0=160u;if(y1>160u)y1=160u;if(y1<=y0)return;unsigned off=y0*240u;dma_copy32(title_fb+off,(void*)((uintptr_t)MODE4_VRAM16+off),(y1-y0)*240u);}
static void render(uint16_t press){
    (void)press;
    for(unsigned y=0;y<160u;y++) for(unsigned x=0;x<240u;x++) title_fb[y*240u+x]=sample_idx(sxmap(x),symap(y));
}

static void render_title_intro_bg(unsigned text_counter){
    ps4_audio_prepare_frame();ps4_audio_prepare_guard_frame();
    for(unsigned y=0u;y<160u;y++){
        unsigned logical=symap(y)+32u+text_counter;
        const uint8_t* src=ps4_title_intro_rows+(logical-PS4_TITLE_INTRO_FIRST_Y)*PS4_TITLE_INTRO_ROW_WIDTH;
        dma_copy32(src,title_fb+y*240u,PS4_TITLE_INTRO_ROW_WIDTH);
    }
}
static uint32_t title_intro_text_ptr(unsigned id){uint32_t p=0x2AAB9Cu;while(id--){while(p<0x300000u&&rom8(p)!=0xFFu)++p;if(p<0x300000u)++p;}return p;}
static void title_intro_draw_line(unsigned id,unsigned line){
    uint32_t p=title_intro_text_ptr(id);unsigned x=54u,y=line?85u:62u;uint8_t color=line?47u:31u;
    while(p<0x300000u&&rom8(p)!=0xFFu&&x<232u){uint8_t ch=rom8(p++);if(ch>=0xF0u)continue;uint32_t art=0x2A3542u+(uint32_t)ch*16u;
        for(unsigned dy=0u;dy<12u&&y+dy<160u;dy++){unsigned yy=(dy*4u)/3u;uint8_t bits=rom8(art+yy);for(unsigned dx=0u;dx<6u&&x+dx<240u;dx++){unsigned xx=(dx*4u)/3u;if(bits&(uint8_t)(0x80u>>xx))title_fb[(y+dy)*240u+x+dx]=color;}}x+=6u;
    }
}
static void title_intro_present(unsigned counter,int line1,int line2){render_title_intro_bg(counter);if(line1>=0)title_intro_draw_line((unsigned)line1,0u);if(line2>=0)title_intro_draw_line((unsigned)line2,1u);waitvb();title_present_full();}
static int title_run_fading_text_intro(void){
    /* TitleRoutine_FadingText: retain the source ring-buffer background,
     * $2280 16.16 scroll increment, Title_FadingTextData schedule and the two
     * $222 palette ramps.  Start skips directly to the final title as on MD. */
    static const uint8_t ev[][3]={{0x18,0,0},{0x19,2,0},{0x20,1,1},{0x21,3,0},{0x30,4,0},{0x38,5,0},{0x48,0,2},{0x49,2,0},{0x50,1,3},{0x51,3,0},{0x60,4,0},{0x68,5,0},{0x78,0,4},{0x79,2,0},{0x80,1,5},{0x81,3,0},{0x90,4,0},{0x98,5,0},{0xA8,0,6},{0xA9,2,0},{0xB0,1,7},{0xB1,3,0},{0xC8,4,0},{0xD0,5,0}};
    uint16_t* bottom=(uint16_t*)(void*)(work_ram+0x4000u);dma_fill32(plane_a,4096u,0u);dma_fill32(plane_b,4096u,0u);
    unsigned n=enigma_decode(0x1D2172u,0x60E3u,eni_tmp,2048u);if(n!=8u*28u)return 41;plane_map(eni_tmp,0x920Cu,8u,28u);
    n=enigma_decode(0x1D223Eu,0x60E3u,bottom,224u);if(n!=8u*28u)return 42;plane_map(bottom,0x900Cu,8u,1u);
    cram[31u]=0u;cram[47u]=0u;title_load_palette();title_intro_present(0u,-1,-1);REG_BLDCNT=(uint16_t)((3u<<6)|BLD_TGT_BG2);REG_BLDY=16u;REG_DISPCNT=(uint16_t)(MODE4|BG2_ENABLE);
    for(unsigned q=14u;q>0u;q--){REG_BLDY=(uint16_t)(((q-1u)*16u+13u)/14u);waitvb();}REG_BLDY=0u;REG_BLDCNT=0u;
    unsigned counter=0u,frac=0u,epi=0u;uint8_t fade=0u;uint16_t t1=0u,t2=0u;int line1=-1,line2=-1;uint8_t held=md_io_read_psiv_pad();unsigned frame=0u;
    while(counter<0xE0u){
        waitvb();++frame;uint8_t now=md_io_read_psiv_pad(),pressed=(uint8_t)(now&(uint8_t)~held);held=now;if(pressed&0x80u)break;
        unsigned old=counter;frac+=0x2280u;if(frac>=0x10000u){frac-=0x10000u;++counter;}
        if(counter!=old&&counter<0xE0u&&(counter&7u)==0u){unsigned row=counter>>3;if(row<28u)plane_map(bottom+row*8u,(uint16_t)(0x900Cu+(row<<7)),8u,1u);}
        while(epi<sizeof(ev)/sizeof(ev[0])&&ev[epi][0]==counter){unsigned a=ev[epi][1],v=ev[epi][2];if(a==0u)line1=(int)v;else if(a==1u)line2=(int)v;else fade|=(uint8_t)(1u<<a);++epi;}
        int tone_changed=0;if((frame&7u)==0u){
            if(fade&4u){if(t1<=0x0CCCu){t1=(uint16_t)(t1+0x0222u);tone_changed=1;}else fade&=(uint8_t)~4u;}
            if(fade&8u){if(t2<=0x0CCCu){t2=(uint16_t)(t2+0x0222u);tone_changed=1;}else fade&=(uint8_t)~8u;}
            if(fade&16u){if(t1){t1=(uint16_t)(t1-0x0222u);tone_changed=1;}else{fade&=(uint8_t)~16u;line1=-1;}}
            if(fade&32u){if(t2){t2=(uint16_t)(t2-0x0222u);tone_changed=1;}else{fade&=(uint8_t)~32u;line2=-1;}}
            if(tone_changed){BG_PALETTE[31u]=md2gba(t1);BG_PALETTE[47u]=md2gba(t2);}
        }
        if(counter!=old||tone_changed)title_intro_present(counter,line1,line2);
    }
    cram[31u]=0x0EEEu;cram[47u]=0x0EEEu;return build_original_title_final_planes();
}

/* Event_GameStart's AW 2284 interlude uses the original 320x224 Motavia
 * background and DialogueTree17 entries $3A-$41.  Render the MD background
 * through the same whole-frame 320x224 -> 240x160 adapter used by the title,
 * then scale the source 8x16 font by the same 3/4 x 5/7 geometry. */
static void game_start_draw_scaled_source_glyph(unsigned sx,unsigned sy,uint8_t ch,uint8_t color){
    uint32_t art=0x2A3542u+(uint32_t)(ch&0x7Fu)*16u;unsigned dx0=(sx*3u)/4u,dy0=(sy*5u)/7u;
    for(unsigned dy=0u;dy<12u;dy++){unsigned yy=(dy*16u)/12u;uint8_t bits=rom8(art+yy);unsigned py=dy0+dy;if(py>=160u)continue;
        for(unsigned dx=0u;dx<6u;dx++){unsigned xx=(dx*8u)/6u,xe=((dx+1u)*8u)/6u,px=dx0+dx;uint8_t mask=0u;for(unsigned k=xx;k<xe;k++)mask|=(uint8_t)(0x80u>>k);if(px<240u&&(bits&mask))field_pause_fb[py*240u+px]=color;}}
}
static void game_start_draw_text_id(unsigned id,unsigned source_y){
    const uint8_t* p=field_dialogue_entry(id);const uint8_t* end=field_dialogue+field_dialogue_len;unsigned sx=40u;
    while(p<end&&*p!=0xFFu){uint8_t q=*p++;if(q<0xF0u){game_start_draw_scaled_source_glyph(sx,source_y,q,0xF0u);sx+=8u;}}
}
static void game_start_present_world_frame(void){waitvb();dma_copy32(field_pause_pal,(void*)BG_PALETTE,512u);dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,240u*160u);REG_BLDCNT=0u;REG_BLDY=0u;REG_BG2CNT=3u;REG_DISPCNT=(uint16_t)(MODE4|BG2_ENABLE);}
static void game_start_text_tone(uint16_t genesis_color){BG_PALETTE[0xF0u]=md2gba(genesis_color);}
static void game_start_world_text_set(unsigned first_id){
    ps4_audio_prepare_frame();ps4_audio_prepare_guard_frame();dma_copy32((const void*)0x0600A000u,field_pause_fb,240u*160u);for(unsigned i=0u;i<4u;i++)game_start_draw_text_id(first_id+i,64u+i*24u);
    field_pause_pal[0xF0u]=md2gba(0x0666u);game_start_present_world_frame();uint16_t tone=0x0666u;
    for(unsigned f=0u;f<20u;f++){if((f&3u)==0u&&tone<0x0EEEu){tone=(uint16_t)(tone+0x0222u);if(tone>0x0EEEu)tone=0x0EEEu;}game_start_text_tone(tone);waitvb();}
    for(unsigned f=0u;f<=0x0383u;f++)waitvb();
    for(unsigned f=0u;f<20u;f++){if((f&3u)==0u&&tone>0x0666u)tone=(uint16_t)(tone-0x0222u);game_start_text_tone(tone);waitvb();}
    dma_copy32((const void*)0x0600A000u,field_pause_fb,240u*160u);game_start_present_world_frame();
}
static void game_start_world_text_sequence(void){
    REG_DISPCNT=FORCE_BLANK;dma_fill32(md_vram,65536u,0u);dma_fill32(plane_a,4096u,0u);dma_fill32(plane_b,4096u,0u);
    unsigned n=nemesis_fast(0x1CF1F2u,&work_ram[0x2000u],0x6000u);if(n){unsigned dst=0x0010u*32u;for(unsigned i=0u;i<n&&dst+i<65536u;i++)md_vram[dst+i]=work_ram[0x2000u+i];}
    n=enigma_decode(0x1D25BEu,0x2010u,eni_tmp,2048u);if(n>40u*16u)n=40u*16u;if(n)plane_map(eni_tmp,0x9280u,40u,n/40u);
    for(unsigned i=0u;i<64u;i++)cram[i]=rom16(0x1D2A3Cu+i*2u);render(0u);dma_fill32(field_pause_pal,512u,0u);for(unsigned i=0u;i<64u;i++)field_pause_pal[i]=md2gba(cram[i]);
    unsigned dn=kos_decode(0x1EBA90u,field_dialogue,FIELD_DIALOGUE_CAP);if(dn)field_dialogue_len=dn;
    dma_copy32(title_fb,(void*)0x0600A000u,240u*160u);game_start_world_text_set(0x3Au);for(unsigned f=0u;f<=0x59u;f++)waitvb();game_start_world_text_set(0x3Eu);for(unsigned f=0u;f<=0x77u;f++)waitvb();field_palette_fade_out();REG_DISPCNT=FORCE_BLANK;
}
/* The animated PRESS START color is one CRAM entry in PSIV.  In indexed Mode 4,
 * mirror that single palette entry instead of redrawing a strip of framebuffer. */
static void render_press(uint16_t press){ BG_PALETTE[37u]=md2gba(press); }
/* Only the main PSIV logo/subtitle tile range ($10-$C9) is modified by the
 * Start dissolve.  v0.7.3 correctly narrowed redraws to the logo rectangle, but
 * still re-ran the full two-plane pixel compositor ~16.9k times per stage.  On a
 * 16.78 MHz ARM7 that can stretch one source stage across multiple physical
 * VBlanks even though the source routine itself advances once per VBlank.
 *
 * Build a compact dependency cache from the fully rendered title instead.  Each
 * 16-bit token contains the exact MD VRAM byte, nibble, and palette line for an
 * A-plane pixel that can visibly change during the dissolve.  A one-byte fallback
 * stores the already-known Plane-B pixel revealed if that A pixel becomes zero.
 * Pixels that are transparent or permanently hidden by high-priority Plane B get
 * TITLE_TOKEN_NONE and cost only one compare during an update. */
static uint8_t sample_plane_b_idx(unsigned sx,unsigned sy){
    unsigned tx=(sx>>3)&63u,ty=(sy>>3)&31u,x=sx&7u,y=sy&7u;
    uint16_t b=plane_b[ty*64u+tx]; unsigned ib=tile_px(b,x,y);
    return ib?(uint8_t)((((b>>13)&3u)<<4)|ib):0u;
}
static uint16_t title_make_token(uint16_t e,unsigned x,unsigned y){
    if(e&0x0800u)x=7u-x;
    if(e&0x1000u)y=7u-y;
    unsigned off=((unsigned)(e&0x07FFu)*32u)+(y*4u)+(x>>1);
    unsigned low=x&1u;
    unsigned pal_line=(e>>13)&3u;
    return (uint16_t)((off&0x1FFFu)|(low<<13)|(pal_line<<14));
}
static void title_build_logo_fastmap(void){
    unsigned k=0u;
    for(unsigned y=TITLE_LOGO_Y0;y<=TITLE_LOGO_Y1;y++){
        unsigned sy=symap(y),ty=(sy>>3)&31u,py=sy&7u;
        for(unsigned x=TITLE_LOGO_X0;x<=TITLE_LOGO_X1;x++,k++){
            unsigned sx=sxmap(x),tx=(sx>>3)&63u,px=sx&7u;
            uint16_t a=plane_a[ty*64u+tx],b=plane_b[ty*64u+tx];
            unsigned t=a&0x07FFu,ia=tile_px(a,px,py),ib=tile_px(b,px,py);
            unsigned pra=a>>15,prb=b>>15;
            title_logo_token[k]=TITLE_TOKEN_NONE;
            title_logo_fallback[k]=ib?(uint8_t)((((b>>13)&3u)<<4)|ib):0u;
            /* Dissolve only clears bits, so an initially transparent A pixel can
             * never become visible.  Likewise a high-priority opaque B pixel
             * permanently hides a low-priority A pixel. */
            if(t>=0x0010u && t<0x00CAu && ia && (pra || !ib || !prb))
                title_logo_token[k]=title_make_token(a,px,py);
        }
    }
}
static void render_title_logo_region_fast(void){
    unsigned k=0u;
    for(unsigned y=TITLE_LOGO_Y0;y<=TITLE_LOGO_Y1;y++){
        uint8_t* dst=&title_fb[y*240u+TITLE_LOGO_X0];
        for(unsigned x=0u;x<TITLE_LOGO_W;x++,k++){
            uint16_t token=title_logo_token[k];
            if(token==TITLE_TOKEN_NONE) continue;
            uint8_t packed=md_vram[token&0x1FFFu];
            unsigned ci=(token&0x2000u)?(packed&15u):(packed>>4);
            dst[x]=ci?(uint8_t)((((token>>14)&3u)<<4)|ci):title_logo_fallback[k];
        }
    }
}
static void render_plane_b_only(const uint16_t* pal3_override){
    (void)pal3_override;
    for(unsigned y=0;y<160u;y++){ unsigned sy=symap(y);
        for(unsigned x=0;x<240u;x++) title_fb[y*240u+x]=sample_plane_b_idx(sxmap(x),sy);
    }
}
__attribute__((section(".iwram.text"),noinline,aligned(16)))
static void waitvb(void){unsigned audio_blank_at_entry=REG_VCOUNT>=160u;battle_enemy_cue_tick();ps4_audio_prepare_frame();if(battle_generic_pending_mask&&!battle_generic_stage_ready){unsigned ch=battle_generic_pending_mask;battle_generic_pending_mask=0u;(void)battle_ui_stage_generic_anim(ch);}if(audio_blank_at_entry)ps4_audio_late_vblank();while(REG_VCOUNT>=160){}while(REG_VCOUNT<160){}ps4_audio_vblank_commit();field_hscroll_present_current();battle_ui_commit_generic_anim();ram16w(0xEF1Cu,(uint16_t)(ram16r(0xEF1Cu)+1u));++native_main_frame_count;}
static void fail(unsigned code){
    uint16_t c;
    if(code==30u)c=0x001Fu;          /* state mismatch: red */
    else if(code==28u)c=0x7C1Fu;    /* SRAM unavailable/misdetected: magenta */
    else c=(uint16_t)(0x001Fu | ((code&31u)<<5));
    REG_BLDCNT=0u; REG_BLDY=0u; REG_DISPCNT=(uint16_t)(MODE3|BG2_ENABLE);
    for(unsigned i=0;i<38400u;i++)VRAM16[i]=c; for(;;){}
}

/* Original Start transition's 8-step nibble-mask dissolve, applied to the translated title art. */
static void dissolve_title(void){
    uint8_t mask=0xFEu;
    for(unsigned stage=1;stage<=8;stage++){
        uint32_t d1=0xFFFFFFFFu,d0=mask; unsigned rot=0;
        for(unsigned bit=0;bit<8;bit++){
            uint32_t carry=d0&1u; d0>>=1;
            if(!carry) d1&=0xFFFFFFF0u;
            d1=(d1<<4)|(d1>>28); d1=(d1<<8)|(d1>>24); rot++;
        }
        (void)rot;
        for(unsigned i=0;i<0x1740u;i+=4){
            uint32_t v=((uint32_t)work_ram[0x2000+i]<<24)|((uint32_t)work_ram[0x2001+i]<<16)|((uint32_t)work_ram[0x2002+i]<<8)|work_ram[0x2003+i];
            v&=d1;
            md_vram[0x0200+i]=(uint8_t)(v>>24);md_vram[0x0201+i]=(uint8_t)(v>>16);md_vram[0x0202+i]=(uint8_t)(v>>8);md_vram[0x0203+i]=(uint8_t)v;
            d1=(d1<<4)|(d1>>28); d1=(d1<<8)|(d1>>24);
        }
        mask=(uint8_t)(mask & ((uint8_t)(mask<<3)|(uint8_t)(mask>>5)));
        render_title_logo_region_fast();
        waitvb(); title_present_full();
    }
    for(unsigned i=0;i<2048;i++)plane_a[i]=0;
    render_plane_b_only(0);
    waitvb(); title_present_full();
}

void gba_main(void){
    int rc;unsigned frame;uint16_t press;uint8_t held,now,pressed;
    REG_DISPCNT=FORCE_BLANK;md_bus_reset();
    /* Dedicated free-running raster-phase source for the MD $C00008 H/V
     * counter semantics used by UpdateRNGSeed2. Audio owns timer 0 only. */
    REG_TM2CNT_H=0u;REG_TM2CNT_L=0u;REG_TM2CNT_H=0x0080u;
    ps4_audio_init();
restart_title:
    ps4_return_to_title=0u;REG_DISPCNT=FORCE_BLANK;REG_BLDCNT=0u;REG_BLDY=0u;ps4_audio_stop();ps4_audio_play(0xACu);ps4_audio_prepare_frame();
    rc=build_original_title_state(); if(rc) fail((unsigned)rc);
    rc=title_run_fading_text_intro();if(rc)fail((unsigned)rc);
    frame=0u;press=press_cycle[0];
    title_load_palette(); render(press); title_build_logo_fastmap(); title_present_full();
    REG_DISPCNT=(uint16_t)(MODE4|BG2_ENABLE);
    held=md_io_read_psiv_pad();
    for(;;){
        waitvb(); ++frame;
        now=md_io_read_psiv_pad();
        pressed=(uint8_t)(now & (uint8_t)~held);
        if((frame&3u)==0){ press=press_cycle[(frame&0x7Cu)>>2]; render_press(press); }
        if(pressed&0x80u){
            dissolve_title();
            for(unsigned i=0;i<16u;i++) menu_bg_pal3[i]=cram[48u+i];
            /* loc_42C90: source swaps palette line 3 before TitleRoutine_PickOption. */
            for(unsigned i=0;i<16u;i++) cram[48u+i]=rom16(0x296300u+i*2u);
            /* TitleRoutine_PickOption first-run path. */
            ram16w(0xEC28u,0x0011u); ram16w(0xF43Eu,0u);
            if(load_nem(0x1D2096u,0x05E4u,0x0160u,0x6000u)) fail(29u);
            if(!ps4_sram_header_present()){
                ps4_init_sram();
                if(!ps4_verify_sram_after_init()) fail(28u);
            }
            uint8_t invalid=ps4_sram_invalid_mask();
            for(;;){
                int top;
                if(invalid==7u){
                    draw_start_window();render_start_menu_clean(press_cycle[0]);waitvb();title_present_full();held=md_io_read_psiv_pad();
                    for(;;){waitvb();now=md_io_read_psiv_pad();pressed=(uint8_t)(now&(uint8_t)~held);held=now;if(pressed&0xE0u){while(md_io_read_psiv_pad()&0xF0u)waitvb();top=1;break;}}
                }else{
                    top=title_pick_three("CONTINUE","START","ERASE DATA",0u,press_cycle[0]);if(top<0)continue;
                }
                title_menu_clear_region(13u,10u,14u,7u);clear_start_window();
                if(top==0){
                    int slot=title_pick_three("SLOT 1","SLOT 2","SLOT 3",invalid,press_cycle[0]);if(slot<0)continue;
                    if(!ps4_sram_load_slot((unsigned)slot)){invalid=ps4_sram_invalid_mask();continue;}
                    fade_framebuffer_to_black();ps4_audio_stop();saved_game_handoff_loop();if(ps4_return_to_title)goto restart_title;goto restart_title;
                }
                if(top==2){
                    int slot=title_pick_three("SLOT 1","SLOT 2","SLOT 3",invalid,press_cycle[0]);if(slot<0)continue;ps4_sram_erase_slot((unsigned)slot);invalid=ps4_sram_invalid_mask();continue;
                }
                /* Title_StartOption: source new-game initialization and opening handoff. */
                fade_framebuffer_to_black();ps4_new_game_init_core();if(new_game_state_hash()!=0x5FA7F43Bu)fail(30u);
                ps4_audio_stop();opening_handoff_loop();if(ps4_return_to_title)goto restart_title;goto restart_title;
            }
        }
        held=now;
    }
}
