"""Local MixiPlan gateway, SQLite history, webhook ingest, and role-based dashboard API."""

from __future__ import annotations

import hmac
import json
import os
import secrets
import sqlite3
import threading
import time
import uuid
from urllib.request import Request, urlopen
from dotenv import load_dotenv
from contextlib import asynccontextmanager
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

from fastapi import Depends, FastAPI, Header, HTTPException, Query
from fastapi.responses import FileResponse
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from jose import JWTError, jwt
from paho.mqtt import client as mqtt
from pydantic import BaseModel, Field


ROOT = Path(__file__).resolve().parent
# The local .env file is the source of truth for this demo, even if an old
# value was previously exported in the terminal environment.
load_dotenv(ROOT / ".env", override=True)
STATIC = ROOT / "static"
JWT_ALGORITHM = "HS256"
JWT_SECRET = os.getenv("JWT_SECRET", "")
ADMIN_PASSWORD = os.getenv("ADMIN_PASSWORD", "")
VIEWER_PASSWORD = os.getenv("VIEWER_PASSWORD", "")
WEBHOOK_TOKEN = os.getenv("WEBHOOK_TOKEN", "")
DATABASE_PATH = Path(os.getenv("DATABASE_PATH", str(ROOT / "data" / "mixiplan.sqlite3")))
MQTT_HOST = os.getenv("MQTT_HOST", "broker.hivemq.com")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TLS = os.getenv("MQTT_TLS", "0") == "1"
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_CA_CERT = os.getenv("MQTT_CA_CERT", "")
TELEMETRY_TOPIC = os.getenv("MQTT_TELEMETRY_TOPIC", "mixiplan/tuoicay_mx7k2/trangthai")
COMMAND_TOPIC = os.getenv("MQTT_COMMAND_TOPIC", "mixiplan/tuoicay_mx7k2/bom/set")
ACK_TOPIC = os.getenv("MQTT_ACK_TOPIC", "mixiplan/tuoicay_mx7k2/bom/ack")
WEATHER_URL = (
    "https://api.open-meteo.com/v1/forecast?latitude=10.8231&longitude=106.6297"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m"
    "&hourly=temperature_2m,precipitation_probability,precipitation,weather_code"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,precipitation_sum"
    "&forecast_days=3&timezone=Asia%2FHo_Chi_Minh"
)
WEATHER_CACHE_SECONDS = 600

db_lock = threading.Lock()
mqtt_client: mqtt.Client | None = None
broker_connected = threading.Event()
bearer = HTTPBearer(auto_error=False)
weather_cache: dict[str, Any] | None = None
weather_cache_time = 0.0
weather_lock = threading.Lock()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def db_connect() -> sqlite3.Connection:
    DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(DATABASE_PATH, timeout=10)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with db_lock, db_connect() as conn:
        conn.execute("PRAGMA journal_mode=WAL")
        conn.execute(
            """CREATE TABLE IF NOT EXISTS telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                received_at TEXT NOT NULL,
                device_id TEXT NOT NULL,
                payload TEXT NOT NULL
            )"""
        )
        conn.execute(
            """CREATE TABLE IF NOT EXISTS commands (
                id TEXT PRIMARY KEY,
                created_at TEXT NOT NULL,
                state TEXT NOT NULL,
                status TEXT NOT NULL
            )"""
        )
        conn.execute("CREATE INDEX IF NOT EXISTS idx_telemetry_time ON telemetry(id DESC)")
        conn.execute(
            """CREATE TABLE IF NOT EXISTS command_acks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                received_at TEXT NOT NULL,
                command_id TEXT,
                payload TEXT NOT NULL
            )"""
        )


def store_telemetry(payload: dict[str, Any]) -> None:
    device_id = str(payload.get("device_id") or payload.get("device") or "mixiplan-esp32")[:80]
    with db_lock, db_connect() as conn:
        conn.execute(
            "INSERT INTO telemetry(received_at, device_id, payload) VALUES (?, ?, ?)",
            (utc_now(), device_id, json.dumps(payload, separators=(",", ":"))),
        )


def store_ack(payload: dict[str, Any]) -> None:
    with db_lock, db_connect() as conn:
        conn.execute(
            "INSERT INTO command_acks(received_at, command_id, payload) VALUES (?, ?, ?)",
            (utc_now(), str(payload.get("id", ""))[:100], json.dumps(payload, separators=(",", ":"))),
        )
        if payload.get("id"):
            conn.execute(
                "UPDATE commands SET status = ? WHERE id = ?",
                ("acknowledged" if payload.get("accepted") else "rejected", str(payload["id"])),
            )


def on_connect(client: mqtt.Client, _userdata: Any, _flags: Any, reason_code: Any, _properties: Any) -> None:
    if getattr(reason_code, "is_failure", False):
        broker_connected.clear()
        print(f"[MQTT] Broker connection rejected: {reason_code}", flush=True)
        return
    broker_connected.set()
    client.subscribe([(TELEMETRY_TOPIC, 1), (ACK_TOPIC, 1)])
    print(f"[MQTT] Subscribed: {TELEMETRY_TOPIC}, {ACK_TOPIC}", flush=True)


