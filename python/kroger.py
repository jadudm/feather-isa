# Copyright 2019 Matt Jadud <matt@jadud.com>
# This code is made available under the MIT License
# https://opensource.org/licenses/MIT
import usb_midi
import adafruit_midi
from adafruit_midi.control_change          import ControlChange
from adafruit_midi.note_off                import NoteOff
from adafruit_midi.note_on                 import NoteOn
import board, digitalio, math
from time import monotonic
from random import randrange
from analogio import AnalogIn
import time, busio
import neopixel 
import adafruit_hcsr04
import microcontroller

# Configure the Feather as a MIDI device.
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], midi_in=usb_midi.ports[0], out_channel=0, in_channel = 0)
# Set up the UART for talking to the Micro:Bit
uart = busio.UART(board.TX, board.RX, baudrate = 115200)

# Pin Configuration
# I handle some pins as digital, and some as analog.
# The end-user can plug switches and buttons into the top of the board
# (which are all digital pins), and they can plug knobs and other 
# continuous devices into the bottom of the board (analog).
digital_pins = [board.D5, board.D6, board.D9, board.D10, board.D11, board.D12, board.D13, board.A0, board.A1, board.A5]
NUM_DIGITAL_PINS = len(digital_pins)
analog_pins  = [board.A2, board.A3, board.A4]
NUM_ANALOG_PINS  = len(analog_pins)

# All of the arrays are predefined, so as to minimize allocation and GC.
digital_objects = list()
current_digital = list() 
prev_digital   = list() 
for i in range(NUM_DIGITAL_PINS):
  digital_objects.append(None)
  current_digital.append(True)
  prev_digital.append(True)

analog_objects = list() 
current_analog  = list()
prev_analog     = list()
for i in range(NUM_ANALOG_PINS):
  analog_objects.append(None)
  current_analog.append(0)
  prev_analog.append(0)

# The setup functions make sure that the pins are headed in the right direction.
def setup_digital_pins():
  for ndx, p in enumerate(digital_pins):
    digital_objects[ndx] = digitalio.DigitalInOut(p)
    digital_objects[ndx].switch_to_input()
    digital_objects[ndx].pull = digitalio.Pull.UP

def setup_analog_pins():
  for ndx, p in enumerate(analog_pins):
    analog_objects[ndx] = AnalogIn(p)

# For the digital pins, I find out if anyone flipped state.
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
def fill(started = False):
  buff = [-1, -1, -1]
  ndx = 0
  READ = started
  while not READ:
    data = uart.read(1) 
    if (data == b'S'):
      READ = True

  while READ and ndx < 3:
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

######################################################
# NEOPIXELS
######################################################
def setBoardPixel(t):
  bneo[0] = t

NEOPIXEL_LENGTH = 100
NEOPIXEL_INDEX = 0
NEOPIXEL_END_NDX = None
bneo = neopixel.NeoPixel(board.NEOPIXEL, 1, brightness = 0.1)
ring = neopixel.NeoPixel(board.D4, NEOPIXEL_LENGTH, brightness = 0.1)
neo = [0, 0, 0]


######################################################
# SONAR
######################################################
# https://www.sparkfun.com/products/13959
sonar = [None, None, None]
last_sonar = [monotonic(), monotonic(), monotonic()]
SONAR_MAX = 200

# Needs to remove trig, echo from the board.
def setup_sonar(ndx, trig, echo):
  global digital_pins, digital_objects, NUM_DIGITAL_PINS
  # If I try setting up a sonar on this location 
  # more than once, this will fail... so, try...
  trig = numberToBoardPin(trig)
  echo = numberToBoardPin(echo)
  try:
    # Get the index of those board pins in the 
    # board pin array.
    trigNdx = digital_pins.index(trig)
    # Us that index to deinit() those pins.
    digital_objects[trigNdx].deinit()
    # Remove them from the digital objects list.
    digital_pins.pop(trigNdx)
    digital_objects.pop(trigNdx)

    # Now, do the echo pin. Do these in order, because
    # I'm destructing the lists as I go.
    echoNdx = digital_pins.index(echo)
    # print("E ndx: {0}".format(echoNdx))
    digital_objects[echoNdx].deinit()
    digital_pins.pop(echoNdx)
    digital_objects.pop(echoNdx)
  
    # Update how many digital pins are in the list.
    NUM_DIGITAL_PINS = len(digital_pins)
  except:
    pass

  if sonar[ndx] is not None:
    sonar[ndx].deinit()
  sonar[ndx] = adafruit_hcsr04.HCSR04(trigger_pin=trig, echo_pin=echo)

