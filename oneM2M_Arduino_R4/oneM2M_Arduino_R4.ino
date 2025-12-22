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

char apikey[] = API_KEY;
char lecture[] = LECTURE;
char creator[] = CREATOR;
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;           // your network key index number (needed only for WEP)

const char *server = "onem2m.iotcoss.ac.kr";  // lms
IPAddress serverIP(211, 180, 114, 135);       // IP address of onem2m.iotcoss.ac.kr
const int port = 443;
int status = WL_IDLE_STATUS;
int loopCount = 0;

int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

WiFiSSLClient wifi;
WiFiClient testClient;  // For TCP-only test

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
    post("/tinyIoT/Arduino/pir_sensor","CIN","","0");
    post("/tinyIoT/Arduino/led", "CIN", "", "0");
    post("/tinyIoT/Arduino/buz", "CIN", "", "0");
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
        post("/tinyIoT/Arduino/pir_sensor","CIN","","1");
        post("/tinyIoT/Arduino/led", "CIN", "", "1");
        post("/tinyIoT/Arduino/buz", "CIN", "", "1");
      }
    }
  }
}

/* Test TCP Connection (without SSL) */
void testTCP(){
  Serial.println(F("\n========== TCP TEST START =========="));
  Serial.print(F("Testing TCP connection to "));
  Serial.print(serverIP);
  Serial.println(F(":443"));

  if (testClient.connect(serverIP, port)) {
    Serial.println(F("✓ TCP connection successful!"));
    testClient.stop();
  } else {
    Serial.println(F("✗ TCP connection failed"));
  }

  Serial.println(F("========== TCP TEST END ==========\n"));
  delay(1000);
}

/* Test HTTPS Connection */
void testHTTPS(){
  Serial.println(F("\n========== HTTPS TEST START =========="));
  Serial.print(F("Testing SSL connection to "));
  Serial.print(serverIP);
  Serial.println(F(":443"));

  if (wifi.connect(serverIP, port)) {
    Serial.println(F("✓ SSL connection successful!"));
    wifi.stop();
  } else {
    Serial.println(F("✗ SSL connection failed"));
  }

  Serial.println(F("========== HTTPS TEST END ==========\n"));
  delay(1000);
}

/* Set Device */
void setDevice(){
  Serial.println(F("Setup Started!"));
  startMelody();

  // Test connections
  testTCP();
  testHTTPS();

  /* Check Arduino AE exists in tinyIoT if not create Arduino AE, CNT */
  int status = get("/tinyIoT/Arduino");
  if(status == 1) {
    Serial.println(F("Successfully retrieved!"));
  } else if(status == 0){
    Serial.println(F("Not Found... Creating AE"));

    int state = post("/tinyIoT","AE","Arduino","");
    if(state == 1){
      Serial.println(F("Successfully created AE"));
      post("/tinyIoT/Arduino", "CNT", "pir_sensor", "") == 1 ? Serial.println(F("Successfully created CNT_pir!")) : Serial.println(F("Failed to create CNT!"));
      post("/tinyIoT/Arduino", "CNT", "led", "") == 1 ? Serial.println(F("Successfully created CNT_led")) : Serial.println(F("Failed to create CNT!"));
      post("/tinyIoT/Arduino", "CNT", "buz", "") == 1 ? Serial.println(F("Successfully created CNT_buz")) : Serial.println(F("Failed to create CNT!"));
    } else if(state == 0){
      Serial.println(F("Invalid Header or URI"));
    } else {
      Serial.println(F("Server connection failed!"));
    }
  } else{
    Serial.println(F("Server connection failed!"));
  }
  delay(60000);

  startMelody();
  Serial.println(F("Setup completed!"));
}

/* Get Request(AE, CNT, CIN) */
int get(String path) {
  Serial.println(F("\n========== GET REQUEST START =========="));
  Serial.print(F("Connecting to: "));
  Serial.print(server);
  Serial.print(F(":"));
  Serial.println(port);

  // HTTP Request Header
  String request = "GET " + path + " HTTP/1.1\r\n"
                   "Host: " + String(server) + ":" + String(port) + "\r\n"
                   "X-M2M-RI: retrieve\r\n"
                   "X-M2M-Rvi: 2a\r\n"
                   "X-M2M-Origin: CAdmin\r\n"
                   "X-API-KEY: " + String(apikey) + "\r\n"
                   "X-AUTH-CUSTOM-LECTURE: " + String(lecture) + "\r\n"
                   "X-AUTH-CUSTOM-CREATOR: " + String(creator) + "\r\n"
                   "Accept: application/json\r\n"
                   "Connection: close\r\n\r\n";

  Serial.println(F("Request headers:"));
  Serial.println(request);

  // Send Request with headers
  Serial.println(F("Attempting to connect..."));
  if (wifi.connect(serverIP, port)) {
    Serial.println(F("✓ Connected to server"));

    // Send HTTP request line by line
    wifi.println("GET " + path + " HTTP/1.1");
    wifi.println("Host: " + String(server));
    wifi.println("X-M2M-RI: retrieve");
    wifi.println("X-M2M-Rvi: 2a");
    wifi.println("X-M2M-Origin: CAdmin");
    wifi.println("X-API-KEY: " + String(apikey));
    wifi.println("X-AUTH-CUSTOM-LECTURE: " + String(lecture));
    wifi.println("X-AUTH-CUSTOM-CREATOR: " + String(creator));
    wifi.println("Accept: application/json");
    wifi.println("Connection: close");
    wifi.println();

    Serial.println(F("✓ Request sent"));

    int status = -1;
    Serial.println(F("Reading response..."));
    Serial.print(F("Connection state: "));
    Serial.println(wifi.connected() ? "CONNECTED" : "DISCONNECTED");
    Serial.print(F("Bytes available: "));
    Serial.println(wifi.available());

    // Get Response
    while (wifi.connected()) {
      while (wifi.available()) {
        String line = wifi.readStringUntil('\n');
        Serial.print(F("< "));
        Serial.println(line);
        if(line.startsWith("HTTP/1.1") || line.startsWith("HTTP/2")){
          status = line.substring(9,12).toInt();
          Serial.print(F("Status Code: "));
          Serial.println(status);
        }
        delay(100);
      }
      wifi.stop();
    }

    Serial.print(F("Final Status: "));
    Serial.println(status);

    if(status == 200 || status == 201){
      Serial.println(F("✓ Success!"));
      Serial.println(F("========== GET REQUEST END ==========\n"));
      return 1;
    } else {
      Serial.println(F("✗ Failed!"));
      Serial.println(F("========== GET REQUEST END ==========\n"));
      return 0;
    }
  } else {
    Serial.println(F("✗ Connection to server failed"));
    Serial.println(F("========== GET REQUEST END ==========\n"));
    return -1;
  }
}

