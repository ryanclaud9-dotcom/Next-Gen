# 🔧 Compilation Error Fix

## Error: 'LED_INVERTED' was not declared in this scope

### 🚀 Quick Fix (Option 1)

Add this line at the top of your Arduino sketch, right after the includes:

```cpp
#include "config.h"

// Add this line if LED_INVERTED is not defined
#define LED_INVERTED true  // ESP32 built-in LED is inverted

// Function declarations
void handleStoredSMS(String response);
// ... rest of your code
```

### 🔧 Complete Fix (Option 2)

Make sure your `config.h` file contains this section:

```cpp
// LED Logic Configuration
#define LED_INVERTED true  // ESP32 built-in LED is inverted (LOW=ON, HIGH=OFF)

// Relay Logic Configuration  
#define IGNITION_SWITCH_INVERTED true   // Use NC contact to prevent auto-start
#define IGNITION_COIL_INVERTED false    // Keep as false for ignition coil
```

### 🎯 Alternative Simple Fix (Option 3)

If you want to avoid the configuration complexity, replace the `setLED` function with this simpler version:

```cpp
// Simple LED control function (no configuration needed)
void setLED(bool enable) {
  // ESP32 built-in LED is inverted: LOW=ON, HIGH=OFF
  digitalWrite(LIGHT_INDICATOR_PIN, enable ? LOW : HIGH);
  Serial.print("LED (GPIO 2): ");
  Serial.println(enable ? "ON" : "OFF");
}
```

### 📁 File Check

Make sure you have these files in your Arduino sketch folder:
- `YourSketch.ino` (main file)
- `config.h` (configuration file)

### 🔄 Steps to Fix

1. **Open your Arduino sketch**
2. **Add the LED_INVERTED definition** (use Option 1 above)
3. **Compile again** - should work now!
4. **Upload to ESP32**
5. **Test with `1234 START`** - LED should light up!

### ✅ Expected Result

After fixing, the DS1 red LED should:
- Turn ON when engine starts (SMS: `1234 START` or Dashboard START)
- Turn OFF when engine stops (SMS: `1234 STOP` or Dashboard STOP)

The compilation error should be resolved and the LED should work correctly with inverted logic! 🔴✨