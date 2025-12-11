// ESP32 Anti-Theft System - Optimized for Flash Memory
// Reduced size version with core features only

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <TinyGPS++.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"

// Fallback LED configuration if not defined in config.h
#ifndef LED_INVERTED
#define LED_INVERTED true  // ESP32 built-in LED is inverted (LOW=ON, HIGH=OFF)
#endif

// Function declarations
void handleStoredSMS(String response);
void processSMSCommand(String body, String sender);
void handleImmediateTheftAlert();
void sendImmediateSMSAlert();
void sendEngineStatusSMS(String status);
void performGSMHealthCheck();
String decodeHexSMS(String hexString);
void parseSimpleSMS(String response);
void syncTimeWithNTP();
String getCurrentTimestamp();
unsigned long getCurrentUnixTime();

// Watchdog timeout (30 seconds)
#define WDT_TIMEOUT 30

// Relay Logic Configuration (from config.h)
#ifndef IGNITION_SWITCH_INVERTED
#define IGNITION_SWITCH_INVERTED false
#endif
#ifndef IGNITION_COIL_INVERTED
#define IGNITION_COIL_INVERTED false
#endif

// Hardware Serial
HardwareSerial gpsSerial(1);
HardwareSerial gsmSerial(2);
TinyGPSPlus gps;

// Persistent storage
Preferences prefs;

// Timing for starter safety
unsigned long starterStartTime = 0;
unsigned long lastStarterAttempt = 0;
bool starterActive = false;
#define STARTER_TIMEOUT 3000      // 3 seconds max
#define STARTER_COOLDOWN 10000    // 10 seconds between attempts
#define MAX_STARTER_ATTEMPTS 3    // Max attempts before lockout
int starterAttempts = 0;
unsigned long starterLockoutTime = 0;
#define STARTER_LOCKOUT_DURATION 300000  // 5 minutes lockout

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// State
bool engineRunning = false;
bool systemArmed = true;
unsigned long lastUpdate = 0;
unsigned long lastHeartbeat = 0;
unsigned long reconnectAttempt = 0;
bool useGSMFallback = false;
bool gsmConnected = false;
unsigned long lastGSMCheck = 0;

// SMS and Dashboard commands work together seamlessly

// System Stability Monitoring
unsigned long lastGSMHealthCheck = 0;
#define GSM_HEALTH_CHECK_INTERVAL 60000  // Check GSM health every 60 seconds
bool gsmHealthy = false;

// WiFi Loss Protection
unsigned long lastWiFiCheck = 0;
unsigned long wifiLostTime = 0;
bool wifiWasConnected = false;
bool wifiLossProtectionTriggered = false;
#define WIFI_CHECK_INTERVAL 5000        // Check WiFi every 5 seconds
#define WIFI_LOSS_TIMEOUT (WIFI_LOSS_TIMEOUT_SECONDS * 1000)  // Convert to milliseconds

// Monitoring
unsigned long lastMonitorPrint = 0;
#define MONITOR_INTERVAL 30000  // Print sensor status every 30 seconds (reduced spam)

// GPS Diagnostics
unsigned long lastGPSDiagnostic = 0;
#define GPS_DIAGNOSTIC_INTERVAL 30000  // GPS diagnostics every 30 seconds

// SMS Check
unsigned long lastSMSCheck = 0;
#define SMS_CHECK_INTERVAL 15000  // Check for SMS every 15 seconds

// SMS Spam Prevention - Enhanced System
unsigned long lastSMSSent = 0;
unsigned long lastEngineStatusSMS = 0;
unsigned long lastMovementSMS = 0;
unsigned long lastSystemNotificationSMS = 0;
#define SMS_COOLDOWN 30000  // 30 seconds between SMS sends
#define ENGINE_STATUS_SMS_COOLDOWN 300000  // 5 minutes between engine status SMS
#define MOVEMENT_SMS_COOLDOWN 120000  // 2 minutes between movement SMS
#define SYSTEM_SMS_COOLDOWN 600000  // 10 minutes between system notifications

// Smart Movement Alert System
unsigned long lastMovementAlert = 0;
unsigned long firstMovementTime = 0;
int movementCount = 0;
bool alertSent = false;
#define MOVEMENT_DETECTION_WINDOW 30000   // 30 seconds to detect multiple movements
#define MOVEMENT_THRESHOLD 3              // 3 movements within 30 seconds = alert
#define MOVEMENT_ALERT_COOLDOWN 120000    // 2 minutes between alerts (reduced from 5 minutes)

// Cached GSM status (to avoid blocking)
struct GSMStatus {
  bool moduleResponding = false;
  int signalStrength = 0;
  bool networkRegistered = false;
  unsigned long lastUpdate = 0;
};
GSMStatus cachedGSMStatus;
#define GSM_STATUS_UPDATE_INTERVAL 60000  // Update GSM status every 60s



// Geofence with hysteresis
struct GeoFence {
  double centerLat;
  double centerLng;
  double radiusMeters;
  double hysteresis;  // Buffer zone to prevent rapid alerts
  bool enabled;
};
GeoFence homeFence = {14.5995, 120.9842, 500.0, 20.0, false};  // 20m hysteresis
bool wasInsideFence = true;

// Speed
float speedLimit = 80.0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow serial to stabilize
  
  // Print reboot reason for debugging
  Serial.println("\n==================================================");
  Serial.println("🔄 ESP32 REBOOT DETECTED");
  Serial.println("Reboot reason: " + String(esp_reset_reason()));
  Serial.println("==================================================");
  
  // Initialize watchdog timer (CRITICAL SAFETY FEATURE)
  Serial.println("Initializing watchdog timer...");
  // ESP32 v3.x compatible watchdog configuration
  esp_task_wdt_deinit();  // Deinitialize if already initialized
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,  // Convert seconds to milliseconds
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_err_t wdt_result = esp_task_wdt_init(&wdt_config);
  if (wdt_result == ESP_OK) {
    esp_task_wdt_add(NULL);
    Serial.println("✓ Watchdog timer initialized (30s timeout)");
  } else {
    Serial.println("⚠️ Watchdog already active (system default)");
  }
  
  // Initialize persistent storage
  prefs.begin("antitheft", false);
  
  // Pins
  pinMode(IGNITION_SWITCH_RELAY_PIN, OUTPUT);  // Ignition switch relay (GPIO 12)
  pinMode(IGNITION_COIL_RELAY_PIN, OUTPUT);    // Ignition coil relay (GPIO 13)
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_INDICATOR_PIN, OUTPUT);
  
  // Initialize relays for ANTI-THEFT PROTECTION
  Serial.println("🔒 Initializing ANTI-THEFT system...");
  setIgnitionSwitchRelay(false);  // Ignition switch OFF
  setIgnitionCoilRelay(false);    // Ignition coil OFF (CRITICAL - prevents physical starting)
  digitalWrite(BUZZER_PIN, LOW);
  setLED(false);  // LED OFF initially
  
  // Restore engine state from persistent storage
  engineRunning = prefs.getBool("engineRun", false);
  systemArmed = prefs.getBool("armed", true);
  
  // ANTI-THEFT LOGIC: Only restore ignition coil if engine was running AND system is not armed
  if (engineRunning && !systemArmed) {
    Serial.println("⚠️ Restoring engine state: RUNNING (system disarmed)");
    setIgnitionSwitchRelay(true);   // Enable ignition switch
    setIgnitionCoilRelay(true);     // Enable ignition coil
    setLED(true);  // LED ON when engine running
  } else if (engineRunning && systemArmed) {
    Serial.println("🔒 Engine was running but ANTI-THEFT is ACTIVE - ignition coil disabled");
    Serial.println("🔒 Send '1234 START' to enable ignition coil");
    engineRunning = false;  // Force engine state to stopped for safety
    prefs.putBool("engineRun", false);  // Update persistent storage
    setLED(false);  // LED OFF
  } else {
    setLED(false);  // LED OFF when engine stopped
    Serial.println("✓ Engine state: STOPPED");
  }
  
  // GPS & GSM
  Serial.println("Initializing GPS...");
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("✓ GPS Serial initialized (9600 baud, RX=16, TX=17)");
  
  Serial.println("Initializing GSM...");
  gsmSerial.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  Serial.println("✓ GSM Serial initialized (9600 baud, RX=25, TX=26)");
  
  Serial.println("Connecting WiFi...");
  connectWiFi();
  
  Serial.println("Initializing GSM module...");
  // Reset watchdog before long GSM initialization
  esp_task_wdt_reset();
  
  // Try GSM initialization multiple times with better error handling
  bool gsmInitialized = false;
  for (int attempt = 1; attempt <= 5; attempt++) {
    Serial.println("GSM initialization attempt " + String(attempt) + "/5");
    
    // Clear GSM buffer before each attempt
    while (gsmSerial.available()) {
      gsmSerial.read();
    }
    
    if (initializeGSM()) {
      Serial.println("✓ GSM module ready for SMS commands");
      Serial.println("✓ GSM internet fallback available");
      gsmInitialized = true;
      gsmHealthy = true;
      break;
    } else {
      Serial.println("❌ GSM initialization attempt " + String(attempt) + " failed");
      if (attempt < 5) {
        Serial.println("⏳ Waiting 3 seconds before retry...");
        delay(3000);
        esp_task_wdt_reset();
      }
    }
  }
  
  if (!gsmInitialized) {
    Serial.println("⚠️ GSM initialization failed after 3 attempts");
    Serial.println("📱 SMS commands may not work - check GSM module connection");
  }
  
  Serial.println("Init Firebase...");
  setupFirebase();
  
  // Check if Firebase connected
  if (!Firebase.ready()) {
    Serial.println("⚠️ WARNING: Firebase connection failed!");
    Serial.println("📱 System will work in SMS-ONLY mode");
    Serial.println("✓ You can still control via SMS commands");
  } else {
    Serial.println("✓ Firebase connected successfully");
    updateStatus();
  }
  
  Serial.println("\n========================================");
  Serial.println("🚗 ANTI-THEFT SYSTEM READY!");
  Serial.println("========================================");
  Serial.println("🔒 ANTI-THEFT STATUS: ACTIVE");
  Serial.println("🔒 Ignition coil is DISABLED - engine cannot start");
  Serial.println("🔒 Physical ignition switch will NOT work");
  Serial.println("");
  Serial.println("Control Methods:");
  Serial.println("1. SMS: Send '1234 START' to ENABLE ignition coil");
  Serial.println("2. SMS: Send '1234 STOP' to DISABLE ignition coil");
  Serial.println("3. SMS: Send '1234 LOCATE' for GPS location");
  Serial.println("");
  Serial.println("⚠️  IMPORTANT: Engine cannot start until you send '1234 START'");
  
  // Print initial sensor status
  Serial.println("\n📊 SENSOR STATUS:");
  printSensorStatus();
  if (Firebase.ready()) {
    Serial.println("4. Dashboard: Use web interface");
  }
  Serial.println("========================================\n");
}

