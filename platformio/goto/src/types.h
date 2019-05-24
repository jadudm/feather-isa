#ifndef _TYPESH
#define _TYPESH

#include <stdint.h>

// This is used for MIDI, UART buffers.
#define PACKET_SIZE 16

typedef uint8_t byte;

typedef struct
{
  byte controller;
  byte value;
} CV;

// This buffer is used by the interpreter.
// The MIDI and UART systems copy into it just 
// before interpretation. Wasteful, but
// I have "lots" of RAM on the M4.
extern CV buffer[];

#endif