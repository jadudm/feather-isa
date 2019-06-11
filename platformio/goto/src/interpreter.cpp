#include "interpreter.h"
#include "neopixels.h"
#include "types.h"

CV buffer[PACKET_SIZE];

bool is_valid_buffer_crc() {
  int sum = 0;
  int localcrc = 0;
  int remcrc = 1;
  byte byte_count = 0;
  byte_count = buffer[0].value;

  for (int ndx = 1; ndx <= byte_count; ndx++)
  {
    sum = sum + buffer[ndx].value;
  }
  localcrc = sum % 128;
  remcrc = buffer[byte_count + 1].value;
  return (remcrc == localcrc);
}

void interpret()
{

  // Serial.println("INTERP");
  // The packet is:
  // [0] MSG TYPE
  // [1 ... ] Payload (per message type)
  switch (buffer[1].value)
  {
  case SET_BOARD_PIXEL:
  {
    byte r = buffer[2].value;
    byte g = buffer[3].value;
    byte b = buffer[4].value;
    board(r, g, b);
  }
  break;

  case SET_PIXEL_AT_INDEX:
  {
    int ndx = buffer[2].value;
    int r = buffer[3].value;
    int g = buffer[4].value;
    int b = buffer[5].value;
    ribbon_set(ndx, r, g, b);
    ribbon_update();
  }
  break;

  case SET_PIXEL_RANGE:
  {
    int start = buffer[2].value;
    int end = buffer[3].value;

    for (int ndx = start; ndx < end; ndx++)
    {
      ribbon_set(ndx, buffer[4].value, buffer[5].value, buffer[6].value);
    }
    ribbon_update();
  }
  break;
  }
}
