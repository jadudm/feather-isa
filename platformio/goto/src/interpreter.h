#ifndef _INTERPRETERH
#define _INTERPRETERH

enum MsgTypes
{
  SET_BOARD_PIXEL = 1,
  SET_PIXEL_AT_INDEX = 2,
  SET_PIXEL_RANGE = 3
};

void interpret();

#endif