#ifndef _MIDI_MACHINEH
#define _MIDI_MACHINEH

#include "MIDIUSB.h"
#include "types.h"

#define MIDI_CHANNEL 0

bool read_midi_message();
// bool is_valid_midi_buffer();
void copy_midi_buffer();
void reset_midi_buffer();

#endif