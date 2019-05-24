
#include "types.h"
#include "midi_machine.h"
#include "neopixels.h"
#include "interpreter.h"

// NewPing Source
// http://bit.ly/2EjVV4D
// #include "NewPing.h"


#define UART_PACKET_SIZE 16 //FIXME
#define PIN_CHECK_INTERVAL 50
#define MAX_SONARS 3
#define MAX_SONAR_DISTANCE 200

#define SERIAL0_SPEED 115200
#define SERIAL1_SPEED 115200
#define SERIAL1_TIMEOUT 18

// Define if I want to periodically send out all of the
// analog readings, even if they have not substantively change.
// #define PERIODIC_ANALOG_REFRESH 1


int analog_pins[] = {A0, A1, A2, A3, A4, A5};
int ANALOG_PIN_COUNT = sizeof(analog_pins) / sizeof(analog_pins[0]);
int *analog_readings = (int *)malloc(ANALOG_PIN_COUNT * sizeof(int));
int digital_pins[] = {5, 6, 9, 10, 11, 12, 13};
int DIGITAL_PIN_COUNT = sizeof(digital_pins) / sizeof(digital_pins[0]);
int *digital_readings = (int *)malloc(DIGITAL_PIN_COUNT * sizeof(int));
bool *digital_changed = (bool *)malloc(DIGITAL_PIN_COUNT * sizeof(bool));

int pin_check_clock;

CV uart_pkt[UART_PACKET_SIZE];

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

void reset_uart_pkts()
{
  for (int ndx = 0; ndx < UART_PACKET_SIZE; ndx++)
  {
    uart_pkt[ndx].controller = 0xFF;
    uart_pkt[ndx].value = 0xFF;
  }
}

/*
    dMMMMb  .aMMMb  .aMMMb  dMP dMP dMMMMMP dMMMMMMP .dMMMb
   dMP.dMP dMP"dMP dMP"VMP dMP.dMP dMP        dMP   dMP" VP
  dMMMMP" dMMMMMP dMP     dMMMMK" dMMMP      dMP    VMMMb
 dMP     dMP dMP dMP.aMP dMP"AMF dMP        dMP   dP .dMP
dMP     dMP dMP  VMMMP" dMP dMP dMMMMMP    dMP    VMMMP"

*/

/*
    dMP dMMMMb dMMMMMMP dMMMMMP dMMMMb  dMMMMb  dMMMMb  dMMMMMP dMMMMMMP
   amr dMP dMP   dMP   dMP     dMP.dMP dMP.dMP dMP.dMP dMP        dMP
  dMP dMP dMP   dMP   dMMMP   dMMMMK" dMMMMP" dMMMMK" dMMMP      dMP
 dMP dMP dMP   dMP   dMP     dMP"AMF dMP     dMP"AMF dMP        dMP
dMP dMP dMP   dMP   dMMMMMP dMP dMP dMP     dMP dMP dMMMMMP    dMP

*/

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
  Serial.begin(SERIAL0_SPEED);
  // The Micro:Bit
  Serial1.begin(SERIAL1_SPEED);

  delay(1000);

  // Init Neopixels
  board_init();
  ribbon_init();
  
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

  is_good_pkt = read_midi_message();
  if (is_good_pkt && is_valid_midi_buffer())
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
  UART_CRC,
  UART_TERM
};

const char startOfNumberDelimiter = '<';
const char endOfNumberDelimiter = '>';

bool unwanted_byte(byte b)
{
  return ((b >= 0xFF));
}

