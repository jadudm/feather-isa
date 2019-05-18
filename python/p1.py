import usb_midi, adafruit_midi
import board
from adafruit_midi.control_change          import ControlChange
from adafruit_midi.note_off                import NoteOff
from adafruit_midi.note_on                 import NoteOn
import neopixel 

m0 = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], midi_in=usb_midi.ports[0], out_channel=0, in_channel = 0)
m1 = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], midi_in=usb_midi.ports[0], out_channel=1, in_channel = 1)

bneo = neopixel.NeoPixel(board.NEOPIXEL, 1, brightness = 0.1)
bneo[0] = (127, 127, 0)

while True:
  msg0 = m0.receive() 
  if (msg0 is not None):
    if isinstance(msg0, ControlChange):
      print("M0 {0} {1}".format(msg0.control, msg0.value))
      bneo[0] = (127,0,0)
  
  msg1 = m1.receive()
  if (msg1 is not None):
    if isinstance(msg1, ControlChange):
      print("M1 {0} {1}".format(msg1.control, msg1.value))
      bneo[0] = (0, 0, 127)