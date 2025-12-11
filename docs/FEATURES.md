# Features Documentation

## Real-Time GPS Tracking

The system continuously monitors vehicle location using a GPS module and updates the dashboard every 10 seconds.

**Features:**
- Live location on interactive map
- Coordinate display (latitude/longitude)
- Satellite count indicator
- Speed from GPS data
- Route history visualization

## Remote Engine Control

Control your vehicle's ignition remotely through the web dashboard.

**Commands:**
- **START** - Start the engine remotely
- **STOP** - Stop the engine immediately
- **Confirmation required** for safety

**How it works:**
- Dashboard sends command to Firebase
- ESP32 reads command from database
- Relay activates/deactivates ignition circuit
- Status updated in real-time

## Security System

Comprehensive security features to protect your vehicle.

**Features:**
- **ARM/DISARM** - Enable/disable security system
- **Vibration Detection** - SW-420 sensor detects tampering
- **Instant Alerts** - SMS and dashboard notifications
- **Buzzer Alarm** - Audible alert on breach

## Speed Monitoring

Track vehicle speed and set custom limits.

**Features:**
- Real-time speed display from GPS
- Configurable speed limits
- Instant alerts when limit exceeded
- Max speed recording
- Visual warnings on dashboard

**Setting Speed Limit:**
1. Go to Overview tab
2. Enter desired limit (10-200 km/h)
3. Click "Set" button
4. System applies immediately

## Geofencing

Create virtual boundaries and receive alerts when vehicle leaves the area.

**Setup:**
1. Click "Configure Geofence" button
2. Enter center coordinates
3. Set radius in meters
4. Name the zone
5. System monitors automatically

**Alerts:**
- Dashboard notification
- SMS to authorized number
- Distance from boundary shown

## Battery Monitoring

Real-time monitoring of vehicle battery status.

**Displays:**
- Voltage (V)
- Percentage level
- Status (Good/Low/Critical)
- Visual battery indicator

**Alerts:**
- Low battery warning at 30%
- Critical alert at 15%

## SMS Notifications

Instant alerts via GSM module to authorized phone number.

**Alert Types:**
- Security breach (vibration detected)
- Speed limit exceeded
- Geofence breach
- Low battery
- System armed/disarmed

**SMS Commands:**
- Send "STATUS" - Get current status
- Send "LOCATE" - Get GPS coordinates
- Send "ARM" - Arm security system
- Send "DISARM" - Disarm system

## Trip Statistics

Track your vehicle usage and driving patterns.

**Metrics:**
- Today's distance traveled
- Maximum speed reached
- Average speed
- Trip duration
- Route history

**Export Data:**
- CSV format
- Includes timestamp, location, speed
- Download from dashboard

## Dashboard Features

Modern, responsive web interface.

**Tabs:**
- **Overview** - All key metrics at a glance
- **Map** - Full-screen location tracking
- **Control** - Remote command center
- **History** - Events and notifications log

**Mobile Optimized:**
- Bottom navigation on mobile
- Touch-friendly controls
- Responsive grid layout
- Works on all screen sizes

## Event Logging

Complete history of all system events.

**Logged Events:**
- Engine start/stop
- System arm/disarm
- Security alerts
- Speed violations
- Geofence breaches
- Command executions

**Features:**
- Timestamp for each event
- Color-coded by severity
- Searchable history
- Auto-refresh

## Notifications

Real-time alerts in dashboard and browser.

**Types:**
- Security alerts (red)
- System status (blue)
- Speed warnings (orange)
- Battery alerts (yellow)

**Delivery:**
- Dashboard timeline
- Browser notifications
- SMS to phone
- All synchronized via Firebase