def on_disconnect(_client: mqtt.Client, _userdata: Any, _disconnect_flags: Any, reason_code: Any, _properties: Any) -> None:
    broker_connected.clear()
    print(f"[MQTT] Disconnected: {reason_code}", flush=True)


def on_message(_client: mqtt.Client, _userdata: Any, message: mqtt.MQTTMessage) -> None:
    try:
        payload = json.loads(message.payload.decode("utf-8"))
        if not isinstance(payload, dict):
            return
        if message.topic == ACK_TOPIC:
            store_ack(payload)
        else:
            store_telemetry(payload)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        print(f"[MQTT] Ignoring invalid JSON: {exc}", flush=True)


def start_mqtt() -> mqtt.Client:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"mixiplan-gateway-{uuid.uuid4().hex[:8]}")
    if MQTT_USERNAME:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    if MQTT_TLS:
        if not MQTT_CA_CERT:
            raise RuntimeError("MQTT_TLS=1 requires MQTT_CA_CERT")
        client.tls_set(ca_certs=MQTT_CA_CERT)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=2, max_delay=60)
    client.connect_async(MQTT_HOST, MQTT_PORT, keepalive=30)
    client.loop_start()
    return client


def check_environment() -> None:
    missing = [name for name, value in {
        "JWT_SECRET": JWT_SECRET,
        "ADMIN_PASSWORD": ADMIN_PASSWORD,
        "VIEWER_PASSWORD": VIEWER_PASSWORD,
        "WEBHOOK_TOKEN": WEBHOOK_TOKEN,
    }.items() if not value or value.startswith("replace_with_")]
    if missing:
        raise RuntimeError("Set required environment variables: " + ", ".join(missing))
    if len(JWT_SECRET) < 32:
        raise RuntimeError("JWT_SECRET must contain at least 32 characters")
    if MQTT_HOST == "broker.hivemq.com" and not MQTT_TLS:
        print("[Security] Public unauthenticated MQTT is demo-only; do not attach real actuators.", flush=True)


@asynccontextmanager
async def lifespan(_app: FastAPI):
    global mqtt_client
    check_environment()
    init_db()
    mqtt_client = start_mqtt()
    yield
    if mqtt_client:
        mqtt_client.disconnect()
        mqtt_client.loop_stop()


app = FastAPI(title="MixiPlan Edge Gateway", version="0.1.0", lifespan=lifespan)


class LoginRequest(BaseModel):
    username: str = Field(min_length=1, max_length=40)
    password: str = Field(min_length=1, max_length=200)


class CommandRequest(BaseModel):
    state: str = Field(pattern="^(ON|OFF|AUTO)$")


def current_user(credentials: HTTPAuthorizationCredentials | None = Depends(bearer)) -> dict[str, str]:
    if credentials is None:
        raise HTTPException(status_code=401, detail="Login required")
    try:
        claims = jwt.decode(credentials.credentials, JWT_SECRET, algorithms=[JWT_ALGORITHM])
        if claims.get("role") not in {"admin", "viewer"}:
            raise JWTError("invalid role")
        return {"username": str(claims["sub"]), "role": str(claims["role"])}
    except (JWTError, KeyError, TypeError):
        raise HTTPException(status_code=401, detail="Invalid or expired token")


def require_admin(user: dict[str, str] = Depends(current_user)) -> dict[str, str]:
    if user["role"] != "admin":
        raise HTTPException(status_code=403, detail="Admin role required")
    return user


@app.get("/")
def dashboard() -> FileResponse:
    return FileResponse(STATIC / "index.html")


@app.get("/api/health")
def health() -> dict[str, Any]:
    return {"ok": True, "mqtt_connected": broker_connected.is_set(), "database": str(DATABASE_PATH)}


@app.get("/api/weather")
def weather_forecast(_user: dict[str, str] = Depends(current_user)) -> dict[str, Any]:
    global weather_cache, weather_cache_time
    now = time.monotonic()
    with weather_lock:
        if weather_cache is not None and now - weather_cache_time < WEATHER_CACHE_SECONDS:
            return weather_cache

        request = Request(WEATHER_URL, headers={"User-Agent": "MixiPlan/1.0"})
        try:
            with urlopen(request, timeout=8) as response:
                raw = json.loads(response.read().decode("utf-8"))

            current = raw["current"]
            hourly = raw["hourly"]
            daily = raw["daily"]
            current_time = current["time"]
            future_hours = []
            for i, timestamp in enumerate(hourly["time"]):
                if timestamp <= current_time:
                    continue
                future_hours.append({
                    "time": timestamp,
                    "temperature": hourly["temperature_2m"][i],
                    "rain_probability": hourly["precipitation_probability"][i],
                    "precipitation": hourly["precipitation"][i],
                    "weather_code": hourly["weather_code"][i],
                })
                if len(future_hours) == 8:
                    break

            forecast = {
                "location": "TP. Hồ Chí Minh",
                "timezone": raw.get("timezone", "Asia/Ho_Chi_Minh"),
                "updated_at": utc_now(),
                "current": {
                    "time": current_time,
                    "temperature": current["temperature_2m"],
                    "feels_like": current["apparent_temperature"],
                    "humidity": current["relative_humidity_2m"],
                    "precipitation": current["precipitation"],
                    "wind_speed": current["wind_speed_10m"],
                    "weather_code": current["weather_code"],
                },
                "hourly": future_hours,
                "daily": [
                    {
                        "date": daily["time"][i],
                        "weather_code": daily["weather_code"][i],
                        "temperature_max": daily["temperature_2m_max"][i],
                        "temperature_min": daily["temperature_2m_min"][i],
                        "rain_probability_max": daily["precipitation_probability_max"][i],
                        "precipitation_sum": daily["precipitation_sum"][i],
                    }
                    for i in range(min(3, len(daily["time"])))
                ],
            }
            weather_cache = forecast
            weather_cache_time = time.monotonic()
            return forecast
        except Exception as exc:
            if weather_cache is not None:
                return {**weather_cache, "stale": True, "error": "Không tải được cập nhật mới"}
            raise HTTPException(status_code=502, detail="Không lấy được dự báo Open-Meteo") from exc


