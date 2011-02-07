/*
 * buildbrighton-ir-badge 
 *
 * Copyright 2010 BuildBrighton (Mike Pountney, Matthew Edwards)
 * For details, see http://github.com/mikepea/buildbrighton-ir-badge
 *
 * Interrupt code based on NECIRrcv by Joe Knapp, IRremote by Ken Shirriff
 *    http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1210243556
 *
 * HSVtoRGB code based on code from Nuno Santos: 
 *   http://www.nunosantos.net/archives/114
 *
 */

#include "my_id.h"
#include "badge.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

uint8_t same_colour_count = 0;
uint8_t curr_colour = (uint8_t)MY_BADGE_ID & displayRGBMask;
uint8_t curr_r = 0;
uint8_t curr_g = 0;
uint8_t curr_b = 0;
uint8_t my_mode = CYCLE_COLOURS_SEEN;
//uint8_t my_mode = INIT_MODE;
uint8_t debug_modes = 0x00;

// counts 'ticks' (kinda-seconds) of main loop
int     main_loop_counter = 0;

uint8_t bit_by_zombie_count = 0;
int     time_infected = 0;

void delay_ten_us(unsigned int us) {
  unsigned int count;

  while (us != 0) {
    for (count=0; count <= 8; count++) {
            PINB |= bogusMask;
    }
    us--;
  }
}

void enable_ir_recving(void) {
  //Timer0 Overflow Interrupt Enable
  TIMSK |= _BV(TOIE0);
}

void disable_ir_recving(void) {
  //Timer0 Overflow Interrupt disable
  TIMSK &= ~(_BV(TOIE0));
}

void mark(int time) {
  // Sends an IR mark for the specified number of microseconds.
  // The mark output is modulated at the PWM frequency.
  GTCCR |= _BV(COM1B0);  // turn on OC1B PWM output
  delay_ten_us(time / 10);
}

/* Leave pin off for time (given in microseconds) */
void space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  GTCCR &= ~(_BV(COM1B0));  // turn on OC1B PWM output
  delay_ten_us(time / 10);
}

void enableIROut(void) {

  TCCR1 = _BV(CS10);  // turn on clock, prescale = 1
  GTCCR = _BV(PWM1B) | _BV(COM1B0);  // toggle OC1B on compare match; PWM mode on OCR1C/B.
  OCR1C = 210;
  OCR1B = 70;

}

void sendNEC(unsigned long data)
{
#ifndef DISABLE_IR_SENDING_CODE
    // handle turning on an approximation of our colour,
    // as RGB PWM is off during IR sending.
    // TODO

    //enableIROut(); // put timer into send mode
    mark(NEC_HDR_MARK);
    space(NEC_HDR_SPACE);
    for (uint8_t i = 0; i < 32; i++) {
        if (data & 1) {
            mark(NEC_BIT_MARK);
            space(NEC_ONE_SPACE);
        } else {
            mark(NEC_BIT_MARK);
            space(NEC_ZERO_SPACE);
        }
        data >>= 1;
    }
    mark(NEC_BIT_MARK);
    space(0);
    //enableIRIn(); // switch back to recv mode
#endif
}

// initialization
void enableIRIn(void) {
  // setup pulse clock timer interrupt
  TCCR0A = 0;  // normal mode

  //Prescale /8 (8M/8 = 1 microseconds per tick)
  // Therefore, the timer interval can range from 1 to 256 microseconds
  // depending on the reset value (255 to 0)
  //cbi(TCCR0B,CS02);
  TCCR0B &= ~(_BV(CS02));
  //sbi(TCCR0B,CS01);
  TCCR0B |= _BV(CS01);
  //cbi(TCCR0B,CS00);
  TCCR0B &= ~(_BV(CS00));

  //Timer0 Overflow Interrupt Enable
  TIMSK |= _BV(TOIE0);

  RESET_TIMER0;

  sei();  // enable interrupts

  // initialize state machine variables
  irparams.rcvstate = IDLE ;
  irparams.bitcounter = 0 ;
  irparams.ircode = 0 ;
  irparams.fptr = 0 ;
  irparams.rptr = 0 ;

  // set pin modes
  //pinMode(irparams.recvpin, INPUT);
}

