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

String systemState = "SAFE";
String cause = "NONE";

// ---------------- CONNECT ----------------
void connectSystem() {

  Serial.println("\n[BOOT] Industrial Safety System");

  WiFi.begin(ssid, password);
  Serial.print("[WIFI] Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\n[WIFI] Connected");

  client.setServer(mqtt_server, mqtt_port);

  Serial.print("[MQTT] Connecting");
  while (!client.connected()) {
    client.connect("ESP32_C6");
    delay(300);
    Serial.print(".");
  }

  Serial.println("\n[MQTT] Connected");
}

// ---------------- APPLY STATE ----------------
void applyState(String state) {

  // RESET OUTPUTS
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_Y, LOW);
  digitalWrite(LED_R, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  if (state == "SAFE") {
    digitalWrite(LED_G, HIGH);
    digitalWrite(RELAY_PIN, LOW);   // FAN OFF
    windowServo.write(0);
  }

  else if (state == "MODERATE") {
    digitalWrite(LED_Y, HIGH);
    digitalWrite(RELAY_PIN, HIGH);

    // smooth opening
    for (int pos = 0; pos <= 60; pos += 2) {
      windowServo.write(pos);
      delay(10);
    }
  }

  else if (state == "CRITICAL") {
    digitalWrite(LED_R, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(RELAY_PIN, HIGH);

    // fast opening
    for (int pos = 0; pos <= 90; pos += 5) {
      windowServo.write(pos);
      delay(5);
    }
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

  Serial.println("[SYSTEM] Calibration...");
  delay(20000);

  // ACS calibration
  float sum = 0;
  for (int i = 0; i < 200; i++) {
    sum += analogRead(ACS712_PIN);
    delay(2);
  }
  acsOffset = (sum / 200.0) * (3.3 / 4095.0);

  // GAS calibration
  sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += analogRead(MQ135_PIN);
    delay(10);
  }
  R0 = sum / 100.0;

  Serial.println("[SYSTEM] Ready");
}

// ---------------- LOOP ----------------
void loop() {

  if (!client.connected()) client.connect("ESP32_C6");
  client.loop();

  // ---------- READ ----------
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float pressure = bmp.readPressure() / 100.0;

  int rawGas = analogRead(MQ135_PIN);
  filteredGas = alpha * rawGas + (1 - alpha) * filteredGas;
  float gas = (filteredGas / R0) * 100.0;

  float voltage = analogRead(ACS712_PIN) * (3.3 / 4095.0);
  float current = (voltage - acsOffset) / 0.185;
  if (abs(current) < 0.1) current = 0;

  if (pressure < 300 || pressure > 1200) pressure = 0;

  // ---------- CONDITIONS ----------
  bool gasMod = gas > 80;
  bool gasCrit = gas > 140;

  bool tempMod = temp > 30;
  bool tempCrit = temp > 35;

  bool humMod = hum > 70;
  bool humCrit = hum > 85;

  bool currMod = current > 5;
  bool currCrit = current > 10;

  bool presMod = pressure > 1020 || pressure < 940;
  bool presCrit = pressure > 1050 || pressure < 900;

  // ---------- BUILD CAUSE (NO DUPLICATES) ----------
  cause = "";

  if (gasCrit) cause += "GAS_CRIT ";
  else if (gasMod) cause += "GAS_MOD ";

  if (tempCrit) cause += "TEMP_CRIT ";
  else if (tempMod) cause += "TEMP_MOD ";

  if (humCrit) cause += "HUM_CRIT ";
  else if (humMod) cause += "HUM_MOD ";

  if (currCrit) cause += "CURR_CRIT ";
  else if (currMod) cause += "CURR_MOD ";

  if (presCrit) cause += "PRESS_CRIT ";
  else if (presMod) cause += "PRESS_MOD ";

  if (cause == "") cause = "NONE";

  // ---------- STATE ----------
  bool critical = gasCrit || tempCrit || humCrit || currCrit || presCrit;
  bool moderate = gasMod || tempMod || humMod || currMod || presMod;

  String newState;

  if (critical) newState = "CRITICAL";
  else if (moderate) newState = "MODERATE";
  else newState = "SAFE";

  // ---------- APPLY ----------
  if (newState != systemState) {
    systemState = newState;
    applyState(systemState);
  }

  // ---------- SERIAL ----------
  Serial.println("\n========== INDUSTRIAL MONITOR ==========");
  Serial.printf("Temperature : %.2f °C\n", temp);
  Serial.printf("Humidity    : %.2f %%\n", hum);
  Serial.printf("Pressure    : %.2f hPa\n", pressure);
  Serial.printf("Gas Index   : %.2f\n", gas);
  Serial.printf("Current     : %.2f A\n", current);
  Serial.printf("STATE       : %s\n", systemState.c_str());
  Serial.printf("CAUSE       : %s\n", cause.c_str());
  Serial.println("=======================================");

  // ---------- MQTT ----------
  char payload[256];
  snprintf(payload, sizeof(payload),
    "{\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"g\":%.2f,\"c\":%.2f,\"r\":\"%s\",\"cause\":\"%s\"}",
    temp, hum, pressure, gas, current, systemState.c_str(), cause.c_str()
  );

  client.publish(mqtt_topic, payload);

  delay(2000);
}