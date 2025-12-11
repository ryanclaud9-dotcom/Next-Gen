let map;
let marker;
let fullMap;
let fullMapMarker;
let currentUser = null;
let routePolyline = null;
let speedLimit = 80;

// Check authentication state
auth.onAuthStateChanged((user) => {
    if (user) {
        currentUser = user;
        showDashboard();
        
        // Initialize maps with multiple attempts
        setTimeout(() => {
            console.log('First map initialization attempt...');
            initializeMap();
        }, 500);
        
        setTimeout(() => {
            console.log('Second map initialization attempt...');
            if (!map || !marker) {
                initializeMap();
            }
        }, 2000);
        
        startRealtimeListeners();
    } else {
        showLogin();
    }
});

function showLogin() {
    document.getElementById('login-container').style.display = 'block';
    document.getElementById('dashboard-container').style.display = 'none';
}

function showDashboard() {
    document.getElementById('login-container').style.display = 'none';
    document.getElementById('dashboard-container').style.display = 'flex';
    updateSidebarUser();
    
    // Initialize map after dashboard is visible
    setTimeout(() => {
        if (!map) {
            initializeMap();
        }
    }, 500);
}

function showLogin() {
    document.getElementById('login-form').style.display = 'block';
    document.getElementById('register-form').style.display = 'none';
}

function showRegister() {
    document.getElementById('login-form').style.display = 'none';
    document.getElementById('register-form').style.display = 'block';
}

function login() {
    const email = document.getElementById('login-email').value;
    const password = document.getElementById('login-password').value;
    const errorElement = document.getElementById('login-error');
    
    if (!email || !password) {
        errorElement.textContent = 'Please enter email and password';
        return;
    }
    
    auth.signInWithEmailAndPassword(email, password)
        .then(() => {
            errorElement.textContent = '';
        })
        .catch((error) => {
            errorElement.textContent = error.message;
        });
}

function register() {
    const email = document.getElementById('register-email').value;
    const password = document.getElementById('register-password').value;
    const confirmPassword = document.getElementById('register-confirm').value;
    const errorElement = document.getElementById('register-error');
    
    // Validation
    if (!email || !password || !confirmPassword) {
        errorElement.textContent = 'Please fill in all fields';
        return;
    }
    
    if (password.length < 6) {
        errorElement.textContent = 'Password must be at least 6 characters';
        return;
    }
    
    if (password !== confirmPassword) {
        errorElement.textContent = 'Passwords do not match';
        return;
    }
    
    // Create user
    auth.createUserWithEmailAndPassword(email, password)
        .then((userCredential) => {
            errorElement.textContent = '';
            console.log('User registered:', userCredential.user.email);
            // Automatically logged in after registration
        })
        .catch((error) => {
            errorElement.textContent = error.message;
        });
}

function logout() {
    auth.signOut();
}

function initializeMap() {
    console.log('Initializing maps...');
    
    // Wait for map container to be visible
    setTimeout(() => {
        // Initialize overview map
        const mapContainer = document.getElementById('map');
        if (!map && mapContainer) {
            console.log('Creating overview map...');
            map = L.map('map', {
                zoomControl: true,
                attributionControl: true,
                preferCanvas: true
            }).setView([14.5995, 120.9842], 13);
            
            // Add tile layer with better styling
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                attribution: '© OpenStreetMap contributors',
                maxZoom: 19,
                minZoom: 3
            }).addTo(map);
            
            addVehicleMarker(map);
            console.log('Overview map initialized successfully');
        } else if (!mapContainer) {
            console.warn('Map container #map not found');
        }
        
        // Initialize full-screen map
        const fullMapContainer = document.getElementById('map-full');
        if (!fullMap && fullMapContainer) {
            console.log('Creating full-screen map...');
            fullMap = L.map('map-full', {
                zoomControl: true,
                attributionControl: true,
                preferCanvas: true
            }).setView([14.5995, 120.9842], 15);
            
            // Add tile layer
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                attribution: '© OpenStreetMap contributors',
                maxZoom: 19,
                minZoom: 3
            }).addTo(fullMap);
            
            addVehicleMarker(fullMap);
            console.log('Full-screen map initialized successfully');
        } else if (!fullMapContainer) {
            console.warn('Map container #map-full not found');
        }
    }, 100);
}

