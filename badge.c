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


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include "badge.h"
//#include "trippyrgb.h"

uint8_t last_eeprom_read = 1;
uint8_t enable_rgb_led = 1;
uint8_t curr_colour = 0;
uint8_t curr_r = 0;
uint8_t curr_g = 0;
uint8_t curr_b = 0;
uint8_t my_id = 0xff;
uint8_t my_mode = INIT_MODE;
uint8_t debug_modes = 0x00;
uint8_t rgb_colours[3] = { 1, 81, 161 }; // curr_colour values for R, G, and B.
uint8_t buffer_count = 0;
uint8_t factory_reset_keycombo_count = 0;

// counts 'ticks' (kinda-seconds) of main loop
unsigned int     main_loop_counter = 0;

uint8_t bit_by_zombie_count = 0;
int     time_infected = 0;

badge_record_t global_badge_buffer[BADGE_BUFFER_SIZE];

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
#ifdef TRIPPY_RGB_WAVE
  GTCCR |= _BV(COM0A0);  // turn on OC0A PWM output
#else
  GTCCR |= _BV(COM1B0);  // turn on OC1B PWM output
#endif
  delay_ten_us(time / 10);
}

/* Leave pin off for time (given in microseconds) */
void space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
#ifdef TRIPPY_RGB_WAVE
  GTCCR &= ~(_BV(COM0A0));  // turn off OC0A PWM output
#else
  GTCCR &= ~(_BV(COM1B0));  // turn off OC1B PWM output
#endif
  delay_ten_us(time / 10);
}

void enableIROut(void) {

#ifdef TRIPPY_RGB_WAVE
  TCCR0A = 0b01000010;  // COM0A1:0=01 to toggle OC0A on Compare Match
                            // COM0B1:0=00 to disconnect OC0B
                            // bits 3:2 are unused
                            // WGM01:00=10 for CTC Mode (WGM02=0 in TCCR0B)
  TCCR0B = 0b00000001;  // FOC0A=0 (no force compare)
                              // F0C0B=0 (no force compare)
                              // bits 5:4 are unused
                              // WGM2=0 for CTC Mode (WGM01:00=10 in TCCR0A)
                              // CS02:00=001 for divide by 1 prescaler (this starts Timer0)
  OCR0A = 104;  // to output 38,095.2KHz on OC0A (PB0, pin 5)

#else
  TCCR1 = _BV(CS10);  // turn on clock, prescale = 1
  GTCCR = _BV(PWM1B) | _BV(COM1B0);  // toggle OC1B on compare match; PWM mode on OCR1C/B.
  // these two values give 38khz PWM on IR LED (OC1B == PB4 == pin3),
  // with 33%ish duty cycle
  OCR1C = 210;
  OCR1B = 70;
#endif

}

void display_colour(uint8_t tick) {

  if ( enable_rgb_led ) {
#ifndef TURN_OFF_COLOUR_DISPLAY
#ifndef TURN_OFF_PWM_COLOUR
    if ((curr_r > tick) && ( tick % 5 == 0) ) {
        PORTB &= ~redMask; // turn on
    } else {
        PORTB |= redMask;
    }

    if ((curr_g > tick) && ( tick % 5 == 0)) {
        PORTB &= ~grnMask; // turn on
    } else {
        PORTB |= grnMask;
    }

    if ((curr_b > tick) && ( tick % 2 == 0)) {
        PORTB &= ~bluMask; // turn on
    } else {
        PORTB |= bluMask;
    }

#else
    if ( (curr_r == 255) && (tick < 32) ) {
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
    } else {
        PORTB |= bluMask;
    }
#endif
#endif
  }

}


void sendNEC(unsigned long data)
{
#ifndef DISABLE_IR_SENDING_CODE
    // handle turning on an approximation of our colour,
    // as RGB PWM is off during IR sending.

    uint8_t t = 0; // count of 50us marks

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

  rgb_tick += 1;
  display_colour(rgb_tick);

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
          irparams.irbuf[irparams.fptr] = irparams.ircode ;   // store code at fptr position
          irparams.fptr = (irparams.fptr + 1) % MAXBUF ; // move fptr to next empty slot
        }
        nextstate(IDLE) ;  // finished with this code, go back to IDLE
      }
      break ;
  }
  // end state processing

}

void flash_byte(uint8_t data) {
#ifdef ENABLE_FLASH_BYTE_CODE

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

#endif
}

