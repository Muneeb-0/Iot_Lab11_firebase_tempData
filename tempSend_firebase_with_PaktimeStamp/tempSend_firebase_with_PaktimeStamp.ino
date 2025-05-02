#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <time.h>  // For NTP time

// ======= WiFi Credentials ======= //
const char* ssid = "TampleDiago";
const char* password = "12345699";

// ======= Firebase Configuration ======= //
const String FIREBASE_HOST = "iot-lab11-9a1b7-default-rtdb.firebaseio.com";
const String FIREBASE_AUTH = "YvgN4lztA6kxTDTO67vorsENzX1cmfJexIDgGeY4";
const String FIREBASE_PATH = "/sensor_data.json";

// ======= DHT Sensor Configuration ======= //
#define DHTPIN 4       // GPIO4 (change if needed)
#define DHTTYPE DHT11  // DHT11 or DHT22

DHT dht(DHTPIN, DHTTYPE);

// ======= Time (NTP) Configuration ======= //
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 5 * 3600;   // Pakistan Standard Time (UTC+5)
const int   daylightOffset_sec = 0;     // No DST

// ======= Timing ======= //
const unsigned long SEND_INTERVAL = 10000;  // 10 seconds
const unsigned long SENSOR_DELAY = 2000;    // 2 seconds between reads
unsigned long lastSendTime = 0;
unsigned long lastReadTime = 0;

// ======= Setup ======= //
void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-S3 DHT11 Firebase Monitor");

  initDHT();
  connectWiFi();

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("Waiting for NTP time sync");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nTime synchronized");
}

// ======= Main Loop ======= //
void loop() {
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Read sensor (with proper timing)
  if (millis() - lastReadTime >= SENSOR_DELAY) {
    float temp, hum;
    if (readDHT(&temp, &hum)) {
      // Send to Firebase (with proper timing)
      if (millis() - lastSendTime >= SEND_INTERVAL) {
        sendToFirebase(temp, hum);
        lastSendTime = millis();
      }
    }
    lastReadTime = millis();
  }
}

// ======= DHT Sensor Functions ======= //
void initDHT() {
  dht.begin();
  Serial.println("DHT sensor initialized");
  delay(500);  // Short stabilization delay
}

bool readDHT(float* temp, float* humidity) {
  *temp = dht.readTemperature();
  *humidity = dht.readHumidity();

  if (isnan(*temp) || isnan(*humidity)) {
    Serial.println("DHT read failed! Retrying...");
    
    // Attempt sensor recovery
    digitalWrite(DHTPIN, LOW);  // Reset pin state
    pinMode(DHTPIN, INPUT);
    delay(100);
    initDHT();  // Reinitialize
    
    return false;
  }

  Serial.printf("DHT Read: %.1f°C, %.1f%%\n", *temp, *humidity);
  return true;
}

// ======= WiFi Functions ======= //
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.disconnect(true);  // Clear previous config
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed!");
  }
}

// ======= Firebase Functions ======= //
void sendToFirebase(float temp, float humidity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send - WiFi disconnected");
    return;
  }

  // Get current NTP time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  char timestampStr[25];
  strftime(timestampStr, sizeof(timestampStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

  // Prepare JSON payload
  String jsonPayload = "{\"temperature\":" + String(temp) + 
                       ",\"humidity\":" + String(humidity) + 
                       ",\"timestamp\":\"" + String(timestampStr) + "\"}";

  String url = "https://" + FIREBASE_HOST + FIREBASE_PATH + "?auth=" + FIREBASE_AUTH;

  Serial.println("Sending to Firebase...");
  Serial.println(jsonPayload);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(jsonPayload);

  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_ACCEPTED) {
    Serial.println("Firebase update successful");
  } else {
    Serial.printf("Firebase error: %d\n", httpCode);
    if (httpCode == -1) {
      Serial.println("Check your Firebase URL and authentication");
    }
  }

  http.end();
}