function addVehicleMarker(mapInstance) {
    console.log('Adding vehicle marker to map instance:', mapInstance);
    
    // Determine if this is mobile for enhanced visibility
    const isMobile = window.innerWidth <= 768;
    const markerSize = isMobile ? 55 : 45; // Larger marker on mobile
    const iconSize = isMobile ? 28 : 24; // Larger icon on mobile
    
    console.log(`📱 Creating ${isMobile ? 'MOBILE' : 'DESKTOP'} marker (${markerSize}px)`);
    
    // Create custom motorcycle marker icon (enhanced for mobile)
    const vehicleIcon = L.divIcon({
        className: 'custom-vehicle-marker',
        html: `
            <div style="
                background: linear-gradient(135deg, #6366f1, #8b5cf6);
                width: ${markerSize}px;
                height: ${markerSize}px;
                border-radius: 50%;
                display: flex;
                align-items: center;
                justify-content: center;
                box-shadow: 0 8px 25px rgba(99, 102, 241, 0.6);
                border: 4px solid white;
                animation: markerPulse 2s infinite;
                position: relative;
                z-index: 10000;
                transform: translateZ(0);
                will-change: transform;
                ${isMobile ? 'filter: drop-shadow(0 4px 8px rgba(0,0,0,0.4));' : ''}
            ">
                <svg width="${iconSize}" height="${iconSize}" viewBox="0 0 24 24" fill="white" style="filter: drop-shadow(0 2px 4px rgba(0,0,0,0.4));">
                    <path d="M12 2L13.09 8.26L22 9L13.09 9.74L12 16L10.91 9.74L2 9L10.91 8.26L12 2Z"/>
                    <path d="M5 15.5A2.5 2.5 0 1 0 7.5 18A2.5 2.5 0 0 0 5 15.5Z"/>
                    <path d="M16.5 15.5A2.5 2.5 0 1 0 19 18A2.5 2.5 0 0 0 16.5 15.5Z"/>
                    <path d="M8 12H16L15 10H9L8 12Z"/>
                    <circle cx="5" cy="15.5" r="1" fill="white"/>
                    <circle cx="16.5" cy="15.5" r="1" fill="white"/>
                </svg>
            </div>
        `,
        iconSize: [markerSize, markerSize],
        iconAnchor: [markerSize/2, markerSize/2]
    });
    
    // Get current GPS location or use default
    let initialLocation = [14.5995, 120.9842]; // Default location
    
    // Try to get current GPS location immediately
    database.ref(`/devices/${DEVICE_ID}/location`).once('value', (snapshot) => {
        const data = snapshot.val();
        if (data && data.latitude && data.longitude && data.latitude !== 0 && data.longitude !== 0) {
            initialLocation = [data.latitude, data.longitude];
            console.log(`📍 Using GPS location for marker: ${initialLocation}`);
            
            // Update marker position if it was already created
            if (newMarker) {
                newMarker.setLatLng(initialLocation);
                mapInstance.setView(initialLocation, isMobile ? 17 : 15);
                console.log(`📱 Marker repositioned to GPS location`);
            }
        }
    });
    
    // Add marker to the map instance
    const newMarker = L.marker(initialLocation, { icon: vehicleIcon }).addTo(mapInstance);
    newMarker.bindPopup('<strong>🏍️ Motorcycle Location</strong><br>Waiting for GPS data...');
    
    console.log(`📍 Marker created at: ${initialLocation}`);
    
    // Store markers for updates
    if (mapInstance === map) {
        marker = newMarker;
        console.log('Overview map marker created and stored');
    } else if (mapInstance === fullMap) {
        fullMapMarker = newMarker;
        console.log('Full-screen map marker created and stored');
    }
    
    // Add CSS for marker animation (only once)
    if (!document.getElementById('marker-animation-style')) {
        const style = document.createElement('style');
        style.id = 'marker-animation-style';
        style.textContent = `
            @keyframes markerPulse {
                0%, 100% { transform: scale(1); }
                50% { transform: scale(1.1); }
            }
        `;
        document.head.appendChild(style);
    }
}

function startRealtimeListeners() {
    // Listen to location updates
    database.ref(`/devices/${DEVICE_ID}/location`).on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            updateLocation(data);
        }
    });
    
    // Listen to status updates
    database.ref(`/devices/${DEVICE_ID}/status`).on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            updateStatus(data);
        }
    });
    

    
    // Listen to geofence updates
    database.ref(`/devices/${DEVICE_ID}/geofence`).on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            updateGeofenceDisplay(data);
        }
    });
    
    // Listen to trip stats
    database.ref(`/devices/${DEVICE_ID}/stats`).on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            updateStatsDisplay(data);
        }
    });
    
    // Listen to events
    database.ref(`/devices/${DEVICE_ID}/events`).limitToLast(10).on('value', (snapshot) => {
        const events = [];
        snapshot.forEach((child) => {
            events.push(child.val());
        });
        updateEventsLog(events.reverse());
    });
    
    // Listen to notifications
    database.ref(`/devices/${DEVICE_ID}/notifications`).limitToLast(10).on('value', (snapshot) => {
        const notifications = [];
        snapshot.forEach((child) => {
            notifications.push(child.val());
        });
        updateNotificationsLog(notifications.reverse());
        
        // Show browser notification for new alerts
        if (notifications.length > 0) {
            const latest = notifications[0];
            if (Notification.permission === "granted") {
                new Notification(latest.title, {
                    body: latest.body,
                    icon: '🚗'
                });
            }
        }
    });
    
    // Load speed limit setting
    database.ref(`/devices/${DEVICE_ID}/settings/speedLimit`).once('value', (snapshot) => {
        if (snapshot.val()) {
            speedLimit = snapshot.val();
            document.getElementById('speed-limit').value = speedLimit;
        }
    });
}

