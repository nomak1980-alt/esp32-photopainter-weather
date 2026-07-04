"""SwitchBot Cloud API v1.1: Signatur + Geraeteliste (nur Thermo-/Hygrometer)."""
import base64
import hashlib
import hmac
import json
import time
import urllib.error
import urllib.request
import uuid

API_BASE = "https://api.switch-bot.com"


def sign_headers(token, secret, t=None, nonce=None):
    t = t if t is not None else str(int(time.time() * 1000))
    nonce = nonce if nonce is not None else str(uuid.uuid4())
    msg = (token + t + nonce).encode()
    sig = base64.b64encode(
        hmac.new(secret.encode(), msg, hashlib.sha256).digest()).decode()
    return {"Authorization": token, "sign": sig, "t": t, "nonce": nonce,
            "Content-Type": "application/json"}


def is_meter(device_type):
    return "Meter" in device_type or device_type in ("WoIOSensor", "Hub 2")


def list_meters(token, secret, timeout=15):
    req = urllib.request.Request(API_BASE + "/v1.1/devices",
                                 headers=sign_headers(token, secret))
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            data = json.load(r)
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"SwitchBot-API HTTP {e.code} - Token/Secret pruefen.") from e
    except OSError as e:
        raise RuntimeError(f"Keine Verbindung zur SwitchBot-API: {e}") from e
    if data.get("statusCode") != 100:
        raise RuntimeError(f"SwitchBot-API Fehler: {data.get('message', data)}")
    return [{"id": d["deviceId"],
             "name": d.get("deviceName") or d["deviceId"],
             "type": d.get("deviceType", "")}
            for d in data.get("body", {}).get("deviceList", [])
            if is_meter(d.get("deviceType", ""))]
