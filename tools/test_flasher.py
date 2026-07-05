import unittest
from types import SimpleNamespace
from unittest.mock import patch, MagicMock

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

    @staticmethod
    def _mock_proc(lines, returncode):
        proc = MagicMock()
        proc.stdout = iter(lines)
        proc.wait.return_value = returncode
        return proc

    @patch('flasher.subprocess.Popen')
    def test_upload_success_streams_lines(self, mock_popen):
        mock_popen.return_value = self._mock_proc(["Writing...\n", "SUCCESS\n"], 0)
        seen = []
        self.assertTrue(flasher.upload("COM4", seen.append))
        self.assertEqual(seen, ["Writing...", "SUCCESS"])
        args = mock_popen.call_args.args[0]
        self.assertIn("--upload-port", args)
        self.assertIn("COM4", args)

    @patch('flasher.subprocess.Popen')
    def test_upload_failure(self, mock_popen):
        mock_popen.return_value = self._mock_proc(["Error 1\n"], 1)
        self.assertFalse(flasher.upload("COM4", lambda line: None))

    @patch('flasher.subprocess.Popen')
    def test_build_has_no_upload_target(self, mock_popen):
        mock_popen.return_value = self._mock_proc([], 0)
        self.assertTrue(flasher.build(lambda line: None))
        args = mock_popen.call_args.args[0]
        self.assertNotIn("upload", args)

    @patch('flasher.subprocess.Popen')
    def test_no_console_window_flag(self, mock_popen):
        # pythonw: Unterprozesse duerfen keine Konsolenfenster aufpoppen lassen
        mock_popen.return_value = self._mock_proc([], 0)
        flasher.build(lambda line: None)
        self.assertEqual(mock_popen.call_args.kwargs.get("creationflags"),
                         flasher.NO_WINDOW)


if __name__ == "__main__":
    unittest.main()
