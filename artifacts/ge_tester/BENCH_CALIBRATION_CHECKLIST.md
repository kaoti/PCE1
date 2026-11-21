# BENCH CALIBRATION & HARDWARE BRING-UP CHECKLIST
## Handheld Germanium Parametric Characterization Engine (Rev 5.0 Dual-MCU)

---

## 1. Safety Rules & Initial Environmental Setup
> [!CAUTION]
> **DO NOT INSERT ACTIVE MCUS OR SOCKETED ICS UNTIL STEP 2.4 IS COMPLETE!**
> Powering an uncalibrated boost converter (`MT3608`) into un-trimmed analog ICs can destroy the `LMC6484IN` ($V_{\text{max}} = 16\text{ V}$) and `ADS1115` ($V_{\text{max}} = 5.5\text{ V}$).

1. **Bench Environment:**
   - Ambient Temperature: $20^\circ\text{C}$ to $25^\circ\text{C}$ (ambient tracked via calibrated on-board `J_TEMP` `DS18B20`).
   - ESD Mat grounded to mains earth.
   - 6.5-digit Bench DMM (Keysight 34465A, Keithley 2000, or Fluke 87V) calibrated within 12 months.
   - Current-limited DC Bench Power Supply ($5.00\text{ V}$, $1.00\text{ A}$ limit) or certified USB-C PD/5V power adapter.
2. **Visual Inspection:**
   - Verify zero solder bridges between adjacent IC socket pins (`DG408DJ`, `DG412LDJ`, `74HC595N`, `MCP23017`).
   - Confirm polarities of electrolytic capacitors (`C_PI1A`, `C_PI2A`, `C1`, `C3`, `C5`, `C7`, `C17`).
   - Verify orientation of `D_OVP` (`1N4743A` cathode to `+12V_A`) and `D_TVS_USB` (`P6KE15A` cathode to `V_USB_PROT`).
   - Confirm `R_STAR` ($0\,\Omega$ bridge or solder jumper) is installed, bridging `AGND` and `DGND` at exactly one physical star point.

---

## 2. Power-Rail Bring-Up Sequence (No ICs Seated)

### 2.1 Battery & USB-C Power-Path Inspection
| Step | Action / Node | Multimeter Connection | Target Voltage | Pass/Fail Limit | Status |
| :---: | :--- | :--- | :---: | :---: | :---: |
| **2.1.1** | `J_USB_C` Input Voltage | `TP1 (V_USB_PROT)` to `DGND` | $5.000\text{ V}$ | $4.750\text{ V}$ to $5.250\text{ V}$ | [ ] |
| **2.1.2** | USB-C CC Pull-Down Resistors | Measure across `R_CC1` and `R_CC2` | $5.10\text{ k}\Omega$ | $5.05\text{ k}\Omega$ to $5.15\text{ k}\Omega$ | [ ] |
| **2.1.3** | `J_BAT` Hard-Off Isolation | Set `SW_PWR = OFF`; measure `TP_BAT` | $0.000\text{ V}$ / $0.0\,\mu\text{A}$ | $<0.05\,\mu\text{A}$ leakage | [ ] |
| **2.1.4** | `Q_REV_BAT` Reverse Battery Guard | Apply $-4.20\text{ V}$ to `J_BAT` (reverse); probe `V_BAT_PROT` | $0.000\text{ V}$ | $<1.0\,\mu\text{A}$ reverse drain | [ ] |
| **2.1.5** | `BQ25895` NVDC SYS Rail | Set `SW_PWR = ON`; probe `TP2 (V_BUS_SYS)` | $3.800\text{ V}$ to $4.200\text{ V}$ | $3.500\text{ V}$ to $4.350\text{ V}$ | [ ] |