void HSVtoRGB( uint8_t *r, uint8_t *g, uint8_t *b, uint8_t hue, uint8_t s, uint8_t v )
{
    int f;
    long p, q, t;

    if( s == 0 )
    {
        // easy - just shades of grey.
        *r = *g = *b = v;
        return;
    }

    // special case, treat hue=0 as black
    //if ( hue == 0) {
    //    *r = *g = *b = 0;
    //}
 
    // hue is from 1-240, where 40=60deg, 80=120deg, so we can fit into a byte
    f = ((hue%40)*255)/40;
    hue /= 40;
 
    p = (v * (256 - s))/256;
    q = (v * ( 256 - (s * f)/256 ))/256;
    t = (v * ( 256 - (s * ( 256 - f ))/256))/256;
 
    switch( hue ) {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
}
 

/*
ISR(TIMER0_OVF_vect) {
  RESET_TIMER0;
  if ( irparams.timer > 5000 ) {
      irparams.timer = 0;
      PORTB ^= bluMask; delay_ten_us(1000); PORTB ^= bluMask;
  }
  irparams.timer++;
}
*/

// Recorded in ticks of 50 microseconds.

ISR(TIMER0_OVF_vect) {

  RESET_TIMER0;

  rgb_tick = (rgb_tick + 1) % 256;

#ifndef TURN_OFF_PWM_COLOUR
    //if ((curr_r > rgb_tick) && (rgb_tick < 32 )) {
    if ((curr_r > rgb_tick) && ( rgb_tick % 8 == 0) ) {
        PORTB &= ~redMask; // turn on
    } else {
        PORTB |= redMask;
    }

    if ((curr_g > rgb_tick) && ( rgb_tick % 4 == 0)) {
        PORTB &= ~grnMask; // turn on
    } else {
        PORTB |= grnMask;
    }

    if ((curr_b > rgb_tick) && ( rgb_tick % 2 == 0)) {
        PORTB &= ~bluMask; // turn on
    } else {
        PORTB |= bluMask;
    }

#else
    if ( (curr_r == 255) && (rgb_tick < 32) ) {
        PORTB &= ~redMask; // turn on
    } else {
        PORTB |= redMask;
    }

    if (curr_g == 255) {
        PORTB &= ~grnMask; // turn on
    } else {
        PORTB |= grnMask;
    }

    if (curr_b == 255) {
        PORTB &= ~bluMask; // turn on
    }
    else {
        PORTB |= bluMask;
    }

#endif

  irparams.irdata = (PINB & irInMask) >> (irInPortBPin - 1);

  // process current state
  switch(irparams.rcvstate) {
    case IDLE:
      if (irparams.irdata == MARK) {  // got some activity
          nextstate(STARTH) ;
          irparams.timer = 0 ;
      }
      break ;
    case STARTH:   // looking for initial start MARK
      // entered on MARK
      if (irparams.irdata == SPACE) {   // MARK ended, check time
        if ((irparams.timer >= STARTMIN) && (irparams.timer <= STARTMAX)) {
          nextstate(STARTL) ;  // time OK, now look for start SPACE
          irparams.timer = 0 ;
        } else {
          nextstate(IDLE) ;  // bad MARK time, go back to IDLE
        }
      }
      else {
        irparams.timer++ ;  // still MARK, increment timer
      }
      break ;
    case STARTL:
      // entered on SPACE
      if (irparams.irdata == MARK) {  // SPACE ended, check time
        if ((irparams.timer >= SPACEMIN) && (irparams.timer <= SPACEMAX)) {
          nextstate(BITMARK) ;  // time OK, check first bit MARK
          irparams.timer = 0 ;
          irparams.bitcounter = 0 ;  // initialize ircode vars
          irparams.irmask = (unsigned long)0x1 ;
          irparams.ircode = 0 ;
        }
        else if ((irparams.timer >= RPTSPACEMIN) && (irparams.timer <= RPTSPACEMAX)) {  
          // not a start SPACE, maybe this is a repeat signal
          nextstate(RPTMARK) ;   // yep, it's a repeat signal
          irparams.timer = 0 ;
        }
        else
          nextstate(IDLE) ;  // bad start SPACE time, go back to IDLE

      }
      else {   // still SPACE
        irparams.timer++ ;    // increment time
        if (irparams.timer >= SPACEMAX)  // check against max time for SPACE
          nextstate(IDLE) ;  // max time exceeded, go back to IDLE
      }
      break ;
    case RPTMARK:
      irparams.timer++ ;  // measuring MARK
      if (irparams.irdata == SPACE) {  // MARK ended, check time
        if ((irparams.timer >= BITMARKMIN) && (irparams.timer <= BITMARKMAX))
          nextstate(IDLE) ;  // repeats are ignored here, just go back to IDLE
        else
          nextstate(IDLE) ;  // bad repeat MARK time, go back to IDLE
      }
      break ;
    case BITMARK:
      irparams.timer++ ;   // timing MARK
      if (irparams.irdata == SPACE) {   // MARK ended, check time
        if ((irparams.timer < BITMARKMIN) || (irparams.timer > BITMARKMAX))
          nextstate(IDLE) ;  // bad MARK time, go back to idle
        else {
          irparams.rcvstate = BIT ;  // MARK time OK, go to BIT
          irparams.timer = 0 ;
        }
      }
      break ;
    case BIT:
      irparams.timer++ ; // measuring SPACE
      if (irparams.irdata == MARK) {  // bit SPACE ended, check time
        if ((irparams.timer >= ONESPACEMIN) && (irparams.timer <= ONESPACEMAX)) {
          nextstate(ONE) ;   // SPACE matched ONE timing
          irparams.timer = 0 ;
        }
        else if ((irparams.timer >= ZEROSPACEMIN) && (irparams.timer <= ZEROSPACEMAX)) {
          nextstate(ZERO) ;  // SPACE matched ZERO timimg
          irparams.timer = 0 ;
        }
        else
          nextstate(IDLE) ;  // bad SPACE time, go back to IDLE
      }
      else {  // still SPACE, check against max time
        if (irparams.timer > ONESPACEMAX)
          nextstate(IDLE) ;  // SPACE exceeded max time, go back to IDLE
      }
      break ;
    case ONE:
      irparams.ircode |= irparams.irmask ;  // got a ONE, update ircode
      irparams.irmask <<= 1 ;  // set mask to next bit
      irparams.bitcounter++ ;  // update bitcounter
      if (irparams.bitcounter < NBITS)  // if not done, look for next bit
        nextstate(BITMARK) ;
      else
        nextstate(STOP) ;  // done, got NBITS, go to STOP
      break ;
    case ZERO:
      irparams.irmask <<= 1 ;  // got a ZERO, update mask
      irparams.bitcounter++ ;  // update bitcounter
      if (irparams.bitcounter < NBITS)  // if not done, look for next bit
        nextstate(BITMARK) ;
      else
        nextstate(STOP) ;  // done, got NBITS, go to STOP
      break ;
    case STOP:
      irparams.timer++ ;  //measuring MARK
      if (irparams.irdata == SPACE) {  // got a SPACE, check stop MARK time
        if ((irparams.timer >= BITMARKMIN) && (irparams.timer <= BITMARKMAX)) {
          // time OK -- got an IR code
          //FLASH_RED;
          irparams.irbuf[irparams.fptr] = irparams.ircode ;   // store code at fptr position
          irparams.fptr = (irparams.fptr + 1) % MAXBUF ; // move fptr to next empty slot
        }
        nextstate(IDLE) ;  // finished with this code, go back to IDLE
      }
      break ;
  }
  // end state processing

}

#ifdef ENABLE_FLASH_BYTE_CODE
void flash_byte(uint8_t data) {

    for (uint8_t i=0; i<8; i++) {
        if ( data & 1 ) {
            PORTB |= rgbMask; // turns off RGB
            PORTB ^= redMask; // turns on red
            delay_ten_us(20000);
            PORTB |= rgbMask; // turns off RGB
            delay_ten_us(IR_DATA_PRINT_DELAY);
        } else {
            PORTB |= rgbMask; // turns off RGB
            PORTB ^= bluMask; // turns on red
            delay_ten_us(20000);
            PORTB |= bluMask; // turns off RGB
            delay_ten_us(IR_DATA_PRINT_DELAY);
        }
        data >>= 1;
    }

}
#endif

void check_all_ir_buffers_for_data(void) {

    for (uint8_t j=0; j<MAXBUF; j++) {

        if (IRBUF_CUR) {

            //FLASH_GREEN;

            //flash_ircode(irparams.irbuf[j]);
            
            //if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_PREV_TRACK ) {
            //    my_mode = AM_INFECTED;

            if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_VOLUME_DOWN ) {
                // turn'em off, n sync-ish em up.
                my_mode = CYCLE_COLOURS_SEEN;
                curr_colour = 0;

            } else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_PLAY ) {
                // zombie 'em up
                my_mode = AM_INFECTED;

            //} else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_MENU ) {
            //    // go into data transfer mode - IR all known info to a receiving station
            //    my_mode = SEND_ALL_EEPROM;

            } else if ( (IRBUF_CUR & COMMON_CODE_MASK) == (long)(OUR_COMMON_CODE)<<24) {

                // recving from a known badge
                
                // who is it?
                //
                uint8_t recd_id = (IRBUF_CUR & ID_MASK) >> 16;

                // have we seen them before?
                // if not, record that we have
                if ( eeprom_read_byte((uint8_t*)recd_id) != 1 ) {
                    eeprom_write_byte((uint8_t*)recd_id, 1);
                }

                // what mode are they in?
                uint8_t recd_mode = (IRBUF_CUR & MODE_MASK) >> 8;
                //flash_byte(recd_mode);

                // what data did they send me?
                //uint8_t recd_data = (IRBUF_CUR & DATA_MASK);

                if (recd_mode == AM_ZOMBIE) {
                    // eek
                    if ( bit_by_zombie_count > BITTEN_MAX ) {
                        my_mode = AM_INFECTED;
                        bit_by_zombie_count = 0;
                        time_infected = main_loop_counter;
                    } else {
                        bit_by_zombie_count++;
                    }

                } else if (recd_mode == SEND_ALL_EEPROM) {
                    continue; // ignore them

                } else if (recd_mode == SEND_ME_YOUR_DATA ) {
                    my_mode = SEND_ALL_EEPROM;
                    continue;

                } else if (recd_mode == CYCLE_COLOURS_SEEN) {
                    if ( my_mode == AM_INFECTED ) {
                        // phew, found someone to fix me.
                        my_mode = CYCLE_COLOURS_SEEN;
                    }
           
                } 

            }

            IRBUF_CUR = 0; // processed this code, delete it.

        }
        //FLASH_BLUE;
    }

}

