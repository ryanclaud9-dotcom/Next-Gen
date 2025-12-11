# 🔆 LED Indicator Troubleshooting Guide

## Issue: DS1 LED Not Lighting When Engine Starts

### 🔍 Possible Causes & Solutions

### 1. **Wiring Issues** (Most Common)

#### Check LED Connection:
```
ESP32 GPIO 2 → LED Anode (+) → 220Ω Resistor → GND
OR
ESP32 GPIO 2 → 220Ω Resistor → LED Anode (+) → LED Cathode (-) → GND
```

#### LED Polarity:
- **Anode (+)**: Longer leg, connects to GPIO 2 (through resistor)
- **Cathode (-)**: Shorter leg, connects to GND

### 2. **Test LED Manually**

#### Upload the updated firmware with debugging, then test:
```
// In Serial Monitor, type:
TEST LED
```

This will:
1. Turn LED OFF for 1 second
2. Turn LED ON for 1 second  
3. Turn LED OFF again
4. Show test results

### 3. **Check Serial Monitor Output**

When you start the engine, you should see:
```
🔆 LED CONTROL: Setting GPIO 2 to HIGH (LED should be ON)
🔆 LED Status: ON
```

If you see "LED Status: OFF" when it should be ON, there's a wiring issue.

### 4. **Hardware Troubleshooting**

#### Step 1: Check Voltage
- Use multimeter on GPIO 2
- Should read ~3.3V when engine starts
- Should read 0V when engine stops

#### Step 2: Check LED with External Power
- Connect LED directly to 3.3V and GND
- If LED works → Wiring issue
- If LED doesn't work → Bad LED

#### Step 3: Check Resistor
- Use 220Ω to 1kΩ resistor
- Too high resistance = dim LED
- No resistor = LED might burn out

### 5. **Alternative LED Connection**

If current wiring doesn't work, try this:

```
ESP32 GPIO 2 → 220Ω Resistor → LED Anode (+)
LED Cathode (-) → GND
```

### 6. **Code Verification**

The firmware should have this logic:
```cpp
// Engine START
digitalWrite(LIGHT_INDICATOR_PIN, HIGH);  // LED ON

// Engine STOP  
digitalWrite(LIGHT_INDICATOR_PIN, LOW);   // LED OFF
```

### 7. **Common Wiring Mistakes**

❌ **Wrong:**
- LED connected backwards (reverse polarity)
- No resistor (LED burns out)
- Connected to wrong GPIO pin
- Poor connections/loose wires

✅ **Correct:**
- LED anode (+) to GPIO 2 through resistor
- LED cathode (-) to GND
- Solid connections
- 220Ω-1kΩ resistor

### 8. **Testing Steps**

1. **Upload updated firmware** with LED debugging
2. **Open Serial Monitor** (115200 baud)
3. **Type "TEST LED"** to test LED manually
4. **Send "1234 START"** SMS and watch serial output
5. **Check for LED debugging messages**

### 9. **Expected Serial Output**

#### When Engine Starts:
```
🚗 SMS COMMAND: Starting engine...
🔓 ANTI-THEFT: Enabling ignition coil
🔆 LED CONTROL: Setting GPIO 2 to HIGH (LED should be ON)
🔆 LED Status: ON
✓ Starter engaged
```

#### When Engine Stops:
```
🛑 SMS COMMAND: Stopping engine...
🔒 ANTI-THEFT: Disabling ignition coil
🔅 LED CONTROL: Setting GPIO 2 to LOW (LED should be OFF)
🔅 LED Status: OFF
✓ Engine stopped
```

### 10. **Quick Fix Checklist**

- [ ] LED connected with correct polarity
- [ ] 220Ω resistor in series with LED
- [ ] GPIO 2 connected to LED anode (through resistor)
- [ ] LED cathode connected to GND
- [ ] All connections are solid
- [ ] LED is not burned out (test with external power)
- [ ] Serial monitor shows "LED Status: ON" when starting

### 11. **Alternative GPIO Pin**

If GPIO 2 has issues, you can change to another pin:

```cpp
// In config.h, change:
#define LIGHT_INDICATOR_PIN 4  // Use GPIO 4 instead of GPIO 2
```

Then reconnect LED to GPIO 4.

## 🔧 Most Likely Solution

**Check LED polarity and wiring** - this is the most common cause. The LED should light up when you send `1234 START` and turn off when you send `1234 STOP`.

Use the `TEST LED` command to verify the LED hardware is working correctly!