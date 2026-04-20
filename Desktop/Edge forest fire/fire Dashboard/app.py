"""
Fire Detection IoT Dashboard - Flask Backend
Receives sensor data from ESP32 and serves it to the frontend dashboard.
"""

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import sqlite3
import os
from datetime import datetime

app = Flask(__name__, static_folder="static")
CORS(app)  # Allow cross-origin requests from the frontend

# ─── Database Configuration ──────────────────────────────────────────────────

DB_PATH = "fire_data.db"

def get_db():
    """Create a database connection."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row  # Return rows as dict-like objects
    return conn

def init_db():
    """Initialize the SQLite database and create the sensor_data table."""
    conn = get_db()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sensor_data (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            temperature REAL    NOT NULL,
            humidity    REAL    NOT NULL,
            smoke       INTEGER NOT NULL,
            flame       INTEGER NOT NULL,   -- 0 = safe, 1 = fire detected
            fire_prob   REAL    NOT NULL,   -- ML model output 0.0 – 1.0
            timestamp   TEXT    NOT NULL    -- ISO-8601 datetime string
        )
    """)
    conn.commit()
    conn.close()
    print("✅  Database initialised at", DB_PATH)

# ─── Helper ───────────────────────────────────────────────────────────────────

def row_to_dict(row):
    """Convert a sqlite3.Row to a plain dict."""
    return {
        "id":          row["id"],
        "temperature": row["temperature"],
        "humidity":    row["humidity"],
        "smoke":       row["smoke"],
        "flame":       row["flame"],
        "fireProb":    row["fire_prob"],
        "timestamp":   row["timestamp"],
    }

# ─── Routes ──────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    """Serve the main dashboard HTML."""
    return send_from_directory("static", "index.html")

@app.route("/data", methods=["POST"])
def receive_data():
    """
    POST /data
    Accepts JSON from the ESP32 and stores it in the database.

    Expected payload:
        {
            "temperature": 30.0,
            "humidity":    60.0,
            "smoke":       1200,
            "flame":       0,
            "fireProb":    0.7
        }
    """
    payload = request.get_json(silent=True)
    if not payload:
        return jsonify({"error": "Invalid or missing JSON body"}), 400

    # Validate required fields
    required = ["temperature", "humidity", "smoke", "flame", "fireProb"]
    missing = [f for f in required if f not in payload]
    if missing:
        return jsonify({"error": f"Missing fields: {missing}"}), 400

    timestamp = datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")

    conn = get_db()
    conn.execute(
        """INSERT INTO sensor_data
               (temperature, humidity, smoke, flame, fire_prob, timestamp)
           VALUES (?, ?, ?, ?, ?, ?)""",
        (
            float(payload["temperature"]),
            float(payload["humidity"]),
            int(payload["smoke"]),
            int(payload["flame"]),
            float(payload["fireProb"]),
            timestamp,
        ),
    )
    conn.commit()
    conn.close()

    print(f"[{timestamp}] Data received — temp={payload['temperature']}°C  "
          f"smoke={payload['smoke']}  flame={payload['flame']}  "
          f"fireProb={payload['fireProb']}")

    return jsonify({"status": "ok", "timestamp": timestamp}), 201


@app.route("/data", methods=["GET"])
def get_data():
    """
    GET /data
    Returns the latest reading plus the last 100 historical records.

    Query params:
        limit  (int, default 100) — max history records to return
    """
    limit = min(int(request.args.get("limit", 100)), 500)

    conn = get_db()

    # Latest single record
    latest_row = conn.execute(
        "SELECT * FROM sensor_data ORDER BY id DESC LIMIT 1"
    ).fetchone()

    # Historical records (newest first, then reversed for charting)
    history_rows = conn.execute(
        "SELECT * FROM sensor_data ORDER BY id DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()

    latest   = row_to_dict(latest_row)   if latest_row   else None
    history  = [row_to_dict(r) for r in reversed(history_rows)]

    return jsonify({"latest": latest, "history": history})


@app.route("/data/clear", methods=["DELETE"])
def clear_data():
    """DELETE /data/clear — wipe all records (useful for testing)."""
    conn = get_db()
    conn.execute("DELETE FROM sensor_data")
    conn.commit()
    conn.close()
    return jsonify({"status": "cleared"}), 200


# ─── Entry Point ─────────────────────────────────────────────────────────────

if __name__ == "__main__":
    init_db()
    print("🔥  Fire Detection Dashboard running on http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=True)