void update_my_state(void) {

    if ( my_mode == AM_INFECTED ) {
        // yikes. 
        curr_colour = ( main_loop_counter % 2 ) ? 0 : 80;
        if ( (main_loop_counter - time_infected) > MAX_TIME_INFECTED ) {
            // you die
            my_mode = AM_ZOMBIE;
        }

    } else if ( my_mode == AM_ZOMBIE ) {
        // hnnnngggg... brains...
        //
        curr_colour = ( main_loop_counter % 3 ) ? 0 : displayRedMask;

    } else if ( my_mode == CYCLE_COLOURS_SEEN ) {
        // read next valid id from EEPROM
        //FLASH_GREEN;
        uint8_t i = curr_colour;
        if ( i == 0 ) { i++; } // EEPROM 0x00 reserved for MY_ID.
        uint8_t seen = 0;
        do {
            i++;
            if (i > 240) {
                i = 1;
            }
            seen = eeprom_read_byte((uint8_t*)i);
        } while ( (seen == 0xff) || (seen == 0) );
        curr_colour = i;

    } else if ( my_mode == INIT_MODE ) {
        // do nothing
        if ( main_loop_counter > 5 ) {
            my_mode = CYCLE_COLOURS_SEEN;
        }
    }

    // curr_colour is 1-240, 0=off
    if ( curr_colour == 0 ) {
        // converts to RGB 0x0 - & LEDs off.
        HSVtoRGB(&curr_r, &curr_g, &curr_b, 0, 0, 0);
    } else {
        HSVtoRGB(&curr_r, &curr_g, &curr_b, (curr_colour - 1), 255, 255);
    }

}