function updateLocation(data) {
    console.log('🔄 updateLocation called with data:', data);
    
    // Validate GPS data
    if (!data || !data.latitude || !data.longitude || data.latitude === 0 || data.longitude === 0) {
        // Only log once every 30 seconds to avoid spam
        if (!window.lastGPSWarning || Date.now() - window.lastGPSWarning > 30000) {
            console.warn('⚠️ GPS data not available - waiting for satellite fix...');
            window.lastGPSWarning = Date.now();
        }
        
        // Update UI to show GPS status
        const latElement = document.getElementById('latitude');
        const lngElement = document.getElementById('longitude');
        if (latElement) latElement.textContent = 'Acquiring...';
        if (lngElement) lngElement.textContent = 'Acquiring...';
        
        const latMapElement = document.getElementById('latitude-map');
        const lngMapElement = document.getElementById('longitude-map');
        if (latMapElement) latMapElement.textContent = 'Acquiring...';
        if (lngMapElement) lngMapElement.textContent = 'Acquiring...';
        
        return;
    }
    
    // Clear the warning flag when we get valid data
    window.lastGPSWarning = null;
    
    // Update GPS coordinates (overview)
    const latElement = document.getElementById('latitude');
    const lngElement = document.getElementById('longitude');
    if (latElement) latElement.textContent = data.latitude.toFixed(6);
    if (lngElement) lngElement.textContent = data.longitude.toFixed(6);
    
    // Update map info panel
    const latMapElement = document.getElementById('latitude-map');
    const lngMapElement = document.getElementById('longitude-map');
    if (latMapElement) latMapElement.textContent = data.latitude.toFixed(6);
    if (lngMapElement) lngMapElement.textContent = data.longitude.toFixed(6);
    
    // Update speed from GPS module
    const gpsSpeed = data.speed || 0;
    const speedMapElement = document.getElementById('speed-map');
    if (speedMapElement) speedMapElement.textContent = gpsSpeed.toFixed(1);
    
    // Update satellites
    const satellites = data.satellites || 0;
    const satMiniElement = document.getElementById('satellites-mini');
    const satMapElement = document.getElementById('satellites-map');
    if (satMiniElement) satMiniElement.textContent = satellites;
    if (satMapElement) satMapElement.textContent = satellites;
    
    // Update speed gauge (from GPS)
    const speedElement = document.getElementById('current-speed');
    if (speedElement) {
        speedElement.textContent = Math.round(gpsSpeed);
        
        // Color code based on speed limit
        const speedGauge = speedElement.closest('.speed-gauge');
        if (speedGauge) {
            if (gpsSpeed > speedLimit) {
                speedGauge.style.background = 'linear-gradient(135deg, #ef4444, #dc2626)';
            } else if (gpsSpeed > speedLimit * 0.8) {
                speedGauge.style.background = 'linear-gradient(135deg, #f59e0b, #d97706)';
            } else {
                speedGauge.style.background = 'linear-gradient(135deg, #6366f1, #8b5cf6, #ec4899)';
            }
        }
        
        // Check for speed violations
        checkSpeedFromGPS(gpsSpeed);
    }
    
    // Update map markers and center
    const latlng = [data.latitude, data.longitude];
    console.log('📍 Processing GPS coordinates:', latlng);
    
    // Initialize maps if they don't exist - with multiple attempts
    if (!map || !marker || !fullMap || !fullMapMarker) {
        console.log('🗺️ Maps missing, initializing...', {
            map: !!map,
            marker: !!marker, 
            fullMap: !!fullMap,
            fullMapMarker: !!fullMapMarker
        });
        
        // Force initialization
        setTimeout(() => {
            initializeMap();
            
            // Try again after a short delay if still missing
            setTimeout(() => {
                if (!map || !marker || !fullMap || !fullMapMarker) {
                    console.log('🔄 Second initialization attempt...');
                    initializeMap();
                }
            }, 1000);
        }, 100);
    }
    
    // Update overview map marker
    if (marker && map) {
        console.log('✅ Updating overview map marker to:', latlng);
        marker.setLatLng(latlng);
        marker.bindPopup(`
            <div style="text-align: center; padding: 10px; min-width: 200px;">
                <strong>🏍️ Motorcycle Location</strong><br>
                <span style="font-size: 13px; color: #6366f1; font-weight: 600;">Speed: ${gpsSpeed.toFixed(1)} km/h</span><br>
                <span style="font-size: 12px; color: #059669;">📡 ${satellites} satellites</span><br>
                <span style="font-size: 11px; color: #666; margin-top: 4px; display: block;">
                    📍 ${data.latitude.toFixed(6)}, ${data.longitude.toFixed(6)}
                </span>
            </div>
        `);
        
        // Ensure marker is visible by centering occasionally
        if (Math.random() < 0.3) { // 30% chance to recenter
            map.setView(latlng, map.getZoom() || 15);
            console.log('🎯 Centered overview map on vehicle');
        }
    } else {
        console.warn('❌ Overview map or marker not available:', {
            map: !!map,
            marker: !!marker
        });
    }
    
    // Update full-screen map marker
    if (fullMapMarker && fullMap) {
        console.log('✅ Updating full-screen map marker to:', latlng);
        fullMapMarker.setLatLng(latlng);
        fullMapMarker.bindPopup(`
            <div style="text-align: center; padding: 12px; min-width: 220px;">
                <strong>🏍️ Motorcycle Location</strong><br>
                <span style="font-size: 14px; color: #6366f1; font-weight: 600;">Speed: ${gpsSpeed.toFixed(1)} km/h</span><br>
                <span style="font-size: 13px; color: #059669;">📡 ${satellites} satellites connected</span><br>
                <span style="font-size: 12px; color: #666; margin-top: 6px; display: block;">
                    📍 Lat: ${data.latitude.toFixed(6)}<br>
                    📍 Lng: ${data.longitude.toFixed(6)}
                </span>
            </div>
        `);
        
        // Auto-center full map when on map tab
        const mapTab = document.getElementById('tab-map');
        if (mapTab && mapTab.classList.contains('active')) {
            fullMap.setView(latlng, fullMap.getZoom() || 16);
            console.log('🎯 Centered full-screen map on vehicle');
        }
    } else {
        console.warn('❌ Full-screen map or marker not available:', {
            fullMap: !!fullMap,
            fullMapMarker: !!fullMapMarker
        });
    }
    
    // CRITICAL: Force map refresh and marker visibility on mobile devices
    if (window.innerWidth <= 768) {
        console.log('📱 MOBILE: Detected mobile device, applying fixes...');
        
        // Immediate marker positioning for mobile
        if (marker && map) {
            console.log('📱 MOBILE: Positioning overview marker immediately');
            marker.setLatLng(latlng);
            map.setView(latlng, 16); // Higher zoom for mobile
            
            // Force marker to be visible
            setTimeout(() => {
                map.panTo(latlng);
                console.log('📱 MOBILE: Overview map panned to marker');
            }, 100);
        }
        
        if (fullMapMarker && fullMap) {
            console.log('📱 MOBILE: Positioning full-screen marker immediately');
            fullMapMarker.setLatLng(latlng);
            fullMap.setView(latlng, 17); // Even higher zoom for mobile full-screen
            
            // Force marker to be visible
            setTimeout(() => {
                fullMap.panTo(latlng);
                console.log('📱 MOBILE: Full-screen map panned to marker');
            }, 100);
        }
        
        // Refresh map sizes after positioning
        setTimeout(() => {
            if (map) {
                map.invalidateSize();
                map.setView(latlng, 16); // Re-center after size refresh
                console.log('📱 MOBILE: Overview map size refreshed and re-centered');
            }
            if (fullMap) {
                fullMap.invalidateSize();
                fullMap.setView(latlng, 17); // Re-center after size refresh
                console.log('📱 MOBILE: Full-screen map size refreshed and re-centered');
            }
        }, 300);
        
        // Final positioning check for mobile
        setTimeout(() => {
            if (marker && map) {
                map.setView(latlng, 16);
                console.log('📱 MOBILE: Final overview map positioning');
            }
            if (fullMapMarker && fullMap) {
                const mapTab = document.getElementById('tab-map');
                if (mapTab && mapTab.classList.contains('active')) {
                    fullMap.setView(latlng, 17);
                    console.log('📱 MOBILE: Final full-screen map positioning');
                }
            }
        }, 800);
    }
    
    // Log comprehensive GPS update info
    console.log('📊 GPS Update Complete:', {
        coordinates: `${data.latitude.toFixed(6)}, ${data.longitude.toFixed(6)}`,
        speed: `${gpsSpeed.toFixed(1)} km/h`,
        satellites: satellites,
        mapStatus: {
            overviewMap: !!map,
            overviewMarker: !!marker,
            fullMap: !!fullMap,
            fullMapMarker: !!fullMapMarker
        },
        isMobile: window.innerWidth <= 768
    });
}

