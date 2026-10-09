.syntax unified
.cpu arm7tdmi
.thumb
.section .text.psc4_launcher_hook,"ax",%progbits
.global psc4_launcher_hook
.thumb_func

/*
 * P17 control-safe native fourth-card presentation for the stock Phantasy Star Collection
 * launcher.
 *
 * Runtime findings from P13/P14:
 *   - PSC continuously owns/reasserts PSIII's BG3 palette while the launcher
 *     sits on virtual slot IV.  P17 retains P15's control-safe ownership model. The PSIV card uses the
 *     native PSIII palette plus only entries 14/15, which are proven unused
 *     by the stock PSIII card and are patched identically in the retail
 *     palette source. No dynamic palette ownership is required.
 *   - When wrapping I -> IV, BG3 is already hidden at 0x80.  P14 swapped
 *     immediately and reused a stale +8 direction, so IV came from the bottom
 *     even though left navigation requires -8/top entry.  P15 captures the
 *     *current* live +/-8 step even on the already-hidden swap frame, but only
 *     when r9 actually contains a valid retail step.  III -> IV continues to
 *     use the previously captured +8 after stock zeros r9 at its hidden
 *     midpoint.
 *
 * Hook entry replaces stock:
 *   ldr r4,[r1]
 *   movs r7,#0x80
 * at ROM 0x080006A8.  r1 points to selected slot.  Caller sp+10 is current
 * BG3/card-3 scroll byte.  r9 is the stock signed +/-8 animation step.
 */
psc4_launcher_hook:
    mov     r0,sp
    adds    r0,#10
    ldrb    r2,[r0]
    push    {r5,r6,lr}

    ldr     r4,[r1]             /* actual selection 0..3 */

    ldr     r0,=.Lstate_addr
    ldr     r0,[r0]
    ldr     r5,[r0]
    ldr     r6,=.Lstate_magic
    ldr     r6,[r6]
    cmp     r5,r6
    beq     .Lstate_ok
    str     r6,[r0]
    movs    r5,#0               /* +4: 0=PSIII, 1=PSIV */
    strb    r5,[r0,#4]
    movs    r5,#8               /* +5: last valid retail signed step byte */
    strb    r5,[r0,#5]
.Lstate_ok:
    ldrb    r5,[r0,#4]
    cmp     r5,#0
    bne     .Lcontent_iv

/* BG3 currently contains PSIII. */
.Lcontent_iii:
    cmp     r4,#3
    bne     .Ldone

    cmp     r2,#0x80
    beq     .Lswap_to_iv

    /* Visible III -> IV hide. Save the live retail +/-8 direction. */
    bl      .Lsave_step_if_valid
    movs    r4,#3               /* no physical card selected: all -> hidden */
    b       .Ldone

.Lswap_to_iv:
    /* Important for I -> IV wrap: BG3 starts already hidden, but live r9 is
       still the new left-navigation -8.  Capture it before the swap.  On
       ordinary III -> IV hidden midpoint r9 may already be 0, in which case
       this helper deliberately leaves the previously stored +8 unchanged. */
    bl      .Lsave_step_if_valid
    bl      .Lload_iv_card
    ldr     r0,=.Lstate_addr
    ldr     r0,[r0]
    movs    r5,#1
    strb    r5,[r0,#4]
    movs    r4,#2               /* virtual IV owns physical BG3 */
    bl      .Lrestore_step
    b       .Ldone

/* BG3 currently contains PSIV. */
.Lcontent_iv:
    cmp     r4,#3
    beq     .Liv_selected
    cmp     r4,#2
    beq     .Liv_to_iii

    /* Leaving IV for I/II: stock target card follows live retail r9.  Restore
       PSIII's payload only once BG3 is completely hidden.  No palette swap is
       needed because IV and III deliberately share the exact PSIII palette. */
    cmp     r2,#0x80
    bne     .Ldone
    bl      .Lload_iii_card
    ldr     r0,=.Lstate_addr
    ldr     r0,[r0]
    movs    r5,#0
    strb    r5,[r0,#4]
    b       .Ldone

.Liv_selected:
    movs    r4,#2
    b       .Ldone

.Liv_to_iii:
    cmp     r2,#0x80
    beq     .Lswap_back_to_iii
    bl      .Lsave_step_if_valid
    movs    r4,#3
    b       .Ldone

.Lswap_back_to_iii:
    bl      .Lsave_step_if_valid
    bl      .Lload_iii_card
    ldr     r0,=.Lstate_addr
    ldr     r0,[r0]
    movs    r5,#0
    strb    r5,[r0,#4]
    movs    r4,#2
    bl      .Lrestore_step

.Ldone:
    movs    r7,#0x80
    movs    r3,#0
    pop     {r5,r6,pc}

/* Store direction only when live r9 is exactly +8 or -8.  This lets an
   already-hidden I -> IV swap capture -8 without allowing a zeroed midpoint
   frame to erase the direction saved during III -> IV hiding. */
.Lsave_step_if_valid:
    push    {r0,r1,r2,lr}
    mov     r0,r9
    movs    r2,#8
    cmp     r0,r2
    beq     .Lsave_step_store
    negs    r2,r2
    cmp     r0,r2
    bne     .Lsave_step_done
.Lsave_step_store:
    ldr     r1,=.Lstate_addr
    ldr     r1,[r1]
    strb    r0,[r1,#5]
.Lsave_step_done:
    pop     {r0,r1,r2,pc}

.Lrestore_step:
    push    {r0,r1,lr}
    ldr     r1,=.Lstate_addr
    ldr     r1,[r1]
    ldrb    r0,[r1,#5]
    cmp     r0,#8
    beq     .Lrestore_plus
    movs    r0,#8
    negs    r0,r0
    mov     r9,r0
    pop     {r0,r1,pc}
.Lrestore_plus:
    movs    r0,#8
    mov     r9,r0
    pop     {r0,r1,pc}

/* PSC's stock card loader owns tile/map/palette-cache setup.  P17's IV card palette differs only in PSIII's two unused slots 14/15; the
   retail PSIII palette source is patched to the same values, so whichever
   path refreshes BG3 palette state produces identical colors. */
.Lload_iv_card:
    push    {r4,lr}
    movs    r0,#3
    ldr     r1,=.Liv_gfx
    ldr     r1,[r1]
    ldr     r2,=.Liv_map
    ldr     r2,[r2]
    ldr     r3,=.Liv_pal
    ldr     r3,[r3]
    bl      .Lcall_stock_card_loader
    pop     {r4,pc}

.Lload_iii_card:
    push    {r4,lr}
    movs    r0,#3
    ldr     r1,=.Liii_gfx
    ldr     r1,[r1]
    ldr     r2,=.Liii_map
    ldr     r2,[r2]
    ldr     r3,=.Liii_pal
    ldr     r3,[r3]
    bl      .Lcall_stock_card_loader
    pop     {r4,pc}

/* ARM7TDMI has BX but no Thumb BLX-register. */
.Lcall_stock_card_loader:
    ldr     r4,=.Lstock_loader
    ldr     r4,[r4]
    bx      r4

.align 2
.Lstate_addr:   .word 0x0203FFF0
.Lstate_magic:  .word 0x34435350      /* "PSC4" */
.Liv_gfx:       .word 0x08042000
.Liv_map:       .word 0x08043600
.Liv_pal:       .word 0x08043B00
.Liii_gfx:      .word 0x08016580
.Liii_map:      .word 0x08017776
.Liii_pal:      .word 0x08017C26
.Lstock_loader: .word 0x0800095D
