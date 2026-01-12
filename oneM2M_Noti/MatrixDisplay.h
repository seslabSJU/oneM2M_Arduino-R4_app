/*
 * MatrixDisplay.h
 * 설명: Arduino Uno R4 LED Matrix 제어용 클래스 헤더
 */

#ifndef MATRIX_DISPLAY_H
#define MATRIX_DISPLAY_H

#include <Arduino.h>
#include "Arduino_LED_Matrix.h"

class MatrixDisplay {
  private:
    ArduinoLEDMatrix matrix;
    
    // 내부에서만 쓰는 헬퍼 함수들
    void drawDigit(uint8_t frame[8][12], int num, int startX, int startY);
    void drawPixel(uint8_t frame[8][12], int x, int y);

  public:
    // 생성자
    MatrixDisplay();

    // 초기화 함수
    void begin();

    // 기능 함수들
    void showFloat(float value);  // 99.9 실수 출력
};

#endif

