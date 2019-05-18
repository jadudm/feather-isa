#include "MIDIUSB.h"
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_BOARD_PIN 8
#define MIDI_CHANNEL 0

Adafruit_NeoPixel board_neo(1, NEOPIXEL_BOARD_PIN, NEO_GRB + NEO_KHZ800);

enum States_Enum
{
  START,
  NEXT,
  READ_MSG,
  PX_R,
  PX_G,
  PX_B,
  UPDATE_BOARD_PIXEL
};
enum PktEnum
{
  PKT_START,
  P1,
  P2,
  P3, 
  P4,
  P5
};

uint8_t state = START;
PktEnum pkt_state = PKT_START;

typedef struct
{
  byte controller;
  byte value;
} MIDIControl;

void controlChange(byte channel, byte control, byte value)
{
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}


void reset(MIDIControl *mc)
{
  mc->controller = 0xFF;
  mc->value = 0xFF;
}

void read_msg(MIDIControl *mc)
{
  midiEventPacket_t rx;
  reset(mc);
  do
  {
    rx = MidiUSB.read();
    if (rx.header != 0)
    {
      Serial.print("Received: ");
      Serial.print(rx.header, HEX);
      Serial.print("-");
      Serial.print(rx.byte1, HEX);
      Serial.print("-");
      Serial.print(rx.byte2, HEX);
      Serial.print("-");
      Serial.println(rx.byte3, HEX);

      mc->controller = rx.byte2;
      mc->value      = rx.byte3;
    }
  } while (rx.header != 0);
}

bool check(MIDIControl *mc, byte controller, byte value)
{
  return (mc->controller == controller) && (mc->value == value);
}

void board(byte r, byte g, byte b)
{
  board_neo.setPixelColor(0, board_neo.Color(r / 2, g / 2, b / 2));
  board_neo.show();
}

#define PS(sym)            \
  case sym:                \
    Serial.print("" #sym); \
    break
void print_state(int n)
{
  switch (n)
  {
    PS(START);
    PS(NEXT);
    PS(READ_MSG);
    PS(PX_R);
    PS(PX_G);
    PS(PX_B);
  }
}
void update(int state, MIDIControl *mc)
{
  Serial.print("State ");
  print_state(state);
  Serial.println(" CC " + String(mc->controller, HEX) + " V  " + String(mc->value, HEX));
}

void run_machine(MIDIControl *mc)
{
  switch (state)
  {
  // STATE: START -> NEXT
  // To begin a message, we should get a 42 then 43 on CC 0.
  case START:
    Serial.println("START");
    update(START, mc);
    if (check(mc, 0x00, 0x2A))
    {
      Serial.println("-> NEXT");
      state = NEXT;
    }
    break;
  case NEXT:
    Serial.println("NEXT");
    update(NEXT, mc);
    if (check(mc, 0x00, 0x2B))
    {
      Serial.println("-> READ_MSG");
      state = READ_MSG;
    }
    else
    {
      state = START;
    }
    break;

  // STATE: READ_MSG
  // If we received our preamble, then CC 1 should be where
  // interesting data comes in.
  case READ_MSG:
    update(READ_MSG, mc);
    // Set board pixel?
    if (check(mc, 0x01, 0x00))
    {
      Serial.println("-> PX_R");
      state = PX_R;
    }
    else
    {
      state = START;
    }
    break;

    byte pix[3];
  case PX_R:
    update(PX_R, mc);
    pix[0] = mc->value;
    state = PX_G;
    break;
  case PX_G:
    update(PX_G, mc);
    pix[1] = mc->value;
    state = PX_B;
    break;
  case PX_B:
    update(PX_B, mc);
    pix[2] = mc->value;
    state = UPDATE_BOARD_PIXEL;
    break;
  case UPDATE_BOARD_PIXEL:
    update(UPDATE_BOARD_PIXEL, mc);
    for (int i = 0; i < 3; i++)
    {
      Serial.println("\t pix[" + String(i) + "] " + String(pix[i]));
    }
    board(pix[0], pix[1], pix[2]);
    state = START;
    break;
  }
}

byte packet[5];
bool read_packet(MIDIControl *mc)
{
  switch (pkt_state)
  {
  case PKT_START:
    if (check(mc, 0x00, 0x2A))
    {
      Serial.println("-> P1");
      pkt_state = P1;
    } else { pkt_state = PKT_START; }
    break;
  case P1: 
    if (mc->controller == 0x01) {
      Serial.println("-> P2");
      packet[0] = mc->value;
      pkt_state = P2;
    } else if (check(mc, 0x00, 0x24)) {
      pkt_state = PKT_START;
      return 1;
    } else { pkt_state = PKT_START; }
    break;
  case P2:
    if (mc->controller == 0x01) {
      Serial.println("-> P3");
      packet[1] = mc->value;
      pkt_state = P3;
    } else if (check(mc, 0x00, 0x24)) {
      pkt_state = PKT_START;
      return 1;
    } else { pkt_state = PKT_START; }
    break;
  case P3:
    if (mc->controller == 0x01) {
      Serial.println("-> P4");
      packet[2] = mc->value;
      pkt_state = P4;
    } else if (check(mc, 0x00, 0x24)) {
      pkt_state = PKT_START;
      return 1;
    } else { pkt_state = PKT_START; }
    break;
  case P4:
    if (mc->controller == 0x01) {
      Serial.println("-> P5");
      packet[4] = mc->value;
      pkt_state = P5;
    } else if (check(mc, 0x00, 0x24)) {
      pkt_state = PKT_START;
      return 1;
    } else { pkt_state = PKT_START; }
    break;
  case P5:
    if (mc->controller == 0x01) {
      Serial.println("-> DONE; RESET");
      packet[4] = mc->value;
      pkt_state = PKT_START;
      return 1;
    } else if (check(mc, 0x00, 0x24)) {
      pkt_state = PKT_START;
      return 1;
    } else { pkt_state = PKT_START; }
    break;
  }

  return 0;
}

int counter = 0;
MIDIControl rx;
void setup()
{
  delay(1000);
  Serial.begin(115200);
  Serial.println("ISA MIDI Interface");
  board_neo.begin();
  // Flush the MIDI buffer
  for (int i = 0; i < 60; i++)
  {
    MidiUSB.read();
  }
}

void loop()
{
  bool is_pkt = false;
  read_msg(&rx);
  if (rx.controller != 0xFF)
  {
    //Serial.println("controller: " + String(rx.controller, HEX) + " value: " + String(rx.value));
    is_pkt = read_packet(&rx);
    reset(&rx);
  }
 
  if (is_pkt) {
    Serial.println("IS PACKET");
  }
  if ((counter % 1000000) == 0)
  {
    Serial.println(counter);
  }
  counter = counter + 1;
}