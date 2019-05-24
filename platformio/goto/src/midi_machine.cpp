#include "midi_machine.h"

// The MIDI buffer is where I read data into 
// from the state machine that parses incoming messages.
CV midi_buffer[PACKET_SIZE];

// reset : void -> void
// Clears the entire midi_buffer[] array, setting all values to
// 0xFF. This makes it a bit easier to see, as no MIDI
// value should be 255.
void reset_midi_buffer()
{
  for (int ndx = 0; ndx < PACKET_SIZE; ndx++)
  {
    midi_buffer[ndx].controller = 0xFF;
    midi_buffer[ndx].value = 0xFF;
  }
}

// MIDI State Machine
// This machine keeps track of things as they come in.
enum MIDIBufferMachine
{
  MIDI_START,
  MIDI_HEADER,
  MIDI_LENGTH,
  MIDI_COMMAND,
  MIDI_BODY
};

bool read_midi_message()
{
  midiEventPacket_t rx;
  MIDIBufferMachine state = MIDI_START;
  int byte_count = 0;  
  int expected_length = -1;
  bool is_good_packet = false;
  
  // Reset the MIDI buffer.
  reset_midi_buffer();

  // Start the machine.
  do
  {
    // Check if anything has come in 
    // from the MIDI port.
    rx = MidiUSB.read();
    
    if (rx.header != 0)
    {
      switch (state)
      {
      
      // A valid packet should start with 0x2A 0x2B.
      // This helps eliminate random start/stop.
      case MIDI_START:
        if (rx.byte3 == 0x2A)
        {
          state = MIDI_HEADER;
        }
        break;

      case MIDI_HEADER:
        if (rx.byte3 == 0x2B)
        {
          state = MIDI_LENGTH;
        }
        else
        {
          state = MIDI_START;
        }
        break;
      
      // The next byte should be the length of the packet
      // including the CRC. 
      case MIDI_LENGTH:
        midi_buffer[byte_count].controller = rx.byte2;
        midi_buffer[byte_count].value = rx.byte3;
        // I use the expected length for the state machine.
        expected_length = rx.byte3;
        byte_count = byte_count + 1;

        // If we accidentally got here, or the client attempts to
        // tell us a message is larger than the buffer, I can just go
        // back to the start and wait for a new packet to come my way.
        if (expected_length > PACKET_SIZE)
        {
          state = MIDI_START;
        }
        else
        {
          state = MIDI_COMMAND;
        }
        break;
        
      // COMMAND SETS
      // Messages 0-120 will be straight-forward commands.
      // Messages 121-126 will indicate one of 5 command sets.
      //
      // A command (0-120) gets interpreted directly.
      // A command set tells us that we have a second command byte following.
      // This gives us 120 + (5 * 127), or 755 possible commands.
      // None of this matters for the state machine, however.
      case MIDI_COMMAND:
        midi_buffer[byte_count].controller = rx.byte2;
        midi_buffer[byte_count].value = rx.byte3;
        byte_count = byte_count + 1;
        state = MIDI_BODY;
        break;

      case MIDI_BODY:
      // Either I'm in the message body...
        if (byte_count <= expected_length)
        {
          midi_buffer[byte_count].controller = rx.byte2;
          midi_buffer[byte_count].value = rx.byte3;
          byte_count = byte_count + 1;
          state = MIDI_BODY;
        }
        // Or I'm reading the CRC.
        else
        {
          midi_buffer[byte_count].controller = rx.byte2;
          midi_buffer[byte_count].value = rx.byte3;
          // Report a good read.
          is_good_packet = true;
          state = MIDI_START;
        }
        break;
      }
    }
  } while ((rx.header != 0) && (byte_count < PACKET_SIZE));

  return is_good_packet;
}

bool is_valid_midi_buffer()
{
  int sum = 0;
  int localcrc = 0;
  int remcrc = 1;
  byte byte_count = 0;
  byte_count = midi_buffer[0].value;

  for (int ndx = 1; ndx <= byte_count; ndx++)
  {
    sum = sum + midi_buffer[ndx].value;
  }
  localcrc = sum % 128;
  remcrc = midi_buffer[byte_count + 1].value;
  if (remcrc == localcrc)
  {
  }
  else
  {
  }
  return (remcrc == localcrc);
}

void copy_midi_buffer() {
  for (int ndx = 0; ndx < PACKET_SIZE ; ndx++) {
    buffer[ndx] = midi_buffer[ndx];
  }
}