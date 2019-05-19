#include "MIDIUSB.h"
#include "Adafruit_NeoPixel.h"
// NewPing Source
// http://bit.ly/2EjVV4D
// #include "NewPing.h"

#define NEOPIXEL_BOARD_PIN 8
#define NEOPIXEL_RIBBON_PIN 4
#define NEOPIXEL_RIBBON_LENGTH 120
#define MIDI_CHANNEL 0
#define MIDI_PACKET_SIZE 16
#define UART_PACKET_SIZE 16
#define PIN_CHECK_INTERVAL 50
#define MAX_SONARS 3
#define MAX_SONAR_DISTANCE 200

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
MIDIControl rx;
int pin_check_clock;

// UART Packets
typedef struct
{
  byte controller;
  byte value;
} UARTControl;

UARTControl uart_pkt[UART_PACKET_SIZE];

// // This is the only way to get the space allocated
// // statically. The pins then have to be overwritten?
// // There's ISRs that are introduced, too...
// NewPing sonars[] = {NewPing(5, 6, MAX_SONAR_DISTANCE),
//                     NewPing(9, 10, MAX_SONAR_DISTANCE),
//                     NewPing(11, 12, MAX_SONAR_DISTANCE)};
// int sonar_output_pins[] = {5, 9, 11};
// int sonar_enabled[] = {false, false, false};
// int sonar_distances[] = {0, 0, 0};

void controlChange(byte channel, byte control, byte value)
{
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

/*
    dMMMMb  dMMMMMP .dMMMb  dMMMMMP dMMMMMMP .dMMMb
   dMP.dMP dMP     dMP" VP dMP        dMP   dMP" VP
  dMMMMK" dMMMP    VMMMb  dMMMP      dMP    VMMMb
 dMP"AMF dMP     dP .dMP dMP        dMP   dP .dMP
dMP dMP dMMMMMP  VMMMP" dMMMMMP    dMP    VMMMP"

*/
// reset : void -> void
// Clears the entire pkt[] array, setting all values to
// 0xFF. This makes it a bit easier to see, as no MIDI
// value should be 255.
void reset_midi_pkts()
{
  for (int ndx = 0; ndx < MIDI_PACKET_SIZE; ndx++)
  {
    pkt[ndx].controller = 0xFF;
    pkt[ndx].value = 0xFF;
  }
}

void reset_uart_pkts()
{
  for (int ndx = 0; ndx < UART_PACKET_SIZE; ndx++)
  {
    uart_pkt[ndx].controller = 0xFF;
    uart_pkt[ndx].value = 0xFF;
  }
}

// void reset_sonars()
// {
//   for (int ndx = 0; ndx < MAX_SONARS; ndx++)
//   {
//     sonar_enabled[ndx] = false;
//     sonar_distances[ndx] = 0;
//   }
// }

/*
    dMMMMb  dMMMMMP .aMMMb  dMMMMb  dMMMMMMMMb  .dMMMb  .aMMMMP
   dMP.dMP dMP     dMP"dMP dMP VMP dMP"dMP"dMP dMP" VP dMP"
  dMMMMK" dMMMP   dMMMMMP dMP dMP dMP dMP dMP  VMMMb  dMP MMP"
 dMP"AMF dMP     dMP dMP dMP.aMP dMP dMP dMP dP .dMP dMP.dMP
dMP dMP dMMMMMP dMP dMP dMMMMP" dMP dMP dMP  VMMMP"  VMMMP"

*/

// Packet State Machine
// This machine keeps track of things as they come in.
enum PktRead
{
  START,
  HEADER,
  LENGTH,
  PKTS
};

bool read_msgs()
{
  midiEventPacket_t rx;
  int byte_count = 0;
  int msg_length = 0;
  PktRead state = START;
  reset_midi_pkts();
  bool is_good_packet = false;

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
        pkt[byte_count].controller = 0xBE;
        msg_length = rx.byte3;
        pkt[byte_count].value = rx.byte3;
        byte_count = byte_count + 1;

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
          // Include the CRC
          
          // Serial.println("Reading CRC");
          pkt[byte_count].controller = rx.byte2;
          pkt[byte_count].value = rx.byte3;
          // byte_count = byte_count + 1;
          is_good_packet = true;
          state = START;
        }
        break;
      }
    }
  } while (rx.header != 0);

  return is_good_packet;
}

