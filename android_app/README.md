# KAOTI Ge-LAB PRO — Companion Android Camera App

Native Kotlin + CameraX companion app for **KAOTI Ge-LAB PRO (Rev 4.0)**.
Polls the local monitoring server (`GET /api/pending-photo`) every 2 seconds, vibrates/beeps when a new transistor test arrives, displays the exact characterization hash overlay on the live viewfinder, and uploads compressed JPEGs in one tap (`POST /api/tests/<hash>/photo`).

## 3-Step Build & Install

1. **Start the Web Server on your Mac/PC**:
   ```bash
   cd ../webapp
   python3 server.py
   ```
   Note the `LAN/Phone: http://<YOUR_IP>:8080` address printed at startup.

2. **Open & Build in Android Studio**:
   - Open the `android_app/` folder in **Android Studio** (Jellyfish / Koala / Ladybug or newer).
   - Wait for Gradle sync to finish, connect your Android phone via USB (with **USB Debugging** enabled), and click **Run ▶** (or run `./gradlew installDebug` if Gradle wrapper is initialized).

3. **Pair to Your Lab Server**:
   - Ensure your phone is on the same Wi-Fi LAN as the Mac/PC running `server.py`.
   - Enter `http://<YOUR_IP>:8080` in the top URL bar and tap **Save**.
   - Test any transistor on the ESP32-S3 Parametric Engine — the phone will alert and prompt you to snap & upload!
