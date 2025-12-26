#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>
#include <Arduino_LED_Matrix.h>

#include "arduino_secrets.h"
#include "pitches.h"
#include "led_patterns.h"

#define ARDUINOJSON_SLOT_ID_SIZE 1
#define ARDUINOJSON_STRING_LENGTH_SIZE 1
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0
#define LED_PIN 8
#define PIR_PIN 4
#define BUZ_PIN 6

static const char *HOST = "onem2m.iotcoss.ac.kr";  
static const int PORT = 443;
static const char* ONEM2M_ORIGIN = "SOrigin_12341234_test";
static const char* RVI = "2a";
static const char* CSEBASE = "/Mobius";
static const char* AE_RN  = "R4_TUTORIAL";         
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
ArduinoLEDMatrix matrix;

bool ready = false;
bool pirState = false;                   // PIR 상태
bool isToneOn = false;
unsigned long motionStartTime = 0;       //PIR이 처음 HIGH가 된 시간
unsigned long thresholdDuration = 2500;  //2.5초(필요시 변경)
uint32_t reqSeq = 1;                     // RI 생성용 시퀀스
unsigned long lastTick = 0;              // 마지막 실행 시간
static const unsigned long PERIOD_MS = 5000;  // 5초 간격

/* Update LED Matrix based on current state */
void updateLEDMatrix() {
  // 우선순위: WiFi 연결 > oneM2M Ready > PIR 센서 상태

  if (WiFi.status() != WL_CONNECTED) {
    // 1. WiFi 연결 안됨 -> "W" 표시
    matrix.renderBitmap(PATTERN_WIFI_CONNECTING, 8, 12);
  } else if (!ready) {
    // 2. WiFi 연결됨, oneM2M 준비 안됨 -> 체크마크 표시
    matrix.renderBitmap(PATTERN_WIFI_CONNECTED, 8, 12);
  } else if (pirState) {
    // 3. 모든 준비 완료, PIR 센서 감지 -> 느낌표(!) 표시
    matrix.renderBitmap(PATTERN_PIR_DETECTED, 8, 12);
  } else {
    // 4. 모든 준비 완료, PIR 대기 중 -> 빈 화면 (모든 LED 꺼짐)
    matrix.renderBitmap(PATTERN_PIR_IDLE, 8, 12);
  }
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial);

  // Initialize LED Matrix
  matrix.begin();

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!\r\nPlease reset your device!");

    while (true) delay(1000);
  }
  // Attempt to connect to WiFi network:
  if (!ensureWifiConnected(30000)) {
    Serial.println("[FATAL] WiFi connect failed.");
    while (true) delay(1000);
  }

  // Print Current WiFi State
  printWifiStatus();
  updateLEDMatrix();  // Update LED after WiFi connected

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZ_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  // Set LED off
  digitalWrite(LED_PIN, LOW);

  // Check CB first
  Serial.println("\n[STEP] GET CB (CSEBase) ...");
  int sc = get(String(CSEBASE));
  Serial.print("[CB] HTTP status = ");
  Serial.println(sc);

  if (sc == 200 || sc == 201) {
    // Procedure Stack
    setDevice();
  } else {
    Serial.println("[WARN] CB not reachable. Setup failed.");
    ready = false;
  }
  updateLEDMatrix();  // Update LED after setup complete
}

void loop() {
  int pirVal = digitalRead(PIR_PIN);
  unsigned long now = millis();

  // WiFi drop handling
  if (WiFi.status() != WL_CONNECTED) {
    ready = false;
    Serial.println("[WiFi] Disconnected. Reconnecting...");
    updateLEDMatrix();  // Show WiFi disconnected
    ensureWifiConnected(30000);
    printWifiStatus();
    updateLEDMatrix();  // Update after reconnection
  }

  if (now - lastTick < PERIOD_MS) return;
  lastTick = now;

  // If not ready, retry CB + setDevice
  if (!ready) {
    Serial.println("\n[RETRY] GET CB (CSEBase) ...");
    int sc = get(String(CSEBASE));
    Serial.print("[CB] HTTP status = ");
    Serial.println(sc);

    if (sc == 200 || sc == 201) {
      setDevice();
      updateLEDMatrix();  // Update after setDevice
    }
    return;
  }

  // Activates when pir sensor detects something
  if (pirVal == HIGH && pirState == false) {
    pirState = true;
    motionStartTime = millis();  // Records time when pir sensor detected
    Serial.println("PIR detected HIGH");
    updateLEDMatrix();  // Update LED for PIR detection
  }

  // Activates when pir sensor detects nothing
  if (pirVal == LOW && pirState == true) {
    pirState = false;
    isToneOn = false;
    Serial.println("PIR returned to LOW");
    digitalWrite(LED_PIN, LOW);
    updateLEDMatrix();  // Update LED when PIR returns to LOW

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

/* Ensure WiFi Connected */
static bool ensureWifiConnected(unsigned long maxWaitMs) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("[WiFi] Connecting to SSID: ");
  Serial.println(SECRET_SSID);

  WiFi.begin(SECRET_SSID, SECRET_PASS);

  unsigned long start = millis();
  while (millis() - start < maxWaitMs) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Connected.");
      return true;
    }
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  return (WiFi.status() == WL_CONNECTED);
}