function updateStatus(data) {
    const statusBadge = document.getElementById('system-status');
    
    // Show connection type
    const connectionType = data.connection || 'WiFi';
    const statusText = data.status.charAt(0).toUpperCase() + data.status.slice(1);
    statusBadge.textContent = `${statusText} (${connectionType})`;
    statusBadge.className = 'status-badge ' + data.status;
    
    // Update connection indicator color
    if (connectionType === 'GSM') {
        statusBadge.style.background = 'linear-gradient(135deg, #f59e0b, #d97706)';
    } else {
        statusBadge.style.background = 'linear-gradient(135deg, #10b981, #059669)';
    }
    
    document.getElementById('engine-status').textContent = data.engineRunning ? '🟢 Running' : '🔴 Stopped';
    document.getElementById('armed-status').textContent = data.systemArmed ? '🔒 Armed' : '🔓 Disarmed';
    // Handle timestamp - prefer lastUpdate string, fallback to timestamp conversion
    let displayTime;
    if (data.lastUpdate) {
        displayTime = data.lastUpdate;
    } else if (data.timestamp) {
        // Convert timestamp - if it's too small, it's probably Unix seconds, otherwise milliseconds
        const timestamp = data.timestamp;
        if (timestamp > 1000000000000) {
            // Looks like milliseconds
            displayTime = new Date(timestamp).toLocaleString();
        } else if (timestamp > 1000000000) {
            // Looks like Unix seconds
            displayTime = new Date(timestamp * 1000).toLocaleString();
        } else {
            // Fallback to current time
            displayTime = new Date().toLocaleString();
        }
    } else {
        displayTime = 'Unknown';
    }
    
    document.getElementById('last-update').textContent = displayTime;
    
    // Update arm button label
    const armLabel = document.getElementById('arm-label');
    if (armLabel) {
        armLabel.textContent = data.systemArmed ? 'Disarm' : 'Arm';
    }
    
    if (data.uptime) {
        const hours = Math.floor(data.uptime / 3600);
        const minutes = Math.floor((data.uptime % 3600) / 60);
        document.getElementById('uptime').textContent = `${hours}h ${minutes}m`;
    }
}

function updateEventsLog(events) {
    const logContainer = document.getElementById('events-log');
    logContainer.innerHTML = '';
    
    events.forEach(event => {
        const div = document.createElement('div');
        div.className = event.event.includes('ALERT') ? 'timeline-item alert' : 'timeline-item';
        div.innerHTML = `
            <div class="timeline-content">
                <div class="timeline-title">${event.event}</div>
                <div class="timeline-time">${formatTimestamp(event.timestamp)}</div>
            </div>
        `;
        logContainer.appendChild(div);
    });
}

function updateNotificationsLog(notifications) {
    const logContainer = document.getElementById('notifications-log');
    logContainer.innerHTML = '';
    
    notifications.forEach(notif => {
        const div = document.createElement('div');
        div.className = 'timeline-item';
        div.innerHTML = `
            <div class="timeline-content">
                <div class="timeline-title">${notif.title}</div>
                <div style="font-size: 13px; color: var(--text-secondary); margin: 4px 0;">${notif.body}</div>
                <div class="timeline-time">${formatTimestamp(notif.timestamp)}</div>
            </div>
        `;
        logContainer.appendChild(div);
    });
}

