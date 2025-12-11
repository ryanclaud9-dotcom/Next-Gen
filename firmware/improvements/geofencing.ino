// Geofencing Module
// Add this to your main firmware

struct GeoFence {
  double centerLat;
  double centerLng;
  double radiusMeters;
  bool enabled;
  String name;
};

GeoFence homeFence = {14.5995, 120.9842, 500.0, true, "Home"};  // Example: 500m radius
bool wasInsideFence = true;

double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
  // Haversine formula
  double R = 6371000; // Earth radius in meters
  double dLat = (lat2 - lat1) * PI / 180.0;
  double dLon = (lon2 - lon1) * PI / 180.0;
  
  double a = sin(dLat/2) * sin(dLat/2) +
             cos(lat1 * PI / 180.0) * cos(lat2 * PI / 180.0) *
             sin(dLon/2) * sin(dLon/2);
  
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

void checkGeofence() {
  if (!gps.location.isValid() || !homeFence.enabled) return;
  
  double distance = calculateDistance(
    gps.location.lat(), gps.location.lng(),
    homeFence.centerLat, homeFence.centerLng
  );
  
  bool isInside = distance <= homeFence.radiusMeters;
  
  // Detect fence breach
  if (wasInsideFence && !isInside) {
    String msg = "ALERT: Vehicle left " + homeFence.name + " zone!";
    logEvent(msg);
    sendPushNotification("Geofence Alert", msg);
    sendSMS(msg + "\nDistance: " + String(distance) + "m");
  } else if (!wasInsideFence && isInside) {
    String msg = "Vehicle returned to " + homeFence.name + " zone";
    logEvent(msg);
  }
  
  wasInsideFence = isInside;
  
  // Update Firebase
  FirebaseJson json;
  json.set("distance", distance);
  json.set("inside", isInside);
  json.set("fence", homeFence.name);
  
  String path = String("/devices/") + DEVICE_ID + "/geofence";
  Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json);
}

// Add to loop():
// if (gps.location.isUpdated()) {
//   checkGeofence();
// }
