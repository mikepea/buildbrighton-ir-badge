 // the below should generate a 38k ir signal. I have more somewhere with the ir pulsing separate [so it works summink like sendIr(HIGH|LOW); // pulse @ 38k for 6 cycles [low] or 12 cycls [high]]. Just loops through a byte sending out each bit


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>


#define redMask  0b00000010;
#define grnMask  0b00010000;
#define bluMask  0b00000100;
#define irIn     0b00000000;
#define irOut    0b00000001;

void delay_ten_us(unsigned long int us) {
  unsigned long int count;
  const unsigned long int DelayCount=6;

  while (us != 0) {
    for (count=0; count <= DelayCount; count++) {PINB |= 0b00100000;};
    us--;
  }
}

void delay_x_us(unsigned long int x) {
  unsigned long int count;
  const unsigned long int DelayCount=0;

  while (x != 0) {
    for (count=0; count <= DelayCount; count++) {PINB |= 0b00100000;};
    x--;
  }
}


// Little start up pattern
void startUp1() {
    for (int i = 0; i<15; i++) {
        PORTB ^= redMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= redMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= grnMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= grnMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= bluMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= bluMask;
        delay_ten_us(1600-(i*100));
    }
}


int main(void) {
    TCCR0A = 0;
    TCCR0B = 0;
    DDRB = 0b00010111;
    PORTB = 0xff; // write all outputs high & internal resistors on inputs (turns led's off)
    
    startUp1();
    
    while (1==1) {
        
        PORTB ^= irOut;
        PORTB ^= redMask;
        delay_x_us(26000); // 17 works
        
    }
}
