#!/usr/bin/env python3
"""
KAOTI Ge-LAB PRO (Rev 4.0) — Companion Monitoring Web Server & SQLite Backend.

Runs strictly on the Python 3 standard library (zero external dependencies):
    python3 server.py [--host 0.0.0.0] [--port 8080] [--db transistors.db]
"""

import argparse
import base64
import csv
import datetime
import hashlib
import io
import json
import mimetypes
import os
import re
import socket
import sqlite3
import threading
import urllib.parse
import uuid
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DB_PATH = os.path.join(BASE_DIR, "transistors.db")
PHOTOS_DIR = os.path.join(BASE_DIR, "photos")
INDEX_HTML_PATH = os.path.join(BASE_DIR, "index.html")

DB_LOCK = threading.Lock()


def utc_now_iso() -> str:
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def safe_float(val):
    if val is None or val == "":
        return None
    if isinstance(val, bool):
        return None
    try:
        f = float(val)
        if f != f:  # NaN check
            return None
        return f
    except (TypeError, ValueError):
        return None


def sanitize_hash(raw_hash: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._-]", "", (raw_hash or "").strip())
    return cleaned[:64]


def generate_curve_family(is_ge: bool, is_pnp: bool, hfe: float, iceo_uA: float, vbe_mV: float):
    """Generate realistic synthetic 6-curve Ic(Vce, Ib) sweep data for demo records."""
    vce_points = [0.0, 0.15, 0.3, 0.6, 1.0, 2.0, 3.5, 5.0, 7.0, 9.0]
    ib_steps_uA = [10.0, 25.0, 50.0, 75.0, 100.0, 150.0]
    iceo_mA = max(0.0, (iceo_uA or 0.0) / 1000.0)
    early_v = 28.0 if is_ge else 65.0
    v_knee = 0.22 if is_ge else 0.35

    curves = []
    for ib in ib_steps_uA:
        pts = []
        ic_base = (hfe * ib) / 1000.0  # mA
        for vce in vce_points:
            if vce <= 0.0:
                ic = 0.0
            else:
                sat = 1.0 - pow(2.718281828, -vce / v_knee)
                early = 1.0 + (vce / early_v)
                leak = iceo_mA * (1.0 - pow(2.718281828, -vce / 0.4)) * (1.0 + vce / 18.0)
                ic = round(ic_base * sat * early + leak, 3)
            pts.append({"vce_V": vce, "ic_mA": ic})
        curves.append({"ib_uA": ib, "points": pts})
    return {
        "vce_max_V": 9.0,
        "polarity": "PNP" if is_pnp else "NPN",
        "material": "Ge" if is_ge else "Si",
        "vbe_mV": vbe_mV,
        "curves": curves,
    }


def generate_char_hash(payload: dict) -> str:
    """Build a human-readable KAOTI characterization hash like GEP-092-L048-A41C."""
    dev_type = (payload.get("dev_type") or "DEV").upper()
    mat = (payload.get("material") or "").upper()
    pol = (payload.get("polarity") or "").upper()

    if dev_type == "BJT":
        m_char = "G" if mat == "GE" else ("S" if mat == "SI" else "B")
        e_char = "E" if mat == "GE" else ("I" if mat == "SI" else "J")
        p_char = "P" if "PNP" in pol else ("N" if "NPN" in pol else "X")
        prefix = f"{m_char}{e_char}{p_char}"
        hfe = safe_float(payload.get("hfe_true")) or safe_float(payload.get("hfe_raw")) or 0.0
        iceo = safe_float(payload.get("iceo_uA")) or 0.0
        mid = f"{int(round(max(0.0, min(9999.0, hfe)))):03d}-L{int(round(max(0.0, min(9999.0, iceo)))):03d}"
    elif dev_type == "JFET":
        prefix = "JFP" if "P" in pol else "JFN"
        idss = safe_float(payload.get("idss_mA")) or 0.0
        vgs = abs(safe_float(payload.get("vgsoff_V")) or 0.0)
        mid = f"I{int(round(idss * 10)):03d}-V{int(round(vgs * 10)):02d}"
    elif dev_type == "MOSFET":
        prefix = "MSP" if "P" in pol else "MSN"
        vth = abs(safe_float(payload.get("vgsth_V")) or 0.0)
        mid = f"VT{int(round(vth * 100)):03d}"
    elif dev_type == "DIODE":
        prefix = "DIO"
        vbe = safe_float(payload.get("vbe_mV")) or 0.0
        mid = f"VF{int(round(vbe)):04d}"
    elif dev_type == "RESISTOR":
        prefix = "RES"
        r = safe_float(payload.get("r_ohm")) or 0.0
        mid = f"R{int(round(min(999999, r))):04d}"
    elif dev_type == "CAPACITOR":
        prefix = "CAP"
        c = safe_float(payload.get("c_nF")) or 0.0
        mid = f"C{int(round(min(999999, c))):04d}"
    else:
        prefix = dev_type[:3].ljust(3, "X")
        mid = "000"

    seed_str = f"{json.dumps(payload, sort_keys=True)}-{uuid.uuid4().hex}"
    digest = hashlib.sha1(seed_str.encode("utf-8")).hexdigest()[:4].upper()
    return f"{prefix}-{mid}-{digest}"


