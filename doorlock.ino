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

//Network stack
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


/*-----( Declare Constants and Pin Numbers )-----*/
#include "pcb_roller_shutter.h"
#include "logo.h"
#include "config.h"
#include "secrets.h" // WiFi SSID and password

/*-----( Declare objects )-----*/
//Initialise display library with no reset pin by passing it -1
Adafruit_SSD1306 display(-1);

#if (SSD1306_LCDHEIGHT != 48)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/*-----( Declare Variables )-----*/
bool ota_enabled = true;

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

  WiFi.mode(WIFI_STA); //Station mode -> connects to an AP
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    printSerialAndDisplay("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(WWW_PASSWORD);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      printSerialAndDisplay("Start updating " + type);
      
    })
    .onEnd([]() {
      printSerialAndDisplay("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.clearDisplay();
  display.println(WiFi.localIP());
  display.println("");

  if (ota_enabled){
    display.println("OTA: On");
  } else {
    display.println("OTA: Off");
  }


  display.display();


}  //--(end setup )---

void loop() { /****** LOOP: RUNS CONSTANTLY ******/

  ArduinoOTA.handle();

}  //--(end main loop )---

/*-----( Declare User-written Functions )-----*/
void printSerialAndDisplay(String text){
  display.clearDisplay();
  display.println(text);
  display.display();
  Serial.println(text);
}