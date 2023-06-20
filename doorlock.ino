/* Doorlock
  - WHAT IT DOES
   Interfaces between an RFID card reader and either a magnetic door lock or electric roller shutter. 
   Uses a filesystem library to compare the RFID card ID against a list of authorised cards in a file.
   Provides a web service for viewing logs and performing OTA updates.
*/

/*-----( Import needed libraries )-----*/
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Network stack
#include <WiFiManager.h> 
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

//Other
#include <Wiegand.h>

//Filesystem
//#include <FS.h>
#include <LittleFS.h>       // https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS

/*-----( Declare Constants and Pin Numbers )-----*/
#include "pcb_roller_shutter.h" // board specific pin definitions
#include "logo.h"
#include "config.h"
#include "secrets.h" // login credentials. TODO: investigate using WiFi manager to not hardcode this.
#include "html.h"

const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";

/*-----( Declare objects )-----*/
WiFiManager wm;
AsyncWebServer server(80);
Adafruit_SSD1306 display(-1); //Initialise display library with no reset pin by passing it -1

#if (SSD1306_LCDHEIGHT != 48)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

WIEGAND wg;

/*-----( Declare Variables )-----*/
bool LED1_on = false;
bool LED2_on = false;
bool ButtonRelay_On = false;
bool SwitchLED_On = false;
bool RFID_Authenticated = false;

unsigned long tAuth = 0;
unsigned long tButton = 0;
int timeout = 0;

String debugMessage;

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons = "";
    buttons += "<h4>Output - Switch LED</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\" " + String(SWITCH_LED) + "\" " + outputState(SWITCH_LED) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - RFID LED</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\" " + String(RFID_LED) + "\" " + outputState(RFID_LED) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - Relay</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\" " + String(BUTTON_RELAY) + "\" " + outputState(BUTTON_RELAY) + "><span class=\"slider\"></span></label>";
    
    if (LittleFS.exists(CARD_FILE)) {
    
     buttons += "<p>Cardfile: " + String(CARD_FILE) + " is " + fileSize(CARD_FILE) + " bytes";
     int count = sanityCheck(CARD_FILE);
     if (count <= 0) {
      buttons += ", in an invalid file";
     } else {
      buttons += ", contains " + String(count) + " keyfob IDs";
      buttons += " - <a href=\"/download\">Download</a>";
     }
     
     buttons += ".</p>";
     } else {
       buttons += "<p>No cardfile found</p>";
     }

    return buttons;
  }
  return String();
}

String outputState(int output){
  if(digitalRead(output)){
    return "checked";
  }
  else {
    return "";
  }
}

void setup() { /****** SETUP: RUNS ONCE ******/

  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(RFID_LED, OUTPUT);
  pinMode(OUTSIDE_RAISE_LOGIC, INPUT_PULLUP);
  pinMode(OUTSIDE_LOWER_LOGIC, INPUT_PULLUP);

  digitalWrite(RFID_LED, LOW);
  pinMode(BUTTON_RELAY, OUTPUT);
  digitalWrite(BUTTON_RELAY, LOW);
  pinMode(SWITCH_LED, OUTPUT);
  digitalWrite(SWITCH_LED, LOW);

  Serial.begin(115200);

  // generate display voltage from 3V3 rail
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)

  display.setRotation(2);
  display.clearDisplay();
  // display init complete
  display.drawBitmap(8, 0, logo_bmp, 48, 48, 1);
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (!LittleFS.begin(true)) {
    printSerialAndDisplay("FS: Cannot mount LittleFS");
  }

  //WiFi Manager
  wm.setConfigPortalTimeout(180); // 3 minutes to complete setup
  bool result;
  result = wm.autoConnect(MANAGER_AP);

  if(!result) {
    printSerialAndDisplay("[WLAN] Failed to Connect");
  } else {
    printSerialAndDisplay("[WLAN] Connected");
  }

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

  String localIP = WiFi.localIP().toString();
  printSerialAndDisplay(localIP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.onNotFound(notFound);

  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request){
    String out = "<html><head><title>Upload Keyfob list</title></head><body>\
<form enctype=\"multipart/form-data\" action=\"/upload\" method=\"POST\">\
<input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"32000\" />\
Select file to upload: <input name=\"file\" type=\"file\" />\
<input type=\"submit\" value=\"Upload file\" />\
</form></body></html>";
    request->send(200, "text/html", out);

    File uploadFile;

    String upload_error;
    int upload_code = 200;
  }); 

  server.on( "/upload", HTTP_POST, [] (AsyncWebServerRequest *request){
    request->send(200, "text/html");
  },handleFileUpload);

  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
  });

  server.begin();


  wg.begin(DATA0,DATA1);


}  //--(end setup )---


void loop() { /****** LOOP: RUNS CONSTANTLY ******/

  checkResetButton();
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

void grantAccess(){
  RFID_Authenticated = true;
  Serial.println("[AUTH] Relay ON");
  digitalWrite(BUTTON_RELAY, HIGH);
  digitalWrite(SWITCH_LED, HIGH);
}

void denyAccess(){
  Serial.println("[AUTH] Relay OFF");
  digitalWrite(BUTTON_RELAY, LOW);
  digitalWrite(SWITCH_LED, LOW);
  RFID_Authenticated = false;
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
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if( digitalRead(RESET_BUTTON) == LOW ){
      printSerialAndDisplay("Hold to reset");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if( digitalRead(RESET_BUTTON) == LOW ){
        printSerialAndDisplay("Erasing Config, restarting");
        wm.resetSettings();
        ESP.restart();
      }
    }
  }
}

/*-----( Declare User-written Functions )-----*/
void printSerialAndDisplay(String text){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(text);
  display.display();
  Serial.println(text);
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

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

/* debug
    Serial.print("Line: code='");
    Serial.print(wcode);
    Serial.print("' (");
    Serial.print(newcode);
    Serial.print(") name='");
    Serial.print(wname);
    Serial.print("'");
*/
    if (cardID == newcode) {
   //   Serial.println(" - FOUND IT");
      answer = wname;
      break;
    }
    //Serial.println();
  }
  f.close();
  return answer;
}

/* how big is a file */
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

void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){

  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  printSerialAndDisplay(logmessage);

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = LittleFS.open(CARD_TMPFILE, "w");
    printSerialAndDisplay(logmessage);
  }
  
  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
    printSerialAndDisplay(logmessage);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    printSerialAndDisplay(logmessage);
    LittleFS.remove(CARD_FILE);
    LittleFS.rename(CARD_TMPFILE, CARD_FILE);

    logmessage = "Cardfile Created";
    printSerialAndDisplay(logmessage);

    if (LittleFS.exists(CARD_FILE)) {
      logmessage = "Cardfile exists!!!";
      printSerialAndDisplay(logmessage);
    }

    if (LittleFS.exists(CARD_TMPFILE)) {
      logmessage = "tempfile exists!!!";
      printSerialAndDisplay(logmessage);
    }

    request->redirect("/");
  }
}