  .syntax unified
  .text
  .thumb
  .align 2

  .extern _impure_ptr
  .extern oldPendSV

  .global atomSVC_ISR
  .thumb_func
  .type atomSVR_ISR, %function
atomSVC_ISR:
  push {r4,lr}
  b 1f
  .size atomSVC_ISR, .-atomSVC_ISR

  .global atomPendSV_ISR
  .thumb_func
  .type atomPendSV_ISR, %function
  @ call the existing PendSV ISR first then do SVC
atomPendSV_ISR:
  ldr r0, =oldPendSV
  push {r4,lr}
  ldr r4, [r0]
  cbz r4, 1f
  bl atomIntEnter
  blx r4
  movs r0, #0
  bl atomIntExit
1:
  bl atomCurrentContext
  movs r4, r0
  beq 2f    @ shouldn't happen - PendSV/SVC should not trigger inside atomIntEnter/atomIntExit
  bl atomRunSched
  @ perform context switch only if new TCB is different from current
  cmp r0, r4
  mov r1, r4
2:
  ite ne
  popne {r4, lr}
  popeq {r4, pc}

  @ TASK SWITCH:
  @ r0: ATOM_TCB *new_tcb
  @ r1: ATOM_TCB *old_tcb

  mrs r3, PSP           @ get old SP
  ldr r2, =_impure_ptr
  str r3, [r1]          @ save old SP
  ldr r0, [r0]          @ load new SP

  ldr r1, [r2]          @ get old reent
  vstmdb r3!, {d8-d15}  @ save old fregs
  msr PSP, r0           @ set new SP
  vldmdb r0!, {d8-d15}  @ load old fregs
  stmdb r3, {r1,r4-r11} @ save reent, old gprs
  ldmdb r0, {r0,r4-r11} @ load reent, new gprs
  str r0, [r2]          @ set new reent

  bx lr
  .pool
  .size atomPendSV_ISR, .-atomPendSV_ISR

