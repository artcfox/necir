#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "necir.h"

#define setInput(ddr, pin) ((ddr) &= ~(1 << (pin)))
#define setOutput(ddr, pin) ((ddr) |= (1 << (pin)))
#define setLow(port, pin) ((port) &= ~(1 << (pin)))
#define setHigh(port, pin) ((port) |= (1 << (pin)))
#define getValue(port, pin) ((port) & (1 << (pin)))
#define inputState(input, pin) ((input) & (1 << (pin)))
#define outputState(port, pin) ((port) & (1 << (pin)))
#define enablePullup(port, pin) ((port) |= (1 << (pin)))
 

int main(void)
{

  // Set IR pin as input and enable pullup
  setInput(IR_DDR, IR_PIN);
  enablePullup(IR_PORT, IR_PIN);

  // Set diagnostic LED pin as output and turn LED off
  setOutput(LED_DDR, LED_PIN);
  setHigh(LED_PORT, LED_PIN);
  
  // Initialize the NEC IR library
  NECIR_Init();
  
  // Enable Global Interrupts
  sei();

  uint32_t bits; // stores the decoded message

  for (;;) {
    // Process all queued NEC IR events
    while (NECIR_HasEvent()) {
      NECIR_GetNextEvent(&bits);

      if (bits == 0xF708FB04)
	setHigh(LED_INPUT, LED_PIN);
      else if (bits == 0xFD02FB04)
	for (uint8_t i = 0; i < 4; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(75);
	}
      else if (bits == 0xE51ABF00)
	for (uint8_t i = 0; i < 6; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(75);
	}
      else if (bits == 0xFF00BF00)
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(75);
	}

    }
  }

  return 0;
}
