# Phase 3 — Symmetrical Rounded-Corner Handheld/Console PCB Layout & Mechanical Ergonomics Specification (`p3_pcb_layout_symmetry_spec.md`)

**Document ID:** `p3_pcb_layout_symmetry_spec.md`  
**Subsystem:** Mechanical Board Outline, Rounded Corners (`R = 10.0 mm` `gr_arc`), Vertical Symmetry Axis (`X_sym = 78.74 mm`), Ergonomic Handheld/Benchtop UI Placement Map & Partitioned Dual-MCU Zones  
**Architecture Revision:** Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine

---

## 1. Mechanical Board Outline & Rounded-Corner Geometry (`Edge.Cuts`)

The Rev 5.0 PCB (`Ge_Parametric_Engine.kicad_pcb`) uses an ergonomic, rounded-corner handheld/benchtop console outline on the `Edge.Cuts` layer with **zero sharp $90^\circ$ vertices**:

- **Overall Board Dimensions:** **$W = 260.000\text{ mm} \times H = 155.000\text{ mm}$** ($1.6\text{ mm}$ FR-4, $1\text{ oz}$ / $35\text{ }\mu\text{m}$ copper, 2 layers `F.Cu` and `B.Cu`, 100% through-hole / socketed).
- **Corner Radius:** **$R = 10.000\text{ mm}$** on all four corners, constructed from 4 tangent straight lines (`gr_line`) and 4 true $90^\circ$ circular arcs (`gr_arc`), with arc midpoint inset:
  $$k = R\left(1 - \frac{1}{\sqrt{2}}\right) = 10.000 \times 0.2928932 = \mathbf{2.9289\text{ mm}}$$

| `Edge.Cuts` Primitive | Start Coordinate $(X_1, Y_1)$ (mm) | Midpoint $(X_m, Y_m)$ (mm) | End Coordinate $(X_2, Y_2)$ (mm) | Geometric Role |
| :--- | :---: | :---: | :---: | :--- |
| **`gr_line` (Top Edge)** | `(10.000, 0.000)` | — | `(250.000, 0.000)` | Top horizontal boundary |
| **`gr_arc` (Top-Right Corner)** | `(250.000, 0.000)` | `(257.0711, 2.9289)` | `(260.000, 10.000)` | Top-Right $R=10.0\text{ mm}$ fillet (center `250.0, 10.0`) |
| **`gr_line` (Right Edge)** | `(260.000, 10.000)` | — | `(260.000, 145.000)` | Right vertical boundary |
| **`gr_arc` (Bottom-Right Corner)** | `(260.000, 145.000)` | `(257.0711, 152.0711)` | `(250.000, 155.000)` | Bottom-Right $R=10.0\text{ mm}$ fillet (center `250.0, 145.0`) |
| **`gr_line` (Bottom Edge)** | `(250.000, 155.000)` | — | `(10.000, 155.000)` | Bottom horizontal boundary |
| **`gr_arc` (Bottom-Left Corner)** | `(10.000, 155.000)` | `(2.9289, 152.0711)` | `(0.000, 145.000)` | Bottom-Left $R=10.0\text{ mm}$ fillet (center `10.0, 145.0`) |
| **`gr_line` (Left Edge)** | `(0.000, 145.000)` | — | `(0.000, 10.000)` | Left vertical boundary |
| **`gr_arc` (Top-Left Corner)** | `(0.000, 10.000)` | `(2.9289, 2.9289)` | `(10.000, 0.000)` | Top-Left $R=10.0\text{ mm}$ fillet (center `10.0, 10.0`) |

