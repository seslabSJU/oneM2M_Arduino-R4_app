## 문서 안내

- 목적
  - iot-coss 플랫폼 사이트의 tinyIoT/Mobius 플랫폼을 활용하여 아두이노 디바이스가 oneM2M 리소스를 만들고 데이터를 올리는 일련의 과정을 설명, 그리고 실제 구성을 안내

- 대상
  - iot-coss 소속 5개 대학(대전대, 동국대, 세종대, 제주대, 제주한라대) 1~2학년 학생들

- 준비물
  - 개인 PC, 아두이노 키트(arduino R4 wifi 모델) 등

## iot-coss 개방형 사물인터넷 플랫폼 소개 및 사용 방법

- 강의 교안 사용할예정

## iot-coss tinyIoT/Mobius API test 가이드

- 강의 교안의 swagger문서 사용법 사용

## Arduino 소개와 활용

- Arduion R4 Uno
- 키트 내용물 소개
- 키트 구매처에 내용물 및 사용법 있는지 확인 후 작성

## Arduino - oneM2M 플랫폼 연결 예제 소개

- oneM2M_Arduino_R4 폴더 코드 사용(url등만 살짝 조정)

## 추가자료제공

https://docs.arduino.cc/hardware/uno-r4-wifi/#tutorials

## 최종적으로 gitbook혹은 mkdocs사용



✓ TCP 연결 성공 - 네트워크/방화벽은 문제없음
✗ SSL 연결 실패 - WiFiSSLClient가 이 서버의 SSL과 호환 안됨
문제는 Arduino R4 WiFi의 WiFiSSLClient가 이 서버의 TLS 설정과 호환되지 않는다는 것입니다. OpenSSL 테스트에서 봤던 정보:
서버: TLSv1.3, ECDSA 인증서 (prime256v1)
Cipher: AEAD-CHACHA20-POLY1305-SHA256
Arduino R4 WiFi가 이런 최신 암호화 방식을 지원 안할 가능성이 높습니다.
해결 방법:
가장 현실적: 서버 관리자에게 TLSv1.2 지원 요청 (Arduino 호환성을 위해)
차선책: 중간 프록시 서버 구축 (ESP32나 라즈베리파이로 HTTP→HTTPS 변환)
최후: 다른 보드 사용 (ESP32는 최신 TLS 잘 지원)
서버 측에서 TLS 설정을 조정할 수 있나요? 아니면 프록시 서버 만들어야 할 것 같습니다.