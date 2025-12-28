#include <RTC.h>
RTCTime currentTime;

// UNO-R4 Wifi LED Matrix:

#include "Arduino_LED_Matrix.h"
#include "fonts.h"
ArduinoLEDMatrix matrix;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(1500);
  Serial.println(__FILE__);
  // a lot of work to extract HMS from __TIME__:
  // hour
  char time[] = __TIME__;
  char temp[8];
  strncpy(temp, time, 2);
  int h = atoi(temp);
  // minute:
  memset(time, ' ', 3);
  strncpy(temp, time, 5);
  int m = atoi(temp);
  // second:
  memset(time, ' ', 6);
  int s = atoi(time);
  // RTC:
  RTC.begin();
  /*
    name of month, day of week, and saveLight
    are not shown and therefore ignored
  */
  RTCTime startTime(1, Month::JANUARY, 13, h, m, s, DayOfWeek::MONDAY, SaveLight::SAVING_TIME_ACTIVE);
  RTC.setTime(startTime);
  matrix.begin();
}

void loop() {
  int h10, h01, m10, m01, s10, s01;
  // Get current time from RTC
  RTC.getTime(currentTime);
  int hour = currentTime.getHour();
  int minute = currentTime.getMinutes();
  int seconds = currentTime.getSeconds();
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.println(seconds);
  h10 = hour / 10;
  h01 = hour % 10;
  m10 = minute / 10;
  m01 = minute % 10;
  s10 = seconds / 10;
  s01 = seconds % 10;
  clear_frame();
  // Matrix row/col Hochformat
  // row: links 7, rechts 0
  // col: oben 0, unten 11
  add_to_frame(h10, 5, 0);
  add_to_frame(h01, 1, 0);
  add_to_frame(m10, 5, 6);
  add_to_frame(m01, 1, 6);
  display_frame();
  delay(5000);
}

uint8_t frame[8][12];

void clear_frame() {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 12; col++) {
      frame[row][col] = 0;
    }
  }
}

void display_frame() {
  matrix.renderBitmap(frame, 8, 12);
}

void add_to_frame(int index, int x, int y) {
  // digit_fonts[index]를 byte[3][5] 형태로 캐스팅
  byte (*digit_pattern)[5] = (byte(*)[5])digit_fonts[index];

  // 3행 × 5열 패턴을 그대로 복사
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 5; col++) {
      frame[x + row][y + col] = digit_pattern[row][col];
    }
  }
}