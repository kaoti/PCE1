# Phase 3 — Partitioned Star-Ground (`R_STAR`), Dual-Layer Routing, Kelvin Guard Rings & RF Antenna Keep-Out Specification (`p3_star_ground_routing.md`)

**Document ID:** `p3_star_ground_routing.md`  
**Subsystem:** 2-Layer PCB Stackup, Split `AGND` / `DGND` Ground Pours ($\ge 2.0\text{ mm}$ Moat), Single-Point Star Bridge `R_STAR` (`0R_STAR_BRIDGE`), Kelvin Guard Rings & Dual RF Antenna Keep-Out Zones  
**Architecture Revision:** Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine

---

## 1. Two-Layer PCB Stackup & Copper Layer Assignment

| Layer ID | KiCad Layer Name | Material / Thickness | Primary Routing & Electromagnetic Role |
| :---: | :--- | :--- | :--- |
| **Top Silk** | `F.SilkS` | White Epoxy Ink | **100% Component Reference Designators AND Component Values** + Front-Panel UI Callouts |
| **Top Mask** | `F.Mask` | Green / Matte-Black LPI ($20\text{ }\mu\text{m}$) | Solder mask with $0.05\text{ mm}$ pad clearance; continuous cover over Kelvin guard tracks |
| **Layer 1 (Top Copper)** | **`F.Cu`** | $1\text{ oz}$ ($35\text{ }\mu\text{m}$) Electrodeposited Cu | Horizontal-biased Manhattan signal routing, `PWR_HIGH_CURRENT` trunks, and Kelvin sense paths |
| **Dielectric Core** | `FR-4 Core` | $1.53\text{ mm}$ High-$T_g$ FR-4 ($\varepsilon_r \approx 4.5$) | High surface insulation resistance ($>10^{13}\,\Omega/\text{sq}$) |
| **Layer 2 (Bottom Copper)** | **`B.Cu`** | $1\text{ oz}$ ($35\text{ }\mu\text{m}$) Electrodeposited Cu | **Split `DGND` and `AGND` Solid Reference Planes** ($\ge 2.40\text{ mm}$ galvanic moat) + vertical orthogonal routing |
| **Bottom Mask / Silk** | `B.Mask` / `B.SilkS` | LPI Mask + Ink | Tenting over all 0.70/0.35 mm vias (`tenting front back`) |

---

## 2. Partitioned `DGND` vs. `AGND` Copper Planes & Single-Point Star Bridge (`R_STAR`)

To guarantee that high-frequency switching currents from `MOD_CHG` (`BQ25895` $1.5\text{ MHz}$ buck converter at up to $3\text{ A}$), $320\text{ mA}$ Wi-Fi/BLE packet bursts on `U_MCU2` (`ESP32-C3`), $40\text{ MHz}$ SPI display transfers to `MOD_TFT1`/`MOD_TFT2`, and $35\text{ mA}$ buzzer transients on `BZ1` never flow across the precision Germanium metrology ground reference, the `B.Cu` ground plane is physically divided into two non-overlapping polygons separated by a **$2.40\text{ mm}$ copper-free moat** along $X_{\text{moat}} = 152.40\text{ mm}$:

