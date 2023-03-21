/* Doorlock
  - WHAT IT DOES
   Interfaces between an RFID card reader and either a magnetic door lock or electric roller shutter. 
   Uses a filesystem library to compare the RFID card ID against a list of authorised cards in a file.
   Provides a web service for viewing logs and performing OTA updates.
*/

/*-----( Import needed libraries )-----*/
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/*-----( Declare Constants and Pin Numbers )-----*/
<<<<<<< HEAD
#include <pcb_roller_shutter.h>
#include <logo.h>
=======
#include "pcb_roller_shutter.h"
#include "logo.h"
#include "config.h"
#include "secrets.h"
>>>>>>> c6d7a32 (let's not share passwords with the internet)

/*-----( Declare objects )-----*/
//Initialise display library with no reset pin by passing it -1
Adafruit_SSD1306 display(-1);

#if (SSD1306_LCDHEIGHT != 48)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/*-----( Declare Variables )-----*/

void setup() { /****** SETUP: RUNS ONCE ******/

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(RFID_LED, OUTPUT);
  pinMode(BUTTON_RELAY, OUTPUT);

  Serial.begin(9600);

  // generate display voltage from 3V3 rail
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)

  display.setRotation(2);
  display.clearDisplay();
  // display init complete
  display.drawBitmap(8, 0, logo_bmp, 48, 48, 1);
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);

}  //--(end setup )---

void loop() { /****** LOOP: RUNS CONSTANTLY ******/

}  //--(end main loop )---

/*-----( Declare User-written Functions )-----*/