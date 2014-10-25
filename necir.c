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

#if ((NECIR_QUEUE_LENGTH <= 0) || NECIR_QUEUE_LENGTH > 256)
#error "NECIR_QUEUE_LENGTH must be between 1 and 256, powers of two strongly preferred"
#endif // NECIR_QUEUE_LENGTH

const uint8_t oneLeftShift[8] = {1, 2, 4, 8, 16, 32, 64, 128}; // avoids having to bit shift by a variable amount, always runs in constant time

volatile uint32_t NECIR_messageQueue[NECIR_QUEUE_LENGTH];
volatile uint8_t NECIR_head = 0;
volatile uint8_t NECIR_tail = 0;

volatile uint8_t NECIR_repeatFlagQueue[NECIR_REPEAT_QUEUE_BYTES];

static inline uint8_t NECIR_full(void) __attribute__(( always_inline ));
static inline uint8_t NECIR_full(void) {
  return (NECIR_head == (NECIR_tail + 1) % NELEMS(NECIR_messageQueue));
}

static inline void NECIR_enqueue(uint32_t *message, bool isRepeat) __attribute__(( always_inline ));
static inline void NECIR_enqueue(uint32_t *message, bool isRepeat) {
  NECIR_messageQueue[NECIR_tail] = *message;
  if (isRepeat)
    NECIR_repeatFlagQueue[NECIR_tail/8] |= oneLeftShift[NECIR_tail%8];
  else
    NECIR_repeatFlagQueue[NECIR_tail/8] &= oneLeftShift[NECIR_tail%8] ^ 0xFF;
  NECIR_tail = (NECIR_tail + 1) % NELEMS(NECIR_messageQueue);
}

void NECIR_Init(void)
{
#if (NECIR_ISR_CTC_TIMER == 0)
  // CTC with OCR0A as TOP
  TCCR0A = (1 << WGM01);
  // clk_io/256 (From prescaler)
  TCCR0B = (1 << CS02);

  OCR0A = NECIR_CTC_TOP; // Generate an interrupt every (NECIR_CTC_TOP + 1)*256 clock cycles

  // Enable Timer/Counter0 Compare Match A interrupt
  TIMSK0 |= (1 << OCIE0A);
#elif (NECIR_ISR_CTC_TIMER == 2)
  // CTC with OCR2A as TOP
  TCCR2A = (1 << WGM21);
  // clk_io/256 (From prescaler)
  TCCR2B = (1 << CS22) | (1 << CS21);

  OCR2A = NECIR_CTC_TOP; // Generate an interrupt every (NECIR_CTC_TOP + 1)*256 clock cycles

  // Enable Timer/Counter2 Compare Match A interrupt
  TIMSK2 |= (1 << OCIE2A);
#else
#error "NECIR_ISR_CTC_TIMER must be 0 or 2"
#endif // NECIR_ISR_CTC_TIMER
}

