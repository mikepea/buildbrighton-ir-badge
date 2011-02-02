
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

// This function delays the specified number of 10 microseconds
void delay_ten_us(unsigned long int us) {
  unsigned long int count;
  const unsigned long int DelayCount=8;  // this value was determined by trial and error

  while (us != 0) {
    // Toggling PB5 is done here to force the compiler to do this loop, rather than optimize it away
    //   NOTE:  Below you will see: "0b00100000".
    //   This is an 8-bit binary number with all bits set to 0 except for the the 6th one from the right.
    //   Since bits in binary numbers are labeled starting from 0, the bit in this binary number that is set to 1
    //     is called PB5, which is the one unsed PORTB pin, which is why we can use it here
    //     (to fool the optimizing compiler to not optimize away this delay loop).
    for (count=0; count <= DelayCount; count++) {PINB |= 0b00100000;};
    us--;
  }
}



// This function delays (1.56 microseconds * x) + 2 microseconds
//   (determined empirically)
//    e.g.  if x = 1, the delay is (1 * 1.56) + 2 = 5.1 microseconds
//          if x = 255, the delay is (255 * 1.56) + 2 = 399.8 microseconds
void delay_x_us(unsigned long int x) {
  unsigned long int count;
  const unsigned long int DelayCount=0;  // the shortest delay

  while (x != 0) {
    // Toggling PB5 is done here to force the compiler to do this loop, rather than optimize it away
    //   (see NOTE in the comments in the delay_ten_us() function, above)
    for (count=0; count <= DelayCount; count++) {PINB |= 0b00100000;};
    x--;
  }
}

#define grnMask  0b00000100

int main(void) {

    PORTB = 0xff; // write all outputs high & internal resistors on inputs (turns led's off)
    DDRB = 0b00010111;  // setting a bit to 1 makes it an output, setting a bit to 0 makes it an input

    while (1==1) {

        PORTB ^= grnMask;
        delay_ten_us(10); // 1 ms
        PORTB ^= grnMask;
        for ( int i = 0; i < 1000; i++ ) {
            delay_ten_us(100); // 1 ms
        }

    }

}
