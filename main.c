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

  for (;;) {
    // Uncomment the following line to see how long it takes the ISR to execute in its various states
    /* while (1) setHigh(LED_INPUT, LED_PIN); */

    // Process all queued NEC IR events
    while (!NECIR_QueueEmpty()) {
      NECIR_Dequeue(&message, &isRepeat);

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
