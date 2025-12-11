# Quick Start Guide

## Prerequisites

- ESP32 board with USB cable
- Arduino IDE installed
- Firebase account
- Active SIM card for GSM module

## Step 1: Hardware Assembly

1. Connect GPS module to ESP32:
   - GPS TX → ESP32 RX (GPIO 16)
   - GPS RX → ESP32 TX (GPIO 17)
   - VCC → 3.3V, GND → GND

2. Connect GSM module:
   - GSM TX → ESP32 RX (GPIO 26)
   - GSM RX → ESP32 TX (GPIO 27)
   - VCC → 5V, GND → GND

3. Connect sensors:
   - Vibration sensor → GPIO 14
   - Relay → GPIO 12
   - Buzzer → GPIO 13
   - LED → GPIO 2

## Step 2: Firebase Configuration

1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Create new project
3. Enable Realtime Database (Start in test mode)
4. Enable Authentication → Email/Password
5. Copy your config from Project Settings

## Step 3: Firmware Setup

1. Open Arduino IDE
2. Install ESP32 board support
3. Install libraries via Library Manager:
   - TinyGPS++
   - Firebase ESP32 Client
   - ArduinoJson

4. Copy `firmware/config.h.template` to `firmware/config.h`
5. Edit `config.h` with your credentials
6. Open `firmware/anti_theft_esp32_optimized.ino`
7. Select board: "ESP32 Dev Module"
8. Upload to ESP32

## Step 4: Dashboard Setup

1. Edit `dashboard/js/firebase-config.js`:
```javascript
const firebaseConfig = {
    apiKey: "your-api-key",
    authDomain: "your-project.firebaseapp.com",
    databaseURL: "https://your-project.firebaseio.com",
    projectId: "your-project-id",
    storageBucket: "your-project.appspot.com",
    messagingSenderId: "your-sender-id",
    appId: "your-app-id"
};
```

2. Open `dashboard/index.html` in browser
3. Register a new account
4. Login and start monitoring!

## Troubleshooting

**ESP32 won't connect to WiFi:**
- Check SSID and password in config.h
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)

**No GPS data:**
- GPS needs clear sky view
- Wait 1-2 minutes for initial fix
- Check wiring connections

**Dashboard shows offline:**
- Verify Firebase credentials
- Check ESP32 serial monitor for errors
- Ensure database rules allow read/write

**GSM not working:**
- Check SIM card has credit
- Verify APN settings for your carrier
- Ensure antenna is connected

## Next Steps

- Configure geofencing in dashboard
- Set speed limits
- Test remote commands
- Review security settings
