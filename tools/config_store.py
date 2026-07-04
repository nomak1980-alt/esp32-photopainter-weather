"""Konfiguration des Wetter-Displays: JSON laden/speichern, C-Header generieren.

Quelle der Wahrheit ist tools/wetter_config.json (gitignored). Beim Speichern
werden include/user_config.h und include/secrets.h daraus generiert.
Existiert noch kein JSON, werden vorhandene Header als Vorbelegung geparst.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "tools" / "wetter_config.json"
USER_CONFIG_H = ROOT / "include" / "user_config.h"
SECRETS_H = ROOT / "include" / "secrets.h"

MODE_NAMES = ["Tages-Vorschau", "Stunden-Vorschau", "6 Thermometer"]

DEFAULTS = {
    "header_title": "SwitchBot Wetter",
    "display_mode": 0,
    "wifi_ssid": "",
    "wifi_pass": "",
    "sb_token": "",
    "sb_secret": "",
    "lat": "48.0000",
    "lon": "16.0000",
    "tz": "Europe/Vienna",
    "devices": [],
}


def device_limit(mode):
    return 6 if mode == 2 else 4


def selected_devices(cfg):
    return [d for d in cfg["devices"] if d.get("selected")]


def _c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _defines(text):
    """#define NAME "wert" -> dict (nur String-Defines)."""
    return dict(re.findall(r'#define\s+(\w+)\s+"((?:[^"\\]|\\.)*)"', text))


def _unescape(s):
    return s.replace('\\"', '"').replace("\\\\", "\\")


def _str_list(text, name):
    """Werte aus  static ... NAME[...] = {"a","b"};  extrahieren."""
    m = re.search(re.escape(name) + r"\[[^\]]*\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    if not m:
        return []
    return [_unescape(v) for v in re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))]


def _bool_list(text, name):
    m = re.search(re.escape(name) + r"\[[^\]]*\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    if not m:
        return []
    return [w.strip() == "true" for w in m.group(1).split(",") if w.strip()]


def _parse_headers(user_config_path, secrets_path):
    cfg = dict(DEFAULTS, devices=[])
    if user_config_path.exists():
        text = user_config_path.read_text(encoding="utf-8")
        d = _defines(text)
        cfg["lat"] = d.get("FORECAST_LAT", cfg["lat"])
        cfg["lon"] = d.get("FORECAST_LON", cfg["lon"])
        cfg["tz"] = d.get("FORECAST_TZ", cfg["tz"])
        cfg["header_title"] = _unescape(d.get("HEADER_TITLE", cfg["header_title"]))
        m = re.search(r"#define\s+DISPLAY_MODE\s+(\d)", text)
        if m:
            cfg["display_mode"] = int(m.group(1))
        ids = _str_list(text, "DEVICE_IDS")
        names = _str_list(text, "DEVICE_NAMES")
        outs = _bool_list(text, "DEVICE_OUTDOOR")
        for i, dev_id in enumerate(ids):
            cfg["devices"].append({
                "id": dev_id,
                "name": names[i] if i < len(names) else dev_id,
                "outdoor": outs[i] if i < len(outs) else False,
                "selected": True,
            })
    if secrets_path.exists():
        d = _defines(secrets_path.read_text(encoding="utf-8"))
        cfg["wifi_ssid"] = _unescape(d.get("WIFI_SSID", ""))
        cfg["wifi_pass"] = _unescape(d.get("WIFI_PASS", ""))
        cfg["sb_token"] = _unescape(d.get("SB_TOKEN", ""))
        cfg["sb_secret"] = _unescape(d.get("SB_SECRET", ""))
    return cfg


def load(json_path=None, user_config_path=None, secrets_path=None):
    json_path = Path(json_path or JSON_PATH)
    if json_path.exists():
        cfg = dict(DEFAULTS)
        cfg.update(json.loads(json_path.read_text(encoding="utf-8")))
        return cfg
    return _parse_headers(Path(user_config_path or USER_CONFIG_H),
                          Path(secrets_path or SECRETS_H))


def _gen_user_config(cfg):
    sel = selected_devices(cfg)
    ids = ",".join(_c_str(d["id"]) for d in sel)
    names = ",".join(_c_str(d["name"]) for d in sel)
    outs = ",".join("true" if d["outdoor"] else "false" for d in sel)
    return (
        "#pragma once\n"
        "// Von tools/configurator.py generiert - nicht von Hand editieren, nicht committen.\n\n"
        f"#define DEVICE_COUNT {len(sel)}\n"
        "static const char* const DEVICE_IDS[DEVICE_COUNT] =\n"
        f"  {{{ids}}};\n"
        "static const char* const DEVICE_NAMES[DEVICE_COUNT] =\n"
        f"  {{{names}}};\n"
        f"static const bool DEVICE_OUTDOOR[DEVICE_COUNT] = {{{outs}}};\n\n"
        f'#define FORECAST_LAT  {_c_str(cfg["lat"])}\n'
        f'#define FORECAST_LON  {_c_str(cfg["lon"])}\n'
        f'#define FORECAST_TZ   {_c_str(cfg["tz"])}\n\n'
        f'#define DISPLAY_MODE {cfg["display_mode"]}\n'
        f'#define HEADER_TITLE {_c_str(cfg["header_title"])}\n'
    )


def _gen_secrets(cfg):
    return (
        "#pragma once\n"
        "// Von tools/configurator.py generiert - nicht committen.\n"
        f'#define WIFI_SSID  {_c_str(cfg["wifi_ssid"])}\n'
        f'#define WIFI_PASS  {_c_str(cfg["wifi_pass"])}\n'
        f'#define SB_TOKEN   {_c_str(cfg["sb_token"])}\n'
        f'#define SB_SECRET  {_c_str(cfg["sb_secret"])}\n'
    )


def save(cfg, json_path=None, user_config_path=None, secrets_path=None):
    Path(json_path or JSON_PATH).write_text(
        json.dumps(cfg, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    Path(user_config_path or USER_CONFIG_H).write_text(_gen_user_config(cfg), encoding="utf-8")
    Path(secrets_path or SECRETS_H).write_text(_gen_secrets(cfg), encoding="utf-8")
