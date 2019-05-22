
/**
 * Code to support Interactive Sound Art at Bates College.
 * MIT License
 * Copyright 2019 Matt Jadud <mjadud@bates.edu>
 */
// https://semantic-ui.com/elements/icon.html
// Paw f1b0
// Sound Art F519
// Headphones F025
// Bullhorn F0A1
//% weight=100 color=#881127 icon="\uF0A1"

namespace ISA {
    function delay(): void {
        basic.pause(1);
    }
    function writeS(s: string): void {
        serial.writeString(s);
    }
    function writeN(n: number): void {
        writeS("<")
        serial.writeNumber(n);
        writeS(">")
        delay();
    }

    function write_cmd(ls: number[]): void {
        let crc = 0;
        for (let v of ls) {
            crc = crc + v;
        }
        crc = crc % 128;
        // Header
        writeS("*+");
        writeN(ls.length);
        for (let v of ls) {
            writeN(v);
        }
        writeN(crc);
        writeS("^");
    }

    /**
     * sends a MIDI message
     * @param chan MIDI command channel
     * @param value Value to send
     */
    //% blockId="isa_midi_command" block="midi_cmd|msg %value"
    export function midi_command(msg: number[]): void {
        serial.redirect(SerialPin.P0, SerialPin.P1, 115200);
        write_cmd(msg);
    }

    /**
     * sends a MIDI message
     * @param chan MIDI command channel
     * @param value Value to send
     */
    //% blockId="isa_midi_message" block="midi_message|chan %command_channel|value %value"
    export function midi_message(chan: number, value: number): void {
        if (value < 0) {
            value = 0;
        } else if (value > 127) {
            value = 127;
        }
        midi_command([chan, value]);
    }

    /**
     * sends a scaled MIDI message
     * @param chan MIDI command channel
     * @param value Value to scale
     * @param from_low Low end of input range
     * @param from_high High end of input range
     * @param to_low Low end of output range
     * @param to_high High end of output range
     */
    //% blockId="isa_midi_scaled" block="midi_scaled chan %command_channel value %value from %from_low \u2192 %from_high to %to_low \u2192 %to_high"
    //% inlineInputMode=inline
    export function midi_scaled(chan: number, value: number, from_low: number, from_high: number, to_low: number, to_high: number): void {
        value = pins.map(value, from_low, from_high, to_low, to_high);
        if (value < 0) {
            value = 0;
        } else if (value > 127) {
            value = 127;
        }
        midi_command([chan, value]);
    }

    /**
     * sends a MIDI bang
     * @param chan Foo
     * @param value Foo
     */
    //% 
    //% blockId="isa_bang" block="bang chan %command_channel"
    export function bang(chan: number): void {
        midi_command([chan, 1]);
        midi_command([chan, 0]);
    }

    /**
    * sends scaled acceleromter data
    * @param axis The axis
    */
    //% blockId = isa_acceleration
    //% blockId="isa_accel" block="acceleration (scaled 0 - 127) %axis_enum"
    //% color=#D400D4
    export function acceleration(d: Dimension): number {
        return pins.map(input.acceleration(d), -1024, 1024, 0, 127);
    }
}
