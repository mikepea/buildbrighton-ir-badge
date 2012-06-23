
Using an Apple Remote
---------------------

Set badge ID (after factory reset, or new badge):
 - PLAY 

Factory Reset:
 - PLAY PLAY DOWN UP PLAY

Infect a badge:
 - PLAY PLAY PLAY






Random Notes
------------

38KHz = 1 pulse every 26.315us
8Mhz  = 1 instruction every 0.125us
 => 8-bit clock has period of 32us

560us = 17.5 clock cycles

Pin setup (Trippy RGB Wave):

IRin: PB3: 0b00001000

NEC Signal length: 9000 + 4500 + 560*6*16 = 67260us
 => 14 per second

From http://www.nongnu.org/avr-libc/user-manual/group__avr__sfr.html:
cbi and sbi are no longer supported, use:

  sbi(PORTB, PB1); is now PORTB |= _BV(PB1);

  cbi(sfr,bit); is now sfr &= ~(_BV(bit));

Setting Fuse bits
    SUT_CKSEL: FF, DF, C2   - set 8Mhz

Some fun codes, as read by IRremote:

APPLE_PLAY              0x77E1203A
APPLE_VOLUME_UP         0x77E1D03A
APPLE_VOLUME_DOWN       0x77E1B03A
APPLE_NEXT_TRACK        0x77E1E03A
APPLE_PREV_TRACK        0x77E1103A
APPLE_MENU              0x77E1403A

A2 = 10100010
5D = 01011101
80 = 10000000
7F = 01111111

TOSHIBA_1               0xA25D807F
TOSHIBA_2               0xA25D40BF
TOSHIBA_3               0xA25DC03F
TOSHIBA_4               0xA25D20DF
TOSHIBA_5               0xA25DA05F
TOSHIBA_6               0xA25D609F
TOSHIBA_7               0xA25DE01F
TOSHIBA_8               0xA25D10EF
TOSHIBA_9               0xA25D906F
TOSHIBA_0               0xA25D50AF

