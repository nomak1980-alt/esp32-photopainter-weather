"""ESP32-S3-PhotoPainter finden (USB-VID 303A), pruefen und flashen."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ESP32S3_VID = 0x303A

PORT_HELP = (
    "Kein PhotoPainter gefunden (USB-VID 303A).\n\n"
    "1. USB-Kabel pruefen - viele Kabel sind reine Ladekabel.\n"
    "2. Das Board laeuft auf Akku weiter! Echter Neustart nur per PWR-Taste\n"
    "   (aus- und wieder einschalten).\n"
    "3. Schlaeft die Firmware sofort wieder ein (Deep-Sleep), Download-Modus\n"
    "   erzwingen: BOOT-Taste halten, dabei PWR aus/an, BOOT ~5 s weiter halten.\n"
)


# Unter pythonw (kein Konsolenfenster) wuerde jeder Unterprozess ein eigenes
# sichtbares Konsolenfenster aufmachen -> unterdruecken.
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)

# Laeuft das Formular unter pythonw.exe, darf PlatformIO NICHT damit gestartet
# werden: pio reicht sein Python an den esptool-Upload weiter, und der schlaegt
# unter pythonw ohne jede Ausgabe fehl (reproduziert). Immer python.exe nehmen.
PYTHON = sys.executable.replace("pythonw.exe", "python.exe")


def pick_port(ports):
    for p in ports:
        if getattr(p, "vid", None) == ESP32S3_VID:
            return p.device
    return None


def find_port():
    from serial.tools import list_ports  # pyserial (Teil der PlatformIO-Installation)
    return pick_port(list_ports.comports())


def _run_pio(args, on_line):
    proc = subprocess.Popen(
        [PYTHON, "-m", "platformio", "run", "-e", "photopainter", *args],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding="utf-8", errors="replace", creationflags=NO_WINDOW)
    for line in proc.stdout:
        on_line(line.rstrip())
    return proc.wait() == 0


def build(on_line):
    return _run_pio([], on_line)


def upload(port, on_line):
    return _run_pio(["-t", "upload", "--upload-port", port], on_line)
