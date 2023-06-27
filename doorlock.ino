/* Doorlock
  - WHAT IT DOES
   Interfaces between an RFID card reader and either a magnetic door lock or electric roller shutter. 
   Uses a filesystem library to compare the RFID card ID against a list of authorised cards in a file.
   Provides a web service for viewing logs and performing OTA updates.
  - ATTRIBUTION
   Based on https://github.com/swanseahackspace/doorlock
*/

/*-----( Import needed libraries )-----*/

//LCD
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Network stack
#include <WiFiManager.h> 
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"

//Other
#include <Wiegand.h>

//Filesystem
#include <LittleFS.h> // https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS

/*-----( Declare Constants and Pin Numbers )-----*/
#include "pcb_roller_shutter.h" //board specific pin definitions
#include "logo.h"
#include "config.h"
#include "secrets.h" // Credentials for HTTP Basic Auth. Please define WWW_USER and WWW_PASSWORD constants.

/*-----( Declare objects )-----*/
WiFiManager wm;
AsyncWebServer server(80);
WIEGAND wg;

//Display
Adafruit_SSD1306 display(-1); // Initialise display library with no reset pin by passing it -1
#if (SSD1306_LCDHEIGHT != 48)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/*-----( Declare Variables )-----*/

// General
bool ota_enabled = true;
String debugMessage;

// Server
File uploadFile;
String upload_error;
int upload_code = 200;

// Auth
bool RFID_Authenticated = false; // Set True when authentication is given
unsigned long tAuth = 0; // time in milliseconds when authorisation is first given
unsigned long tButton = 0; // time in milliseconds when a button was most recently pressed
int timeout = 0; // stores different timeout lengths depending on the current state

// Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0; 

void setup() { /****** SETUP: RUNS ONCE ******/

  // IO Setup
  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(RFID_LED, OUTPUT); // Red light on the RFID reader
  pinMode(OUTSIDE_RAISE_LOGIC, INPUT_PULLUP);
  pinMode(OUTSIDE_LOWER_LOGIC, INPUT_PULLUP);

  digitalWrite(RFID_LED, LOW);
  pinMode(BUTTON_RELAY, OUTPUT);
  digitalWrite(BUTTON_RELAY, LOW);
  pinMode(SWITCH_LED, OUTPUT);
  digitalWrite(SWITCH_LED, LOW);

  // Serial Setup
  Serial.begin(115200);

  // Display Setup
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48). generate display voltage from 3V3 rail
  display.setRotation(2);
  display.clearDisplay();
  display.drawBitmap(8, 0, logo_bmp, 48, 48, 1);
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Filesystem Setup
  if (!LittleFS.begin(true)) {
    printSerialAndDisplay("[FS] Cannot mount LittleFS");
  }

  // WiFi Manager Setup
  wm.setConfigPortalTimeout(180); // 3 minutes to complete setup
  bool result;
  result = wm.autoConnect(MANAGER_AP);

  if(!result) {
    printSerialAndDisplay("[WLAN] Failed to Connect");
  } else {
    printSerialAndDisplay("[WLAN] Connected");
  }

  debugMessage = "[WLAN] " + WiFi.localIP().toString();
  printSerialAndDisplay(debugMessage);

  // OTA Setup
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(WWW_PASSWORD);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_LittleFS
      type = "filesystem";
    // NOTE: if updating LittleFS this would be the place to unmount LittleFS using LittleFS.end()
    printSerialAndDisplay("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    printSerialAndDisplay("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) printSerialAndDisplay("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) printSerialAndDisplay("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) printSerialAndDisplay("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) printSerialAndDisplay("Receive Failed");
    else if (error == OTA_END_ERROR) printSerialAndDisplay("End Failed");
  });

  ArduinoOTA.begin();

  // NTP Setup
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wiegand Setup
  wg.begin(DATA0,DATA1);

  // Web Server Setup
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_GET, handleUploadRequest);
  server.on( "/upload", HTTP_POST, [] (AsyncWebServerRequest *request){
    request->send(200, "text/html");
  },handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();

}  //--(end setup )---