@app.post("/api/login")
def login(request: LoginRequest) -> dict[str, str]:
    users = {
        "admin": (ADMIN_PASSWORD, "admin"),
        "viewer": (VIEWER_PASSWORD, "viewer"),
    }
    stored = users.get(request.username)
    if not stored or not secrets.compare_digest(request.password, stored[0]):
        raise HTTPException(status_code=401, detail="Wrong username or password")
    token = jwt.encode(
        {"sub": request.username, "role": stored[1], "exp": datetime.now(timezone.utc) + timedelta(hours=8)},
        JWT_SECRET,
        algorithm=JWT_ALGORITHM,
    )
    return {"access_token": token, "token_type": "bearer", "role": stored[1]}


@app.get("/api/me")
def me(user: dict[str, str] = Depends(current_user)) -> dict[str, str]:
    return user


@app.get("/api/latest")
def latest(_user: dict[str, str] = Depends(current_user)) -> dict[str, Any] | None:
    with db_connect() as conn:
        row = conn.execute("SELECT received_at, device_id, payload FROM telemetry ORDER BY id DESC LIMIT 1").fetchone()
    return {"received_at": row["received_at"], "device_id": row["device_id"], **json.loads(row["payload"])} if row else None


@app.get("/api/history")
def history(
    limit: int = Query(default=120, ge=1, le=1000),
    _user: dict[str, str] = Depends(current_user),
) -> list[dict[str, Any]]:
    with db_connect() as conn:
        rows = conn.execute(
            "SELECT received_at, device_id, payload FROM telemetry ORDER BY id DESC LIMIT ?", (limit,)
        ).fetchall()
    result = []
    for row in reversed(rows):
        result.append({"received_at": row["received_at"], "device_id": row["device_id"], **json.loads(row["payload"])})
    return result


@app.get("/api/acks/latest")
def latest_ack(_user: dict[str, str] = Depends(current_user)) -> dict[str, Any] | None:
    with db_connect() as conn:
        row = conn.execute("SELECT received_at, payload FROM command_acks ORDER BY id DESC LIMIT 1").fetchone()
    return {"received_at": row["received_at"], **json.loads(row["payload"])} if row else None


@app.post("/api/command")
def send_command(request: CommandRequest, _user: dict[str, str] = Depends(require_admin)) -> dict[str, str]:
    if not broker_connected.is_set() or mqtt_client is None:
        raise HTTPException(status_code=503, detail="MQTT broker is offline")
    command_id = uuid.uuid4().hex
    payload = json.dumps({"id": command_id, "device": "pump", "state": request.state})
    info = mqtt_client.publish(COMMAND_TOPIC, payload, qos=1, retain=False)
    if info.rc != mqtt.MQTT_ERR_SUCCESS:
        raise HTTPException(status_code=503, detail="MQTT publish failed")
    with db_lock, db_connect() as conn:
        conn.execute(
            "INSERT INTO commands(id, created_at, state, status) VALUES (?, ?, ?, ?)",
            (command_id, utc_now(), request.state, "queued"),
        )
    return {"id": command_id, "state": request.state, "status": "queued"}


@app.get("/api/commands/latest")
def latest_command(_user: dict[str, str] = Depends(current_user)) -> dict[str, Any] | None:
    with db_connect() as conn:
        row = conn.execute("SELECT * FROM commands ORDER BY created_at DESC LIMIT 1").fetchone()
    return dict(row) if row else None


@app.post("/api/webhooks/telemetry")
def telemetry_webhook(
    payload: dict[str, Any],
    x_webhook_token: str = Header(default=""),
) -> dict[str, str]:
    if not WEBHOOK_TOKEN:
        raise HTTPException(status_code=503, detail="Webhook token is not configured")
    if not hmac.compare_digest(x_webhook_token, WEBHOOK_TOKEN):
        raise HTTPException(status_code=401, detail="Invalid webhook token")
    store_telemetry(payload)
    return {"status": "stored"}