// This interrupt will get called every (NECIR_CTC_TOP + 1)*256 clock cycles
#if (NECIR_ISR_CTC_TIMER == 0)
ISR(TIMER0_COMPA_vect)
#elif (NECIR_ISR_CTC_TIMER == 2)
ISR(TIMER2_COMPA_vect)
#else
#error "NECIR_ISR_CTC_TIMER must be 0 or 2"
#endif // NECIR_ISR_CTC_TIMER
{
  setHigh(PROBE_INPUT, PROBE_PIN);
  static enum { NECIR_STATE_WAITING_FOR_IDLE, NECIR_STATE_IDLE, NECIR_STATE_LEADER, NECIR_STATE_PAUSE, NECIR_STATE_BIT_LEADER, NECIR_STATE_BIT_PAUSE } state;
  static uint8_t stateCounter; // stores the number of times we have sampled the state minus one
  static uint8_t bitCounter; // keeps track of how many bits we've currently decoded

  /*
    A counter `repeatCounter' is needed to detect when a repeat code
    should no longer be accepted. Without this variable and its
    accompanying test, if the IR signal from a second message is
    blocked, but its repeat codes are seen, the library would
    incorrectly issue repeats for the previous (wrong) command.

    The variable `stateCounter' can't be used for this purpose,
    because stateCounter is still in use counting the number of times
    a voltage level has been sampled to determine transitions between
    states. Detecting repeat codes happens at a higher level, across
    multiple state transitions, and takes enough time to require a
    16-bit variable.
  */
  static uint16_t repeatCounter; // zeroed after 32-bits have been received, and every time a repeat code has been seen
  static uint8_t nativeRepeatsSeen; // counts the number of native repeat messages seen, because the repeats we issue will be a multiple of this number
  static uint8_t nativeRepeatsNeeded; // when nativeRepeatsSeen counts up to this value, then we actually issue a repeat

#if (NECIR_ENABLE_TURBO_MODE)
  static uint8_t turboModeCounter;
#endif // NECIR_ENABLE_TURBO_MODE

  union Message {
    uint32_t value;
    uint8_t byte[4];
  };
  static union Message message;
  
  uint8_t sample = getValue(IR_INPUT, IR_PIN);

  ++repeatCounter;
  switch (state) {
  case NECIR_STATE_WAITING_FOR_IDLE: // IR was low, waiting for high
    if (sample) // if high now, switch to idle state
      state = NECIR_STATE_IDLE;
    break;
  case NECIR_STATE_IDLE: // IR was high, waiting for low
    if (!sample) { // if low now, reset the counter, and switch to leader state
      stateCounter = 0;
      state = NECIR_STATE_LEADER;
    }
    break;
  case NECIR_STATE_LEADER: // IR was low, needs to be low for 9ms
    if (!sample) { // if low now, make sure it hasn't been low for too long
      if (++stateCounter > (uint8_t)(9.0/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
	state = NECIR_STATE_WAITING_FOR_IDLE; // low for too long, switch to wait for idle state
    } else { // if high now, make sure that it was low for long enough
      if (stateCounter < (uint8_t)(9.0/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) - 2))
	state = NECIR_STATE_IDLE; // was not low for long enough, switch to idle state
      else { // was low for 9ms, switch to pause state
	stateCounter = 0;
	state = NECIR_STATE_PAUSE;
      }
    }
    break;
  case NECIR_STATE_PAUSE: // IR was high, needs to be high for 4.5ms
    if (sample) { // if high now, make sure it hasn't been high for too long
      if (++stateCounter > (uint8_t)(4.5/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
	state = NECIR_STATE_IDLE; // high for too long, switch to idle state
    } else { // if low now, make sure that it was high for long enough
      if (stateCounter < (uint8_t)(2.25/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) - 2)) {
	state = NECIR_STATE_WAITING_FOR_IDLE; // was not high for long enough, switch to wait for idle state
	break;
      } else if (stateCounter > (uint8_t)(2.25/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1)) { // was high for longer than a repeat code, so switch to bit leader state
	repeatCounter = stateCounter = bitCounter = message.value = 0;
	state = NECIR_STATE_BIT_LEADER;
      } else { // was a repeat code
	state = NECIR_STATE_WAITING_FOR_IDLE;
	if (repeatCounter <= (uint16_t)(110.25/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1)) { // make sure the repeat code came within 110ms of the last message
	  repeatCounter = 0; // reset the repeat timeout counter so it can be used for additional repeat messages

	  /*
	    To implement "delay until repeat" behavior, we need to
	    have seen a certain number of native repeat messages
	    before we pass one back to the application.
	  */
	  if (++nativeRepeatsSeen == nativeRepeatsNeeded) { // have we seen the proper number of native repeat messages? 
	    nativeRepeatsSeen = 0; // reset the counter so we can begin counting for the next repeat message

	    /*
	      After we've met the condition for "delay until repeat"
	      behavior, decrease the nativeRepeatsNeeded variable to
	      implement "repeat interval" behavior.
	    */
	    switch (nativeRepeatsNeeded) {
	    case NECIR_DELAY_UNTIL_REPEAT:
	      nativeRepeatsNeeded = NECIR_REPEAT_INTERVAL; // set the initial repeat interval
	      break;
#if (NECIR_ENABLE_TURBO_MODE)
	    case NECIR_REPEAT_INTERVAL:
	      if (++turboModeCounter == NECIR_TURBO_MODE_AFTER) { // have we repeated enough to decrease the repeat interval even further?
		//		turboModeCounter = 0;
		nativeRepeatsNeeded = 1;
	      }
	      break;
#endif // NECIR_ENABLE_TURBO_MODE
	    }
	    if (!NECIR_full()) // if there is room on the queue, put the decoded message on it, otherwise we drop the message
	      NECIR_enqueue(&message.value, true);
	  }
	}
      }
    }
    break;
  case NECIR_STATE_BIT_LEADER: // IR was low, needs to be low for 562.5uS
    if (!sample) { // if low now, make sure it hasn't been low for too long
      if (++stateCounter > (uint8_t)(0.5625/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
	state = NECIR_STATE_WAITING_FOR_IDLE; // low for too long, switch to wait for idle state
    } else { // if high now, make sure that it was low for long enough
      if (stateCounter < (uint8_t)(0.5625/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) - 2))
	state = NECIR_STATE_IDLE; // was not low for long enough, switch to idle state
      else  { // was low for 562.5uS, switch to bit pause state
	stateCounter = 0;
	state = NECIR_STATE_BIT_PAUSE;
      }
    }
    break;
  case NECIR_STATE_BIT_PAUSE: // IR was high, needs to be high for either 562.5uS (0-bit) or 1.6875ms (1-bit)
    if (sample) { // if high now, make sure it hasn't been high for too long
      if (++stateCounter > (uint8_t)(1.6875/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
	state = NECIR_STATE_IDLE; // high for too long, switch to idle state
    } else { // if low now, make sure that it was high for long enough
      if (stateCounter < (uint8_t)(0.5625/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) - 2)) {
	state = NECIR_STATE_WAITING_FOR_IDLE; // was not high for long enough, switch to wait for idle state
	break;
      } else if (stateCounter > (uint8_t)(0.5625/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1)) // was high for longer than a 0-bit, so it's a 1-bit
	message.byte[bitCounter/8] |= oneLeftShift[bitCounter%8]; // does the same thing as "message.value |= ((uint32_t)1 << bitCounter)" but orders of magnitude faster
      if (++bitCounter > 31) { // At this point 'message' contains a 32-bit value representing the raw bits received
	state = NECIR_STATE_WAITING_FOR_IDLE;
	if (!NECIR_full()) // if there is room on the queue, put the decoded message on it, otherwise we drop the message
	  NECIR_enqueue(&message.value, false);
	nativeRepeatsSeen = 0; // initialize repeat counters
#if (NECIR_ENABLE_TURBO_MODE)
	turboModeCounter = 0;
#endif // NECIR_ENABLE_TURBO_MODE
	nativeRepeatsNeeded = NECIR_DELAY_UNTIL_REPEAT; // set "delay until repeat" length
      } else {
	stateCounter = 0;
	state = NECIR_STATE_BIT_LEADER;
      }
    }
    break;
  }
  setHigh(PROBE_INPUT, PROBE_PIN);
}