/*
    dMMMMb  dMP dMP dMP dMMMMMP dMP    .dMMMb
   dMP.dMP amr dMK.dMP dMP     dMP    dMP" VP
  dMMMMP" dMP .dMMMK" dMMMP   dMP     VMMMb
 dMP     dMP dMP"AMF dMP     dMP    dP .dMP
dMP     dMP dMP dMP dMMMMMP dMMMMMP VMMMP"

*/
void board(byte r, byte g, byte b)
{
  board_neo.setPixelColor(0, board_neo.Color(r / 2, g / 2, b / 2));
  board_neo.show();
}

void ribbon(byte ndx, byte r, byte g, byte b)
{
  ribbon_neo.setPixelColor(ndx, board_neo.Color(r / 2, g / 2, b / 2));
}

/*
    dMMMMb  .aMMMb  .aMMMb  dMP dMP dMMMMMP dMMMMMMP .dMMMb
   dMP.dMP dMP"dMP dMP"VMP dMP.dMP dMP        dMP   dMP" VP
  dMMMMP" dMMMMMP dMP     dMMMMK" dMMMP      dMP    VMMMb
 dMP     dMP dMP dMP.aMP dMP"AMF dMP        dMP   dP .dMP
dMP     dMP dMP  VMMMP" dMP dMP dMMMMMP    dMP    VMMMP"

*/
void show_packet()
{
  for (int ndx = 0; ndx < MIDI_PACKET_SIZE; ndx++)
  {
    if (pkt[ndx].value != 0xFF)
    {
      Serial.print(String(ndx) + "[" + String(pkt[ndx].value) + "] ");
    }
  }
  Serial.println("");
}

bool is_valid_packet()
{
  int sum = 0;
  int localcrc = 0;
  int remcrc = 1;
  byte byte_count = 0;

  // show_packet();
  byte_count = pkt[0].value;

  for (int ndx = 1; ndx < byte_count; ndx++)
  {
    // Serial.println("SUM " + String(sum) + " V " + String(pkt[ndx].value));
    sum = sum + pkt[ndx].value;
  }
  // Serial.println("SUM " + String(sum));
  localcrc = sum % 128;
  remcrc   = pkt[byte_count].value;
  // Serial.println("MIDI CRC LOCAL " + String(localcrc) + " REM " + String(remcrc));
  if (remcrc == localcrc) {
    // Serial.println("CRC PASS");
  } else {
    // Serial.println("CRC FAIL");
  }
  return (remcrc == localcrc);
}

/*
    dMP dMMMMb dMMMMMMP dMMMMMP dMMMMb  dMMMMb  dMMMMb  dMMMMMP dMMMMMMP
   amr dMP dMP   dMP   dMP     dMP.dMP dMP.dMP dMP.dMP dMP        dMP
  dMP dMP dMP   dMP   dMMMP   dMMMMK" dMMMMP" dMMMMK" dMMMP      dMP
 dMP dMP dMP   dMP   dMP     dMP"AMF dMP     dMP"AMF dMP        dMP
dMP dMP dMP   dMP   dMMMMMP dMP dMP dMP     dMP dMP dMMMMMP    dMP

*/
enum MsgTypes
{
  SET_BOARD_PIXEL = 1,
  SET_PIXEL_AT_INDEX = 2,
  SET_PIXEL_RANGE = 3,
  SETUP_SONAR = 4,
};

