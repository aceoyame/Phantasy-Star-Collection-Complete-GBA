@ Integer helpers for the freestanding Genesis runtime.  ARM code; every routine is typed as a function and returns
@ with BX so it can be called from Thumb (the statically recompiled code) through the linker's interworking thunks.
.syntax unified
.arm
.global __aeabi_uidivmod
.type __aeabi_uidivmod,%function
__aeabi_uidivmod:
 push {r4,lr}
 mov r4,r0
 push {r1}
 bl __aeabi_uidiv
 pop {r3}
 mul r2,r0,r3
 sub r1,r4,r2
 pop {r4,lr}
 bx lr
.global __aeabi_idivmod
.type __aeabi_idivmod,%function
__aeabi_idivmod:
 push {r4,lr}
 mov r4,r0
 push {r1}
 bl __aeabi_idiv
 pop {r3}
 mul r2,r0,r3
 sub r1,r4,r2
 pop {r4,lr}
 bx lr
.global __aeabi_lmul
.type __aeabi_lmul,%function
__aeabi_lmul:
 push {r4,lr}
 mul r4,r0,r3
 mla r4,r1,r2,r4
 umull r0,r1,r0,r2
 add r1,r1,r4
 pop {r4,lr}
 bx lr
