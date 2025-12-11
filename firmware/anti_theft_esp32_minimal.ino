// ESP32 Anti-Theft System - MINIMAL VERSION for Memory Constraints
// Core features only - SMS control, engine control, GPS tracking

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <TinyGPS++.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <time.h>
#include "config.h"

// Core function declarations
void processSMSCommand(String body, String sender);
void syncTimeWithNTP();
unsigned long getCurrentUnixTime();

// Hardware Serial
HardwareSerial gpsSerial(1);
HardwareSerial gsmSerial(2);
TinyGPSPlus gps;
Preferences prefs;

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Core state variables
bool engineRunning = false;
bool systemArmed = true;
unsigned long lastUpdate = 0;
unsigned long lastSMSCheck = 0;
unsigned long lastSMSSent = 0;
bool gsmHealthy = false;

// WiFi Loss Protection
unsigned long lastWiFiCheck = 0;
unsigned long wifiLostTime = 0;
bool wifiWasConnected = false;
bool wifiLossProtectionTriggered = false;
#define WIFI_CHECK_INTERVAL 10000
#define WIFI_LOSS_TIMEOUT (WIFI_LOSS_TIMEOUT_SECONDS * 1000)

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🚗 ESP32 Anti-Theft System - MINIMAL VERSION");
  Serial.println("Core features: SMS control, Engine control, GPS tracking");
  
  // Initialize watchdog
  esp_task_wdt_deinit();
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  // Initialize storage
  prefs.begin("antitheft", false);
  
  // Initialize pins
  pinMode(IGNITION_SWITCH_RELAY_PIN, OUTPUT);
  pinMode(IGNITION_COIL_RELAY_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_INDICATOR_PIN, OUTPUT);
  
  // Initialize relays for ANTI-THEFT
  setIgnitionSwitchRelay(false);
  setIgnitionCoilRelay(false);
  digitalWrite(BUZZER_PIN, LOW);
  setLED(false);
  
  // Restore engine state
  engineRunning = prefs.getBool("engineRun", false);
  systemArmed = prefs.getBool("armed", true);
  
  if (engineRunning && !systemArmed) {
    setIgnitionSwitchRelay(true);
    setIgnitionCoilRelay(true);
    setLED(true);
  } else {
    engineRunning = false;
    prefs.putBool("engineRun", false);
    setLED(false);
  }
  
  // Initialize GPS & GSM
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  gsmSerial.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  
  // Connect WiFi
  connectWiFi();
  
  // Initialize GSM
  initializeGSM();
  
  // Setup Firebase
  setupFirebase();
  
  Serial.println("✅ System Ready - Send '1234 STATUS' via SMS to test");
}

void loop() {
  esp_task_wdt_reset();
  
  // WiFi Loss Protection
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    bool currentWiFiStatus = (WiFi.status() == WL_CONNECTED);
    
    if (currentWiFiStatus) {
      if (!wifiWasConnected) {
        Serial.println("📶 WiFi restored");
        if (wifiLossProtectionTriggered) {
          wifiLossProtectionTriggered = false;
        }
      }
      wifiWasConnected = true;
      wifiLostTime = 0;
    } else {
      if (wifiWasConnected) {
        Serial.println("📶 WiFi lost - starting protection timer");
        wifiLostTime = millis();
        wifiWasConnected = false;
        connectWiFi();
      }
      
      if (WIFI_LOSS_PROTECTION_ENABLED && wifiLostTime > 0 && 
          (millis() - wifiLostTime > WIFI_LOSS_TIMEOUT) && 
          !wifiLossProtectionTriggered) {
        
        Serial.println("🚨 WIFI LOSS PROTECTION - STOPPING ENGINE");
        stopEngine();
        wifiLossProtectionTriggered = true;
        
        if (gsmHealthy) {
          sendSMS("SECURITY: WiFi lost for " + String(WIFI_LOSS_TIMEOUT/1000) + "s. Engine stopped. Send '1234 START' to restart.");
        }
      }
    }
    lastWiFiCheck = millis();
  }
  
  // Process GPS data
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      // GPS data processed
    }
  }
  
  // Update location every 30s
  if (millis() - lastUpdate > 30000 && gps.location.isValid()) {
    updateLocation();
    lastUpdate = millis();
  }
  
  // Check SMS every 15s
  if (millis() - lastSMSCheck > 15000) {
    checkStoredSMS();
    lastSMSCheck = millis();
  }
  
  // Handle GSM data
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    if (response.indexOf("+CMGR:") != -1) {
      parseSimpleSMS(response);
    }
  }
  
  // Vibration detection (simplified)
  static unsigned long lastVibCheck = 0;
  if (millis() - lastVibCheck > 2000) {
    if (digitalRead(VIBRATION_PIN) == HIGH && systemArmed && !engineRunning) {
      Serial.println("🚨 Movement detected!");
      setLED(true);
      tone(BUZZER_PIN, 1500, 200);
      delay(200);
      setLED(false);
      noTone(BUZZER_PIN);
      
      if (gsmHealthy && millis() - lastSMSSent > 120000) {
        sendSMS("ALERT: Movement detected!");
        lastSMSSent = millis();
      }
    }
    lastVibCheck = millis();
  }
  
  // Check Firebase commands
  checkCommands();
  
  delay(100);
}

