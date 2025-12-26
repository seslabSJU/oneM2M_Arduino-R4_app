#include <WiFiS3.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"
#include "pitches.h"

#define ARDUINOJSON_SLOT_ID_SIZE 1
#define ARDUINOJSON_STRING_LENGTH_SIZE 1
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0
#define LED_PIN 8
#define PIR_PIN 4
#define BUZ_PIN 6

static const char *HOST = "onem2m.iotcoss.ac.kr";  // lms
static const int PORT = 443;
static const char* ONEM2M_ORIGIN = "SOrigin_25110182";
static const char* RVI = "2a";
static const char* CSEBASE = "/tinyIoT";
static const char* AE_RN  = "R4";         
static const char* PIR_CNT = "pir";    
static const char* LED_CNT = "led";
static const char* BUZ_CNT = "buz";

int status = WL_IDLE_STATUS;
int loopCount = 0;

int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

WiFiSSLClient wifi;

bool pirState = false;                   // PIR 상태
bool isToneOn = false;
unsigned long motionStartTime = 0;       //PIR이 처음 HIGH가 된 시간
unsigned long thresholdDuration = 2500;  //2.5초(필요시 변경)

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!\r\nPlease reset your device!");
    // don't continue
    while (true);
  }
  // Attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    if(loopCount > 5) break;
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(SECRET_SSID);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(SECRET_SSID, SECRET_PASS);
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
    Serial.println("PIR detected HIGH");
  }

  // Activates when pir sensor detects nothing
  if (pirVal == LOW && pirState == true) {
    pirState = false;
    isToneOn = false;
    Serial.println("PIR returned to LOW");
    digitalWrite(LED_PIN, LOW);

    // Send Changed State to tinyIoT
    post(String(CSEBASE) + "/" + AE_RN + "/" + PIR_CNT, "CIN", "", "0");
    post(String(CSEBASE) + "/" + AE_RN + "/" + LED_CNT, "CIN", "", "0");
    post(String(CSEBASE) + "/" + AE_RN + "/" + BUZ_CNT, "CIN", "", "0");
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
        post(String(CSEBASE) + "/" + AE_RN + "/" + PIR_CNT, "CIN", "", "1");
        post(String(CSEBASE) + "/" + AE_RN + "/" + LED_CNT, "CIN", "", "1");
        post(String(CSEBASE) + "/" + AE_RN + "/" + BUZ_CNT, "CIN", "", "1");
      }
    }
  }
}

/* Set Device */
void setDevice(){
  Serial.println("Setup Started!");
  startMelody();

  /* Check Arduino AE exists in tinyIoT if not create Arduino AE, CNT */
  String aePath = String(CSEBASE) + "/" + AE_RN;
  int status = get(aePath);
  if(status == 1) {
    Serial.println("Successfully retrieved!");
  } else if(status == 0){
    Serial.println("Not Found... Creating AE");

    int state = post(CSEBASE, "AE", AE_RN, "");
    if(state == 1){
      Serial.println("Successfully created AE");
      post(aePath, "CNT", PIR_CNT, "") == 1 ? Serial.println("Successfully created CNT_pir!") : Serial.println("Failed to create CNT!");
      post(aePath, "CNT", LED_CNT, "") == 1 ? Serial.println("Successfully created CNT_led") : Serial.println("Failed to create CNT!");
      post(aePath, "CNT", BUZ_CNT, "") == 1 ? Serial.println("Successfully created CNT_buz") : Serial.println("Failed to create CNT!");
    } else if(state == 0){
      Serial.println("Invalid Header or URI");
    } else {
      Serial.println("Server connection failed!");
    }
  } else{
    Serial.println("Server connection failed!");
  }
  delay(60000);

  startMelody();
  Serial.println("Setup completed!");
}

