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

WiFiServer server(80);

/*-----( Declare Variables )-----*/
bool ota_enabled = true;
String header;

// Auxiliar variables to store the current output state
bool LED1_on = false;
bool LED2_on = false;
bool ButtonRelay_On = false;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

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
  server.begin(); // start webserver

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

  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /led1/on") >= 0) {
              Serial.println("LED1 on");
              LED1_on = true;
              digitalWrite(LED1, HIGH);
            } else if (header.indexOf("GET /led1/off") >= 0) {
              Serial.println("LED1 off");
              LED1_on = false;
              digitalWrite(LED1, LOW);
            } else if (header.indexOf("GET /led2/on") >= 0) {
              Serial.println("LED2 on");
              LED2_on = true;
              digitalWrite(LED2, HIGH);
            } else if (header.indexOf("GET /led2/off") >= 0) {
              Serial.println("LED2 off");
              LED2_on = false;
              digitalWrite(LED2, LOW);
            } else if (header.indexOf("GET /ButtonRelay/on") >= 0) {
              Serial.println("ButtonRelay off");
              ButtonRelay_On = true;
              digitalWrite(BUTTON_RELAY, HIGH);
            } else if (header.indexOf("GET /ButtonRelay/off") >= 0) {
              Serial.println("ButtonRelay off");
              ButtonRelay_On = false;
              digitalWrite(BUTTON_RELAY, LOW);
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            
            // If the lED1_on is false, it displays the ON button       
            if (!LED1_on) {
              client.println("<p>LED1 - Off</p>");
              client.println("<p><a href=\"/led1/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p>LED1 - On</p>");
              client.println("<p><a href=\"/led1/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               
            // If the lED2_on is false, it displays the ON button       
            if (!LED2_on) {
              client.println("<p>LED2 - Off</p>");
              client.println("<p><a href=\"/led2/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p>LED2 - On</p>");
              client.println("<p><a href=\"/led2/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            // If the ButtonRelay_On is false, it displays the ON button       
            if (!ButtonRelay_On) {
              client.println("<p>ButtonRelay - Off</p>");
              client.println("<p><a href=\"/ButtonRelay/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p>ButtonRelay - On</p>");
              client.println("<p><a href=\"/ButtonRelay/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

}  //--(end main loop )---

/*-----( Declare User-written Functions )-----*/
void printSerialAndDisplay(String text){
  display.clearDisplay();
  display.println(text);
  display.display();
  Serial.println(text);
}