// ===== CORE FUNCTIONS =====

void setLED(bool enable) {
  digitalWrite(LIGHT_INDICATOR_PIN, enable ? LOW : HIGH);
}

void setIgnitionSwitchRelay(bool enable) {
  digitalWrite(IGNITION_SWITCH_RELAY_PIN, IGNITION_SWITCH_INVERTED ? !enable : enable);
}

void setIgnitionCoilRelay(bool enable) {
  digitalWrite(IGNITION_COIL_RELAY_PIN, IGNITION_COIL_INVERTED ? !enable : enable);
}

void startEngine() {
  if (engineRunning) return;
  
  Serial.println("🚗 Starting engine...");
  setIgnitionCoilRelay(true);
  delay(500);
  setIgnitionSwitchRelay(true);
  setLED(true);
  engineRunning = true;
  prefs.putBool("engineRun", true);
  updateStatus();
}

void stopEngine() {
  Serial.println("🛑 Stopping engine...");
  setIgnitionSwitchRelay(false);
  setIgnitionCoilRelay(false);
  setLED(false);
  engineRunning = false;
  prefs.putBool("engineRun", false);
  
  if (Firebase.ready()) {
    Firebase.RTDB.setString(&fbdo, (String("/devices/") + DEVICE_ID + "/commands/pending").c_str(), "");
  }
  updateStatus();
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
    syncTimeWithNTP();
  }
}

void setupFirebase() {
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;
  auth.user.email = FIREBASE_USER_EMAIL;
  auth.user.password = FIREBASE_USER_PASSWORD;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  delay(2000);
  if (Firebase.ready()) {
    Serial.println("✅ Firebase connected");
  }
}

void updateLocation() {
  if (!Firebase.ready() || !gps.location.isValid()) return;
  
  FirebaseJson json;
  json.set("latitude", gps.location.lat());
  json.set("longitude", gps.location.lng());
  json.set("speed", gps.speed.kmph());
  json.set("satellites", gps.satellites.value());
  json.set("timestamp", getCurrentUnixTime());
  json.set("valid", true);
  
  Firebase.RTDB.setJSON(&fbdo, (String("/devices/") + DEVICE_ID + "/location").c_str(), &json);
}

void updateStatus() {
  if (!Firebase.ready()) return;
  
  FirebaseJson json;
  json.set("status", "online");
  json.set("engineRunning", engineRunning);
  json.set("systemArmed", systemArmed);
  json.set("timestamp", getCurrentUnixTime());
  json.set("uptime", millis() / 1000);
  
  Firebase.RTDB.setJSON(&fbdo, (String("/devices/") + DEVICE_ID + "/status").c_str(), &json);
}

void checkCommands() {
  if (!Firebase.ready()) return;
  
  static unsigned long lastCmd = 0;
  if (millis() - lastCmd < 3000) return;
  
  if (Firebase.RTDB.getString(&fbdo, (String("/devices/") + DEVICE_ID + "/commands/pending").c_str())) {
    String cmd = fbdo.stringData();
    if (cmd.length() > 0 && cmd != "null") {
      cmd.toUpperCase();
      Firebase.RTDB.setString(&fbdo, (String("/devices/") + DEVICE_ID + "/commands/pending").c_str(), "");
      
      if (cmd == "START") {
        startEngine();
      } else if (cmd == "STOP") {
        stopEngine();
      } else if (cmd == "ARM") {
        systemArmed = true;
        updateStatus();
      } else if (cmd == "DISARM") {
        systemArmed = false;
        updateStatus();
      }
      lastCmd = millis();
    }
  }
}