/* Set Device */
void setDevice(){
  Serial.println("Setup Started!");
  
  startMelody();

  String aePath = String(CSEBASE) + "/" + AE_RN;

  // Check if AE exists
  int aeSc = get(aePath);

  if (aeSc == 404 || aeSc == 403) {
    Serial.println("[setDevice] AE not found -> create AE");
    int cr = post(CSEBASE, "AE", AE_RN, "");

    if (cr == 201 || cr == 200 || cr == 409) {
      Serial.println("[setDevice] AE created/exists");

      // Create CNTs
      int pirSc = post(aePath, "CNT", PIR_CNT, "");
      pirSc == 201 || pirSc == 200 || pirSc == 409 ? Serial.println("[setDevice] CNT_pir OK") : Serial.println("[setDevice] CNT_pir FAILED");

      int ledSc = post(aePath, "CNT", LED_CNT, "");
      ledSc == 201 || ledSc == 200 || ledSc == 409 ? Serial.println("[setDevice] CNT_led OK") : Serial.println("[setDevice] CNT_led FAILED");

      int buzSc = post(aePath, "CNT", BUZ_CNT, "");
      buzSc == 201 || buzSc == 200 || buzSc == 409 ? Serial.println("[setDevice] CNT_buz OK") : Serial.println("[setDevice] CNT_buz FAILED");

      ready = true;  // CNT 생성 완료, ready 설정
    } else {
      Serial.println("[setDevice] AE create failed");
      ready = false;
      return;
    }
  } else if (aeSc == 200 || aeSc == 201) {
    Serial.println("[setDevice] AE exists");
    ready = true;  // AE 존재 확인, ready 설정
  } else {
    Serial.println("[setDevice] AE check failed");
    ready = false;
    return;
  }

  startMelody();
  Serial.println("Setup completed!");
}

/* Generate unique RI */
String nextRI() {
  String ri = "r4-";
  ri += String(reqSeq++);
  return ri;
}

/* Read HTTP status line */
int readHttpStatusLine() {
  String line = wifi.readStringUntil('\n');
  line.trim();

  if (!line.startsWith("HTTP/")) return -1;

  int sp1 = line.indexOf(' ');
  if (sp1 < 0) return -1;
  int sp2 = line.indexOf(' ', sp1 + 1);
  if (sp2 < 0) sp2 = line.length();

  String codeStr = line.substring(sp1 + 1, sp2);
  return codeStr.toInt();
}

/* Read X-M2M-RSC from headers */
int readXM2MRSCFromHeaders() {
  int rsc = -1;

  while (wifi.connected()) {
    String line = wifi.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;

    if (line.startsWith("X-M2M-RSC:") || line.startsWith("x-m2m-rsc:")) {
      int colon = line.indexOf(':');
      if (colon >= 0) {
        String v = line.substring(colon + 1);
        v.trim();
        rsc = v.toInt();
      }
    }
  }
  return rsc;
}

/* Drain body briefly */
void drainBodyBrief(size_t maxBytes) {
  size_t printed = 0;
  while (wifi.available() && printed < maxBytes) {
    char c = (char)wifi.read();
    Serial.print(c);
    printed++;
  }
  if (wifi.available()) {
    Serial.println("\n[Body truncated]");
    while (wifi.available()) (void)wifi.read();
  } else {
    Serial.println();
  }
}