void loop() {
  // Reset watchdog timer (CRITICAL - prevents system hang)
  esp_task_wdt_reset();
  
  // WiFi Loss Protection - Enhanced Security Feature
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    bool currentWiFiStatus = (WiFi.status() == WL_CONNECTED);
    
    if (currentWiFiStatus) {
      // WiFi is connected
      if (!wifiWasConnected) {
        Serial.println("📶 WiFi connection restored");
        if (wifiLossProtectionTriggered) {
          Serial.println("🔓 WiFi restored - Anti-theft protection can be disabled via SMS/Dashboard");
          wifiLossProtectionTriggered = false;
        }
      }
      wifiWasConnected = true;
      wifiLostTime = 0;  // Reset WiFi loss timer
      
      // Disable GSM fallback when WiFi is restored
      if (useGSMFallback) {
        Serial.println("WiFi restored, disabling GSM fallback");
        useGSMFallback = false;
      }
      
    } else {
      // WiFi is disconnected
      if (wifiWasConnected) {
        Serial.println("📶 WiFi connection lost - starting protection timer");
        wifiLostTime = millis();
        wifiWasConnected = false;
        
        // Attempt reconnection
        Serial.println("Attempting WiFi reconnection...");
        connectWiFi();
      }
      
      // Check if WiFi has been lost for too long
      if (WIFI_LOSS_PROTECTION_ENABLED && wifiLostTime > 0 && 
          (millis() - wifiLostTime > WIFI_LOSS_TIMEOUT) && 
          !wifiLossProtectionTriggered) {
        
        Serial.println("🚨 WIFI LOSS PROTECTION TRIGGERED!");
        Serial.println("🔒 WiFi lost for " + String(WIFI_LOSS_TIMEOUT/1000) + " seconds - STOPPING ENGINE FOR SECURITY");
        
        // Force stop engine for security
        stopEngine();
        wifiLossProtectionTriggered = true;
        
        // Send SMS alert if GSM is available
        if (gsmHealthy) {
          String alertMsg = "SECURITY ALERT: WiFi connection lost for " + String(WIFI_LOSS_TIMEOUT/1000) + 
                           " seconds. Engine stopped for protection. Send '1234 START' to restart.";
          sendSMS(alertMsg, AUTHORIZED_NUMBER_1);
          delay(2000);
          sendSMS(alertMsg, AUTHORIZED_NUMBER_2);
        }
        
        // Enable GSM fallback for emergency control
        if (!useGSMFallback) {
          Serial.println("📱 Enabling GSM fallback for emergency control");
          useGSMFallback = true;
          enableGPRS();
        }
      }
      
      // Enable GSM fallback after 30 seconds of WiFi loss
      if (wifiLostTime > 0 && (millis() - wifiLostTime > 30000) && !useGSMFallback) {
        Serial.println("WiFi lost for 30s, enabling GSM fallback");
        useGSMFallback = true;
        enableGPRS();
      }
    }
    
    lastWiFiCheck = millis();
  }
  
  // Update cached GSM status every 60s (non-blocking)
  if (millis() - cachedGSMStatus.lastUpdate > GSM_STATUS_UPDATE_INTERVAL) {
    updateGSMStatusCache();
    cachedGSMStatus.lastUpdate = millis();
  }
  
  // Check GSM connection status every 60s
  if (useGSMFallback && millis() - lastGSMCheck > 60000) {
    checkGSMConnection();
    lastGSMCheck = millis();
  }
  
  // GSM Health Check - Monitor and reinitialize if needed
  if (millis() - lastGSMHealthCheck > GSM_HEALTH_CHECK_INTERVAL) {
    Serial.println("🔍 GSM Health Check...");
    bool previousGSMHealth = gsmHealthy;
    performGSMHealthCheck();
    
    // Notify when GSM becomes stable (only once)
    if (!previousGSMHealth && gsmHealthy) {
      Serial.println("📱 GSM NOW STABLE - SMS commands available");
      // Optional: Send single notification when GSM becomes ready
      // sendSMS("GSM Ready - You can now send SMS commands", AUTHORIZED_NUMBER_1);
    }
    
    lastGSMHealthCheck = millis();
  }
  
  // Check for stored SMS messages every 10 seconds (enhanced parsing)
  if (millis() - lastSMSCheck > 10000) {
    Serial.println("🔍 Enhanced SMS check...");
    checkStoredSMSEnhanced();
    lastSMSCheck = millis();
  }
  
  // GPS - Process incoming data
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (gps.encode(c)) {
      // New sentence processed - check if we got a fix
      if (gps.location.isUpdated()) {
        Serial.println("🛰️ GPS location updated!");
      }
    }
  }
  
  // Update location every 15s (less frequent to save data, but with better validation)
  if (millis() - lastUpdate > 15000 && gps.location.isValid()) {
    if (useGSMFallback) {
      sendLocationViaGSM();
    } else {
      updateLocationEnhanced();  // Use enhanced location function
    }
    checkGeofence();
    lastUpdate = millis();
  }
  
  // Heartbeat every 30s
  if (millis() - lastHeartbeat > 30000) {
    if (useGSMFallback) {
      sendStatusViaGSM();
    } else {
      updateStatus();
    }
    lastHeartbeat = millis();
  }
  

  
  // Smart Vibration Detection (only when engine stopped and system armed)
  static unsigned long lastVibrationCheck = 0;
  static bool lastVibrationState = false;
  static unsigned long lastVibrationTrigger = 0;
  
  if (millis() - lastVibrationCheck > 1000) { // Check every 1 second (reduced frequency)
    bool currentVibrationState = digitalRead(VIBRATION_PIN) == HIGH;
    
    // Debug vibration sensor state changes (reduced spam)
    if (currentVibrationState != lastVibrationState) {
      Serial.println("Vibration: " + String(currentVibrationState ? "DETECTED" : "STOPPED"));
      Serial.println("Engine: " + String(engineRunning ? "ON" : "OFF") + " | Armed: " + String(systemArmed ? "YES" : "NO"));
    }
    
    // CONTROLLED ALERT on movement detection (prevent infinite buzzing)
    if (currentVibrationState && !lastVibrationState && systemArmed && !engineRunning) {
      // Minimum 30 seconds between alerts (prevent buzzer spam)
      if (millis() - lastVibrationTrigger > 30000) {
        Serial.println("🚨 VIBRATION ALERT TRIGGERED!");
        handleAlert();  // Use standard alert function (not immediate)
        lastVibrationTrigger = millis();
      } else {
        Serial.println("🔍 Vibration detected but cooldown active (" + String((30000 - (millis() - lastVibrationTrigger))/1000) + "s remaining)");
      }
    }
    
    lastVibrationState = currentVibrationState;
    lastVibrationCheck = millis();
  }
  
  // GSM (with buffer overflow protection and debugging)
  static String gsmBuffer = ""; // Buffer to accumulate GSM data
  
  if (gsmSerial.available()) {
    String response = "";
    unsigned long startTime = millis();
    // Read with timeout and size limit
    while (gsmSerial.available() && response.length() < 512 && (millis() - startTime < 1000)) {
      char c = gsmSerial.read();
      if (c != -1) response += c;
      delay(1);  // Small delay for serial buffer
    }
    
    // Add to buffer for multi-line SMS processing
    gsmBuffer += response;
    
    // Debug: Show all GSM responses (but filter out empty responses)
    if (response.length() > 2) {
      Serial.println("📡 GSM Data Received: " + response);
      
      // Check for different SMS notification formats
      if (response.indexOf("RING") != -1) {
        Serial.println("📞 Incoming call detected");
        handleCall(response);
      }
      
      // Handle immediate SMS notifications (+CMT format)
      if (gsmBuffer.indexOf("+CMT:") != -1) {
        Serial.println("📱 Immediate SMS message detected (+CMT)");
        
        // Check if we have the complete SMS (header + body)
        int cmtIndex = gsmBuffer.indexOf("+CMT:");
        int nextLineIndex = gsmBuffer.indexOf("\n", cmtIndex);
        
        if (nextLineIndex != -1 && gsmBuffer.length() > nextLineIndex + 1) {
          // We have both header and body, process the complete SMS
          String completeSMS = gsmBuffer.substring(cmtIndex);
          Serial.println("📱 Processing complete SMS: " + completeSMS);
          handleSMS(completeSMS);
          
          // Clear the processed SMS from buffer
          gsmBuffer = "";
        }
      }
      
      // Handle SMS delivery notifications (+CMTI format)
      if (response.indexOf("+CMTI:") != -1) {
        Serial.println("📱 SMS storage notification detected (+CMTI)");
        // SMS was stored, check it immediately
        delay(1000);
        checkStoredSMS();
      }
    }
    
    // Clear buffer if it gets too large (prevent memory issues)
    if (gsmBuffer.length() > 1000) {
      Serial.println("⚠️ GSM buffer too large, clearing...");
      gsmBuffer = "";
    }
  }
  
  // Check starter timeout
  checkStarterTimeout();
  
  // Commands
  if (!useGSMFallback) {
    checkCommands();
  }
  
  // Serial commands for testing SMS
  if (Serial.available()) {
    String serialCommand = Serial.readString();
    serialCommand.trim();
    serialCommand.toUpperCase();
    
    if (serialCommand == "TEST SMS") {
      Serial.println("🧪 Testing SMS reception...");
      checkStoredSMS();
    } else if (serialCommand == "SMS STATUS") {
      testSMSFunctionality();
    } else if (serialCommand.startsWith("SEND ")) {
      String message = serialCommand.substring(5);
      sendSMS(message);
    } else if (serialCommand == "CHECK SMS") {
      checkStoredSMS();
    } else if (serialCommand == "SHOW CONFIG") {
      Serial.println("📋 CURRENT CONFIGURATION:");
      Serial.println("Authorized Number 1: " + String(AUTHORIZED_NUMBER_1));
      Serial.println("Authorized Number 2: " + String(AUTHORIZED_NUMBER_2));
      Serial.println("SMS Password: " + String(SMS_PASSWORD));
      Serial.println("Engine Running: " + String(engineRunning ? "YES" : "NO"));
      Serial.println("System Armed: " + String(systemArmed ? "YES" : "NO"));
    } else if (serialCommand == "FORCE START") {
      Serial.println("🚗 FORCE STARTING ENGINE...");
      startEngine();
    } else if (serialCommand == "RESET SMS") {
      Serial.println("🔄 RESETTING SMS CONFIGURATION...");
      initializeGSM();
    } else if (serialCommand == "ALERT STATUS") {
      Serial.println("📊 MOVEMENT ALERT STATUS:");
      Serial.println("   Movement Count: " + String(movementCount) + "/" + String(MOVEMENT_THRESHOLD));
      Serial.println("   Alert Sent: " + String(alertSent ? "YES" : "NO"));
      Serial.println("   Cooldown Remaining: " + String(max(0L, (long)(MOVEMENT_ALERT_COOLDOWN - (millis() - lastMovementAlert))/1000)) + "s");
      Serial.println("   Vibration Pin State: " + String(digitalRead(VIBRATION_PIN) == HIGH ? "ACTIVE" : "INACTIVE"));
      Serial.println("   Engine Running: " + String(engineRunning ? "YES" : "NO"));
      Serial.println("   System Armed: " + String(systemArmed ? "YES" : "NO"));
    } else if (serialCommand == "RESET ALERTS") {
      Serial.println("🔄 RESETTING MOVEMENT ALERTS...");
      movementCount = 0;
      alertSent = false;
      lastMovementAlert = 0;
      firstMovementTime = 0;
      Serial.println("✅ Movement alert system reset");
    } else if (serialCommand == "RESET GSM") {
      Serial.println("🔄 MANUALLY RESETTING GSM MODULE...");
      gsmHealthy = false;
      performGSMHealthCheck();
    } else if (serialCommand == "WIFI STATUS") {
      Serial.println("📶 WIFI PROTECTION STATUS:");
      Serial.println("  WiFi Connected: " + String(WiFi.status() == WL_CONNECTED ? "YES" : "NO"));
      Serial.println("  Protection Enabled: " + String(WIFI_LOSS_PROTECTION_ENABLED ? "YES" : "NO"));
      Serial.println("  Protection Triggered: " + String(wifiLossProtectionTriggered ? "YES" : "NO"));
      Serial.println("  Timeout Setting: " + String(WIFI_LOSS_TIMEOUT_SECONDS) + " seconds");
      if (wifiLostTime > 0 && WiFi.status() != WL_CONNECTED) {
        unsigned long lostDuration = (millis() - wifiLostTime) / 1000;
        Serial.println("  WiFi Lost For: " + String(lostDuration) + " seconds");
        Serial.println("  Time Until Auto-Stop: " + String(max(0L, (long)(WIFI_LOSS_TIMEOUT_SECONDS - lostDuration))) + " seconds");
      }
    } else if (serialCommand == "TEST GSM") {
      Serial.println("🧪 TESTING GSM MODULE...");
      testSMSFunctionality();
    } else if (serialCommand == "TEST LED") {
      Serial.println("🔆 TESTING LED INDICATOR...");
      Serial.println("LED OFF");
      setLED(false);
      delay(1000);
      Serial.println("LED ON");
      setLED(true);
      delay(1000);
      Serial.println("LED OFF");
      setLED(false);
      Serial.println("✅ LED test complete");
    } else if (serialCommand == "TEST RELAYS") {
      Serial.println("🔌 TESTING RELAY CONTROL...");
      Serial.println("All relays OFF");
      setIgnitionSwitchRelay(false);
      setIgnitionCoilRelay(false);
      delay(2000);
      Serial.println("Ignition Switch Relay ON");
      setIgnitionSwitchRelay(true);
      delay(2000);
      Serial.println("Ignition Coil Relay ON");
      setIgnitionCoilRelay(true);
      delay(2000);
      Serial.println("All relays OFF");
      setIgnitionSwitchRelay(false);
      setIgnitionCoilRelay(false);
      Serial.println("✅ Relay test complete");
    } else if (serialCommand == "READ SMS") {
      Serial.println("📱 MANUALLY READING SMS...");
      checkStoredSMS();
    } else if (serialCommand == "CLEAR SMS") {
      Serial.println("🗑️ CLEARING ALL SMS...");
      gsmSerial.println("AT+CMGDA=\"DEL ALL\"");
      delay(2000);
      Serial.println("✅ All SMS cleared");
    } else if (serialCommand == "TEST SMS PARSE") {
      Serial.println("🧪 TESTING SMS PARSING...");
      String testSMS = "+CMT: \"+639675715673\",\"\",\"25/12/10,17:32:20+32\"\n1234 START";
      Serial.println("Test SMS: " + testSMS);
      handleSMS(testSMS);
    } else if (serialCommand == "ENHANCED SMS CHECK") {
      Serial.println("🧪 TESTING ENHANCED SMS CHECK...");
      checkStoredSMSEnhanced();
    } else if (serialCommand == "TEST HEX DECODE") {
      Serial.println("🧪 TESTING HEX DECODE...");
      String testHex = "31323334205354415254"; // "1234 START" in hex
      String decoded = decodeHexSMS(testHex);
      Serial.println("Hex: " + testHex + " -> Decoded: " + decoded);
    } else if (serialCommand == "SYNC TIME") {
      Serial.println("🕐 MANUAL TIME SYNC...");
      syncTimeWithNTP();
    } else if (serialCommand == "SHOW TIME") {
      Serial.println("🕐 CURRENT TIME INFO:");
      Serial.println("Unix timestamp: " + String(getCurrentUnixTime()));
      Serial.println("Human readable: " + getCurrentTimestamp());
      Serial.println("System millis: " + String(millis()));
    } else if (serialCommand == "TEST GPS") {
      Serial.println("🛰️ GPS TEST UPDATE...");
      updateLocationEnhanced();
    } else if (serialCommand == "DISABLE BUZZER") {
      Serial.println("🔇 EMERGENCY: DISABLING BUZZER!");
      noTone(BUZZER_PIN);
      digitalWrite(BUZZER_PIN, LOW);
      systemArmed = false; // Disarm system to stop alerts
      Serial.println("✅ Buzzer disabled and system disarmed");
    } else if (serialCommand == "EMERGENCY STOP") {
      Serial.println("🚨 EMERGENCY STOP ACTIVATED!");
      noTone(BUZZER_PIN);
      digitalWrite(BUZZER_PIN, LOW);
      systemArmed = false;
      stopEngine();
      Serial.println("✅ Emergency stop complete - all systems disabled");
    }
  }
  
  // Print sensor monitoring every 30 seconds (reduced spam)
  if (millis() - lastMonitorPrint > MONITOR_INTERVAL) {
    printSensorStatus();
    lastMonitorPrint = millis();
  }
  
  // GPS diagnostics every 30 seconds
  if (millis() - lastGPSDiagnostic > GPS_DIAGNOSTIC_INTERVAL) {
    printGPSDiagnostics();
    lastGPSDiagnostic = millis();
  }
  
  // Small delay to prevent tight loop (non-blocking)
  delay(10);  // Reduced from 100ms for better responsiveness
}

