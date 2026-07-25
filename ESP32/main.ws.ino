#include <WiFi.h>
// #include <HTTPClient.h>
#include "esp_camera.h"
// #include "driver/temperature_sensor.h"
#include <WebSocketsClient.h>
#include <base64.h>
#include <ArduinoJson.h>


// ================= WIFI =================

const char* ssid = "********";
const char* password = "********";

WebSocketsClient webSocket;
// #define CHUNK_SIZE 4096
// unsigned long lastReceivedResponse = 0;

// #define IP "192.168.0.31"
// #define GATEWAY "255.255.255.0"
// #define SUBNET "192.168.0.1"
// #define DNS1 "1.1.1.1"
// #define DNS2 "1.0.0.1"

// TEMPERATURA
// temperature_sensor_handle_t temp_handle = NULL;

// IP DEL PC CHE ESEGUE FLASK
#define SERVER_URL "192.168.0.213"
#define SERVER_PORT 4512
#define SERVER_PATH "/upload"
String deviceId = "";

// ================= CAMERA XIAO ESP32S3 SENSE =================

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1

#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39

#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15

#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13

// ================= CAMERA INIT =================
bool initCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIXFORMAT_JPEG;

  // usa PSRAM
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  return true;
}

// ================= INVIO FOTO =================

String generateDeviceId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "esp32s3-" + mac;
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("Tentativo di riconnessione WiFi...");

  // WIFI
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  // WiFi.config(IP, GATEWAY, SUBNET, DNS1, DNS2);
  delay(500);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  const unsigned long wifiTimeoutMs = 60000;
  const unsigned long maxWifiWaitMs = 10 * 60 * 1000;
  unsigned long startWait = millis();

  Serial.println("Aspetto la connessione WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    // Serial.println("IP da DHCP: " + WiFi.localIP().toString());
    Serial.print(".");

    if (millis() - startWait >= maxWifiWaitMs) {
      Serial.println();
      Serial.println("WiFi non connesso dopo 10 minuti");
      WiFi.disconnect(true, true);
      // ESP.restart();
      return false;
    }
  }

  Serial.println();
  Serial.println("WiFi connesso");
  return true;
}

String encodeImage(uint8_t* imageBuffer, size_t imageLength) {
  return base64::encode(imageBuffer, imageLength);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected from server!");
      break;

    case WStype_CONNECTED:
      Serial.println("[WS] Connected to server successfully!");
      // lastReceivedResponse = millis();
      break;

    case WStype_TEXT:
      Serial.printf("[WS] Received text from server: %s\n", payload);
      // lastReceivedResponse = millis();
      break;

    case WStype_BIN:
      Serial.println("[WS] Received binary data");
      break;
  }
}

void sendPhoto() {
  Serial.println("Scatto foto...");
  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Errore acquisizione");
    return;
  }

  if (!connectWiFi()) {
    Serial.println("Wifi not connected, returning camera result");
    esp_camera_fb_return(fb);
    return;
  }

  String base64image = encodeImage(fb->buf, fb->len);
  JsonDocument doc;

  // float temp;
  // temperature_sensor_get_celsius(temp_handle, &temp);

  doc["device_id"] = deviceId;
  // doc["temperature"] = temp;
  // doc["uptime"] = millis();
  doc["image"] = base64image;

  String payload;
  serializeJson(doc, payload);
  webSocket.sendTXT(payload);

  esp_camera_fb_return(fb);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(2000);

  // temperature_sensor_config_t temp_sensor = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
  // temperature_sensor_install(&temp_sensor, &temp_handle);
  // temperature_sensor_enable(temp_handle);

  Serial.println();
  Serial.println("XIAO ESP32S3 Camera Client");

  // inizializza camera
  if (!initCamera()) {
    Serial.println("Camera KO");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("Camera OK");

  if (!connectWiFi()) {
    Serial.println();
    Serial.println("WiFi fallito durante setup. Riavvio sistema.");
    ESP.restart();
  }

  deviceId = generateDeviceId();
  Serial.print("Device ID: ");
  Serial.println(deviceId);

  Serial.println();
  Serial.println("WiFi connesso");

  webSocket.begin(SERVER_URL, SERVER_PORT, SERVER_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(1000);

  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

// ================= LOOP =================
unsigned long lastSendTime = 0;
void loop() {
  webSocket.loop();
  if (millis() - lastSendTime > 1000) {
    lastSendTime = millis();
    sendPhoto();
  }
}