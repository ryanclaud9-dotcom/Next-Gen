// Firebase Configuration
const firebaseConfig = {
    apiKey: "AIzaSyBvpk3feAd5yfXtSYL3zUQaf6nlMRKAJa8",
    authDomain: "next-gen-b819e.firebaseapp.com",
    databaseURL: "https://next-gen-b819e-default-rtdb.firebaseio.com",
    projectId: "next-gen-b819e",
    storageBucket: "next-gen-b819e.firebasestorage.app",
    messagingSenderId: "647211877550",
    appId: "1:647211877550:web:45fd8ccd1f628519bdd93f",
    measurementId: "G-3BJ9DRCM1N"
};

// Initialize Firebase
firebase.initializeApp(firebaseConfig);

// Get Firebase services
const auth = firebase.auth();
const database = firebase.database();

// Device ID (should match ESP32 config)
const DEVICE_ID = "vehicle_001";
