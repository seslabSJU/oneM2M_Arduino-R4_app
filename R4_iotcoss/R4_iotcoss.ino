/*
  UNO R4 WiFi oneM2M (iot-coss Mobius) Tutorial - Step 1
  - HTTPS (WiFiSSLClient)
  - setup: WiFi -> GET CB -> ensure AE/CNT
  - loop : POST CIN every 5 seconds (dummy payload)
  - Keeps "lab style" structure: setDevice(), get(), post(), serializeJsonBody()
*/

#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>
#include "secrets.h"   // SECRET_SSID, SECRET_PASS, IOTCOSS_API_KEY, IOTCOSS_LECTURE, IOTCOSS_CREATOR, ONEM2M_ORIGIN

// ------------------------------
// Target (iot-coss Mobius)
// ------------------------------
static const char* HOST = "onem2m.iotcoss.ac.kr";   // IMPORTANT: no "https://"
static const uint16_t PORT = 443;
static const char* CSEBASE = "/Mobius";             // CB path

// ------------------------------
// Demo resource names (수업 규칙에 맞게 바꿔)
// ------------------------------
static const char* AE_RN  = "R4_TUTORIAL_AE";       // 예: AIoT-HW-학번 형태 권장
static const char* CNT_RN = "demo_cnt";

// oneM2M release indicator you want to use
static const char* RVI = "2a";

// interval
static const unsigned long PERIOD_MS = 5000;

//originator
static const char* ONEM2M_ORIGIN = "SOrigin_12341234_de";

// ------------------------------
WiFiSSLClient wifi;
bool ready = false;
unsigned long lastTick = 0;
uint32_t reqSeq = 1;

// ------------------------------
// Forward decl
// ------------------------------
static bool ensureWifiConnected(unsigned long maxWaitMs);
static void printWifiStatus();

static void setDevice();

static int  get(const String& path);  // returns HTTP status code, or -1 on connect/timeout
static int  post(const String& path, const String& ty, const String& rn, const String& con); // returns HTTP status code, or -1

static String serializeJsonBody(const String& ty, const String& rn, const String& con);

static String nextRI();
static int  readHttpStatusLine();
static int  readXM2MRSCFromHeaders();         // optional debug
static void drainBodyBrief(size_t maxBytes);  // optional debug

// ======================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println();
  Serial.println("==============================================");
  Serial.println(" UNO R4 WiFi oneM2M Tutorial (AE/CNT/CIN)");
  Serial.println("==============================================");

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("[FATAL] WiFi module not found. Reset board.");
    while (true) delay(1000);
  }

  if (!ensureWifiConnected(30000)) {
    Serial.println("[FATAL] WiFi connect failed.");
    while (true) delay(1000);
  }
  printWifiStatus();

  // 1) CB check
  Serial.println("\n[STEP] GET CB (CSEBase) ...");
  int sc = get(String(CSEBASE));
  Serial.print("[CB] HTTP status = ");
  Serial.println(sc);

  if (sc == 200 || sc == 201) {
    // 2) AE/CNT ensure
    setDevice();
  } else {
    Serial.println("[WARN] CB not reachable now. Will retry in loop.");
    ready = false;
  }

  lastTick = millis();
}

void loop() {
  unsigned long now = millis();

  // WiFi drop handling
  if (WiFi.status() != WL_CONNECTED) {
    ready = false;
    Serial.println("[WiFi] Disconnected. Reconnecting...");
    ensureWifiConnected(30000);
    printWifiStatus();
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
    }
    return;
  }

  // Ready -> send CIN every 5 sec
  static uint32_t cinCounter = 0;
  cinCounter++;

  String aePath  = String(CSEBASE) + "/" + AE_RN;
  String cntPath = aePath + "/" + CNT_RN;

  String con = String("dummy-") + String(cinCounter);  // 센서 없이 더미 값
  Serial.println("\n[STEP] POST CIN ...");
  int sc = post(cntPath, "CIN", "", con);
  Serial.print("[CIN] HTTP status = ");
  Serial.println(sc);
}

