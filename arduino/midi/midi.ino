#include "MIDIUSB.h"
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_BOARD_PIN     8
#define MIDI_CHANNEL           0

Adafruit_NeoPixel board_neo(1, NEOPIXEL_BOARD_PIN, NEO_GRB + NEO_KHZ800);


enum States_Enum {START, SET_BOARD_PIXEL, THREE};
uint8_t state = START;

typedef struct {
  byte controller;
  byte value;
} MIDIControl;

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("ISA MIDI Interface");
  board_neo.begin();
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

void read_msg (MIDIControl *cc) {
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read();
    if (rx.header != 0) {
      Serial.print("Received: ");
      Serial.print(rx.header, HEX);
      Serial.print("-");
      Serial.print(rx.byte1, HEX);
      Serial.print("-");
      Serial.print(rx.byte2, HEX);
      Serial.print("-");
      Serial.println(rx.byte3, HEX);

      cc->controller = rx.byte2;
      cc->value      = rx.byte3;
    }
  } while (rx.header != 0);

}

bool check(MIDIControl *mc, byte controller, byte value) {
  return (mc->controller == controller) && (mc->value == value);
}


bool board(byte r, byte g, byte b) {
  board_neo.setPixelColor(0, board_neo.Color(r / 2, g / 2, b / 2));
  board_neo.show(); 
}

void run_machine(MIDIControl *mc) {
  switch (state) {
    case START:
      board(0x00, 0x00, 0x00);
      // Serial.println("START");
      if (check(mc, 0x00, 0x2A)) {
        // Serial.println("controller: " + String(mc->controller, HEX) + " value: " + String(mc->value));
        
        state = SET_BOARD_PIXEL;
      }
      break;

    case SET_BOARD_PIXEL:
      if (check(mc, 0x00, 0x2B)) {
        board(0x00, 0xFF, 0x00);
        state = THREE;
      } else {
        board(0x00, 0x00, 0xFF);
        state = START;
      }
      break;

   case THREE:
    board(0x00, 0xFF, 0xFF);
    state = START;
    break;
  }
}

void reset(MIDIControl *rx) {
  rx->controller = 0xFF;
  rx->value      = 0xFF;
}

int counter = 0;
MIDIControl rx;

void loop() {

  reset(&rx);
  read_msg(&rx);
  
  if (rx.controller != 0xFF) {
    Serial.println("controller: " + String(rx.controller, HEX) + " value: " + String(rx.value));
    run_machine(&rx);
    reset(&rx);
  }

  if ((counter % 100000) == 0) {
    //Serial.println(counter);
  }
  counter = counter + 1;
}