/* Post Request Method(AE, CNT, CIN) */
int post(String path, String contentType, String name, String content){
  Serial.println(F("\n========== POST REQUEST START =========="));
  Serial.print(F("Connecting to: "));
  Serial.print(server);
  Serial.print(F(":"));
  Serial.println(port);
  Serial.print(F("Content Type: "));
  Serial.println(contentType);

  // HTTP Request Header
  String request = "POST " + path + " HTTP/1.1\r\n"
                   "Host: " + String(server) + ":" + String(port) + "\r\n"
                   "X-M2M-RI: create\r\n"
                   "X-M2M-Rvi: 2a\r\n"
                   "X-M2M-Origin: SArduino\r\n"
                   "X-API-KEY: " + String(apikey) + "\r\n"
                   "X-AUTH-CUSTOM-LECTURE: " + String(lecture) + "\r\n"
                   "X-AUTH-CUSTOM-CREATOR: " + String(creator) + "\r\n";

  if(contentType.equals("AE")){
    request += "Content-Type: application/json;ty=2\r\n";
  } else if(contentType.equals("CNT")){
    request += "Content-Type: application/json;ty=3\r\n";
  } else if(contentType.equals("CIN")){
    request += "Content-Type: application/json;ty=4\r\n";
  } else{
    Serial.println(F("✗ Invalid Content Type!"));
    Serial.println(F("========== POST REQUEST END ==========\n"));
    return 0;
  }

  // Serialize Json Body
  String body = serializeJsonBody(contentType, name, content);

  request += "Content-Length: " + unsignedToString(body.length()) + "\r\n"
             "Accept: application/json\r\n"
             "Connection: close\r\n\r\n";

  Serial.println(F("Request headers:"));
  Serial.println(request);
  Serial.println(F("Request body:"));
  Serial.println(body);

  // Send Request with headers and body
  Serial.println(F("Attempting to connect..."));
  if (wifi.connect(serverIP, port)) {
    Serial.println(F("✓ Connected to server"));

    // Send HTTP request line by line
    wifi.println("POST " + path + " HTTP/1.1");
    wifi.println("Host: " + String(server));
    wifi.println("X-M2M-RI: create");
    wifi.println("X-M2M-Rvi: 2a");
    wifi.println("X-M2M-Origin: SArduino");
    wifi.println("X-API-KEY: " + String(apikey));
    wifi.println("X-AUTH-CUSTOM-LECTURE: " + String(lecture));
    wifi.println("X-AUTH-CUSTOM-CREATOR: " + String(creator));

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

    Serial.println(F("✓ Request sent"));

    int status = -1;
    Serial.println(F("Reading response..."));
    Serial.print(F("Connection state: "));
    Serial.println(wifi.connected() ? "CONNECTED" : "DISCONNECTED");
    Serial.print(F("Bytes available: "));
    Serial.println(wifi.available());

    // Get Response
    while (wifi.connected()) {
      while (wifi.available()) {
        String line = wifi.readStringUntil('\n');
        Serial.print(F("< "));
        Serial.println(line);
        if(line.startsWith("HTTP/1.1") || line.startsWith("HTTP/2")){
          status = line.substring(9,12).toInt();
          Serial.print(F("Status Code: "));
          Serial.println(status);
        }
        delay(100);
      }
      wifi.stop();
    }

    Serial.print(F("Final Status: "));
    Serial.println(status);

    if(status == 200 || status == 201){
      Serial.println(F("✓ Success!"));
      Serial.println(F("========== POST REQUEST END ==========\n"));
      return 1;
    } else {
      Serial.println(F("✗ Failed!"));
      Serial.println(F("========== POST REQUEST END ==========\n"));
      return 0;
    }
  } else {
    Serial.println(F("✗ Connection to server failed"));
    Serial.println(F("========== POST REQUEST END ==========\n"));
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
