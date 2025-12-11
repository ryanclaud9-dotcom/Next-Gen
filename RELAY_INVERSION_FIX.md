# 🔄 Relay Inversion Fix

## Issue: START Button Stops Engine, STOP Button Starts Engine

### 🔍 Root Cause
The relay logic configuration doesn't match your actual relay wiring. This is a common issue when using different relay contact types (NO vs NC).

### ✅ Solution Applied

**Changed relay configuration in `config.h`:**

```cpp
// OLD (causing inverted behavior):
#define IGNITION_SWITCH_INVERTED true   
#define IGNITION_COIL_INVERTED false    

// NEW (fixed):
#define IGNITION_SWITCH_INVERTED false  
#define IGNITION_COIL_INVERTED true     
```

### 🎯 Expected Behavior After Fix

#### START Command (SMS: `1234 START` or Dashboard START):
- ✅ Engine should START/allow starting
- ✅ Relays activate to enable ignition
- ✅ DS1 LED turns ON

#### STOP Command (SMS: `1234 STOP` or Dashboard STOP):
- ✅ Engine should STOP/prevent starting  
- ✅ Relays deactivate to disable ignition
- ✅ DS1 LED turns OFF

### 🧪 How to Test

1. **Upload the updated firmware** with the fixed relay configuration
2. **Click START button** on dashboard → Engine should start/allow starting
3. **Click STOP button** on dashboard → Engine should stop/prevent starting
4. **Test SMS**: Send `1234 START` → Should start engine
5. **Test SMS**: Send `1234 STOP` → Should stop engine

### 📊 Serial Monitor Output

You should now see correct behavior:
```
// When clicking START:
🚗 Firebase COMMAND: Starting engine...
🔌 Ignition Coil Relay (GPIO 13): ENABLED (NC logic - wrote LOW)
🔌 Ignition Switch Relay (GPIO 12): ENABLED (NO logic - wrote HIGH)
🔆 LED (GPIO 2): ON (wrote LOW to GPIO)

// When clicking STOP:
🛑 Firebase COMMAND: Stopping engine...
🔌 Ignition Coil Relay (GPIO 13): DISABLED (NC logic - wrote HIGH)
🔌 Ignition Switch Relay (GPIO 12): DISABLED (NO logic - wrote LOW)
🔆 LED (GPIO 2): OFF (wrote HIGH to GPIO)
```

### 🔧 Understanding Relay Types

#### NO (Normally Open) Contacts:
- **OFF state**: No connection (open circuit)
- **ON state**: Connected (closed circuit)
- **Control**: HIGH = Connected, LOW = Disconnected

#### NC (Normally Closed) Contacts:
- **OFF state**: Connected (closed circuit)  
- **ON state**: No connection (open circuit)
- **Control**: HIGH = Disconnected, LOW = Connected

### 🎯 Your Relay Configuration

Based on the fix applied, your relays are wired as:
- **Ignition Switch Relay (GPIO 12)**: NO contacts
- **Ignition Coil Relay (GPIO 13)**: NC contacts

### 🚨 Alternative Hardware Fix

If you prefer to change the wiring instead of the code:

#### Option 1: Change Relay Contacts
- Move wires from NC to NO (or vice versa) on the relay module
- Revert the code changes

#### Option 2: Use Different Relay Pins
- Connect to different relay outputs with the desired contact type
- Update pin definitions in config.h

### ✅ Verification

After uploading the fix:
- **START** should start the engine ✅
- **STOP** should stop the engine ✅  
- **SMS commands** should work correctly ✅
- **Dashboard controls** should work correctly ✅

The relay behavior should now be correct! 🎯