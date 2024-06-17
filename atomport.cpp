#include "atom.h"
#include <imxrt.h>
#include <EventResponder.h>
#include <setjmp.h>
#include <new>
#include <sys/reent.h>

#define IDLE_STACK_SIZE 256

void (*oldPendSV)();

extern "C" {

extern void atomPendSV_ISR(void);
extern void atomSVC_ISR(void);

typedef struct {
  /* 0x00 */ uint32_t r0,r1,r2,r3;
  /* 0x10 */ uint32_t r12;
  /* 0x14 */ uint32_t lr;
  /* 0x18 */ uint32_t ret_addr;
  /* 0x1C */ uint32_t XPSR;
  /* 0x20 */ union {
      float s[16]; // s0-s15
      double d[8]; // d0-d7
  };
  /* 0x60 */ uint32_t fpscr;
  /* 0x64 */ uint32_t reserved;
  /* 0x68 total size */
} ctxt_frame;

FLASHMEM void archThreadContextInit (ATOM_TCB *tcb_ptr, void *stack_top, void (*entry_point)(uint32_t), uint32_t entry_param) {
  // set up an exception frame
  ctxt_frame* frame = (ctxt_frame*)stack_top - 1;
  memset(frame, 0, sizeof(*frame));
  memset(&tcb_ptr->priv, 0, sizeof(tcb_ptr->priv));

  frame->ret_addr = (uint32_t)entry_point;
  frame->r0 = entry_param;
  frame->lr = 0xFFFFFFFF; // invalid EXC_RETURN value, entry point must not return
  frame->XPSR = 1<<24; // EPSR.T must be set, CPU supports thumb mode only!
  tcb_ptr->sp_save_ptr = frame;

  tcb_ptr->priv.r = &tcb_ptr->priv.reent;
  _REENT_INIT_PTR(tcb_ptr->priv.r);
}

// assume this thread is already running (e.g. entry_point is a redirect function such as longjmp)
FLASHMEM void archFirstThreadRestore(ATOM_TCB *new_tcb_ptr) {
  // first thread uses existing reent
  _reclaim_reent(new_tcb_ptr->priv.r);
  new_tcb_ptr->priv.r = _impure_ptr;

  new_tcb_ptr->entry_point(new_tcb_ptr->entry_param);
}

FLASHMEM static void longjmpWrap(uint32_t arg) {
  jmp_buf* jmp = (jmp_buf*)arg;
  longjmp(*jmp, 1);
}

}

class AtomSystickEventResponder : EventResponder
{
private:
  MillisTimer Timer;
public:
  AtomSystickEventResponder() {
    // call attachInterrupt to make sure the correct systick ISR gets installed (and PendSV priority is reduced)
    // no callback function required since triggerEvent() is overridden
    attachInterrupt(NULL);

    // trigger event every X milliseconds to match SYSTEM_TICKS_PER_SEC (typically 10)
    Timer.beginRepeating(1000/SYSTEM_TICKS_PER_SEC, *this);
  }

  void triggerEvent(int, void*) {
    atomIntEnter();
    atomTimerTick();
    atomIntExit(TRUE);
  }
};

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
    if (atomThreadCreate(&main_tcb, 128, longjmpWrap, (uint32_t)&jmp, stk, sizeof(stk), FALSE) == ATOM_OK) {
      // static instance is at this scope so it gets initialized as soon as execution reaches here,
      // rather than when global objects are created
      static AtomSystickEventResponder ticker;

      _VectorsRam[11] = atomSVC_ISR;
      oldPendSV = _VectorsRam[14];
      _VectorsRam[14] = atomPendSV_ISR;
      SCB_SHPR2 = 0; // SVCall priority 0

      atomOSStart();
    }
  }
}