def read_sonar(ndx):
  global sonar, last_sonar
  now = monotonic()
  if (now - last_sonar[ndx] > 0.05):
    last_sonar[ndx] = now
    # print("Reading again")
    try:
      return sonar[ndx].distance
    except RuntimeError:
      return None
 
######################################################
# MIDI PROTOCOL
######################################################
pins = [board.D5, board.D6, None, None, board.D9, board.D10, board.D11, board.D12, board.D13]
def numberToBoardPin(n):
  global pins
  if n < 5:
    return board.D5
  elif n > 13:
    return board.D13
  else:
    n = n - 5
    return pins[n]

def get_midi_msg():
  msg = midi.receive() 
  start = monotonic()
  while msg is None and ((monotonic() - start) < 0.01):
    msg = midi.receive() 
  return msg

# FIXME
# These should all be named constants. However, Python does not have constants.
# Need to think about how best to do that. Identity function? const() from MicroPython?
# HC-SRO4 SONAR
# 116 : Configure the sonar NUM on TRIG, ECHO
# 117 : Set sonar scaling distance (2 - 400)
## NEOPIXELS
# 118 : Set neopixel NDX, R, G, B as one 4-byte sequence
# 119 : Clear all neopixels on the strand.
# 120 : Set the index of the neopixel I'm talking to
# 121 : Pass. Reserved.
# 122 : Set the red value.
# 123 : Set the green value.
# 124 : Set the blue value.
# 125 : Reserved
# 126 : Set the number of pixels in the array
def midi_protocol (c, v):
  global ring, neo, bneo, NEOPIXEL_INDEX, NEOPIXEL_LENGTH, NEOPIXEL_END_NDX
  global SONAR_MAX
  update = False
  # print("MIDI C " + str(c) + " V " + str(v))
  
  ### 116
  # Set up sonar
  if (c == 116) and (v < 3):
    ndx = v
    trig = get_midi_msg().value
    echo = trig = get_midi_msg().value
    setup_sonar(ndx, trig, echo)
  ### 117
  # Set the sonar max ranging distance
  if (c == 117):
    if v < 2:
      SONAR_MAX = 2
    elif v > 400:
      SONAR_MAX = 400
    else:
      SONAR_MAX = v

  ### 118
  if c == 118:
    ntimeout = monotonic()
    # This is the start index.
    sndx = v
    # This is the end index.
    endx = get_midi_msg()
    if endx is not None:
      endx = endx.value
      # Now, I expect a color to come in.
      rgb = [0, 0, 0]
      rgbndx = 0
      # WARNING It seems to take approximately 0.048s to send the four bytes.
      # (That's a rough, eyeball, single-message estimate.)
      # I'm doubling that for the timeout value.
      while (((monotonic() - ntimeout) < 0.01) and rgbndx < 3):
        msg = get_midi_msg()
        if msg is not None:
          neo[rgbndx] = msg.value
          print("v {3} {2} {1} {0}".format(monotonic() - ntimeout, msg.value, rgbndx, sndx))
          rgbndx = rgbndx + 1
      # If things look good, I'll set the update flag.
      # This both sets the index to update as well as indicates
      # that I should use the neo[] list as a tuple() for the pixel.
      if rgbndx == 3:
        # print("UPDATE {0}".format(ndx))
        NEOPIXEL_INDEX = sndx
        NEOPIXEL_END_NDX = endx
        # for i in range(0, 3):
        #   # print("{0} {1}".format(i, rgb[i]))
        #   neo[i] = rgb[i]
        update = True

  ### 119
  # Clear everything
  if c == 119:
    t = (0, 0, 0)
    for i in range(0, NEOPIXEL_LENGTH):
      if i < NEOPIXEL_LENGTH:
        ring[i] = t
    # Flush some of the buffer. 
    for i in range(0, 20):
      midi.receive()
  ### 120
  # Set neopixel index
  elif c == 120:
    NEOPIXEL_INDEX = v % NEOPIXEL_LENGTH
    update = True
  ### 121
  # Pass
  elif c == 121:
    pass
  ### 122
  # Set Red
  elif c == 122:
    # print("RED")
    neo[0] = (v * 2)
    update = True
  ### 123
  # Set Blue
  elif c == 123:
    neo[1] = (v * 2)
    update = True
  ### 124
  # Set Green
  elif c == 124:
    neo[2] = (v * 2)
    update = True
  ### 125
  # Pass
  elif c == 125:
    pass
  ### 126
  # Set number of pixels in array.
  elif c == 126:
    NEOPIXEL_LENGTH = v
    ring.deinit()
    neo = [0, 0, 0]
    ring = neopixel.NeoPixel(board.D4, NEOPIXEL_LENGTH, brightness = 0.1)
  else:
    pass
  
  if update:
    t = tuple(neo)
    if NEOPIXEL_END_NDX:
      print("S {0} E {1}".format(NEOPIXEL_INDEX, NEOPIXEL_END_NDX))
      for i in range(NEOPIXEL_INDEX, NEOPIXEL_END_NDX):
        print("Setting {0} to {1}".format(i, t))
        if i < NEOPIXEL_LENGTH:
          ring[i] = t
      # This is a hack to allow a range of pixels to be set.
      NEOPIXEL_END_NDX = None
    bneo[0] = t