// ===== LED CONTROL HELPER FUNCTION =====
void setLED(bool enable) {
  // ESP32 built-in LED is always inverted: LOW=ON, HIGH=OFF
  digitalWrite(LIGHT_INDICATOR_PIN, enable ? LOW : HIGH);
  Serial.print("🔆 LED (GPIO 2): ");
  Serial.print(enable ? "ON" : "OFF");
  Serial.print(" (wrote ");
  Serial.print(enable ? "LOW" : "HIGH");
  Serial.println(" to GPIO)");
  
  // Verify the LED state
  delay(10);
  int readValue = digitalRead(LIGHT_INDICATOR_PIN);
  Serial.println("🔍 GPIO 2 actual state: " + String(readValue == LOW ? "LOW" : "HIGH"));
}

// ===== RELAY CONTROL HELPER FUNCTIONS =====
void setIgnitionSwitchRelay(bool enable) {
  if (IGNITION_SWITCH_INVERTED) {
    digitalWrite(IGNITION_SWITCH_RELAY_PIN, enable ? LOW : HIGH);
    Serial.print("🔌 Ignition Switch Relay (GPIO 12): ");
    Serial.print(enable ? "ENABLED" : "DISABLED");
    Serial.print(" (NC logic - wrote ");
    Serial.print(enable ? "LOW" : "HIGH");
    Serial.println(")");
  } else {
    digitalWrite(IGNITION_SWITCH_RELAY_PIN, enable ? HIGH : LOW);
    Serial.print("🔌 Ignition Switch Relay (GPIO 12): ");
    Serial.print(enable ? "ENABLED" : "DISABLED");
    Serial.print(" (NO logic - wrote ");
    Serial.print(enable ? "HIGH" : "LOW");
    Serial.println(")");
  }
  
  // Verify relay state
  delay(10);
  int actualState = digitalRead(IGNITION_SWITCH_RELAY_PIN);
  Serial.println("🔍 GPIO 12 actual state: " + String(actualState == HIGH ? "HIGH" : "LOW"));
}

void setIgnitionCoilRelay(bool enable) {
  if (IGNITION_COIL_INVERTED) {
    digitalWrite(IGNITION_COIL_RELAY_PIN, enable ? LOW : HIGH);
    Serial.print("🔌 Ignition Coil Relay (GPIO 13): ");
    Serial.print(enable ? "ENABLED" : "DISABLED");
    Serial.print(" (NC logic - wrote ");
    Serial.print(enable ? "LOW" : "HIGH");
    Serial.println(")");
  } else {
    digitalWrite(IGNITION_COIL_RELAY_PIN, enable ? HIGH : LOW);
    Serial.print("🔌 Ignition Coil Relay (GPIO 13): ");
    Serial.print(enable ? "ENABLED" : "DISABLED");
    Serial.print(" (NO logic - wrote ");
    Serial.print(enable ? "HIGH" : "LOW");
    Serial.println(")");
  }
  
  // Verify relay state
  delay(10);
  int actualState = digitalRead(IGNITION_COIL_RELAY_PIN);
  Serial.println("🔍 GPIO 13 actual state: " + String(actualState == HIGH ? "HIGH" : "LOW"));
}

// ===== GSM STATUS CACHE UPDATE (NON-BLOCKING) =====
void updateGSMStatusCache() {
  // Quick check - module responding
  gsmSerial.println("AT");
  delay(100);
  if (gsmSerial.available()) {
    String response = "";
    while (gsmSerial.available() && response.length() < 128) {
      response += (char)gsmSerial.read();
    }
    cachedGSMStatus.moduleResponding = (response.indexOf("OK") != -1);
  } else {
    cachedGSMStatus.moduleResponding = false;
  }
  
  // Signal strength
  gsmSerial.println("AT+CSQ");
  delay(100);
  if (gsmSerial.available()) {
    String response = "";
    while (gsmSerial.available() && response.length() < 128) {
      response += (char)gsmSerial.read();
    }
    int csqIndex = response.indexOf("+CSQ: ");
    if (csqIndex != -1) {
      cachedGSMStatus.signalStrength = response.substring(csqIndex + 6, csqIndex + 8).toInt();
    }
  }
  
  // Network registration
  gsmSerial.println("AT+CREG?");
  delay(100);
  if (gsmSerial.available()) {
    String response = "";
    while (gsmSerial.available() && response.length() < 128) {
      response += (char)gsmSerial.read();
    }
    cachedGSMStatus.networkRegistered = (response.indexOf(",1") != -1 || response.indexOf(",5") != -1);
  }
}

// ===== SENSOR MONITORING FUNCTION =====
void printSensorStatus() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║       SENSOR STATUS MONITOR            ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  // System Status
  Serial.println("\n📡 SYSTEM:");
  Serial.print("  ├─ WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✓ Connected (");
    Serial.print(WiFi.localIP());
    Serial.print(") RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ Disconnected");
  }
  
  Serial.print("  ├─ Firebase: ");
  Serial.println(Firebase.ready() ? "✓ Connected" : "✗ Disconnected");
  
  Serial.print("  ├─ GSM Fallback: ");
  Serial.println(useGSMFallback ? "✓ Active" : "○ Standby");
  
  Serial.print("  ├─ WiFi Protection: ");
  Serial.println(WIFI_LOSS_PROTECTION_ENABLED ? "✓ Enabled" : "○ Disabled");
  if (wifiLossProtectionTriggered) {
    Serial.println("  ├─ Protection Status: ⚠️ TRIGGERED - Engine stopped for security");
  }
  Serial.print("  └─ Uptime: ");
  unsigned long uptime = millis() / 1000;
  Serial.print(uptime / 3600);
  Serial.print("h ");
  Serial.print((uptime % 3600) / 60);
  Serial.print("m ");
  Serial.print(uptime % 60);
  Serial.println("s");
  
  // GPS Status
  Serial.println("\n🛰️ GPS:");
  Serial.print("  ├─ Status: ");
  if (gps.location.isValid()) {
    Serial.println("✓ Fix Acquired");
    Serial.print("  ├─ Latitude: ");
    Serial.println(gps.location.lat(), 6);
    Serial.print("  ├─ Longitude: ");
    Serial.println(gps.location.lng(), 6);
    Serial.print("  ├─ Speed: ");
    Serial.print(gps.speed.kmph(), 1);
    Serial.println(" km/h");
    Serial.print("  ├─ Altitude: ");
    Serial.print(gps.altitude.meters(), 1);
    Serial.println(" m");
    Serial.print("  ├─ Satellites: ");
    Serial.println(gps.satellites.value());
    Serial.print("  └─ HDOP: ");
    Serial.println(gps.hdop.value());
  } else {
    Serial.println("✗ No Fix");
    Serial.print("  ├─ Satellites: ");
    Serial.println(gps.satellites.value());
    Serial.print("  └─ Characters: ");
    Serial.println(gps.charsProcessed());
  }
  
  // GSM Status (using cached data - non-blocking)
  Serial.println("\n📱 GSM:");
  Serial.print("  ├─ Module: ");
  Serial.println(cachedGSMStatus.moduleResponding ? "✓ Responding" : "✗ Not Responding");
  
  Serial.print("  ├─ Signal: ");
  Serial.print(cachedGSMStatus.signalStrength);
  Serial.print("/31 (");
  if (cachedGSMStatus.signalStrength > 20) Serial.print("Excellent");
  else if (cachedGSMStatus.signalStrength > 15) Serial.print("Good");
  else if (cachedGSMStatus.signalStrength > 10) Serial.print("Fair");
  else if (cachedGSMStatus.signalStrength > 5) Serial.print("Poor");
  else Serial.print("Very Poor");
  Serial.println(")");
  
  Serial.print("  └─ Network: ");
  Serial.println(cachedGSMStatus.networkRegistered ? "✓ Registered" : "✗ Not Registered");
  
  // Engine Control
  Serial.println("\n🚗 ENGINE:");
  Serial.print("  ├─ Status: ");
  Serial.println(engineRunning ? "✓ Running" : "○ Stopped");
  Serial.print("  ├─ Ignition Switch Relay (GPIO ");
  Serial.print(IGNITION_SWITCH_RELAY_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(IGNITION_SWITCH_RELAY_PIN) ? "ON" : "OFF");
  Serial.print("  └─ Ignition Coil Relay (GPIO ");
  Serial.print(IGNITION_COIL_RELAY_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(IGNITION_COIL_RELAY_PIN) ? "ON" : "OFF");
  
  // Security
  Serial.println("\n🔒 SECURITY:");
  Serial.print("  ├─ System: ");
  Serial.println(systemArmed ? "✓ Armed" : "○ Disarmed");
  Serial.print("  ├─ Vibration Sensor (GPIO ");
  Serial.print(VIBRATION_PIN);
  Serial.print("): ");
  int vibration = digitalRead(VIBRATION_PIN);
  Serial.println(vibration ? "⚠️ TRIGGERED" : "○ Normal");
  Serial.print("  ├─ Buzzer (GPIO ");
  Serial.print(BUZZER_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(BUZZER_PIN) ? "ON" : "OFF");
  Serial.print("  └─ LED (GPIO ");
  Serial.print(LIGHT_INDICATOR_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(LIGHT_INDICATOR_PIN) ? "ON" : "OFF");
  
  // Geofence
  if (homeFence.enabled && gps.location.isValid()) {
    Serial.println("\n📍 GEOFENCE:");
    double distance = calculateDistance(
      gps.location.lat(), gps.location.lng(),
      homeFence.centerLat, homeFence.centerLng
    );
    Serial.print("  ├─ Status: ");
    Serial.println(distance <= homeFence.radiusMeters ? "✓ Inside" : "⚠️ Outside");
    Serial.print("  ├─ Distance: ");
    Serial.print(distance, 0);
    Serial.println(" m");
    Serial.print("  └─ Radius: ");
    Serial.print(homeFence.radiusMeters, 0);
    Serial.println(" m");
  }
  
  // Speed Monitoring
  if (gps.speed.isValid()) {
    Serial.println("\n⚡ SPEED:");
    Serial.print("  ├─ Current: ");
    Serial.print(gps.speed.kmph(), 1);
    Serial.println(" km/h");
    Serial.print("  ├─ Limit: ");
    Serial.print(speedLimit, 0);
    Serial.println(" km/h");
    Serial.print("  └─ Status: ");
    if (gps.speed.kmph() > speedLimit) {
      Serial.println("⚠️ OVER LIMIT!");
    } else {
      Serial.println("✓ Normal");
    }
  }
  
  Serial.println("\n════════════════════════════════════════\n");
}

