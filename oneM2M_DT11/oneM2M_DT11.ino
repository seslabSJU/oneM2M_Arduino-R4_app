#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Arduino_LED_Matrix.h>

#include "secrets.h"
#include "digit_patterns.h"

#define ARDUINOJSON_SLOT_ID_SIZE 1
#define ARDUINOJSON_STRING_LENGTH_SIZE 1
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0

#define DHTPIN 2        // DHT11 데이터 핀 (디지털 2번)
#define DHTTYPE DHT11

static const int DELAY = 10000;
static const int PORT = 443;
static const char *HOST = "onem2m.iotcoss.ac.kr";  
static const char* ONEM2M_ORIGIN = "SOrigin_12341234_t1";
static const char* RVI = "2a";
static const char* CSEBASE = "/Mobius";
static const char* AE_RN  = "R4_TUTO";         
static const char* TEM_CNT = "TEM";    
static const char* HUM_CNT = "HUM";

bool ready = false;
uint32_t reqSeq = 1;  // RI 생성용 시퀀스
byte frame[8][12];  // 전역 프레임 버퍼

DHT dht(DHTPIN, DHTTYPE);
WiFiSSLClient wifi;
ArduinoLEDMatrix matrix;

void setup() {
  // 시리얼 초기화 및 포트 오픈 대기
  Serial.begin(115200);
  while (!Serial);

  // LED 매트릭스 초기화
  matrix.begin();
  dht.begin();  // DHT11 센서 초기화

  // WiFi 모듈 체크
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!\r\nPlease reset your device!");
    while (true) delay(1000);
  }
  // WiFi 네트워크 연결 시도
  if (!ensureWifiConnected(30000)) {
    Serial.println("[FATAL] WiFi connect failed.");
    while (true) delay(1000);
  }

  // WiFi 연결 후 네트워크 스택 안정화 대기
  delay(5000);

  Serial.println("\n[SETUP] Complete. Entering loop...");
}

void loop() {
  // WiFi 연결 끊김 처리
  if (WiFi.status() != WL_CONNECTED) {
    ready = false;
    Serial.println("[WiFi] Disconnected. Reconnecting...");

    ensureWifiConnected(30000);
    printWifiStatus();
  }

  // 준비되지 않았으면 CB 확인 및 setDevice 재시도
  if (!ready) {
    Serial.println("\n[TRY] GET CB (CSEBase) ...");
    int sc = get(String(CSEBASE));
    Serial.print("[CB] HTTP status = ");
    Serial.println(sc);

    if (sc == 200 || sc == 201) {
      setDevice();
    }
    delay(5000);  // 5초 대기 후 재시도
    return;
  }

  float temperature = dht.readTemperature();  // 온도 (°C)
  float humidity = dht.readHumidity();        // 습도 (%)

  if (isnan(humidity) || isnan(temperature)) {  // 읽기 실패 체크
    Serial.println("[ERROR] DHT11 ERROR");
    delay(2000);
    return;
  }

  Serial.println("----------------------");
  Serial.print("TEM: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("HUM: ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.println("----------------------");

  // LED 매트릭스에 온습도 표시
  displayTemperatureHumidity((int)temperature, (int)humidity);

  Serial.println("[POST] Sending state to server...");
  post(String(CSEBASE) + "/" + AE_RN + "/" + TEM_CNT, "CIN", "", String(temperature));
  post(String(CSEBASE) + "/" + AE_RN + "/" + HUM_CNT, "CIN", "", String(humidity));

  delay(DELAY);
}

// 프레임 초기화 함수
void clear_frame(byte frame[8][12]) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 12; j++) {
      frame[i][j] = 0;
    }
  }
}

// 숫자 패턴을 프레임의 특정 위치에 복사
void add_digit_to_frame(byte frame[8][12], int index, int x, int y) {
  // digit_fonts[index]를 byte[3][5] 형태로 캐스팅
  byte (*digit_pattern)[5] = (byte(*)[5])digit_fonts[index];

  // 3행 × 5열 패턴을 그대로 복사
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 5; col++) {
      frame[x + row][y + col] = digit_pattern[row][col];
    }
  }
}

// 온도와 습도를 LED 매트릭스에 표시
void displayTemperatureHumidity(int temp, int humid) {
  clear_frame(frame);

  // 온도 십의 자리, 일의 자리 계산
  int tempTens = temp / 10;
  int tempOnes = temp % 10;

  // 습도 십의 자리, 일의 자리 계산
  int humidTens = humid / 10;
  int humidOnes = humid % 10;

  // 온도 (왼쪽 절반: y=0~5)
  add_digit_to_frame(frame, tempTens, 5, 0);   // x=5 (아래쪽), y=0
  add_digit_to_frame(frame, tempOnes, 1, 0);   // x=1 (위쪽), y=0

  // 습도 (오른쪽 절반: y=6~11)
  add_digit_to_frame(frame, humidTens, 5, 6);  // x=5 (아래쪽), y=6
  add_digit_to_frame(frame, humidOnes, 1, 6);  // x=1 (위쪽), y=6

  matrix.renderBitmap(frame, 8, 12);
}