### 2.2 Digital Rail & LDO Verification
| Step | Action / Node | Multimeter Connection | Target Voltage | Pass/Fail Limit | Status |
| :---: | :--- | :--- | :---: | :---: | :---: |
| **2.2.1** | Digital LDO `U_LDO_DIG` | `TP3 (+3V3_DIG)` to `DGND` | $3.300\text{ V}$ | $3.234\text{ V}$ to $3.366\text{ V}$ ($\pm 2\%$) | [ ] |
| **2.2.2** | Power LED `D_LED1` | Visual inspection | Green Illuminates | Stable, crisp brightness | [ ] |
| **2.2.3** | Standby Switch `SW_STBY` | Measure `U11` Pin 28 (`GPA7`) in Active vs Standby | Active: $3.30\text{ V}$, Stby: $0.00\text{ V}$ | Logic High / Low clean | [ ] |

### 2.3 Analog Core Switching & Trimming (`MT3608` Adjustment)
| Step | Action / Node | Multimeter Connection | Target Voltage | Pass/Fail Limit | Status |
| :---: | :--- | :--- | :---: | :---: | :---: |
| **2.3.1** | Analog Power Gate `Q_ANA_SW` | Probe `TP4 (V_ANA_IN)` with `ANA_PWR_EN = HIGH` | $3.800\text{ V}$ to $4.200\text{ V}$ | Drops to $0.000\text{ V}$ in Standby | [ ] |
| **2.3.2** | **CRITICAL: Trim `MOD_BOOST`** | **Probe `V_BOOST_RAW` to `AGND`; adjust blue multi-turn trimmer** | **Trim exactly to $14.500\text{ V}$** | **$14.450\text{ V}$ to $14.550\text{ V}$ (NEVER $>15.5\text{V}$!)** | [ ] |
| **2.3.3** | Pi-Filter #1 Output | Probe `TP5 (V_14V_FILT)` to `AGND` | $14.480\text{ V}$ | $14.40\text{ V}$ to $14.55\text{ V}$ | [ ] |
| **2.3.4** | +12V Linear Rail `U_REG12` | Probe `TP6 (+12V_A)` to `AGND` | $12.000\text{ V}$ | $11.750\text{ V}$ to $12.250\text{ V}$ | [ ] |
| **2.3.5** | +9V Reference Rail `U_REG9` | Probe `TP7 (+9V_REF)` to `AGND` | $9.000\text{ V}$ | $8.820\text{ V}$ to $9.180\text{ V}$ ($\pm 2\%$) | [ ] |
| **2.3.6** | +9V Guarded Rail `Q_LIM1/2` | Probe `TP8 (+9V_LIM)` to `AGND` | $8.950\text{ V}$ | $8.750\text{ V}$ to $9.150\text{ V}$ | [ ] |
| **2.3.7** | Analog 3.3V LDO `U_LDO_ANA` | Probe `TP9 (+3V3_ANA)` to `AGND` | $3.300\text{ V}$ | $3.234\text{ V}$ to $3.366\text{ V}$ | [ ] |
| **2.3.8** | Quiet ADC Rail `R_ADC_F` | Probe `TP10 (+3V3_ADC)` to `AGND` | $3.300\text{ V}$ | $<1.0\text{ mV}_{\text{rms}}$ noise | [ ] |

---

## 3. Passive Shunt & Resistor Network Precision Verification
*Measure with precision 4-wire Ohmmeter between test points before IC seating:*

