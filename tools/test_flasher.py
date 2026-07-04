import unittest
from types import SimpleNamespace

import flasher


class TestFlasher(unittest.TestCase):
    def test_pick_port_finds_esp32s3(self):
        ports = [SimpleNamespace(vid=0x046D, device="COM7"),
                 SimpleNamespace(vid=0x303A, device="COM4"),
                 SimpleNamespace(vid=None, device="COM1")]
        self.assertEqual(flasher.pick_port(ports), "COM4")

    def test_pick_port_none(self):
        self.assertIsNone(flasher.pick_port([]))
        self.assertIsNone(flasher.pick_port([SimpleNamespace(vid=0x1234, device="COM9")]))

    def test_help_text_mentions_pwr_and_boot(self):
        self.assertIn("PWR", flasher.PORT_HELP)
        self.assertIn("BOOT", flasher.PORT_HELP)


if __name__ == "__main__":
    unittest.main()
