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

  .global asmContextSwitch
  .thumb_func
  .type  asmContextSwitch, %function
asmContextSwitch:
  @ r0: ATOM_TCB *new_tcb
  @ r1: ATOM_TCB *old_tcb

  mrs r3, PSP
  stmia r1!, {r3-r11}
  ldmia r0!, {r3-r11}
  vstm r1, {s16-s31}
  vldm r0, {s16-s31}
  msr PSP, r3
  bx lr
  .size asmContextSwitch, .-asmContextSwitch

