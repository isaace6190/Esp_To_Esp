#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define ESPNOW_CHANNEL 1
#define BTN_PIN 21
#define BUZZER_PIN 2

#define NODE_ID 3 // this will change depending on which node I am uploading to
#define TTL_START 2

#define SELECT_WINDOW_MS 2000 //Amount of time for me to select which node to send to
#define DEBOUNCE_MS 250
#define RELAY_DELAY_MS 400
#define BUZZER_ON_MS 500

//information being sent 
//creating packet
typedef struct __attribute__((packed)) {
    uint8_t origin_id;
    uint8_t target_id;
    uint8_t ttl; 
    uint32_t seq;
} Msg;

uint8_t broadcastAddr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

uint32_t seqNum = 0;

//button clicking selection states
bool counting = false;
int pressCount = 0;
unsigned long firstTimePress = 0;
unsigned long lastPressMs = 0;

//to help with suppression and congestion
uint8_t lastOrigin = 0xFF;
uint32_t lastSeq = 0;

void buzzOutput(int times, int onMs = 150, int offMs = 120){
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(onMs);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < times - 1) delay(offMs);
    }
}

uint8_t mapPressCountToTarget(int count) {
  if (count < 1 || count > NODE_ID) {
    return 0;   // invalid target
  }
  return (uint8_t)count;
}

bool isDuplicate(const Msg &msg){
  return (msg.origin_id == lastOrigin && msg.seq == lastSeq);
}

void markSeen(const Msg &msg) {
    lastOrigin = msg.origin_id;
    lastSeq = msg.seq;
}

void sendNewMessage(uint8_t chosenTarget) {
  Msg msg;
  msg.origin_id = NODE_ID;
  msg.target_id = chosenTarget;
  msg.ttl = TTL_START;
  msg.seq = ++seqNum;

  esp_err_t result = esp_now_send(broadcastAddr, (uint8_t *)&msg, sizeof(msg));

  Serial.print("TX origin=");
  Serial.print(msg.origin_id);
  Serial.print(" target=");
  Serial.print(msg.target_id);
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
}

//receiving callback
void onReceive(const uint8_t *mac, const uint8_t *data, int len){
    if (len != sizeof(Msg)) return;
    Msg msg;
    memcpy(&msg, data, sizeof(msg));

    Serial.print("RX origin=");
    Serial.print(msg.origin_id);
    Serial.print(" target=");
    Serial.print(msg.target_id);
    Serial.print(" seq=");
    Serial.print(msg.seq);
    Serial.print(" ttl=");
    Serial.println(msg.ttl);

// Ignore our own message coming back around if it returns to us
  if (msg.origin_id == NODE_ID) {
    Serial.println("Ignoring my own originated message");
    return;
  }

// Duplicate suppression
  if (isDuplicate(msg)) {
    Serial.println("Duplicate ignored");
    return;
  }
  markSeen(msg);

    // If this node is the target, react and stop
  if (msg.target_id == NODE_ID) {
    Serial.println("I am the target -> activating output");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(BUZZER_ON_MS);
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  // If not target, optionally blink shortly to show relay activity
  buzzOutput(1, 120, 80);

  // Forward only if TTL remains
  if (msg.ttl == 0) {
    Serial.println("TTL expired, not forwarding");
    return;
  }

  msg.ttl--;

  delay(RELAY_DELAY_MS);

  esp_err_t result = esp_now_send(broadcastAddr, (uint8_t *)&msg, sizeof(msg));
  Serial.print("FWD origin=");
  Serial.print(msg.origin_id);
  Serial.print(" target=");
  Serial.print(msg.target_id);
  Serial.print(" seq=");
  Serial.print(msg.seq);
  Serial.print(" newTTL=");
  Serial.print(msg.ttl);
  Serial.print(" result=");
  if (result == ESP_OK) {
    Serial.println("OK");
  } else {
    Serial.println("FAIL");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.mode(WIFI_STA);

  // Force all nodes onto same channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) {
      delay(1000);
    }
  }

  // Add broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddr, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (!esp_now_is_peer_exist(broadcastAddr)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Add broadcast peer failed");
      while (true) {
        delay(1000);
      }
    }
  }

  esp_now_register_recv_cb(onReceive);

  Serial.println("Unified node ready");
  Serial.print("NODE_ID = ");
  Serial.println(NODE_ID);
  Serial.println("1 click -> node 1");
  Serial.println("2 clicks -> node 2");
  Serial.println("3 clicks -> node 3");
}


void loop() {
  static int lastBtn = HIGH;
  int nowBtn = digitalRead(BTN_PIN);
  unsigned long nowMs = millis();

  // Detect a new button press
  if (lastBtn == HIGH && nowBtn == LOW) {
    if (nowMs - lastPressMs > DEBOUNCE_MS) {
      lastPressMs = nowMs;

      if (!counting) {
        counting = true;
        pressCount = 1;
        firstTimePress = nowMs;
        Serial.println("Selection started");
      } else {
        pressCount++;
      }

      Serial.print("Press count = ");
      Serial.println(pressCount);

      // Optional short buzz for feedback
      buzzOutput(1, 60, 40);
    }
  }

  lastBtn = nowBtn;

  // Once selection window ends, choose target and send
  if (counting && (nowMs - firstTimePress >= SELECT_WINDOW_MS)) {
    uint8_t chosenTarget = mapPressCountToTarget(pressCount);

    Serial.print("Window ended, chosen target = ");
    Serial.println(chosenTarget);

    if (chosenTarget == 0) {
      Serial.println("Invalid target, not sending");
    } else if (chosenTarget == NODE_ID) {
      Serial.println("Target is this same node, not sending");
    } else {
      sendNewMessage(chosenTarget);
    }

    counting = false;
    pressCount = 0;
  }
}