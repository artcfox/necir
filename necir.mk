# ---------- Begin NECIR Configuration Section ----------

# Defines which 8-bit Timer is used to generate the interrupt that
# fires every ((uint8_t)(F_CPU/1000000))*256 clock cycles. Useful if
# you are already using a timer for something else.
#  0 = 8-bit Timer/Counter0
#  2 = 8-bit Timer/Counter2
NECIR_ISR_CTC_TIMER = 0

# NEC IR messages are decoded in real-time by the interrupt routine,
# and are placed into a queue where the main() routine can fetch them.
# NECIR_QUEUE_LENGTH defines the length of this queue, and thus the
# number of outstanding (unread) messages at any given time. As long
# this queue isn't full, any new incoming messages will be correctly
# received, even if the code running inside main() is busy at the
# time new messages are received.
#
# Note: NECIR_QUEUE_LENGTH must be between 1 and 256, powers of two
#       are strongly preferred.
NECIR_QUEUE_LENGTH = 16

# The NEC IR standard specifies a 32-bit message, sent LSB-first with
# the first 8-bits being an address, followed by the 8-bit inverse of
# that address, followed by an 8-bit command, followed by the 8-bit
# inverse of that command. This results in 16 bits of actual data
# being sent per message. If you are willing to give up error
# checking, it is possible to instead use those inverse bits for
# actual data, increasing the total message size to 32 bits.
#
# 0 = Use the standard NEC protocol
# 1 = Use the extended NEC protocol (Required for Adafruit Mini IR Remote)
NECIR_USE_EXTENDED_PROTOCOL = 1

# NECIR_DELAY_UNTIL_REPEAT defines how many repeats at the native
# repeat interval of the IR remote (108ms) are skipped before emitting
# the first repeat, after which the repeats will be emitted every
# NECIR_REPEAT_INTERVAL * 108ms
#
NECIR_DELAY_UNTIL_REPEAT = 6
NECIR_REPEAT_INTERVAL = 3

# NECIR_TURBO_MODE_AFTER defines how many repeats at the
# NECIR_REPEAT_INTERVAL interval (repeats actually emitted by this
# library) must occur before the repeat interval gets changed to
# NECIR_TURBO_REPEAT_INTERVAL. The following two settings allow the
# repeats to come even quicker when a button is held down long enough.
# 
# NECIR_TURBO_MODE_AFTER
#     0 = Disable turbo mode
# 1-255 = Number of repeats before changing the repeat interval to
#         NECIR_TURBO_REPEAT_INTERVAL
#
# NECIR_TURBO_REPEAT_INTERVAL
# 1-255 = Turbo repeats occur at this multiple of 108 ms.
#
NECIR_TURBO_MODE_AFTER = 0
NECIR_TURBO_REPEAT_INTERVAL = 1

# This defines which pin the IR receiver is connected to:
IR_DDR = DDRB
IR_PORT = PORTB
IR_INPUT = PINB
IR_PIN = PB3

# This line integrates all NECIR-related defines into a single flag called:
#     $(NECIR_DEFINES)
# which should be appended to the definition of COMPILE below
NECIR_DEFINES = -DNECIR_ISR_CTC_TIMER=$(NECIR_ISR_CTC_TIMER) \
                -DNECIR_QUEUE_LENGTH=$(NECIR_QUEUE_LENGTH) \
                -DNECIR_USE_EXTENDED_PROTOCOL=$(NECIR_USE_EXTENDED_PROTOCOL) \
                -DNECIR_DELAY_UNTIL_REPEAT=$(NECIR_DELAY_UNTIL_REPEAT) \
                -DNECIR_REPEAT_INTERVAL=$(NECIR_REPEAT_INTERVAL) \
                -DNECIR_TURBO_MODE_AFTER=$(NECIR_TURBO_MODE_AFTER) \
                -DNECIR_TURBO_REPEAT_INTERVAL=$(NECIR_TURBO_REPEAT_INTERVAL) \
                -DIR_DDR=$(IR_DDR) \
                -DIR_PORT=$(IR_PORT) \
                -DIR_INPUT=$(IR_INPUT) \
                -DIR_PIN=$(IR_PIN)

# ---------- End NECIR Configuration Section ----------