def normalize_test_payload(raw: dict) -> dict:
    """Normalize both canonical REST payloads and native ESP32 firmware JSON lines."""
    dev_type = (raw.get("dev_type") or raw.get("type") or "UNKNOWN").upper()
    material = raw.get("material") or raw.get("mat") or ""
    polarity = raw.get("polarity") or raw.get("pol") or ""
    pins = raw.get("pins") or ""

    if dev_type == "DIODE" and not material:
        subtype = str(raw.get("subtype") or "")
        if "Ge" in subtype:
            material = "Ge"
        elif "Si" in subtype:
            material = "Si"
        if subtype and not polarity:
            polarity = subtype

    # Normalize material capitalization
    if material.lower() == "ge":
        material = "Ge"
    elif material.lower() == "si":
        material = "Si"

    vbe_mV = safe_float(raw.get("vbe_mV"))
    if vbe_mV is None:
        vbe_mV = safe_float(raw.get("vf_2mA_mV")) or safe_float(raw.get("vf_gate_mV")) or safe_float(raw.get("vf_body_mV"))

    vgsth_V = safe_float(raw.get("vgsth_V"))
    if vgsth_V is None:
        vgsth_V = safe_float(raw.get("vth_V"))

    r_ohm = safe_float(raw.get("r_ohm"))
    if r_ohm is None:
        r_ohm = safe_float(raw.get("rds_4v5_ohm"))

    iceo_uA = safe_float(raw.get("iceo_uA"))
    if iceo_uA is None and dev_type == "DIODE":
        iceo_uA = safe_float(raw.get("irev_uA"))

    curve_val = raw.get("curve_json")
    if curve_val is None and "curve" in raw:
        curve_val = raw.get("curve")
    if curve_val is None and "curves" in raw:
        curve_val = {"curves": raw.get("curves")}
    if isinstance(curve_val, (dict, list)):
        curve_json = json.dumps(curve_val)
    elif isinstance(curve_val, str) and curve_val.strip():
        curve_json = curve_val.strip()
    else:
        curve_json = None

    normalized = {
        "hash": sanitize_hash(raw.get("hash") or ""),
        "created_at": raw.get("created_at") or utc_now_iso(),
        "dev_type": dev_type,
        "material": material or None,
        "polarity": polarity or None,
        "pins": pins or None,
        "hfe_true": safe_float(raw.get("hfe_true")),
        "hfe_raw": safe_float(raw.get("hfe_raw")),
        "iceo_uA": iceo_uA,
        "icbo_uA": safe_float(raw.get("icbo_uA")),
        "vbe_mV": vbe_mV,
        "vcesat_mV": safe_float(raw.get("vcesat_mV")),
        "idss_mA": safe_float(raw.get("idss_mA")),
        "vgsoff_V": safe_float(raw.get("vgsoff_V")),
        "vgsth_V": vgsth_V,
        "r_ohm": r_ohm,
        "c_nF": safe_float(raw.get("c_nF")),
        "temp_C": safe_float(raw.get("temp_C")),
        "curve_json": curve_json,
        "notes": (raw.get("notes") or "").strip() or None,
    }
    if not normalized["hash"]:
        normalized["hash"] = generate_char_hash(normalized)
    return normalized


