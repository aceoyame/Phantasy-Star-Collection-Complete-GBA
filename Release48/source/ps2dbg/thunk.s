.syntax unified
.cpu arm7tdmi
.thumb
.section .start,"ax"
.global ps2dbg_thunk
.thumb_func
/* replaces the `bl 0x0839488C` at PSII 0x08390414 (last init call before the main loop):
 * run the original function, then give the debug hook a chance. */
ps2dbg_thunk:
    push    {r4,lr}
    ldr     r4,=0x0839488D
    bl      .Lcall_r4
    bl      ps2dbg_hook
    pop     {r4}
    pop     {r1}
    bx      r1
.Lcall_r4:
    bx      r4

/* ---- PSII title-screen warp hooks (statically translated game code; see ps2warp.h) ---------------------------------
 * Both are entered with BL from inside translated code, which keeps its 68k registers in r0-r7 and r12 (r12 = 0x03004000, the
 * return-stack base).  lr is not live there (translated functions save it in their prologue), so BL is safe; we preserve
 * r0-r3 and r12 around the C calls and execute the two displaced instructions ourselves. */
.global ps2warp_thunk_a
.thumb_func
/* replaces `movs r3,#0 ; ldr r0,=0x0200000E` at 0x0839F2FC (title: START pressed -> new-game intro).
 * If a warp is armed, return into MoveToOpeningScreen (0x0839F340) instead -- the game's own "enter the Level screen" path. */
ps2warp_thunk_a:
    push    {r0-r3,lr}
    mov     r0,r12
    push    {r0}
    bl      ps2warp_check
    adds    r2,r0,#0
    pop     {r0}
    mov     r12,r0
    cmp     r2,#0
    beq     1f
    ldr     r0,=0x0839F341
    str     r0,[sp,#16]
1:  pop     {r0-r3}
    movs    r3,#0
    ldr     r0,=0x0200000E
    pop     {pc}
.ltorg

.global ps2warp_thunk_b
.thumb_func
/* replaces `ldr r3,=0x03004000 ; ldr r2,[r3,#0x3C]` at 0x0839F376 (end of MoveToOpeningScreen): write the warp state */
ps2warp_thunk_b:
    push    {r0-r3,lr}
    mov     r0,r12
    push    {r0}
    bl      ps2warp_commit
    pop     {r0}
    mov     r12,r0
    pop     {r0-r3}
    ldr     r3,=0x03004000
    ldr     r2,[r3,#0x3C]
    pop     {pc}
.ltorg
