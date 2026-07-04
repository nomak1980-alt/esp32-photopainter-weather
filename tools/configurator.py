"""Konfigurations- und Installations-Tool fuer den PhotoPainter-Wetter-Monitor.

Start: python tools/configurator.py
Speichern = JSON + Header generieren. Upload = Speichern + Flashen via PlatformIO.
"""
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

import config_store as cs
import flasher
import sb_api


class App:
    def __init__(self, root):
        self.root = root
        root.title("PhotoPainter Wetter - Konfiguration")
        self.cfg = cs.load()
        self.msgq = queue.Queue()   # (typ, text) aus Worker-Threads
        self.busy = False

        main = ttk.Frame(root, padding=10)
        main.grid(sticky="nsew")
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)
        main.columnconfigure(0, weight=1)

        # --- Allgemein ---
        gen = ttk.LabelFrame(main, text="Anzeige", padding=8)
        gen.grid(sticky="ew", pady=(0, 8))
        gen.columnconfigure(1, weight=1)
        ttk.Label(gen, text="Überschrift:").grid(row=0, column=0, sticky="w")
        self.v_title = tk.StringVar(value=self.cfg["header_title"])
        ttk.Entry(gen, textvariable=self.v_title).grid(row=0, column=1, sticky="ew", padx=6)
        ttk.Label(gen, text="Modus:").grid(row=1, column=0, sticky="w")
        self.v_mode = tk.IntVar(value=self.cfg["display_mode"])
        modefrm = ttk.Frame(gen)
        modefrm.grid(row=1, column=1, sticky="w", padx=6)
        for i, name in enumerate(cs.MODE_NAMES):
            ttk.Radiobutton(modefrm, text=name, value=i, variable=self.v_mode,
                            command=self.on_mode_change).pack(side="left", padx=(0, 10))

        # --- Zugangsdaten ---
        acc = ttk.LabelFrame(main, text="Zugangsdaten", padding=8)
        acc.grid(sticky="ew", pady=(0, 8))
        acc.columnconfigure(1, weight=1)
        acc.columnconfigure(3, weight=1)
        self.v_ssid = self._entry(acc, 0, 0, "WLAN-SSID:", self.cfg["wifi_ssid"])
        self.v_pass = self._entry(acc, 0, 2, "Passwort:", self.cfg["wifi_pass"], show="*")
        self.v_token = self._entry(acc, 1, 0, "SwitchBot-Token:", self.cfg["sb_token"], show="*")
        self.v_secret = self._entry(acc, 1, 2, "Secret:", self.cfg["sb_secret"], show="*")

        # --- Standort ---
        loc = ttk.LabelFrame(main, text="Standort (Open-Meteo)", padding=8)
        loc.grid(sticky="ew", pady=(0, 8))
        for c in (1, 3, 5):
            loc.columnconfigure(c, weight=1)
        self.v_lat = self._entry(loc, 0, 0, "Breite:", self.cfg["lat"])
        self.v_lon = self._entry(loc, 0, 2, "Länge:", self.cfg["lon"])
        self.v_tz = self._entry(loc, 0, 4, "Zeitzone:", self.cfg["tz"])

        # --- Sensoren ---
        sens = ttk.LabelFrame(main, text="Thermometer", padding=8)
        sens.grid(sticky="nsew", pady=(0, 8))
        main.rowconfigure(3, weight=1)
        sens.columnconfigure(0, weight=1)
        self.dev_frame = ttk.Frame(sens)
        self.dev_frame.grid(sticky="nsew")
        btns = ttk.Frame(sens)
        btns.grid(sticky="w", pady=(6, 0))
        self.btn_load = ttk.Button(btns, text="Sensoren laden (SwitchBot-Cloud)",
                                   command=self.on_load_devices)
        self.btn_load.pack(side="left")
        self.lbl_limit = ttk.Label(btns, text="")
        self.lbl_limit.pack(side="left", padx=10)
        self.render_devices()

        # --- Log + Aktionen ---
        self.log = tk.Text(main, height=10, state="disabled", wrap="none")
        self.log.grid(sticky="nsew", pady=(0, 8))
        main.rowconfigure(4, weight=1)
        act = ttk.Frame(main)
        act.grid(sticky="ew")
        self.btn_save = ttk.Button(act, text="Speichern", command=self.on_save)
        self.btn_save.pack(side="left")
        self.btn_upload = ttk.Button(act, text="Upload (Speichern + Flashen)",
                                     command=self.on_upload)
        self.btn_upload.pack(side="left", padx=8)
        self.status = ttk.Label(act, text="")
        self.status.pack(side="left", padx=10)

        self.root.after(100, self.poll_queue)

    def _entry(self, parent, row, col, label, value, show=None):
        ttk.Label(parent, text=label).grid(row=row, column=col, sticky="w")
        var = tk.StringVar(value=value)
        ttk.Entry(parent, textvariable=var, show=show).grid(
            row=row, column=col + 1, sticky="ew", padx=6, pady=2)
        return var

    # ---------- Sensorliste ----------
    def render_devices(self):
        for w in self.dev_frame.winfo_children():
            w.destroy()
        self.dev_vars = []
        for i, d in enumerate(self.cfg["devices"]):
            v_sel = tk.BooleanVar(value=d["selected"])
            v_name = tk.StringVar(value=d["name"])
            v_out = tk.BooleanVar(value=d["outdoor"])
            self.dev_vars.append((v_sel, v_name, v_out))
            ttk.Checkbutton(self.dev_frame, variable=v_sel,
                            command=lambda i=i: self.on_select(i)).grid(row=i, column=0)
            ttk.Entry(self.dev_frame, textvariable=v_name, width=24).grid(
                row=i, column=1, padx=4, pady=1)
            ttk.Checkbutton(self.dev_frame, text="Außen", variable=v_out).grid(row=i, column=2)
            offline = " (nicht in der Cloud gefunden)" if d.get("offline") else ""
            ttk.Label(self.dev_frame, text=d["id"] + offline).grid(
                row=i, column=3, sticky="w", padx=6)
            ttk.Button(self.dev_frame, text="↑", width=2,
                       command=lambda i=i: self.move(i, -1)).grid(row=i, column=4)
            ttk.Button(self.dev_frame, text="↓", width=2,
                       command=lambda i=i: self.move(i, +1)).grid(row=i, column=5)
        self.update_limit_label()

    def sync_devices(self):
        for d, (v_sel, v_name, v_out) in zip(self.cfg["devices"], self.dev_vars):
            d["selected"] = v_sel.get()
            d["name"] = v_name.get().strip() or d["id"]
            d["outdoor"] = v_out.get()

    def update_limit_label(self):
        limit = cs.device_limit(self.v_mode.get())
        count = sum(1 for v_sel, _, _ in self.dev_vars if v_sel.get())
        self.lbl_limit.config(text=f"{count}/{limit} ausgewählt")

    def on_select(self, i):
        limit = cs.device_limit(self.v_mode.get())
        count = sum(1 for v_sel, _, _ in self.dev_vars if v_sel.get())
        if count > limit:
            self.dev_vars[i][0].set(False)
            messagebox.showwarning(
                "Limit", f"In diesem Modus sind maximal {limit} Thermometer möglich.")
        self.update_limit_label()

    def on_mode_change(self):
        limit = cs.device_limit(self.v_mode.get())
        count = sum(1 for v_sel, _, _ in self.dev_vars if v_sel.get())
        if count > limit:
            messagebox.showwarning(
                "Limit", f"Es sind {count} Thermometer gewählt, der Modus erlaubt {limit}.\n"
                         "Bitte Auswahl reduzieren.")
        self.update_limit_label()

    def move(self, i, delta):
        j = i + delta
        if 0 <= j < len(self.cfg["devices"]):
            self.sync_devices()
            devs = self.cfg["devices"]
            devs[i], devs[j] = devs[j], devs[i]
            self.render_devices()

    def on_load_devices(self):
        if self.busy:
            return
        self.sync_devices()
        token, secret = self.v_token.get().strip(), self.v_secret.get().strip()
        if not token or not secret:
            messagebox.showerror("Fehlt", "SwitchBot-Token und -Secret eintragen.")
            return
        self.set_busy(True, "Frage SwitchBot-Cloud ab…")
        threading.Thread(target=self._load_devices_worker,
                         args=(token, secret), daemon=True).start()

    def _load_devices_worker(self, token, secret):
        try:
            meters = sb_api.list_meters(token, secret)
            self.msgq.put(("devices", meters))
        except RuntimeError as e:
            self.msgq.put(("error", str(e)))
        except Exception as e:
            self.msgq.put(("error", f"Unerwarteter Fehler bei der Cloud-Abfrage: {e}"))

    def merge_devices(self, meters):
        """Cloud-Liste einarbeiten: Bekanntes behalten, Neues anhaengen."""
        known = {d["id"]: d for d in self.cfg["devices"]}
        found = set()
        for m in meters:
            found.add(m["id"])
            if m["id"] in known:
                known[m["id"]].pop("offline", None)
            else:
                self.cfg["devices"].append(
                    {"id": m["id"], "name": m["name"], "outdoor": False, "selected": False})
        for d in self.cfg["devices"]:
            if d["id"] not in found:
                d["offline"] = True
        self.render_devices()

    # ---------- Speichern / Upload ----------
    def collect(self):
        self.sync_devices()
        try:
            float(self.v_lat.get()), float(self.v_lon.get())
        except ValueError:
            raise ValueError("Breite/Länge müssen Zahlen sein (Punkt als Dezimaltrenner).")
        mode = self.v_mode.get()
        sel = sum(1 for d in self.cfg["devices"] if d["selected"])
        if sel == 0:
            raise ValueError("Mindestens ein Thermometer auswählen.")
        if sel > cs.device_limit(mode):
            raise ValueError(f"Maximal {cs.device_limit(mode)} Thermometer in diesem Modus.")
        self.cfg.update(
            header_title=self.v_title.get().strip() or "SwitchBot Wetter",
            display_mode=mode,
            wifi_ssid=self.v_ssid.get().strip(), wifi_pass=self.v_pass.get(),
            sb_token=self.v_token.get().strip(), sb_secret=self.v_secret.get().strip(),
            lat=self.v_lat.get().strip(), lon=self.v_lon.get().strip(),
            tz=self.v_tz.get().strip())
        return self.cfg

    def on_save(self):
        try:
            cs.save(self.collect())
        except ValueError as e:
            messagebox.showerror("Ungültig", str(e))
            return False
        self.status.config(text="Gespeichert (JSON + Header generiert).")
        return True

    def on_upload(self):
        if self.busy or not self.on_save():
            return
        port = flasher.find_port()
        if not port:
            messagebox.showerror("Kein Board", flasher.PORT_HELP)
            return
        self.set_busy(True, f"Flashe über {port}…")
        self.log_clear()
        threading.Thread(target=self._upload_worker, args=(port,), daemon=True).start()

    def _upload_worker(self, port):
        ok, out = flasher.check_connection(port)
        if not ok:
            self.msgq.put(("log", out))
            self.msgq.put(("error", "Chip antwortet nicht.\n\n" + flasher.PORT_HELP))
            return
        ok = flasher.upload(port, lambda line: self.msgq.put(("log", line)))
        if ok:
            self.msgq.put(("done", "Upload erfolgreich!\n\nDas Board startet nach dem "
                                   "Flashen NICHT von selbst: PWR-Taste aus- und wieder "
                                   "einschalten, dann läuft die neue Firmware."))
        else:
            self.msgq.put(("error", "Upload fehlgeschlagen - Details im Log."))

    # ---------- Infrastruktur ----------
    def set_busy(self, busy, text=""):
        self.busy = busy
        state = "disabled" if busy else "normal"
        self.btn_save.config(state=state)
        self.btn_upload.config(state=state)
        self.btn_load.config(state=state)
        self.status.config(text=text)

    def log_clear(self):
        self.log.config(state="normal")
        self.log.delete("1.0", "end")
        self.log.config(state="disabled")

    def log_line(self, line):
        self.log.config(state="normal")
        self.log.insert("end", line + "\n")
        self.log.see("end")
        self.log.config(state="disabled")

    def poll_queue(self):
        try:
            while True:
                kind, payload = self.msgq.get_nowait()
                if kind == "log":
                    self.log_line(payload)
                elif kind == "devices":
                    self.sync_devices()  # aktuelle Eingaben sichern, bevor render_devices() ueberschreibt
                    self.merge_devices(payload)
                    self.set_busy(False, f"{len(payload)} Sensoren gefunden.")
                elif kind == "done":
                    self.set_busy(False, "Fertig.")
                    messagebox.showinfo("Fertig", payload)
                elif kind == "error":
                    self.set_busy(False, "Fehler.")
                    messagebox.showerror("Fehler", payload)
        except queue.Empty:
            pass
        self.root.after(100, self.poll_queue)


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
