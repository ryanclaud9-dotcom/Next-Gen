# Complete Setup Guide

## Table of Contents
1. [Hardware Setup](#hardware-setup)
2. [Software Installation](#software-installation)
3. [Firebase Configuration](#firebase-configuration)
4. [Firmware Configuration](#firmware-configuration)
5. [Dashboard Deployment](#dashboard-deployment)
6. [Testing](#testing)

## Hardware Setup

### Required Components

| Component | Specification | Quantity |
|-----------|--------------|----------|
| ESP32 | Dev Module | 1 |
| GPS Module | NEO-6M | 1 |
| GSM Module | SIM800L/SIM900 | 1 |
| Vibration Sensor | SW-420 | 1 |
| Relay Module | 5V, 10A | 1 |
| Buzzer | 5V Active | 1 |
| LED | Any color | 1 |
| Resistor | 220Ω | 1 |
| Power Converter | 12V to 5V, 3A | 1 |

### Wiring Diagram

```
ESP32 Connections:
├── GPS Module (NEO-6M)
│   ├── GPS TX → ESP32 GPIO 16 (RX2)
│   ├── GPS RX → ESP32 GPIO 17 (TX2)
│   ├── VCC → 3.3V
│   └── GND → GND
│
├── GSM Module (SIM800L)
│   ├── GSM TX → ESP32 GPIO 26 (RX1)
│   ├── GSM RX → ESP32 GPIO 27 (TX1)
│   ├── VCC → 5V (via converter)
│   └── GND → GND
│
├── Sensors & Actuators
│   ├── Vibration Sensor → GPIO 14
│   ├── Relay Control → GPIO 12
│   ├── Buzzer → GPIO 13
│   └── LED → GPIO 2 (with 220Ω resistor)
│
└── Power
    ├── 12V from vehicle battery
    └── 5V/3.3V via converter
```

### Assembly Steps

1. **Mount ESP32** on breadboard or PCB
2. **Connect GPS module** - Ensure TX/RX are crossed
3. **Connect GSM module** - Use separate power if needed
4. **Wire sensors** - Follow GPIO assignments
5. **Connect relay** - For ignition control
6. **Add power converter** - 12V vehicle → 5V/3.3V
7. **Test connections** - Use multimeter to verify

### Important Notes

- GSM module needs 2A peak current - use capacitor or separate supply
- GPS antenna needs clear sky view
- Keep GPS away from GSM to avoid interference
- Use proper gauge wire for relay (handles ignition current)
- Add fuse on 12V input for safety

## Software Installation

### Arduino IDE Setup

1. **Download Arduino IDE**
   - Visit [arduino.cc](https://www.arduino.cc/en/software)
   - Install latest version

2. **Add ESP32 Board Support**
   - Open Arduino IDE
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search "ESP32" and install

3. **Install Required Libraries**
   - Go to Sketch → Include Library → Manage Libraries
   - Install these libraries:
     - **TinyGPS++** by Mikal Hart
     - **Firebase ESP32 Client** by Mobizt
     - **ArduinoJson** by Benoit Blanchon

## Firebase Configuration

### Create Firebase Project

1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Click "Add Project"
3. Enter project name (e.g., "vehicle-tracker")
4. Disable Google Analytics (optional)
5. Click "Create Project"

### Setup Realtime Database

1. In Firebase Console, go to "Realtime Database"
2. Click "Create Database"
3. Choose location closest to you
4. Start in **test mode** (we'll secure it later)
5. Click "Enable"

### Configure Authentication

1. Go to "Authentication" in Firebase Console
2. Click "Get Started"
3. Enable "Email/Password" provider
4. Save changes

### Get Configuration

1. Go to Project Settings (gear icon)
2. Scroll to "Your apps"
3. Click web icon (</>)
4. Register app (name: "Dashboard")
5. Copy the firebaseConfig object

### Security Rules

1. Go to Realtime Database → Rules
2. Replace with:
```json
{
  "rules": {
    "devices": {
      "$deviceId": {
        ".read": "auth != null",
        ".write": "auth != null"
      }
    }
  }
}
```
3. Publish rules

## Firmware Configuration

### Create Config File

1. Navigate to `firmware/` folder
2. Copy `config.h.template` to `config.h`
3. Edit `config.h`:

```cpp
// WiFi Configuration
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"

// Firebase Configuration
#define FIREBASE_HOST "your-project.firebaseio.com"
#define FIREBASE_AUTH "your-database-secret"
#define DEVICE_ID "vehicle_001"

// GSM Configuration
#define AUTHORIZED_PHONE "+1234567890"
#define SMS_PASSWORD "1234"

// GPS Configuration
#define GPS_BAUD 9600

// Pin Definitions (already set, verify if needed)
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GSM_RX_PIN 26
#define GSM_TX_PIN 27
#define VIBRATION_PIN 14
#define RELAY_PIN 12
#define BUZZER_PIN 13
#define LED_PIN 2
```

### Upload Firmware

1. Open `anti_theft_esp32_optimized.ino` in Arduino IDE
2. Select board: Tools → Board → ESP32 Dev Module
3. Select port: Tools → Port → (your ESP32 port)
4. Click Upload button
5. Wait for "Done uploading" message
6. Open Serial Monitor (115200 baud) to view logs

## Dashboard Deployment

### Option 1: Local (Development)

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

const DEVICE_ID = "vehicle_001"; // Match firmware
```

2. Open `dashboard/index.html` in web browser
3. Register new account
4. Login and test

### Option 2: Firebase Hosting (Production)

1. Install Firebase CLI:
```bash
npm install -g firebase-tools
```

2. Login to Firebase:
```bash
firebase login
```

3. Initialize hosting:
```bash
cd dashboard
firebase init hosting
```

4. Select your project
5. Set public directory: `.` (current)
6. Configure as single-page app: No
7. Deploy:
```bash
firebase deploy
```

8. Access at: `https://your-project.firebaseapp.com`

## Testing

### Test Checklist

- [ ] ESP32 connects to WiFi
- [ ] GPS gets satellite fix
- [ ] GSM module registers on network
- [ ] Dashboard shows "Online" status
- [ ] Location updates on map
- [ ] Speed displays correctly
- [ ] Battery voltage shows
- [ ] Remote START command works
- [ ] Remote STOP command works
- [ ] ARM/DISARM functions
- [ ] Vibration sensor triggers alert
- [ ] SMS notifications received
- [ ] Geofencing alerts work
- [ ] Speed limit alerts trigger

### Troubleshooting

**ESP32 won't connect:**
- Check WiFi credentials
- Ensure 2.4GHz network
- Check signal strength

**No GPS fix:**
- Move to open area
- Wait 2-5 minutes
- Check antenna connection

**GSM not working:**
- Verify SIM has credit
- Check network coverage
- Confirm APN settings

**Dashboard offline:**
- Verify Firebase config
- Check browser console for errors
- Ensure database rules allow access

**Commands not working:**
- Check Firebase connection
- Verify DEVICE_ID matches
- Review serial monitor logs

## Next Steps

1. Configure geofencing
2. Set speed limits
3. Test all remote commands
4. Add more authorized users
5. Set up backup power
6. Install in vehicle
7. Test in real conditions

## Support

For issues, check:
- Serial monitor output
- Browser console logs
- Firebase database structure
- Hardware connections
