# Wiring Diagram

## Complete Connection Guide

### ESP32 Pin Assignments

```
ESP32 GPIO Mapping:
├── GPIO 16 (RX2) ← GPS TX
├── GPIO 17 (TX2) → GPS RX
├── GPIO 26 (RX1) ← GSM TX
├── GPIO 27 (TX1) → GSM RX
├── GPIO 14 ← Vibration Sensor
├── GPIO 12 → Relay Control
├── GPIO 13 → Buzzer
└── GPIO 2 → LED
```

### Detailed Connections

#### GPS Module (NEO-6M)
```
GPS Module          ESP32
─────────────────────────
VCC (3.3V)    →    3.3V
GND           →    GND
TX            →    GPIO 16 (RX2)
RX            ←    GPIO 17 (TX2)
```

#### GSM Module (SIM800L)
```
GSM Module          ESP32
─────────────────────────
VCC (4.2V)    →    5V (via converter)
GND           →    GND
TX            →    GPIO 26 (RX1)
RX            ←    GPIO 27 (TX1)
RST           →    (optional)
```

**Important:** SIM800L needs 2A peak current. Use:
- Large capacitor (1000µF) near VCC/GND
- Or separate 5V/2A power supply

#### Vibration Sensor (SW-420)
```
Sensor              ESP32
─────────────────────────
VCC           →    3.3V
GND           →    GND
DO            →    GPIO 14
```

#### Relay Module (5V)
```
Relay               ESP32
─────────────────────────
VCC           →    5V
GND           →    GND
IN            ←    GPIO 12
COM           →    Ignition wire
NO            →    Ignition control
```

#### Buzzer (Active 5V)
```
Buzzer              ESP32
─────────────────────────
+             ←    GPIO 13
-             →    GND
```

#### LED Indicator
```
LED                 ESP32
─────────────────────────
Anode (+)     ←    GPIO 2
              ↓    220Ω Resistor
Cathode (-)   →    GND
```

### Power Supply

```
Vehicle 12V Battery
        ↓
    [Fuse 5A]
        ↓
    [12V to 5V Converter (3A)]
        ├─→ 5V Rail
        │   ├─→ ESP32 VIN
        │   ├─→ GSM VCC
        │   ├─→ Relay VCC
        │   └─→ Buzzer VCC
        │
        └─→ 3.3V Rail (from ESP32)
            ├─→ GPS VCC
            └─→ Vibration Sensor VCC
```

### Schematic Diagram

```
                    ┌─────────────┐
                    │   ESP32     │
                    │             │
    GPS Module      │  GPIO 16 ◄──┼── GPS TX
    ┌────────┐      │  GPIO 17 ──►┼── GPS RX
    │ NEO-6M │      │             │
    │        │      │  GPIO 26 ◄──┼── GSM TX
    │  TX ───┼──────┤  GPIO 27 ──►┼── GSM RX
    │  RX ───┼──────┤             │
    │  VCC───┼──────┤  3.3V       │
    │  GND───┼──────┤  GND        │
    └────────┘      │             │
                    │  GPIO 14 ◄──┼── Vibration Sensor
    GSM Module      │  GPIO 12 ──►┼── Relay
    ┌────────┐      │  GPIO 13 ──►┼── Buzzer
    │SIM800L │      │  GPIO 2  ──►┼── LED
    │        │      │             │
    │  TX ───┼──────┤  VIN ◄──────┼── 5V Supply
    │  RX ───┼──────┤  GND ───────┼── Ground
    │  VCC───┼──────┤             │
    │  GND───┼──────┤             │
    └────────┘      └─────────────┘
```

### Relay Connection to Vehicle

```
Vehicle Ignition System:

    Battery (+12V)
         │
         ├─────────────┐
         │             │
    [Ignition         │
      Switch]      [Relay COM]
         │             │
         └─────────────┤
                   [Relay NO]
                       │
                  [Starter/Ignition]
                       │
                     Ground

Relay Control:
    ESP32 GPIO 12 → Relay IN
    When HIGH: Relay closes, ignition active
    When LOW: Relay opens, ignition off
```

### PCB Layout Recommendations

1. **Separate Power Planes**
   - 5V plane for high-current devices
   - 3.3V plane for logic

2. **Component Placement**
   - GPS away from GSM (interference)
   - Capacitors near power pins
   - Short traces for high-frequency signals

3. **Grounding**
   - Single ground plane
   - Star grounding for power
   - Separate analog/digital grounds if possible

### Cable Specifications

| Connection | Wire Gauge | Max Length |
|------------|-----------|------------|
| Power (12V) | 18 AWG | 3m |
| GPS | 24-26 AWG | 1m |
| GSM | 22-24 AWG | 0.5m |
| Sensors | 24-26 AWG | 2m |
| Relay | 20-22 AWG | 1m |

### Safety Considerations

1. **Fuse Protection**
   - 5A fuse on 12V input
   - Protects against short circuits

2. **Reverse Polarity Protection**
   - Add diode on 12V input
   - Prevents damage from wrong connection

3. **ESD Protection**
   - Use ESD-safe handling
   - Add TVS diodes on exposed pins

4. **Heat Management**
   - Ensure adequate ventilation
   - Heat sink for voltage regulator if needed
   - Keep away from hot engine parts

### Testing Points

Mark these test points on your PCB:

- **TP1:** 12V Input
- **TP2:** 5V Rail
- **TP3:** 3.3V Rail
- **TP4:** GPS TX
- **TP5:** GSM TX
- **TP6:** Ground

### Troubleshooting

**No Power:**
- Check fuse
- Verify 12V input
- Test voltage regulator output

**GPS Not Working:**
- Verify 3.3V at GPS VCC
- Check TX/RX connections (crossed)
- Ensure antenna has clear view

**GSM Not Working:**
- Check 5V supply (needs 2A)
- Verify SIM card inserted
- Test with separate power supply

**Relay Not Switching:**
- Measure voltage at GPIO 12
- Check relay coil resistance
- Verify 5V at relay VCC

**Interference Issues:**
- Separate GPS and GSM
- Add ferrite beads on power lines
- Use shielded cables if needed