/* Get Request(AE, CNT, CIN) */
int get(String path) {
  // Send Request with headers
  if (wifi.connect(HOST, PORT)) {
    Serial.println("Connected to HOST");
    // Send HTTP request line by line
    wifi.println("GET " + path + " HTTP/1.1");
    wifi.println("Host: " + String(HOST));
    wifi.println("X-M2M-Origin: " + String(ONEM2M_ORIGIN));
    wifi.println("X-M2M-RI: retrieve");
    wifi.println("X-M2M-RVI: 2a");
    wifi.println("X-API-KEY: " + String(API_KEY));
    wifi.println("X-AUTH-CUSTOM-LECTURE: " + String(LECTURE));
    wifi.println("X-AUTH-CUSTOM-CREATOR: " + String(CREATOR));
    wifi.println("Accept: application/json");
    wifi.println("Connection: close");
    wifi.println();

    Serial.println(("✓ Request sent"));

    int status = -1;
    Serial.println(("Reading response..."));
    Serial.print(("Connection state: "));
    Serial.println(wifi.connected() ? "CONNECTED" : "DISCONNECTED");
    Serial.print(("Bytes available: "));
    Serial.println(wifi.available());

    // Get Response
    while (wifi.connected()) {
      while (wifi.available()) {
        String line = wifi.readStringUntil('\n');
        Serial.print(("< "));
        Serial.println(line);
        if(line.startsWith("HTTP/1.1") || line.startsWith("HTTP/2")){
          status = line.substring(9,12).toInt();
          Serial.print(("Status Code: "));
          Serial.println(status);
        }
        delay(100);
      }
      wifi.stop();
    }

    Serial.print("Final Status: ");
    Serial.println(status);

    if(status == 200 || status == 201){
      Serial.println(("✓ Success!"));
      Serial.println(("========== GET REQUEST END ==========\n"));
      return 1;
    } else {
      Serial.println(("✗ Failed!"));
      Serial.println(("========== GET REQUEST END ==========\n"));
      return 0;
    }
  } else {
    Serial.println(("✗ Connection to HOST failed"));
    Serial.println(("========== GET REQUEST END ==========\n"));
    return -1;
  }
}

/* Post Request Method(AE, CNT, CIN) */
int post(String path, String contentType, String name, String content){
  // Send Request with headers and body
  Serial.println(("Attempting to connect..."));
  if (wifi.connect(HOST, PORT)) {
    Serial.println(("✓ Connected to HOST"));

    // Send HTTP request line by line
    wifi.println("POST " + path + " HTTP/1.1");
    wifi.println("Host: " + String(HOST));
    wifi.println("X-M2M-RI: create");
    wifi.println("X-M2M-RVI: 2a");
    wifi.println("X-M2M-Origin: " + String(ONEM2M_ORIGIN));
    wifi.println("X-API-KEY: " + String(API_KEY));
    wifi.println("X-AUTH-CUSTOM-LECTURE: " + String(LECTURE));
    wifi.println("X-AUTH-CUSTOM-CREATOR: " + String(CREATOR));

    if(contentType.equals("AE")){
      wifi.println("Content-Type: application/json;ty=2");
    } else if(contentType.equals("CNT")){
      wifi.println("Content-Type: application/json;ty=3");
    } else if(contentType.equals("CIN")){
      wifi.println("Content-Type: application/json;ty=4");
    }

    wifi.println("Content-Length: " + unsignedToString(body.length()));
    wifi.println("Accept: application/json");
    wifi.println("Connection: close");
    wifi.println();
    wifi.print(body);

    Serial.println(("✓ Request sent"));

    int status = -1;
    Serial.println(("Reading response..."));
    Serial.print(("Connection state: "));
    Serial.println(wifi.connected() ? "CONNECTED" : "DISCONNECTED");
    Serial.print(("Bytes available: "));
    Serial.println(wifi.available());

    // Get Response
    while (wifi.connected()) {
      while (wifi.available()) {
        String line = wifi.readStringUntil('\n');
        Serial.print(("< "));
        Serial.println(line);
        if(line.startsWith("HTTP/1.1") || line.startsWith("HTTP/2")){
          status = line.substring(9,12).toInt();
          Serial.print(("Status Code: "));
          Serial.println(status);
        }
        delay(100);
      }
      wifi.stop();
    }

    Serial.print(("Final Status: "));
    Serial.println(status);

    if(status == 200 || status == 201){
      Serial.println("✓ Success!");
      Serial.println("========== POST REQUEST END ==========\n");
      return 1;
    } else {
      Serial.println("✗ Failed!"));
      Serial.println("========== POST REQUEST END ==========\n");
      return 0;
    }
  } else {
    Serial.println("✗ Connection to HOST failed");
    Serial.println("========== POST REQUEST END ==========\n");
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
    Serial.println("Wifi not connected!");
    return;
  }

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
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
