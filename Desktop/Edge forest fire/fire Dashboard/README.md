# 🔥 FireSense — Local IoT Fire Detection Dashboard

A fully local replacement for ThingSpeak.  
ESP32 → Flask (SQLite) → Real-time HTML dashboard.

---

## Project Structure

```
fire_dashboard/
├── app.py              ← Flask backend
├── requirements.txt    ← Python dependencies
├── fire_data.db        ← SQLite DB (auto-created on first run)
├── esp32_sender.ino    ← Arduino sketch for ESP32
└── static/
    └── index.html      ← Dashboard frontend
```

---

## 1 — Python Backend Setup

### Requirements
- Python 3.9+

### Install dependencies

```bash
pip install -r requirements.txt
```

### Run the server

```bash
python app.py
```

The server starts on **http://0.0.0.0:5000**  
Open **http://localhost:5000** in your browser to view the dashboard.

---

## 2 — API Endpoints

| Method | Endpoint      | Description                                  |
|--------|---------------|----------------------------------------------|
| POST   | `/data`        | Receive JSON from ESP32, store in SQLite      |
| GET    | `/data`        | Return latest reading + last 100 history rows |
| DELETE | `/data/clear`  | Wipe all records (testing)                   |
| GET    | `/`            | Serve the dashboard HTML                     |

### POST /data — expected JSON

```json
{
  "temperature": 30.0,
  "humidity": 60.0,
  "smoke": 1200,
  "flame": 0,
  "fireProb": 0.7
}
```

---

## 3 — ESP32 Setup

1. Open `esp32_sender.ino` in the Arduino IDE.
2. Install required libraries via the Library Manager:
   - **DHT sensor library** (Adafruit)
   - **ArduinoJson** (Benoit Blanchon)
3. Edit the top of the sketch:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   const char* SERVER_URL    = "http://<YOUR_PC_LAN_IP>:5000/data";
   ```
4. Select **ESP32 Dev Module** as the board.
5. Upload the sketch and open the Serial Monitor at 115200 baud.

> **Finding your PC's LAN IP:**  
> Windows: `ipconfig` → IPv4 Address  
> Linux/macOS: `ip addr` or `ifconfig`

---

## 4 — Fire Detection Logic

| Condition | Threshold | Weight |
|-----------|-----------|--------|
| Flame sensor | `flame == 1` | ⚠ immediate alert |
| ML fire probability | `fireProb >= 0.50` | ⚠ alert |
| Smoke level (MQ-4) | `smoke >= 1000` | ⚠ alert |

Any one condition triggers the **🔥 FIRE DETECTED** banner.

---

## 5 — Testing Without an ESP32

Send a test POST with `curl`:

```bash
# Safe reading
curl -X POST http://localhost:5000/data \
  -H "Content-Type: application/json" \
  -d '{"temperature":28,"humidity":55,"smoke":300,"flame":0,"fireProb":0.1}'

# Fire scenario
curl -X POST http://localhost:5000/data \
  -H "Content-Type: application/json" \
  -d '{"temperature":75,"humidity":20,"smoke":1500,"flame":1,"fireProb":0.92}'
```

---

## 6 — Dashboard Features

- **5 metric cards** — Temperature, Humidity, Smoke, Fire Probability, Flame Status
- **4 real-time Chart.js graphs** — rolling 60-point history
- **Auto-refresh** every 2.5 seconds (no page reload)
- **🔥 FIRE DETECTED banner** with specific trigger reasons
- **Historical data seeded** from database on first page load