| Resistor Designator | Nominal Value | Circuit Role | Allowed Tolerance | Measured Value | Pass/Fail |
| :--- | :---: | :--- | :---: | :---: | :---: |
| `R_PR1, R_PR2, R_PR3` | $100.00\,\Omega$ | Kelvin Series Protection (0.5W) | $0.10\%$ ($99.90\,\Omega$ to $100.10\,\Omega$) | | [ ] |
| `R_LA1, R_LA2, R_LA3` | $470.00\,\Omega$ | High-Current Rail A Shunt | $0.10\%$ ($469.53\,\Omega$ to $470.47\,\Omega$) | | [ ] |
| `R_LG1, R_LG2, R_LG3` | $470.00\,\Omega$ | High-Current Ground Shunt | $0.10\%$ ($469.53\,\Omega$ to $470.47\,\Omega$) | | [ ] |
| `R_KA1, R_KA2, R_KA3` | $2490.0\,\Omega$ | **Primary R.G. Keen Collector Shunt** | $0.10\%$ ($2487.5\,\Omega$ to $2492.5\,\Omega$) | | [ ] |
| `R_KG1, R_KG2, R_KG3` | $2490.0\,\Omega$ | Primary R.G. Keen Emitter Shunt | $0.10\%$ ($2487.5\,\Omega$ to $2492.5\,\Omega$) | | [ ] |
| `R_MB1, R_MB2, R_MB3` | $100.00\text{ k}\Omega$ | Micro-Leakage Base/Sense Shunt | $0.10\%$ ($99.90\text{ k}\Omega$ to $100.10\text{ k}\Omega$) | | [ ] |
| `R_KB1, R_KB2, R_KB3` | $2.2000\text{ M}\Omega$ | High-Z R.G. Keen Base Shunt | $0.10\%$ ($2.1978\text{ M}\Omega$ to $2.2022\text{ M}\Omega$) | | [ ] |
| `R_D1A..R_D4A` | $1500.0\,\Omega$ | 4:1 Sense Attenuator Top Arm | $0.10\%$ ($1498.5\,\Omega$ to $1501.5\,\Omega$) | | [ ] |
| `R_D1B..R_D4B` | $499.00\,\Omega$ | 4:1 Sense Attenuator Bottom Arm | $0.10\%$ ($498.50\,\Omega$ to $499.50\,\Omega$) | | [ ] |
| `R_HV1` | $100.00\text{ k}\Omega$ | 11:1 HV DMM Divider Top Arm | $0.10\%$ ($99.90\text{ k}\Omega$ to $100.10\text{ k}\Omega$) | | [ ] |
| `R_HV2` | $10.000\text{ k}\Omega$ | 11:1 HV DMM Divider Bottom Arm | $0.10\%$ ($9.990\text{ k}\Omega$ to $10.010\text{ k}\Omega$) | | [ ] |
| `R_ILIM` | $43.00\,\Omega$ | 15mA Current Limiter Sense Resistor | $1.0\%$ ($42.57\,\Omega$ to $43.43\,\Omega$) | | [ ] |

---

## 4. First-Boot & Dual-MCU Flashing Checklist

1. **Jumper Configuration for Isolated Flashing:**
   - Remove shunt on `JP_M1_3V3` (isolates MCU 1 on-board LDO from `+3V3_ANA`).
   - Remove shunt on `JP_M2_3V3` (isolates MCU 2 on-board LDO from `+3V3_DIG`).
2. **Flash MCU 1 (`WEMOS LOLIN32 Lite V1.0.0`):**
   - Connect USB-C cable directly to `U_MCU1`.
   - Upload firmware: `firmware/mcu1_analog_core/mcu1_analog_core.ino`.
   - Confirm Serial Monitor output:
     ```text
     [MCU1] Ge Parametric Engine - Quiet Analog Core Rev 5.0
     [MCU1] Radios Permanently Disabled: WiFi=OFF, BT=OFF
     [MCU1] ADS1115 Found @ 0x48. Status: OK
     [MCU1] Safe Hi-Z State Initialized. SR_OE_N=HIGH
     ```
3. **Flash MCU 2 (`ESP32-C3 SuperMini`):**
   - Connect USB-C cable directly to `U_MCU2`.
   - Upload firmware: `firmware/mcu2_ui_comms/mcu2_ui_comms.ino`.
   - Confirm Serial Monitor output:
     ```text
     [MCU2] UI & Comms Core Rev 5.0 Booted
     [MCU2] MCP23017 Expander Found @ 0x20
     [MCU2] BQ25895 Charger Found @ 0x6A. VREG programmed to 4.192V
     [MCU2] Dual ST7789 Displays Initialized
     ```