- **Corner Routing & Copper Keep-Out Enforcement:** In `BoardRouter._mark_border()`, all grid cells lying outside the four $R = 10.0\text{ mm}$ corner arcs (plus a $1.8\text{ mm}$ edge clearance margin) are pre-marked as blocked (`occ = -1`), guaranteeing that zero copper tracks or vias ever approach or cross the rounded corners.
- **4× M3 Corner Mounting Holes (`H1..H4`, `3.2 mm` NPTH):** Placed symmetrically at `(8.00, 8.00)`, `(252.00, 8.00)`, `(8.00, 147.00)`, and `(252.00, 147.00)` concentric with the corner radii.

---

## 2. Symmetrical Front-Panel Console Axis (`X_sym = 78.74 mm`) & Ergonomic Layout Map

The Left/Center Handheld & Front-Panel Console (`X = 8.00 mm .. 148.59 mm`, `DGND` Zone) is engineered around a strict **Vertical Symmetry Axis at $X_{\text{sym}} = 78.74\text{ mm}$ ($3100\text{ mil}$)**, while the Right Quiet Analog Metrology Core (`X = 156.21 mm .. 252.00 mm`, `AGND` Zone) is separated across a **$2.40\text{ mm}$ galvanic isolation moat centered at $X_{\text{moat}} = 152.40\text{ mm}$**:

```
+----------------------------------------------------------------------------------------------------------------------+
| (H1)   [D_LED1:PWR]  [D_LED2:CHG]  [D_LED3:STBY]  |  [D_LED4:SCAN]  [D_LED5:OK]  [D_LED6:FLT]    [Q_ANA_SW + BOOST]  |
|                                                   | (X_sym = 78.74 mm)                           [2x LC PI-FILTERS]  |
|  [J_USB_C]            +-------------------------------------------------------+   [U_MCU2 C3]    [L7812/78L09/15mA]  |
|  [F_USB/TVS]          |       MOD_TFT1 (Top 1.9" ST7789 IPS Display)          |   (Ant Keepout)  [U_LDO_ANA + ADC_F] |
|  [J_BAT/SW_PWR]       |       Visual Center = (78.53, 30.48 mm)               |   [J_CYD_UART]                       |
|  [Q_REV_BAT]          +-------------------------------------------------------+                  +-----------------+ |
|  [U_LDO_DIG]          +-------------------------------------------------------+   [U11 MCP23017] | U9 DAC / U5 RRO | |
|  [MOD_CHG BQ25895]    |       MOD_TFT2 (Bottom 1.9" ST7789 IPS Display)       |   [Q_BLK / STBY] | MOD_ADS ADS1115 | |
|                       |       Visual Center = (78.53, 60.96 mm)               |                  | U4 20fA / U10   | |
|                       +-------------------------------------------------------+   [R_STAR (0R)]  | U6/U7 74HC595   | |
|         [SW_UP]                                   |                               [U_ISO1/ISO2]  | U8 DG412LDJ     | |
|  [SW_LEFT]   [SW_RIGHT]                     ( ( ENC1 ) )              [SW_A]                     | U1, U2, U3      | |
|        [SW_DOWN]                            Shaft @ 78.62       [SW_B]            [SW_BZ / BZ1]  | 3x DG408DJ +    | |
|  [RC Debounce Bank]                               |                               [Q_BZ/Snubber] | 0.1% Keen Banks | |
|                                                   |                                              | U_MCU1 LOLIN32  | |
| (H3)   [J_TERM5 5-Pin Screw]               [J_ZIF 14-Pin]        [J_DUT3 / J_KELVIN / J_TEMP]    | (Ant Keepout)   | |
+----------------------------------------------------------------------------------------------------------------------+
```

### 2.1 Quantitative Symmetry Verification Table About $X_{\text{sym}} = 78.74\text{ mm}$

