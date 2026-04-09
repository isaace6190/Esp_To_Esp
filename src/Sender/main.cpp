#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define ESPNOW_CHANNEL 1
#define BTN_PIN 21
#define ORIGIN_ID 1
#define NODE_ID 1 //THIS is the ID for the specific node
#define TTL_START 2

typedef struct __attribute__((packed)) {
  uint8_t origin_id;
  uint8_t ttl;
  uint8_t target_id;
  uint32_t seq;
} Msg;

Msg msg;
uint32_t seqNum = 0;

uint8_t broadcastAddr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(BTN_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

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
      Serial.println("Sender add broadcast peer failed");
      while (true) delay(1000);
    }
  }

  Serial.println("Sender ready (button)");
}

void loop() {
  static int last = HIGH;
  int now = digitalRead(BTN_PIN);

  if (last == HIGH && now == LOW) {
    msg.origin_id = ORIGIN_ID;
    msg.ttl = TTL_START;
    msg.seq = ++seqNum;

    esp_err_t result = esp_now_send(broadcastAddr, (uint8_t*)&msg, sizeof(msg));

    Serial.print("TX origin=");
    Serial.print(msg.origin_id);
    Serial.print(" seq=");
    Serial.print(msg.seq);
    Serial.print(" ttl=");
    Serial.print(msg.ttl);
    Serial.print(" result=");
    
    if (result == ESP_OK) {
    Serial.println("OK");
    } else {
    Serial.println("FAIL");
}

    delay(250);
  }

  last = now;
}