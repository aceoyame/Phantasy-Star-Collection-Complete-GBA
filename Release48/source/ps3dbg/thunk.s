.syntax unified
.cpu arm7tdmi
.thumb
.section .start,"ax"
.global ps3dbg_thunk
.thumb_func
/* Last init call, 0x080802A2: execute the original routine first. */
ps3dbg_thunk:
 push {r4,lr}
 ldr r4,=0x0808212D
 bl .Lcall_r4
 bl ps3dbg_hook
 pop {r4}
 pop {r1}
 bx r1
.Lcall_r4:
 bx r4
.ltorg
.global ps3mix_thunk
.thumb_func
/* 0x0808310E originally called bx-r1 glue at 0x08082CE0. */
ps3mix_thunk:
 push {r4,lr}
 adds r4,r1,#0
 push {r0-r3}
 mov r0,r12
 push {r0,r1}
 bl ps3dbg_mute
 pop {r2,r3}
 mov r12,r2
 pop {r0-r3}
 adds r1,r4,#0
 bl .Lcall_r1
 pop {r4}
 pop {r1}
 bx r1
.Lcall_r1:
 bx r1
.ltorg