void loop() { /****** LOOP: RUNS CONSTANTLY ******/

  checkResetButton(); // checks if on-module button is being held. If so: reset wifi config
  ArduinoOTA.handle();

  if(!RFID_Authenticated){
    if(wg.available()){
      unsigned long cardID = wg.getCode();
      
      debugMessage = "[WG] Scanned Card: " + String(cardID);
      Serial.println(debugMessage);

      String cardholderName = findKeyfob(cardID);

      if (cardholderName != NULL) { // CardID is in card file - grant access
        debugMessage = "[AUTH] Granted User: " + cardholderName;
        printSerialAndDisplay(debugMessage);
        tAuth = millis();
        tButton = tAuth;
        timeout = TIMER_INITIAL;
        grantAccess();
        //logEntry(now(), code);

      } else if(cardID != 0) { // CardID not in card file - deny access
      debugMessage = "[AUTH] Denied Card  " + String(cardID);
      printSerialAndDisplay(debugMessage);
      flashButtonLights(2);
      } else {
        Serial.println("[WG] Invalid CardID");
      }

    }
  }

  if(RFID_Authenticated){
    unsigned long t = millis();
    if((t - tAuth) > TIMER_SANITY || (t - tButton > timeout)){ // deauths if overall sanity timer has expired or button hasn't been pressed for n seconds
      denyAccess();
      flashButtonLights(3);
    }else if((digitalRead(OUTSIDE_RAISE_LOGIC) == LOW) || (digitalRead(OUTSIDE_LOWER_LOGIC) == LOW)){ // buttons are active low
      timeout = TIMER_BUTTON;
      tButton = millis(); 
    }
	}
}  //--(end main loop )---

/*-----( Declare User-written Functions )-----*/

// Web server callback functions
void handleRoot(AsyncWebServerRequest *request){

  char upTime[16];
  int sec = millis() / 1000;
  int mi = sec / 60;
  int hr = mi / 60;
  int day = hr / 24;
  
  snprintf(upTime, 16, "%dd %02d:%02d:%02d", day, hr % 24, mi % 60, sec % 60);

  char currentTime[20] = {0};
  getCurrentTime(currentTime);
    
  String out = "<html>\
  <head>\
    <title>Roller Shutter Controller</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; }\
    </style>\
  </head>\
  <body>\
    <h1>Roller Shutter Controller!</h1>\
    <p>Uptime: " + (String)upTime + "</p>\n";

  out += "<p>Time now: " + String(currentTime) + "</p>\n";
  out += "<p>Onboard Flash disk: - Size:" + String(LittleFS.totalBytes()) + " Used:" + String(LittleFS.usedBytes()) + "</p>\n";
  out += "<p>Lock is currently ";

  if(RFID_Authenticated) { out += "AUTHORISED"; } else { out += "UNAUTHORISED"; }

  out += "</p>\n";

  if (LittleFS.exists(CARD_FILE)) {
    
     out += "<p>Cardfile: " + String(CARD_FILE) + " is " + fileSize(CARD_FILE) + " bytes";
     int count = sanityCheck(CARD_FILE);
     if (count <= 0) {
      out += ", in an invalid file";
     } else {
      out += ", contains " + String(count) + " keyfob IDs";
      out += " - <a href=\"/download\">Download</a>";
     }
     
     out += ".</p>";
  } else {
    out += "<p>No cardfile found :(</p>";
  }

  out += "<ul>\
      <li><a href=\"/reset\">Reset Configuration</a></li>\
      <li><a href=\"/upload\">Upload Cardlist</a></li>";

  if (LittleFS.exists(LOG_FILE)) {
    out += "<li><a href=\"/wipelog\">Wipe log file</a></li>";
    out += "<li><a href=\"/viewlog?count=30\">View entry log</a></li>";
    out += "<li><a href=\"/download_logfile\">Download full logfile</a></li>";
  } else {
    out += "<li>No logfile found</li>";
  }

  if (ota_enabled) {
    out += "<li>OTA Updates ENABLED.";
  } else {
    out += "<li><a href=\"/enable_ota\">Enable OTA Updates</a>";
  }
  
  out += "</ul>";
  out += "</body></html>";

  request->send(200, "text/html", out);
};

void handleUploadRequest(AsyncWebServerRequest *request){
  if(!request->authenticate(WWW_USER, WWW_PASSWORD)){
    return request->requestAuthentication();
  }
  String out = "<html><head><title>Upload Keyfob list</title></head><body>\
  <form enctype=\"multipart/form-data\" action=\"/upload\" method=\"POST\">\
  <input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"32000\" />\
  Select file to upload: <input name=\"file\" type=\"file\" />\
  <input type=\"submit\" name=\"submit\" value=\"Upload file\" />\
  </form></body></html>";
  request->send(200, "text/html", out);
} 