```
+-------------------------------------------------------+       +-------------------------------------------------------+
|              LEFT / CENTER COPPER POUR                | 2.4mm |                  RIGHT COPPER POUR                    |
|                Net: "DGND" (Layer B.Cu)               |  MOAT |                 Net: "AGND" (Layer B.Cu)              |
|            X = 1.50 mm .. 151.20 mm                   |       |             X = 153.60 mm .. 258.50 mm                |
|                                                       |152.4mm|                                                       |
|  • J_USB_C, F_USB, D_TVS_USB, J_BAT-, MOD_CHG (GND)   |       |  • MOD_BOOST, C_PI1/2, U_REG12, D_OVP, U_REG9         |
|  • U_LDO_DIG, U_MCU2 (ESP32-C3), U11 (MCP23017)       |       |  • C_A1/2, U_LDO_ANA, C_ADC_FA, C9, U_MCU1 (LOLIN32)  |
|  • MOD_TFT1, MOD_TFT2, Q_BLK, SW_UP..RIGHT, SW_A/B    |       |  • U9 (MCP4822), U5 (TLC2272), MOD_ADS (ADS1115)      |
|  • ENC1, SW_STBY, D_LED1..6, BZ1, Q_BZ, J_CYD_UART    |       |  • U4 (LMC6484IN), U10 (DG409DJ), U6/U7 (74HC595N)    |
|  • Q_ANA_N (Source), U_ISO1 Pin 5 (Detector GND)      |       |  • U8 (DG412LDJ), U1..U3 (DG408DJ), R.G. Keen Banks   |
|                                                       |       |  • U_ISO2 Pin 5 (Detector GND), J_KELVIN/J_TERM5 COM  |
|                                       [Pin 1: 148.59] o=======o [Pin 2: 156.21]                                       |
|                                                    R_STAR (0R_STAR_BRIDGE)                                            |
|                                                At (148.59, 77.47 mm) Boundary                                         |
+-------------------------------------------------------+       +-------------------------------------------------------+
```

### Exact `B.Cu` Zone Polygon Coordinates & Star-Bridge Specification:
1. **`DGND` Copper Pour Polygon (`B.Cu`, Net `DGND`):**
   - Vertices (chamfered at $10.50\text{ mm}$ to respect the $R = 10.0\text{ mm}$ left corners):
     `(10.50, 1.50)` $\to$ `(151.20, 1.50)` $\to$ `(151.20, 153.50)` $\to$ `(10.50, 153.50)` $\to$ `(1.50, 144.50)` $\to$ `(1.50, 10.50)`.
2. **`AGND` Copper Pour Polygon (`B.Cu`, Net `AGND`):**
   - Vertices (chamfered at $10.50\text{ mm}$ to respect the $R = 10.0\text{ mm}$ right corners):
     `(153.60, 1.50)` $\to$ `(249.50, 1.50)` $\to$ `(258.50, 10.50)` $\to$ `(258.50, 144.50)` $\to$ `(249.50, 153.50)` $\to$ `(153.60, 153.50)`.
3. **Moat Clearance Verification:**
   $$\Delta X_{\text{moat}} = 153.60\text{ mm} - 151.20\text{ mm} = \mathbf{2.40\text{ mm}} \ge 2.00\text{ mm} \text{ mandatory minimum}$$
4. **Single-Point Star Bridge (`R_STAR` — `Star_Bridge_0R`, `0R_STAR_BRIDGE`):**
   - Placed at **`(148.59, 77.47 mm)`** directly across the $X = 152.40\text{ mm}$ moat:
     - **Pin 1 (`148.59, 77.47 mm`):** Anchored directly into the `DGND` plane ($2.61\text{ mm}$ inside `DGND` edge).
     - **Pin 2 (`156.21, 77.47 mm`):** Anchored directly into the `AGND` plane ($2.61\text{ mm}$ inside `AGND` edge) adjacent to `U_LDO_ANA` and `MOD_ADS`.
5. **Optocoupler Galvanic Straddling (`U_ISO1`, `U_ISO2` — `2x H11L1M` DIP-6):**
   - **`U_ISO1` (M1 $\to$ M2, placed at `156.21, 91.44 mm`, rotated $180^\circ$):** Input LED Pins 1–3 sit at $X = 156.21\text{ mm}$ inside the `AGND` zone (`+3V3_ANA` / `LINK_TX_M1`), while Detector Pins 4–6 sit at $X = 148.59\text{ mm}$ inside the `DGND` zone (`+3V3_DIG` / `DGND` / `LINK_RX_M2`).
   - **`U_ISO2` (M2 $\to$ M1, placed at `148.59, 100.33 mm`, rotated $0^\circ$):** Input LED Pins 1–3 sit at $X = 148.59\text{ mm}$ inside the `DGND` zone (`+3V3_DIG` / `LINK_TX_M2`), while Detector Pins 4–6 sit at $X = 156.21\text{ mm}$ inside the `AGND` zone (`+3V3_ANA` / `AGND` / `LINK_RX_M1`).
   - **Result:** Zero signal traces cross the $X = 152.40\text{ mm}$ moat except the high-side power switch feed (`V_BUS_SYS` $\to$ `Q_ANA_SW` and `Q_ANA_SW_G`) and `R_STAR` itself!