function confirmCommand(command, label) {
    if (confirm(`Are you sure you want to ${label}?`)) {
        sendCommand(command);
    }
}

function sendCommand(command) {
    const button = event.target.closest('button');
    button.classList.add('loading');
    button.disabled = true;
    
    database.ref(`/devices/${DEVICE_ID}/commands/pending`).set(command)
        .then(() => {
            console.log('Command sent:', command);
            
            // Show success feedback
            const originalHTML = button.innerHTML;
            button.innerHTML = '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3"><polyline points="20 6 9 17 4 12"></polyline></svg>';
            
            setTimeout(() => {
                button.innerHTML = originalHTML;
                button.classList.remove('loading');
                button.disabled = false;
            }, 2000);
        })
        .catch((error) => {
            console.error('Error sending command:', error);
            alert('Failed to send command. Please check your connection.');
            button.classList.remove('loading');
            button.disabled = false;
        });
}

// Toggle arm/disarm with visual feedback
function toggleArm() {
    const button = event.target.closest('button');
    const label = document.getElementById('arm-label');
    
    // Get current status
    database.ref(`/devices/${DEVICE_ID}/status/systemArmed`).once('value', (snapshot) => {
        const currentlyArmed = snapshot.val();
        const newCommand = currentlyArmed ? 'DISARM' : 'ARM';
        const newLabel = currentlyArmed ? 'Disarm' : 'Arm';
        
        if (confirm(`Are you sure you want to ${newLabel.toLowerCase()} the system?`)) {
            button.classList.add('loading');
            button.disabled = true;
            
            database.ref(`/devices/${DEVICE_ID}/commands/pending`).set(newCommand)
                .then(() => {
                    console.log('Command sent:', newCommand);
                    
                    // Update button label
                    label.textContent = currentlyArmed ? 'Arm' : 'Disarm';
                    
                    // Show success
                    setTimeout(() => {
                        button.classList.remove('loading');
                        button.disabled = false;
                    }, 2000);
                })
                .catch((error) => {
                    console.error('Error:', error);
                    alert('Failed to send command. Please check your connection.');
                    button.classList.remove('loading');
                    button.disabled = false;
                });
        }
    });
}



// Utility function to format timestamps properly
function formatTimestamp(timestamp) {
    if (!timestamp) return 'Unknown';
    
    // Handle different timestamp formats
    if (typeof timestamp === 'string') {
        // Already formatted string
        return timestamp;
    }
    
    // Convert timestamp - if it's too small, it's probably Unix seconds, otherwise milliseconds
    if (timestamp > 1000000000000) {
        // Looks like milliseconds
        return new Date(timestamp).toLocaleString();
    } else if (timestamp > 1000000000) {
        // Looks like Unix seconds
        return new Date(timestamp * 1000).toLocaleString();
    } else {
        // Fallback to current time
        return new Date().toLocaleString();
    }
}

// Geofencing
function updateGeofenceDisplay(data) {
    console.log('🔄 updateGeofenceDisplay called with data:', data);
    
    // Validate geofence data and provide defaults
    const fenceName = data.fence || data.name || 'Home Zone';
    const distance = (data.distance !== undefined && !isNaN(data.distance)) ? Math.round(data.distance) : 0;
    const inside = data.inside || false;
    
    // Create status text with proper validation
    let statusText;
    if (inside) {
        statusText = `Inside ${fenceName} ✓`;
    } else {
        statusText = `Outside ${fenceName} (${distance}m)`;
    }
    
    const statusColor = inside ? '#10b981' : '#ef4444';
    
    console.log('📍 Geofence status:', statusText);
    
    // Update all geofence status elements
    const statusElements = [
        document.getElementById('geofence-status'),
        document.getElementById('geofence-status-map')
    ];
    
    statusElements.forEach(element => {
        if (element) {
            element.textContent = statusText;
            element.style.color = statusColor;
            element.style.fontWeight = '600';
        }
    });
}

function setupGeofence() {
    const lat = prompt('Enter center latitude:', '14.5995');
    const lng = prompt('Enter center longitude:', '120.9842');
    const radius = prompt('Enter radius in meters:', '500');
    const name = prompt('Enter zone name:', 'Home');
    
    if (lat && lng && radius && name) {
        database.ref(`/devices/${DEVICE_ID}/geofence/config`).set({
            centerLat: parseFloat(lat),
            centerLng: parseFloat(lng),
            radiusMeters: parseFloat(radius),
            name: name,
            enabled: true
        }).then(() => {
            alert('Geofence configured! System will reboot to apply changes.');
            sendCommand('REBOOT');
        });
    }
}

// Speed Monitoring
function setSpeedLimit() {
    const limit = document.getElementById('speed-limit').value;
    speedLimit = parseInt(limit);
    
    if (speedLimit < 10 || speedLimit > 200) {
        alert('Please enter a speed limit between 10 and 200 km/h');
        return;
    }
    
    database.ref(`/devices/${DEVICE_ID}/settings/speedLimit`).set(speedLimit)
        .then(() => {
            alert('Speed limit set to ' + speedLimit + ' km/h\nSystem will apply changes automatically.');
            
            // Visual feedback
            const input = document.getElementById('speed-limit');
            input.style.borderColor = '#10b981';
            setTimeout(() => {
                input.style.borderColor = '';
            }, 2000);
        })
        .catch((error) => {
            alert('Failed to set speed limit: ' + error.message);
        });
}

