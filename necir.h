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
#include <avr/pgmspace.h>

#define setInput(ddr, pin) ((ddr) &= ~(1 << (pin)))
#define setOutput(ddr, pin) ((ddr) |= (1 << (pin)))
#define setLow(port, pin) ((port) &= ~(1 << (pin)))
#define setHigh(port, pin) ((port) |= (1 << (pin)))
#define getValue(port, pin) ((port) & (1 << (pin)))
#define inputState(input, pin) ((input) & (1 << (pin)))
#define outputState(port, pin) ((port) & (1 << (pin)))
#define enablePullup(port, pin) ((port) |= (1 << (pin)))

#define NELEMS(x) (sizeof(x)/sizeof(x[0]))

extern const uint8_t NECIR_oneLeftShiftedBy[8] PROGMEM; // avoids having to bit shift by a variable amount

#if (NECIR_USE_EXTENDED_PROTOCOL)
typedef uint32_t necir_message_t;
#else // NECIR_USE_EXTENDED_PROTOCOL
typedef uint16_t necir_message_t;
#endif // NECIR_USE_EXTENDED_PROTOCOL

#if ((NECIR_QUEUE_LENGTH <= 0) || NECIR_QUEUE_LENGTH > 256)
#error "NECIR_QUEUE_LENGTH must be between 1 and 256, powers of two preferred"
#endif // NECIR_QUEUE_LENGTH
extern volatile necir_message_t NECIR_messageQueue[NECIR_QUEUE_LENGTH];

#if ((NECIR_QUEUE_LENGTH) % 8 == 0)
#define NECIR_REPEAT_QUEUE_BYTES ((NECIR_QUEUE_LENGTH) / 8)
#else // NECIR_QUEUE_LENGTH
#define NECIR_REPEAT_QUEUE_BYTES ((NECIR_QUEUE_LENGTH) / 8 + 1)
#endif // NECIR_QUEUE_LENGTH
extern volatile uint8_t NECIR_repeatFlagQueue[NECIR_REPEAT_QUEUE_BYTES];

extern uint8_t NECIR_head;
extern volatile uint8_t NECIR_tail;

static inline uint8_t NECIR_QueueEmpty(void) __attribute__(( always_inline ));
static inline uint8_t NECIR_QueueEmpty(void) {
  return (NECIR_head == NECIR_tail);
}

static inline void NECIR_Dequeue(necir_message_t *message, bool *isRepeat) __attribute__(( always_inline ));
static inline void NECIR_Dequeue(necir_message_t *message, bool *isRepeat) {
  *message = NECIR_messageQueue[NECIR_head];
  *isRepeat = NECIR_repeatFlagQueue[NECIR_head / 8] & pgm_read_byte(&NECIR_oneLeftShiftedBy[NECIR_head % 8]);
  NECIR_head = (NECIR_head + 1) % NELEMS(NECIR_messageQueue);
}

void NECIR_Init(void);
