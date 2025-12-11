# 🚀 Quick Start Guide - ESP32 Anti-Theft System

## ⚡ 5-Minute Setup

### Step 1: Hardware Connections
```
ESP32 → Component
GPIO 16 → GPS RX
GPIO 17 → GPS TX  
GPIO 25 → GSM RX
GPIO 26 → GSM TX
GPIO 12 → Ignition Switch Relay
GPIO 13 → Ignition Coil Relay
GPIO 14 → Vibration Sensor
GPIO 15 → Buzzer
GPIO 2  → LED Indicator
```

### Step 2: Configure Firmware
1. **Copy config template**:
   ```bash
   cp firmware/config.h.template firmware/config.h
   ```

2. **Edit firmware/config.h**:
   ```cpp
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASSWORD "YourWiFiPassword"
   #define AUTHORIZED_NUMBER_1 "+1234567890"  // Your phone number
   #define AUTHORIZED_NUMBER_2 "+0987654321"  // Backup number
   ```

3. **Upload to ESP32**:
   - Open `firmware/anti_theft_esp32_optimized.ino` in Arduino IDE
   - Select ESP32 board and port
   - Click Upload

### Step 3: Setup Firebase (Optional)
1. Create Firebase project at https://console.firebase.google.com
2. Enable Realtime Database
3. Copy config template:
   ```bash
   cp dashboard/js/firebase-config.js.template dashboard/js/firebase-config.js
   ```
4. Update with your Firebase credentials

### Step 4: Test SMS Commands
Send SMS to the SIM card number:
- `1234 STATUS` - Check system status
- `1234 START` - Enable engine start
- `1234 STOP` - Disable engine start
- `1234 LOCATE` - Get GPS location

## 🔧 Troubleshooting

### SMS Not Working?
1. Check SIM card has SMS plan
2. Verify phone numbers in config.h
3. Check GSM signal strength

### GPS Not Working?
1. GPS needs clear sky view
2. Wait 2-5 minutes for first fix
3. Check antenna connection

### Engine Control Issues?
1. Verify relay wiring
2. Check relay type (NC recommended)
3. Test with multimeter

## 📱 SMS Commands Reference

| Command | Function |
|---------|----------|
| `1234 START` | Enable ignition coil |
| `1234 STOP` | Disable ignition coil |
| `1234 STATUS` | System status |
| `1234 LOCATE` | GPS location |
| `1234 ARM` | Arm anti-theft |
| `1234 DISARM` | Disarm anti-theft |

## 🌐 Web Dashboard

1. Host the `dashboard/` folder on any web server
2. Access via browser for real-time control
3. Features: GPS tracking, engine control, status monitoring

## ⚠️ Security Notes

- Change default SMS password from "1234"
- Only add trusted phone numbers
- Hide ESP32 installation in vehicle
- Use SIM with PIN protection

## 📞 Need Help?

- Check serial monitor output for debugging
- Use `TEST GSM` command in serial monitor
- Create GitHub issue for support

**Ready to secure your vehicle! 🔒**