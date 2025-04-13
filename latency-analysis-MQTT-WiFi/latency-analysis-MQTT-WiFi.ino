#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const char* mqtt_server = "test.mosquitto.org";

const char* topic_send = "codzetest/feeds/send";
const char* topic_echo = "codzetest/feeds/echo";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long last_sent_time = 0;
unsigned long rtts[100];
int rtt_count = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = String((char*)payload);

  int commaIndex = message.indexOf(',');
  if (commaIndex == -1) return;

  unsigned long original_timestamp = message.substring(0, commaIndex).toInt();
  unsigned long pc_echo_time = message.substring(commaIndex + 1).toInt();
  unsigned long now = millis();

  unsigned long rtt = now - original_timestamp;
  unsigned long adjusted_rtt = rtt - pc_echo_time;

  Serial.print("RTT: "); Serial.print(rtt);
  Serial.print(" ms | PC echo: "); Serial.print(pc_echo_time);
  Serial.print(" ms | Adjusted RTT: "); Serial.print(adjusted_rtt);
  Serial.println(" ms");

  if (rtt_count < 100) {
    rtts[rtt_count++] = adjusted_rtt;
  }

  if (rtt_count == 100) {
    unsigned long sum = 0;
    for (int i = 0; i < 100; i++) {
      sum += rtts[i];
    }
    float avg = (float)sum / 100.0;

    Serial.println();
    Serial.println("====== FINAL RESULT ======");
    Serial.print("Average Adjusted RTT over 100 samples: ");
    Serial.print(avg);
    Serial.println(" ms");
    Serial.println("==========================");

    while (true); // Stop after collecting 100 samples
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(topic_echo);
    } else {
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();
}

void loop() {
  client.loop();
  if (!client.connected()) reconnect();

  static unsigned long last_send = 0;
  if (rtt_count < 100 && millis() - last_send > 1000) {
    last_send = millis();
    last_sent_time = millis();
    char msg[32];
    sprintf(msg, "%lu", last_sent_time);
    client.publish(topic_send, msg);
    Serial.print("Sent: "); Serial.println(last_sent_time);
  }
}
