#include "types.h"
#include "midi_machine.h"
#include "uart_machine.h"
#include "neopixels.h"
#include "interpreter.h"

// NewPing Source
// http://bit.ly/2EjVV4D
// #include "NewPing.h"

#define PIN_CHECK_INTERVAL 50
#define MAX_SONARS 3
#define MAX_SONAR_DISTANCE 200

#define SERIAL0_SPEED 115200

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
  init_uart_machine();

  delay(100);

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
  if (is_good_pkt && is_valid_buffer_crc())
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


void process_uart()
{
  if (Serial1.peek() != -1)
  {
    reset_uart_buffer();
    bool is_good = read_uart_message();
    if (is_good && is_valid_buffer_crc())
    {
#ifdef DEBUG_UART
      Serial.println("UART CRC PASS");
#endif
      interpret();
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