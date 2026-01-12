// ==========================================
// WiFi 연결 정보
// ==========================================
#define SECRET_SSID "WiFi SSID"   // 와이파이 SSID
#define SECRET_PASS "WiFi Pass"   // 와이파이 비밀번호

// ==========================================
// MQTT 연결 정보
// ==========================================
#define MQTT_HOST "onem2m.iotcoss.ac.kr"  // MQTT 브로커 주소 
#define MQTT_PORT 11883                   // MQTT 브로커 포트번호

// ==========================================
// IoTcoss 개방형사물인터넷 플랫폼 연결 정보
// ==========================================
#define API_HOST "onem2m.iotcoss.ac.kr"   // 플랫폼 주소

#define CHALLENGE_TEAM "team001"          // 챌린지 참가 팀 명칭 (예: team001)
#define ORIGIN "SOrigin_" CHALLENGE_TEAM  // oneM2M Origin(예: SOrigin_team001)

//  oneM2M Notification 수신을 위한 MQTT Subscription topic 
#define NOTY_SUB_TOPIC "/oneM2M/req/Mobius/" ORIGIN "/#"
//  oneM2M Notification을 발생시킨 <sub> 리소스의 경로
#define NOTY_SUB_RESOURCE "Mobius/ae_" CHALLENGE_TEAM "/temp/sub_temp"

    