// ===== GPS DIAGNOSTICS FUNCTION =====
void printGPSDiagnostics() {
  Serial.println("\n🛰️ GPS DIAGNOSTICS:");
  Serial.print("  ├─ Characters processed: ");
  Serial.println(gps.charsProcessed());
  Serial.print("  ├─ Sentences with fix: ");
  Serial.println(gps.sentencesWithFix());
  Serial.print("  ├─ Failed checksum: ");
  Serial.println(gps.failedChecksum());
  Serial.print("  ├─ Passed checksum: ");
  Serial.println(gps.passedChecksum());
  
  if (gps.location.isValid()) {
    Serial.print("  ├─ Location: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.println(gps.location.lng(), 6);
    Serial.print("  ├─ Age: ");
    Serial.print(gps.location.age());
    Serial.println(" ms");
  } else {
    Serial.println("  ├─ Location: INVALID");
  }
  
  if (gps.date.isValid()) {
    Serial.print("  ├─ Date: ");
    Serial.print(gps.date.month());
    Serial.print("/");
    Serial.print(gps.date.day());
    Serial.print("/");
    Serial.println(gps.date.year());
  }
  
  if (gps.time.isValid()) {
    Serial.print("  ├─ Time: ");
    Serial.print(gps.time.hour());
    Serial.print(":");
    Serial.print(gps.time.minute());
    Serial.print(":");
    Serial.println(gps.time.second());
  }
  
  Serial.print("  ├─ Satellites: ");
  Serial.println(gps.satellites.value());
  Serial.print("  ├─ HDOP: ");
  Serial.println(gps.hdop.value());
  Serial.print("  └─ Speed: ");
  Serial.print(gps.speed.kmph(), 1);
  Serial.println(" km/h");
  
  // GPS Status Summary
  if (gps.charsProcessed() < 10) {
    Serial.println("  ⚠️ GPS MODULE NOT RESPONDING - Check wiring!");
  } else if (gps.satellites.value() == 0) {
    Serial.println("  ⚠️ NO SATELLITES - Move to window or outdoors");
  } else if (gps.satellites.value() < 4) {
    Serial.println("  ⚠️ INSUFFICIENT SATELLITES - Need 4+ for fix");
  } else {
    Serial.println("  ✓ GPS WORKING PROPERLY");
  }
  
  Serial.println("════════════════════════════════════════");
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK");
    
    // Sync time with NTP servers when WiFi connects
    Serial.println("Syncing time with NTP servers...");
    syncTimeWithNTP();
  }
}

void setupFirebase() {
  Serial.println("Configuring Firebase...");
  
  // Set timeouts to prevent hanging
  config.timeout.serverResponse = 10 * 1000;  // 10 seconds
  config.timeout.socketConnection = 10 * 1000;
  config.timeout.sslHandshake = 10 * 1000;
  
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;
  auth.user.email = FIREBASE_USER_EMAIL;
  auth.user.password = FIREBASE_USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  
  Serial.println("Starting Firebase connection...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Wait a bit for connection
  Serial.print("Waiting for Firebase");
  for (int i = 0; i < 10; i++) {
    Serial.print(".");
    delay(500);
    if (Firebase.ready()) {
      Serial.println(" Connected!");
      return;
    }
  }
  Serial.println(" Timeout!");
  Serial.println("⚠️ Firebase connection failed - continuing in SMS-only mode");
}

void updateLocation() {
  if (!Firebase.ready()) return;
  
  // Only send location if GPS has valid fix
  if (!gps.location.isValid()) {
    Serial.println("⚠️ GPS location not valid, skipping Firebase update");
    return;
  }
  
  // Get current Unix timestamp (seconds since 1970)
  unsigned long currentTime = getCurrentUnixTime();
  
  FirebaseJson json;
  json.set("latitude", gps.location.lat());
  json.set("longitude", gps.location.lng());
  json.set("speed", gps.speed.kmph());
  json.set("altitude", gps.altitude.meters());
  json.set("satellites", gps.satellites.value());
  json.set("hdop", gps.hdop.value());
  json.set("timestamp", currentTime);  // Use Unix timestamp instead of millis()
  json.set("timestampMs", millis());   // Keep millis for debugging
  json.set("valid", true);
  json.set("lastUpdate", getCurrentTimestamp());  // Human-readable timestamp
  
  Serial.println("📍 Sending GPS: Lat=" + String(gps.location.lat(), 6) + 
                 ", Lng=" + String(gps.location.lng(), 6) + 
                 ", Time=" + getCurrentTimestamp());
  
  if (Firebase.RTDB.setJSON(&fbdo, (String("/devices/") + DEVICE_ID + "/location").c_str(), &json)) {
    Serial.println("✓ Location updated to Firebase with proper timestamp");
  } else {
    Serial.print("✗ Firebase location update failed: ");
    Serial.println(fbdo.errorReason());
  }
}

void updateStatus() {
  if (!Firebase.ready()) return;
  
  // Get current Unix timestamp
  unsigned long currentTime = getCurrentUnixTime();
  
  FirebaseJson json;
  json.set("status", "online");
  json.set("engineRunning", engineRunning);
  json.set("systemArmed", systemArmed);
  json.set("timestamp", currentTime);  // Use Unix timestamp
  json.set("timestampMs", millis());   // Keep millis for debugging
  json.set("uptime", millis() / 1000);
  json.set("lastUpdate", getCurrentTimestamp());  // Human-readable timestamp
  json.set("wifiConnected", WiFi.status() == WL_CONNECTED);
  json.set("gpsValid", gps.location.isValid());
  json.set("satellites", gps.satellites.value());
  
  Firebase.RTDB.setJSON(&fbdo, (String("/devices/") + DEVICE_ID + "/status").c_str(), &json);
}



double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
  double R = 6371000;
  double dLat = (lat2 - lat1) * PI / 180.0;
  double dLon = (lon2 - lon1) * PI / 180.0;
  double a = sin(dLat/2) * sin(dLat/2) + cos(lat1 * PI / 180.0) * cos(lat2 * PI / 180.0) * sin(dLon/2) * sin(dLon/2);
  return R * 2 * atan2(sqrt(a), sqrt(1-a));
}

void checkGeofence() {
  if (!homeFence.enabled || !gps.location.isValid()) return;
  
  double distance = calculateDistance(gps.location.lat(), gps.location.lng(), homeFence.centerLat, homeFence.centerLng);
  bool isInside = distance <= homeFence.radiusMeters;
  
  if (wasInsideFence && !isInside) {
    sendNotification("Geofence Alert", "Vehicle left Home Zone!");
    sendSMS("ALERT: Vehicle left Home Zone - " + String(distance, 0) + "m away!");
  }
  
  wasInsideFence = isInside;
  
  if (!Firebase.ready()) return;
  
  // Send enhanced geofence data with proper names and validation
  FirebaseJson json;
  json.set("distance", distance);
  json.set("inside", isInside);
  json.set("fence", "Home Zone");  // Add fence name
  json.set("name", "Home Zone");   // Alternative name field
  json.set("radius", homeFence.radiusMeters);
  json.set("centerLat", homeFence.centerLat);
  json.set("centerLng", homeFence.centerLng);
  json.set("timestamp", getCurrentUnixTime());
  json.set("lastUpdate", getCurrentTimestamp());
  
  Serial.println("📍 Geofence: " + String(isInside ? "Inside" : "Outside") + 
                 " Home Zone (" + String(distance, 0) + "m)");
  
  Firebase.RTDB.setJSON(&fbdo, (String("/devices/") + DEVICE_ID + "/geofence").c_str(), &json);
}