/* Get Request(AE, CNT, CIN) */
int get(String path) {
  if (!wifi.connect(HOST, PORT)) {
    Serial.println("[ERROR] TLS connect failed");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();

  // Send HTTP request
  wifi.println("GET " + path + " HTTP/1.1");
  wifi.println("Host: " + String(HOST));
  wifi.println("X-M2M-Origin: " + String(ONEM2M_ORIGIN));
  wifi.println("X-M2M-RI: " + ri);
  wifi.println("X-M2M-RVI: " + String(RVI));
  wifi.println("X-API-KEY: " + String(API_KEY));
  wifi.println("X-AUTH-CUSTOM-LECTURE: " + String(LECTURE));
  wifi.println("X-AUTH-CUSTOM-CREATOR: " + String(CREATOR));
  wifi.println("Accept: application/json");
  wifi.println("Connection: close");
  wifi.println();

  // Wait for response
  unsigned long t0 = millis();
  while (!wifi.available()) {
    if (millis() - t0 > 8000) {
      Serial.println("[ERROR] Response timeout (GET)");
      wifi.stop();
      return -1;
    }
    delay(10);
  }

  int httpStatus = readHttpStatusLine();
  int rsc = readXM2MRSCFromHeaders();

  Serial.print("[GET] HTTP=");
  Serial.print(httpStatus);
  if (rsc > 0) {
    Serial.print("  X-M2M-RSC=");
    Serial.print(rsc);
  }
  Serial.println();

  drainBodyBrief(500);

  wifi.stop();
  return httpStatus;
}

/* Post Request Method(AE, CNT, CIN) */
int post(String path, String contentType, String name, String content){
  if (!wifi.connect(HOST, PORT)) {
    Serial.println("[ERROR] TLS connect failed");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();
  String body = serializeJsonBody(contentType, name, content);

  // Send HTTP request
  wifi.println("POST " + path + " HTTP/1.1");
  wifi.println("Host: " + String(HOST));
  wifi.println("X-M2M-Origin: " + String(ONEM2M_ORIGIN));
  wifi.println("X-M2M-RI: " + ri);
  wifi.println("X-M2M-RVI: " + String(RVI));
  wifi.println("X-API-KEY: " + String(API_KEY));
  wifi.println("X-AUTH-CUSTOM-LECTURE: " + String(LECTURE));
  wifi.println("X-AUTH-CUSTOM-CREATOR: " + String(CREATOR));

  if(contentType.equals("AE")){
    wifi.println("Content-Type: application/json;ty=2");
  } else if(contentType.equals("CNT")){
    wifi.println("Content-Type: application/json;ty=3");
  } else if(contentType.equals("CIN")){
    wifi.println("Content-Type: application/json;ty=4");
  } else {
    Serial.println("[ERROR] Invalid content type");
    wifi.stop();
    return -1;
  }

  wifi.println("Content-Length: " + String(body.length()));
  wifi.println("Accept: application/json");
  wifi.println("Connection: close");
  wifi.println();
  wifi.print(body);

  // Wait for response
  unsigned long t0 = millis();
  while (!wifi.available()) {
    if (millis() - t0 > 8000) {
      Serial.println("[ERROR] Response timeout (POST)");
      wifi.stop();
      return -1;
    }
    delay(10);
  }

  int httpStatus = readHttpStatusLine();
  int rsc = readXM2MRSCFromHeaders();

  Serial.print("[POST ");
  Serial.print(contentType);
  Serial.print("] HTTP=");
  Serial.print(httpStatus);
  if (rsc > 0) {
    Serial.print("  X-M2M-RSC=");
    Serial.print(rsc);
  }
  Serial.println();

  drainBodyBrief(500);

  wifi.stop();
  return httpStatus;
}

/* Serialize JsonDocument Object to String and returns it */
String serializeJsonBody(String contentType, String name, String content){
  String body;
  StaticJsonDocument<512> doc;

  if(contentType.equals("AE")){
    JsonObject m2m_ae = doc.createNestedObject("m2m:ae");
    m2m_ae["rn"] = name;
    m2m_ae["api"] = "NArduino";
    m2m_ae["rr"] = true;
    JsonArray srv = m2m_ae.createNestedArray("srv");
    srv.add(RVI);
  } else if(contentType.equals("CNT")){
    JsonObject m2m_cnt = doc.createNestedObject("m2m:cnt");
    m2m_cnt["rn"] = name;
    m2m_cnt["mbs"] = 16384;
  } else if(contentType.equals("CIN")){
    JsonObject m2m_cin = doc.createNestedObject("m2m:cin");
    m2m_cin["con"] = content;
  }

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