void update_recd_id_in_eeprom(uint8_t id) {
    // have we seen them before?
    // if not, record that we have
    uint8_t times_seen = eeprom_read_byte((uint8_t*)id);
    if ( times_seen == 255 ) {
        // new id, since eeprom is initialised to 0xFF
        eeprom_write_byte((uint8_t*)id, 1);
    } else if (times_seen < 254 ) {
        eeprom_write_byte((uint8_t*)id, times_seen + 1);
    } // otherwise seen max (254) times, leave at that.
}

uint8_t have_not_seen_id_recently(uint8_t recd_id) {
    for ( uint8_t i=0; i<BADGE_BUFFER_SIZE; i++ ) {
        if ( global_badge_buffer[i].badge_id == recd_id ) {
            if ( global_badge_buffer[i].first_seen + BADGE_LAST_SEEN_MAX > main_loop_counter ) {
                //FLASH_RED;
                // have seen it recently
                return 0;
            } else {
                // blank that we've seen the badge, otherwise we'll get duplicates
                // in the buffer
                global_badge_buffer[i].badge_id = 0;
                global_badge_buffer[i].first_seen = 0;
                //FLASH_GREEN;
            }
        }
    }
    //FLASH_BLUE;
    return 1;
}

void record_that_we_have_seen_badge(uint8_t id) {
    if ( have_not_seen_id_recently(id) ) {
        global_badge_buffer[buffer_count].badge_id = id;
        global_badge_buffer[buffer_count].first_seen = main_loop_counter;
        update_recd_id_in_eeprom(id);
        buffer_count = ( buffer_count + 1 ) % BADGE_BUFFER_SIZE;
    }
}

void process_badge_message(unsigned long code) {

    // recving from a known badge
    //
    uint8_t recd_id = (code & ID_MASK) >> 16;

    if (recd_id == 0x0 || recd_id == 0xff ) {
        // not badge ids, ignore.
        return;
    }

#ifdef DEBUG_DISPLAY_DATA_RECEIVED
    flash_byte(recd_id);
#endif

    // TODO: only update to eeprom if we have not 
    //       seen the badge for a while
    if ( have_not_seen_id_recently(recd_id) ) {
        record_that_we_have_seen_badge(recd_id);
    }

    // what mode are they in?
    uint8_t recd_mode = (code & MODE_MASK) >> 8;

#ifdef DEBUG_DISPLAY_DATA_RECEIVED
    flash_byte(recd_mode);
#endif

    // what data did they send me?
    uint8_t recd_data = (code & DATA_MASK);

    if (my_mode == INIT_MODE && recd_mode != PROGRAM_BADGE_ID ) {
        return;
    }

    if (my_mode == REFLECT_COLOUR ) {
        curr_colour = recd_data;
        return;
    }

    if (recd_mode == AM_ZOMBIE) {
        // eek
        if ( bit_by_zombie_count > BITTEN_MAX ) {
            // oh noes! 
            my_mode = AM_INFECTED;
            bit_by_zombie_count = 0;
            time_infected = main_loop_counter;
        } else {
            // munch munch
            bit_by_zombie_count++;
        }

    } else if (recd_mode == SEND_ALL_EEPROM) {
        // ignore them, only collection stations need to listen

    } else if (recd_mode == SEND_ME_YOUR_DATA ) {
        // ooh, a collection station, upload!
        my_mode = SEND_ALL_EEPROM;

    } else if (recd_mode == PROGRAM_BADGE_ID ) {
        if ( my_mode == INIT_MODE ) {
            // sweet, getting ID from registration station
            eeprom_write_byte((uint8_t*)0, recd_data);
        } // but if we're not in INIT_MODE, we've already got one.

    } else if (recd_mode == CYCLE_COLOURS_SEEN) {
        if ( my_mode == AM_INFECTED ) {
            // phew, found someone to fix me.
            my_mode = CYCLE_COLOURS_SEEN;
        }

    } 

}