bool initializeGSM() {
  Serial.println("🔧 Initializing GSM...");
  
  gsmSerial.println("AT");
  delay(1000);
  if (!gsmSerial.available()) return false;
  
  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  
  gsmSerial.println("AT+CNMI=2,1,0,0,0");
  delay(1000);
  
  gsmHealthy = true;
  Serial.println("✅ GSM ready");
  return true;
}

void checkStoredSMS() {
  for (int i = 1; i <= 5; i++) {
    gsmSerial.println("AT+CMGR=" + String(i));
    delay(1000);
    
    String response = "";
    while (gsmSerial.available()) {
      response += (char)gsmSerial.read();
    }
    
    if (response.indexOf("+CMGR:") != -1) {
      parseSimpleSMS(response);
      gsmSerial.println("AT+CMGD=" + String(i));
      delay(500);
    }
  }
}

void parseSimpleSMS(String response) {
  int bodyStart = response.indexOf("\n") + 1;
  int bodyEnd = response.indexOf("OK", bodyStart);
  if (bodyEnd == -1) bodyEnd = response.length();
  
  String messageBody = response.substring(bodyStart, bodyEnd);
  messageBody.trim();
  
  if (messageBody.length() > 20) {
    // Try hex decode
    String decoded = "";
    for (int i = 0; i < messageBody.length(); i += 2) {
      if (i + 1 < messageBody.length()) {
        String hexPair = messageBody.substring(i, i + 2);
        char c = (char)strtol(hexPair.c_str(), NULL, 16);
        if (c >= 32 && c <= 126) decoded += c;
      }
    }
    if (decoded.indexOf("1234") != -1) {
      processSMSCommand(decoded, AUTHORIZED_NUMBER_1);
    }
  } else if (messageBody.indexOf("1234") != -1) {
    processSMSCommand(messageBody, AUTHORIZED_NUMBER_1);
  }
}

void processSMSCommand(String body, String sender) {
  if (body.startsWith(String(SMS_PASSWORD) + " ")) {
    String cmd = body.substring(String(SMS_PASSWORD).length() + 1);
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "START") {
      startEngine();
      sendSMS("ENGINE STARTED", sender);
    } else if (cmd == "STOP") {
      stopEngine();
      sendSMS("ENGINE STOPPED", sender);
    } else if (cmd == "STATUS") {
      String status = "STATUS: ";
      status += engineRunning ? "ON" : "OFF";
      status += systemArmed ? " | ARMED" : " | DISARMED";
      status += " | GPS: " + String(gps.satellites.value()) + " sats";
      sendSMS(status, sender);
    } else if (cmd == "LOCATE") {
      if (gps.location.isValid()) {
        String loc = "LOCATION: " + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
        sendSMS(loc, sender);
      } else {
        sendSMS("GPS NOT READY", sender);
      }
    } else if (cmd == "ARM") {
      systemArmed = true;
      sendSMS("SYSTEM ARMED", sender);
    } else if (cmd == "DISARM") {
      systemArmed = false;
      sendSMS("SYSTEM DISARMED", sender);
    }
  }
}

void sendSMS(String message, String phoneNumber = AUTHORIZED_NUMBER_1) {
  if (!gsmHealthy) return;
  
  gsmSerial.print("AT+CMGS=\"" + phoneNumber + "\"\r\n");
  delay(2000);
  gsmSerial.print(message);
  delay(500);
  gsmSerial.write(26);
  delay(2000);
}

void syncTimeWithNTP() {
  configTime(8 * 3600, 0, "pool.ntp.org");
  delay(2000);
}

unsigned long getCurrentUnixTime() {
  time_t now = time(nullptr);
  if (now > 1000000000L) {
    return (unsigned long)now;
  } else {
    return 1735689600 + (millis() / 1000); // 2025 base
  }
}