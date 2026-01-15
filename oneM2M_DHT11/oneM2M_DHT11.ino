#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Arduino_LED_Matrix.h>

#include "secrets.h"                 // WiFi 및 API 인증 정보
#include "digit_patterns.h"          // 숫자 0~9 패턴 정의

// ArduinoJson 메모리 최적화 설정
#define ARDUINOJSON_SLOT_ID_SIZE 1           // 슬롯 ID 크기 최소화
#define ARDUINOJSON_STRING_LENGTH_SIZE 1     // 문자열 길이 크기 최소화
#define ARDUINOJSON_USE_DOUBLE 0             // double 타입 비활성화 (메모리 절약)
#define ARDUINOJSON_USE_LONG_LONG 0          // long long 타입 비활성화

// DHT11 센서 설정
#define DHTPIN 2                      // DHT11 데이터 핀 (디지털 2번)
#define DHTTYPE DHT11                 // 센서 타입

const int DELAY = 10000;              // 데이터 전송 주기 (10초)
const char* RVI = "2a";               // oneM2M 릴리즈 버전

// oneM2M 리소스 경로 설정
String CSEBASE = "/Mobius";           // CSE Base 이름
String R4_AE = String("R4_TUTO_") + String(ORIGIN);  // Application Entity 이름
String TEM_CNT = "TEM";               // 온도 Container 이름
String HUM_CNT = "HUM";               // 습도 Container 이름

// 전역 변수
bool ready = false;                   // oneM2M 리소스 초기화 완료 여부
uint32_t reqSeq = 1;                  // HTTP 요청 ID 생성용 시퀀스 번호
byte frame[8][12];                    // LED 매트릭스 프레임 버퍼 (8행 x 12열)

// 객체 생성
DHT dht(DHTPIN, DHTTYPE);             // DHT11 센서 객체
WiFiSSLClient wifi;                   // HTTPS 통신 클라이언트 객체
ArduinoLEDMatrix matrix;              // LED 매트릭스 객체

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
  Serial.println("[SETUP] Complete. Entering loop...");
}

void loop() {
  // WiFi 연결 끊김 처리
  if (WiFi.status() != WL_CONNECTED) {
    ready = false;
    Serial.println("[WiFi] Disconnected. Reconnecting...");
    ensureWifiConnected(30000);  // WiFi 연결 시도
  }

  // 준비되지 않았으면 CB 확인 및 setDevice 재시도
  if (!ready) {
    Serial.println("[TRY] GET CB (CSEBase) ...");
    int sc = get(CSEBASE);
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
  postCIN(CSEBASE + "/" + R4_AE + "/" + TEM_CNT, String(temperature));
  postCIN(CSEBASE + "/" + R4_AE + "/" + HUM_CNT, String(humidity));

  delay(DELAY);
}

// WiFi 연결 시도
bool ensureWifiConnected(unsigned long maxWaitMs) {
  // 이미 연결되어 있으면 즉시 반환
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("[WiFi] Connecting to SSID: ");
  Serial.println(SECRET_SSID);

  // WiFi 연결 시작
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  // 최대 대기 시간까지 연결 시도
  unsigned long start = millis();
  while (millis() - start < maxWaitMs) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Connected.");

      // WiFi 연결 정보 출력
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());

      IPAddress ip = WiFi.localIP();
      Serial.print("IP Address: ");
      Serial.println(ip);

      long rssi = WiFi.RSSI();
      Serial.print("signal strength (RSSI):");
      Serial.print(rssi);
      Serial.println(" dBm");

      return true;
    }
    delay(250);  // 250ms 대기
    Serial.print(".");  // 연결 시도 진행 표시
  }
  Serial.println();
  return (WiFi.status() == WL_CONNECTED);
}

// 고유 RI 생성 (예: "r4-1", "r4-2", "r4-3" ...)
String nextRI() {
  String ri = "r4-";
  ri += String(reqSeq++);
  return ri;
}

// HTTP 응답에서 상태 코드 추출 (예: "HTTP/1.1 200 OK" → 200)
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

