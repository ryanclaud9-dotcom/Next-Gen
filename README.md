# 🚗 ESP32 Anti-Theft System - Next Gen

A comprehensive IoT-based anti-theft system for motorcycles/vehicles using ESP32, GPS tracking, GSM communication, and real-time web dashboard.

## 🌟 Features

### 🔒 Anti-Theft Protection
- **Dual Relay Control**: Ignition switch + ignition coil control
- **SMS Commands**: Remote engine control via SMS (`1234 START`, `1234 STOP`)
- **Real-time Vibration Detection**: Immediate alerts on unauthorized movement
- **GPS Tracking**: Live location monitoring with geofencing
- **Web Dashboard**: Real-time control and monitoring interface

### 📱 Communication
- **SMS Control**: Send commands via SMS from authorized numbers
- **Firebase Integration**: Real-time data synchronization
- **GSM Fallback**: Internet backup when WiFi fails
- **Instant Alerts**: SMS notifications for theft attempts

### 🛡️ Security Features
- **Authorized Numbers Only**: SMS commands restricted to pre-configured numbers
- **Password Protection**: SMS commands require password (`1234`)
- **Anti-Theft Logic**: Engine cannot start without SMS/Dashboard authorization
- **Movement Detection**: Smart vibration sensor with immediate alerts

## 🔧 Hardware Requirements

### Core Components
- **ESP32 Development Board** (ESP32-WROOM-32)
- **SIM800L GSM Module** (SMS and internet backup)
- **NEO-6M GPS Module** (Location tracking)
- **2-Channel Relay Module** (Engine control)
- **Vibration Sensor** (Movement detection)
- **Buzzer** (Audio alerts)
- **LED Indicator** (Status indication)

### Wiring Connections
```
ESP32 Pin | Component
----------|----------
GPIO 16   | GPS RX
GPIO 17   | GPS TX
GPIO 25   | GSM RX
GPIO 26   | GSM TX
GPIO 12   | Ignition Switch Relay
GPIO 13   | Ignition Coil Relay
GPIO 14   | Vibration Sensor
GPIO 15   | Buzzer
GPIO 2    | LED Indicator
```

## 🚀 Quick Start

### 1. Hardware Setup
1. Connect all components according to the wiring diagram
2. Install SIM card in GSM module with SMS plan
3. Connect GPS and GSM antennas
4. Power the ESP32 with 5V supply

### 2. Software Configuration
1. **Configure WiFi and Firebase**:
   ```cpp
   // Copy firmware/config.h.secure_template to firmware/config.h
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASSWORD "YourWiFiPassword"
   #define FIREBASE_API_KEY "your-firebase-api-key"
   #define FIREBASE_DATABASE_URL "your-project.firebaseio.com"
   ```

2. **Set Authorized Phone Numbers**:
   ```cpp
   #define AUTHORIZED_NUMBER_1 "+1234567890"
   #define AUTHORIZED_NUMBER_2 "+0987654321"
   #define SMS_PASSWORD "1234"
   ```

3. **Upload Firmware**:
   - Open `firmware/anti_theft_esp32_optimized.ino` in Arduino IDE
   - Select ESP32 board and correct port
   - Upload the code

### 3. Dashboard Setup
1. **Configure Firebase**:
   ```javascript
   // Copy dashboard/js/firebase-config.js.template to dashboard/js/firebase-config.js
   const firebaseConfig = {
     apiKey: "your-api-key",
     authDomain: "your-project.firebaseapp.com",
     databaseURL: "https://your-project.firebaseio.com",
     projectId: "your-project-id"
   };
   ```

2. **Deploy Dashboard**:
   - Host the `dashboard/` folder on a web server
   - Or use Firebase Hosting for easy deployment

## 📱 SMS Commands

Send these SMS commands to control your vehicle:

| Command | Function | Response |
|---------|----------|----------|
| `1234 START` | Enable ignition coil (engine can start) | "ENGINE STARTED - Anti-theft disabled" |
| `1234 STOP` | Disable ignition coil (engine cannot start) | "ENGINE STOPPED - Anti-theft activated" |
| `1234 STATUS` | Get system status | Current engine/GPS/network status |
| `1234 LOCATE` | Get GPS location | GPS coordinates and speed |
| `1234 ARM` | Arm anti-theft system | "SYSTEM ARMED" |
| `1234 DISARM` | Disarm anti-theft system | "SYSTEM DISARMED" |

## 🌐 Web Dashboard

Access the web dashboard to:
- **Monitor Location**: Real-time GPS tracking on map
- **Control Engine**: START/STOP buttons
- **View Status**: System health and sensor data
- **Check History**: Location and event logs
- **Configure Settings**: Geofencing and alerts

## 🔧 Advanced Configuration

### Relay Logic Configuration
```cpp
// For Normally Closed (NC) relays (recommended)
#define IGNITION_SWITCH_INVERTED true
#define IGNITION_COIL_INVERTED false
```

### Vibration Sensitivity
```cpp
// Ultra-fast detection (10ms intervals)
// Adjust debouncing for your environment
#define VIBRATION_DEBOUNCE 100  // milliseconds
```

### SMS Cooldown Settings
```cpp
#define SMS_COOLDOWN 30000           // 30 seconds between SMS
#define MOVEMENT_SMS_COOLDOWN 120000 // 2 minutes between movement alerts
```

## 🛠️ Troubleshooting

### Common Issues

**SMS Commands Not Working**:
- Check SIM card has SMS plan and credit
- Verify authorized phone numbers in config.h
- Check GSM signal strength (should be >10)

**GPS Not Working**:
- Ensure GPS antenna is connected and has clear sky view
- Wait 2-5 minutes for initial GPS fix
- Check GPS wiring connections

**Engine Control Issues**:
- Verify relay wiring to ignition switch and coil
- Check relay logic configuration (NC vs NO)
- Test relays manually with multimeter

**Dashboard Not Loading**:
- Check Firebase configuration
- Verify internet connection
- Check browser console for errors

### Serial Monitor Commands
Type these in Arduino IDE Serial Monitor for debugging:
- `TEST GSM` - Test GSM functionality
- `READ SMS` - Manually check for SMS messages
- `SHOW CONFIG` - Display current configuration
- `RESET GSM` - Reinitialize GSM module

## 📊 System Architecture

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   ESP32     │◄──►│  Firebase   │◄──►│ Web Dashboard│
│ Controller  │    │  Database   │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
       │
       ├── GPS Module (Location)
       ├── GSM Module (SMS + Internet)
       ├── Relay Module (Engine Control)
       ├── Vibration Sensor (Movement)
       └── Buzzer + LED (Alerts)
```

## 🔐 Security Considerations

- **Change Default Password**: Modify SMS_PASSWORD from "1234"
- **Secure Firebase**: Configure proper Firebase security rules
- **Authorized Numbers**: Only add trusted phone numbers
- **Physical Security**: Hide ESP32 and wiring in vehicle
- **SIM Security**: Use SIM with PIN protection

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📞 Support

For support and questions:
- Create an issue on GitHub
- Check the troubleshooting section
- Review the serial monitor output for debugging

---

**⚠️ Disclaimer**: This system is for educational and security purposes. Always comply with local laws and regulations regarding vehicle modifications and tracking devices.