#include <Arduino.h>
#include <ESPmDNS.h>
#include <ConfigPortal32.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHTesp.h>  // DHTesp 라이브러리 추가

char* ssid_pfix = (char*)"lab5";
String user_config_html = ""
    "<p><input type='text' name='meta.influxdbAddress' placeholder='InfluxDB Address'></p>"
    "<p><input type='text' name='meta.influxDBToken' placeholder='InfluxDB Token'></p>" 
    "<p><input type='text' name='meta.bucket' placeholder='InfluxDB bucket'></p>" 
    "<p><input type='number' name='meta.reportInterval' placeholder='Report Interval (ms)'></p>"; // reportInterval 입력 필드 추가

int report = 2000; // 기본 보고 간격 (2초)
char influxdbAddress[50];
String influxDBURL = "";  // 빈 문자열로 초기화
WebServer server(80);
#define RELAY 4
#define DHTPIN 15  // DHT 센서의 핀 번호

DHTesp dht;  // DHTesp 객체 생성

String influxDBToken = "";  // InfluxDB 토큰을 저장할 변수
String bucket = "";  // 버킷 값을 저장할 변수

void sendToInfluxDB(float temperature, float humidity) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // InfluxDB URL에 bucket 추가
        String influxDBURLWithBucket = "http://" + String(influxdbAddress) + ":8086/api/v2/write?bucket=" + bucket + "&org=1f17224af73c9ba2&precision=s";

        String payload = "temperature,room=room2 value=" + String(temperature) + "\n";
        payload += "humidity,room=room2 value=" + String(humidity);

        http.begin(influxDBURLWithBucket); // InfluxDB URL 설정
        http.addHeader("Authorization", "Token " + influxDBToken);  // 인증 헤더 추가
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        int httpCode = http.POST(payload);  // 데이터 전송
        if (httpCode > 0) {
            Serial.printf("InfluxDB Response Code: %d\n", httpCode);
        } else {
            Serial.printf("Error sending data to InfluxDB: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end(); // HTTP 요청 종료
    } else {
        Serial.println("WiFi not connected, skipping InfluxDB update.");
    }
}

void setup() {
    Serial.begin(115200);

    loadConfig();
    if(!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
        configDevice();
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.printf("Connected to WiFi\nIP address: %s\n", WiFi.localIP().toString().c_str());
    pinMode(RELAY, OUTPUT);

    if (MDNS.begin("AdvRelayWeb")) {
        Serial.println("MDNS responder started");
    }
    server.begin();

    dht.setup(DHTPIN, DHTesp::DHT22);

    // Bucket 값 읽기
    if(cfg.containsKey("meta") && cfg["meta"].containsKey("bucket")) {
        bucket = (const char*)cfg["meta"]["bucket"];
        Serial.printf("Bucket: %s\n", bucket.c_str());
    } else {
        Serial.println("Bucket not found, using default.");
        bucket = "defaultBucket";  // 기본 버킷 값 설정
    }

    // influxdbAddress 값 읽기
    if(cfg.containsKey("meta") && cfg["meta"].containsKey("influxdbAddress")) {
        String influxdbAddressFromConfig = (const char*)cfg["meta"]["influxdbAddress"];
        strncpy(influxdbAddress, influxdbAddressFromConfig.c_str(), sizeof(influxdbAddress)-1);
        influxdbAddress[sizeof(influxdbAddress)-1] = '\0';  // null terminate to avoid buffer overflow
        Serial.printf("InfluxDB Address: %s\n", influxdbAddress);
    } else {
        Serial.println("InfluxDB address not found, using default.");
        strcpy(influxdbAddress, "192.168.219.121");  // 기본 주소 설정
    }

    // InfluxDB Token 값 읽기
    if(cfg.containsKey("meta") && cfg["meta"].containsKey("influxDBToken")) {
        influxDBToken = (const char*)cfg["meta"]["influxDBToken"];
        Serial.printf("InfluxDB Token: %s\n", influxDBToken.c_str());
    } else {
        Serial.println("InfluxDB token not found, using default.");
        influxDBToken = "defaultToken";  // 기본 토큰 값 설정
    }

    // Report Interval 값 읽기
    if(cfg.containsKey("meta") && cfg["meta"].containsKey("reportInterval")) {
        report = atoi((const char*)cfg["meta"]["reportInterval"]);
        Serial.printf("Report Interval: %d ms\n", report);
    } else {
        Serial.println("Report Interval not found, using default.");
        report = 2000;  // 기본 보고 간격 설정 (2초)
    }

    // InfluxDB URL 구성
    String influxDBURLWithBucket = "http://" + String(influxdbAddress) + ":8086/api/v2/write?bucket=" + bucket + "&org=1f17224af73c9ba2&precision=s";
    Serial.printf("InfluxDB URL with bucket: %s\n", influxDBURLWithBucket.c_str());
}

void loop() {
    server.handleClient();
    
    // DHT 센서에서 온도 및 습도 값 읽기
    float temperature = dht.getTemperature();  // 온도 읽기
    float humidity = dht.getHumidity();        // 습도 읽기

    // 온도와 습도 값이 유효한지 확인
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
    } else {
        // 온도와 습도 값을 시리얼 모니터로 출력
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" °C | ");
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");
    }

    // InfluxDB로 데이터 전송
    sendToInfluxDB(temperature, humidity);
    
    delay(report); // 설정된 데이터 전송 주기 (report 변수 사용)
}
