#include "MIDIUSB.h"
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_BOARD_PIN 8
#define MIDI_CHANNEL 0
#define MIDI_PACKET_SIZE 8

Adafruit_NeoPixel board_neo(1, NEOPIXEL_BOARD_PIN, NEO_GRB + NEO_KHZ800);

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

MIDIControl pkt[MIDI_PACKET_SIZE];
byte msg_count = 0;

void reset()
{
  for (int ndx = 0; ndx < MIDI_PACKET_SIZE; ndx++)
  {
    pkt[ndx].controller = 0xFF;
    pkt[ndx].value = 0xFF;
  }
}

enum PktRead
{
  START,
  HEADER,
  LENGTH,
  PKTS,
  CRC
};

void read_msgs()
{
  midiEventPacket_t rx;
  int byte_count = 0;
  int msg_length = 0;
  msg_count = 0;
  PktRead state = START;
  reset();
  do
  {
    rx = MidiUSB.read();
    if (rx.header != 0)
    {
      // Serial.print("RCV: ");
      // Serial.print(rx.header, HEX);
      // Serial.print("-");
      // Serial.print(rx.byte1, HEX);
      // Serial.print(" C ");
      // Serial.print(rx.byte2, HEX);
      // Serial.print(" V ");
      // Serial.println(rx.byte3, HEX);

      switch (state)
      {
      case START:
        if (rx.byte3 == 0x2A)
        {
          // Serial.println("-> HEADER");
          state = HEADER;
        }
        break;
      case HEADER:
        if (rx.byte3 == 0x2B)
        {
          // Serial.println("-> LENGTH");
          state = LENGTH;
        }
        else
        {
          // Serial.println("-> START");
          state = START;
        }
        break;
      case LENGTH:
        msg_length = rx.byte3;
        // Serial.println("LENGTH: " + String(msg_length));
        // Serial.println("-> PKTS");
        state = PKTS;
        break;
      case PKTS:
        if (byte_count < msg_length)
        {
          // Serial.println(" R " + String(byte_count) + " " + rx.byte3);
          pkt[byte_count].controller = rx.byte2;
          pkt[byte_count].value = rx.byte3;
          byte_count = byte_count + 1;
        }
        else
        {
          // This reads the CRC
          // Serial.println("Reading CRC");
          byte_count = byte_count + 1;
          pkt[byte_count].controller = rx.byte2;
          pkt[byte_count].value = rx.byte3;
          // Include the CRC
          msg_count = msg_length + 1;
          state = START;
        }
        break;
      }
    }
  } while (rx.header != 0);
}

void board(byte r, byte g, byte b)
{
  board_neo.setPixelColor(0, board_neo.Color(r / 2, g / 2, b / 2));
  board_neo.show();
}

bool is_valid_packet()
{
  int sum = 0;
  int crc = 0;
  for (int ndx = 0; ndx < (msg_count - 1); ndx++)
  {
    // Serial.println("SUM " + String(sum) + " V " + String(pkt[ndx].value));
    sum = sum + pkt[ndx].value;
  }
  // Serial.println("SUM " + String(sum));
  crc = sum % 128;
  // Serial.println("COMPARE LOCAL " + String(crc) + " REM " + String(pkt[msg_count].value));
  return (pkt[msg_count].value == crc);
}

enum MsgTypes {
  SET_BOARD_PIXEL = 1
  };

void interpret()
{
  if (is_valid_packet())
  {
    Serial.println("INTERP");

    // The packet is:
    // [0] MSG TYPE
    // [1 ... ] Payload (per message type)
    switch(pkt[0].value) {
      case SET_BOARD_PIXEL:
        board(pkt[1].value, pkt[2].value, pkt[3].value);
      break;
    }

  }
}

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
  read_msgs();
  if (msg_count > 0)
  {
    Serial.println("MSGS " + String(msg_count));
    for (int ndx = 0; ndx < msg_count; ndx++)
    {
      //Serial.println(" NDX " + String(ndx) + " C " + String(pkt[ndx].controller) + " V " + String(pkt[ndx].value));
    }
    // Assumes that pkt is global, and in good health.
    interpret();
  }

  counter = counter + 1;
  if ((counter % 1000000) == 0)
  {
    Serial.println(counter);
  }
}