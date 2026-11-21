#!/usr/bin/env python3
"""
KAOTI Ge-LAB PRO (Rev 4.0) — USB-C Serial Bridge.

Reads newline-delimited JSON emitted by the ESP32-S3 Parametric Engine over
USB-C (115200 baud) and forwards each measurement to the local web server.

Usage:
    python3 serial_bridge.py --port /dev/cu.usbmodem14101
    python3 serial_bridge.py --list-ports
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None


TEST_RESULT_TYPES = {"BJT", "JFET", "MOSFET", "DIODE", "RESISTOR", "CAPACITOR"}


def post_json(url: str, payload: dict, timeout: float = 5.0) -> dict:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def list_serial_ports():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports detected.")
        return []
    print("Available serial ports:")
    for p in ports:
        print(f"  {p.device:28s}  {p.description}")
    return ports


def auto_detect_port() -> str:
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        desc = f"{p.device} {p.description}".lower()
        if any(k in desc for k in ("usbmodem", "usbserial", "esp32", "cp210", "ch340", "jtag")):
            return p.device
    return ports[0].device if ports else ""


def build_curve_payload(sweep_points_by_curve: dict, done_msg: dict) -> dict:
    curves = []
    for c_idx in sorted(sweep_points_by_curve.keys()):
        pts = sweep_points_by_curve[c_idx]
        if not pts:
            continue
        ib_uA = pts[0].get("ib_uA", 0.0)
        curves.append(
            {
                "curve": c_idx,
                "ib_uA": ib_uA,
                "points": [{"vce_V": pt["vce_V"], "ic_mA": pt["ic_mA"]} for pt in pts],
            }
        )
    return {
        "material": done_msg.get("mat"),
        "polarity": done_msg.get("pol"),
        "vce_max_V": done_msg.get("max_vce_V", 9.0),
        "curves": curves,
    }


def main():
    parser = argparse.ArgumentParser(description="KAOTI Ge-LAB PRO ESP32 USB-C Serial Bridge")
    parser.add_argument("--port", default="", help="Serial port (e.g. /dev/cu.usbmodem101 or COM5)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--url", default="http://127.0.0.1:8080/api/tests", help="Target POST endpoint")
    parser.add_argument("--list-ports", action="store_true", help="List detected serial ports and exit")
    args = parser.parse_args()

    if serial is None:
        print(
            "ERROR: 'pyserial' is not installed.\n"
            "Install it with:\n"
            "    python3 -m pip install pyserial\n"
            "Then re-run serial_bridge.py (server.py itself needs zero packages).",
            file=sys.stderr,
        )
        sys.exit(1)

    if args.list_ports:
        list_serial_ports()
        return

    port = args.port or auto_detect_port()
    if not port:
        print("No serial port specified and none auto-detected. Use --list-ports or --port <DEV>.", file=sys.stderr)
        sys.exit(1)

    base_api_url = args.url.rstrip("/")
    print(f"[KAOTI Serial Bridge] Opening {port} @ {args.baud} baud -> {base_api_url}")

    last_hash = None
    sweep_pts = {}

    while True:
        try:
            with serial.Serial(port, args.baud, timeout=1.0) as ser:
                print(f"[CONNECTED] Listening to {port}...")
                while True:
                    raw_line = ser.readline()
                    if not raw_line:
                        continue
                    line = raw_line.decode("utf-8", errors="replace").strip()
                    if not line.startswith("{"):
                        continue

                    try:
                        msg = json.loads(line)
                    except json.JSONDecodeError:
                        print(f"[WARN] Malformed JSON line ignored: {line[:80]}")
                        continue

                    msg_type = (msg.get("type") or msg.get("dev_type") or "").upper()

                    if msg_type in TEST_RESULT_TYPES:
                        sweep_pts.clear()
                        try:
                            resp = post_json(base_api_url, msg)
                            last_hash = resp.get("hash")
                            print(f"[POST OK] {msg_type} -> hash={last_hash} (needs_photo=1)")
                        except urllib.error.URLError as exc:
                            print(f"[POST ERR] Could not reach {base_api_url}: {exc}")

                    elif msg_type == "SWEEP_PT":
                        c_idx = int(msg.get("curve", 0))
                        sweep_pts.setdefault(c_idx, []).append(
                            {
                                "ib_uA": float(msg.get("ib_uA", 0.0)),
                                "vce_V": float(msg.get("vce_V", 0.0)),
                                "ic_mA": float(msg.get("ic_mA", 0.0)),
                            }
                        )

                    elif msg_type == "SWEEP_DONE":
                        if last_hash and sweep_pts:
                            curve_payload = build_curve_payload(sweep_pts, msg)
                            curve_url = f"{base_api_url}/{last_hash}/curve"
                            try:
                                post_json(curve_url, curve_payload)
                                print(f"[CURVE OK] Attached {len(curve_payload['curves'])} curves to {last_hash}")
                            except urllib.error.URLError as exc:
                                print(f"[CURVE ERR] {exc}")
                        sweep_pts.clear()

                    elif msg_type in ("STATUS", "ABORT", "NONE"):
                        print(f"[{msg_type}] {msg.get('msg') or msg.get('reason') or ''}")

        except serial.SerialException as exc:
            print(f"[DISCONNECTED] {exc} — retrying in 3s...")
            time.sleep(3.0)
        except KeyboardInterrupt:
            print("\nSerial bridge stopped.")
            break


if __name__ == "__main__":
    main()
