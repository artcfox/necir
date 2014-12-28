#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "necir.h"

int main(void)
{
  // Set diagnostic LED pin as output and turn LED off
  setOutput(LED_DDR, LED_PIN);
  setLow(LED_PORT, LED_PIN);
  
  // Initialize the NEC IR library
  NECIR_Init();
  
  // Enable Global Interrupts
  sei();

  necir_message_t message; // stores the decoded message
  bool isRepeat; // whether the message is a repeat message or not

  // The following block of code is used for profiling the execution
  // time of the interrupt using a logic analyzer. The inline asm is
  // to work around a compiler bug that inserts two 'out' instructions
  // before looping, which causes the duty-cycle of the square wave to
  // not be 50%. Check the disassembly to make sure both the memory
  // location and register used in the inline assembly match the
  // memory location and register used in the statement above the
  // loop.

 /*  LED_INPUT = (1 << LED_PIN); */
 /* loop: */
 /*  __asm__ volatile ("out 0x16,r24" "\n\t" ::); */
 /*  goto loop; */

  for (;;) {
    // Process all queued NEC IR events
    while (!NECIR_QueueEmpty()) {
      NECIR_Dequeue(&message, &isRepeat);

      // In a typical application, you will choose to either support
      // the normal NECIR protocol, or the extended NECIR protocol, so
      // you won't need the following set of conditionals that support
      // both.
      //
      // Keep in mind that in order to support the Adafruit Mini IR
      // Remote, you must use the extended NECIR protocol.

#if (NECIR_USE_EXTENDED_PROTOCOL)
      if (message == 0x04FB08F7 && !isRepeat) // disallow repeat for power button
        setHigh(LED_INPUT, LED_PIN);
      else if (message == 0x04FB02FD) // VOL_UP
        for (uint8_t i = 0; i < 2; ++i) {
          setHigh(LED_INPUT, LED_PIN);
          _delay_ms(37.5);
        }
      else if (message == 0x04FB03FC) // VOL_DN
        for (uint8_t i = 0; i < 2; ++i) {
          setHigh(LED_INPUT, LED_PIN);
          _delay_ms(50);
        }
      else if (message == 0x04FB00FF) // CH_UP
        for (uint8_t i = 0; i < 2; ++i) {
          setHigh(LED_INPUT, LED_PIN);
          _delay_ms(12.5);
        }
      else if (message == 0x04FB01FE) // CH_DN
        for (uint8_t i = 0; i < 2; ++i) {
          setHigh(LED_INPUT, LED_PIN);
          _delay_ms(25);
        }
      else if ((uint16_t)(message >> 16) == 0x00BF) // Address bits for Adafruit Mini IR Remote
        for (uint8_t i = 0; i < 2; ++i) {
          setHigh(LED_INPUT, LED_PIN);
          _delay_ms(250);
        }

#else // NECIR_SUPPORT_EXTENDED_PROTOCOL
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
#endif // NECIR_SUPPORT_EXTENDED_PROTOCOL
    }
  }

  return 0;
}