void checkCommands() {
  if (!Firebase.ready()) return;
  
  // SMS and Dashboard commands work together seamlessly
  // No priority system needed - both should work equally
  
  // Add Firebase command cooldown to prevent spam and loops
  static unsigned long lastFirebaseCommand = 0;
  if (millis() - lastFirebaseCommand < 5000) {
    Serial.println("🔒 Firebase command cooldown active (" + String((5000 - (millis() - lastFirebaseCommand))/1000) + "s remaining)");
    return; // Prevent Firebase command spam
  }
  
  if (Firebase.RTDB.getString(&fbdo, (String("/devices/") + DEVICE_ID + "/commands/pending").c_str())) {
    String cmd = fbdo.stringData();
    if (cmd.length() > 0 && cmd != "null" && cmd != "" && cmd != "\"\"") {
      cmd.toUpperCase();
      Serial.println("🔍 Firebase command received: '" + cmd + "' (length: " + String(cmd.length()) + ")");
      Serial.println("🔍 Command source: Firebase Dashboard or API");
      
      // CRITICAL: Clear the command IMMEDIATELY to prevent loops
      Firebase.RTDB.setString(&fbdo, (String("/devices/") + DEVICE_ID + "/commands/pending").c_str(), "");
      
      // Update command timestamp to prevent spam
      lastFirebaseCommand = millis();
      
      // Process Firebase command (dashboard responsiveness)
      
      if (cmd == "START") {
        Serial.println("�  Firebase COMMAND: Starting engine...");
        startEngine();
      } else if (cmd == "STOP") {
        Serial.println("🛑 Firebase COMMAND: Stopping engine...");
        stopEngine();
      } else if (cmd == "ARM") { 
        systemArmed = true; 
        updateStatus(); 
      } else if (cmd == "DISARM") { 
        systemArmed = false; 
        updateStatus(); 
      } else if (cmd == "LOCATE") {
        updateLocation();
      } else if (cmd == "REBOOT") {
        Serial.println("🔄 REBOOT command received - restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
      }
      
      // Command already cleared at the beginning to prevent loops
    }
  }
}

void startEngine() {
  // Safety check: don't start if already running
  if (engineRunning) {
    Serial.println("⚠️ Engine already running - command ignored");
    return;
  }
  
  // Safety check: lockout after too many attempts
  if (starterAttempts >= MAX_STARTER_ATTEMPTS) {
    if (millis() - starterLockoutTime < STARTER_LOCKOUT_DURATION) {
      unsigned long remaining = (STARTER_LOCKOUT_DURATION - (millis() - starterLockoutTime)) / 1000;
      Serial.print("⚠️ STARTER LOCKED OUT - Wait ");
      Serial.print(remaining);
      Serial.println(" seconds");
      sendNotification("Start Failed", "Too many attempts. Locked for 5 minutes.");
      return;
    } else {
      // Lockout expired, reset counter
      starterAttempts = 0;
      Serial.println("✓ Starter lockout expired");
    }
  }
  
  // Safety check: cooldown period
  if (millis() - lastStarterAttempt < STARTER_COOLDOWN) {
    Serial.println("⚠️ Starter cooldown active, please wait");
    sendNotification("Start Failed", "Please wait 10 seconds between attempts");
    return;
  }
  
  Serial.println("🚗 Starting engine sequence...");
  
  // Increment attempt counter
  starterAttempts++;
  if (starterAttempts == 1) {
    starterLockoutTime = millis();  // Start lockout timer
  }
  
  // ANTI-THEFT SEQUENCE:
  // Step 1: Enable ignition coil (CRITICAL - allows engine to start physically)
  Serial.println("🔓 ANTI-THEFT: Enabling ignition coil - engine can now start");
  setIgnitionCoilRelay(true);
  delay(500); // Wait for ignition coil to activate
  
  // Step 2: Enable ignition switch (allows remote starting)
  setIgnitionSwitchRelay(true);
  starterActive = true;
  starterStartTime = millis();
  lastStarterAttempt = millis();
  
  setLED(true);  // LED ON when engine running
  engineRunning = true;
  
  // Save state to persistent storage
  prefs.putBool("engineRun", true);
  
  Serial.println("✓ Starter engaged");
  Serial.print("  Attempt ");
  Serial.print(starterAttempts);
  Serial.print(" of ");
  Serial.println(MAX_STARTER_ATTEMPTS);
  
  updateStatus();
  sendNotification("Engine Control", "Starting engine...");
  
  // Send SMS notification for dashboard commands (with cooldown)
  if (millis() - lastEngineStatusSMS > 60000) { // 1 minute cooldown
    sendSMS("ENGINE STARTED via Dashboard - Anti-theft disabled", AUTHORIZED_NUMBER_1);
    lastEngineStatusSMS = millis();
  }
}

void stopEngine() {
  Serial.println("🛑 STOP ENGINE CALLED!");
  Serial.println("🔍 Current engine state: " + String(engineRunning ? "RUNNING" : "STOPPED"));
  
  // Safety check: don't stop if already stopped
  if (!engineRunning) {
    Serial.println("⚠️ Engine already stopped - command ignored");
    return;
  }
  
  // Execute STOP for anti-theft protection
  Serial.println("🛑 Executing ANTI-THEFT engine stop...");
  
  // ANTI-THEFT SEQUENCE:
  // Step 1: Disable ignition switch (stops remote starting)
  setIgnitionSwitchRelay(false);
  
  // Step 2: Cut ignition coil (CRITICAL - prevents physical starting)
  Serial.println("🔒 ANTI-THEFT: Disabling ignition coil - engine CANNOT start even with physical key");
  setIgnitionCoilRelay(false);
  
  starterActive = false;
  setLED(false);  // LED OFF when engine stopped
  engineRunning = false;
  
  // Reset starter attempt counter on successful stop
  starterAttempts = 0;
  
  // Save state to persistent storage
  prefs.putBool("engineRun", false);
  
  // CRITICAL: Clear any pending Firebase START commands to prevent auto-restart
  if (Firebase.ready()) {
    Firebase.RTDB.setString(&fbdo, (String("/devices/") + DEVICE_ID + "/commands/pending").c_str(), "");
    Serial.println("🔒 Cleared pending Firebase commands to prevent auto-restart");
  }
  
  updateStatus();
  sendNotification("Engine Control", "Engine stopped");
  Serial.println("✓ Engine stopped - ANTI-THEFT ACTIVE");
  
  // Send SMS notification for dashboard commands (with cooldown)
  if (millis() - lastEngineStatusSMS > 60000) { // 1 minute cooldown
    sendSMS("ENGINE STOPPED via Dashboard - Anti-theft activated", AUTHORIZED_NUMBER_1);
    lastEngineStatusSMS = millis();
  }
}

// Check starter timeout in main loop
void checkStarterTimeout() {
  if (starterActive && (millis() - starterStartTime >= STARTER_TIMEOUT)) {
    // After timeout, ignition coil stays ON to keep engine running
    // Only ignition switch relay can be turned off for security
    starterActive = false;
    Serial.println("✓ Engine start sequence completed (timeout)");
    Serial.println("✓ Engine should now be running");
    
    // Both relays stay ON to keep engine running
    // Only STOP command will turn off both relays
  }
}

void handleAlert() {
  unsigned long currentTime = millis();
  
  Serial.println("🔍 Movement detected!");
  Serial.println("   Engine Running: " + String(engineRunning ? "YES" : "NO"));
  Serial.println("   System Armed: " + String(systemArmed ? "YES" : "NO"));
  
  // Only process alerts when engine is stopped and system is armed
  if (engineRunning || !systemArmed) {
    Serial.println("⚠️ Alert ignored - engine running or system disarmed");
    return;
  }
  
  // Check if we're in alert cooldown period
  if (alertSent && (currentTime - lastMovementAlert < MOVEMENT_ALERT_COOLDOWN)) {
    Serial.println("⚠️ Movement detected but alert cooldown active (" + 
                   String((MOVEMENT_ALERT_COOLDOWN - (currentTime - lastMovementAlert)) / 1000) + "s remaining)");
    return;
  }
  
  // Reset movement counting if detection window expired
  if (currentTime - firstMovementTime > MOVEMENT_DETECTION_WINDOW) {
    Serial.println("🔄 Movement detection window expired - resetting counter");
    movementCount = 0;
    firstMovementTime = currentTime;
    alertSent = false;
  }
  
  // If this is the first movement in a new window, record the time
  if (movementCount == 0) {
    firstMovementTime = currentTime;
  }
  
  // Increment movement count
  movementCount++;
  Serial.println("📊 Movement count: " + String(movementCount) + "/" + String(MOVEMENT_THRESHOLD) + 
                 " (within " + String(MOVEMENT_DETECTION_WINDOW/1000) + "s window)");
  
  // Visual and audio alert (non-blocking)
  setLED(true);
  tone(BUZZER_PIN, 1500, 100); // Very short beep
  delay(100);
  setLED(false);
  noTone(BUZZER_PIN);
  esp_task_wdt_reset(); // Reset watchdog after buzzer
  
  // Send SMS alert if threshold reached and not already sent
  if (movementCount >= MOVEMENT_THRESHOLD && !alertSent) {
    Serial.println("🚨 MOVEMENT THRESHOLD REACHED - SENDING ALERT!");
    
    // Send notifications to authorized numbers only (with SMS spam prevention)
    sendNotification("Security Alert", "Unauthorized movement detected!");
    
    // Check if GSM is healthy before sending SMS
    if (gsmHealthy) {
      String alertMessage = "ALERT! Movement detected " + String(movementCount) + " times in " + 
                           String(MOVEMENT_DETECTION_WINDOW/1000) + " seconds. Possible theft attempt!";
      
      sendSMS(alertMessage, AUTHORIZED_NUMBER_1);
      delay(5000); // 5 second delay between SMS sends
      esp_task_wdt_reset(); // Reset watchdog during delay
      sendSMS(alertMessage, AUTHORIZED_NUMBER_2);
    } else {
      Serial.println("GSM not healthy - skipping SMS alert");
    }
    
    // Update alert tracking
    lastMovementAlert = currentTime;
    alertSent = true;
    
    Serial.println("🚨 Movement alert sent to authorized numbers");
    Serial.println("🚨 Next alert available in " + String(MOVEMENT_ALERT_COOLDOWN/1000) + " seconds");
  } else if (movementCount < MOVEMENT_THRESHOLD) {
    Serial.println("⚠️ Movement detected but threshold not reached yet (" + 
                   String(MOVEMENT_THRESHOLD - movementCount) + " more needed)");
  } else if (alertSent) {
    Serial.println("⚠️ Movement detected but alert already sent for this detection window");
  }
}

// CONTROLLED THEFT ALERT - Non-blocking buzzer with watchdog protection
void handleImmediateTheftAlert() {
  Serial.println("=== CONTROLLED THEFT ALERT ===");
  Serial.println("Movement detected - executing controlled alert");
  Serial.println("Engine: " + String(engineRunning ? "RUNNING" : "STOPPED"));
  Serial.println("System: " + String(systemArmed ? "ARMED" : "DISARMED"));
  
  // Only process if system is armed and engine is stopped
  if (engineRunning || !systemArmed) {
    Serial.println("Alert ignored - engine running or system disarmed");
    return;
  }
  
  // CONTROLLED 3-BUZZ ALERT (non-blocking with watchdog protection)
  Serial.println("TRIGGERING CONTROLLED ALERT!");
  
  for (int i = 1; i <= 3; i++) {
    Serial.println("Buzz " + String(i) + "/3");
    setLED(true);
    tone(BUZZER_PIN, 1500, 100);  // Shorter duration
    delay(100);
    setLED(false);
    noTone(BUZZER_PIN);
    delay(50);  // Very short gap
    esp_task_wdt_reset(); // Reset watchdog during buzzing
  }
  
  Serial.println("CONTROLLED ALERT COMPLETED!");
  
  // Use standard alert system (with SMS cooldown)
  handleAlert();
}

// IMMEDIATE SMS ALERT - Send SMS on ANY movement (no delays, no thresholds)
void sendImmediateSMSAlert() {
  static unsigned long lastImmediateSMS = 0;
  
  // Prevent SMS spam - minimum 2 minutes between immediate alerts
  if (millis() - lastImmediateSMS < 120000) {
    Serial.println("Immediate SMS alert blocked - cooldown active (" + 
                   String((120000 - (millis() - lastImmediateSMS)) / 1000) + "s remaining)");
    return;
  }
  
  // Check if GSM is healthy before sending SMS
  if (!gsmHealthy) {
    Serial.println("GSM not healthy - skipping SMS alert");
    return;
  }
  
  Serial.println("SENDING IMMEDIATE THEFT ALERT SMS!");
  
  // Send to both authorized numbers with delay between sends
  sendSMS("ALERT! UNAUTHORIZED MOVEMENT DETECTED - Motorcycle security breach!", AUTHORIZED_NUMBER_1);
  delay(5000); // 5 second delay between SMS sends
  esp_task_wdt_reset(); // Reset watchdog during delay
  sendSMS("ALERT! UNAUTHORIZED MOVEMENT DETECTED - Motorcycle security breach!", AUTHORIZED_NUMBER_2);
  
  lastImmediateSMS = millis();
  Serial.println("IMMEDIATE SMS ALERTS SENT TO BOTH NUMBERS!");
}

// ENGINE STATUS SMS NOTIFICATIONS - DISABLED TO PREVENT SPAM
void sendEngineStatusSMS(String status) {
  Serial.println("ENGINE STATUS SMS DISABLED - preventing spam");
  Serial.println("Status change: " + status + " (SMS notification disabled)");
  
  // DISABLED: Automatic engine status SMS causes spam
  // Only send SMS when explicitly requested by user commands
  // User can check status with "1234 STATUS" command
}

void sendNotification(String title, String body) {
  if (!Firebase.ready()) return;
  FirebaseJson json;
  json.set("title", title);
  json.set("body", body);
  json.set("timestamp", millis());
  Firebase.RTDB.setJSON(&fbdo, (String("/devices/") + DEVICE_ID + "/notifications/" + String(millis())).c_str(), &json);
}

// GSM Functions - ENHANCED FOR SMS COMMANDS
bool initializeGSM() {
  Serial.println("🔧 Initializing GSM module for SMS commands...");
  
  // Reset watchdog before GSM initialization
  esp_task_wdt_reset();
  
  // Step 1: Check if module responds
  Serial.println("🔧 Step 1: Testing GSM module response...");
  gsmSerial.println("AT");
  delay(1000);
  esp_task_wdt_reset(); // Reset watchdog after delay
  
  if (!waitForGSMResponse("OK", 3000)) {
    Serial.println("❌ GSM module not responding");
    return false;
  }
  Serial.println("✓ GSM module responding");
  
  // Step 2: Set SMS text mode
  Serial.println("🔧 Step 2: Setting SMS text mode...");
  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  esp_task_wdt_reset(); // Reset watchdog after delay
  
  if (!waitForGSMResponse("OK", 3000)) {
    Serial.println("❌ Failed to set SMS text mode");
    return false;
  }
  Serial.println("✓ SMS text mode enabled");
  
  // Step 3: Configure SMS notifications (multiple modes for better compatibility)
  Serial.println("🔧 Step 3: Configuring SMS notifications...");
  esp_task_wdt_reset(); // Reset watchdog before SMS configuration
  
  // Mode 1: Store and notify
  gsmSerial.println("AT+CNMI=2,1,0,0,0");
  delay(1000);
  esp_task_wdt_reset();
  if (waitForGSMResponse("OK", 3000)) {
    Serial.println("✓ SMS notification mode 1 set (store and notify)");
  }
  
  // Mode 2: Also try immediate forwarding
  gsmSerial.println("AT+CNMI=1,2,0,0,0");
  delay(1000);
  esp_task_wdt_reset();
  if (waitForGSMResponse("OK", 3000)) {
    Serial.println("✓ SMS notification mode 2 set (immediate forward)");
  }
  
  Serial.println("✓ SMS notifications configured with multiple modes");
  
  // Step 4: Delete all SMS to clear memory
  Serial.println("🔧 Step 4: Clearing SMS memory...");
  gsmSerial.println("AT+CMGDA=\"DEL ALL\"");
  delay(2000);
  esp_task_wdt_reset(); // Reset watchdog after long delay
  Serial.println("✓ SMS memory cleared");
  
  // Step 5: Check network registration
  gsmSerial.println("AT+CREG?");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    if (response.indexOf(",1") != -1 || response.indexOf(",5") != -1) {
      Serial.println("✓ GSM network registered");
    } else {
      Serial.println("⚠️ GSM network not registered - SMS may not work");
    }
  }
  
  // Step 6: Check signal strength
  gsmSerial.println("AT+CSQ");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    Serial.println("📶 Signal strength: " + response);
  }
  
  Serial.println("✅ GSM initialization complete!");
  Serial.println("📱 Ready to receive SMS commands:");
  Serial.println("   • Send: '1234 START' to start engine");
  Serial.println("   • Send: '1234 STOP' to stop engine");  
  Serial.println("   • Send: '1234 LOCATE' for GPS location");
  Serial.println("   • Send: '1234 STATUS' for system status");
  Serial.println("   • Send: '1234 RESET' to restart system");
  
  // Test SMS functionality
  delay(2000);
  testSMSFunctionality();
  
  // Configure SMS to be stored and forwarded
  Serial.println("🔧 Configuring SMS reception...");
  
  // Set SMS storage to SIM card
  gsmSerial.println("AT+CPMS=\"SM\",\"SM\",\"SM\"");
  delay(1000);
  if (waitForGSMResponse("OK", 3000)) {
    Serial.println("✅ SMS storage set to SIM card");
  }
  
  // Try multiple SMS notification modes
  Serial.println("🔧 Setting SMS notification mode 1...");
  gsmSerial.println("AT+CNMI=1,2,0,0,0"); // Direct forwarding
  delay(1000);
  
  Serial.println("🔧 Setting SMS notification mode 2...");
  gsmSerial.println("AT+CNMI=2,1,0,0,0"); // Store and notify
  delay(1000);
  
  Serial.println("🔧 Setting SMS notification mode 3...");
  gsmSerial.println("AT+CNMI=2,2,0,0,0"); // Store and forward
  delay(1000);
  
  Serial.println("✅ SMS reception configured with multiple modes");
  Serial.println("📱 Send '1234 STATUS' to test SMS commands");
  
  return true;
}