void interpret()
{
  if (is_valid_packet())
  {
    // Serial.println("INTERP");
    // The packet is:
    // [0] MSG TYPE
    // [1 ... ] Payload (per message type)
    switch (pkt[1].value)
    {
    case SET_BOARD_PIXEL:
    {  
      byte r = pkt[2].value;
      byte g = pkt[3].value;
      byte b = pkt[4].value;
      // Serial.println("SET_BOARD_PIXEL " + String(r, HEX) + " " 
      //                 + String(g, HEX) + " "
      //                 + String(b, HEX));
      board(r, g, b);
      }
      break;

    case SET_PIXEL_AT_INDEX:
      ribbon(pkt[2].value, pkt[3].value, pkt[4].value, pkt[5].value);
      break;

    case SET_PIXEL_RANGE:
    {
      // Serial.println("SET_PIXEL_RANGE");
      int start = pkt[2].value;
      int end = pkt[3].value;
      // Serial.println("S " + String(start) + " E " + String(end));
      for (int ndx = start; ndx < end; ndx++)
      {
        ribbon(ndx, pkt[4].value, pkt[5].value, pkt[6].value);
      }
      ribbon_neo.show();
    }
    break;

    case SETUP_SONAR:
    {
      // int ndx = pkt[1].value;
      // Serial.println("SONAR SETUP " + String(ndx));
      // // All pins are defaulted to input.
      // // Flip the trigger back to output.
      // // pinMode(sonar_output_pins[ndx], OUTPUT);
      // sonars[ndx] = NewPing(9, 10, 200);
      // sonar_enabled[ndx] = true;
    }
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

void setup_digital_pins()
{
  for (int ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
  {
    pinMode(digital_pins[ndx], INPUT_PULLUP);
    // I need an initial state.
    digital_readings[ndx] = digitalRead(digital_pins[ndx]);
  }
}

void setup_analog_pins()
{
  // Setup the analog inputs.
  // Do an initial read, so the array is not empty.
  for (int ndx = 0; ndx < ANALOG_PIN_COUNT; ndx++)
  {
    pinMode(analog_pins[ndx], INPUT);
    analog_readings[ndx] = analogRead(analog_pins[ndx]);
    delay(5);
  }
}


/*
   .dMMMb  dMMMMMP dMMMMMMP dMP dMP dMMMMb
  dMP" VP dMP        dMP   dMP dMP dMP.dMP
  VMMMb  dMMMP      dMP   dMP dMP dMMMMP"
dP .dMP dMP        dMP   dMP.aMP dMP
VMMMP" dMMMMMP    dMP    VMMMP" dMP

*/
void setup()
{
  // The USB
  Serial.begin(115200);
  // The Micro:Bit
  Serial1.begin(115200);

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
  // Pins
  // They get checked every PIN_CHECK_INTERVAL.
  // This is the start of that clock.
  pin_check_clock = millis();
  setup_analog_pins();
  setup_digital_pins();
  // Setup the sonars.
  // reset_sonars();
}

/*
    dMMMMb  dMMMMb  .aMMMb  .aMMMb  dMMMMMP .dMMMb  .dMMMb
   dMP.dMP dMP.dMP dMP"dMP dMP"VMP dMP     dMP" VP dMP" VP
  dMMMMP" dMMMMK" dMP dMP dMP     dMMMP    VMMMb   VMMMb
 dMP     dMP"AMF dMP.aMP dMP.aMP dMP     dP .dMP dP .dMP
dMP     dMP dMP  VMMMP"  VMMMP" dMMMMMP  VMMMP"  VMMMP"

*/
void process_midi_in()
{
  bool is_good_pkt;

  is_good_pkt = read_msgs();
  if (is_good_pkt)
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
    // +/- 20 on a 1024 scale.
    if ((reading > analog_readings[ndx] + 20) || (reading < analog_readings[ndx] - 20))
    {
      analog_readings[ndx] = reading;
      // Serial.println("A" + String(analog_pins[ndx]) + " " + String(reading));
      controlChange(0, ndx, scale_analog(analog_readings[ndx]));
    }
  }
}

void process_digital_inputs()
{
  int reading;
  for (int ndx = 0; ndx < DIGITAL_PIN_COUNT; ndx++)
  {
    // The pins may have been taken over by a sonar setup.
    // This means I'll set that index to -1.
    // It can no longer be read as a digital pin.
    if (digital_pins[ndx] > 0)
    {
      reading = digitalRead(digital_pins[ndx]);
      if (reading != digital_readings[ndx])
      {
        digital_readings[ndx] = reading;
        controlChange(0, ndx + DIGITAL_PIN_COUNT, digital_readings[ndx]);
      }
    }
  }
}

// Bad stuck sensor?
// http://bit.ly/30uhVn6
// void process_sonars()
// {
//   for (int ndx = 0; ndx < MAX_SONARS; ndx++)
//   {
//     if (sonar_enabled[ndx] == true)
//     {
//       int dist = sonars[ndx].ping_cm();
//       if (dist != sonar_distances[ndx])
//       {
//         sonar_distances[ndx] = dist;
//         Serial.println("S[" + String(ndx) + "] " + String(dist));
//         controlChange(0, 20, dist);
//       }
//     }
//   }
// }

/*
   dMP dMP .aMMMb  dMMMMb dMMMMMMP
  dMP dMP dMP"dMP dMP.dMP   dMP
 dMP dMP dMMMMMP dMMMMK"   dMP
dMP.aMP dMP dMP dMP"AMF   dMP
VMMMP" dMP dMP dMP dMP   dMP

*/
// Serial1 on the Feather M4.
enum UARTPkt
{
  UART_START,
  UART_HEADER,
  UART_PKT_LENGTH,
  UART_PKT,
  UART_CRC
};

UARTPkt uart_state = UART_START;

const char startOfNumberDelimiter = '<';
const char endOfNumberDelimiter = '>';

byte read_uart_byte2()
{
  byte b = (byte)Serial1.read();
  if (b != 0xFF)
  {
    Serial.println("B " + String(b, HEX) + " " + String(char(b)));
  }
  return b;
}

byte read_uart_byte()
{
  byte b = 0xFF;
  do {
    if (Serial1.peek() >= 0xFF) {
      Serial1.read();
    } else {
      b = Serial1.read();
    }
  } while (b == 0xFF);

  // Serial.println("B " + String(b, HEX) + " " + String(char(b)));
  return b;
}

/*
   dMP dMP .aMMMb  dMMMMb dMMMMMMP         dMMMMb  dMP dMP dMMMMMMMMb
  dMP dMP dMP"dMP dMP.dMP   dMP           dMP dMP dMP dMP dMP"dMP"dMP
 dMP dMP dMMMMMP dMMMMK"   dMP           dMP dMP dMP dMP dMP dMP dMP
dMP.aMP dMP dMP dMP"AMF   dMP           dMP dMP dMP.aMP dMP dMP dMP
VMMMP" dMP dMP dMP dMP   dMP           dMP dMP  VMMMP" dMP dMP dMP

*/
int read_number()
{
  int receivedNumber = 0;
  bool negative = false;
  int counter = 0;
  // Serial.println("read_number()");

  do
  {
    byte c = read_uart_byte();
    counter += 1;

    switch (c)
    {
    case 0xFF:
      // Don't count these.
      counter = counter - 1;
      // Where does the M:B get this garbage?
      break;
    case endOfNumberDelimiter:
      if (negative)
      {
        // Serial.println("REC# -" + String(receivedNumber));
        return (-receivedNumber);
      }
      else
      {
        // Serial.println("REC# " + String(receivedNumber));
        return (receivedNumber);
      }
      break;

    case '0' ... '9':
      receivedNumber *= 10;
      receivedNumber += c - '0';
      break;

    case '-':
      negative = true;
      break;

    } // end of switch

  } while (counter < 6);

  return -1;
}

/*
   dMP dMP .aMMMb  dMMMMb dMMMMMMP         dMMMMMMMMb  .dMMMb  .aMMMMP
  dMP dMP dMP"dMP dMP.dMP   dMP           dMP"dMP"dMP dMP" VP dMP"
 dMP dMP dMMMMMP dMMMMK"   dMP           dMP dMP dMP  VMMMb  dMP MMP"
dMP.aMP dMP dMP dMP"AMF   dMP           dMP dMP dMP dP .dMP dMP.dMP
VMMMP" dMP dMP dMP dMP   dMP           dMP dMP dMP  VMMMP"  VMMMP"

*/
bool read_uart_msg()
{
  byte b;
  byte length = 0;
  byte count = 0;
  bool uart_done = false;
  bool good_packet = false;

  int start = millis();

  do
  {
    int now = millis();
    if ((now - start) > 90)
    {
      Serial.println("TIMED OUT UART MSG");
      good_packet = false;
      break;
    }

    // if (Serial1.peek() == 0xFF)
    // {
    //   Serial1.read();
    // }

    switch (uart_state)
    {
    case UART_START:
    {
      b = read_uart_byte();
      if (b == 0x2A)
      {
        // Serial.println("-> U HEADER");
        uart_state = UART_HEADER;
      }
    }
    break;

    case UART_HEADER:
    {
      b = read_uart_byte();
      if (b == 0x2B)
      {
        // Serial.println("-> U LEN");
        uart_state = UART_PKT_LENGTH;
      }
      else
      {
        // Serial.println("-> U START");
        uart_state = UART_START;
      }
    }
    break;

    case UART_PKT_LENGTH:
    {
      length = read_number();
      uart_pkt[count].value = length;
      count += 1;

      // Serial.println("LEN: " + String(length));
      // FIXME: Should be less than max length.
      // Serial.println("-> U PKT");
      uart_state = UART_PKT;
    }
    break;

    case UART_PKT:
      if (count < length)
      {
        uart_pkt[count].value = read_number();
        count = count + 1;
        // Serial.println("-> PKT CTD");
      }
      else
      {
        // Serial.println("-> READ CRC");
        uart_pkt[count].value = read_number();
        // Serial.println("-> U START");
        uart_state = UART_START;
        good_packet = true;
        uart_done = true;
      }
      break;
    }
  } while (!uart_done);

  return good_packet;
}

void show_uart_packet()
{
  for (int ndx = 0; ndx < UART_PACKET_SIZE; ndx++)
  {
    if (uart_pkt[ndx].value != 0xFF)
    {
      Serial.print(String(ndx) + "[" + String(uart_pkt[ndx].value) + "] ");
    }
  }
  Serial.println("");
}

bool check_uart_packet()
{
  int crc = 0;
  int length = uart_pkt[0].value;

  show_uart_packet();
  if (!(uart_pkt[0].value == 0x2A) && (uart_pkt[1].value == 0x2B))
  {
    Serial.println("HEADER DOES NOT CHECK");
    return false;
  }
  else
  {
    for (int ndx = 1; ndx <= length - 1; ndx++)
    {

      // Serial.println("SUMMING " + String(uart_pkt[ndx].value));
      crc += uart_pkt[ndx].value;
    }
    crc = crc % 128;
    // Serial.println("CUP CALC " + String(crc));

    int rcvcrc = uart_pkt[length].value;

    Serial.println("CUP CRC " + String(crc) + " RCV " + rcvcrc);
    return (crc == rcvcrc);
  }
}

// FIXME: UART packets carry their length.
// MIDI packets do not. 
// Need to include the length in the MIDI packets 
// so these are normalized...
void interpret_uart() {
  for (int ndx = 0 ; ndx < UART_PACKET_SIZE; ndx++) {
    pkt[ndx].controller = uart_pkt[ndx].controller;
    pkt[ndx].value = uart_pkt[ndx].value;
  }
  interpret();
}

void process_uart()
{
  if (Serial1.available())
  {
    reset_uart_pkts();
    bool is_good = read_uart_msg();
    if (is_good && check_uart_packet())
    {
      Serial.println("GOOD UART PKT");
      interpret_uart();
    }
  }
}


/*
    dMP    .aMMMb  .aMMMb  dMMMMb
   dMP    dMP"dMP dMP"dMP dMP.dMP
  dMP    dMP dMP dMP dMP dMMMMP"
 dMP    dMP.aMP dMP.aMP dMP
dMMMMMP VMMMP"  VMMMP" dMP

*/
int counter = 0;
void loop()
{
  // Process the MIDI
  process_midi_in();
  process_uart();

  // Process the analog and digital pins.
  int now = millis();
  if ((now - pin_check_clock) > PIN_CHECK_INTERVAL)
  {
    process_analog_inputs();
    process_digital_inputs();
    //process_sonars();
    pin_check_clock = millis();
  }

  // Just so I know we're alive.
  counter = counter + 1;
  if ((counter % 100000) == 0)
  {
    Serial.println(counter);
  }
}