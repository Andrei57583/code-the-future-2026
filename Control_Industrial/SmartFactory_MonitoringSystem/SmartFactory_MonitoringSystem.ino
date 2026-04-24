#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
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

// ---------------- PINS (ESP32-C6) ----------------
#define MQ135_PIN 2
#define ACS712_PIN 3
#define DHTPIN 4

#define RELAY_PIN 7
#define SERVO_PIN 6

#define LED_G 10
#define LED_Y 11
#define LED_R 8

#define BUZZER_PIN 9

// ---------------- SENSORS ----------------
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp;
Servo windowServo;

// ---------------- GAS ----------------
float R0 = 10;
float filteredGas = 0;
float alpha = 0.08;

// ---------------- ACS712 ----------------
float acsOffset = 2.5;

// ---------------- TIMERS ----------------
unsigned long lastSensor = 0;
unsigned long lastMQTT = 0;

// ---------------- WIFI ----------------
void setup_wifi() {
  WiFi.begin(ssid, password);
  unsigned long t = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
    delay(300);
  }
}

// ---------------- MQTT ----------------
void reconnect() {
  while (!client.connected()) {
    client.connect("ESP32_C6");
    delay(500);
  }
}

// ---------------- BMP280 FIX ----------------
void initBMP() {
  if (!bmp.begin(0x76)) {
    if (!bmp.begin(0x77)) {
      Serial.println("BMP280 FAILED");
    }
  }
}

// ---------------- ACS712 CALIBRATION ----------------
void calibrateACS() {
  float sum = 0;

  for (int i = 0; i < 200; i++) {
    sum += analogRead(ACS712_PIN);
    delay(2);
  }

  acsOffset = (sum / 200.0) * (3.3 / 4095.0);
}

// ---------------- MQ CALIBRATION ----------------
void calibrateGas() {
  float sum = 0;

  for (int i = 0; i < 100; i++) {
    sum += analogRead(MQ135_PIN);
    delay(20);
  }

  float baseline = sum / 100.0;
  R0 = baseline / 3.6;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  dht.begin();
  initBMP();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);

  windowServo.attach(SERVO_PIN);
  windowServo.write(0);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  delay(60000); // MQ warmup

  calibrateGas();
  calibrateACS();
}

// ---------------- LOOP ----------------
void loop() {

  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();

  if (now - lastSensor > 2000) {
    lastSensor = now;

    // ---------- DHT ----------
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    // ---------- BMP280 ----------
    float pressure = bmp.readPressure();
    if (isnan(pressure)) pressure = 0;
    else pressure /= 100.0;

    // ---------- GAS ----------
    int rawGas = analogRead(MQ135_PIN);
    filteredGas = alpha * rawGas + (1 - alpha) * filteredGas;

    float ratio = filteredGas / R0;
    float gas = 116.6 * pow(ratio, -2.7);

    // ---------- ACS712 ----------
    float current = 0;

    for (int i = 0; i < 30; i++) {
      int raw = analogRead(ACS712_PIN);
      float voltage = raw * (3.3 / 4095.0);
      current += (voltage - acsOffset) / 0.185;
    }

    current /= 30;

    // eliminare zgomot
    if (current < 0.05 && current > -0.05) current = 0;

    // ---------- RISK ----------
    const char* risk;

    if (gas < 80) risk = "SAFE";
    else if (gas < 150) risk = "LOW";
    else if (gas < 300) risk = "MEDIUM";
    else if (gas < 500) risk = "HIGH";
    else risk = "CRITICAL";

    // ---------- ACTUATORS ----------
    if (strcmp(risk, "CRITICAL") == 0 || strcmp(risk, "HIGH") == 0) {
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
      windowServo.write(90);
    } else {
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      windowServo.write(0);
    }

    // ---------- SERIAL ----------
    Serial.printf("Temp: %.2f\n", temp);
    Serial.printf("Hum: %.2f\n", hum);
    Serial.printf("Pressure: %.2f hPa\n", pressure);
    Serial.printf("Gas: %.2f\n", gas);
    Serial.printf("Current: %.2f A\n", current);
    Serial.printf("Risk: %s\n", risk);

    // ---------- MQTT ----------
    if (now - lastMQTT > 3000) {
      lastMQTT = now;

      char payload[256];
      snprintf(payload, sizeof(payload),
        "{\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"g\":%.2f,\"c\":%.2f,\"r\":\"%s\"}",
        temp, hum, pressure, gas, current, risk
      );

      client.publish(mqtt_topic, payload);
    }
  }
}