// Wait for GSM response with timeout - IMPROVED WITH WATCHDOG PROTECTION
bool waitForGSMResponse(String expectedResponse, unsigned long timeout) {
  unsigned long startTime = millis();
  String response = "";
  response.reserve(100); // Reserve memory to prevent fragmentation
  
  while (millis() - startTime < timeout) {
    // Reset watchdog every 1 second during wait
    if ((millis() - startTime) % 1000 == 0) {
      esp_task_wdt_reset();
    }
    
    if (gsmSerial.available()) {
      char c = (char)gsmSerial.read();
      if (response.length() < 80) { // Prevent memory overflow
        response += c;
      }
      
      if (response.indexOf(expectedResponse) != -1) {
        return true;
      }
    }
    delay(10);
  }
  return false;
}

// GSM Health Check - Monitor GSM module and reinitialize if needed
void performGSMHealthCheck() {
  Serial.println("🔍 Performing GSM health check...");
  
  // Clear any pending data first
  while (gsmSerial.available()) {
    gsmSerial.read();
  }
  
  // Test basic AT command with timeout
  gsmSerial.println("AT");
  delay(2000); // Longer delay for response
  esp_task_wdt_reset(); // Reset watchdog after delay
  
  String response = "";
  unsigned long startTime = millis();
  while (gsmSerial.available() && (millis() - startTime < 3000)) {
    response += (char)gsmSerial.read();
    delay(10);
  }
  
  Serial.println("GSM Response: '" + response + "'");
  
  if (response.indexOf("OK") != -1) {
    if (!gsmHealthy) {
      Serial.println("✅ GSM module healthy - SMS commands now available");
      gsmHealthy = true;
    }
  } else {
    Serial.println("❌ GSM module not responding - attempting recovery...");
    gsmHealthy = false;
    
    // Try hardware reset approach
    Serial.println("🔄 Attempting GSM module recovery...");
    
    // Send multiple AT commands to wake up module
    for (int i = 0; i < 3; i++) {
      gsmSerial.println("AT");
      delay(1000);
      esp_task_wdt_reset();
    }
    
    // Clear buffer and test again
    while (gsmSerial.available()) {
      gsmSerial.read();
    }
    
    gsmSerial.println("AT");
    delay(2000);
    esp_task_wdt_reset();
    
    response = "";
    startTime = millis();
    while (gsmSerial.available() && (millis() - startTime < 3000)) {
      response += (char)gsmSerial.read();
      delay(10);
    }
    
    if (response.indexOf("OK") != -1) {
      Serial.println("✅ GSM module recovered - reinitializing...");
      if (initializeGSM()) {
        Serial.println("✅ GSM module reinitialized successfully");
        gsmHealthy = true;
      } else {
        Serial.println("❌ GSM reinitialization failed");
      }
    } else {
      Serial.println("❌ GSM module recovery failed - check hardware connection");
      Serial.println("💡 Try: 1) Check GSM module power, 2) Check RX/TX wiring, 3) Check antenna");
    }
  }
}

// Check for stored SMS messages manually - IMPROVED FOR POST-REBOOT RELIABILITY
void checkStoredSMS() {
  Serial.println("📱 Checking for stored SMS messages...");
  
  // Reset watchdog before SMS operations
  esp_task_wdt_reset();
  
  // Clear any pending data first
  while (gsmSerial.available()) {
    gsmSerial.read();
  }
  
  // First, check if GSM module is responding
  gsmSerial.println("AT");
  delay(500);
  String testResponse = "";
  while (gsmSerial.available()) {
    testResponse += (char)gsmSerial.read();
  }
  
  if (testResponse.indexOf("OK") == -1) {
    Serial.println("❌ GSM module not responding - reinitializing...");
    initializeGSM();
    return;
  }
  
  // Try different SMS reading approaches for better compatibility
  Serial.println("🔍 Checking unread SMS messages...");
  
  // Method 1: Try reading unread messages
  gsmSerial.println("AT+CMGL=\"REC UNREAD\"");
  delay(3000);
  esp_task_wdt_reset();
  
  String response = "";
  unsigned long startTime = millis();
  while (gsmSerial.available() && (millis() - startTime < 5000)) {
    response += (char)gsmSerial.read();
    delay(10);
  }
  
  Serial.println("📱 Unread SMS response: " + response);
  
  // If we got an error, try reading by index
  if (response.indexOf("ERROR") != -1) {
    Serial.println("⚠️ CMGL command failed, trying individual SMS reading...");
    
    // Try reading SMS by index (1-20)
    for (int i = 1; i <= 20; i++) {
      gsmSerial.println("AT+CMGR=" + String(i));
      delay(1000);
      esp_task_wdt_reset();
      
      String smsResponse = "";
      unsigned long smsStartTime = millis();
      while (gsmSerial.available() && (millis() - smsStartTime < 2000)) {
        smsResponse += (char)gsmSerial.read();
        delay(10);
      }
      
      if (smsResponse.indexOf("+CMGR:") != -1) {
        Serial.println("📨 Found SMS at index " + String(i) + ": " + smsResponse);
        handleStoredSMS(smsResponse);
        
        // Delete this message
        gsmSerial.println("AT+CMGD=" + String(i));
        delay(500);
      }
    }
    return;
  }
  
  if (response.length() > 10 && response.indexOf("+CMGL:") != -1) {
    Serial.println("📨 Unread SMS Messages found:");
    Serial.println(response);
    handleStoredSMS(response);
    
    // Delete processed messages to free memory
    gsmSerial.println("AT+CMGD=1,4"); // Delete all read messages
    delay(1000);
    esp_task_wdt_reset();
    Serial.println("🗑️ Processed SMS messages deleted");
    return;
  }
  
  // If no unread messages, check all messages
  Serial.println("🔍 Checking all SMS messages...");
  gsmSerial.println("AT+CMGL=\"ALL\"");
  delay(3000); // Longer delay
  
  response = "";
  startTime = millis();
  while (gsmSerial.available() && (millis() - startTime < 5000)) { // Longer timeout
    response += (char)gsmSerial.read();
    delay(10);
  }
  
  Serial.println("� All S MS response: " + response); // Debug output
  
  if (response.length() > 10 && response.indexOf("+CMGL:") != -1) {
    Serial.println("📨 All SMS Messages found:");
    Serial.println(response);
    handleStoredSMS(response);
    
    // Delete processed messages
    gsmSerial.println("AT+CMGD=1,4");
    delay(1000);
    Serial.println("🗑️ All SMS messages deleted");
  } else {
    Serial.println("📭 No SMS messages found - checking GSM status...");
    
    // Additional debugging - check GSM status when no messages found
    gsmSerial.println("AT+CREG?");
    delay(1000);
    String regStatus = "";
    while (gsmSerial.available()) {
      regStatus += (char)gsmSerial.read();
    }
    Serial.println("📶 Network registration: " + regStatus);
    
    gsmSerial.println("AT+CSQ");
    delay(1000);
    String signalStatus = "";
    while (gsmSerial.available()) {
      signalStatus += (char)gsmSerial.read();
    }
    Serial.println("📶 Signal strength: " + signalStatus);
  }
}

// Test SMS functionality - call this to diagnose issues
void testSMSFunctionality() {
  Serial.println("🧪 TESTING SMS FUNCTIONALITY...");
  Serial.println("================================");
  
  // Test 1: Basic AT command
  Serial.println("Test 1: GSM Module Response");
  gsmSerial.println("AT");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    Serial.println("✅ GSM responds: " + response);
  } else {
    Serial.println("❌ GSM not responding");
    return;
  }
  
  // Test 2: SIM card status
  Serial.println("Test 2: SIM Card Status");
  gsmSerial.println("AT+CPIN?");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    Serial.println("📱 SIM status: " + response);
    if (response.indexOf("READY") == -1) {
      Serial.println("❌ SIM card not ready - check if inserted and not PIN locked");
    }
  }
  
  // Test 3: Network registration
  Serial.println("Test 3: Network Registration");
  gsmSerial.println("AT+CREG?");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    Serial.println("📶 Network: " + response);
    if (response.indexOf(",1") != -1 || response.indexOf(",5") != -1) {
      Serial.println("✅ Registered to network");
    } else {
      Serial.println("❌ Not registered - check SIM card and signal");
    }
  }
  
  // Test 4: Signal strength
  Serial.println("Test 4: Signal Strength");
  gsmSerial.println("AT+CSQ");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    Serial.println("📶 Signal: " + response);
  }
  
  // Test 5: SMS mode
  Serial.println("Test 5: SMS Text Mode");
  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    if (response.indexOf("OK") != -1) {
      Serial.println("✅ SMS text mode set");
    } else {
      Serial.println("❌ Failed to set SMS mode: " + response);
    }
  }
  
  // Test 6: SMS prompt test
  Serial.println("Test 6: SMS Prompt Test");
  Serial.println("Sending test SMS command...");
  gsmSerial.print("AT+CMGS=\"" + String(AUTHORIZED_NUMBER_1) + "\"\r\n");
  delay(3000);
  
  String promptResponse = "";
  while (gsmSerial.available()) {
    promptResponse += (char)gsmSerial.read();
  }
  
  if (promptResponse.indexOf(">") != -1) {
    Serial.println("✅ SMS prompt received!");
    gsmSerial.write(27); // ESC to cancel
    delay(1000);
  } else {
    Serial.println("❌ No SMS prompt. Response: " + promptResponse);
  }
  
  Serial.println("================================");
  Serial.println("🧪 SMS TEST COMPLETE");
  
  // Test SMS functionality without sending actual SMS to prevent reboots
  if (promptResponse.indexOf(">") != -1) {
    Serial.println("✅ SMS prompt test successful");
    Serial.println("📱 System ready to receive SMS commands");
    Serial.println("🔒 Automatic test SMS disabled to prevent system instability");
    
    // Send system ready notification only when GSM is stable
    if (gsmHealthy) {
      Serial.println("📱 GSM STABLE - Ready for SMS commands");
      // Optional: Send a single notification that system is ready (user controlled)
      // sendSMS("Anti-Theft System Online - Ready for commands", AUTHORIZED_NUMBER_1);
    }
  }
}

// Enable GPRS for data connection
bool enableGPRS() {
  Serial.println("Enabling GPRS...");
  
  // Set APN (change according to your SIM provider)
  gsmSerial.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  delay(2000);
  gsmSerial.println("AT+SAPBR=3,1,\"APN\",\"internet\""); // Change to your APN
  delay(2000);
  
  // Open GPRS context
  gsmSerial.println("AT+SAPBR=1,1");
  delay(5000);
  
  // Check connection
  gsmSerial.println("AT+SAPBR=2,1");
  delay(2000);
  
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    if (response.indexOf("1,1") != -1) {
      Serial.println("GPRS connected!");
      gsmConnected = true;
      return true;
    }
  }
  
  Serial.println("GPRS connection failed");
  gsmConnected = false;
  return false;
}

// Send data via GSM HTTP when WiFi is down
bool sendDataViaGSM(String jsonData) {
  if (!gsmConnected) {
    Serial.println("❌ GSM not connected, cannot send data");
    return false;
  }
  
  Serial.println("📡 Sending data via GSM...");
  
  // Initialize HTTP service
  gsmSerial.println("AT+HTTPINIT");
  delay(2000);
  
  // Set HTTP parameters
  gsmSerial.println("AT+HTTPPARA=\"CID\",1");
  delay(1000);
  
  // Set Firebase URL (you may need to adjust this)
  String url = "https://" + String(FIREBASE_DATABASE_URL) + "/devices/" + DEVICE_ID + "/location.json?auth=" + String(FIREBASE_API_KEY);
  gsmSerial.println("AT+HTTPPARA=\"URL\",\"" + url + "\"");
  delay(1000);
  
  // Set content type
  gsmSerial.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(1000);
  
  // Set data to send
  gsmSerial.println("AT+HTTPDATA=" + String(jsonData.length()) + ",10000");
  delay(1000);
  
  if (gsmSerial.find(">")) {
    gsmSerial.print(jsonData);
    delay(2000);
    
    // Send HTTP POST request
    gsmSerial.println("AT+HTTPACTION=1");
    delay(5000);
    
    // Check response
    if (gsmSerial.available()) {
      String response = gsmSerial.readString();
      if (response.indexOf("200") != -1) {
        Serial.println("✅ Data sent via GSM successfully");
        gsmSerial.println("AT+HTTPTERM");
        delay(1000);
        return true;
      }
    }
  }
  
  Serial.println("❌ Failed to send data via GSM");
  gsmSerial.println("AT+HTTPTERM");
  delay(1000);
  return false;
}