// Check speed from GPS and alert if exceeded
function checkSpeedFromGPS(currentSpeed) {
    if (currentSpeed > speedLimit && currentSpeed > 10) {
        // Show visual alert
        const speedGauge = document.querySelector('.speed-gauge');
        if (speedGauge) {
            speedGauge.style.animation = 'shake 0.5s';
            setTimeout(() => {
                speedGauge.style.animation = '';
            }, 500);
        }
        
        // Browser notification
        if (Notification.permission === "granted") {
            new Notification('Speed Alert!', {
                body: `Vehicle exceeding speed limit: ${currentSpeed.toFixed(0)} km/h (Limit: ${speedLimit} km/h)`,
                icon: '🚗',
                tag: 'speed-alert'
            });
        }
    }
}

// Add shake animation
const shakeStyle = document.createElement('style');
shakeStyle.textContent = `
    @keyframes shake {
        0%, 100% { transform: translateX(0); }
        25% { transform: translateX(-10px); }
        75% { transform: translateX(10px); }
    }
`;
document.head.appendChild(shakeStyle);

// Trip Statistics
function updateStatsDisplay(data) {
    document.getElementById('distance-today').textContent = data.distanceToday.toFixed(2) + ' km';
    document.getElementById('max-speed').textContent = Math.round(data.maxSpeed) + ' km/h';
}

// Map Functions
function centerMap() {
    database.ref(`/devices/${DEVICE_ID}/location`).once('value', (snapshot) => {
        const data = snapshot.val();
        if (data && data.latitude && data.longitude) {
            const latlng = [data.latitude, data.longitude];
            
            // Center overview map
            if (map) {
                map.setView(latlng, 15);
            }
            
            // Center full-screen map
            if (fullMap) {
                fullMap.setView(latlng, 16);
            }
            
            console.log('Maps centered to:', latlng);
        }
    });
}

function loadRouteHistory() {
    const startTime = new Date().setHours(0, 0, 0, 0);
    
    database.ref(`/devices/${DEVICE_ID}/history`)
        .orderByChild('timestamp')
        .startAt(startTime)
        .once('value', (snapshot) => {
            let points = [];
            snapshot.forEach((child) => {
                const data = child.val();
                points.push([data.latitude, data.longitude]);
            });
            
            if (routePolyline) {
                map.removeLayer(routePolyline);
            }
            
            if (points.length > 0) {
                routePolyline = L.polyline(points, {
                    color: '#667eea',
                    weight: 4,
                    opacity: 0.7
                }).addTo(map);
                
                map.fitBounds(routePolyline.getBounds());
                alert(`Loaded ${points.length} location points from today`);
            } else {
                alert('No route history available for today');
            }
        });
}

function exportData() {
    database.ref(`/devices/${DEVICE_ID}/history`)
        .orderByChild('timestamp')
        .startAt(new Date().setHours(0, 0, 0, 0))
        .once('value', (snapshot) => {
            let csv = 'Timestamp,Latitude,Longitude,Speed,Altitude,Satellites\n';
            
            snapshot.forEach((child) => {
                const data = child.val();
                csv += `${new Date(data.timestamp).toISOString()},${data.latitude},${data.longitude},${data.speed},${data.altitude},${data.satellites}\n`;
            });
            
            const blob = new Blob([csv], { type: 'text/csv' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `vehicle_data_${new Date().toISOString().split('T')[0]}.csv`;
            a.click();
        });
}

// Tab Switching
function switchTab(tabName) {
    // Hide all tabs
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    
    // Remove active from all nav items
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
    });
    
    // Show selected tab
    document.getElementById('tab-' + tabName).classList.add('active');
    
    // Set active nav item
    event.target.closest('.nav-item').classList.add('active');
    
    // Initialize maps when switching to relevant tabs
    if (tabName === 'overview' || tabName === 'map') {
        // Initialize maps if not already done
        if (!map || !fullMap) {
            console.log('Initializing maps from tab switch to:', tabName);
            initializeMap();
        }
    }
    
    // Special handling for map tab
    if (tabName === 'map') {
        console.log('🗺️ Switching to map tab, ensuring maps are ready...');
        
        // Force map initialization if missing
        if (!fullMap || !fullMapMarker) {
            console.log('🔄 Full-screen map missing, initializing...');
            initializeMap();
        }
        
        // MOBILE-SPECIFIC MAP TAB HANDLING
        if (window.innerWidth <= 768) {
            console.log('📱 MOBILE: Switching to map tab, applying mobile fixes...');
            
            // Force immediate map refresh and positioning
            setTimeout(() => {
                if (fullMap) {
                    fullMap.invalidateSize();
                    console.log('📱 MOBILE: Full-screen map size invalidated');
                    
                    // Get current GPS location and center map
                    database.ref(`/devices/${DEVICE_ID}/location`).once('value', (snapshot) => {
                        const data = snapshot.val();
                        if (data && data.latitude && data.longitude && data.latitude !== 0 && data.longitude !== 0) {
                            const latlng = [data.latitude, data.longitude];
                            
                            // Force high zoom and center for mobile
                            fullMap.setView(latlng, 18); // Very high zoom for mobile
                            console.log('📱 MOBILE: Full-screen map centered at high zoom:', latlng);
                            
                            // Ensure marker exists and is positioned correctly
                            if (fullMapMarker) {
                                fullMapMarker.setLatLng(latlng);
                                console.log('📱 MOBILE: Full-screen marker positioned');
                                
                                // Force marker to be visible with pan
                                setTimeout(() => {
                                    fullMap.panTo(latlng);
                                    console.log('📱 MOBILE: Map panned to marker location');
                                }, 200);
                            } else {
                                // Create marker if missing
                                console.log('📱 MOBILE: Creating missing full-screen marker');
                                addVehicleMarker(fullMap);
                                if (fullMapMarker) {
                                    fullMapMarker.setLatLng(latlng);
                                }
                            }
                        } else {
                            // Use default location if no GPS data
                            const defaultLocation = [14.5995, 120.9842];
                            fullMap.setView(defaultLocation, 15);
                            console.log('📱 MOBILE: Using default location');
                        }
                    });
                }
            }, 100);
            
            // Additional mobile refresh cycles
            setTimeout(() => {
                if (fullMap) {
                    fullMap.invalidateSize();
                    console.log('📱 MOBILE: Second map refresh');
                }
            }, 500);
            
            setTimeout(() => {
                if (fullMap) {
                    fullMap.invalidateSize();
                    console.log('📱 MOBILE: Final map refresh');
                }
            }, 1000);
            
        } else {
            // Desktop handling
            setTimeout(() => {
                if (map) {
                    map.invalidateSize();
                    console.log('🔄 Overview map size invalidated');
                }
                if (fullMap) {
                    fullMap.invalidateSize();
                    console.log('🔄 Full-screen map size invalidated');
                    
                    // Force center on current location
                    database.ref(`/devices/${DEVICE_ID}/location`).once('value', (snapshot) => {
                        const data = snapshot.val();
                        if (data && data.latitude && data.longitude && data.latitude !== 0 && data.longitude !== 0) {
                            const latlng = [data.latitude, data.longitude];
                            fullMap.setView(latlng, 16);
                            console.log('🎯 Full-screen map centered on current location:', latlng);
                            
                            // Ensure marker exists and is positioned correctly
                            if (fullMapMarker) {
                                fullMapMarker.setLatLng(latlng);
                                console.log('✅ Full-screen marker positioned');
                            }
                        }
                    });
                }
            }, 200);
        }
    }
    
    // Sync event logs
    if (tabName === 'history') {
        document.getElementById('events-log-history').innerHTML = 
            document.getElementById('events-log').innerHTML;
        document.getElementById('notifications-log-history').innerHTML = 
            document.getElementById('notifications-log').innerHTML;
    }
}

