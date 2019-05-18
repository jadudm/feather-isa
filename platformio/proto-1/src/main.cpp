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

MIDIControl packet[5];
byte msgs = 0;
void read_msg()
{
  midiEventPacket_t rx;
  msgs = 0;
  for (int ndx = 0; ndx < 5; ndx++)
  {
    reset(&packet[ndx]);
  }
  do
  {
    rx = MidiUSB.read();
    if (rx.header != 0)
    {
      Serial.print("RCV: ");
      Serial.print(rx.header, HEX);
      Serial.print("-");
      Serial.print(rx.byte1, HEX);
      Serial.print(" C ");
      Serial.print(rx.byte2, HEX);
      Serial.print(" V ");
      Serial.println(rx.byte3, HEX);

      if (rx.byte2 != 0xFF) {
        packet[msgs].controller = rx.byte2;
        packet[msgs].value = rx.byte3;
        msgs = msgs + 1;
      }
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

// bool read_packet(MIDIControl *mc)
// {
//   Serial.println("read_packet C " + String(mc->controller, HEX) + " V " + String(mc->value));
//   switch (pkt_state)
//   {
//   case PKT_START:
//     if (check(mc, 0x00, 0x2A))
//     {
//       Serial.println("-> P1");
//       pkt_state = P1;
//     } else { pkt_state = PKT_START; }
//     break;
//   case P1:
//     if (mc->controller == 0x01) {
//       Serial.println("-> P2");
//       packet[0] = mc->value;
//       pkt_state = P2;
//     } else if (check(mc, 0x00, 0x24)) {
//       pkt_state = PKT_START;
//       return 1;
//     } else { pkt_state = PKT_START; }
//     break;
//   case P2:
//     if (mc->controller == 0x01) {
//       Serial.println("-> P3");
//       packet[1] = mc->value;
//       pkt_state = P3;
//     } else if (check(mc, 0x00, 0x24)) {
//       pkt_state = PKT_START;
//       return 1;
//     } else { pkt_state = PKT_START; }
//     break;
//   case P3:
//     if (mc->controller == 0x01) {
//       Serial.println("-> P4");
//       packet[2] = mc->value;
//       pkt_state = P4;
//     } else if (check(mc, 0x00, 0x24)) {
//       pkt_state = PKT_START;
//       return 1;
//     } else { pkt_state = PKT_START; }
//     break;
//   case P4:
//     if (mc->controller == 0x01) {
//       Serial.println("-> P5");
//       packet[4] = mc->value;
//       pkt_state = P5;
//     } else if (check(mc, 0x00, 0x24)) {
//       pkt_state = PKT_START;
//       return 1;
//     } else { pkt_state = PKT_START; }
//     break;
//   case P5:
//     if (mc->controller == 0x01) {
//       Serial.println("-> DONE; RESET");
//       packet[4] = mc->value;
//       pkt_state = PKT_START;
//       return 1;
//     } else if (check(mc, 0x00, 0x24)) {
//       pkt_state = PKT_START;
//       return 1;
//     } else { pkt_state = PKT_START; }
//     break;
//   }

//   return 0;
// }

int counter = 0;
MIDIControl rx;
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("ISA MIDI Interface");
  board_neo.begin();
  // Flush the MIDI buffer, if there is one.
  for (int i = 0; i < 60; i++)
  {
    MidiUSB.read();
  }
}

void loop()
{
  bool is_pkt = false;
  read_msg();

  if (msgs != 0)
  {
    Serial.println("MSGS " + String(msgs));
    for (int ndx = 0; ndx < msgs; ndx++)
    {
      Serial.println("\tC " + String(packet[ndx].controller) + " V " + String(packet[ndx].value));
    }
  }

  if (rx.controller != 0xFF)
  {

    //is_pkt = read_packet(&rx);
  }

  if (is_pkt)
  {
    Serial.println("IS PACKET");
  }

  counter = counter + 1;
  if ((counter % 1000000) == 0)
  {
    Serial.println(counter);
  }
}