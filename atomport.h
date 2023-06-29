/*
 * Copyright (c) 2010, Kelvin Lawson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. No personal names or organizations' names associated with the
 *    Atomthreads project may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ATOMTHREADS PROJECT AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ATOM_PORT_H
#define __ATOM_PORT_H

#include <stddef.h>
#include <stdint.h>

/* Required number of system ticks per second (normally 100 for 10ms tick) */
#define SYSTEM_TICKS_PER_SEC            100

/* Size of each stack entry / stack alignment size (e.g. 8 bits) */
#define STACK_ALIGN_SIZE                8

#define POINTER void*

typedef struct {
  // r4,r5,r6,r7,r8,r9,r10,r11
  uint32_t regs[8];
  // s16,s17,s18,s19,s20,s21,s22,s23,s24,s25,s26,s27,s28,s29,s30,s31
  float fregs[16];
} thread_private;

#define THREAD_PORT_PRIV thread_private priv;

/**
 * Critical region protection: this should disable interrupts
 * to protect OS data structures during modification. It must
 * allow nested calls, which means that interrupts should only
 * be re-enabled when the outer CRITICAL_END() is reached.
 */
#define CRITICAL_STORE  uint32_t __primask
#define CRITICAL_START()    asm volatile("mrs %0, PRIMASK\ncpsid i" : "=r"(__primask))
#define CRITICAL_END()      asm volatile("msr PRIMASK, %0" :: "r"(__primask))

/* Uncomment to enable stack-checking */
/* #define ATOM_STACK_CHECKING */

static uint32_t atomic_add(uint32_t *, uint32_t) __attribute__((unused));
static uint32_t atomic_add(uint32_t *p, uint32_t amount) {
   uint32_t r, status;
   do {
     asm volatile("ldrex %0, %1" : "=r"(r) : "m"(*p));
     r += amount;
     asm volatile("strex %0, %2, %1" : "=&r"(status), "=m"(*p) : "r"(r));
   } while (status==1);
   return r;
}


#endif /* __ATOM_PORT_H */