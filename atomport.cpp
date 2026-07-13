#include "TeensyAtomThreads.h"
#include <imxrt.h>
#include <EventResponder.h>
#include <setjmp.h>
#include <new>
#include <sys/reent.h>

#define IDLE_STACK_SIZE 256

void (*oldPendSV)();
uint32_t svc_tick;

extern "C" {

FLASHMEM void atomThreadTerminate(void* ret) {
  ATOM_TCB* tcb = atomCurrentContext();
  if (tcb != NULL) {
    tcb->priority = 0; // uninterruptible

    if (tcb->flags & TCB_STATE_ATTACHED) {
      tcb->return_value = ret;
      // signal that we've finished, return value is valid
      atomMutexPut(tcb->owned);
      // wait (infinitely) for a thread to join to us
      atomSemGet(tcb->detach, 0);
      // release any other threads attempting to join this one
      atomMutexDelete(tcb->owned);
      // cleanup
      atomSemDelete(tcb->detach);
    }

    _reclaim_reent(&tcb->reent);
    // mark this thread as ended and switch to a new one
    // this thread is not in any queues so it will never run again
    tcb->flags |= TCB_STATE_SUSPENDED|TCB_STATE_TERMINATED;
    atomSched(FALSE); // should never return
  }

  asm volatile("wfi");
}

extern void atomPendSV_ISR(void);
extern void atomSVC_ISR(void);

typedef struct __attribute__((packed)) {
  non_volatile_stack nv;
  /* 0x00  struct _reent* */
  /* 0x04  s16-s31 / d8-d15 */
  /* 0x44  r4-r11 */
  /* INTERRUPT CONTEXT BEGINS HERE */
  /* 0x64 */ uint32_t r0,r1,r2,r3;
  /* 0x74 */ uint32_t r12;
  /* 0x78 */ uint32_t lr;
  /* 0x7C */ uint32_t ret_addr;
  /* 0x80 */ uint32_t XPSR;
  /* 0x84 */ union {
      float s[16]; // s0-s15
      double d[8]; // d0-d7
  };
  /* 0xC4 */ uint32_t fpscr;
  /* 0xC8 */ uint32_t :32;
  /* 0xCC total size */
} ctxt_frame;

FLASHMEM void archThreadContextInit (ATOM_TCB *tcb_ptr, void *stack_top, thread_func_t entry_point, thread_param_t entry_param) {
  // set up an exception frame
  ctxt_frame* frame = (ctxt_frame*)stack_top - 1;
  memset(frame, 0, sizeof(*frame));
  memset(&tcb_ptr->reent, 0, sizeof(tcb_ptr->reent));

  frame->ret_addr = (uint32_t)entry_point;
  frame->r0 = (uint32_t)entry_param;
  frame->lr = (uint32_t)atomThreadTerminate;
  frame->XPSR = 1<<24; // EPSR.T must be set, CPU supports thumb mode only!
  frame->fpscr = SCB_FPDSCR; // default floating point controls
  frame->nv.reent = &tcb_ptr->reent;
  tcb_ptr->sp_save_ptr = &frame->r0;
  tcb_ptr->ticks = 0;

  _REENT_INIT_PTR(&tcb_ptr->reent);
}

// assume this thread is already running (e.g. entry_point is a redirect function such as longjmp)
FLASHMEM void archFirstThreadRestore(ATOM_TCB *new_tcb_ptr) {
  // first thread uses existing reent
  _reclaim_reent(&new_tcb_ptr->reent);
  new_tcb_ptr->flags = 0; // detached

  svc_tick = ARM_DWT_CYCCNT;
  jmp_buf* jmp = *(jmp_buf**)(new_tcb_ptr->sp_save_ptr);
  longjmp(*jmp, 1);
}

uint64_t archThreadTicks(ATOM_TCB *tcb_ptr) {
  ATOM_TCB *current = atomCurrentContext();

  if (current == NULL)
    return 0;

  if (tcb_ptr != current && tcb_ptr != NULL)
    return tcb_ptr->ticks;

  return current->ticks + (uint32_t)(ARM_DWT_CYCCNT - svc_tick);
}

}

class AtomSystickEventResponder : EventResponder
{
private:
  MillisTimer Timer;
public:
  AtomSystickEventResponder();

  void triggerEvent(int, void*) {
    atomIntEnter();
    atomTimerTick();
    atomIntExit(TRUE);
  }
};

FLASHMEM AtomSystickEventResponder::AtomSystickEventResponder() {
  // call attachInterrupt to make sure the correct systick ISR gets installed (and PendSV priority is reduced)
  // no callback function required since triggerEvent() is overridden
  attachInterrupt(NULL);

  // trigger event every X milliseconds to match SYSTEM_TICKS_PER_SEC (typically 10)
  Timer.beginRepeating(1000/SYSTEM_TICKS_PER_SEC, *this);
}

// switch from mainSP to processSP, allocate new stack for mainSP
FLASHMEM static void __switchStack(void) {
  static uint8_t handlerStack[2048] __attribute__((aligned(8)));
  uint32_t r;
  void *st = &handlerStack[sizeof(handlerStack)];
  asm volatile (
    "mov %0, sp\n"
    "msr PSP, %0\n"
    "isb\n"
    "mrs %0, CONTROL\n"
    "orr %0, #2\n" // SPSEL = 1
    "msr CONTROL, %0\n" // sp is now PSP
    "isb\n"
    "msr MSP, %1\n"
    : "=&r"(r)
    : "r"(st)
  );
}

extern "C" FLASHMEM void startup_middle_hook(void) {
  static ATOM_TCB main_tcb;
  // stack for idle thread
  static uint8_t idleStack[IDLE_STACK_SIZE] __attribute__((aligned(8)));

  // used to restore execution of main thread after it gets activated by the scheduler
  jmp_buf jmp;
  // archThreadContextInit needs temp space to create a context frame for the main thread
  uint32_t stk[256] __attribute__((aligned(8)));

  __switchStack();

  if (setjmp(jmp)!=0)
    return;

  if (atomOSInit(idleStack, IDLE_STACK_SIZE, FALSE) == ATOM_OK) {
    if (atomThreadCreate(&main_tcb, 127, (thread_func_t)1, &jmp, stk, sizeof(stk), FALSE) == ATOM_OK) {
      // this is a static instance but not declared as one so placement new can be invoked
      // (should be initialized before other static classes)
      alignas(AtomSystickEventResponder) static uint8_t aser[sizeof(AtomSystickEventResponder)];

      new(aser) AtomSystickEventResponder();

      _VectorsRam[11] = atomSVC_ISR;
      oldPendSV = _VectorsRam[14];
      _VectorsRam[14] = atomPendSV_ISR;
      SCB_SHPR2 = 0; // SVCall priority 0

      // this is required to keep systick running during WFI (which the idle thread uses)
#if 1
      // keep memory powered during sleep
      CCM_CGPR |= CCM_CGPR_INT_MEM_CLK_LPM;
      // keep cpu clock on in wait mode (required for systick to trigger wake-up)
      CCM_CLPCR &= ~(CCM_CLPCR_ARM_CLK_DIS_ON_LPM | CCM_CLPCR_LPM(3));
      // set SoC low power mode to wait mode
      CCM_CLPCR |= CCM_CLPCR_LPM(1);
      // ensure all config is done before executing WFI
      asm volatile("dsb");
#endif

      atomOSStart();
    }
  }
}