// Refresh Data
function refreshData() {
    updateLocation();
    updateSystemStatus('online');
    updateBatteryStatus();
    
    // Visual feedback
    const btn = event.target.closest('.icon-btn');
    btn.style.transform = 'rotate(360deg)';
    setTimeout(() => {
        btn.style.transform = '';
    }, 600);
}

// Update user email in sidebar
function updateSidebarUser() {
    if (currentUser) {
        const email = currentUser.email;
        document.getElementById('user-email-sidebar').textContent = email;
    }
}

// Request notification permission
if (Notification.permission === "default") {
    Notification.requestPermission();
}

// Diagnostic function for debugging map issues
function diagnosticMapStatus() {
    console.log('🔍 === MAP DIAGNOSTIC REPORT ===');
    console.log('📱 Device Info:', {
        screenWidth: window.innerWidth,
        screenHeight: window.innerHeight,
        isMobile: window.innerWidth <= 768,
        userAgent: navigator.userAgent.substring(0, 50) + '...'
    });
    
    console.log('🗺️ Map Status:', {
        overviewMap: !!map,
        overviewMarker: !!marker,
        fullScreenMap: !!fullMap,
        fullScreenMarker: !!fullMapMarker,
        leafletLoaded: typeof L !== 'undefined'
    });
    
    console.log('📦 DOM Elements:', {
        mapContainer: !!document.getElementById('map'),
        fullMapContainer: !!document.getElementById('map-full'),
        mapContainerVisible: document.getElementById('map')?.offsetHeight > 0,
        fullMapContainerVisible: document.getElementById('map-full')?.offsetHeight > 0
    });
    
    console.log('🔥 Firebase Status:', {
        databaseConnected: !!database,
        authenticated: !!currentUser
    });
    
    // Test GPS data
    if (database) {
        database.ref(`/devices/${DEVICE_ID}/location`).once('value', (snapshot) => {
            const data = snapshot.val();
            console.log('📍 Latest GPS Data:', {
                hasData: !!data,
                latitude: data?.latitude,
                longitude: data?.longitude,
                isValidCoords: data?.latitude !== 0 && data?.longitude !== 0,
                satellites: data?.satellites,
                speed: data?.speed
            });
            
            if (data && data.latitude && data.longitude && data.latitude !== 0 && data.longitude !== 0) {
                console.log('✅ GPS data is valid, attempting marker update...');
                
                // Force marker update
                const latlng = [data.latitude, data.longitude];
                if (marker && map) {
                    marker.setLatLng(latlng);
                    map.setView(latlng, 15);
                    console.log('✅ Overview marker updated');
                }
                if (fullMapMarker && fullMap) {
                    fullMapMarker.setLatLng(latlng);
                    fullMap.setView(latlng, 16);
                    console.log('✅ Full-screen marker updated');
                }
            } else {
                console.log('❌ GPS data invalid or missing');
            }
        });
    }
    
    console.log('🔍 === END DIAGNOSTIC ===');
}