4. **Re-install Jumpers & Seat All Sockets:**
   - Disconnect USB cables. Re-install shunts on `JP_M1_3V3` and `JP_M2_3V3`.
   - Insert all socketed ICs (`U1..U3, U4, U5, U6, U7, U8, U9, U10, U11, U_ISO1, U_ISO2, MOD_ADS, MOD_CHG`).
   - Power up via main battery `J_BAT` or front USB-C jack with `SW_PWR = ON`.

---

## 5. Golden Standard Calibration Verification Matrix

*Insert known golden reference components into `J_ZIF` or `J_DUT3` and trigger test via `BTN_A`:*

| Golden Reference Component | Test Mode / Permutation | Expected Parameter | Expected ADS1115 Voltage / Range | Acceptable Error Tolerance | Measured Value | Pass/Fail |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **$100.0\,\Omega$ 0.1% Resistor** | Resistor Mode (`TP1–TP2`) | $R = 100.0\,\Omega$ | Kelvin Sense via $470\,\Omega$ ($S1/S5$) | $\pm 0.3\,\Omega$ ($0.3\%$) | | [ ] |
| **$1.000\text{ k}\Omega$ 0.1% Resistor** | Resistor Mode (`TP1–TP2`) | $R = 1000.0\,\Omega$ | R.G. Keen Shunt $2.49\text{ k}\Omega$ ($S2$) | $\pm 2.0\,\Omega$ ($0.2\%$) | | [ ] |
| **$10.00\text{ k}\Omega$ 0.1% Resistor** | Resistor Mode (`TP2–TP3`) | $R = 10.000\text{ k}\Omega$ | $V_{\text{DUT}} \approx 1.80\text{ V}$ | $\pm 20\,\Omega$ ($0.2\%$) | | [ ] |
| **$100.0\text{ k}\Omega$ 0.1% Resistor** | Resistor Mode (`TP1–TP3`) | $R = 100.00\text{ k}\Omega$ | Micro-leakage shunt $100\text{ k}\Omega$ | $\pm 200\,\Omega$ ($0.2\%$) | | [ ] |
| **$1.000\text{ M}\Omega$ 0.1% Resistor** | Resistor Mode (`TP1–TP2`) | $R = 1.0000\text{ M}\Omega$ | Base shunt $2.20\text{ M}\Omega$ | $\pm 3.0\text{ k}\Omega$ ($0.3\%$) | | [ ] |
| **1N4148 Silicon Diode** | Diode Mode (`A=TP1, K=TP2`) | $V_F = 620\text{ to }680\text{ mV}$ | Measured at $I_F \approx 1.0\text{ mA}$ | $\pm 10\text{ mV}$ | | [ ] |
| **1N34A Germanium Diode** | Diode Mode (`A=TP1, K=TP2`) | $V_F = 260\text{ to }320\text{ mV}$ | Measured at $I_F \approx 1.0\text{ mA}$ | $\pm 15\text{ mV}$ | | [ ] |
| **1N5819 Schottky Diode** | Diode Mode (`A=TP1, K=TP2`) | $V_F = 320\text{ to }380\text{ mV}$ | Measured at $I_F \approx 1.0\text{ mA}$ | $\pm 12\text{ mV}$ | | [ ] |
| **5.1V Zener Diode** | Zener Mode (`K=TP1, A=TP2`) | $V_Z = 5.00\text{ to }5.20\text{ V}$ | Measured at $I_Z \approx 5.0\text{ mA}$ via `+9V_LIM` | $\pm 50\text{ mV}$ | | [ ] |
| **2N3904 (Silicon NPN)** | BJT Auto-Detect Permutations | Pinout: `E-B-C`<br/>$h_{FE} = 150\text{–}250$<br/>$I_{CEO} < 0.1\,\mu\text{A}$ | $I_{CEO}$ in PGA $\pm 0.256\text{V}$ reads $\approx 0\text{ counts}$ | $h_{FE} \pm 3\%$<br/>$I_{CEO} < 0.5\,\mu\text{A}$ | | [ ] |
| **2N3906 (Silicon PNP)** | BJT Auto-Detect Permutations | Pinout: `E-B-C`<br/>$h_{FE} = 150\text{–}250$<br/>$I_{CEO} < 0.1\,\mu\text{A}$ | Reverse polarity checked | $h_{FE} \pm 3\%$<br/>$I_{CEO} < 0.5\,\mu\text{A}$ | | [ ] |
| **AC128 (Germanium PNP)** | Vintage Ge Metrology Mode | Pinout: `E-B-C`<br/>$h_{FE(\text{true})} = 60\text{–}120$<br/>$I_{CEO} = 40\text{–}180\,\mu\text{A}$ | PGA $\pm 0.512\text{V}$ or $\pm 1.024\text{V}$ auto-ranged | $h_{FE} \pm 2.5\%$<br/>$I_{CEO} \pm 1.5\,\mu\text{A}$ | | [ ] |
| **2N1308 (Germanium NPN)** | Vintage Ge Metrology Mode | Pinout: `E-B-C`<br/>$h_{FE(\text{true})} = 80\text{–}150$<br/>$I_{CEO} = 20\text{–}90\,\mu\text{A}$ | Polarities inverted solid-state | $h_{FE} \pm 2.5\%$<br/>$I_{CEO} \pm 1.5\,\mu\text{A}$ | | [ ] |
| **2N4393 (JFET Golden DUT)** | JFET Characterization Mode | Pinout: `D-S-G`<br/>$I_{DSS} = 5.0\text{–}30\text{ mA}$<br/>$V_{GS(\text{off})} = -1.5\text{ to }-3.0\text{ V}$ | Gate stepped via $V_{\text{SRC\_B}}$ | $I_{DSS} \pm 5\%$<br/>$V_{GS(\text{off})} \pm 50\text{ mV}$ | | [ ] |
| **2N7000 (MOSFET Golden DUT)** | MOSFET Characterization Mode | Pinout: `S-G-D`<br/>$V_{GS(\text{th})} = 1.8\text{–}2.4\text{ V}$ | Gate ramped via $V_{\text{SRC\_B}}$ | $V_{GS(\text{th})} \pm 30\text{ mV}$ | | [ ] |

