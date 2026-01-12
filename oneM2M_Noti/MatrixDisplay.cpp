/*
 * MatrixDisplay.cpp
 * 설명: LED Matrix 제어 로직 구현부
 */

#include "MatrixDisplay.h"

// --------------------------------------------------------
// [숨김 처리] 폰트 데이터는 여기에 둡니다 (Main 코드가 깔끔해짐)
// --------------------------------------------------------

// 3x5 초소형 폰트 (Micro Font)
static const byte fontDigits[10][5] = {
  { 0x7, 0x5, 0x5, 0x5, 0x7 }, // 0
  { 0x2, 0x2, 0x2, 0x2, 0x2 }, // 1
  { 0x7, 0x1, 0x7, 0x4, 0x7 }, // 2
  { 0x7, 0x1, 0x7, 0x1, 0x7 }, // 3
  { 0x5, 0x5, 0x7, 0x1, 0x1 }, // 4
  { 0x7, 0x4, 0x7, 0x1, 0x7 }, // 5
  { 0x7, 0x4, 0x7, 0x5, 0x7 }, // 6
  { 0x7, 0x1, 0x1, 0x1, 0x1 }, // 7
  { 0x7, 0x5, 0x7, 0x5, 0x7 }, // 8
  { 0x7, 0x5, 0x7, 0x1, 0x7 }  // 9
};

// --------------------------------------------------------
// 구현부 시작
// --------------------------------------------------------

MatrixDisplay::MatrixDisplay() {
  // 생성자
}

void MatrixDisplay::begin() {
  matrix.begin();
}

// 내부 헬퍼 함수
void MatrixDisplay::drawDigit(uint8_t frame[8][12], int num, int sX, int sY) {
  if (num < 0 || num > 9) return;
  for (int y = 0; y < 5; y++) {
    byte rowData = fontDigits[num][y]; 
    for (int x = 0; x < 3; x++) {
      if ((rowData >> (2 - x)) & 0x01) {
        if (sY + y < 8 && sX + x < 12) {
          frame[sY + y][sX + x] = 1;
        }
      }
    }
  }
}

void MatrixDisplay::drawPixel(uint8_t frame[8][12], int x, int y) {
  if (x < 0 || x > 11) return;
  if (y < 0 || y > 8) return;
  frame[y][x] = 1;
}

// 실수 출력 함수 (반올림 포함)
void MatrixDisplay::showFloat(float value) {
  if (value < 0) value = 0;
  if (value > 99.9) value = 99.9;

  int temp = (int)(value * 10.0 + 0.5); // 반올림 처리
  int tens = (temp / 100);
  int units = (temp / 10) % 10;
  int decimal = temp % 10;

  uint8_t frame[8][12];
  memset(frame, 0, sizeof(frame)); // 배열 0으로 초기화

  if (tens > 0) drawDigit(frame, tens, 0, 1);
  drawDigit(frame, units, 4, 1);
  drawPixel(frame, 8, 6);
  drawDigit(frame, decimal, 9, 1);

  matrix.renderBitmap(frame, 8, 12);
}