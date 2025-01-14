#include <WiFiS3.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"
#include "pitches.h"

// C++ code
#define ARDUINOJSON_SLOT_ID_SIZE 1
#define ARDUINOJSON_STRING_LENGTH_SIZE 1
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0
#define LED_PIN 8
#define PIR_PIN 4
#define BUZ_PIN 6

char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;           // your network key index number (needed only for WEP)

const char *server = "Your_ipv4_Address";  // your pc ip address
const int port = 3000;
int status = WL_IDLE_STATUS;
int loopCount = 0;

int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

WiFiClient wifi;

bool pirState = false;  //현재 PIR이 HIGH인지 LOW인지 상태
bool isToneOn = false;
unsigned long motionStartTime = 0;       //PIR이 처음 HIGH가 된 시간
unsigned long thresholdDuration = 2500;  //2.5초(필요시 변경)

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("Communication with WiFi module failed!\r\nPlease reset your device!"));
    // don't continue
    while (true);
  }
  // Attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    if(loopCount > 5) break;
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000); loopCount++;
  }

  // Print Current WiFi State
  printWifiStatus();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZ_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  
  // Set LED off
  digitalWrite(LED_PIN, LOW);  

  // Procedure Stack
  setDevice();
}

void loop() {
  int pirVal = digitalRead(PIR_PIN);

  // Activates when pir sensor detects something
  if (pirVal == HIGH && pirState == false) {
    pirState = true;
    motionStartTime = millis();  // Records time when pir sensor detected
    Serial.println(F("PIR detected HIGH"));
  }

  // Activates when pir sensor detects nothing
  if (pirVal == LOW && pirState == true) {
    pirState = false;
    isToneOn = false;
    Serial.println(F("PIR returned to LOW"));
    digitalWrite(LED_PIN, LOW);

    // Send Changed State to tinyIoT
    post("/TinyIoT/Arduino/pir_sensor","CIN","","0");
    post("/TinyIoT/Arduino/led", "CIN", "", "0");
    post("/TinyIoT/Arduino/buz", "CIN", "", "0");
  }

  if (pirState == true) {
    unsigned long currentTime = millis();
    // If pirVal remained HIGH more than thresholdtime, turn on LED and Buzzer
    if (currentTime - motionStartTime >= thresholdDuration) {
      digitalWrite(LED_PIN, HIGH);
      if (!isToneOn) {
        tone(BUZ_PIN, 262, 200);
        isToneOn = true;

        // Send Changed State to tinyIoT
        post("/TinyIoT/Arduino/pir_sensor","CIN","","1");
        post("/TinyIoT/Arduino/led", "CIN", "", "1");
        post("/TinyIoT/Arduino/buz", "CIN", "", "1");
      }
    }
  }
}

/* Set Device */
void setDevice(){
  Serial.println(F("Setup Started!"));
  startMelody();

  /* Check Arduino AE exists in TinyIoT if not create Arduino AE, CNT */
  int status = get("/TinyIoT/Arduino");
  if(status == 1) {
    Serial.println(F("Successfully retrieved!"));
  } else if(status == 0){
    Serial.println(F("Not Found... Creating AE"));

    int state = post("/TinyIoT","AE","Arduino","");
    if(state == 1){
      Serial.println(F("Successfully created AE"));
      post("/TinyIoT/Arduino", "CNT", "pir_sensor", "") == 1 ? Serial.println(F("Successfully created CNT_pir!")) : Serial.println(F("Failed to create CNT!"));
      post("/TinyIoT/Arduino", "CNT", "led", "") == 1 ? Serial.println(F("Successfully created CNT_led")) : Serial.println(F("Failed to create CNT!"));
      post("/TinyIoT/Arduino", "CNT", "buz", "") == 1 ? Serial.println(F("Successfully created CNT_buz")) : Serial.println(F("Failed to create CNT!"));
    } else if(state == 0){
      Serial.println(F("Invalid Header or URI"));
    } else {
      Serial.println(F("Wifi not connected!"));
    }
  } else{
    Serial.println(F("Wifi not connected!"));
  }
  delay(60000);

  startMelody();
  Serial.println(F("Setup completed!"));
}