void check_all_ir_buffers_for_data(void) {

    for (uint8_t j=0; j<MAXBUF; j++) {

        if (IRBUF_CUR) {

            if ( (IRBUF_CUR & COMMON_CODE_MASK) == (long)(OUR_COMMON_CODE)<<24) {
                // we got a BB Badge code, process it.
                process_badge_message(IRBUF_CUR);

            } else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_PREV_TRACK ) { // DISPLAY ID
                // display our current id (by flashing binary blue/red on LED)
		            if ( my_mode != INIT_MODE ) {
	                  eeprom_write_byte((uint8_t*)0, curr_colour);
		                FLASH_GREEN;
		            } 
                factory_reset_keycombo_count = 0;

            } else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_NEXT_TRACK ) { // REFLECT COLOUR MODE
                // turn'em off, n sync-ish em up.
                enable_rgb_led = 1;
                if ( my_mode != INIT_MODE ) {
		                my_mode = REFLECT_COLOUR;
		            }
                factory_reset_keycombo_count = 0;

            } else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_VOLUME_UP ) { // TOGGLE LED DISPLA
                // turn'em off, n sync-ish em up.
                if ( factory_reset_keycombo_count == 3 ) { 
                    FLASH_RED;
                    factory_reset_keycombo_count++;
                } else {
                    // toggle whether to display colours
                    factory_reset_keycombo_count = 0;
                    enable_rgb_led = enable_rgb_led ? 0 : 1;
                }

            } else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_VOLUME_DOWN ) { // CYCLE COLOURS MODE
                // turn'em off, n sync-ish em up.
                if ( factory_reset_keycombo_count == 2 ) { 
                    //enable_rgb_led = 0;
                    FLASH_RED;
                    factory_reset_keycombo_count++;
                } else {
                    if ( my_mode != INIT_MODE ) {
                        FLASH_GREEN;
                        enable_rgb_led = 1;
                        my_mode = CYCLE_COLOURS_SEEN;
                        curr_colour = 0;
                    }
                    factory_reset_keycombo_count = 0;
                }

            } else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_PLAY ) { // INFECT! (or set badge ID)
                if ( my_mode == INIT_MODE ) {
                    // set id to current colour
                    eeprom_write_byte((uint8_t*)0, curr_colour);
                    // show that we've done this
                    FLASH_BLUE; delay_ten_us(1000);
                    FLASH_GREEN; delay_ten_us(1000);
                    FLASH_BLUE; delay_ten_us(1000);
                    FLASH_GREEN; delay_ten_us(1000);
                }
                if ( factory_reset_keycombo_count == 0 
                        || factory_reset_keycombo_count == 1 
                        || factory_reset_keycombo_count == 4 ) {
                    //enable_rgb_led = 0;
                    FLASH_RED;
                    factory_reset_keycombo_count++;
                } else {
                    // infect!
                    if ( my_mode != INIT_MODE ) {
                        my_mode = AM_INFECTED;
                        enable_rgb_led = 1;
                        time_infected = main_loop_counter;
                        factory_reset_keycombo_count = 0;
                    } else {
                    }
                }

            } else if ( ( IRBUF_CUR & ~COMMON_CODE_MASK ) == APPLE_MENU ) { // SEND EEPROM DATA
                // go into data transfer mode - IR all known info to a receiving station
                my_mode = SEND_ALL_EEPROM;
                factory_reset_keycombo_count = 0;

            }

            IRBUF_CUR = 0; // processed this code, delete it.

        }

    }

}

uint8_t get_next_colour() {

    // read next valid id from EEPROM
    uint8_t i = last_eeprom_read;
    uint8_t count = 0;

    if ( i == 0 ) { i++; } // EEPROM 0x00 reserved for my_id
    uint8_t seen = 0;
    do {
        i++;
        count++;
        if (i > 240) {
            i = 1;
        }
        seen = eeprom_read_byte((uint8_t*)i);
    } while ( ( count < 5 ) && (seen == 0xff) || (seen == 0) );

    last_eeprom_read = i;

    if ( seen != 0xff && seen != 0 ) {
        // new colour
        return last_eeprom_read;
    } else {
        return curr_colour;
    }

}

void factory_reset(void) {
    // blank all EEPROM (to 0xff)
    for (uint8_t i=0; i<1; i++) { 
        FLASH_RED;
        delay_ten_us(100);
        FLASH_GREEN;
        delay_ten_us(100);
        FLASH_BLUE;
        delay_ten_us(100);
        eeprom_write_byte((uint8_t*)i, 0xff);
        my_mode = INIT_MODE;
        enable_rgb_led = 1;
    }
}