void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  if(!request->authenticate(WWW_USER, WWW_PASSWORD)){
    return request->requestAuthentication();
  }

  debugMessage = "[HTTP] Client:" + request->client()->remoteIP().toString() + " " + request->url();
  printSerialAndDisplay(debugMessage);

  if (!index) {
    debugMessage = "[HTTP] Upload Start: " + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = LittleFS.open(CARD_TMPFILE, "w");
    printSerialAndDisplay(debugMessage);
  }
  
  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    debugMessage = "[FS] Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
    printSerialAndDisplay(debugMessage);
  }

  if (final) {
    debugMessage = "[HTTP] Upload Complete: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    printSerialAndDisplay(debugMessage);
  }

  handleUploadComplete(request);
}

void handleUploadComplete(AsyncWebServerRequest *request){
  if(!request->authenticate(WWW_USER, WWW_PASSWORD)){
    return request->requestAuthentication();
  }

  String out = "<p>Upload finished.";
  if (upload_code != 200) {
    out += "Error: "+upload_error;
  } else {
    out += " Success";
    // upload with no errors, replace old one
    LittleFS.remove(CARD_FILE);
    LittleFS.rename(CARD_TMPFILE, CARD_FILE);

    debugMessage = "[FS] Cardfile Updated";
    printSerialAndDisplay(debugMessage);
  }
  out += "</p><a href=\"/\">Back</a>";

  request->send(upload_code, "text/html", out);
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

// io functions
void grantAccess(){
  RFID_Authenticated = true;
  Serial.println("[AUTH] Relay ON");
  digitalWrite(BUTTON_RELAY, HIGH);
  digitalWrite(SWITCH_LED, HIGH);
}

void denyAccess(){
  digitalWrite(BUTTON_RELAY, LOW);
  digitalWrite(SWITCH_LED, LOW);
  RFID_Authenticated = false;
  debugMessage = "[AUTH] Deauthed";
  printSerialAndDisplay(debugMessage);
}

void flashButtonLights(int i){
  for(int j=0;j<i;j++){
    digitalWrite(SWITCH_LED, HIGH);
    delay(100);
    digitalWrite(SWITCH_LED, LOW);
    delay(100);
  }
}

void checkResetButton(){

  // check for button press
  if ( digitalRead(RESET_BUTTON) == LOW ) {
    // poor mans debounce/press-hold, not ideal for production
    delay(50);
    if( digitalRead(RESET_BUTTON) == LOW ){
      printSerialAndDisplay("Hold to reset");
      // still holding button for 3000 ms, reset settings, not ideal for production
      delay(3000); // reset delay hold
      if( digitalRead(RESET_BUTTON) == LOW ){
        printSerialAndDisplay("Erasing Config, restarting");
        wm.resetSettings();
        ESP.restart();
      }
    }
  }
}

// filesystem functions
String findKeyfob(unsigned int cardID)
{
  File f = LittleFS.open(CARD_FILE, "r");
  if (!f) {
    Serial.println("Error opening card file " CARD_FILE);
    return "";
  }

  String answer = "";
  while (f.available()) {
    char c = f.peek();
    // skip comment lines
    if (c == '#') {
      f.find("\n");
      continue;
    }

    String wcode = f.readStringUntil(',');
    String wname = f.readStringUntil('\n');

    unsigned int newcode = (wcode.toInt() & 0xFFFFFF);

    if (cardID == newcode) {
      answer = wname;
      break;
    }
  }
  f.close();
  return answer;
}

int fileSize(const char *filename){
  int ret = -1;
  File file = LittleFS.open(filename, "r");
  if (file) {
    ret = file.size();
    file.close();
  }
  return ret;
}

int sanityCheck(const char * filename)
{
  int count = 0;
  
  File f = LittleFS.open(filename, "r");
  if (!f) {
    Serial.print("Sanity Check: Could not open ");
    Serial.println(filename);
    return -1;
  }
  while (f.available()) {
    char c = f.peek();
    // skip comment lines
    if (c == '#') {
      f.find("\n");
      continue;
    }

    String wcode = f.readStringUntil(',');
    String wname = f.readStringUntil('\n');
    unsigned int newcode = wcode.toInt();

    if (newcode != 0) count++;
  }
  f.close();

  return count; 
}

// other functions
void printSerialAndDisplay(String text){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(text);
  display.display();
  Serial.println(text);
}

void getCurrentTime(char *currentTime){

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  
  snprintf(currentTime, 20, "%02d/%02d/%04d %02d:%02d:%02d",\
  timeinfo.tm_mday,\
  timeinfo.tm_mon + 1,\
  timeinfo.tm_year + 1900,\
  timeinfo.tm_hour + 1,\
  timeinfo.tm_min + 1,\
  timeinfo.tm_sec + 1);
}