// Check GSM connection status
void checkGSMConnection() {
  gsmSerial.println("AT+SAPBR=2,1");
  delay(1000);
  
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    gsmConnected = (response.indexOf("1,1") != -1);
    
    if (!gsmConnected) {
      Serial.println("GSM disconnected, reconnecting...");
      enableGPRS();
    }
  }
}

// Send location via GSM HTTP POST
void sendLocationViaGSM() {
  if (!gsmConnected || !gps.location.isValid()) return;
  
  Serial.println("Sending location via GSM...");
  
  // Initialize HTTP
  gsmSerial.println("AT+HTTPINIT");
  delay(2000);
  
  // Set HTTP parameters
  gsmSerial.println("AT+HTTPPARA=\"CID\",1");
  delay(1000);
  
  // Set Firebase URL (you'll need to use Firebase REST API)
  String url = "AT+HTTPPARA=\"URL\",\"" + String(FIREBASE_DATABASE_URL) + 
               "/devices/" + String(DEVICE_ID) + "/location.json?auth=" + 
               String(FIREBASE_API_KEY) + "\"";
  gsmSerial.println(url);
  delay(1000);
  
  // Set content type
  gsmSerial.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(1000);
  
  // Prepare JSON data
  String jsonData = "{\"latitude\":" + String(gps.location.lat(), 6) + 
                    ",\"longitude\":" + String(gps.location.lng(), 6) + 
                    ",\"speed\":" + String(gps.speed.kmph(), 2) + 
                    ",\"satellites\":" + String(gps.satellites.value()) + 
                    ",\"timestamp\":" + String(millis()) + 
                    ",\"connection\":\"GSM\"}";
  
  // Set data length
  gsmSerial.println("AT+HTTPDATA=" + String(jsonData.length()) + ",10000");
  delay(2000);
  
  // Send data
  gsmSerial.println(jsonData);
  delay(2000);
  
  // Execute POST
  gsmSerial.println("AT+HTTPACTION=1"); // 1 = POST
  delay(5000);
  
  // Terminate HTTP
  gsmSerial.println("AT+HTTPTERM");
  delay(1000);
  
  Serial.println("Location sent via GSM");
}

// Send status via GSM
void sendStatusViaGSM() {
  if (!gsmConnected) return;
  
  Serial.println("Sending status via GSM...");
  
  // Similar to sendLocationViaGSM but for status
  gsmSerial.println("AT+HTTPINIT");
  delay(2000);
  
  gsmSerial.println("AT+HTTPPARA=\"CID\",1");
  delay(1000);
  
  String url = "AT+HTTPPARA=\"URL\",\"" + String(FIREBASE_DATABASE_URL) + 
               "/devices/" + String(DEVICE_ID) + "/status.json?auth=" + 
               String(FIREBASE_API_KEY) + "\"";
  gsmSerial.println(url);
  delay(1000);
  
  gsmSerial.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(1000);
  
  String jsonData = "{\"status\":\"online\",\"engineRunning\":" + 
                    String(engineRunning ? "true" : "false") + 
                    ",\"systemArmed\":" + String(systemArmed ? "true" : "false") + 
                    ",\"timestamp\":" + String(millis()) + 
                    ",\"uptime\":" + String(millis() / 1000) + 
                    ",\"connection\":\"GSM\"}";
  
  gsmSerial.println("AT+HTTPDATA=" + String(jsonData.length()) + ",10000");
  delay(2000);
  
  gsmSerial.println(jsonData);
  delay(2000);
  
  gsmSerial.println("AT+HTTPACTION=1");
  delay(5000);
  
  gsmSerial.println("AT+HTTPTERM");
  delay(1000);
  
  Serial.println("Status sent via GSM");
}

// Send SMS to default authorized number
void sendSMS(String message) {
  sendSMS(message, AUTHORIZED_NUMBER_1);
}

// Send SMS with spam prevention
void sendSMSWithCooldown(String message, String phoneNumber) {
  // Check cooldown to prevent SMS spam
  if (millis() - lastSMSSent < SMS_COOLDOWN) {
    Serial.println("⚠️ SMS cooldown active - message not sent: " + message);
    return;
  }
  
  sendSMS(message, phoneNumber);
  lastSMSSent = millis();
}

// Send SMS to specific number - ENHANCED WITH DEBUGGING
void sendSMS(String message, String phoneNumber) {
  Serial.println("📤 Attempting to send SMS to " + phoneNumber + ": " + message);
  
  // Reset watchdog before SMS operations
  esp_task_wdt_reset();
  
  // Step 1: Check if GSM module is responding
  gsmSerial.println("AT");
  delay(500);
  esp_task_wdt_reset(); // Reset after delay
  
  if (!gsmSerial.available()) {
    Serial.println("❌ GSM module not responding");
    return;
  }
  
  // Clear any pending data
  while (gsmSerial.available()) {
    gsmSerial.read();
  }
  
  // Step 2: Check network registration
  gsmSerial.println("AT+CREG?");
  delay(1000);
  esp_task_wdt_reset(); // Reset after delay
  
  String regResponse = "";
  unsigned long startTime = millis();
  while (gsmSerial.available() && (millis() - startTime < 2000)) {
    regResponse += (char)gsmSerial.read();
    delay(10);
  }
  Serial.println("📶 Network status: " + regResponse);
  
  if (regResponse.indexOf(",1") == -1 && regResponse.indexOf(",5") == -1) {
    Serial.println("❌ Not registered to network - SMS will fail");
    return;
  }
  
  // Step 3: Set SMS text mode (ensure it's set)
  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  esp_task_wdt_reset(); // Reset after delay
  
  if (!waitForGSMResponse("OK", 3000)) {
    Serial.println("❌ Failed to set SMS text mode");
    return;
  }
  
  // Step 4: Send SMS command
  Serial.println("📱 Sending SMS command...");
  gsmSerial.print("AT+CMGS=\"" + phoneNumber + "\"\r\n");
  delay(2000);
  esp_task_wdt_reset(); // Reset after delay
  
  // Step 5: Wait for prompt with timeout
  unsigned long promptStartTime = millis();
  bool promptReceived = false;
  String response = "";
  response.reserve(100); // Reserve memory to prevent fragmentation
  
  while (millis() - promptStartTime < 10000) { // 10 second timeout
    // Reset watchdog every 2 seconds during wait
    if ((millis() - promptStartTime) % 2000 == 0) {
      esp_task_wdt_reset();
    }
    
    if (gsmSerial.available()) {
      char c = gsmSerial.read();
      if (response.length() < 50) { // Prevent memory overflow
        response += c;
      }
      Serial.print(c); // Debug: show what we're receiving
      
      if (c == '>') {
        promptReceived = true;
        break;
      }
    }
    delay(10);
  }
  
  if (promptReceived) {
    Serial.println("\n✅ Prompt received, sending message...");
    gsmSerial.print(message);
    delay(500);
    esp_task_wdt_reset(); // Reset after delay
    gsmSerial.write(26); // Ctrl+Z to send
    
    // Wait for send confirmation
    unsigned long sendStartTime = millis();
    response = "";
    response.reserve(100); // Reserve memory
    
    while (millis() - sendStartTime < 15000) { // 15 second timeout for sending
      // Reset watchdog every 3 seconds during wait
      if ((millis() - sendStartTime) % 3000 == 0) {
        esp_task_wdt_reset();
      }
      
      if (gsmSerial.available()) {
        char c = (char)gsmSerial.read();
        if (response.length() < 80) { // Prevent memory overflow
          response += c;
        }
        
        if (response.indexOf("OK") != -1) {
          Serial.println("✅ SMS sent successfully!");
          return;
        }
        if (response.indexOf("ERROR") != -1) {
          Serial.println("❌ SMS send error: " + response);
          return;
        }
      }
      delay(10);
    }
    Serial.println("⚠️ SMS send timeout - status unknown");
  } else {
    Serial.println("❌ No SMS prompt received. Response: " + response);
    Serial.println("🔧 Trying to reset SMS mode...");
    
    // Try to reset and retry once
    gsmSerial.write(27); // ESC to cancel
    delay(1000);
    esp_task_wdt_reset(); // Reset after delay
    
    gsmSerial.println("AT+CMGF=1");
    delay(1000);
    esp_task_wdt_reset(); // Reset after delay
    
    Serial.println("❌ SMS failed - check SIM card and network");
  }
}

void handleCall(String response) {
  int start = response.indexOf("\"") + 1;
  int end = response.indexOf("\"", start);
  if (start == 0 || end == -1) return;
  
  String caller = response.substring(start, end);
  Serial.println("📞 Incoming call from: " + caller);
  
  if (caller != AUTHORIZED_NUMBER_1 && caller != AUTHORIZED_NUMBER_2) {
    Serial.println("❌ Unauthorized caller - hanging up");
    gsmSerial.println("ATH");
    return;
  }
  
  Serial.println("✅ Authorized caller - call rejected (use SMS for commands)");
  gsmSerial.println("ATH"); // Hang up - use SMS for commands instead
}

// Handle stored SMS messages (different format than immediate SMS)
void handleStoredSMS(String response) {
  Serial.println("📱 Processing stored SMS: " + response);
  
  // Handle both CMGL (list) and CMGR (read) formats
  int msgStart = 0;
  
  // Try CMGL format first: +CMGL: index,"status","sender","","timestamp"
  while ((msgStart = response.indexOf("+CMGL:", msgStart)) != -1) {
    // Find the sender phone number
    int firstQuote = response.indexOf("\"", msgStart);
    int secondQuote = response.indexOf("\"", firstQuote + 1);
    int thirdQuote = response.indexOf("\"", secondQuote + 1);
    int fourthQuote = response.indexOf("\"", thirdQuote + 1);
    
    if (firstQuote == -1 || secondQuote == -1 || thirdQuote == -1 || fourthQuote == -1) {
      msgStart++;
      continue;
    }
    
    String sender = response.substring(thirdQuote + 1, fourthQuote);
    Serial.println("📞 Stored SMS from: " + sender);
    
    // Check if sender is authorized
    if (sender != AUTHORIZED_NUMBER_1 && sender != AUTHORIZED_NUMBER_2) {
      Serial.println("❌ Unauthorized sender: " + sender);
      msgStart = fourthQuote + 1;
      continue;
    }
    
    // Find the message body (after the timestamp line)
    int bodyStart = response.indexOf("\r\n", fourthQuote) + 2;
    int bodyEnd = response.indexOf("\r\n", bodyStart);
    if (bodyEnd == -1) bodyEnd = response.indexOf("+CMGL:", bodyStart);
    if (bodyEnd == -1) bodyEnd = response.length();
    
    if (bodyStart < fourthQuote || bodyStart >= bodyEnd) {
      Serial.println("❌ Could not find SMS body");
      msgStart = fourthQuote + 1;
      continue;
    }
    
    String body = response.substring(bodyStart, bodyEnd);
    body.trim();
    Serial.println("� Stored OSMS message: " + body);
    
    // Process the command
    processSMSCommand(body, sender);
    
    msgStart = bodyEnd;
  }
  
  // If no CMGL messages found, try CMGR format: +CMGR: "status","sender","","timestamp"
  msgStart = 0;
  while ((msgStart = response.indexOf("+CMGR:", msgStart)) != -1) {
    Serial.println("📱 Processing CMGR format SMS...");
    
    // Find the sender phone number in CMGR format
    int firstQuote = response.indexOf("\"", msgStart);
    int secondQuote = response.indexOf("\"", firstQuote + 1);
    int thirdQuote = response.indexOf("\"", secondQuote + 1);
    int fourthQuote = response.indexOf("\"", thirdQuote + 1);
    
    if (firstQuote == -1 || secondQuote == -1 || thirdQuote == -1 || fourthQuote == -1) {
      msgStart++;
      continue;
    }
    
    String sender = response.substring(secondQuote + 1, thirdQuote);
    Serial.println("📞 CMGR SMS from: " + sender);
    
    // Check if sender is authorized
    if (sender != AUTHORIZED_NUMBER_1 && sender != AUTHORIZED_NUMBER_2) {
      Serial.println("❌ Unauthorized sender: " + sender);
      msgStart = fourthQuote + 1;
      continue;
    }
    
    // Find the message body (after the timestamp line)
    int bodyStart = response.indexOf("\r\n", fourthQuote) + 2;
    int bodyEnd = response.indexOf("\r\n", bodyStart);
    if (bodyEnd == -1) bodyEnd = response.indexOf("+CMGR:", bodyStart);
    if (bodyEnd == -1) bodyEnd = response.length();
    
    if (bodyStart < fourthQuote || bodyStart >= bodyEnd) {
      Serial.println("❌ Could not find CMGR SMS body");
      msgStart = fourthQuote + 1;
      continue;
    }
    
    String body = response.substring(bodyStart, bodyEnd);
    body.trim();
    Serial.println("💬 CMGR SMS message: " + body);
    
    // Process the command
    processSMSCommand(body, sender);
    
    msgStart = bodyEnd;
  }
}

