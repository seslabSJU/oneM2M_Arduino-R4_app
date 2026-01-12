#include <DHT.h>
#include <Arduino_LED_Matrix.h>
// 숫자 0~9 패턴 정의
#include "digit_patterns.h"

// DHT11 센서 설정
#define DHTPIN 2        // DHT11 데이터 핀 (디지털 2번)
#define DHTTYPE DHT11   // 센서 타입

// DHT11 센서 객체 생성
DHT dht(DHTPIN, DHTTYPE);

// LED 매트릭스 객체 생성
ArduinoLEDMatrix matrix;

// LED 매트릭스 프레임 버퍼 (8행 x 12열)
byte frame[8][12];

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

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("DHT11 온습도 센서 + LED 매트릭스 테스트");
  Serial.println("======================================");

  // LED 매트릭스 초기화
  matrix.begin();

  // DHT11 센서 초기화
  dht.begin();
  delay(2000);  // 센서 안정화 대기
}

void loop() {
  // DHT11은 최소 2초 간격으로 읽어야 함
  float humidity = dht.readHumidity();        // 습도 (%)
  float temperature = dht.readTemperature();  // 온도 (°C)

  // 읽기 실패 체크
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("[ERROR] DHT11 센서 읽기 실패!");
    delay(2000);
    return;
  }

  // 시리얼 모니터 출력
  Serial.println("----------------------");
  Serial.print("온도: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("습도: ");
  Serial.print(humidity);
  Serial.println(" %");

  // LED 매트릭스에 온습도 표시
  displayTemperatureHumidity((int)temperature, (int)humidity);

  // 2초 대기 (DHT11 읽기 간격)
  delay(2000);
}