byte read_uart_byte()
{
  int b = 0xFF;
  bool done = false;
  int start = millis();

  do
  {
    if ((millis() - start) > SERIAL1_TIMEOUT)
    {
      done = true;
    }
  } while ((Serial1.peek() == -1) and !done);

  b = Serial1.read();

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
  int read_ctr = 0;
  // Serial.println("read_number()");

  do
  {
    byte c = read_uart_byte();
    read_ctr += 1;

    switch (c)
    {
    case 0xFF:
      // Don't count these.
      read_ctr = read_ctr - 1;
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

  } while (read_ctr < 6);

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
  byte b = 0xFF;
  byte length = 0;
  byte count = 0;
  bool uart_done = false;
  bool good_packet = false;
  UARTPkt uart_state = UART_START;

  do
  {
    switch (uart_state)
    {
    case UART_START:
    {
      b = read_uart_byte();

      if (b == 0x2A)
      {
#ifdef DEBUG_UART
        Serial.println("-> U HEADER");
#endif
        uart_state = UART_HEADER;
      }
      else
      {
        uart_done = true;
      }
    }
    break;

    case UART_HEADER:
    {
      b = read_uart_byte();
      if (b == 0x2B)
      {
#ifdef DEBUG_UART
        Serial.println("-> U LEN");
#endif
        uart_state = UART_PKT_LENGTH;
      }
      else
      {
        uart_done = true;
      }
    }
    break;

    case UART_PKT_LENGTH:
    {
      length = read_number();
      uart_pkt[count].value = length;
      count = count + 1;

#ifdef DEBUG_UART
      Serial.println("LEN: " + String(length));
      Serial.println("-> U PKT");
#endif
      uart_state = UART_PKT;
    }
    break;

    case UART_PKT:
      if (count <= length)
      {
        uart_pkt[count].value = read_number();
        count = count + 1;
        if (count > length)
        {
#ifdef DEBUG_UART
          Serial.println("-> READ CRC");
#endif
          uart_state = UART_CRC;
        }
        else
        {
#ifdef DEBUG_UART
          Serial.println("-> PKT CTD");
#endif
        }
      }
      break;
    case UART_CRC:
    {
      int crc = read_number();
      uart_pkt[count].value = crc;
#ifdef DEBUG_UART
      Serial.println("CRC " + String(crc));
      Serial.println("-> U TERM");
#endif
      uart_state = UART_TERM;
    }
    break;
    case UART_TERM:
    {
      b = read_uart_byte();
#ifdef DEBUG_UART
      Serial.println(String(b));
#endif
      if (b == '^')
      {
#ifdef DEBUG_UART
        Serial.println("GOOD TERM");
#endif
        good_packet = true;
        uart_done = true;
      }
      else
      {
#ifdef DEBUG_UART
        Serial.println("BAD TERM");
#endif
        uart_done = true;
        good_packet = false;
      }
    }
    break;
    }
  } while (!uart_done);

  return good_packet;
}

void show_uart_packet()
{
  int length = uart_pkt[0].value;
  for (int ndx = 0; ndx <= (length + 3); ndx++)
  {
#ifdef DEBUG_UART
    Serial.println(String(ndx) + "[" + String(uart_pkt[ndx].value) + "] ");
#endif
  }
#ifdef DEBUG_UART
  Serial.println("");
#endif
}

int calc_uart_crc()
{
  int crc = 0;
  int length = uart_pkt[0].value;
#ifdef DEBUG_UART
  Serial.println("PACKET LENGTH " + String(length));
#endif

  for (int ndx = 1; ndx <= length; ndx++)
  {
#ifdef DEBUG_UART
    DEBUG_UART("UART SUM " + String(uart_pkt[ndx].value));
#endif
    crc = crc + uart_pkt[ndx].value;
  }
  crc = crc % 128;
#ifdef DEBUG_UART
  Serial.println("CALCULATED CRC " + String(crc));
#endif

  return crc;
}

bool check_uart_packet()
{
  int length = uart_pkt[0].value;
  if (length < UART_PACKET_SIZE)
  {
    show_uart_packet();
    int crc = calc_uart_crc();
    int rcvcrc = uart_pkt[length + 1].value;
#ifdef DEBUG_UART
    Serial.println("UART LOCAL " + String(crc) + " RCV " + rcvcrc);
#endif

    return (crc == rcvcrc);
  }
  else
  {
    return false;
  }
}

// FIXME: UART packets carry their length.
// MIDI packets do not.
// Need to include the length in the MIDI packets
// so these are normalized...
void interpret_uart()
{
  for (int ndx = 0; ndx < UART_PACKET_SIZE; ndx++)
  {
    byte c = uart_pkt[ndx].controller;
    byte v = uart_pkt[ndx].value;

#ifdef DEBUG_UART
    Serial.println("pkt[" + String(ndx) + "] c[" + String(c) + "] v[" + String(v) + "]");
#endif
    uart_pkt[ndx].controller = c;
    uart_pkt[ndx].value = v;
  }
  interpret();
}

void process_uart()
{
  if (Serial1.peek() != -1)
  {
    reset_uart_pkts();
    bool is_good = read_uart_msg();
    if (is_good && check_uart_packet())
    {
#ifdef DEBUG_UART
      Serial.println("UART CRC PASS");
#endif
      interpret_uart();
    }
    else
    {
#ifdef DEBUG_UART
      Serial.println("UART CRC FAIL");
#endif
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
int loop_counter = 0;
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
    pin_check_clock = millis();
  }

  // Just so I know we're alive.
  loop_counter = loop_counter + 1;
  if ((loop_counter % 100000) == 0)
  {
    Serial.println(loop_counter);
  }
}