#########################################################
### SETUP                                             ###
#########################################################

###########################
# Clear the MIDI stream at startup
# I don't know who is talking to us. 
# I'll clear the queue. It might be 
# 30 messages. It might not. 
for i in range(0, 30):
  midi_msg = midi.receive()
setBoardPixel((0xA0, 0x00, 0xFF))

###########################
# FOREVER

while True:

  # First, read in all of the analog values.
  # Make sure they're 0-127.
  for ndx in range(0, NUM_ANALOG_PINS):
      # print(current_analog[ndx], end=' ')
      v = math.floor((analog_objects[ndx].value / 65535) * 127)
      # These go out on pins 7 through 13.
      midi.control_change(NUM_DIGITAL_PINS + ndx, abs(v))

  # Now, see if any digital pins changed.
  # I only send out updates for pins that have changed.
  if detect_digital_pin_change():
    for ndx in range(0, NUM_DIGITAL_PINS):
      if prev_digital[ndx] != current_digital[ndx]:
        # print(str(count) + " " + str(ndx))
        # I am pulling high, so send 0 when True and 1 when False
        if not current_digital[ndx]:
          midi.control_change(ndx, 1)
        else:
          midi.control_change(ndx, 0)
    # Make sure to update the pin state for all of the digital pins before 
    # dropping out of the loop. This is prev = current.
    update_state()

  # Check for MIDI input
  midi_msg = get_midi_msg()
  if midi_msg is not None:
    if isinstance(midi_msg, ControlChange):
      midi_protocol(midi_msg.control, midi_msg.value)
  
  # Now, service the sonar, if it exists.
  for i in range(len(sonar)):
    if sonar[i] is not None:
      reading = read_sonar(i)
      if reading is not None:
        # print("R{0}: {1}".format(i, reading))
        if reading < 2:
          reading = 2
        elif reading > SONAR_MAX:
          reading = SONAR_MAX
        scaled = int((reading / SONAR_MAX) * 127)
        # print("R{0}: {1}".format(i, scaled))
        midi.control_change(81, scaled)

  # If I have any UART data waiting, I should handle it.
  if uart.in_waiting > 0:
    START_FOUND = False
    TIMED_OUT = False
    # https://learn.adafruit.com/arduino-to-circuitpython/time
    start = monotonic()
    while (not START_FOUND) and (not TIMED_OUT):
      now = monotonic()

      if ((now - start > 0.005)):
        # print("TIMED OUT")
        TIMED_OUT = True
      data = uart.read(1)

      # If I find a start condition
      if (data is not None) and data == b'S':
        START_FOUND = True

    if START_FOUND and not TIMED_OUT:
      cc = fill(started = True)
      v = fill()
      if cc >= 0 and v >= 0 and cc <= 127 and v <= 127:
        midi.control_change(cc, abs(v))
