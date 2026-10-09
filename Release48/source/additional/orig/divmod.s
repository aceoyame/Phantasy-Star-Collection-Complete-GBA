.syntax unified
.arm
.global __aeabi_uidivmod
__aeabi_uidivmod:
 push {r2,r3,r4,lr}
 mov r4,r0
 mov r3,r1
 push {r3}
 bl __aeabi_uidiv
 pop {r3}
 mul r2,r0,r3
 sub r1,r4,r2
 pop {r2,r3,r4,pc}
.global __aeabi_idivmod
__aeabi_idivmod:
 push {r2,r3,r4,lr}
 mov r4,r0
 push {r1}
 bl __aeabi_idiv
 pop {r3}
 mul r2,r0,r3
 sub r1,r4,r2
 pop {r2,r3,r4,pc}
.global __aeabi_lmul
__aeabi_lmul:
 push {r4,lr}
 mul r4,r0,r3
 mla r4,r1,r2,r4
 umull r0,r1,r0,r2
 add r1,r1,r4
 pop {r4,pc}