void update_my_state(int counter) {

    if ( factory_reset_keycombo_count == 5 ) {
        // magic keycombo hit. Reset!
        factory_reset();
        factory_reset_keycombo_count = 0;
        return;
    }

    if ( my_mode == AM_INFECTED ) {
        // yikes. 
        //curr_colour = ( main_loop_counter % 2 ) ? 0 : 81;
        curr_colour = ( counter % 10 > 5 ) ? 0 : 81;
        if ( (main_loop_counter - time_infected) > MAX_TIME_INFECTED ) {
            // you die
            my_mode = AM_ZOMBIE;
        }

    } else if ( my_mode == AM_ZOMBIE ) {
        // hnnnngggg... brains...
        // TODO: Make this more ugly slow pulsing rather than flashing.
        curr_colour = ( counter % 20 > 10 ) ? 0 : 1;
        curr_colour = ( main_loop_counter % 2 ) ? 0 : 1;

    } else if ( my_mode == CYCLE_COLOURS_SEEN ) {
        curr_colour = get_next_colour();

    } else if ( my_mode == INIT_MODE ) {
        // need to work out badge id.
        //  - will be set in EEPROM address 0
        uint8_t eeprom_id = eeprom_read_byte((const uint8_t*)0);
        if ( (eeprom_id > 0) && (eeprom_id < 255) ) {
            my_id = eeprom_id;
            eeprom_write_byte((uint8_t*) my_id, 1); // ensure we're in our own colour db
            my_mode = CYCLE_COLOURS_SEEN;
        } else {
	          // display a bunch of colours, so we can select one
            curr_colour = (curr_colour + 7) % 240;
        }
    }

    // curr_colour is 1-240, 0=off
    if ( curr_colour == 0 ) {
        // converts to RGB 0x0 - & LEDs off.
        HSVtoRGB(&curr_r, &curr_g, &curr_b, 0, 0, 0);
    } else {
	//int v = (16*((counter % 15)+2) - 1); // 32-255 in blocks of 16
	int v = 255;
        HSVtoRGB(&curr_r, &curr_g, &curr_b, (curr_colour - 1), 255, v);
    }

}

void pre_loop_setup() {
    // initialise our globals
    for (uint8_t i=0; i<BADGE_BUFFER_SIZE; i++) {
        global_badge_buffer[i].badge_id = 0;
        global_badge_buffer[i].first_seen = 0;
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

    pre_loop_setup();

    while (1) {

        if ( main_loop_counter % 30 == 0 ) {
            // every n cycles, reset the keycombo-ometer, so
            // we don't accidentally enable it.
            factory_reset_keycombo_count = 0;
        }

        disable_ir_recving();
        enableIROut();

#ifndef DISABLE_EEPROM_SENDING_CODE
        if ( my_mode == SEND_ALL_EEPROM ) {
            my_mode = CYCLE_COLOURS_SEEN; // this will de-zombify the badge, if prev zombied
            sendNEC( MY_CODE_HEADER | (long)(SEND_ALL_EEPROM)<<8 | 0x01); // header
            delay_ten_us(10000);
            // first byte of EEPROM is my_id
            for ( uint8_t i = 1; i < EEPROM_SIZE - 1 ; i++ ) {
                FLASH_BLUE;
                uint8_t data = eeprom_read_byte((uint8_t*)i);
                if ( (data > 0) && (data < 255) ) {
                    sendNEC( (long)(OUR_COMMON_CODE)<<24 | (long)(i)<<8 | data );
                    delay_ten_us(EEPROM_SEND_DELAY);
                }
            }
            sendNEC( MY_CODE_HEADER | (long)(SEND_ALL_EEPROM)<<8 | 0xFF); // footer
            delay_ten_us(EEPROM_SEND_DELAY);
        }
#endif

        my_code  = MY_CODE_HEADER | (long)(my_mode) <<8 | curr_colour;

#ifndef DISABLE_IR_SENDING_CODE
        if ( my_mode != INIT_MODE ) {
          for (uint8_t i=0; i<NUM_SENDS; i++) {
            // transmit our identity, without interruption
            sendNEC(my_code);  // takes ~68ms
          }
        }
#endif

        enableIRIn();
        enable_ir_recving();

        // loop a number of times, to have ~1s of recving/game logic
        int j = 0;
        for (int i=0; i<730; i++) {

            check_all_ir_buffers_for_data();

            if ( i % 73 == 0 ) {
                // every so often (10 times per secondish, 
                //  update the mode we're in, colour we're showing)
                update_my_state(j);
     	          j++;
            }

            delay_ten_us(92 + (my_id % 16));  // differ sleep period so devices are less likely to interfere

        }

        main_loop_counter++;

    }

    return 0;

}