/* WiFi 연결 시도 */
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

/* 디바이스 설정 (AE, CNT 확인 및 생성) */
void setDevice(){
  Serial.println("Setup Started!");

  String aePath = String(CSEBASE) + "/" + AE_RN;
  String temPath = aePath + "/" + TEM_CNT;
  String humPath = aePath + "/" + HUM_CNT;

  // 1. AE 확인 -> 없으면 생성
  int aeSc = get(aePath);
  if (aeSc == 404 || aeSc == 403) {
    Serial.println("[setDevice] AE not found -> creating...");
    int cr = post(CSEBASE, "AE", AE_RN, "");
    if (cr == 201 || cr == 200) {
      Serial.println("[setDevice] AE created");
    } else {
      Serial.println("[setDevice] AE create failed");
      ready = false;
      return;
    }
  } else if (aeSc == 200 || aeSc == 201) {
    Serial.println("[setDevice] AE exists");
  } else {
    Serial.println("[setDevice] AE check failed");
    ready = false;
    return;
  }

  // 2. TEM CNT 확인 -> 없으면 생성
  int temSc = get(temPath);
  if (temSc == 404 || temSc == 403) {
    Serial.println("[setDevice] TEM CNT not found -> creating...");
    int cr = post(aePath, "CNT", TEM_CNT, "");
    if (cr == 201 || cr == 200) {
      Serial.println("[setDevice] TEM CNT created");
    } else {
      Serial.println("[setDevice] TEM CNT create failed");
    }
  } else if (temSc == 200 || temSc == 201) {
    Serial.println("[setDevice] TEM CNT exists");
  }

  // 3. HUM CNT 확인 -> 없으면 생성
  int humSc = get(humPath);
  if (humSc == 404 || humSc == 403) {
    Serial.println("[setDevice] HUM CNT not found -> creating...");
    int cr = post(aePath, "CNT", HUM_CNT, "");
    if (cr == 201 || cr == 200) {
      Serial.println("[setDevice] HUM CNT created");
    } else {
      Serial.println("[setDevice] HUM CNT create failed");
    }
  } else if (humSc == 200 || humSc == 201) {
    Serial.println("[setDevice] HUM CNT exists");
  }

  ready = true;
  Serial.println("Setup completed!");
}

/* 고유 RI 생성 */
String nextRI() {
  String ri = "r4-";
  ri += String(reqSeq++);
  return ri;
}

/* HTTP 상태 라인 읽기 */
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

/* GET 요청 (AE, CNT, CIN) */
int get(String path) {
  Serial.println("----------------------");
  if (!wifi.connect(HOST, PORT)) {
    Serial.println("[ERROR] TLS connect failed");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();

  // HTTP 요청 전송
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

  // 응답 대기
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

  Serial.print("[GET] HTTP=");
  Serial.println(httpStatus);

  // 나머지 응답 출력 (헤더 + 본문)
  if (wifi.available()) {
    while (wifi.available()) {
      Serial.write(wifi.read());
    }
    Serial.println();
  }

  wifi.stop();
  Serial.println("----------------------");
  return httpStatus;
}

/* POST 요청 (AE, CNT, CIN) */
int post(String path, String contentType, String name, String content){
  Serial.println("----------------------");
  if (!wifi.connect(HOST, PORT)) {
    Serial.println("[ERROR] TLS connect failed");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();
  String body = serializeJsonBody(contentType, name, content);

  // HTTP 요청 전송
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

  // 응답 대기
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

  Serial.print("[POST ");
  Serial.print(contentType);
  Serial.print("] HTTP=");
  Serial.println(httpStatus);

  // 나머지 응답 출력 (헤더 + 본문)
  if (wifi.available()) {
    while (wifi.available()) {
      Serial.write(wifi.read());
    }
    Serial.println();
  }

  wifi.stop();
  Serial.println("----------------------");
  return httpStatus;
}

/* JSON 본문 직렬화 */
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

/* WiFi 연결 상태 출력 */
void printWifiStatus() {
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Wifi not connected!");
    return;
  }

  // 연결된 네트워크의 SSID 출력
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // 보드의 IP 주소 출력
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // 수신 신호 강도 출력
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}