// GET 요청 (리소스 조회)
int get(String path) {
  Serial.println("----------------------");
  if (!wifi.connect(API_HOST, HTTPS_PORT)) {
    Serial.println("[ERROR] TLS connect failed");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();

  // HTTP 요청 전송
  wifi.println("GET " + path + " HTTP/1.1");
  wifi.println("Host: " + String(API_HOST));
  wifi.println("X-M2M-Origin: " + String(ORIGIN));
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
    if (millis() - t0 > DELAY) {
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

// AE 생성 요청 
int postAE(String path, String resourceName){
  String body = serializeAE(resourceName);  // AE용 JSON 본문 직렬화
  return post(path, "AE", 2, body);         // ty=2 (AE 타입)
}

// CNT 생성 요청
int postCNT(String path, String resourceName){
  String body = serializeCNT(resourceName);  // CNT용 JSON 본문 직렬화
  return post(path, "CNT", 3, body);         // ty=3 (CNT 타입)
}

// CIN 생성 요청 (데이터 전송)
int postCIN(String path, String content){
  String body = serializeCIN(content);       // CIN용 JSON 본문 직렬화
  return post(path, "CIN", 4, body);         // ty=4 (CIN 타입)
}

// AE용 JSON 본문 직렬화
String serializeAE(String resourceName){
  String body;
  StaticJsonDocument<512> doc;  // JSON 문서 생성 (최대 512바이트)

  // m2m:ae 객체 생성
  JsonObject m2m_ae = doc.createNestedObject("m2m:ae");
  m2m_ae["rn"] = resourceName;      // 리소스 이름 (Resource Name)
  m2m_ae["api"] = "NArduino";       // App ID
  m2m_ae["rr"] = true;              // Request Reachability (요청 도달 가능 여부)

  // srv 배열 생성 및 RVI 추가
  JsonArray srv = m2m_ae.createNestedArray("srv");
  srv.add(RVI);  // 지원하는 릴리즈 버전 추가

  serializeJson(doc, body);  // JSON 객체를 문자열로 변환
  return body;
  /* 생성되는 JSON 예시:
   * {
   *   "m2m:ae": {
   *     "rn": "R4_TUTO",
   *     "api": "NArduino",
   *     "rr": true,
   *     "srv": ["2a"]
   *   }
   * }
   */
}

// CNT용 JSON 본문 직렬화
String serializeCNT(String resourceName){
  String body;
  StaticJsonDocument<512> doc;  // JSON 문서 생성 (최대 512바이트)

  // m2m:cnt 객체 생성
  JsonObject m2m_cnt = doc.createNestedObject("m2m:cnt");
  m2m_cnt["rn"] = resourceName;  // 리소스 이름 (Resource Name)
  m2m_cnt["mbs"] = 16384;        // 최대 바이트 크기 (Max Byte Size)

  serializeJson(doc, body);  // JSON 객체를 문자열로 변환
  return body;
  /* 생성되는 JSON 예시:
   * {
   *   "m2m:cnt": {
   *     "rn": "TEM",
   *     "mbs": 16384
   *   }
   * }
   */
}

// CIN용 JSON 본문 직렬화
String serializeCIN(String content){
  String body;
  StaticJsonDocument<512> doc;  // JSON 문서 생성 (최대 512바이트)

  // m2m:cin 객체 생성
  JsonObject m2m_cin = doc.createNestedObject("m2m:cin");
  m2m_cin["con"] = content;  // 전송할 데이터 내용 (Content)

  serializeJson(doc, body);  // JSON 객체를 문자열로 변환
  return body;
  /* 생성되는 JSON 예시:
   * {
   *   "m2m:cin": {
   *     "con": "25.3"
   *   }
   * }
   */
}

// POST 요청 공통 함수 (내부용)
int post(String path, String contentType, int ty, String body){
  Serial.println("----------------------");
  if (!wifi.connect(API_HOST, HTTPS_PORT)) {
    Serial.println("[ERROR] TLS connect failed");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();

  // HTTP 요청 전송
  wifi.println("POST " + path + " HTTP/1.1");
  wifi.println("Host: " + String(API_HOST));
  wifi.println("X-M2M-Origin: " + String(ORIGIN));
  wifi.println("X-M2M-RI: " + ri);
  wifi.println("X-M2M-RVI: " + String(RVI));
  wifi.println("X-API-KEY: " + String(API_KEY));
  wifi.println("X-AUTH-CUSTOM-LECTURE: " + String(LECTURE));
  wifi.println("X-AUTH-CUSTOM-CREATOR: " + String(CREATOR));
  wifi.println("Content-Type: application/json;ty=" + String(ty));
  wifi.println("Content-Length: " + String(body.length()));
  wifi.println("Accept: application/json");
  wifi.println("Connection: close");
  wifi.println();
  wifi.print(body);

  // 응답 대기
  unsigned long t0 = millis();
  while (!wifi.available()) {
    if (millis() - t0 > DELAY) {
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

// 디바이스 설정 (AE, CNT 확인 및 생성)
void setDevice(){
  Serial.println("Setup Started!");

  String aePath = CSEBASE + "/" + R4_AE;
  String temPath = aePath + "/" + TEM_CNT;
  String humPath = aePath + "/" + HUM_CNT;

  // 1. AE 확인 -> 없으면 생성
  int aeSc = get(aePath);
  if (aeSc == 404 || aeSc == 403) {
    Serial.println("[setDevice] AE not found -> creating...");
    int cr = postAE(CSEBASE, R4_AE);
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
    int cr = postCNT(aePath, TEM_CNT);
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
    int cr = postCNT(aePath, HUM_CNT);
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