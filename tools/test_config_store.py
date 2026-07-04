import tempfile, unittest
from pathlib import Path

import config_store as cs


def make_cfg():
    return {
        "header_title": "Wetter Büro",
        "display_mode": 1,
        "wifi_ssid": "MeinWLAN",
        "wifi_pass": 'pass"wort\\x',
        "sb_token": "tok",
        "sb_secret": "sec",
        "lat": "48.5333",
        "lon": "15.9167",
        "tz": "Europe/Vienna",
        "devices": [
            {"id": "AAAA", "name": "Büro", "outdoor": False, "selected": True},
            {"id": "BBBB", "name": "Garten", "outdoor": True, "selected": True},
            {"id": "CCCC", "name": "Keller", "outdoor": False, "selected": False},
        ],
    }


class TestConfigStore(unittest.TestCase):
    def test_device_limit(self):
        self.assertEqual(cs.device_limit(0), 4)
        self.assertEqual(cs.device_limit(1), 4)
        self.assertEqual(cs.device_limit(2), 6)

    def test_selected_devices(self):
        sel = cs.selected_devices(make_cfg())
        self.assertEqual([d["id"] for d in sel], ["AAAA", "BBBB"])

    def test_save_and_load_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            jp = Path(td) / "cfg.json"
            uc = Path(td) / "user_config.h"
            sh = Path(td) / "secrets.h"
            cfg = make_cfg()
            cs.save(cfg, json_path=jp, user_config_path=uc, secrets_path=sh)
            self.assertEqual(cs.load(json_path=jp), cfg)
            text = uc.read_text(encoding="utf-8")
            self.assertIn('#define DISPLAY_MODE 1', text)
            self.assertIn('#define HEADER_TITLE "Wetter Büro"', text)
            self.assertIn('#define DEVICE_COUNT 2', text)   # nur selektierte
            self.assertIn('{"AAAA","BBBB"}', text.replace("\n  ", ""))
            self.assertIn('{"Büro","Garten"}', text.replace("\n  ", ""))
            self.assertIn('{false,true}', text.replace("\n  ", ""))
            self.assertIn('#define FORECAST_LAT  "48.5333"', text)
            stext = sh.read_text(encoding="utf-8")
            self.assertIn('#define WIFI_SSID  "MeinWLAN"', stext)
            # Escaping: " -> \" und \ -> \\
            self.assertIn('#define WIFI_PASS  "pass\\"wort\\\\x"', stext)

    def test_load_parses_existing_headers(self):
        with tempfile.TemporaryDirectory() as td:
            jp = Path(td) / "cfg.json"   # existiert nicht
            uc = Path(td) / "user_config.h"
            sh = Path(td) / "secrets.h"
            uc.write_text(
                '#define DEVICE_COUNT 2\n'
                'static const char* const DEVICE_IDS[DEVICE_COUNT] =\n'
                '  {"X1","X2"};\n'
                'static const char* const DEVICE_NAMES[DEVICE_COUNT] =\n'
                '  {"Büro","Küche"};\n'
                'static const bool DEVICE_OUTDOOR[DEVICE_COUNT] = {false,true};\n'
                '#define FORECAST_LAT  "48.1"\n'
                '#define FORECAST_LON  "16.2"\n'
                '#define FORECAST_TZ   "Europe/Vienna"\n',
                encoding="utf-8")
            sh.write_text('#define WIFI_SSID  "W"\n#define WIFI_PASS  "P"\n'
                          '#define SB_TOKEN   "T"\n#define SB_SECRET  "S"\n',
                          encoding="utf-8")
            cfg = cs.load(json_path=jp, user_config_path=uc, secrets_path=sh)
            self.assertEqual(cfg["wifi_ssid"], "W")
            self.assertEqual(cfg["lat"], "48.1")
            self.assertEqual(cfg["display_mode"], 0)          # Default
            self.assertEqual(cfg["header_title"], "SwitchBot Wetter")  # Default
            self.assertEqual(len(cfg["devices"]), 2)
            self.assertEqual(cfg["devices"][1],
                             {"id": "X2", "name": "Küche", "outdoor": True, "selected": True})

    def test_load_without_anything_returns_defaults(self):
        with tempfile.TemporaryDirectory() as td:
            cfg = cs.load(json_path=Path(td) / "x.json",
                          user_config_path=Path(td) / "y.h",
                          secrets_path=Path(td) / "z.h")
            self.assertEqual(cfg["display_mode"], 0)
            self.assertEqual(cfg["devices"], [])

    def test_load_json_without_devices_creates_independent_list(self):
        """Regression test: devices list must be deep-copied, not shared with DEFAULTS."""
        with tempfile.TemporaryDirectory() as td:
            jp = Path(td) / "cfg.json"
            # JSON without "devices" key — should not share list with DEFAULTS
            jp.write_text(
                '{"header_title": "Test", "display_mode": 1, "wifi_ssid": "W"}',
                encoding="utf-8")
            cfg = cs.load(json_path=jp,
                          user_config_path=Path(td) / "y.h",
                          secrets_path=Path(td) / "z.h")
            # Mutate loaded cfg's devices list
            cfg["devices"].append({"id": "TEST", "name": "Test", "outdoor": False, "selected": True})
            # DEFAULTS["devices"] must still be empty (not corrupted)
            self.assertEqual(cs.DEFAULTS["devices"], [])
            # Two independent loads must get independent lists
            cfg2 = cs.load(json_path=jp,
                           user_config_path=Path(td) / "y.h",
                           secrets_path=Path(td) / "z.h")
            self.assertIsNot(cfg["devices"], cfg2["devices"])
            self.assertEqual(len(cfg["devices"]), 1)
            self.assertEqual(len(cfg2["devices"]), 0)


if __name__ == "__main__":
    unittest.main()
