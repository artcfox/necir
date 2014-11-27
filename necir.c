/*

  necir.c

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

#include <avr/interrupt.h>

#include "necir.h"

// Auto-calculate timer interval based on F_CPU
#define NECIR_CTC_TOP ((uint8_t)(F_CPU / 1000000) - 1)

// Only call this macro with a constant, otherwise it will pull in floating point math, rather than compile the entire thing down to a constant
#define samplesFromMilliseconds(ms) ((ms)/((float)(NECIR_CTC_TOP + 1) * 256 * 1000 / F_CPU))

const uint8_t NECIR_oneLeftShiftedBy[8] PROGMEM = {1, 2, 4, 8, 16, 32, 64, 128}; // avoids having to bit shift by a variable amount, always runs in constant time

#if ((NECIR_QUEUE_LENGTH <= 0) || NECIR_QUEUE_LENGTH > 256)
#error "NECIR_QUEUE_LENGTH must be between 1 and 256, powers of two strongly preferred"
#endif // NECIR_QUEUE_LENGTH
volatile necir_message_t NECIR_messageQueue[NECIR_QUEUE_LENGTH];
volatile uint8_t NECIR_repeatFlagQueue[NECIR_REPEAT_QUEUE_BYTES];
uint8_t NECIR_head; // initialized to zero by default
volatile uint8_t NECIR_tail; // initialized to zero by default

#if (NECIR_USE_GPIOR0)
#define NECIR_FLAGS GPIOR0
static inline void NECIR_SetRepeatTimeoutFlag(void) __attribute__(( always_inline ));
static inline void NECIR_SetRepeatTimeoutFlag(void) {
  setHigh(NECIR_FLAGS, NECIR_FLAG_REPEAT_TIMEOUT);
}
static inline void NECIR_ClearRepeatTimeoutFlag(void) __attribute__(( always_inline ));
static inline void NECIR_ClearRepeatTimeoutFlag(void) {
  setLow(NECIR_FLAGS, NECIR_FLAG_REPEAT_TIMEOUT);
}
static inline bool NECIR_GetRepeatTimeoutFlag (void) __attribute__(( always_inline ));
static inline bool NECIR_GetRepeatTimeoutFlag (void) {
  return getValue(NECIR_FLAGS, NECIR_FLAG_REPEAT_TIMEOUT);
}
#else // NECIR_USE_GPIOR0
bool NECIR_repeatTimeoutFlag;
static inline void NECIR_SetRepeatTimeoutFlag(void) {
  NECIR_repeatTimeoutFlag = true;
}
static inline void NECIR_ClearRepeatTimeoutFlag(void) {
  NECIR_repeatTimeoutFlag = false;
}
static inline bool NECIR_GetRepeatTimeoutFlag (void) {
  return NECIR_repeatTimeoutFlag;
}
#endif // NECIR_USE_GPIOR0

static inline bool NECIR_EnqueueMessageIfNotFull(uint8_t *message) __attribute__(( always_inline ));
static inline bool NECIR_EnqueueMessageIfNotFull(uint8_t *message) {
  uint8_t tail = NECIR_tail; // cache volatile NECIR_tail, since this function is only ever called from inside the ISR
  if (NECIR_head != (tail + 1) % NELEMS(NECIR_messageQueue)) { // same as if (!NECIR_QueueFull()) ... but uses cached tail
#if (NECIR_USE_EXTENDED_PROTOCOL)
    uint8_t *p = (uint8_t*)&NECIR_messageQueue[tail];
    *p++ = message[3]; // as fast as: NECIR_messageQueue[tail] =
    *p++ = message[2]; //               *((uint32_t*)message);
    *p++ = message[1]; // but we reverse the byte order for free here, rather than
    *p = message[0];   // explicitly reversing the bytes when each bit was received
#else // NECIR_USE_EXTENDED_PROTOCOL
    if (message[0] == (message[1] ^ 0xFF) && message[2] == (message[3] ^ 0xFF)) { // validate inverse bit patterns
      uint8_t *p = (uint8_t*)&NECIR_messageQueue[tail];
      *p++ = message[2]; // faster than: NECIR_messageQueue[tail] =
      *p = message[0];   //                ((uint16_t)message[0] << 8) | message[2];
    } else
      return false; // validation failed, we dropped the message, do not call NECIR_EnqueueRepeat()
#endif // NECIR_USE_EXTENDED_PROTOCOL
    return true; // return success, NECIR_EnqueueRepeat() should be called
  }
  return false; // the queue was full, we dropped the message, do not call NECIR_EnqueueRepeat()
}

// This method doesn't check if the queue is full, so it should only be called if NECIR_EnqueueMessageIfNotFull() returns true
static inline void NECIR_EnqueueRepeat(bool isRepeat) __attribute__(( always_inline ));
static inline void NECIR_EnqueueRepeat(bool isRepeat) {
  uint8_t tail = NECIR_tail; // cache volatile NECIR_tail, since this function is only ever called from inside the ISR
  if (isRepeat)
    NECIR_repeatFlagQueue[tail / 8] |= pgm_read_byte(&NECIR_oneLeftShiftedBy[tail % 8]);
  else
    NECIR_repeatFlagQueue[tail / 8] &= pgm_read_byte(&NECIR_oneLeftShiftedBy[tail % 8]) ^ 0xFF;
  NECIR_tail = (tail + 1) % NELEMS(NECIR_messageQueue);
}

void NECIR_Init(void)
{
  // Set IR pin as input and enable pullup
  setInput(IR_DDR, IR_PIN);
  enablePullup(IR_PORT, IR_PIN);

#if (NECIR_ISR_CTC_TIMER == 0)
  // CTC with OCR0A as TOP
  TCCR0A = (1 << WGM01);
  // clk_io/256 (From prescaler)
  TCCR0B = (1 << CS02);

  OCR0A = NECIR_CTC_TOP; // Generate an interrupt every (NECIR_CTC_TOP + 1)*256 clock cycles

  // Enable Timer/Counter0 Compare Match A interrupt
#ifdef TIMSK0
  TIMSK0 |= (1 << OCIE0A);
#else // TIMSK0
  TIMSK |= (1 << OCIE0A);
#endif // TIMSK0
#elif (NECIR_ISR_CTC_TIMER == 2)
  // CTC with OCR2A as TOP
  TCCR2A = (1 << WGM21);
  // clk_io/256 (From prescaler)
  TCCR2B = (1 << CS22) | (1 << CS21);

  OCR2A = NECIR_CTC_TOP; // Generate an interrupt every (NECIR_CTC_TOP + 1)*256 clock cycles

  // Enable Timer/Counter2 Compare Match A interrupt
  TIMSK2 |= (1 << OCIE2A);
#else // NECIR_ISR_CTC_TIMER
#error "NECIR_ISR_CTC_TIMER must be 0 or 2"
#endif // NECIR_ISR_CTC_TIMER

  // Disallow repeats until a valid command has been seen
  NECIR_SetRepeatTimeoutFlag();
}

// This interrupt will get called every (NECIR_CTC_TOP + 1)*256 clock cycles
#if (NECIR_ISR_CTC_TIMER == 0)
ISR(TIMER0_COMPA_vect)
#elif (NECIR_ISR_CTC_TIMER == 2)
ISR(TIMER2_COMPA_vect)
#else // NECIR_ISR_CTC_TIMER
#error "NECIR_ISR_CTC_TIMER must be 0 or 2"
#endif // NECIR_ISR_CTC_TIMER
{
  static enum { NECIR_STATE_WAITING_FOR_IDLE, NECIR_STATE_IDLE,
                NECIR_STATE_LEADER, NECIR_STATE_PAUSE,
                NECIR_STATE_BIT_LEADER, NECIR_STATE_BIT_PAUSE,
                NECIR_STATE_PROCESS, NECIR_STATE_PROCESS2,
                NECIR_STATE_REPEAT_PROCESS, NECIR_STATE_REPEAT_PROCESS2 } state;

  static uint8_t stateCounter; // stores the number of times we have sampled the state minus one
  static uint8_t bitCounter; // keeps track of how many bits we've currently decoded
  static uint8_t messageBit; // we pre-calculate this value in the BIT_LEADER state, so the BIT_PAUSE state can execute faster
  static uint16_t repeatTimeout; // timeout for when we no longer accept repeat codes for a given command
  static uint8_t nativeRepeatsNeeded; // counts down the number of native repeat messages seen, when zero, emits a repeat to the application
#if (NECIR_TURBO_MODE_AFTER != 0)
  static uint8_t turboModeCounter;
#endif // NECIR_TURBO_MODE_AFTER

  static uint8_t message[4]; // stores the decoded bits

  uint8_t sample = inputState(IR_INPUT, IR_PIN);

  switch (state) {
  case NECIR_STATE_WAITING_FOR_IDLE: // IR was low, waiting for high
    if (sample) // if high now, switch to idle state
      state = NECIR_STATE_IDLE;
    break;
  case NECIR_STATE_IDLE: // IR was high, waiting for low
    // If we haven't hit the timeout for when we no longer accept repeats yet, decrement the timeout counter and check
    if (!NECIR_GetRepeatTimeoutFlag() && --repeatTimeout == 0)
      NECIR_SetRepeatTimeoutFlag();
    if (!sample) { // if low now, reset the counter, and switch to leader state
      stateCounter = 0;
      state = NECIR_STATE_LEADER;
    }
    break;
  case NECIR_STATE_LEADER: // IR was low, needs to be low for 9ms
    if (!sample) { // if low now, make sure it hasn't been low for too long
      if (++stateCounter > (uint8_t)samplesFromMilliseconds(9.0) + 1)
        state = NECIR_STATE_WAITING_FOR_IDLE; // low for too long, switch to wait for idle state
    } else { // if high now, make sure that it was low for long enough
      if (stateCounter < (uint8_t)samplesFromMilliseconds(9.0) - 2)
        state = NECIR_STATE_IDLE; // was not low for long enough, switch to idle state
      else { // was low for 9ms, switch to pause state
        stateCounter = 0;
        state = NECIR_STATE_PAUSE;
      }
    }
    break;
  case NECIR_STATE_PAUSE: // IR was high, needs to be high for 4.5ms
    if (sample) { // if high now, make sure it hasn't been high for too long
      if (++stateCounter > (uint8_t)samplesFromMilliseconds(4.5) + 1)
        state = NECIR_STATE_IDLE; // high for too long, switch to idle state
    } else { // if low now, make sure that it was high for long enough
      if (stateCounter < (uint8_t)samplesFromMilliseconds(2.25) - 2) {
        state = NECIR_STATE_WAITING_FOR_IDLE; // was not high for long enough, switch to wait for idle state
        break;
      } else if (stateCounter > (uint8_t)samplesFromMilliseconds(2.25) + 1) { // was high for longer than a repeat code, so switch to bit leader state
        repeatTimeout = (uint16_t)samplesFromMilliseconds(98.1875) + 1; // initialize to 110 - 9.0 - 2.25 - 0.5625 ms (idle time between repeats)
        NECIR_ClearRepeatTimeoutFlag(); // allow repeat codes
        stateCounter = bitCounter = message[0] = message[1] = message[2] = message[3] = 0;
        state = NECIR_STATE_BIT_LEADER;
      } else { // was a repeat code
        state = NECIR_STATE_WAITING_FOR_IDLE;
        if (!NECIR_GetRepeatTimeoutFlag()) { // are repeat codes are still allowed for this command?
          repeatTimeout = (uint16_t)samplesFromMilliseconds(98.1875) + 1; // initialize to 110 - 9.0 - 2.25 - 0.5625 ms (idle time between repeats)
          if (--nativeRepeatsNeeded == 0) { // have we seen enough native repeat messages to pass one back to the application? 
            nativeRepeatsNeeded = NECIR_REPEAT_INTERVAL; // "delay until repeat" has been satisfied, so now we set the repeat interval
#if (NECIR_TURBO_MODE_AFTER != 0)
            if (++turboModeCounter == NECIR_TURBO_MODE_AFTER) { // have we repeated enough to activate turbo mode?
              --turboModeCounter; // ensure that we stay in turbo mode
              nativeRepeatsNeeded = NECIR_TURBO_REPEAT_INTERVAL; // set the turbo mode repeat interval
            }
#endif // NECIR_TURBO_MODE_AFTER
            state = NECIR_STATE_REPEAT_PROCESS; // keep the maximum execution time of the ISR down by enqueueing the message in a new state
          }
        }
      }
    }
    break;
  case NECIR_STATE_REPEAT_PROCESS:
    if (NECIR_EnqueueMessageIfNotFull(message)) // if there is room on the queue, put the decoded message on it, otherwise drop the message
      state = NECIR_STATE_REPEAT_PROCESS2; // split the enqueue across two states to decrease the maximum running time of this ISR
    else
      state = NECIR_STATE_WAITING_FOR_IDLE;
    break;
  case NECIR_STATE_REPEAT_PROCESS2:
    state = NECIR_STATE_WAITING_FOR_IDLE;
    NECIR_EnqueueRepeat(true); // if there was room on the queue, set the repeat flag
    break;
  case NECIR_STATE_BIT_LEADER: // IR was low, needs to be low for 562.5uS
    if (!sample) { // if low now, make sure it hasn't been low for too long
      if (++stateCounter > (uint8_t)samplesFromMilliseconds(0.5625) + 1)
        state = NECIR_STATE_WAITING_FOR_IDLE; // low for too long, switch to wait for idle state
    } else { // if high now, make sure that it was low for long enough
      if (stateCounter < (uint8_t)samplesFromMilliseconds(0.5625) - 2)
        state = NECIR_STATE_IDLE; // was not low for long enough, switch to idle state
      else  { // was low for 562.5uS, switch to bit pause state
        stateCounter = 0;
        messageBit = pgm_read_byte(&NECIR_oneLeftShiftedBy[bitCounter % 8]); // pre-calculate the value we might need in the next state to make it faster
        state = NECIR_STATE_BIT_PAUSE;
      }
    }
    break;
  case NECIR_STATE_BIT_PAUSE: // IR was high, needs to be high for either 562.5uS (0-bit) or 1.6875ms (1-bit)
    if (sample) { // if high now, make sure it hasn't been high for too long
      if (++stateCounter > (uint8_t)samplesFromMilliseconds(1.6875) + 1)
        state = NECIR_STATE_IDLE; // high for too long, switch to idle state
    } else { // if low now, make sure that it was high for long enough
      if (stateCounter < (uint8_t)samplesFromMilliseconds(0.5625) - 2) {
        state = NECIR_STATE_WAITING_FOR_IDLE; // was not high for long enough, switch to wait for idle state
        break;
      } else if (stateCounter > (uint8_t)samplesFromMilliseconds(0.5625) + 1) // was high for longer than a 0-bit, so it's a 1-bit
        message[bitCounter / 8] |= messageBit; // way faster than "uint32_t message |= ((uint32_t)1 << bitCounter)"
      if (++bitCounter < 32) { // are there more bits we need to read in?
        stateCounter = 0;
        state = NECIR_STATE_BIT_LEADER;
      } else
        state = NECIR_STATE_PROCESS; // keep the maximum execution time of the ISR down by enqueueing the message in a new state
    }
    break;
  case NECIR_STATE_PROCESS: // At this point 'message' contains a 32-bit value representing the raw bits received
#if (NECIR_TURBO_MODE_AFTER != 0)
    turboModeCounter = 0;
#endif // NECIR_TURBO_MODE_AFTER
    nativeRepeatsNeeded = NECIR_DELAY_UNTIL_REPEAT; // set "delay until repeat" length
    if (NECIR_EnqueueMessageIfNotFull(message)) // if there is room on the queue, put the decoded message on it, otherwise drop the message
      state = NECIR_STATE_PROCESS2; // split the enqueue across two states to decrease the maximum running time of this ISR
    else
      state = NECIR_STATE_WAITING_FOR_IDLE;
    break;
  case NECIR_STATE_PROCESS2:
    state = NECIR_STATE_WAITING_FOR_IDLE;
    NECIR_EnqueueRepeat(false); // if there was room on the queue, clear the repeat flag
    break;
  }
}
