import unittest

import sb_api


class TestSbApi(unittest.TestCase):
    def test_sign_headers_deterministic(self):
        h = sb_api.sign_headers("tok", "sec", t="1000", nonce="nonce1")
        self.assertEqual(h["Authorization"], "tok")
        self.assertEqual(h["t"], "1000")
        self.assertEqual(h["nonce"], "nonce1")
        self.assertEqual(h["sign"], "O4Jkd6KlptflxgMcPzDRiTTRCNJVwx8zq7v1ITxJZJQ=")

    def test_sign_headers_autofills(self):
        h = sb_api.sign_headers("tok", "sec")
        self.assertTrue(h["t"].isdigit())
        self.assertTrue(len(h["nonce"]) >= 8)
        self.assertTrue(len(h["sign"]) >= 40)

    def test_is_meter(self):
        for t in ("Meter", "MeterPlus", "MeterPro", "MeterPro(CO2)",
                  "WoIOSensor", "Hub 2", "Hub 3"):
            self.assertTrue(sb_api.is_meter(t), t)
        for t in ("Bot", "Curtain", "Plug Mini (JP)", "Hub Mini", ""):
            self.assertFalse(sb_api.is_meter(t), t)


if __name__ == "__main__":
    unittest.main()
