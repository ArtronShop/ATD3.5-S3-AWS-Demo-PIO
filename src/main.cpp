#include <Arduino.h>
#include <lvgl.h>
#include <ATD3.5-S3.h>
#include "gui/ui.h"
#include "Configs.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArtronShop_SHT3x.h>

#define LED_Y_PIN (5)

ArtronShop_SHT3x sht3x(0x44, &Wire); // ADDR: 0 => 0x44, ADDR: 1 => 0x45

WiFiClientSecure tcpClient;
PubSubClient client(tcpClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (payload[0] == '1') {
    digitalWrite(LED_Y_PIN, LOW); // on LED Y
    lv_obj_add_state(ui_lamp_status_panel, LV_STATE_CHECKED);
    lv_label_set_text(ui_lamp_status_label, "เปิด");
  } else if (payload[0] == '0') {
    digitalWrite(LED_Y_PIN, HIGH); // off LED Y
    lv_obj_clear_state(ui_lamp_status_panel, LV_STATE_CHECKED);
    lv_label_set_text(ui_lamp_status_label, "ปิด");
  }
}

void sensor_update_cb(lv_timer_t *) {
  sht3x.measure();
  int temp = sht3x.temperature();
  int humi = sht3x.humidity();

  // Update UI
  lv_arc_set_value(ui_temp_arc, temp);
  lv_label_set_text_fmt(ui_temp_label, "%d", temp);
  lv_arc_set_value(ui_humi_arc, humi);
  lv_label_set_text_fmt(ui_humi_label, "%d", humi);

  // Update to AWS
  String payload = "{";
  payload += " \"temp\": " + String(temp) + ",";
  payload += " \"humi\": " + String(humi) + " ";
  payload += "}";
  client.publish("ATD3.5-S3/sensor", payload.c_str());
}

void setup() {
  Serial.begin(115200);
  
  // Setup peripherals
  Display.begin(0); // rotation number 0
  Touch.begin();
  Sound.begin();
  // Card.begin(); // uncomment if you want to Read/Write/Play/Load file in MicroSD Card
  pinMode(LED_Y_PIN, OUTPUT);
  digitalWrite(LED_Y_PIN, HIGH); // off LED Y

  Wire.begin();
  while (!sht3x.begin()) {
    Serial.println("SHT3x not found !");
    delay(1000);
  }
  
  // Map peripheral to LVGL
  Display.useLVGL(); // Map display to LVGL
  Touch.useLVGL(); // Map touch screen to LVGL
  Sound.useLVGL(); // Map speaker to LVGL
  // Card.useLVGL(); // Map MicroSD Card to LVGL File System

  Display.enableAutoSleep(120); // Auto off display if not touch in 2 min
  
  // Add load your UI function
  ui_init();

  // Add event handle
  
  lv_timer_create(sensor_update_cb, 5000, NULL);

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait WiFi Connected
  Serial.println("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  tcpClient.setCACert(AWS_ROOT_CA1);
  tcpClient.setCertificate(AWS_DEVICE_CERT);
  tcpClient.setPrivateKey(AWS_DEVICE_PRIVATE_CERT);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setCallback(callback);

}

void loop() {
  Display.loop(); // Keep GUI work

  if (!client.loop()) {
    Serial.println("Connecting to AWS IOT");
    if (client.connect(AWS_CLIENT_ID)) {
      Serial.println("Connected");
      client.subscribe("ATD3.5-S3/led");
    } else {
      Serial.println("Connect fail");
    }
  }
}
