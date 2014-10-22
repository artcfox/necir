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
#include <util/delay.h>

#include "necir.h"

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

// This interrupt will get called every (NEC_CTC_TOP + 1)*256 clock cycles
#if (NECIR_ISR_CTC_TIMER == 0)
ISR(TIMER0_COMPA_vect)
#elif (NECIR_ISR_CTC_TIMER == 2)
ISR(TIMER2_COMPA_vect)
#else
#error "NECIR_ISR_CTC_TIMER must be 0 or 2"
#endif // NECIR_ISR_CTC_TIMER
{
  static enum { NECIR_STATE_WAITING_FOR_IDLE, NECIR_STATE_IDLE, NECIR_STATE_LEADER, NECIR_STATE_PAUSE, NECIR_STATE_BIT_LEADER, NECIR_STATE_BIT_PAUSE } state;
  static uint8_t stateCounter; // stores the number of times we have sampled the state minus one
  static uint8_t bitCounter;
  static uint32_t bits;
  uint8_t sample = getValue(IR_INPUT, IR_PIN);

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
      ++stateCounter;
      if (stateCounter > (uint8_t)(9.0/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
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
      ++stateCounter;
      if (stateCounter > (uint8_t)(4.5/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
	state = NECIR_STATE_IDLE; // high for too long, switch to idle state
    } else { // if low now, make sure that it was high for long enough
      if (stateCounter < (uint8_t)(4.5/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) - 2))
	state = NECIR_STATE_WAITING_FOR_IDLE; // was not high for long enough, switch to wait for idle state
      else { // was high for 4.5ms, switch to bit leader state
	stateCounter = bitCounter = bits = 0;
	state = NECIR_STATE_BIT_LEADER;
      }
    }
    break;
  case NECIR_STATE_BIT_LEADER: // IR was low, needs to be low for 562.5uS
    if (!sample) { // if low now, make sure it hasn't been low for too long
      ++stateCounter;
      if (stateCounter > (uint8_t)(0.5625/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
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
      ++stateCounter;
      if (stateCounter > (uint8_t)(1.6875/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1))
	state = NECIR_STATE_IDLE; // high for too long, switch to idle state
    } else { // if low now, make sure that it was high for long enough
      if (stateCounter < (uint8_t)(0.5625/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) - 2))
	state = NECIR_STATE_WAITING_FOR_IDLE; // was not high for long enough, switch to wait for idle state
      else if (stateCounter > (uint8_t)(0.5625/((float)(NECIR_CTC_TOP+1)*256*1000/F_CPU) + 1)) // was high for longer than a 0-bit, so it's a 1-bit
	bits |= ((uint32_t)1 << bitCounter);
      ++bitCounter;
      if (bitCounter > 31) { // At this point 'bits' contains a 32-bit value representing the raw bits received
	state = NECIR_STATE_WAITING_FOR_IDLE;
	if (bits == 0xF708FB04)
	  setHigh(LED_INPUT, LED_PIN);
	else if (bits == 0xFD02FB04)
	  for (uint8_t i = 0; i < 4; ++i) {
	    setHigh(LED_INPUT, LED_PIN);
	    _delay_ms(100);
	  }
	else if (bits == 0xE51ABF00)
	  for (uint8_t i = 0; i < 6; ++i) {
	    setHigh(LED_INPUT, LED_PIN);
	    _delay_ms(100);
	  }	  
	else if (bits == 0xFF00BF00)
	  for (uint8_t i = 0; i < 6; ++i) {
	    setHigh(LED_INPUT, LED_PIN);
	    _delay_ms(100);
	  }
      } else {
	stateCounter = 0;
	state = NECIR_STATE_BIT_LEADER;
      }
    }
    break;
  }

}
