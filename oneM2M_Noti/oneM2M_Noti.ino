/*
 * [강의 자료] oneM2M MQTT Notification 실습
 * - 하드웨어: Arduino Uno R4 WiFi
 * - 필수 라이브러리
 *   1. PubSubClient (by Nick O'Leary)
 *   2. ArduinoJson (by Benoit Blanchon)
 * - 보드 매너저 라이브러리(보드 매니저 설치 시 기본 포함됨)
 *   1. WiFiS3
 *   2. Arduino_LED_Matrix
 */
#include <PubSubClient.h>    // MQTT 프로토콜 라이브러리
#include <ArduinoJson.h>     // JSON 파싱 라이브러리
#include "WiFiS3.h"          // Uno R4 WiFi 전용 라이브러리
#include "secrets.h"         // WiFi, 사물인터넷 플랫폼 인증정보, MQTT 접속 정보 포함
#include "MatrixDisplay.h"   // 센서 데이터 표시용 LED Matrix 모듈

// ==========================================
// 전역 변수 선언 (WiFi, MQTT 및 oneM2M Notification 관련 )
// ==========================================
char ssid[] = SECRET_SSID;    // 와이파이 SSID
char pass[] = SECRET_PASS;    // 와이파이 비밀번호

// MQTT 브로커 설정 (Mobius/OM2M 서버 IP)
const char* mqtt_server = MQTT_HOST;  // MQTT 브로커 주소 
const int mqtt_port     = MQTT_PORT;  // MQTT 브로커 포트번호 

//  oneM2M Notification 수신을 위한 MQTT Subscription topic 
const char* mqtt_topic = NOTY_SUB_TOPIC; 
//  oneM2M Notification을 발생시킨 <sub> 리소스의 경로
const char* subscription_resource = NOTY_SUB_RESOURCE; 

// ==========================================
// 전역 객체 선언
// ==========================================
WiFiClient wifi;                  // wifi 연결용 라이브러리 객체 
PubSubClient  mqttClient(wifi);   // MQTT 연결용 클라이언트 라이브러리 객체
MatrixDisplay ledDisplay;         // 센서값 표현을 위한 LED Matrix용 클래스 객체

// ==========================================
// 메시지 수신 및 처리 (Callback)
// ==========================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Message received on topic: ");
  Serial.print(topic);
  Serial.println("");

  // MQTT Message payload를 String으로 변환
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  Serial.println("  - Payload: " + messageTemp);

  // JSON 파싱 (메모리 넉넉하게 할당)
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, messageTemp);

  if (error) {
    Serial.print("[MQTT] JSON Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (!doc.containsKey("pc")) {
    Serial.println("[MQTT] Invalid oneM2M message format.(No 'pc')");

    return;
  }
  
  JsonObject pc = doc["pc"];

  // oneM2M Notification 구조 탐색 ("m2m:sgn" -> "nev" -> "rep" -> "m2m:cin" -> "con")
  if (pc.containsKey("m2m:sgn")) {
    JsonObject sgn = pc["m2m:sgn"];

    // MQTT Notification 메시지가 내가 설정한 관심 subscription에 의해 발행된 메시지인지 확인
    if (sgn.containsKey("sur")) {
      const char* received_sur = sgn["sur"];

      // 수신된 sur와 내가 기대하는 TARGET_SUR 비교
      if (String(received_sur) != String(subscription_resource)) {
        Serial.println(">> [MQTT] 'sur' missmatch. Ignoring message.");
        Serial.print("  - Expected: ");
        Serial.println(subscription_resource);
        Serial.print("  - Received: ");
        Serial.println(received_sur);
        return;
      }
    }

    // 구독 검증 요청(Verification Request) 처리
    // vrq 값이 true인 메시지에는 실제 메시지 내용이 포함되지 않음
    if (sgn.containsKey("vrq")) {
      bool isVerificationMessage = sgn["vrq"];
      if (isVerificationMessage) {
        Serial.println("[MQTT] 'vrq' message. Ignoring message.");
        return;
      }
    }

    // 데이터 알림(Notification Event) 처리
    if (sgn.containsKey("nev")) {
      // 안전하게 중첩 객체 접근
      if (sgn["nev"]["rep"].containsKey("m2m:cin")) {
        const char* con = sgn["nev"]["rep"]["m2m:cin"]["con"]; // 값 추출
        if (con != nullptr) {
          String content = String(con);
          Serial.println("[MQTT] Data received: con=" + content);

          float value = content.toFloat();
          ledDisplay.showFloat(value);
        }
        else {
          Serial.println("[MQTT] No 'con' in received oneM2M resource");
        }
      }
      else {
        Serial.println("[MQTT] Not a expected oneM2M resource.(No 'm2m:cin')");
      }
    }
  } else {
    Serial.println("[MQTT] Invalid oneM2M message format.(No 'm2m:sgn')");
  }
}