/* Get Request(AE, CNT, CIN) */
int get(String path) {
  // HTTP Request Header
  String request = "GET " + path + " HTTP/1.1\r\n";
  request += "Host: ";
  request += server;
  request += ":";
  request += port;
  request += "\r\n";
  request += "X-M2M-RI: retrieve\r\n";
  request += "X-M2M-Rvi: 2a\r\n";
  request += "X-M2M-Origin: CAdmin\r\n";
  request += "Accept: application/json\r\n";
  request += "Connection: close\r\n\r\n";

  // Print Request Log
  // Serial.println(F("---- HTTP REQUEST START ----"));
  // Serial.println(request);
  // Serial.println(F("---- HTTP REQUEST END ----"));

  // Send Request with headers
  if (wifi.connect(server, port)) {
    wifi.print(request);
    Serial.println(F("\nGet Request sent"));
    delay(1000);
    
    int status = -1;
    // Get Response
    while (wifi.connected()) {
      while (wifi.available()) {
        String line = wifi.readStringUntil('\n');
        if(line.startsWith("HTTP/1.1")){
          status = line.substring(9,12).toInt();
        }
        delay(100);
      }
      wifi.stop();
    }
    Serial.println(status);
    if(status == 200 || status == 201){
      Serial.println(F("Get Response received"));
    }
    return status == 200 || status == 201 ? 1 : 0;
  } else {
    Serial.println(F("Connection to server failed"));
    return -1;
  }
}

/* Post Request Method(AE, CNT, CIN) */
int post(String path, String contentType, String name, String content){
  // HTTP Request Header
  String request = "POST " + path + " HTTP/1.1\r\n";
  request += "Host: ";
  request += server;
  request += ":";
  request += port;
  request += "\r\n";
  request += "X-M2M-RI: create\r\n";
  request += "X-M2M-Rvi: 2a\r\n";
  request += "X-M2M-Origin: SArduino\r\n";
  
  if(contentType.equals("AE")){
    request += "Content-Type: application/json;ty=2\r\n";
  } else if(contentType.equals("CNT")){
    request += "Content-Type: application/json;ty=3\r\n";
  } else if(contentType.equals("CIN")){
    request += "Content-Type: application/json;ty=4\r\n";
  } else{
    Serial.println(F("Unvalid Content Type!")); 
    return 0;
  }
  
  // Serialize Json Body
  String body = serializeJsonBody(contentType, name, content);

  request += "Content-Length: " + unsignedToString(body.length())  + "\r\n";
  request += "Accept: application/json\r\n";
  request += "Connection: close\r\n\r\n";
  

  // Print Request Log
  // Serial.println(F("---- HTTP REQUEST START ----"));
  // Serial.println(request + body);
  // Serial.println(F("---- HTTP REQUEST END ----"));

  // Send Request with headers and body
  if (wifi.connect(server, port)) {
    wifi.print(request + body);
    Serial.println("\n" + contentType + "Post Request sent");
    delay(1000);
    
    int status = -1;
    // Get Response
    while (wifi.connected()) {
      while (wifi.available()) {
        String line = wifi.readStringUntil('\n');
        if(line.startsWith("HTTP/1.1")){
          status = line.substring(9,12).toInt();
        }
        delay(100);
      }
      wifi.stop();
    }
    Serial.println(status);
    if(status == 200 || status == 201){
      Serial.println(F("Post Response Received"));
    }
    return status == 200 || status == 201 ? 1 : 0;
  } else {
    Serial.println(F("Connection to server failed"));
    return -1;
  }
}

/* Serialize JsonDocument Object to String and returns it */
String serializeJsonBody(String contentType, String name, String content){
  String body;
  JsonDocument doc;

  // Constructs JsonDocument 
  if(contentType.equals("AE")){
    JsonObject m2m_ae = doc["m2m:ae"].to<JsonObject>();
    m2m_ae["rn"] = name;
    m2m_ae["api"] = "NArduino";
    JsonArray lbl = m2m_ae["lbl"].to<JsonArray>();
    lbl.add("ae_"+name);
    m2m_ae["srv"][0] = "3";
    m2m_ae["rr"] = true;
  } else if(contentType.equals("CNT")){
    JsonObject m2m_cnt = doc["m2m:cnt"].to<JsonObject>();
    m2m_cnt["rn"] = name;
    JsonArray lbl = m2m_cnt["lbl"].to<JsonArray>();
    lbl.add("cnt_"+name);
    m2m_cnt["mbs"] = 16384;
  } else if(contentType.equals("CIN")){
    JsonObject m2m_cin = doc["m2m:cin"].to<JsonObject>();
    m2m_cin["con"] = content;
  }
  
  doc.shrinkToFit();
  serializeJson(doc, body);

  return body;
}

/* Convert unsigned int to String */
String unsignedToString(unsigned int value) {
    String result;
    do {
        result = char('0' + (value % 10)) + result;
        value /= 10;
    } while (value > 0);
    return result;
}

/* Print Wifi Connection Status */
void printWifiStatus() {
  if(WiFi.status() != WL_CONNECTED){
    Serial.println(F("Wifi not connected!"));
    return;
  }

  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}

/* Play Sound notes */
void startMelody(){
  for (int thisNote = 0; thisNote < 8; thisNote++) {
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(BUZ_PIN, melody[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(BUZ_PIN);
  }
}
