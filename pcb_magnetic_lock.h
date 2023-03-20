#warning "Using magnetic lock pin definitions"

#define PCB_MAGNETIC_LOCK

/* Board edge connector
 *
 * 1. Vcc 3.3v
 * 2. GPIO5   Door LED
 * 3. Gnd
 * 4. GPIO4   Buzzer
 * 5. GPIO12  Weigand WD0
 * 6. GPIO13  Weigand WD1
 * 7. GPIO14  Lock Sense
 * 8. GPIO15  Emergency Release Switch
 * 
 * MOSFET to GPIO16
 */

// MOSFET for door lock activation
// ON/HIGH == ground the output screw terminal
#define MOSFET D8

// Wiegand keyfob reader pins
#define WD0 D6
#define WD1 D7

// Door lock sense pin
#define SENSE D3

// emergency release switch
#define ERELEASE D4

// Buzzer/LED on keyfob control
#define BUZZER D5   // Gnd to beep
#define DOORLED D0  // low = green, otherwise red

// orientation of some signals
#define DLED_GREEN LOW
#define DLED_RED HIGH
#define LOCK_OPEN HIGH
#define LOCK_CLOSE LOW
#define BUZZ_ON LOW
#define BUZZ_OFF HIGH

// OLED screen shield for extra debug info (optional)
U8X8_SSD1306_128X64_NONAME_SW_I2C display(/* clock=*/ D1, /* data=*/ D2);

// how long to hold the latch open in millis
#define LOCK_TIMEOUT 5000

