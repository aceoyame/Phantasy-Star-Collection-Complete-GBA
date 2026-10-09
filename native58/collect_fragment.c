static ROMFN COLD_THUMB __attribute__((noinline)) u32 collect_dirty(u32 height){
  for(u32 k=0;k<8;k++) dl[k]=0;
  if(vmode_line) return 1;
  /* CP53: no guest VRAM changes means no newly dirty guest lines.
   * Previous per-page dirty bits are still consumed by mdr_render().
   * Changed VDP registers and VSRAM force the full path before this call. */
  u32 dirty_or=0;
  for(u32 w=0;w<64;w++) dirty_or |= md_vdirty[w];
  if(!dirty_or) return 0;                           /* per-column vertical scroll: no simple line mapping */
  u32 bA=((u32)(M.reg[2]&0x38)<<10), bB=((u32)(M.reg[4]&7)<<13), bW=((u32)(M.reg[3]&0x3E)<<10);
  u32 s=M.reg[16]&3u; u32 pw= s==0?32u:s==1?64u:128u; s=(M.reg[16]>>4)&3u; u32 ph= s==0?32u:s==1?64u:128u;
  u32 pwshift=(M.reg[16]&3u)==0?5u:(M.reg[16]&3u)==1?6u:7u;
  u32 wshift=(M.reg[12]&1u)?6u:5u;
  u32 psz=pw*ph*2u, ww=(M.reg[12]&1u)?64u:32u, wsz=ww*32u*2u;
  u32 sat=((u32)(M.reg[5]&0x7Fu))<<9, hs=((u32)(M.reg[13]&0x3F))<<10;
  u32 vsA=md_vsram[0]&0x3FFu, vsB=md_vsram[1]&0x3FFu, pmask=ph*8u-1u;
  u32 anyt=0, sprchg=0;
  { u32 nb=0; for(u32 w=0;w<64;w++){ u32 b=md_vdirty[w]; while(b){ b&=b-1u; nb++; } }
    if(nb>96u) return 1; }                           /* big VRAM change (scene load): a full redraw is cheaper */
  for(u32 k=0;k<64;k++) tdirty[k]=0;
  for(u32 w=0;w<64;w++){
    u32 bits=md_vdirty[w]; if(!bits) continue; md_vdirty[w]=0;
    while(bits){
      u32 b=(u32)__builtin_ctz(bits); bits&=bits-1u;
      u32 blk=w*32u+b, a=blk*32u;
      tdirty[blk>>5]|=1u<<(blk&31u); anyt=1;          /* a block is exactly one tile pattern */
      if(a-hs<height*4u&&(M.reg[11]&3u)!=0) return 1;    /* hscroll table (line/cell modes) */
      if(a-hs<4u) return 1;                          /* full-screen hscroll value */
      if(a-sat<640u) sprchg=1;
      for(u32 pl=0;pl<2;pl++){
        u32 base=pl?bB:bA, vs=pl?vsB:vsA;
        u32 offset=(a-base)&65535u;
        if(offset<psz){
          u32 e0=offset>>1, e1=e0+15u; if(e1>=pw*ph) e1=pw*ph-1u;
          if(tile_index_valid)index_name_words(base,e0,e1,pwshift,vs,pmask,height);
          for(u32 r=e0>>pwshift;r<=e1>>pwshift;r++){ s32 y=(s32)(((r*8u)-vs)&pmask); if(y>=(s32)(pmask+1u)-7) y-=(s32)(pmask+1u); mark_band(y,8,height); }
        }
      }
      u32 woffset=(a-bW)&65535u;
      if(woffset<wsz){ u32 e0=woffset>>1; u32 r0=e0>>wshift, r1=(e0+15u)>>wshift; mark_band((s32)(r0*8u),(s32)((r1-r0+1u)*8u),height); if(tile_index_valid)index_name_words(bW,e0,e0+15u,wshift,0u,255u,height); }
    }
  }
  /* CP53: the parsed sprites depend on the SAT bytes and VDP mode.
   * Mode/register changes are full-redraw cases; a VRAM block write touching
   * the non-wrapping SAT sets sprchg above. If none touched, reuse the
   * already-parsed sprite list. Wrapped SAT tables keep original behavior. */
  if(sprchg || sat > 0x10000u-640u) {
    spr_prepare();
    if(nspr!=onspr) sprchg=1; else { const u8*p=(const u8*)spr,*q=(const u8*)ospr; for(u32 i=0;i<nspr*sizeof(Spr);i++) if(p[i]!=q[i]){ sprchg=1; break; } }
    if(sprchg){ mark_sprites(ospr,onspr,height); mark_sprites(spr,nspr,height); for(u32 i=0;i<nspr;i++) ospr[i]=spr[i]; onspr=nspr; }
  }
  if(anyt){
    if(!((vsA|vsB)&7u)){
      if(!tile_index_valid||tile_index_age>=64u)build_tile_index(height);
      tile_index_age++;
      u32 bands=0;
      for(u32 w=0;w<64;w++){
        u32 bits=tdirty[w];while(bits){u32 t=w*32u+(u32)__builtin_ctz(bits);bits&=bits-1u;bands|=tile_bands[t];}
      }
      while(bands){u32 band=(u32)__builtin_ctz(bands);bands&=bands-1u;dl[band>>2]|=255u<<((band&3u)*8u);}
    }else{
      tile_index_valid=0;
    /* cells of the planes and the window that show a changed tile */
    for(u32 pl=0;pl<2;pl++){
      u32 base=pl?bB:bA, vs=pl?vsB:vsA;
      u32 c0=0,cn=pw;
      /* Only full-screen horizontal scroll has the same visible cells on
       * every line. Other modes retain the complete row scan. */
      if(!(M.reg[11]&3u)){
        u32 hsc=VWA(hs+pl*2u)&0x3FFu;
        c0=((0u-hsc)&(pw*8u-1u))>>3;
        cn=((M.reg[12]&1u)?40u:32u)+1u;if(cn>pw)cn=pw;
      }
      for(u32 r=0;r<ph;r++){
        s32 y=(s32)(((r*8u)-vs)&pmask); if(y>=(s32)(pmask+1u)-7) y-=(s32)(pmask+1u);
        if(y>=(s32)height||y<=-8) continue;
        for(u32 c=0;c<cn;c++){ u32 cc=(c0+c)&(pw-1u);u32 t=md_vram[((base+(r*pw+cc)*2u)&0xFFFFu)>>1]&0x7FFu; if(tdirty[t>>5]&(1u<<(t&31u))){ mark_band(y,8,height); break; } }
      }
    }
    for(u32 r=0;r<32u&&r*8u<height;r++)
      for(u32 c=0;c<((M.reg[12]&1u)?40u:32u);c++){ u32 t=md_vram[((bW+(r*ww+c)*2u)&0xFFFFu)>>1]&0x7FFu; if(tdirty[t>>5]&(1u<<(t&31u))){ mark_band((s32)(r*8u),8,height); break; } }
    }
    for(u32 i=0;i<nspr;i++){
      u32 n=(u32)spr[i].w*spr[i].h, t0=spr[i].attr&0x7FFu;
      for(u32 k=0;k<n;k++){ u32 t=(t0+k)&0x7FFu; if(tdirty[t>>5]&(1u<<(t&31u))){ mark_band(spr[i].top,(s32)spr[i].h*8,height); break; } }
    }
  }
  return 0;
}

static u32 last_w, last_h, last_on;
