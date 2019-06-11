#include "uart_machine.h"
#include <Arduino.h>

CV uart_buffer[PACKET_SIZE];

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

void init_uart_machine() {
  // The Micro:Bit
  Serial1.begin(SERIAL1_SPEED);
}

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
        return (-receivedNumber);
      }
      else
      {
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
bool read_uart_message()
{
  byte b = 0xFF;
  byte length = 0;
  byte count = 0;
  bool uart_done = false;
  bool good_packet = false;
  UARTPkt uart_state = UART_START;

  reset_uart_buffer();

  do
  {
    switch (uart_state)
    {
    case UART_START:
    {
      b = read_uart_byte();

      if (b == 0x2A)
      {
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
      uart_buffer[count].value = length;
      count = count + 1;
      uart_state = UART_PKT;
    }
    break;

    case UART_PKT:
      if (count <= length)
      {
        uart_buffer[count].value = read_number();
        count = count + 1;
        if (count > length)
        {
          uart_state = UART_CRC;
        }
        else
        {
        }
      }
      break;
    case UART_CRC:
    {
      int crc = read_number();
      uart_buffer[count].value = crc;
      uart_state = UART_TERM;
    }
    break;
    case UART_TERM:
    {
      b = read_uart_byte();
      if (b == '^')
      {
        good_packet = true;
        uart_done = true;
      }
      else
      {
        uart_done = true;
        good_packet = false;
      }
    }
    break;
    }
  } while (!uart_done);

  return good_packet;
}

void copy_uart_buffer()
{
  for (int ndx = 0; ndx < PACKET_SIZE; ndx++)
  {
    buffer[ndx] = uart_buffer[ndx];
  }
}


void reset_uart_buffer()
{
  for (int ndx = 0; ndx < PACKET_SIZE; ndx++)
  {
    uart_buffer[ndx].controller = 0xFF;
    uart_buffer[ndx].value = 0xFF;
  }
}
