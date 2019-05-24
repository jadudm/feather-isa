#ifndef _NEOPIXELSH
#define _NEOPIXELSH

#include "types.h"
#include "Adafruit_NeoPixel.h"

#define NEOPIXEL_BOARD_PIN 8
#define NEOPIXEL_RIBBON_PIN 4
#define NEOPIXEL_RIBBON_LENGTH 120

void board_init();
void board(byte r, byte g, byte b);

void ribbon_init();
void ribbon_set(byte ndx, byte r, byte g, byte b);
void ribbon_update();

#endif