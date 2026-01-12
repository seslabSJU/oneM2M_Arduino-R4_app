#define SECRET_SSID "SSID"
#define SECRET_PASS "PASS"


// ==========================================
// WiFi 연결 정보
// ==========================================
#define SECRET_SSID "WiFi SSID"   // 와이파이 SSID
#define SECRET_PASS "WiFi Pass"   // 와이파이 비밀번호

// ==========================================
// IoTcoss 개방형사물인터넷 플랫폼 연결 정보
// ==========================================
#define API_HOST "onem2m.iotcoss.ac.kr"   // 플랫폼 주소
#define ORIGIN "SOrigin_" CHALLENGE_TEAM  // oneM2M Origin(예: SOrigin_team001)

#define HTTPS_PORT 443                    // HTTPS 포트번호

#define API_KEY "API_KEY"
#define LECTURE "LECTURE"
#define CREATOR "CREATOR

#define CHALLENGE_TEAM "team001"          // 챌린지 참가 팀 명칭 (예: team001)

//  oneM2M Notification 수신을 위한 MQTT Subscription topic 
#define NOTY_SUB_TOPIC "/oneM2M/req/Mobius/" ORIGIN "/#"
//  oneM2M Notification을 발생시킨 <sub> 리소스의 경로
#define NOTY_SUB_RESOURCE "Mobius/ae_" CHALLENGE_TEAM "/temp/sub_temp"