---

## 6. Functional & Audio Verification

1. **56k Dial-Up Modem Audio Engine:**
   - Turn `SW_BZ` to `ON` (unmute).
   - Initiate component scan: confirm authentic retro 56k modem chirp burst (dual-tone V.21 carrier, phase reversals, and FSK chirp) plays dynamically during active testing.
   - Confirm test-complete chime plays upon hash generation.
   - Turn `SW_BZ` to `MUTE`: verify zero acoustic emission during scan.
2. **Deterministic Characterization Hash:**
   - Confirm LCD Screen 2 displays unique hash tag (e.g., `GE-PNP-H108-L064-3F8A`).
   - Confirm hash matches serial debug output and web dashboard JSON payload.
3. **Standby Mode & RF Operation:**
   - Slide `SW_STBY` to `STANDBY`:
     - Confirm Dual LCD backlights turn off.
     - Confirm Analog Rail drops to $0.000\text{ V}$.
     - Connect to Wi-Fi SSID `GE_ENGINE_PRO` or open Web Dashboard at `http://192.168.4.1`.
     - Verify saved transistor database is queryable remotely via REST API while instrument rests in low-power standby.

---

## 7. Sign-Off & Calibration Record
- **Instrument Serial Number:** `GE-PCE-2026-` __________________
- **Calibrated By (Lead Engineer):** ___________________________
- **Calibration Date:** ______________________________________
- **Reference Bench DMM Model & Cal Date:** ___________________
- **Final Verdict:** **[ ] PASS — CALIBRATED & LAB-READY**   **[ ] REJECT**
