  .syntax unified
  .text
  .thumb
  .align 2

  .global atomPendSV_ISR
  .thumb_func
  .type atomPendSV_ISR, %function
  @ call the existing PendSV ISR first then do the same as SVC
atomPendSV_ISR:
  ldr r0, =oldPendSV
  push {r4,lr}
  ldr r4, [r0]
  cbz r4, 1f
  bl atomIntEnter
  blx r4
  mov r0, #0
  bl atomIntExit
  b 1f
  .pool
  .size atomPendSV_ISR, .-atomPendSV_ISR

  .global atomSVC_ISR
  .thumb_func
  .type atomSVR_ISR, %function
atomSVC_ISR:
  push {r4,lr}
1:
  bl atomCurrentContext
  movs r4, r0
  beq 2f    @ shouldn't happen - PendSV/SVC should not trigger inside atomIntEnter/atomIntExit
  bl atomRunSched
  @ perform context switch only if new TCB is different from current
  cmp r0, r4
  bne 3f
2:
  pop {r4,pc}
3:
  mov r1, r4
  pop {r4,lr}
  b asmContextSwitch
  .size atomPendSV_ISR, .-atomPendSV_ISR
  
  .extern _impure_ptr

  .thumb_func
  .type  asmContextSwitch, %function
asmContextSwitch:
  @ r0: ATOM_TCB *new_tcb
  @ r1: ATOM_TCB *old_tcb

  mrs r3, PSP
  ldr r2, = _impure_ptr
  str r3, [r1], #8      @ store old SP, increment to &priv
  ldr r3, [r0], #8      @ load new SP, increment to &priv
  stmia r1!, {r4-r11}   @ store old priv.regs
  ldmia r0!, {r4-r11}   @ load new priv.regs
  vstm r1, {d8-d15}     @ store old priv.fregs
  vldm r0, {d8-d15}     @ load new priv.fregs
  ldr r1, [r0,#64]      @ load new _impure_ptr
  msr PSP, r3
  str r1, [r2]
  bx lr
  .pool
  .size asmContextSwitch, .-asmContextSwitch

