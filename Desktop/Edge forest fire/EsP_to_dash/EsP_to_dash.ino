#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ========== WiFi ==========
const char* ssid     = "Mison";
const char* password = "00000000";

// CHANGE to your PC IP
const char* serverURL = "http://192.168.43.176:5000//data";

// ===================================================
#define I2C_ADDR 0x08

// Sensors
#define FLAME_PIN 19
#define DHT_PIN   23
#define DHT_TYPE  DHT11
#define MQ4_PIN   34

DHT dht(DHT_PIN, DHT_TYPE);

// Actuators
#define LED_PIN    2
#define BUZZER_PIN 15

// Thresholds
#define MQ4_THRESHOLD 1000
#define TEMP_THRESHOLD 40.0
#define FIRE_PROB_THRESHOLD 0.5

String receivedData = "";

//  ML VALUES GLOBAL (IMPORTANT)
float fireProb = 0.0;
float noFireProb = 0.0;

// ===================================================
// I2C receive (DO NOT TOUCH LOGIC)
void receiveEvent(int bytes) {
    while (Wire.available()) {
        char c = Wire.read();
        receivedData += c;
    }
}

// ===================================================
void setup() {
    Serial.begin(115200);

    pinMode(FLAME_PIN, INPUT);
    dht.begin();

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    Wire.begin(I2C_ADDR);
    Wire.onReceive(receiveEvent);

    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    Serial.println("ESP32 SLAVE + Sensors Ready");
    Serial.println("================================");
}

// ===================================================
void loop() {

    // ===== WiFi check =====
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
    }

    // ===================================================
    // 1. ML DATA 
    if (receivedData.length() > 0) {
        Serial.println("Received from Arduino:");
        Serial.println(receivedData);

        float fire = 0, nofire = 0;
        sscanf(receivedData.c_str(), "fire:%f,nofire:%f", &fire, &nofire);

        Serial.println("Parsed ML results:");
        Serial.print("Fire: ");
        Serial.println(fire, 5);
        Serial.print("NoFire: ");
        Serial.println(nofire, 5);

        fireProb = fire;        //  THIS is what goes to dashboard
        noFireProb = nofire;

        Serial.println("--------------------------------");
        receivedData = "";
    }

    // ===================================================
    // 2. LOCAL SENSORS
    int flameState = digitalRead(FLAME_PIN);
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int mq4 = analogRead(MQ4_PIN);

    if (isnan(temperature)) temperature = 0;
    if (isnan(humidity)) humidity = 0;

    // ===================================================
    // 3. SENSOR FUSION (ONLY FOR ALERT)
    float flameScore = (flameState == LOW) ? 1.0 : 0.0;
    float tempScore  = constrain((temperature - 25) / 50.0, 0, 1);
    float smokeScore = constrain((mq4 - 300) / 3000.0, 0, 1);

    float fusedFire =
        (fireProb * 0.6) +
        (flameScore * 0.2) +
        (smokeScore * 0.15) +
        (tempScore * 0.05);

    // ===================================================
    // 4. SERIAL OUTPUT (CLEAR SEPARATION)

    Serial.println("==== ML DATA (FROM ARDUINO) ====");
    Serial.print("Fire Prob: ");
    Serial.println(fireProb);
    Serial.print("NoFire Prob: ");
    Serial.println(noFireProb);

    Serial.println("==== LOCAL SENSORS ====");
    Serial.print("Flame: ");
    Serial.println(flameState == LOW ? "FIRE DETECTED" : "SAFE");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("MQ-4: ");
    Serial.println(mq4);

    Serial.println("==== SENSOR FUSION (FOR ALERT ONLY) ====");
    Serial.print("Fused Fire Score: ");
    Serial.println(fusedFire);

    Serial.println("================================\n");

    // ===================================================
    // 5. ACTUATION (USING FUSION ONLY)
    if (fusedFire >= 0.5) {
        digitalWrite(LED_PIN, HIGH);

        // Passive buzzer tone
        tone(BUZZER_PIN, 2000);
        Serial.println("!!! FIRE ALERT (FUSION) !!!");
    } else {
        digitalWrite(LED_PIN, LOW);
        noTone(BUZZER_PIN);
    }

    // ===================================================
    //  6. SEND TO DASHBOARD (ONLY ML fireProb)
    static unsigned long lastSend = 0;

    if (millis() - lastSend >= 3000) {

        if (WiFi.status() == WL_CONNECTED) {

            HTTPClient http;
            http.begin(serverURL);
            http.addHeader("Content-Type", "application/json");

            StaticJsonDocument<256> doc;

            doc["temperature"] = temperature;
            doc["humidity"]    = humidity;
            doc["smoke"]       = mq4;
            doc["flame"]       = (flameState == LOW) ? 1 : 0;

            //  IMPORTANT: SEND ML ONLY
            doc["fireProb"]    = fireProb;
            doc["noFireProb"]  = noFireProb;

            String json;
            serializeJson(doc, json);

            int code = http.POST(json);

            Serial.print("Dashboard Response: ");
            Serial.println(code);

            http.end();
        }

        lastSend = millis();
    }

    delay(3000);
}