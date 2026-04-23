#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// WIFI CONFIG
const char* ssid = "Andruuu";
const char* password = "salut123";

// MQTT CONFIG
const char* mqtt_server = "10.200.168.82";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/data";

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// SENSOR CONFIG 
#define DHTPIN 4
#define DHTTYPE DHT22
#define GAS_PIN 2

DHT dht(DHTPIN, DHTTYPE);

// OUTPUT CONFIG 
// Gas status LEDs
#define GAS_GREEN 5
#define GAS_YELLOW 6
#define GAS_RED 7

// Buzzer (critical alarm)
#define BUZZER_PIN 3


//  WIFI CONNECTION FUNCTION
void setup_wifi() {
  delay(10);
  Serial.println("\n[WIFI] Connecting to network...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[WIFI] Connection established");
  Serial.print("[WIFI] IP Address: ");
  Serial.println(WiFi.localIP());
}


//  MQTT RECONNECTION HANDLER
void reconnect() {
  while (!client.connected()) {
    Serial.println("[MQTT] Connecting...");

    if (client.connect("ESP32_SMART_FACTORY")) {
      Serial.println("[MQTT] Connected successfully");
    } else {
      Serial.print("[MQTT] Failed, rc=");
      Serial.println(client.state());
      Serial.println("[MQTT] Retrying in 2 seconds...");
      delay(2000);
    }
  }
}


//  SETUP FUNCTION
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n======================================");
  Serial.println(" SMART FACTORY MONITORING SYSTEM START");
  Serial.println("======================================");

  // Initialize sensor
  dht.begin();

  // Configure outputs
  pinMode(GAS_GREEN, OUTPUT);
  pinMode(GAS_YELLOW, OUTPUT);
  pinMode(GAS_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // Connect to network
  setup_wifi();

  // MQTT setup
  client.setServer(mqtt_server, mqtt_port);
}


//  MAIN LOOP
void loop() {

  // Maintain MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // SENSOR READ 
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int gasValue = analogRead(GAS_PIN);

  // Validate sensor data
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[ERROR] DHT sensor failure");
    return;
  }

  // AIR QUALITY ANALYSIS 
  const char* airStatus;
  const char* riskLevel;

  bool dangerState = false;

  if (gasValue < 500) {
    airStatus = "GOOD";
    riskLevel = "SAFE";

    digitalWrite(GAS_GREEN, HIGH);
    digitalWrite(GAS_YELLOW, LOW);
    digitalWrite(GAS_RED, LOW);
  }
  else if (gasValue < 800) {
    airStatus = "MODERATE";
    riskLevel = "NORMAL";

    digitalWrite(GAS_GREEN, LOW);
    digitalWrite(GAS_YELLOW, HIGH);
    digitalWrite(GAS_RED, LOW);
  }
  else {
    airStatus = "POOR";
    riskLevel = "DANGEROUS";

    digitalWrite(GAS_GREEN, LOW);
    digitalWrite(GAS_YELLOW, LOW);
    digitalWrite(GAS_RED, HIGH);

    dangerState = true;
  }

  //  BUZZER LOGIC 
  // Activated only in critical (RED) state
  if (dangerState) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // SERIAL OUTPUT 
  Serial.println("\n========== FACTORY STATUS ==========");
  Serial.printf("Temperature : %.2f °C\n", temperature);
  Serial.printf("Humidity    : %.2f %%\n", humidity);
  Serial.printf("Gas Level   : %d\n", gasValue);
  Serial.printf("Air Quality : %s\n", airStatus);
  Serial.printf("Risk Level  : %s\n", riskLevel);
  Serial.println("====================================");


  // MQTT MESSAGE 
  char payload[256];

  snprintf(payload, sizeof(payload),
    "{\"temperature\":%.2f,\"humidity\":%.2f,\"gas\":%d,\"air\":\"%s\",\"risk\":\"%s\"}",
    temperature,
    humidity,
    gasValue,
    airStatus,
    riskLevel
  );

  client.publish(mqtt_topic, payload);

  // LOOP DELAY 
  delay(2000);
}