#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define ESPNOW_CHANNEL 1
#define LED_PIN 2


typedef struct __attribute__((packed)) {
  uint8_t origin_id;
  uint8_t ttl;
  uint32_t seq;

} Msg;

uint8_t broadcastAddr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// simple duplicate suppression
uint8_t lastOrigin = 0xFF;
uint32_t lastSeq = 0;

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(Msg)) return;

  Msg msg;
  memcpy(&msg, data, sizeof(msg));

  Serial.print("RELAY RX origin=");
  Serial.print(msg.origin_id);
  Serial.print(" seq=");
  Serial.print(msg.seq);
  Serial.print(" ttl=");
  Serial.println(msg.ttl);

  digitalWrite(LED_PIN, HIGH);
  delay(500);   // delay so i can see relay activate for demo
  digitalWrite(LED_PIN, LOW);

  // ignore duplicates
  if (msg.origin_id == lastOrigin && msg.seq == lastSeq) return;
  lastOrigin = msg.origin_id;
  lastSeq = msg.seq;

  // forward if ttl remaining
  if (msg.ttl == 0) return;
  msg.ttl--;
  delay(400);
  esp_err_t result = esp_now_send(broadcastAddr, (uint8_t*)&msg, sizeof(msg));
  Serial.print("RELAY FWD result=");
  if (result == ESP_OK) {
    Serial.println("OK");
  } else {
    Serial.println("FAIL");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);

  // force channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }

  // add broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddr, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (!esp_now_is_peer_exist(broadcastAddr)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Relay add broadcast peer failed");
      while (true) delay(1000);
    }
  }

  esp_now_register_recv_cb(onReceive);
  Serial.println("Relay ready");
}

void loop() { delay(1000); }