---

## 3. Kelvin Sense Routing & Guard-Ring Rules (`TP1_DUT..TP3_DUT`)

1. **True 4-Terminal Kelvin Separation at `R_PR1..3` (`100 Ω 0.5W 0.1%`):**
   - High/medium force currents from `U1..U3` (`DG408DJ` Pin 8) and `U8` (`DG412LDJ` Pins 2, 15, 10) converge on `TP{ch}_MUX` (`R_PR{ch}` Pin 2).
   - The sense tap to `R_ESD1..3` ($10\text{ k}\Omega \to$ `U4 LMC6484IN` $20\text{ fA}$ CMOS inputs Pins 3, 5, 10) branches exclusively from the **DUT side (`R_PR{ch}` Pin 1 = `TP{ch}_DUT`)** at the DUT socket bay (`J_ZIF`, `J_DUT3`, `J_KELVIN`, `J_TERM5`).
2. **`AGND` Guard Pour Around High-Z Nodes:**
   - The `B.Cu` `AGND` copper pour provides a continuous equipotential ground shield directly beneath `TP1_DUT..TP3_DUT`, `ESD_TP1..3`, `C{1..3}_S3` ($100\text{ k}\Omega$), and `C{1..3}_S4` ($2.20\text{ M}\Omega$), intercepting any capacitive or surface-leakage coupling from adjacent switching lines.

---

## 4. Dual RF Antenna Copper & Ground-Pour Keep-Out Zones (`U_MCU1` & `U_MCU2`)

Both socketed microcontroller modules carry onboard $2.4\text{ GHz}$ antennas (meander PCB trace antenna on `U_MCU1` `WEMOS LOLIN32 Lite`; 3D red ceramic chip antenna on `U_MCU2` `ESP32-C3 SuperMini`). Even though `U_MCU1` operates with Wi-Fi/BT disabled during analog scans, keeping copper pours and signal traces out from under **both** antenna regions prevents capacitive detuning on `U_MCU2` and prevents high-frequency chassis-eddy coupling into `U_MCU1`:

| Target Module | Module Origin $(X, Y, \text{Rot})$ | Antenna Physical Location | Exact KiCad Keep-Out Zone Polygon (`F.Cu` & `B.Cu`) | Keep-Out Constraints Enforced |
| :--- | :---: | :--- | :---: | :--- |
| **`U_MCU1` (`WEMOS LOLIN32 Lite`)** | `(160.02, 104.14, 0°)` | Top edge above Pins 1 & 14 (`Y = 95.5..102.5 mm`) | **`X = 158.50 .. 184.00 mm`**<br/>**`Y = 95.50 .. 102.50 mm`**<br/>($25.5\text{ mm} \times 7.0\text{ mm}$) | `tracks not_allowed`<br/>`vias not_allowed`<br/>`copperpour not_allowed` |
| **`U_MCU2` (`ESP32-C3 SuperMini`)** | `(116.84, 19.05, 0°)` | Bottom edge below Pins 8 & 16 (`Y = 38.0..41.5 mm`) | **`X = 115.50 .. 133.50 mm`**<br/>**`Y = 38.00 .. 41.50 mm`**<br/>($18.0\text{ mm} \times 3.5\text{ mm}$) | `tracks not_allowed`<br/>`vias not_allowed`<br/>`copperpour not_allowed` |

Both keep-out zones are simultaneously enforced in the Python Manhattan A* autorouter (`r_try.mark_keepout_rect(...)`) and emitted as native KiCad 9 `(zone ... (keepout (tracks not_allowed) (vias not_allowed) (pads allowed) (copperpour not_allowed)))` objects on `F.Cu` and `B.Cu`.
