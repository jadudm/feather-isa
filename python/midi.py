import usb_midi
import adafruit_midi
import board, digitalio, math
from random import randrange
from analogio import AnalogIn
import time, busio
import neopixel 

# Configure the Feather as a MIDI device.
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=0)

# Set up the UART for talking to the Micro:Bit
uart = busio.UART(board.TX, board.RX, baudrate = 115200)

# Why green? I don't know. 
pixels = neopixel.NeoPixel(board.NEOPIXEL, 1, brightness = 0.1)
pixels[0] = (0, 60, 0) 

# Pin Configuration
# We handle some pins as digital, and some as analog.
# The end-user can plug switches and buttons into the top of the board
# (which are all digital pins), and they can plug knobs and other 
# continuous devices into the bottom of the board (analog).
digital_pins = [board.D5, board.D6, board.D9, board.D10, board.D11, board.D12, board.D13]
NUM_DIGITAL_PINS = len(digital_pins)
analog_pins  = [board.A0, board.A1, board.A2, board.A3, board.A4, board.A5]
NUM_ANALOG_PINS  = len(analog_pins)

# All of the arrays are predefined, so as to minimize allocation and GC.
digital_objects = [None, None, None, None, None, None, None]
current_digital = [True, True, True, True, True, True, True]
prev_digital    = [True, True, True, True, True, True, True]
analog_objects  = [None, None, None, None, None, None]
current_analog  = [0, 0, 0, 0, 0, 0]
prev_analog     = [0, 0, 0, 0, 0, 0]

# The setup functions make sure that the pins are headed in the right direction.
def setup_digital_pins():
  for ndx, p in enumerate(digital_pins):
    digital_objects[ndx] = digitalio.DigitalInOut(p)
    digital_objects[ndx].switch_to_input()
    digital_objects[ndx].pull = digitalio.Pull.UP

def setup_analog_pins():
  for ndx, p in enumerate(analog_pins):
    analog_objects[ndx] = AnalogIn(p)

# For the digital pins, we find out if anyone flipped state.
# There is no corresponding analog version; it takes too long,
# and the floating analog inputs are going to just wiggle 
# randomly anyway. Might as well send the data.
def detect_digital_pin_change():
  changed = False
  for ndx in range(NUM_DIGITAL_PINS):
    v = digital_objects[ndx].value
    current_digital[ndx] = v
    if v != prev_digital[ndx]:
      changed = True
  return changed

# Updates the state on all of the pins, digital and analog.
def update_state():
  for ndx in range(0, NUM_DIGITAL_PINS):
    prev_digital[ndx] = current_digital[ndx]
  for ndx in range(0, NUM_ANALOG_PINS):
    prev_analog[ndx] = current_analog[ndx]

# Set up the pins as inputs.
setup_digital_pins()
setup_analog_pins()

# Used for converting serial to a number.
# Probably could be a lot simpler.
def fill():
  buff = [-1, -1, -1]
  ndx = 0
  READ = False
  while not READ:
    data = uart.read(1) 
    if (data == b'S'):
      READ = True

  while READ:
    data = uart.read(1)
    if data == b'E':
      READ = False
    else:
      o = ord(data) 
      if (o >= 0x30) and (o <= 0x39):
        v = o - 48
        # print("v: " + str(v) + " ord: " + str(ord(data)))
        buff[ndx] = v
        ndx = ndx + 1
      else:
        READ = False
  if buff[1] == -1:
    return buff[0]
  if buff[2] == -1:
    v = buff[0] * 10 + buff[1]
    return v
  else:
    v = buff[0] * 100 + buff[1] * 10 + buff[2]
    return v

# This might be painfully slow. We'll see.
def twiddle():
  pixels[0] = (randrange(30, 200), randrange(30, 200), randrange(30, 200))

###########################
# FOREVER
while True:
  # First, read in all of the analog values.
  # Make sure they're 0-127.
  for ndx in range(0, NUM_ANALOG_PINS):
      # print(current_analog[ndx], end=' ')
      v = math.floor((analog_objects[ndx].value / 65535) * 127)
      # These go out on pins 7 through 13.
      midi.control_change(7 + ndx, abs(v))

  # Now, see if any digital pins changed.
  # We only send out updates for pins that have changed.
  if detect_digital_pin_change():
    for ndx in range(0, NUM_DIGITAL_PINS):
      if prev_digital[ndx] != current_digital[ndx]:
        # print(str(count) + " " + str(ndx))
        # We are pulling high, so send 0 when True and 1 when False
        midi.control_change(ndx, not current_digital[ndx])
        twiddle()
    # Make sure to update the pin state for all of the digital pins before 
    # dropping out of the loop. This is prev = current.
    update_state()

  # If we have any UART data waiting, we should handle it.
  if uart.in_waiting > 0:
    # Read a single character.
    data = uart.read(1)
    # If something was there, and it is the start of a transmission, we should 
    # read in a pair of numbers. The first needs to be the control channel, the second
    # needs to be the value that we want to transmit.
    if data is not None:
      if data == b'S':
        # print("C")
        cc = fill()
        # print("V")
        v  = fill()
        # print(str(cc) + ":" + str(v))
        midi.control_change(cc, abs(v))
        twiddle()
