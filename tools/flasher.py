"""ESP32-S3-PhotoPainter finden (USB-VID 303A), pruefen und flashen."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ESP32S3_VID = 0x303A
ESPTOOL = Path.home() / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"

PORT_HELP = (
    "Kein PhotoPainter gefunden (USB-VID 303A).\n\n"
    "1. USB-Kabel pruefen - viele Kabel sind reine Ladekabel.\n"
    "2. Das Board laeuft auf Akku weiter! Echter Neustart nur per PWR-Taste\n"
    "   (aus- und wieder einschalten).\n"
    "3. Schlaeft die Firmware sofort wieder ein (Deep-Sleep), Download-Modus\n"
    "   erzwingen: BOOT-Taste halten, dabei PWR aus/an, BOOT ~5 s weiter halten.\n"
)


def pick_port(ports):
    for p in ports:
        if getattr(p, "vid", None) == ESP32S3_VID:
            return p.device
    return None


def find_port():
    from serial.tools import list_ports  # pyserial (Teil der PlatformIO-Installation)
    return pick_port(list_ports.comports())


def check_connection(port):
    r = subprocess.run(
        [sys.executable, str(ESPTOOL), "--port", port, "--baud", "115200", "read_mac"],
        capture_output=True, text=True, timeout=90)
    return r.returncode == 0, (r.stdout or "") + (r.stderr or "")


def upload(port, on_line):
    proc = subprocess.Popen(
        [sys.executable, "-m", "platformio", "run", "-e", "photopainter",
         "-t", "upload", "--upload-port", port],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding="utf-8", errors="replace")
    for line in proc.stdout:
        on_line(line.rstrip())
    return proc.wait() == 0
