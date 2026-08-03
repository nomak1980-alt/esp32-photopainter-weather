@echo off
rem Startet das Konfigurations-Formular (Einstellungen + Flashen weiterer Panels).
rem pythonw = ohne Konsolenfenster; Fallback auf python, falls nicht vorhanden.
cd /d "%~dp0"
where pythonw >nul 2>nul
if %errorlevel%==0 (
  start "PhotoPainter Konfigurator" pythonw tools\configurator.py
) else (
  python tools\configurator.py
  if errorlevel 1 pause
)