// Handle immediate SMS messages (+CMT format)
void handleSMS(String response) {
  Serial.println("📱 Immediate SMS received: " + response);
  
  // Parse +CMT format: +CMT: "sender","","timestamp"
  int start = response.indexOf("\"") + 1;
  int end = response.indexOf("\"", start);
  if (start == 0 || end == -1) {
    Serial.println("❌ Invalid SMS format - cannot find sender");
    return;
  }
  
  String sender = response.substring(start, end);
  Serial.println("📞 From: " + sender);
  
  // Check if sender is authorized
  if (sender != AUTHORIZED_NUMBER_1 && sender != AUTHORIZED_NUMBER_2) {
    Serial.println("❌ Unauthorized sender: " + sender);
    sendSMS("UNAUTHORIZED ACCESS ATTEMPT FROM: " + sender, AUTHORIZED_NUMBER_1);
    return;
  }
  
  // Find message body - it's on the next line after the +CMT header
  int headerEnd = response.indexOf("\n");
  if (headerEnd == -1) {
    Serial.println("❌ No SMS body found - incomplete message");
    return;
  }
  
  String body = response.substring(headerEnd + 1);
  body.trim();
  body.replace("\r", "");
  body.replace("\n", "");
  
  if (body.length() == 0) {
    Serial.println("❌ Empty SMS body");
    return;
  }
  
  Serial.println("💬 Message: '" + body + "'");
  
  // Process the command
  processSMSCommand(body, sender);
}

// Process SMS command (shared function for both immediate and stored SMS)
void processSMSCommand(String body, String sender) {
  Serial.println("🔍 Processing SMS command from " + sender + ": " + body);
  
  // Check password
  if (body.startsWith(String(SMS_PASSWORD) + " ")) {
    String cmd = body.substring(String(SMS_PASSWORD).length() + 1);
    cmd.trim();
    cmd.toUpperCase();
    
    Serial.println("🔐 Valid password, executing command: " + cmd);
    
    if (cmd == "START") {
      Serial.println("🚗 SMS COMMAND: Starting engine...");
      
      startEngine();
      sendSMS("ENGINE STARTED - Anti-theft disabled, engine can now start physically", sender);
      
    } else if (cmd == "STOP") {
      Serial.println("🛑 SMS COMMAND: Stopping engine...");
      
      stopEngine();
      sendSMS("ENGINE STOPPED - Anti-theft activated, engine cannot start even with physical key", sender);
      
    } else if (cmd == "LOCATE") {
      Serial.println("📍 SMS COMMAND: Getting location...");
      if (gps.location.isValid()) {
        String locationMsg = "LOCATION: " + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
        locationMsg += " Speed: " + String(gps.speed.kmph(), 1) + "km/h";
        locationMsg += " Satellites: " + String(gps.satellites.value());
        sendSMS(locationMsg, sender);
      } else {
        sendSMS("GPS NOT READY - No satellite fix", sender);
      }
      
    } else if (cmd == "STATUS") {
      Serial.println("📊 SMS COMMAND: Getting status...");
      String statusMsg = "STATUS: ";
      statusMsg += engineRunning ? "ENGINE ON" : "ENGINE OFF";
      statusMsg += systemArmed ? " | ARMED" : " | DISARMED";
      statusMsg += " | WiFi: " + String(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL");
      if (wifiLossProtectionTriggered) {
        statusMsg += " | WIFI-PROTECTION: ACTIVE";
      }
      statusMsg += " | GPS: " + String(gps.satellites.value()) + " sats";
      sendSMS(statusMsg, sender);
      
    } else if (cmd == "ARM") {
      Serial.println("🔒 SMS COMMAND: Arming system...");
      systemArmed = true;
      sendSMS("SYSTEM ARMED - Anti-theft protection ON", sender);
      
    } else if (cmd == "DISARM") {
      Serial.println("🔓 SMS COMMAND: Disarming system...");
      systemArmed = false;
      sendSMS("SYSTEM DISARMED - Anti-theft protection OFF", sender);
      
    } else if (cmd == "RESET") {
      Serial.println("🔄 SMS COMMAND: System reset requested...");
      sendSMS("SYSTEM RESET - Restarting in 3 seconds...", sender);
      delay(3000);
      ESP.restart();
      
    } else {
      Serial.println("❌ Unknown command: " + cmd);
      sendSMS("UNKNOWN COMMAND. Valid: START, STOP, LOCATE, STATUS, ARM, DISARM, RESET", sender);
    }
    
  } else {
    Serial.println("❌ Invalid password in SMS: " + body);
    Serial.println("❌ Expected format: '" + String(SMS_PASSWORD) + " COMMAND'");
    sendSMS("INVALID PASSWORD. Format: " + String(SMS_PASSWORD) + " COMMAND", sender);
  }
}

// ===== ENHANCED SMS PARSING FUNCTIONS =====

// Decode hex-encoded SMS messages
String decodeHexSMS(String hexString) {
  String decoded = "";
  hexString.toUpperCase();
  
  // Remove any spaces or non-hex characters
  String cleanHex = "";
  for (int i = 0; i < hexString.length(); i++) {
    char c = hexString.charAt(i);
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
      cleanHex += c;
    }
  }
  
  // Convert hex pairs to ASCII characters
  for (int i = 0; i < cleanHex.length(); i += 2) {
    if (i + 1 < cleanHex.length()) {
      String hexPair = cleanHex.substring(i, i + 2);
      char c = (char)strtol(hexPair.c_str(), NULL, 16);
      if (c >= 32 && c <= 126) { // Printable ASCII only
        decoded += c;
      }
    }
  }
  
  return decoded;
}

// Simple SMS parser for your GSM module format
void parseSimpleSMS(String response) {
  Serial.println("🔍 SIMPLE SMS PARSER: " + response);
  
  // Look for hex-encoded message bodies in CMGR responses
  int cmgrPos = response.indexOf("+CMGR:");
  if (cmgrPos != -1) {
    // Find the message body (usually on the next line after +CMGR header)
    int lineEnd = response.indexOf("\n", cmgrPos);
    if (lineEnd != -1) {
      int bodyStart = lineEnd + 1;
      int bodyEnd = response.indexOf("OK", bodyStart);
      if (bodyEnd == -1) bodyEnd = response.length();
      
      String messageBody = response.substring(bodyStart, bodyEnd);
      messageBody.trim();
      messageBody.replace("\r", "");
      messageBody.replace("\n", "");
      
      Serial.println("📱 Raw message body: '" + messageBody + "'");
      
      // Check if it looks like hex (long string of hex characters)
      if (messageBody.length() > 20) {
        String decoded = decodeHexSMS(messageBody);
        Serial.println("📱 Decoded message: '" + decoded + "'");
        
        // Check if decoded message contains our commands
        if (decoded.indexOf("1234") != -1) {
          Serial.println("✅ Found 1234 command in decoded message!");
          processSMSCommand(decoded, AUTHORIZED_NUMBER_1);
          return;
        }
      }
      
      // If not hex, try processing as plain text
      if (messageBody.indexOf("1234") != -1) {
        Serial.println("✅ Found 1234 command in plain text!");
        processSMSCommand(messageBody, AUTHORIZED_NUMBER_1);
      }
    }
  }
}

// Enhanced checkStoredSMS with better parsing
void checkStoredSMSEnhanced() {
  Serial.println("📱 ENHANCED SMS CHECK...");
  
  // Reset watchdog
  esp_task_wdt_reset();
  
  // Clear buffer
  while (gsmSerial.available()) {
    gsmSerial.read();
  }
  
  // Try reading SMS by index (more reliable)
  for (int i = 1; i <= 10; i++) {
    Serial.println("📱 Checking SMS index " + String(i));
    gsmSerial.println("AT+CMGR=" + String(i));
    delay(2000);
    esp_task_wdt_reset();
    
    String response = "";
    unsigned long startTime = millis();
    while (gsmSerial.available() && (millis() - startTime < 3000)) {
      response += (char)gsmSerial.read();
      delay(10);
    }
    
    if (response.indexOf("+CMGR:") != -1) {
      Serial.println("📨 Found SMS at index " + String(i));
      Serial.println("📨 Response: " + response);
      
      // Use simple parser
      parseSimpleSMS(response);
      
      // Delete this message
      gsmSerial.println("AT+CMGD=" + String(i));
      delay(1000);
      esp_task_wdt_reset();
    }
  }
}
// ===== TIME SYNCHRONIZATION FUNCTIONS =====

// Sync time with NTP servers
void syncTimeWithNTP() {
  Serial.println("🕐 Configuring NTP time synchronization...");
  
  // Configure NTP with multiple servers for reliability
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");  // UTC+8 for Philippines
  
  // Wait for time synchronization
  Serial.print("🕐 Waiting for NTP sync");
  int attempts = 0;
  while (time(nullptr) < 1000000000L && attempts < 20) {  // Wait until we get a reasonable timestamp
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (time(nullptr) > 1000000000L) {
    Serial.println("\n✅ NTP time synchronized successfully!");
    Serial.println("🕐 Current time: " + getCurrentTimestamp());
  } else {
    Serial.println("\n⚠️ NTP sync failed, using system time");
  }
}

// Get current Unix timestamp (seconds since 1970)
unsigned long getCurrentUnixTime() {
  time_t now = time(nullptr);
  if (now > 1000000000L) {  // Valid timestamp
    return (unsigned long)now;
  } else {
    // Fallback: estimate based on millis() + a base time (2025)
    // This is approximate but better than 1970
    unsigned long base2025 = 1735689600;  // Jan 1, 2025 00:00:00 UTC
    return base2025 + (millis() / 1000);
  }
}

// Get human-readable timestamp
String getCurrentTimestamp() {
  time_t now = time(nullptr);
  if (now > 1000000000L) {  // Valid timestamp
    struct tm* timeinfo = localtime(&now);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
  } else {
    // Fallback format
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    return "2025-01-" + String(1 + (days % 31)) + " " + 
           String((hours % 24), DEC) + ":" + 
           String((minutes % 60), DEC) + ":" + 
           String((seconds % 60), DEC);
  }
}

// Enhanced location update with better GPS validation
void updateLocationEnhanced() {
  if (!Firebase.ready()) return;
  
  // Enhanced GPS validation
  if (!gps.location.isValid() || gps.location.age() > 10000) {
    Serial.println("⚠️ GPS data invalid or too old, skipping update");
    return;
  }
  
  // Validate GPS coordinates (basic sanity check)
  double lat = gps.location.lat();
  double lng = gps.location.lng();
  
  if (lat < -90 || lat > 90 || lng < -180 || lng > 180) {
    Serial.println("⚠️ GPS coordinates out of range, skipping update");
    return;
  }
  
  // Get current time
  unsigned long currentTime = getCurrentUnixTime();
  
  FirebaseJson json;
  json.set("latitude", lat);
  json.set("longitude", lng);
  json.set("speed", gps.speed.kmph());
  json.set("altitude", gps.altitude.meters());
  json.set("satellites", gps.satellites.value());
  json.set("hdop", gps.hdop.value());
  json.set("course", gps.course.deg());
  json.set("timestamp", currentTime);
  json.set("timestampMs", millis());
  json.set("valid", true);
  json.set("lastUpdate", getCurrentTimestamp());
  json.set("age", gps.location.age());  // Age of GPS data in milliseconds
  
  // Add GPS quality indicators
  String quality = "Unknown";
  if (gps.satellites.value() >= 8) quality = "Excellent";
  else if (gps.satellites.value() >= 6) quality = "Good";
  else if (gps.satellites.value() >= 4) quality = "Fair";
  else quality = "Poor";
  
  json.set("quality", quality);
  
  Serial.println("📍 Enhanced GPS Update:");
  Serial.println("   Lat: " + String(lat, 6) + ", Lng: " + String(lng, 6));
  Serial.println("   Satellites: " + String(gps.satellites.value()) + " (" + quality + ")");
  Serial.println("   Time: " + getCurrentTimestamp());
  
  if (Firebase.RTDB.setJSON(&fbdo, (String("/devices/") + DEVICE_ID + "/location").c_str(), &json)) {
    Serial.println("✅ Enhanced location updated successfully");
  } else {
    Serial.println("❌ Location update failed: " + fbdo.errorReason());
  }
}