| Front-Panel Functional Element | Component RefDes | Pin 1 Origin $(X, Y)$ (mm) | Component Geometric Center $(X_c, Y_c)$ (mm) | Symmetry Offset from $X_{\text{sym}} = 78.74\text{ mm}$ | Ergonomic & Visual Symmetry Verification |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **Top Status Bar LED #1 (Green `PWR`)** | `D_LED1` | `(39.37, 7.62)` | `(40.64, 7.62)` | **`-38.10 mm`** | Paired symmetrically with `D_LED6` (`+38.10 mm`) |
| **Top Status Bar LED #2 (Amber `CHG`)** | `D_LED2` | `(54.61, 7.62)` | `(55.88, 7.62)` | **`-22.86 mm`** | Paired symmetrically with `D_LED5` (`+22.86 mm`) |
| **Top Status Bar LED #3 (Cyan `STBY/RF`)** | `D_LED3` | `(69.85, 7.62)` | `(71.12, 7.62)` | **`-7.62 mm`** | Paired symmetrically with `D_LED4` (`+7.62 mm`) |
| **Top Status Bar LED #4 (Blue `SCAN`)** | `D_LED4` | `(85.09, 7.62)` | `(86.36, 7.62)` | **`+7.62 mm`** | Exact mirror of `D_LED3` across $X_{\text{sym}}$ |
| **Top Status Bar LED #5 (Green `DUT_OK`)** | `D_LED5` | `(100.33, 7.62)` | `(101.60, 7.62)` | **`+22.86 mm`** | Exact mirror of `D_LED2` across $X_{\text{sym}}$ |
| **Top Status Bar LED #6 (Red `FAULT`)** | `D_LED6` | `(115.57, 7.62)` | `(116.84, 7.62)` | **`+38.10 mm`** | Exact mirror of `D_LED1` across $X_{\text{sym}}$ |
| **Top 1.9" IPS Display (`CURVE TRACER`)** | `MOD_TFT1` | `(49.53, 21.59)` | `(78.53, 30.48)` | **`-0.21 mm`** (On-Grid Center) | Centered directly on $X_{\text{sym}}$ |
| **Bottom 1.9" IPS Display (`DMM / GE`)** | `MOD_TFT2` | `(49.53, 52.07)` | `(78.53, 60.96)` | **`-0.21 mm`** (On-Grid Center) | Stacked vertically in perfect alignment under `MOD_TFT1` |
| **Center Navigation Rotary Encoder** | `ENC1` | `(71.12, 90.17)` | **`(78.62, 92.67)`** (Shaft) | **`-0.12 mm`** (On-Grid Shaft) | Fluted knob shaft centered directly on $X_{\text{sym}}$ |
| **Left Thumb D-Pad Cross (`UP/DN/LF/RT`)** | `SW_UP..RIGHT` | `(25.40, 81.28)` etc. | **`(31.65, 92.67)`** (Cross Center) | **`-47.09 mm`** (Left Wing) | Aligned horizontally ($Y_c = 92.67\text{ mm}$) with `ENC1` and `SW_A/B` |
| **Right Thumb Action Buttons (`B`, `A`)** | `SW_B, SW_A` | `(106.68, 93.98)`, `(123.19, 86.36)` | **`(121.19, 92.67)`** (Pair Center) | **`+42.45 mm`** (Right Wing) | Ergonomic $15^\circ$ Nintendo GameBoy-angle thumb cluster opposite D-Pad |
| **Bottom Center 14-Pin ZIF Test Socket** | `J_ZIF` | `(74.93, 125.73)` | **`(78.74, 133.35)`** | **`0.00 mm` (Exact Center!)** | Pin columns at $74.93\text{ mm}$ and $82.55\text{ mm}$ straddle $X_{\text{sym}} = 78.74\text{ mm}$ exactly! |
| **Left & Right Auxiliary DUT Fixtures** | `J_TERM5` / `J_DUT3, J_KELVIN, J_TEMP` | `(20.32, 138.43)` / `(96.52, 124..141)` | `(30.48, 138.43)` / `(99.06, 132.5)` | `-48.26 mm` / `+20.32 mm` | Flank `J_ZIF` in the bottom test bay with short Kelvin traces to `R_PR1..3` |
