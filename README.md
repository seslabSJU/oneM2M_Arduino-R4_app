# Arduino R4 WiFi oneM2M tutorial

## Arduino R4 WiFi 소개

### 주요 사양

<img src="https://store.arduino.cc/cdn/shop/files/ABX00087_00.front_643x483.jpg?v=1749566714" width="500" />

- **메인 프로세서**: 32-bit ARM Cortex-M4 (RA4M1, Renesas) - 48MHz
- **무선 모듈**: ESP32-S3 (WiFi 및 Bluetooth 5.0 지원)
- **내장 LED 매트릭스**: 12x8 빨간색 LED (96개)
- **메모리**: 256KB Flash, 32KB RAM
- **동작 전압**: 5V
- **디지털 I/O 핀**: 14개 (PWM 출력 6개)
- **아날로그 입력 핀**: 6개 (14-bit ADC)

### 특징

1. **WiFi 연결**: ESP32-S3 모듈을 통한 WiFi 및 Bluetooth 통신
2. **LED 매트릭스**: 추가 부품 없이 간단한 그래픽, 애니메이션, 텍스트 표시 가능
3. **강력한 성능**: 기존 UNO 대비 3배 이상 빠른 처리 속도
4. **Arduino 호환**: 기존 Arduino 코드 및 라이브러리와 호환

[추가 정보](https://docs.arduino.cc/hardware/uno-r4-wifi/)

## Arduino R4 WiFi 펌웨어 업데이트

- 예제를 실행하기 위해서는 펌웨어 업데이트가 필요합니다.
- 윈도우 환경에서 업데이트하는것을 권장합니다.

1. **Arduino IDE 실행**
   - Arduino IDE를 최신 버전으로 업데이트합니다 (2.0 이상 권장)

2. **보드 연결**
   - Arduino R4 WiFi를 USB 케이블로 PC에 연결합니다

3. **보드 및 포트 선택**
   - Tools > Board > Arduino UNO R4 WiFi 선택
   - Tools > Port에서 연결된 포트 선택

4. **펌웨어 업데이트 실행**
   - Tools > Firmware Updater 선택
   - CHECK UPDATES -> 0.6.0 INSTALL

- [펌웨어 업데이트 실패시](https://forum.arduino.cc/t/arduino-uno-r4-wifi-recognized-as-esp32-devices/1177896/5)

## oneM2M 플랫폼 iotcoss사용법

[https://platform.iotcoss.ac.kr](https://platform.iotcoss.ac.kr)

[iotcoss 학생 가이드](https://platform.iotcoss.ac.kr/guides/student-guide.pptx
)

## 참고 사이트

- ARDUINODOCS: [https://docs.arduino.cc/hardware/uno-r4-wifi/](https://docs.arduino.cc/hardware/) 

- Arduino Project Hub: [https://projecthub.arduino.cc/](https://projecthub.arduino.cc/) 