int main(void) {

    // zero our timer controls, for now
    TCCR0A = 0;
    TCCR0B = 0;
    TCCR1 = 0;
    GTCCR = 0;

    DDRB =  (rgbMask) | ( irOutMask );

    // all PORTB output pins High (all LEDs off), except for the
    // IR LED, which is SOURCE not SINK
    PORTB = ( 0xFF & ~irOutMask );
                    // -- (if we set an input pin High it activates a
                    // pull-up resistor, which we don't need, but don't care about either)

    //enableIRIn();
    sei();                // enable microcontroller interrupts

    long my_code = 0;
    eeprom_write_byte((uint8_t*) MY_BADGE_ID, 1); // ensure we're in our own colour db

    while (1) {

        disable_ir_recving();
        enableIROut();
        //FLASH_BLUE;

#ifndef DISABLE_EEPROM_SENDING_CODE
        if ( my_mode == SEND_ALL_EEPROM ) {
            my_mode = CYCLE_COLOURS_SEEN; // this will de-zombify the badge, if prev zombied
            sendNEC( MY_CODE_HEADER | (long)(SEND_ALL_EEPROM)<<8 | 0x01); // header
            delay_ten_us(10000);
            // first byte of EEPROM is my_id
            for ( uint8_t i = 1; i < EEPROM_SIZE - 1 ; i++ ) {
                uint8_t data = eeprom_read_byte((uint8_t*)i);
                if ( (data > 0) && (data < 255) ) {
                    sendNEC( (long)(OUR_COMMON_CODE)<<24 | (long)(i)<<8 | data );
                    delay_ten_us(10000);
                }
            }
            sendNEC( MY_CODE_HEADER | (long)(SEND_ALL_EEPROM)<<8 | 0xFF); // footer
            delay_ten_us(10000);
        }
#endif

        my_code  = MY_CODE_HEADER | (long)(my_mode) <<8 | curr_colour;

#ifndef DISABLE_IR_SENDING_CODE
        for (uint8_t i=0; i<NUM_SENDS; i++) {
           // transmit our identity, without interruption
           sendNEC(my_code);  // takes ~68ms
           //delay_ten_us(3280); // delay for 32ms
           //FLASH_RED;
        }
#endif

        enableIRIn();
        enable_ir_recving();

        // loop a number of times, to have ~1s of recving/game logic
        for (int i=0; i<730; i++) {

            check_all_ir_buffers_for_data();

            //if ( i == 0 ) {
            if ( i % 73 == 0 ) {
                update_my_state();
            }

            delay_ten_us(92 + (MY_BADGE_ID % 16));  // differ sleep period so devices are less likely to interfere

        }

        main_loop_counter++;

    }

    return 0;

}

