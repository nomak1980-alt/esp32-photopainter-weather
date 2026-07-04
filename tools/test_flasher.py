import unittest
from types import SimpleNamespace
from unittest.mock import patch
import subprocess

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

    @patch('flasher.subprocess.run')
    def test_check_connection_timeout(self, mock_run):
        # Timeout (Board schlaeft) -> sollte (False, str) liefern
        mock_run.side_effect = subprocess.TimeoutExpired(cmd="x", timeout=90)
        success, msg = flasher.check_connection("COM3")
        self.assertFalse(success)
        self.assertIn("Timeout", msg)

    @patch('flasher.subprocess.run')
    def test_check_connection_esptool_missing(self, mock_run):
        # esptool.py nicht gefunden -> sollte (False, str) liefern
        mock_run.side_effect = FileNotFoundError("esptool.py nicht gefunden")
        success, msg = flasher.check_connection("COM3")
        self.assertFalse(success)
        self.assertIn("esptool", msg)

    @patch('flasher.subprocess.run')
    def test_check_connection_success(self, mock_run):
        # Erfolgreicher read_mac -> (True, output)
        mock_run.return_value = SimpleNamespace(returncode=0, stdout="MAC: 001122", stderr="")
        success, msg = flasher.check_connection("COM3")
        self.assertTrue(success)
        self.assertIn("MAC", msg)

    @patch('flasher.subprocess.run')
    def test_check_connection_failure(self, mock_run):
        # esptool-Fehler -> (False, stderr)
        mock_run.return_value = SimpleNamespace(returncode=1, stdout="", stderr="Chip not found")
        success, msg = flasher.check_connection("COM3")
        self.assertFalse(success)
        self.assertIn("Chip not found", msg)


if __name__ == "__main__":
    unittest.main()
