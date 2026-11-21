# Phase 3 — Mandatory Dual-Property `F.SilkS` Silkscreen & Front-Panel Instrument Labeling Rules (`p3_silkscreen_rules.md`)

**Document ID:** `p3_silkscreen_rules.md`  
**Subsystem:** Top Silkscreen (`F.SilkS`) Dual-Label Mandate (Reference Designator + Component Value on `F.SilkS` for 100% of Components), Zero-Overlap Placement Offsets, and Front-Panel Console Typography  
**Architecture Revision:** Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine

---

## 1. Mandatory Dual-Label Silkscreen Rule (`Reference` + `Value` on `F.SilkS`)

To allow complete hand-assembly, inspection, and bench troubleshooting of all **242 through-hole and socketed components** without consulting an external placement chart or CAD viewer, the Rev 5.0 footprint library (`scripts/build_libraries.py`) and PCB generator (`scripts/build_design.py`) enforce an uncompromising silkscreen rule:

> [!IMPORTANT]
> **MANDATORY SILKSCREEN INVARIANT (`0` `F.Fab`-ONLY VALUES):**
> 1. **Every single footprint in `Ge_Engine.pretty` places BOTH `(fp_text reference ...)` AND `(fp_text value ...)` explicitly on `(layer "F.SilkS")`.**
> 2. In `scripts/build_libraries.py`, `fp_wrap(...)` defaults to `val_layer="F.SilkS"`, `ref_size=0.80`, `val_size=0.80`, `thickness=0.15`, and `MountingHole_M3.kicad_mod` also places both `REF**` and `M3` on `F.SilkS`.
> 3. Shell verification across all `.kicad_mod` files in `/Users/kaoti/Desktop/PCE1/Ge_Engine.pretty/` confirms **`0` occurrences of `(layer "F.Fab")`**!
> 4. In `scripts/build_design.py`, `embed_footprint(...)` substitutes each component's exact schematic value (e.g., `2.49k_0.1%`, `100R_0.5W_0.1%`, `100nF_C0G`, `1N3595`, `NDP6020P`, `MCP1826S-3302E/AB`, `H11L1M`, `0R_STAR_BRIDGE`) directly into `(fp_text value "<VAL>" ... (layer "F.SilkS"))`.

---

## 2. Footprint-by-Footprint `F.SilkS` Offset & Pad-Clearance Table

Every footprint positions its Reference Designator along the **Top (`-Y`) exterior boundary** and its Component Value along the **Bottom (`+Y`) exterior boundary**, strictly outside all solder mask openings (`*.Mask`) and courtyard outlines:

