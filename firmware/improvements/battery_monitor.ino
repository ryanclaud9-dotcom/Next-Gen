// Battery Monitoring Module
// Add this to your main firmware

#define BATTERY_PIN 34  // ADC pin for voltage divider
#define VOLTAGE_DIVIDER_RATIO 5.7  // Adjust based on your resistor values

float readBatteryVoltage() {
  int rawValue = analogRead(BATTERY_PIN);
  float voltage = (rawValue / 4095.0) * 3.3 * VOLTAGE_DIVIDER_RATIO;
  return voltage;
}

void updateBatteryStatus() {
  float voltage = readBatteryVoltage();
  
  FirebaseJson json;
  json.set("voltage", voltage);
  json.set("percentage", calculateBatteryPercentage(voltage));
  json.set("status", getBatteryStatus(voltage));
  json.set("timestamp", millis());
  
  String path = String("/devices/") + DEVICE_ID + "/battery";
  Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json);
  
  // Alert if battery is low
  if (voltage < 11.5) {
    logEvent("WARNING: Low Battery - " + String(voltage) + "V");
    sendPushNotification("Low Battery Alert", "Vehicle battery at " + String(voltage) + "V");
  }
}

int calculateBatteryPercentage(float voltage) {
  // 12V battery: 12.6V = 100%, 11.8V = 0%
  if (voltage >= 12.6) return 100;
  if (voltage <= 11.8) return 0;
  return (int)((voltage - 11.8) / 0.8 * 100);
}

String getBatteryStatus(float voltage) {
  if (voltage >= 12.4) return "Good";
  if (voltage >= 12.0) return "Fair";
  if (voltage >= 11.5) return "Low";
  return "Critical";
}

// Add to loop():
// if (millis() - lastBatteryCheck > 60000) {  // Every minute
//   updateBatteryStatus();
//   lastBatteryCheck = millis();
// }
