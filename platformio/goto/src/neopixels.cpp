#include "neopixels.h"

// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
Adafruit_NeoPixel board_neo(1, NEOPIXEL_BOARD_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ribbon_neo(NEOPIXEL_RIBBON_LENGTH, NEOPIXEL_RIBBON_PIN, NEO_GRB + NEO_KHZ800);

void board_init() {
  board_neo.begin();
}

void board(byte r, byte g, byte b)
{
  board_neo.setPixelColor(0, board_neo.Color(r / 2, g / 2, b / 2));
  board_neo.show();
}


void ribbon_init() {
  ribbon_neo.begin();
}

void ribbon_set(byte ndx, byte r, byte g, byte b)
{
  ribbon_neo.setPixelColor(ndx, ribbon_neo.Color(r, g, b));
}

void ribbon_update() {
  ribbon_neo.show();
}