class TransistorDB:
    def __init__(self, db_path: str):
        self.db_path = db_path
        os.makedirs(os.path.dirname(os.path.abspath(db_path)), exist_ok=True)
        os.makedirs(PHOTOS_DIR, exist_ok=True)
        self._init_schema()
        self._seed_demo_if_empty()

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path, timeout=15.0)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_schema(self):
        with DB_LOCK, self._connect() as conn:
            conn.execute("PRAGMA journal_mode=WAL;")
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS transistors (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    hash TEXT UNIQUE NOT NULL,
                    created_at TEXT NOT NULL,
                    dev_type TEXT,
                    material TEXT,
                    polarity TEXT,
                    pins TEXT,
                    hfe_true REAL,
                    hfe_raw REAL,
                    iceo_uA REAL,
                    icbo_uA REAL,
                    vbe_mV REAL,
                    vcesat_mV REAL,
                    idss_mA REAL,
                    vgsoff_V REAL,
                    vgsth_V REAL,
                    r_ohm REAL,
                    c_nF REAL,
                    temp_C REAL,
                    curve_json TEXT,
                    photo_path TEXT,
                    notes TEXT,
                    needs_photo INTEGER NOT NULL DEFAULT 1
                )
                """
            )
            conn.commit()

    def _seed_demo_if_empty(self):
        with DB_LOCK, self._connect() as conn:
            count = conn.execute("SELECT COUNT(*) FROM transistors").fetchone()[0]
            if count > 0:
                return

            now = datetime.datetime.now(datetime.timezone.utc)
            demos = [
                {
                    "hash": "GEP-092-L048-A41C",
                    "created_at": (now - datetime.timedelta(minutes=42)).strftime("%Y-%m-%dT%H:%M:%SZ"),
                    "dev_type": "BJT",
                    "material": "Ge",
                    "polarity": "PNP",
                    "pins": "1:E 2:B 3:C",
                    "hfe_true": 92.4,
                    "hfe_raw": 95.1,
                    "iceo_uA": 48.0,
                    "icbo_uA": 4.2,
                    "vbe_mV": 142.0,
                    "vcesat_mV": 68.0,
                    "temp_C": 24.6,
                    "curve_json": json.dumps(generate_curve_family(True, True, 92.4, 48.0, 142.0)),
                    "photo_path": None,
                    "notes": "Mullard OC44 (Black Glass) — Ideal Fuzz Face Q1 / clean gain",
                    "needs_photo": 0,
                },
                {
                    "hash": "GEP-128-L115-3F90",
                    "created_at": (now - datetime.timedelta(minutes=24)).strftime("%Y-%m-%dT%H:%M:%SZ"),
                    "dev_type": "BJT",
                    "material": "Ge",
                    "polarity": "PNP",
                    "pins": "1:E 2:B 3:C",
                    "hfe_true": 128.0,
                    "hfe_raw": 136.5,
                    "iceo_uA": 115.0,
                    "icbo_uA": 11.4,
                    "vbe_mV": 136.0,
                    "vcesat_mV": 74.0,
                    "temp_C": 24.8,
                    "curve_json": json.dumps(generate_curve_family(True, True, 128.0, 115.0, 136.0)),
                    "photo_path": None,
                    "notes": "Valvo / Tungsram AC128 — Fuzz Face Q2 / Rangemaster candidate",
                    "needs_photo": 0,
                },
                {
                    "hash": "GEN-104-L062-8B2E",
                    "created_at": (now - datetime.timedelta(minutes=11)).strftime("%Y-%m-%dT%H:%M:%SZ"),
                    "dev_type": "BJT",
                    "material": "Ge",
                    "polarity": "NPN",
                    "pins": "1:E 2:B 3:C",
                    "hfe_true": 104.2,
                    "hfe_raw": 108.9,
                    "iceo_uA": 62.0,
                    "icbo_uA": 5.8,
                    "vbe_mV": 138.0,
                    "vcesat_mV": 64.0,
                    "temp_C": 24.7,
                    "curve_json": json.dumps(generate_curve_family(True, False, 104.2, 62.0, 138.0)),
                    "photo_path": None,
                    "notes": "Siemens AC187 — Complementary Ge NPN for Class-AB driver stage",
                    "needs_photo": 0,
                },
            ]
            cols = (
                "hash, created_at, dev_type, material, polarity, pins, "
                "hfe_true, hfe_raw, iceo_uA, icbo_uA, vbe_mV, vcesat_mV, "
                "idss_mA, vgsoff_V, vgsth_V, r_ohm, c_nF, temp_C, "
                "curve_json, photo_path, notes, needs_photo"
            )
            for d in demos:
                conn.execute(
                    f"INSERT INTO transistors ({cols}) VALUES "
                    "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (
                        d["hash"], d["created_at"], d["dev_type"], d["material"], d["polarity"], d["pins"],
                        d["hfe_true"], d["hfe_raw"], d["iceo_uA"], d["icbo_uA"], d["vbe_mV"], d["vcesat_mV"],
                        None, None, None, None, None, d["temp_C"],
                        d["curve_json"], d["photo_path"], d["notes"], d["needs_photo"],
                    ),
                )
            conn.commit()

    @staticmethod
    def row_to_dict(row: sqlite3.Row) -> dict:
        d = dict(row)
        curve_obj = None
        if d.get("curve_json"):
            try:
                curve_obj = json.loads(d["curve_json"])
            except Exception:
                curve_obj = None
        d["curve"] = curve_obj
        photo_path = d.get("photo_path")
        if photo_path:
            fname = os.path.basename(photo_path)
            d["photo_url"] = f"/photos/{fname}"
        else:
            d["photo_url"] = None
        return d

    def list_tests(self, filters: dict) -> list:
        clauses = []
        params = []

        dev_type = filters.get("type") or filters.get("dev_type")
        if dev_type and dev_type.upper() != "ALL":
            clauses.append("UPPER(dev_type) = ?")
            params.append(dev_type.strip().upper())

        material = filters.get("material")
        if material and material.upper() != "ALL":
            clauses.append("UPPER(COALESCE(material, '')) = ?")
            params.append(material.strip().upper())

        polarity = filters.get("polarity")
        if polarity and polarity.upper() != "ALL":
            clauses.append("UPPER(COALESCE(polarity, '')) = ?")
            params.append(polarity.strip().upper())

        min_hfe = safe_float(filters.get("min_hfe"))
        if min_hfe is not None:
            clauses.append("hfe_true >= ?")
            params.append(min_hfe)

        max_hfe = safe_float(filters.get("max_hfe"))
        if max_hfe is not None:
            clauses.append("hfe_true <= ?")
            params.append(max_hfe)

        max_iceo = safe_float(filters.get("max_iceo"))
        if max_iceo is not None:
            clauses.append("iceo_uA <= ?")
            params.append(max_iceo)

        needs_photo = filters.get("needs_photo")
        if needs_photo in ("0", "1"):
            clauses.append("needs_photo = ?")
            params.append(int(needs_photo))

        search = (filters.get("search") or "").strip()
        if search:
            like = f"%{search}%"
            clauses.append(
                "(hash LIKE ? OR COALESCE(notes, '') LIKE ? OR COALESCE(dev_type, '') LIKE ? "
                "OR COALESCE(material, '') LIKE ? OR COALESCE(polarity, '') LIKE ?)"
            )
            params.extend([like, like, like, like, like])

        where_sql = ("WHERE " + " AND ".join(clauses)) if clauses else ""
        sql = f"SELECT * FROM transistors {where_sql} ORDER BY id DESC"

        with DB_LOCK, self._connect() as conn:
            rows = conn.execute(sql, params).fetchall()
        return [self.row_to_dict(r) for r in rows]

    def get_by_hash(self, item_hash: str):
        with DB_LOCK, self._connect() as conn:
            row = conn.execute("SELECT * FROM transistors WHERE hash = ?", (item_hash,)).fetchone()
        return self.row_to_dict(row) if row else None

    def insert_test(self, raw_payload: dict) -> dict:
        item = normalize_test_payload(raw_payload)
        with DB_LOCK, self._connect() as conn:
            # Ensure hash uniqueness if caller supplied a duplicate hash
            existing = conn.execute("SELECT id FROM transistors WHERE hash = ?", (item["hash"],)).fetchone()
            if existing:
                item["hash"] = f"{item['hash']}-{uuid.uuid4().hex[:4].upper()}"

            conn.execute(
                """
                INSERT INTO transistors (
                    hash, created_at, dev_type, material, polarity, pins,
                    hfe_true, hfe_raw, iceo_uA, icbo_uA, vbe_mV, vcesat_mV,
                    idss_mA, vgsoff_V, vgsth_V, r_ohm, c_nF, temp_C,
                    curve_json, photo_path, notes, needs_photo
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, 1)
                """,
                (
                    item["hash"], item["created_at"], item["dev_type"], item["material"],
                    item["polarity"], item["pins"], item["hfe_true"], item["hfe_raw"],
                    item["iceo_uA"], item["icbo_uA"], item["vbe_mV"], item["vcesat_mV"],
                    item["idss_mA"], item["vgsoff_V"], item["vgsth_V"], item["r_ohm"],
                    item["c_nF"], item["temp_C"], item["curve_json"], item["notes"],
                ),
            )
            conn.commit()
            row = conn.execute("SELECT * FROM transistors WHERE hash = ?", (item["hash"],)).fetchone()
        return self.row_to_dict(row)

    def get_pending_photo(self) -> tuple:
        with DB_LOCK, self._connect() as conn:
            row = conn.execute(
                "SELECT * FROM transistors WHERE needs_photo = 1 ORDER BY id DESC LIMIT 1"
            ).fetchone()
            count = conn.execute("SELECT COUNT(*) FROM transistors WHERE needs_photo = 1").fetchone()[0]
        return (self.row_to_dict(row) if row else None, int(count))

    def save_photo(self, item_hash: str, image_bytes: bytes) -> dict:
        clean_hash = sanitize_hash(item_hash)
        if not clean_hash:
            return None
        with DB_LOCK, self._connect() as conn:
            row = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
            if not row:
                return None

            filename = f"{clean_hash}.jpg"
            abs_path = os.path.join(PHOTOS_DIR, filename)
            with open(abs_path, "wb") as f:
                f.write(image_bytes)

            rel_path = f"photos/{filename}"
            conn.execute(
                "UPDATE transistors SET photo_path = ?, needs_photo = 0 WHERE hash = ?",
                (rel_path, clean_hash),
            )
            conn.commit()
            updated = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
        return self.row_to_dict(updated)

    def set_needs_photo(self, item_hash: str, needs_photo: int) -> dict:
        clean_hash = sanitize_hash(item_hash)
        with DB_LOCK, self._connect() as conn:
            row = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
            if not row:
                return None
            conn.execute(
                "UPDATE transistors SET needs_photo = ? WHERE hash = ?",
                (1 if needs_photo else 0, clean_hash),
            )
            conn.commit()
            updated = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
        return self.row_to_dict(updated)

    def update_curve(self, item_hash: str, curve_obj) -> dict:
        clean_hash = sanitize_hash(item_hash)
        curve_str = curve_obj if isinstance(curve_obj, str) else json.dumps(curve_obj)
        with DB_LOCK, self._connect() as conn:
            row = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
            if not row:
                return None
            conn.execute(
                "UPDATE transistors SET curve_json = ? WHERE hash = ?",
                (curve_str, clean_hash),
            )
            conn.commit()
            updated = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
        return self.row_to_dict(updated)

    def update_notes(self, item_hash: str, notes: str) -> dict:
        clean_hash = sanitize_hash(item_hash)
        with DB_LOCK, self._connect() as conn:
            row = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
            if not row:
                return None
            conn.execute(
                "UPDATE transistors SET notes = ? WHERE hash = ?",
                (notes.strip() if notes else None, clean_hash),
            )
            conn.commit()
            updated = conn.execute("SELECT * FROM transistors WHERE hash = ?", (clean_hash,)).fetchone()
        return self.row_to_dict(updated)

    def delete_test(self, item_hash: str) -> bool:
        clean_hash = sanitize_hash(item_hash)
        with DB_LOCK, self._connect() as conn:
            cur = conn.execute("DELETE FROM transistors WHERE hash = ?", (clean_hash,))
            conn.commit()
            return cur.rowcount > 0

    def count_all(self) -> int:
        with DB_LOCK, self._connect() as conn:
            return int(conn.execute("SELECT COUNT(*) FROM transistors").fetchone()[0])

    def match_preset(self, preset: str) -> dict:
        preset = (preset or "fuzz_face").strip().lower()
        all_rows = self.list_tests({})

        if preset == "fuzz_face":
            ge_pnp = [
                r for r in all_rows
                if (r.get("dev_type") or "").upper() == "BJT"
                and (r.get("material") or "").lower() == "ge"
                and (r.get("polarity") or "").upper() == "PNP"
                and r.get("hfe_true") is not None
                and r.get("iceo_uA") is not None
            ]
            q1_strict = [r for r in ge_pnp if 75.0 <= r["hfe_true"] <= 95.0 and r["iceo_uA"] < 120.0]
            q2_strict = [r for r in ge_pnp if 110.0 <= r["hfe_true"] <= 140.0 and r["iceo_uA"] < 220.0]

            pairs = []
            strict_match = True
            for q1 in q1_strict:
                for q2 in q2_strict:
                    if q1["hash"] == q2["hash"]:
                        continue
                    score = (
                        abs(q1["hfe_true"] - 85.0) * 1.2
                        + abs(q2["hfe_true"] - 125.0) * 1.0
                        + q1["iceo_uA"] * 0.12
                        + q2["iceo_uA"] * 0.08
                    )
                    pairs.append({
                        "score": round(score, 2),
                        "strict_match": True,
                        "q1": q1,
                        "q2": q2,
                        "ratio_q2_q1": round(q2["hfe_true"] / max(q1["hfe_true"], 1.0), 2),
                        "summary": (
                            f"Q1 hFE {q1['hfe_true']:.1f} (Iceo {q1['iceo_uA']:.0f} uA) + "
                            f"Q2 hFE {q2['hfe_true']:.1f} (Iceo {q2['iceo_uA']:.0f} uA)"
                        ),
                    })

            if not pairs and len(ge_pnp) >= 2:
                strict_match = False
                for i, q1 in enumerate(ge_pnp):
                    for j, q2 in enumerate(ge_pnp):
                        if i == j or q2["hfe_true"] < q1["hfe_true"]:
                            continue
                        score = (
                            abs(q1["hfe_true"] - 85.0) * 1.2
                            + abs(q2["hfe_true"] - 125.0) * 1.0
                            + q1["iceo_uA"] * 0.15
                            + q2["iceo_uA"] * 0.10
                        )
                        pairs.append({
                            "score": round(score, 2),
                            "strict_match": False,
                            "q1": q1,
                            "q2": q2,
                            "ratio_q2_q1": round(q2["hfe_true"] / max(q1["hfe_true"], 1.0), 2),
                            "summary": (
                                f"Near match: Q1 hFE {q1['hfe_true']:.1f} + Q2 hFE {q2['hfe_true']:.1f}"
                            ),
                        })

            pairs.sort(key=lambda p: p["score"])
            return {
                "preset": "fuzz_face",
                "title": "Fuzz Face (Ge PNP Q1 + Q2)",
                "criteria": "Q1: hFE 75-95, Iceo < 120 uA | Q2: hFE 110-140, Iceo < 220 uA",
                "strict_match": strict_match if pairs else False,
                "count": len(pairs),
                "matches": pairs[:10],
            }

        if preset == "rangemaster":
            ge_pnp = [
                r for r in all_rows
                if (r.get("dev_type") or "").upper() == "BJT"
                and (r.get("material") or "").lower() == "ge"
                and (r.get("polarity") or "").upper() == "PNP"
                and r.get("hfe_true") is not None
                and r.get("iceo_uA") is not None
            ]
            strict = [r for r in ge_pnp if 65.0 <= r["hfe_true"] <= 85.0 and 80.0 <= r["iceo_uA"] <= 180.0]
            strict_match = bool(strict)
            pool = strict if strict else ge_pnp
            candidates = []
            for r in pool:
                score = abs(r["hfe_true"] - 75.0) * 1.5 + abs(r["iceo_uA"] - 125.0) * 0.25
                candidates.append({
                    "score": round(score, 2),
                    "strict_match": strict_match,
                    "device": r,
                    "summary": f"hFE {r['hfe_true']:.1f} | Iceo {r['iceo_uA']:.0f} uA | Vbe {r.get('vbe_mV') or 0:.0f} mV",
                })
            candidates.sort(key=lambda c: c["score"])
            return {
                "preset": "rangemaster",
                "title": "Dallas Rangemaster (Ge PNP Treble Booster)",
                "criteria": "Ge PNP: hFE 65-85, Iceo 80-180 uA (sweet-spot self-bias leakage)",
                "strict_match": strict_match,
                "count": len(candidates),
                "matches": candidates[:10],
            }

        if preset == "class_ab":
            ge_bjts = [
                r for r in all_rows
                if (r.get("dev_type") or "").upper() == "BJT"
                and (r.get("material") or "").lower() == "ge"
                and r.get("hfe_true") is not None
            ]
            npns = [r for r in ge_bjts if (r.get("polarity") or "").upper() == "NPN"]
            pnps = [r for r in ge_bjts if (r.get("polarity") or "").upper() == "PNP"]
            pairs = []
            for n in npns:
                for p in pnps:
                    dhfe = abs(n["hfe_true"] - p["hfe_true"])
                    max_hfe = max(n["hfe_true"], p["hfe_true"], 1.0)
                    hfe_pct = (dhfe / max_hfe) * 100.0
                    vbe_n = n.get("vbe_mV") or 0.0
                    vbe_p = p.get("vbe_mV") or 0.0
                    dvbe = abs(vbe_n - vbe_p)
                    diceo = abs((n.get("iceo_uA") or 0.0) - (p.get("iceo_uA") or 0.0))
                    score = hfe_pct + dvbe * 0.5 + diceo * 0.08
                    pairs.append({
                        "score": round(score, 2),
                        "strict_match": hfe_pct <= 25.0 and dvbe <= 25.0,
                        "npn": n,
                        "pnp": p,
                        "delta_hfe": round(dhfe, 1),
                        "hfe_mismatch_pct": round(hfe_pct, 1),
                        "delta_vbe_mV": round(dvbe, 1),
                        "delta_iceo_uA": round(diceo, 1),
                        "summary": (
                            f"NPN {n['hfe_true']:.1f} / PNP {p['hfe_true']:.1f} "
                            f"(dhFE {hfe_pct:.1f}%, dVbe {dvbe:.0f} mV)"
                        ),
                    })
            pairs.sort(key=lambda x: x["score"])
            return {
                "preset": "class_ab",
                "title": "Class-AB Complementary Ge NPN / PNP Pair",
                "criteria": "Minimize |hFE_NPN - hFE_PNP| and |Vbe_NPN - Vbe_PNP| (e.g. AC187 / AC188)",
                "strict_match": any(p["strict_match"] for p in pairs),
                "count": len(pairs),
                "matches": pairs[:10],
            }

        if preset == "jfet_pair":
            jfets = [
                r for r in all_rows
                if (r.get("dev_type") or "").upper() == "JFET"
                and r.get("idss_mA") is not None
                and r.get("vgsoff_V") is not None
            ]
            pairs = []
            for i in range(len(jfets)):
                for j in range(i + 1, len(jfets)):
                    a, b = jfets[i], jfets[j]
                    if (a.get("polarity") or "") != (b.get("polarity") or ""):
                        continue
                    didss = abs(a["idss_mA"] - b["idss_mA"])
                    max_idss = max(abs(a["idss_mA"]), abs(b["idss_mA"]), 0.01)
                    idss_pct = (didss / max_idss) * 100.0
                    dvgs = abs(a["vgsoff_V"] - b["vgsoff_V"])
                    score = idss_pct + dvgs * 25.0
                    pairs.append({
                        "score": round(score, 2),
                        "strict_match": idss_pct <= 10.0 and dvgs <= 0.25,
                        "j1": a,
                        "j2": b,
                        "delta_idss_mA": round(didss, 3),
                        "idss_mismatch_pct": round(idss_pct, 1),
                        "delta_vgsoff_V": round(dvgs, 3),
                        "summary": (
                            f"Idss {a['idss_mA']:.2f}/{b['idss_mA']:.2f} mA ({idss_pct:.1f}%), "
                            f"dVgs(off) {dvgs:.2f} V"
                        ),
                    })
            pairs.sort(key=lambda x: x["score"])
            return {
                "preset": "jfet_pair",
                "title": "Matched JFET Pair (Phase90 / Diff-Amp)",
                "criteria": "Match same-polarity JFETs by Idss (<10% delta) and Vgs(off) (<0.25 V delta)",
                "strict_match": any(p["strict_match"] for p in pairs),
                "count": len(pairs),
                "matches": pairs[:10],
            }

        return {
            "preset": preset,
            "title": "Unknown Preset",
            "criteria": "Use fuzz_face, rangemaster, class_ab, or jfet_pair",
            "strict_match": False,
            "count": 0,
            "matches": [],
        }


def extract_multipart_file(content_type: str, body: bytes) -> bytes:
    """Parse multipart/form-data without deprecated/removed cgi module."""
    match = re.search(r'boundary=(?:"([^"]+)"|([^;\s]+))', content_type)
    if not match:
        raise ValueError("Missing multipart boundary")
    boundary = (match.group(1) or match.group(2)).encode("latin-1")
    delimiter = b"--" + boundary
    parts = body.split(delimiter)
    for part in parts:
        if not part or part.strip() in (b"", b"--"):
            continue
        if part.startswith(b"\r\n"):
            part = part[2:]
        header_end = part.find(b"\r\n\r\n")
        if header_end == -1:
            continue
        headers_raw = part[:header_end].decode("latin-1", errors="replace")
        payload = part[header_end + 4 :]
        if payload.endswith(b"\r\n"):
            payload = payload[:-2]
        if "filename=" in headers_raw or "image/" in headers_raw.lower():
            if payload:
                return payload
    raise ValueError("No image file field found in multipart payload")


def decode_photo_request(content_type: str, body: bytes) -> bytes:
    ct = (content_type or "").lower()
    if "multipart/form-data" in ct:
        return extract_multipart_file(content_type, body)
    if "image/" in ct or "application/octet-stream" in ct:
        if not body:
            raise ValueError("Empty image body")
        return body

    # Default: JSON with base64 string
    try:
        data = json.loads(body.decode("utf-8"))
    except Exception as exc:
        raise ValueError(f"Unsupported photo payload: {exc}") from exc

    b64_val = (
        data.get("image_base64")
        or data.get("photo")
        or data.get("image")
        or data.get("data")
        or ""
    )
    if not b64_val:
        raise ValueError("Missing image_base64 field in JSON payload")
    if "," in b64_val and b64_val.strip().lower().startswith("data:"):
        b64_val = b64_val.split(",", 1)[1]
    return base64.b64decode(b64_val)


class GeLabHandler(BaseHTTPRequestHandler):
    db: TransistorDB = None  # injected by server factory
    server_version = "KAOTI-GeLAB-PRO/4.0"

    def log_message(self, fmt, *args):
        print(f"[{utc_now_iso()}] {self.client_address[0]} - {fmt % args}")

    def _send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PATCH, PUT, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")

    def _send_json(self, status: int, obj: dict):
        raw = json.dumps(obj, indent=None).encode("utf-8")
        self.send_response(status)
        self._send_cors_headers()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def _send_bytes(self, status: int, content_type: str, data: bytes, cache_control: str = "no-cache"):
        self.send_response(status)
        self._send_cors_headers()
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", cache_control)
        self.end_headers()
        self.wfile.write(data)

    def _read_body(self, max_bytes: int = 25 * 1024 * 1024) -> bytes:
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0:
            return b""
        if length > max_bytes:
            raise ValueError("Request payload too large")
        return self.rfile.read(length)

    def do_OPTIONS(self):
        self.send_response(HTTPStatus.NO_CONTENT)
        self._send_cors_headers()
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        qs = urllib.parse.parse_qs(parsed.query)
        flat_qs = {k: v[0] for k, v in qs.items() if v}

        if path in ("/", "/index.html"):
            if not os.path.isfile(INDEX_HTML_PATH):
                self._send_json(404, {"ok": False, "error": "index.html not found"})
                return
            with open(INDEX_HTML_PATH, "rb") as f:
                self._send_bytes(200, "text/html; charset=utf-8", f.read(), cache_control="no-cache")
            return

        if path == "/api/health":
            self._send_json(200, {"ok": True, "version": "4.0", "count": self.db.count_all()})
            return

        if path == "/api/tests":
            tests = self.db.list_tests(flat_qs)
            self._send_json(200, {"ok": True, "count": len(tests), "tests": tests})
            return

        if path.startswith("/api/tests/"):
            item_hash = sanitize_hash(urllib.parse.unquote(path[len("/api/tests/"):]))
            rec = self.db.get_by_hash(item_hash)
            if not rec:
                self._send_json(404, {"ok": False, "error": f"Test {item_hash} not found"})
                return
            self._send_json(200, {"ok": True, "test": rec})
            return

        if path == "/api/pending-photo":
            pending, pending_count = self.db.get_pending_photo()
            self._send_json(
                200,
                {
                    "ok": True,
                    "pending": pending,
                    "pending_count": pending_count,
                    "has_pending": pending is not None,
                },
            )
            return

        if path == "/api/match":
            preset = flat_qs.get("preset", "fuzz_face")
            result = self.db.match_preset(preset)
            self._send_json(200, {"ok": True, **result})
            return

        if path == "/api/export.csv":
            tests = self.db.list_tests(flat_qs)
            buf = io.StringIO()
            fields = [
                "id", "hash", "created_at", "dev_type", "material", "polarity", "pins",
                "hfe_true", "hfe_raw", "iceo_uA", "icbo_uA", "vbe_mV", "vcesat_mV",
                "idss_mA", "vgsoff_V", "vgsth_V", "r_ohm", "c_nF", "temp_C",
                "photo_path", "needs_photo", "notes",
            ]
            writer = csv.DictWriter(buf, fieldnames=fields)
            writer.writeheader()
            for t in tests:
                writer.writerow({k: t.get(k) for k in fields})
            csv_bytes = buf.getvalue().encode("utf-8")
            self.send_response(200)
            self._send_cors_headers()
            self.send_header("Content-Type", "text/csv; charset=utf-8")
            self.send_header("Content-Disposition", 'attachment; filename="kaoti_gelab_export.csv"')
            self.send_header("Content-Length", str(len(csv_bytes)))
            self.end_headers()
            self.wfile.write(csv_bytes)
            return

        if path.startswith("/photos/"):
            raw_name = urllib.parse.unquote(path[len("/photos/"):])
            safe_name = os.path.basename(raw_name)
            if not safe_name or safe_name != raw_name or ".." in safe_name:
                self._send_json(400, {"ok": False, "error": "Invalid photo filename"})
                return
            full_path = os.path.join(PHOTOS_DIR, safe_name)
            if not os.path.isfile(full_path):
                self._send_json(404, {"ok": False, "error": "Photo not found"})
                return
            mime, _ = mimetypes.guess_type(full_path)
            with open(full_path, "rb") as f:
                self._send_bytes(200, mime or "image/jpeg", f.read(), cache_control="no-cache")
            return

        self._send_json(404, {"ok": False, "error": f"Unknown endpoint: {path}"})

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/api/tests":
            try:
                body = self._read_body()
                payload = json.loads(body.decode("utf-8"))
                if not isinstance(payload, dict):
                    raise ValueError("JSON body must be an object")
                created = self.db.insert_test(payload)
                self._send_json(201, {"ok": True, "id": created["id"], "hash": created["hash"], "test": created})
            except Exception as exc:
                self._send_json(400, {"ok": False, "error": str(exc)})
            return

        m_photo = re.match(r"^/api/tests/([^/]+)/photo$", path)
        if m_photo:
            item_hash = sanitize_hash(urllib.parse.unquote(m_photo.group(1)))
            try:
                body = self._read_body()
                img_bytes = decode_photo_request(self.headers.get("Content-Type", ""), body)
                updated = self.db.save_photo(item_hash, img_bytes)
                if not updated:
                    self._send_json(404, {"ok": False, "error": f"Test {item_hash} not found"})
                    return
                self._send_json(
                    200,
                    {
                        "ok": True,
                        "hash": updated["hash"],
                        "photo_path": updated["photo_path"],
                        "photo_url": updated["photo_url"],
                        "needs_photo": updated["needs_photo"],
                        "test": updated,
                    },
                )
            except Exception as exc:
                self._send_json(400, {"ok": False, "error": str(exc)})
            return

        m_skip = re.match(r"^/api/tests/([^/]+)/skip-photo$", path)
        if m_skip:
            item_hash = sanitize_hash(urllib.parse.unquote(m_skip.group(1)))
            updated = self.db.set_needs_photo(item_hash, 0)
            if not updated:
                self._send_json(404, {"ok": False, "error": f"Test {item_hash} not found"})
                return
            self._send_json(200, {"ok": True, "test": updated})
            return

        m_req_photo = re.match(r"^/api/tests/([^/]+)/request-photo$", path)
        if m_req_photo:
            item_hash = sanitize_hash(urllib.parse.unquote(m_req_photo.group(1)))
            updated = self.db.set_needs_photo(item_hash, 1)
            if not updated:
                self._send_json(404, {"ok": False, "error": f"Test {item_hash} not found"})
                return
            self._send_json(200, {"ok": True, "test": updated})
            return

        m_curve = re.match(r"^/api/tests/([^/]+)/curve$", path)
        if m_curve:
            item_hash = sanitize_hash(urllib.parse.unquote(m_curve.group(1)))
            try:
                body = self._read_body()
                payload = json.loads(body.decode("utf-8"))
                updated = self.db.update_curve(item_hash, payload)
                if not updated:
                    self._send_json(404, {"ok": False, "error": f"Test {item_hash} not found"})
                    return
                self._send_json(200, {"ok": True, "test": updated})
            except Exception as exc:
                self._send_json(400, {"ok": False, "error": str(exc)})
            return

        self._send_json(404, {"ok": False, "error": f"Unknown POST endpoint: {path}"})

    def do_PATCH(self):
        parsed = urllib.parse.urlparse(self.path)
        m = re.match(r"^/api/tests/([^/]+)$", parsed.path)
        if not m:
            self._send_json(404, {"ok": False, "error": "Unknown PATCH endpoint"})
            return
        item_hash = sanitize_hash(urllib.parse.unquote(m.group(1)))
        try:
            body = self._read_body()
            payload = json.loads(body.decode("utf-8"))
            updated = self.db.update_notes(item_hash, payload.get("notes", ""))
            if not updated:
                self._send_json(404, {"ok": False, "error": f"Test {item_hash} not found"})
                return
            self._send_json(200, {"ok": True, "test": updated})
        except Exception as exc:
            self._send_json(400, {"ok": False, "error": str(exc)})

    do_PUT = do_PATCH

    def do_DELETE(self):
        parsed = urllib.parse.urlparse(self.path)
        m = re.match(r"^/api/tests/([^/]+)$", parsed.path)
        if not m:
            self._send_json(404, {"ok": False, "error": "Unknown DELETE endpoint"})
            return
        item_hash = sanitize_hash(urllib.parse.unquote(m.group(1)))
        if self.db.delete_test(item_hash):
            self._send_json(200, {"ok": True, "deleted": item_hash})
        else:
            self._send_json(404, {"ok": False, "error": f"Test {item_hash} not found"})


def get_lan_ips() -> list:
    ips = []
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(0.2)
        sock.connect(("8.8.8.8", 80))
        ip = sock.getsockname()[0]
        sock.close()
        if ip and not ip.startswith("127."):
            ips.append(ip)
    except Exception:
        pass
    return ips


def create_server(host: str = "0.0.0.0", port: int = 8080, db_path: str = DEFAULT_DB_PATH):
    db = TransistorDB(db_path)

    class BoundHandler(GeLabHandler):
        pass

    BoundHandler.db = db
    ThreadingHTTPServer.allow_reuse_address = True
    return ThreadingHTTPServer((host, port), BoundHandler)


def main():
    parser = argparse.ArgumentParser(description="KAOTI Ge-LAB PRO (Rev 4.0) Monitoring Server")
    parser.add_argument("--host", default="0.0.0.0", help="Bind address (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="HTTP port (default: 8080)")
    parser.add_argument("--db", default=DEFAULT_DB_PATH, help="SQLite database path")
    args = parser.parse_args()

    httpd = create_server(args.host, args.port, args.db)
    lan_ips = get_lan_ips()

    print("=" * 64)
    print("  KAOTI Ge-LAB PRO (Rev 4.0) — Parametric Lab Server Ready")
    print("=" * 64)
    print(f"  Database : {os.path.abspath(args.db)}")
    print(f"  Photos   : {PHOTOS_DIR}")
    print(f"  Local    : http://127.0.0.1:{args.port}")
    for ip in lan_ips:
        print(f"  LAN/Phone: http://{ip}:{args.port}")
    print("  Press Ctrl+C to shut down.")
    print("=" * 64)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down KAOTI Ge-LAB PRO server...")
    finally:
        httpd.server_close()


if __name__ == "__main__":
    main()
