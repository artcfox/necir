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
  
  // Set diagnostic probe pin as output and set it low
  setOutput(PROBE_DDR, PROBE_PIN);
  setLow(PROBE_PORT, PROBE_PIN);

  // Initialize the NEC IR library
  NECIR_Init();
  
  // Enable Global Interrupts
  sei();

  necir_message_t message; // stores the decoded message
  bool isRepeat; // whether the message is a repeat message or not

  for (;;) {
    // Uncomment the following line to see how long it takes the ISR to execute in its various states
    /* while (1) setHigh(PROBE_INPUT, PROBE_PIN); */

    // Process all queued NEC IR events
    while (NECIR_HasMessage()) {
      NECIR_GetNextMessage(&message, &isRepeat);

#if (NECIR_SUPPORT_EXTENDED_PROTOCOL)
      if (message == 0xF708FB04 && !isRepeat) // disallow repeat for power button
	setHigh(LED_INPUT, LED_PIN);
      else if (message == 0xFD02FB04)
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(50);
	}
      else if (message == 0xE51ABF00)
	for (uint8_t i = 0; i < 6; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(50);
	}
      else if (message == 0xFF00BF00)
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(50);
	}
#else // NECIR_SUPPORT_EXTENDED_PROTOCOL
      /* for (uint16_t i = 0; i < ((uint8_t)message << 1); ++i) { */
      /* 	setHigh(LED_INPUT, LED_PIN); */
      /* 	_delay_ms(200); */
      /* } */
      /* continue; */
      if (message == 0x0408 && !isRepeat) // disallow repeat for power button
	setHigh(LED_INPUT, LED_PIN);
      else if (message == 0x0402) // VOL_UP
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(37.5);
	}
      else if (message == 0x0403) // VOL_DN
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(50);
	}
      else if (message == 0x0400) // CH_UP
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(12.5);
	}
      else if (message == 0x0401) // CH_DN
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(25);
	}
      else if (message == 0x001A)
	for (uint8_t i = 0; i < 6; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(50);
	}
      else if (message == 0x0000)
	for (uint8_t i = 0; i < 2; ++i) {
	  setHigh(LED_INPUT, LED_PIN);
	  _delay_ms(50);
	}
#endif // NECIR_SUPPORT_EXTENDED_PROTOCOL
    }
  }

  return 0;
}
