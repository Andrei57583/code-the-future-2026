#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <ESP32Servo.h>

// ---------------- WIFI ----------------
const char* ssid = "Andruuu";
const char* password = "salut123";

// ---------------- MQTT ----------------
const char* mqtt_server = "10.200.168.82";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/data";

WiFiClient espClient;
PubSubClient client(espClient);

// ---------------- PINS ----------------
#define MQ135_PIN 2
#define ACS712_PIN 3
#define DHTPIN 1

#define RELAY_PIN 7
#define SERVO_PIN 6

#define LED_G 8
#define LED_Y 10
#define LED_R 11

#define BUZZER_PIN 9

// ---------------- SENSORS ----------------
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp;
Servo windowServo;

// ---------------- VARIABLES ----------------
float R0 = 10;
float filteredGas = 0;
float alpha = 0.08;
float acsOffset = 0;

// ---------------- WIFI + MQTT STARTUP ----------------
void connectSystem() {

  Serial.println("\n[BOOT] Smart Factory System Starting...");

  // ---------- WIFI ----------
  Serial.println("[WIFI] Connecting...");
  WiFi.begin(ssid, password);

  int wifiTry = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTry < 20) {
    delay(500);
    Serial.print(".");
    wifiTry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WIFI] FAILED!");
  }

  // ---------- MQTT ----------
  Serial.println("[MQTT] Connecting...");

  client.setServer(mqtt_server, mqtt_port);

  int mqttTry = 0;
  while (!client.connected() && mqttTry < 20) {
    client.connect("ESP32_C6");
    delay(500);
    Serial.print(".");
    mqttTry++;
  }

  if (client.connected()) {
    Serial.println("\n[MQTT] Connected!");
  } else {
    Serial.println("\n[MQTT] FAILED (retry in loop)");
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);

  dht.begin();
  bmp.begin(0x76);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);

  windowServo.attach(SERVO_PIN);
  windowServo.write(0);

  connectSystem();

  delay(30000);

  // ACS calibration
  float sum = 0;
  for (int i = 0; i < 300; i++) {
    sum += analogRead(ACS712_PIN);
    delay(2);
  }
  acsOffset = (sum / 300.0) * (3.3 / 4095.0);

  // GAS calibration
  sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += analogRead(MQ135_PIN);
    delay(10);
  }
  R0 = sum / 100.0;
}

// ---------------- LOOP ----------------
void loop() {

  if (!client.connected()) client.connect("ESP32_C6");
  client.loop();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) return;

  float pressure = bmp.readPressure() / 100.0;
  if (pressure < 300 || pressure > 1200) pressure = 0;

  int rawGas = analogRead(MQ135_PIN);
  filteredGas = alpha * rawGas + (1 - alpha) * filteredGas;
  float gas = (filteredGas / R0) * 100.0;

  float voltage = analogRead(ACS712_PIN) * (3.3 / 4095.0);
  float current = (voltage - acsOffset) / 0.185;
  if (abs(current) < 0.08) current = 0;

  // ---------------- RISK ----------------
  const char* risk;

  if (gas < 80) risk = "SAFE";
  else if (gas < 140) risk = "MODERATE";
  else risk = "CRITICAL";

  // ---------------- RESET LEDs ----------------
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_Y, LOW);
  digitalWrite(LED_R, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  bool fan = false;
  bool window = false;

  if (strcmp(risk, "SAFE") == 0) {
    digitalWrite(LED_G, HIGH);
  }
  else if (strcmp(risk, "MODERATE") == 0) {
    digitalWrite(LED_Y, HIGH);
    fan = true;
    window = true;
  }
  else {
    digitalWrite(LED_R, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    fan = true;
    window = true;
  }

  digitalWrite(RELAY_PIN, fan ? HIGH : LOW);
  windowServo.write(window ? 90 : 0);

  // ---------------- SERIAL ----------------
  Serial.println("\n========== SYSTEM STATUS ==========");
  Serial.printf("Temp: %.2f °C\n", temp);
  Serial.printf("Hum: %.2f %%\n", hum);
  Serial.printf("Pressure: %.2f hPa\n", pressure);
  Serial.printf("Gas: %.2f\n", gas);
  Serial.printf("Current: %.2f A\n", current);
  Serial.printf("Status: %s\n", risk);
  Serial.println("==================================");

  // ---------------- MQTT ----------------
  char payload[256];
  snprintf(payload, sizeof(payload),
    "{\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"g\":%.2f,\"c\":%.2f,\"r\":\"%s\"}",
    temp, hum, pressure, gas, current, risk);

  client.publish(mqtt_topic, payload);

  delay(2000);
}