#ifndef _TEENSY_ATOM_THREADS_H
#define _TEENSY_ATOM_THREADS_H

#include "atom.h"
#include "atommutex.h"
#include "atomqueue.h"
#include "atomsem.h"
#include "atomtimer.h"

#ifdef __cplusplus

class AtomMutex {
friend class AtomCond;
private:
  ATOM_MUTEX mutex;
public:

  class AutoLock {
  private:
    AtomMutex& lock;
    unsigned int locked_count;
  public:
    uint8_t Lock(int32_t timeout) {
      uint8_t ret = lock.Get(timeout);
      if (ret == ATOM_OK) ++locked_count;
      return ret;
    }
    uint8_t Unlock() {
      uint8_t ret = ATOM_OK;
      if (locked_count) {
        ret = lock.Put();
        if (ret == ATOM_OK) --locked_count;
      }
      return ret;
    }
    operator bool() const { return locked_count != 0; }

    constexpr AutoLock(AtomMutex& m, unsigned int level=0) : lock(m),locked_count(level) {}
    AutoLock(const AutoLock& old) = delete;
    AutoLock& operator=(const AutoLock& other) = delete;
    ~AutoLock() { while (locked_count) lock.Put(), --locked_count; }
  };

  uint8_t Init() { return atomMutexCreate(&mutex); }
  uint8_t Deinit() { return atomMutexDelete(&mutex); }
  uint8_t Get(int32_t timeout=0) { return atomMutexGet(&mutex, timeout); }
  uint8_t Put() { return atomMutexPut(&mutex); }
  AutoLock Lock(int32_t timeout=0) { return AutoLock(*this, Get()==ATOM_OK ? 1:0); }

  AtomMutex() { Init(); }
  ~AtomMutex() { Deinit(); }
};

class AtomCond {
private:
  ATOM_COND cond;
public:
  uint8_t Init() { return atomCondCreate(&cond); }
  uint8_t Deinit() { return atomCondDelete(&cond); }
  uint8_t Wait(AtomMutex& m, int32_t timeout=0) { return atomCondWait(&cond, &m.mutex, timeout); }
  uint8_t Signal() { return atomCondSignal(&cond); }
  uint8_t Broadcast() { return atomCondBroadCast(&cond); }

  AtomCond() { Init(); }
  ~AtomCond() { Deinit(); }
};

template <class msg_t>
class AtomQueue {
private:
  ATOM_QUEUE queue = {};
public:
  typedef msg_t MsgType;
  uint8_t Init(msg_t* msgs, size_t msgs_total) { return atomQueueCreate(&queue, msgs, sizeof(msg_t), msgs_total / sizeof(msg_t)); }
  uint8_t Deinit() { return atomQueueDelete(&queue); }
  uint8_t Get(msg_t& msg, int32_t timeout=0) { return atomQueueGet(&queue, timeout, &msg); }
  uint8_t Put(const msg_t& msg, int32_t timeout=0) { return atomQueuePut(&queue, timeout, &msg); }

  constexpr AtomQueue() {}
  AtomQueue(msg_t* msgs, size_t msgs_total) { Init(msgs, msgs_total); }
  ~AtomQueue() { Deinit(); }
};

template<class msg_t, size_t cnt>
class TAtomQueue : public AtomQueue<msg_t> {
private:
  msg_t Msgs[cnt];
public:
  enum { MAX_MSG = cnt };
  uint8_t Init() { return AtomQueue<msg_t>::Init(Msgs, sizeof(Msgs)); }

  TAtomQueue() { Init(); }
  ~TAtomQueue() = default;
};

class AtomSem {
private:
  ATOM_SEM sema = {};
public:
  uint8_t Init(uint8_t initial_count, uint8_t max_count=255) { return atomSemCreateLimit(&sema, initial_count, max_count); }
  uint8_t Deinit() { return atomSemDelete(&sema); }
  uint8_t Get(int32_t timeout=0) { return atomSemGet(&sema, timeout); }
  uint8_t Put() { return atomSemPut(&sema); }
  uint8_t ResetCount(uint8_t count) { return atomSemResetCount(&sema, count); }

  constexpr AtomSem() {}
  AtomSem(uint8_t initial_count, uint8_t max_count=255) { Init(initial_count, max_count); }
  ~AtomSem() { Deinit(); }
};

class AtomTimer {
private:
  virtual void Callback() {}
  static void cb_func(POINTER cb_data) {
    ((AtomTimer*)cb_data)->Callback();
  }
  ATOM_TIMER timer = { .cb_func = cb_func, .cb_data = this };
public:
  uint8_t Cancel() { return atomTimerCancel(&timer); }
  uint8_t Register(uint32_t ticks) {
    Cancel();
    timer.cb_ticks = ticks;
    return atomTimerRegister(&timer);
  }
  uint8_t Register_ms(uint32_t ms) {
    return Register((ms * SYSTEM_TICKS_PER_SEC + 999) / 1000);
  }
  static uint32_t GetTicks() { return atomTimeGet(); }

  constexpr AtomTimer() {}
  virtual ~AtomTimer() { Cancel(); }
};

#endif

#endif // _TEENSY_ATOM_THREADS_H
