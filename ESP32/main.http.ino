#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "driver/temperature_sensor.h"
#include "AsyncUDP.h"


// ================= WIFI =================

const char* ssid = "********";
const char* password = "********";
int connectionTries = 0;


// TEMPERATURA
temperature_sensor_handle_t temp_handle = NULL;

// ASYNC UDP
AsyncUDP udp;

// IP Multicast (deve essere lo stesso sul server)
IPAddress multicastIP(239, 1, 2, 3);
unsigned int multicastPort = 3344;

IPAddress serverIP;
bool serverFound = false;
bool udpOpen = false;


// IP DEL PC CHE ESEGUE FLASK
unsigned int servicePort = 4512;
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

  Serial.println("Tentativo di connessione WiFi...");

  // WIFI
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(500);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 60 * 1000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connesso");
    return true;
  }

  Serial.println();
  Serial.println("Connessione WiFi fallita");
  return false;
}

void sendPhoto() {
  Serial.println("Scatto foto...");
  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Errore acquisizione");
    return;
  }

  if (!connectWiFi()) {
    esp_camera_fb_return(fb);
    return;
  }

  float temp;
  temperature_sensor_get_celsius(temp_handle, &temp);

  HTTPClient http;
  String serverUrl = "http://" + serverIP.toString() + ":" + String(servicePort) + "/upload";
  // Serial.println("Sending picture to URL: " + serverUrl);
  http.begin(serverUrl);

  // inviamo direttamente JPEG
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Device-ID", deviceId);
  http.addHeader("X-Device-TEMP", String(temp));
  int response = http.POST(fb->buf, fb->len);

  // Serial.print("Risposta server: ");
  // Serial.println(response);

  if (response < 0) {
    // Serial.println("Errore invio, provo a riconnettermi al server in un prossimo ciclo.");
    connectionTries++;
  } else {
    connectionTries = 0;
  }

  http.end();
  esp_camera_fb_return(fb);
}

// ================= SETUP =================
void setupMulticast() {
  if (udp.listenMulticast(multicastIP, multicastPort)) {
    Serial.println("In ascolto sul multicast per scoprire l'IP del server...");
    udp.onPacket([](AsyncUDPPacket packet) {
      String message = (char*)packet.data();
      if (message.startsWith("SERVER_ALIVE") && !serverFound) {
        serverIP = packet.remoteIP();
        serverFound = true;
        
        Serial.print("Server scoperto! L'IP del server è: ");
        Serial.println(serverIP.toString());
      }
    });
  }
}


void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("XIAO ESP32S3 Camera Client");

  temperature_sensor_config_t temp_sensor = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
  temperature_sensor_install(&temp_sensor, &temp_handle);
  temperature_sensor_enable(temp_handle);

  // inizializza camera
  if (!initCamera()) {
    Serial.println("Camera KO");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("Camera OK");

  connectWiFi();

  deviceId = generateDeviceId();
  Serial.print("Device ID: ");
  Serial.println(deviceId);

  Serial.println();
  Serial.println("WiFi connesso");

  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  setupMulticast();
}

// ================= LOOP =================
void loop() {
  if (!serverFound) {
    delay(5000);
  } else {
    if (connectionTries > 10 && !udpOpen) {
      setupMulticast();
      connectionTries = 0;
      udpOpen = false;
      Serial.println("Connection failed. Enabling multicasting for new server discovery.");
    }
    if (udpOpen) {
      udp.close();
      udpOpen = false;
      Serial.println("Porta UDP chiusa correttamente. Avvio trasmissione foto...");
    } else {
      sendPhoto();
      delay(250);
    }
  }
}