| Footprint Name (`Ge_Engine.pretty`) | Pad Span $(X \times Y)$ (mm) | Pad Size / Drill (mm) | `Reference` `(rx, ry)` Offset on `F.SilkS` (mm) | `Value` `(vx, vy)` Offset on `F.SilkS` (mm) | Font Size / Stroke (mm) | Min Clearance to Exposed Pad Copper |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **`R_Axial_P7.62mm` / `Star_Bridge_0R`** | `7.62 × 0.00` | `1.60 / 0.80` | `(3.81, -1.95)` | `(3.81, +1.95)` | `0.80 × 0.80 / 0.15` | `> 1.15 mm` (clear above/below body box) |
| **`C_Disc_P5.00mm` / `Fuse_PTC_P5.00mm`** | `5.00 × 0.00` | `1.60 / 0.80` | `(2.50, -2.05)` | `(2.50, +2.05)` | `0.80 × 0.80 / 0.15` | `> 1.25 mm` |
| **`CP_Radial_D8_P3.5mm`** | `3.50 × 0.00` | `1.80 / 0.90` | `(1.75, -5.05)` | `(1.75, +5.05)` | `0.80 × 0.80 / 0.15` | `> 0.85 mm` outside $\varnothing 8.4\text{ mm}$ circle |
| **`L_Radial_P5.00mm`** | `5.00 × 0.00` | `1.80 / 1.00` | `(2.50, -4.90)` | `(2.50, +4.90)` | `0.80 × 0.80 / 0.15` | `> 0.70 mm` outside $\varnothing 8.0\text{ mm}$ circle |
| **`D_DO-35_P7.62mm`** | `7.62 × 0.00` | `1.60 / 0.80` | `(3.81, -1.95)` | `(3.81, +1.95)` | `0.80 × 0.80 / 0.15` | `> 1.15 mm` + Cathode bar at `x=2.20` |
| **`D_DO-41_P10.16mm` / `D_DO-15_P10.16mm`** | `10.16 × 0.00` | `1.80 / 0.95` | `(5.08, -2.40)` | `(5.08, +2.40)` | `0.80 × 0.80 / 0.15` | `> 1.50 mm` + Cathode bar at `x=2.80` |
| **`LED_D3mm_Green/Blue/Red/Yellow/Amber`** | `2.54 × 0.00` | `1.70 / 0.90` | `(1.27, -2.80)` | `(1.27, +2.80)` | `0.80 × 0.80 / 0.15` | `> 1.15 mm` + `"K"` marker at `x=-2.05` |
| **`TO-220-3_Vertical`** | `5.08 × 0.00` | `1.80 / 1.05` | `(2.54, -4.30)` | `(2.54, +2.50)` | `0.80 × 0.80 / 0.15` | `> 1.10 mm` outside tab & pins |
| **`Socket_TO92_3Pin` / `Socket_1x03_DUT`** | `5.08 × 0.00` | `1.70 / 1.00` | `(2.54, -3.50)` | `(2.54, +3.80)` | `0.80 × 0.80 / 0.15` | `> 1.05 mm` + `"FLAT"` orientation label |
| **`Socket_DIP-6/8/14/16/28_W7.62mm`** | `7.62 × ymax` | `1.65 / 0.85` | `(3.81, -2.60)` | `(3.81, ymax + 2.60)` | `0.80 × 0.80 / 0.15` | `> 1.35 mm` beyond Pin 1 notch & bottom |
| **`Socket_Wemos_Lolin32_Lite`** | `22.86 × 30.48` | `1.70 / 1.00` | `(11.43, -10.20)` | `(11.43, +43.00)` | `0.80 × 0.80 / 0.15` | `> 1.50 mm` outside module outline |
| **`Socket_ESP32_C3_SuperMini`** | `15.24 × 17.78` | `1.70 / 1.00` | `(7.62, -4.90)` | `(7.62, +22.00)` | `0.80 × 0.80 / 0.15` | `> 1.20 mm` outside `USB-C` / `C3 ANT` |
| **`Socket_BQ25895_Charger`** | `15.24 × 12.70` | `1.85 / 1.05` | `(7.62, -5.80)` | `(7.62, +20.80)` | `0.80 × 0.80 / 0.15` | `> 1.10 mm` outside `20 × 24 mm` outline |
| **`Socket_ADS1115_Module`** | `22.86 × 0.00` | `1.70 / 1.00` | `(11.43, -16.80)` | `(11.43, +3.80)` | `0.80 × 0.80 / 0.15` | `> 1.60 mm` outside breakout outline |
| **`Socket_MT3608_Boost`** | `33.02 × 12.70` | `2.00 / 1.10` | `(16.51, -3.60)` | `(16.51, +16.50)` | `0.80 × 0.80 / 0.15` | `> 1.40 mm` + `IN+/IN-/OUT+/OUT-` markers |
| **`Socket_TFT_1_9_Curve / DMM`** | `0.00 × 17.78` | `1.70 / 1.00` | `(29.00, -7.20)` | `(29.00, +25.00)` | `0.80 × 0.80 / 0.15` | `> 1.50 mm` outside `62 × 29 mm` bezel |
| **`SW_Tact_12mm_Yellow/Red/Blue`** | `12.50 × 5.00` | `2.10 / 1.10` | `(6.25, -4.80)` | `(6.25, +9.80)` | `0.80 × 0.80 / 0.15` | `> 1.20 mm` outside `12 × 12 mm` switch |
| **`EC11_Knob_Encoder`** | `14.50 × 5.00` | `1.80 / 1.00` | `(7.50, -11.80)` | `(7.50, +16.80)` | `0.80 × 0.80 / 0.15` | `> 1.50 mm` outside $\varnothing 25\text{ mm}$ knob dial |
| **`SW_Slide_DPDT_THT` / `SW_Slide_SPDT_THT`** | `5.08 × 2.54` / `0` | `1.80 / 1.00` | `(2.54, -3.55)` | `(2.54, +6.10)` / `+3.80` | `0.80 × 0.80 / 0.15` | `> 1.05 mm` outside metal shell |
| **`Socket_ZIF_14Pin`** | `7.62 × 15.24` | `1.80 / 1.00` | `(3.81, -10.50)` | `(3.81, +25.00)` | `0.80 × 0.80 / 0.15` | `> 1.50 mm` + Pin `1/1/1/2/3/3/3` callouts |
| **`TerminalBlock_5Pin_508`** | `20.32 × 0.00` | `2.60 / 1.30` | `(10.16, -5.60)` | `(10.16, +6.80)` | `0.80 × 0.80 / 0.15` | `> 1.40 mm` + `1 2 3 COM 30V` legend |

---

## 3. Front-Panel Functional Callouts, Polarity Marks & Galvanic Boundary Legend

In addition to per-component `Reference` and `Value` silkscreens, the board includes high-contrast functional legends on `F.SilkS`:
1. **Instrument Header Banner:** `"KAOTI AUDIO — REV 5.0 DUAL-MCU GERMANIUM PARAMETRIC ENGINE"` centered above the 6-LED status bar at `(78.74, 3.20 mm)`.
2. **Analog Island Banner:** `"QUIET ANALOG CORE (AGND ISLAND + 2x DAMPED LC PI-FILTERS)"` at `(204.00, 3.20 mm)`.
3. **Galvanic Moat Dashed Divider & Star-Ground Callout:** A $0.25\text{ mm}$ dashed vertical divider line runs along $X = 152.40\text{ mm}$ from $Y = 6.00\text{ mm}$ to $149.00\text{ mm}$, labeled `"GALVANIC MOAT (>=2.0mm) | R_STAR SINGLE-POINT GROUND"` at `(152.40, 151.80 mm)`.
4. **DUT Bay Terminal Callouts:** Pin numbers and terminal assignments (`1/1/1/2/3/3/3` on `J_ZIF`, `1 2 3` on `J_DUT3`, `1 2 3 COM 30V` on `J_TERM5`, `+` on all electrolytic caps and `BZ1`, `K` and cathode band on all diodes/LEDs, and `FLAT` on all TO-92 sockets) are permanently printed on `F.SilkS`.