// ======================================================================
// WiFi
// ======================================================================
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

static void printWifiStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Not connected.");
    return;
  }
  Serial.print("[WiFi] SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("[WiFi] RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

// ======================================================================
// oneM2M procedure (AE/CNT ensure)
// ======================================================================
static void setDevice() {
  Serial.println("\n[setDevice] Start");

  String aePath  = String(CSEBASE) + "/" + AE_RN;
  String cntPath = aePath + "/" + CNT_RN;

  // 1) AE exists?
  Serial.println("[setDevice] Check AE ...");
  int aeSc = get(aePath);
  Serial.print("[AE GET] HTTP status = ");
  Serial.println(aeSc);

  if (aeSc == 404) {
    Serial.println("[setDevice] AE not found -> create AE");
    int cr = post(String(CSEBASE), "AE", AE_RN, "");
    Serial.print("[AE CREATE] HTTP status = ");
    Serial.println(cr);

    // 201 Created, 200 OK, or 409 Conflict(이미 있음) 정도는 통과 처리
    if (!(cr == 201 || cr == 200 || cr == 409)) {
      Serial.println("[setDevice] AE create failed -> not ready");
      ready = false;
      return;
    }
  } else if (aeSc == 200 || aeSc == 201) {
    Serial.println("[setDevice] AE exists");
  } else {
    Serial.println("[setDevice] AE check failed -> not ready");
    ready = false;
    return;
  }

  // 2) CNT exists?
  Serial.println("[setDevice] Check CNT ...");
  int cntSc = get(cntPath);
  Serial.print("[CNT GET] HTTP status = ");
  Serial.println(cntSc);

  if (cntSc == 404) {
    Serial.println("[setDevice] CNT not found -> create CNT");
    int cr = post(aePath, "CNT", CNT_RN, "");
    Serial.print("[CNT CREATE] HTTP status = ");
    Serial.println(cr);

    if (!(cr == 201 || cr == 200 || cr == 409)) {
      Serial.println("[setDevice] CNT create failed -> not ready");
      ready = false;
      return;
    }
  } else if (cntSc == 200 || cntSc == 201) {
    Serial.println("[setDevice] CNT exists");
  } else {
    Serial.println("[setDevice] CNT check failed -> not ready");
    ready = false;
    return;
  }

  Serial.println("[setDevice] Completed -> READY");
  ready = true;
}

// ======================================================================
// HTTP GET/POST (lab-style)
// ======================================================================
static int get(const String& path) {
  // TLS connect
  if (!wifi.connect(HOST, PORT)) {
    Serial.println("[ERROR] TLS connect failed (wifi.connect == false)");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();

  // HTTP/1.1 inside TLS is correct.
  wifi.print("GET ");
  wifi.print(path);
  wifi.println(" HTTP/1.1");

  wifi.print("Host: ");
  wifi.println(HOST);

  wifi.print("X-M2M-Origin: ");
  wifi.println(ONEM2M_ORIGIN);

  wifi.print("X-M2M-RI: ");
  wifi.println(ri);

  wifi.print("X-M2M-RVI: ");
  wifi.println(RVI);

  // iot-coss auth headers
  wifi.print("X-API-KEY: ");
  wifi.println(IOTCOSS_API_KEY);

  wifi.print("X-AUTH-CUSTOM-LECTURE: ");
  wifi.println(IOTCOSS_LECTURE);

  wifi.print("X-AUTH-CUSTOM-CREATOR: ");
  wifi.println(IOTCOSS_CREATOR);

  wifi.println("Accept: application/json");
  wifi.println("Connection: close");
  wifi.println();

  // wait response
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
  int rsc = readXM2MRSCFromHeaders(); // optional debug

  Serial.print("[GET] HTTP=");
  Serial.print(httpStatus);
  if (rsc > 0) {
    Serial.print("  X-M2M-RSC=");
    Serial.print(rsc);
  }
  Serial.println();

  // body 일부 출력 (너무 길면 지저분해져서 500바이트만)
  drainBodyBrief(500);

  wifi.stop();
  return httpStatus;
}

static int post(const String& path, const String& ty, const String& rn, const String& con) {
  if (!wifi.connect(HOST, PORT)) {
    Serial.println("[ERROR] TLS connect failed (wifi.connect == false)");
    wifi.stop();
    return -1;
  }

  String ri = nextRI();

  String body = serializeJsonBody(ty, rn, con);

  wifi.print("POST ");
  wifi.print(path);
  wifi.println(" HTTP/1.1");

  wifi.print("Host: ");
  wifi.println(HOST);

  wifi.print("X-M2M-Origin: ");
  wifi.println(ONEM2M_ORIGIN);

  wifi.print("X-M2M-RI: ");
  wifi.println(ri);

  wifi.print("X-M2M-RVI: ");
  wifi.println(RVI);

  // iot-coss auth headers
  wifi.print("X-API-KEY: ");
  wifi.println(IOTCOSS_API_KEY);

  wifi.print("X-AUTH-CUSTOM-LECTURE: ");
  wifi.println(IOTCOSS_LECTURE);

  wifi.print("X-AUTH-CUSTOM-CREATOR: ");
  wifi.println(IOTCOSS_CREATOR);

  // oneM2M ty mapping
  if (ty == "AE") {
    wifi.println("Content-Type: application/json;ty=2");
  } else if (ty == "CNT") {
    wifi.println("Content-Type: application/json;ty=3");
  } else if (ty == "CIN") {
    wifi.println("Content-Type: application/json;ty=4");
  } else {
    Serial.println("[ERROR] Invalid ty in post()");
    wifi.stop();
    return -1;
  }

  wifi.print("Content-Length: ");
  wifi.println(body.length());

  wifi.println("Accept: application/json");
  wifi.println("Connection: close");
  wifi.println();
  wifi.print(body);

  // wait response
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
  int rsc = readXM2MRSCFromHeaders(); // optional debug

  Serial.print("[POST ");
  Serial.print(ty);
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

// ======================================================================
// JSON body builder (ArduinoJson)
// ======================================================================
static String serializeJsonBody(const String& ty, const String& rn, const String& con) {
  String body;
  StaticJsonDocument<512> doc;

  if (ty == "AE") {
    JsonObject ae = doc.createNestedObject("m2m:ae");
    ae["rn"] = rn;
    ae["api"] = "NArduino";     // 필요 시 변경
    ae["rr"] = true;

    // supported release versions (요청 RVI와 맞추는 게 깔끔)
    JsonArray srv = ae.createNestedArray("srv");
    srv.add(RVI);

  } else if (ty == "CNT") {
    JsonObject cnt = doc.createNestedObject("m2m:cnt");
    cnt["rn"] = rn;
    cnt["mbs"] = 16384;

  } else if (ty == "CIN") {
    JsonObject cin = doc.createNestedObject("m2m:cin");
    cin["con"] = con;

  } else {
    // invalid
  }

  serializeJson(doc, body);
  return body;
}

// ======================================================================
// helpers
// ======================================================================
static String nextRI() {
  // 요청마다 고유 RI 생성
  String ri = "r4-";
  ri += String(reqSeq++);
  return ri;
}

static int readHttpStatusLine() {
  // Example: "HTTP/1.1 200 OK\r\n"
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

static int readXM2MRSCFromHeaders() {
  // 헤더 끝(빈 줄)까지 읽으면서 X-M2M-RSC만 뽑아봄
  int rsc = -1;

  while (wifi.connected()) {
    String line = wifi.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;

    // "X-M2M-RSC: 2000"
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

static void drainBodyBrief(size_t maxBytes) {
  // body를 너무 많이 찍으면 시리얼이 지저분해져서 일부만 출력
  size_t printed = 0;
  while (wifi.available() && printed < maxBytes) {
    char c = (char)wifi.read();
    Serial.print(c);
    printed++;
  }
  if (wifi.available()) {
    Serial.println("\n[Body truncated]");
    // 남은 데이터는 버림
    while (wifi.available()) (void)wifi.read();
  } else {
    Serial.println();
  }
}
