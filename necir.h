/*

  necir.h

  Copyright 2014 Matthew T. Pandina. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY MATTHEW T. PANDINA "AS IS" AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHEW T. PANDINA OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

#define setInput(ddr, pin) ((ddr) &= ~(1 << (pin)))
#define setOutput(ddr, pin) ((ddr) |= (1 << (pin)))
#define setLow(port, pin) ((port) &= ~(1 << (pin)))
#define setHigh(port, pin) ((port) |= (1 << (pin)))
#define getValue(port, pin) ((port) & (1 << (pin)))
#define inputState(input, pin) ((input) & (1 << (pin)))
#define outputState(port, pin) ((port) & (1 << (pin)))
#define enablePullup(port, pin) ((port) |= (1 << (pin)))

#define NELEMS(x) (sizeof(x)/sizeof(x[0]))

#if ((NECIR_QUEUE_LENGTH <= 0) || NECIR_QUEUE_LENGTH > 256)
#error "NECIR_QUEUE_LENGTH must be between 1 and 256, powers of two preferred"
#endif // NECIR_QUEUE_LENGTH

extern const uint8_t bitLUT[8]; // avoids having to bit shift by a variable amount

extern volatile uint32_t queue[NECIR_QUEUE_LENGTH];
extern volatile uint8_t head;
extern volatile uint8_t tail;

#if (NECIR_QUEUE_LENGTH % 8 == 0)
#define NECIR_BIT_QUEUE_LENGTH (NECIR_QUEUE_LENGTH)
#else
#define NECIR_BIT_QUEUE_LENGTH (NECIR_QUEUE_LENGTH + 1)
#endif // NECIR_QUEUE_LENGTH
extern volatile uint8_t bitQueue[NECIR_BIT_QUEUE_LENGTH];

static inline uint8_t NECIR_HasEvent(void) __attribute__(( always_inline ));
static inline uint8_t NECIR_HasEvent(void) {
  return (head != tail);
}

static inline void NECIR_GetNextEvent(uint32_t *message, bool *isRepeat) __attribute__(( always_inline ));
static inline void NECIR_GetNextEvent(uint32_t *message, bool *isRepeat) {
  *message = queue[head];
  *isRepeat = bitQueue[head/8] & bitLUT[head%8];
  head = (head + 1) % NELEMS(queue);
}

/* #if (F_CPU >= 16000000) */
/* #define NECIR_CTC_TOP 15 */
/* #elif (F_CPU >= 8000000) */
/* #define NECIR_CTC_TOP 7 */
/* #elif (F_CPU >= 4000000) */
/* #define NECIR_CTC_TOP 3 */
/* #elif (F_CPU >= 2000000) */
/* #define NECIR_CTC_TOP 1 */
/* #elif (F_CPU >= 1000000) */
/* #define NECIR_CTC_TOP 0 */
/* #else */
/* #error "NECIR: F_CPU must be at least 1000000" */
/* #endif */
#define NECIR_CTC_TOP ((uint8_t)((F_CPU/1000000)-1))

void NECIR_Init(void);