// ==========================================
// MQTT 재연결 함수
// ==========================================
void ensureMqttConnected() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Connecting...");
    
    // Client ID 생성 (중복 방지)
    String clientId = "UnoR4Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("\n[MQTT] Connected");

      mqttClient.subscribe(mqtt_topic); // 재연결 후 다시 구독
      Serial.println("[MQTT] Subscribed");
    } else {
      Serial.print("\n[MQTT] Connection failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println("). Retrying in 5 seconds");
      delay(5000);
    }
  }
}


// ==========================================
// WiFi 연결 시도
// ==========================================
bool ensureWifiConnected(unsigned long maxWaitMs) {
  // 이미 연결되어 있으면 즉시 반환
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("[WiFi] Connecting to SSID: ");
  Serial.println(SECRET_SSID);

  // WiFi 연결 시작
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  // 최대 대기 시간까지 연결 시도
  unsigned long start = millis();
  Serial.print("\n[WiFi] Connecting.");
  while (millis() - start < maxWaitMs) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[WiFi] Connected.");
      return true;
    }
    delay(250);  // 250ms 대기
    Serial.print(".");  // 연결 시도 진행 표시
  }
  Serial.println();
  return (WiFi.status() == WL_CONNECTED);
}


/* WiFi 연결 상태 출력 */
void printWifiStatus() {
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("[Wifi] Not connected!");
    return;
  }

  // 연결된 네트워크의 SSID 출력
  Serial.println("[Wifi] WiFi Status Report");
  Serial.print("  - SSID: ");
  Serial.println(WiFi.SSID());

  // 보드의 IP 주소 출력
  IPAddress ip = WiFi.localIP();
  Serial.print("  - IP Address: ");
  Serial.println(ip);

  // 수신 신호 강도 출력
  long rssi = WiFi.RSSI();
  Serial.print("  - Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// ==========================================
// Setup
// ==========================================
void setup() {
  //  시리얼 초기화 및 포트 오픈 대기
  Serial.begin(115200);
  while (!Serial);

  //  LED 매트릭스 초기화 
  ledDisplay.begin();

  // WiFi 모듈 체크
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("[SETUP] Communication with WiFi module failed!");
    Serial.println("        Please reset your device!");
    while (true) delay(1000);
  }

  // WiFi 네트워크 연결 시도
  if (!ensureWifiConnected(30000)) {
    Serial.println("[SETUP] [FATAL] WiFi connect failed.");
    while (true) delay(1000);
  }

  // WiFi 연결 후 네트워크 스택 안정화 대기
  delay(5000);

  // MQTT Broker 연결 정보 설정
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(2048); // oneM2M 메시지 수신에 충분한 크기의 버퍼 설정
  mqttClient.setKeepAlive(60000); // 기본 15초 -> 60초로 증가 (연결 끊김 방지용)

  Serial.println("[SETUP] Complete. Entering loop.");
}

// ==========================================
// Loop
// ==========================================
void loop() {
  // WiFi 연결 끊김 처리
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected. Reconnecting...");

    ensureWifiConnected(30000);
    printWifiStatus();
  }

  // MQTT 연결 끊김 처리
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Disconnected. Reconnecting...");
    ensureMqttConnected();
  }
  mqttClient.loop();

  delay(10);
}