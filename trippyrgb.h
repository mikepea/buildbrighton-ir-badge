
#define TRIPPY_RGB_WAVE 1

// overrides for Cornfield Trippy RGB Wave board
#define bogusMask    0b00100000
#define redMask      0b00000010
#define grnMask      0b00010000
#define bluMask      0b00000100
#define rgbMask      0b00010110
#define irInMask     0b00001000
#define irOutMask    0b00000001
// 4 = PB3
#define irInPortBPin  4

#define JUST_RED_ON      PORTB |= rgbMask; PORTB &= ~(redMask);
#define JUST_GREEN_ON    PORTB |= rgbMask; PORTB &= ~(grnMask);
#define JUST_BLUE_ON     PORTB |= rgbMask; PORTB &= ~(bluMask);

#define FLASH_BLUE      PORTB ^= bluMask; delay_ten_us(100); PORTB ^= bluMask;
#define FLASH_GREEN     PORTB ^= grnMask; delay_ten_us(100); PORTB ^= grnMask;
#define FLASH_RED       PORTB ^= redMask; delay_ten_us(100); PORTB ^= redMask;