// Mobile marker fix function - ENHANCED
function forceMobileMarkerVisibility() {
    console.log('📱 FORCE: Forcing mobile marker visibility...');
    
    // Get current GPS location
    database.ref(`/devices/${DEVICE_ID}/location`).once('value', (snapshot) => {
        const data = snapshot.val();
        if (data && data.latitude && data.longitude && data.latitude !== 0 && data.longitude !== 0) {
            const latlng = [data.latitude, data.longitude];
            console.log('📱 FORCE: Using GPS coordinates:', latlng);
            
            // Force overview map
            if (map) {
                if (!marker) {
                    console.log('📱 FORCE: Creating missing overview marker');
                    addVehicleMarker(map);
                }
                if (marker) {
                    marker.setLatLng(latlng);
                    map.setView(latlng, window.innerWidth <= 768 ? 17 : 15);
                    map.panTo(latlng);
                    console.log('📱 FORCE: Overview marker positioned and visible');
                }
            }
            
            // Force full-screen map
            if (fullMap) {
                if (!fullMapMarker) {
                    console.log('📱 FORCE: Creating missing full-screen marker');
                    addVehicleMarker(fullMap);
                }
                if (fullMapMarker) {
                    fullMapMarker.setLatLng(latlng);
                    fullMap.setView(latlng, window.innerWidth <= 768 ? 18 : 16);
                    fullMap.panTo(latlng);
                    console.log('📱 FORCE: Full-screen marker positioned and visible');
                    
                    // Force popup to show on mobile
                    if (window.innerWidth <= 768) {
                        setTimeout(() => {
                            fullMapMarker.openPopup();
                            console.log('📱 FORCE: Popup opened on mobile');
                        }, 500);
                    }
                }
            }
            
            // Refresh map sizes
            setTimeout(() => {
                if (map) map.invalidateSize();
                if (fullMap) fullMap.invalidateSize();
                console.log('📱 FORCE: Maps refreshed');
            }, 200);
            
        } else {
            console.log('📱 FORCE: No valid GPS data, using default location');
            const defaultLatlng = [14.5995, 120.9842];
            
            if (map && marker) {
                marker.setLatLng(defaultLatlng);
                map.setView(defaultLatlng, 15);
            }
            if (fullMap && fullMapMarker) {
                fullMapMarker.setLatLng(defaultLatlng);
                fullMap.setView(defaultLatlng, 16);
            }
        }
    });
}

// Auto-run mobile fix when page loads
if (window.innerWidth <= 768) {
    // Auto-fix for mobile devices
    setTimeout(() => {
        console.log('📱 AUTO: Running mobile marker fix on page load');
        forceMobileMarkerVisibility();
    }, 3000);
    
    // Auto-fix when switching to map tab
    const originalSwitchTab = window.switchTab;
    window.switchTab = function(tabName) {
        if (originalSwitchTab) originalSwitchTab(tabName);
        
        if (tabName === 'map' && window.innerWidth <= 768) {
            setTimeout(() => {
                console.log('📱 AUTO: Running mobile marker fix on map tab switch');
                forceMobileMarkerVisibility();
            }, 500);
        }
    };
}

// Simple test function for immediate marker visibility
function showMarkerNow() {
    console.log('🚀 IMMEDIATE: Showing marker now...');
    
    // Use the GPS coordinates from your console logs
    const testCoords = [12.747614, 121.482103]; // Your actual GPS location
    
    if (fullMap) {
        // Remove existing marker if any
        if (fullMapMarker) {
            fullMap.removeLayer(fullMapMarker);
        }
        
        // Create new marker at exact GPS location
        const isMobile = window.innerWidth <= 768;
        const markerSize = isMobile ? 60 : 45; // Even larger for visibility
        
        const testIcon = L.divIcon({
            className: 'test-marker',
            html: `
                <div style="
                    background: linear-gradient(135deg, #ff0000, #ff6600);
                    width: ${markerSize}px;
                    height: ${markerSize}px;
                    border-radius: 50%;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    box-shadow: 0 10px 30px rgba(255, 0, 0, 0.8);
                    border: 5px solid white;
                    z-index: 99999;
                    position: relative;
                ">
                    <div style="color: white; font-size: ${isMobile ? '24px' : '20px'}; font-weight: bold;">🏍️</div>
                </div>
            `,
            iconSize: [markerSize, markerSize],
            iconAnchor: [markerSize/2, markerSize/2]
        });
        
        // Create marker at exact location
        fullMapMarker = L.marker(testCoords, { icon: testIcon }).addTo(fullMap);
        fullMapMarker.bindPopup(`
            <div style="text-align: center; padding: 15px; font-size: 16px;">
                <strong>🏍️ TEST MARKER</strong><br>
                <span style="color: #ff0000; font-weight: bold;">GPS: ${testCoords[0]}, ${testCoords[1]}</span><br>
                <span style="color: #666;">This should be visible!</span>
            </div>
        `);
        
        // Force high zoom and center
        fullMap.setView(testCoords, isMobile ? 19 : 17);
        fullMap.panTo(testCoords);
        
        // Open popup immediately
        setTimeout(() => {
            fullMapMarker.openPopup();
        }, 500);
        
        console.log('🚀 IMMEDIATE: Red test marker created at:', testCoords);
        console.log('🚀 IMMEDIATE: Map centered at zoom:', isMobile ? 19 : 17);
    } else {
        console.log('❌ IMMEDIATE: Full map not available');
    }
}

// Make functions available globally for testing
window.diagnosticMapStatus = diagnosticMapStatus;
window.forceMobileMarkerVisibility = forceMobileMarkerVisibility;
window.showMarkerNow = showMarkerNow;
