#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define LED_PIN 2
#define ESPNOW_CHANNEL 1

typedef struct __attribute__((packed)) {
  uint8_t origin_id;   // which node created the alert
  uint8_t ttl;         // hop limit
  uint32_t seq;        // sequence number
} Msg;

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(Msg)) return;

  Msg msg;
  memcpy(&msg, data, sizeof(msg));

  Serial.print("RX origin=");
  Serial.print(msg.origin_id);
  Serial.print(" seq=");
  Serial.print(msg.seq);
  Serial.print(" ttl=");
  Serial.println(msg.ttl);

  if (msg.ttl != 1) return;

  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }

  esp_now_register_recv_cb(onReceive);
  Serial.println("Receiver ready");
}

void loop() {
  delay(1000);
}
