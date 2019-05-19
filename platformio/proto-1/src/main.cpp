#include "MIDIUSB.h"
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_BOARD_PIN 8
#define NEOPIXEL_RIBBON_PIN 4
#define NEOPIXEL_RIBBON_LENGTH 120
#define MIDI_CHANNEL 0
#define MIDI_PACKET_SIZE 16

// Define if I want to periodically send out all of the
// analog readings, even if they have not substantively change.
// #define PERIODIC_ANALOG_REFRESH 1

// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
Adafruit_NeoPixel board_neo(1, NEOPIXEL_BOARD_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ribbon_neo(NEOPIXEL_RIBBON_LENGTH, NEOPIXEL_RIBBON_PIN, NEO_GRB + NEO_KHZ800);

int analog_pins[] = {A0, A1, A2, A3, A4, A5};
int ANALOG_PIN_COUNT = sizeof(analog_pins) / sizeof(analog_pins[0]);
int *analog_readings = (int *)malloc(ANALOG_PIN_COUNT * sizeof(int));
int digital_pins[] = {5, 6, 9, 10, 11, 12, 13};
int DIGITAL_PIN_COUNT = sizeof(digital_pins) / sizeof(digital_pins[0]);
int *digital_readings = (int *)malloc(DIGITAL_PIN_COUNT * sizeof(int));
bool *digital_changed = (bool *)malloc(DIGITAL_PIN_COUNT * sizeof(bool));

typedef struct
{
  byte controller;
  byte value;
} MIDIControl;

// MIDI Packets
// We'll read in up to 16 bytes of information as a
// "packet" of information. It is declared at compile
// time as being limited in extend.
MIDIControl pkt[MIDI_PACKET_SIZE];
byte msg_count = 0;

void controlChange(byte channel, byte control, byte value)
{
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

// reset : void -> void
// Clears the entire pkt[] array, setting all values to
// 0xFF. This makes it a bit easier to see, as no MIDI
// value should be 255.
void reset()
{
  for (int ndx = 0; ndx < MIDI_PACKET_SIZE; ndx++)
  {
    pkt[ndx].controller = 0xFF;
    pkt[ndx].value = 0xFF;
  }
}

// Packet State Machine
// This machine keeps track of things as they come in.
enum PktRead
{
  START,
  HEADER,
  LENGTH,
  PKTS
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
        if (msg_length > MIDI_PACKET_SIZE)
        {
          state = START;
        }
        else
        {
          state = PKTS;
        }
        break;
      case PKTS:
        // FIXME: In theory, a timeout could be useful here.
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

// / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
// NEOPIXELS
void board(byte r, byte g, byte b)
{
  board_neo.setPixelColor(0, board_neo.Color(r / 2, g / 2, b / 2));
  board_neo.show();
}

void ribbon(byte ndx, byte r, byte g, byte b)
{
  ribbon_neo.setPixelColor(ndx, board_neo.Color(r / 2, g / 2, b / 2));
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

enum MsgTypes
{
  SET_BOARD_PIXEL = 1,
  SET_PIXEL_AT_INDEX = 2,
  SET_PIXEL_RANGE = 3
};

void interpret()
{
  if (is_valid_packet())
  {
    // Serial.println("INTERP");
    // The packet is:
    // [0] MSG TYPE
    // [1 ... ] Payload (per message type)
    switch (pkt[0].value)
    {
    case SET_BOARD_PIXEL:
      board(pkt[1].value, pkt[2].value, pkt[3].value);
      break;
    case SET_PIXEL_AT_INDEX:
      ribbon(pkt[1].value, pkt[2].value, pkt[3].value, pkt[4].value);
      break;
    case SET_PIXEL_RANGE:
      // Serial.println("SET_PIXEL_RANGE");
      int start = pkt[1].value;
      int end = pkt[2].value;
      // Serial.println("S " + String(start) + " E " + String(end));
      for (int ndx = start; ndx < end; ndx++)
      {
        ribbon(ndx, pkt[3].value, pkt[4].value, pkt[5].value);
      }
      ribbon_neo.show();
      break;
    }
  }
}

int scale_analog(int reading)
{
  // Get a number from zero to one.
  float tmp = reading / 1024.0;
  // Scale it to MIDI.
  tmp = tmp * 127.0;
  return (int)floor(tmp);
}


// void digitalPinChangeISR () {
//   int reading, ndx;
//   for (ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
//     reading = digitalRead(ndx);
//     if (reading != digital_readings[ndx]) {
//       digital_readings[ndx] = reading;
//       digital_changed[ndx] = true;
//     }
// }

// void process_pin_changes() {
//   int reading;
//   for (int ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
//     if (digital_changed[ndx]) {
//       digital_changed[ndx] = false;
//       reading = digitalRead(ndx);
//       digital_readings[ndx] = reading;
//       controlChange(0, ANALOG_PIN_COUNT + ndx, 1);
//     }
// }

int counter = 0;
MIDIControl rx;
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("ISA MIDI Interface");
  // Initialize Neopixels
  board_neo.begin();
  ribbon_neo.begin();

  // Flush the MIDI buffer, if there is one.
  for (int i = 0; i < 60; i++)
  {
    MidiUSB.read();
  }

  // Setup the analog inputs.
  // Do an initial read, so the array is not empty.
  for (int ndx = 0; ndx < ANALOG_PIN_COUNT; ndx++)
  {
    pinMode(analog_pins[ndx], INPUT);
    analog_readings[ndx] = analogRead(analog_pins[ndx]);
    delay(5);
  }

  for (int ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
  {
    pinMode(digital_pins[ndx], INPUT_PULLUP);
    // I need an initial state.
    //digital_readings[ndx] = digitalRead(digital_pins[ndx]);
    //digital_changed[ndx] = false;
    //attachInterrupt(digitalPinToInterrupt (ndx), digitalPinChangeISR, CHANGE);
  }
}

void process_midi_in()
{
  read_msgs();
  if (msg_count > 0)
  {
    // Assumes that pkt is global, and in good health.
    interpret();
  }
}

int analog_refresh_count = 0;

void process_analog_inputs()
{
  int reading;

  for (int ndx = 0; ndx < ANALOG_PIN_COUNT; ndx++)
  {
    reading = analogRead(analog_pins[ndx]);
    if ((reading > analog_readings[ndx] + 20) || (reading < analog_readings[ndx] - 20))
    {
      analog_readings[ndx] = reading;
      // Serial.println("A" + String(analog_pins[ndx]) + " " + String(reading));
      controlChange(0, ndx, scale_analog(analog_readings[ndx]));
    }
  }

#ifdef PERIODIC_ANALOG_REFRESH
  analog_refresh_count += 1;

  if ((analog_refresh_count % 10000) == 0)
  {
    analog_refresh_count = 0;
    for (int ndx = 0; ndx < ANALOG_PIN_COUNT; ndx++)
    {
      reading = analogRead(analog_pins[ndx]);
      analog_readings[ndx] = reading;
      controlChange(0, ndx, scale_analog(analog_readings[ndx]));
    }
  }
#endif
}

int debounce_clock = 0;
void initial_digital_read()
{
  int reading;
  if (debounce_clock == 0)
  {
    debounce_clock = millis();
    for (int ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
    {
      reading = digitalRead(digital_pins[ndx]);
      if (reading != digital_readings[ndx]) {
        digital_readings[ndx] = reading;
        controlChange(0, ndx + DIGITAL_PIN_COUNT, digital_readings[ndx]);
      }
    }
  }
}

void debounce_digital_read()
{
  int reading;
  int now = millis();

  if ((now - debounce_clock) > 50)
  {
    debounce_clock = 0;
    for (int ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
    {
      reading = digitalRead(digital_pins[ndx]);
      // Output if it is still the same.
      if (reading != digital_readings[ndx])
      {
        digital_readings[ndx] = reading;
        controlChange(0, ndx + DIGITAL_PIN_COUNT, digital_readings[ndx]);
      }
    }
  }
}

void process_digital_inputs() {
  int reading;
    for (int ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
    {
      reading = digitalRead(digital_pins[ndx]);
      if (reading != digital_readings[ndx])
      {
        digital_readings[ndx] = reading;
        controlChange(0, ndx + DIGITAL_PIN_COUNT, digital_readings[ndx]);
      }
    }
}

int pin_check_clock = millis();
void loop()
{
  process_midi_in();
  int now = millis();
  if ((now - pin_check_clock) > 50) {
    process_analog_inputs();
    //process_pin_changes();
    // initial_digital_read();
    // debounce_digital_read();
    process_digital_inputs();
    pin_check_clock = millis();
  }

  counter = counter + 1;
  if ((counter % 10000) == 0)
  {
    Serial.println(counter);
  }
}