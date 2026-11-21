# Phase 3 — Hardware Assembly, Power-Rail-First Bring-Up, Dual-MCU Flashing & 2-Point Calibration Guide (`p3_bringup_guide.md`)

**Document ID:** `p3_bringup_guide.md`  
**Subsystem:** Step-by-Step Through-Hole Assembly Order, Pre-IC Power Rail & `MT3608` $14.50\text{ V}$ Trim Verification, `BQ25895` $4.192\text{ V}$ Non-HV LiPo Initialization, Jumper-Isolated Dual-MCU Flashing, AFE Smoke Test & 2-Point Shunt Self-Calibration  
**Architecture Revision:** Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine

---

## 1. Step-by-Step 100% Through-Hole (THT) Assembly Sequence

Because every component on the Rev 5.0 PCB is 100% through-hole or seated in a 2.54 mm machined female socket, solder components in strictly ascending profile height (`3.5 mm` $\to$ `24.0 mm`) so the board rests flat on the bench when inverted:

1. **Stage 1 — Low-Profile Axial Resistors & Star Bridge ($H = 3.5\text{ mm}$):**
   - Solder **`R_STAR` (`0R_STAR_BRIDGE`)** first across the $X = 152.40\text{ mm}$ `DGND`–`AGND` moat and verify $< 10\text{ m}\Omega$ continuity between the two ground planes.
   - Solder all 35 precision **`0.1%` $25\text{ ppm}/^\circ\text{C}$ metal-film resistors** in Blocks 5 & 6 (`R_PR1..3 = 100 Ω 0.5W 0.1%`, `R_LA1..3 = 470 Ω 0.1%`, `R_KA1..3 = 2.49 kΩ 0.1%`, `R_MB1..3 = 100 kΩ 0.1%`, `R_KB1..3 = 2.20 MΩ 0.1%`, `R_LG1..3 = 470 Ω 0.1%`, `R_KG1..3 = 2.49 kΩ 0.1%`, `R_G1A/G2A = 34.8 kΩ 0.1%`, `R_G1B/G2B = 10.0 kΩ 0.1%`, `R_D1A..4A = 1.50 kΩ 0.1%`, `R_D1B..4B = 499 Ω 0.1%`, `R_HV1 = 100 kΩ 0.1%`, `R_HV2 = 10.0 kΩ 0.1%`). *Tip: Measure each `0.1%` resistor with a 4-wire DMM before soldering; both the reference designator and exact value are printed on `F.SilkS`.*
   - Solder the Pi-filter damping resistors (`R_DAMP1 = 2.2 Ω 0.5W`, `R_DAMP2 = 1.5 Ω 0.25W`), current-limiter sense resistor (`R_ILIM = 43 Ω 1%`), ADC RC resistor (`R_ADC_F = 10 Ω 1%`), and all remaining `1%` pull-up/pull-down/debounce/LED resistors.
2. **Stage 2 — Axial Glass & Power Diodes ($H = 4.0\text{–}5.2\text{ mm}$):**
   - Align the cathode band with the heavy silkscreen bar and `"K"` mark on `F.SilkS` (Pin 1 = Square Pad = Cathode):
     - `8× 1N3595` ultra-low-leakage (`<1 nA`) clamps (`D_CH1..3`, `D_CL1..3`, `D_HVH`, `D_HVL`). *Clean the glass bodies and surrounding `AGND` guard ring with 99.9% isopropyl alcohol (IPA) after soldering to remove flux residue.*
     - `D_BZ` (`1N4148`), `D_OVP` (`1N4743A` $13.0\text{ V}$ 1W Zener), and `D_TVS_USB` (`P6KE15A` $15\text{ V}$ TVS or `1N5919B` $5.6\text{ V}$ Zener).
3. **Stage 3 — Ceramic Disc Capacitors (`C0G` & `X7R`) and Machined IC / Transistor Sockets ($H = 5.0\text{–}8.5\text{ mm}$):**
   - Solder the `4× 100 nF C0G/NP0` zero-piezo ADC anti-aliasing capacitors (`C_A0..C_A3`) and all `10 nF`, `100 nF`, and `1 µF` ceramic decoupling/debounce capacitors.
   - Solder all **DIP Machined IC Sockets** (orienting the half-moon notch with the `F.SilkS` arc at the `-Y` top edge): `2× DIP-6` (`U_ISO1, U_ISO2`), `2× DIP-8` (`U5, U9`), `1× DIP-14` (`U4`), `7× DIP-16` (`U1..U3, U6..U8, U10`), and `1× DIP-28` (`U11`). **Leave all DIP sockets EMPTY for now!**
   - Solder the `8× 1x03` machined TO-92 sockets (`U_REG9, Q_LIM1, Q_LIM2, Q_OVC, Q_ANA_N, Q_BLK, Q_BZ, J_TEMP`) and the `J_DUT3` 3-pin gold DUT socket. Seat the TO-92 transistors/regulator (`L78L09ACZ`, `2× 2N3906`, `2× 2N3904`, `2× 2N7000`, `DS18B20`) matching their flat face to the `"FLAT"` silkscreen label.
4. **Stage 4 — Female Module Headers, Pin Headers, LEDs, Switches, Inductors, Electrolytics & TO-220s ($H = 8.5\text{–}24.0\text{ mm}$):**
   - Solder the female pin sockets for `U_MCU1` (`2× 1x13`), `U_MCU2` (`2× 1x08`), `MOD_CHG` (`2× 1x06`), `MOD_ADS` (`1× 1x10`), `MOD_TFT1/TFT2` (`2× 1x08`), `J_USB_C` (`1x06`), and the 4 corner sockets for `MOD_BOOST`. **Do NOT insert `U_MCU1`, `U_MCU2`, `MOD_ADS`, or `MOD_TFT1/2` yet!**
   - Solder `JP_M1_3V3`, `JP_M2_3V3`, `J_BAT`, `J_NTC`, `J_KELVIN`, `J_CYD_UART`, `L1` ($100\text{ }\mu\text{H}$), `L2` ($47\text{ }\mu\text{H}$), `F_USB` (`RXEF175`), `F1` (`RXEF017`), `BZ1`, `D_LED1..6`, `D1, D2`, all 12 radial electrolytic capacitors (observing `+` square pad polarity), `SW_PWR` (DPDT), `SW_STBY`, `SW_BZ`, `SW_UP..RIGHT`, `SW_A/B`, `ENC1`, `J_ZIF`, `J_TERM5`, and the 5 vertical TO-220 devices (`Q_REV_BAT`, `Q_ANA_SW` [`NDP6020P`], `U_LDO_DIG`, `U_LDO_ANA` [`MCP1826S-3302E/AB`], and `U_REG12` [`L7812CV`]).

---

## 2. Phase A Bring-Up — Power-Rail-First Verification & `MT3608` $14.50\text{ V}$ Pre-Trim (All IC & MCU Sockets Empty!)

> [!CAUTION]
> **NEVER insert any DIP ICs (`U1..U11`, `U_ISO1..2`), `U_MCU1`, `U_MCU2`, `MOD_ADS`, or `MOD_TFT1/2` before trimming `MOD_BOOST` (`MT3608`) to $14.50\text{ V}$ and verifying all 6 regulated supply rails!** Untrimmed `MT3608` modules often ship from the factory set to $24\text{ V–}28\text{ V}$.

1. **Zero-Drain `SW_PWR = OFF` Check:**
   - Remove jumpers `JP_M1_3V3` and `JP_M2_3V3`. Seat only `MOD_CHG` (`BQ25895_Module`) and `MOD_BOOST` (`MT3608`) in their sockets.
   - Set `SW_PWR = OFF`. Connect a standard **$3.7\text{ V}$ Non-HV LiPo battery ($4.20\text{ V}$ max)** to `J_BAT` in series with a DMM on the $\mu\text{A}$ range.
   - **Pass Criterion:** Battery current is strictly **$0.000\text{ }\mu\text{A}$**, and `TP2` (`V_BUS_SYS`) reads **$0.000\text{ V}$**.
   - Plug a $5.0\text{ V}$ USB-C cable into `J_USB_C` while `SW_PWR = OFF`. Verify `TP1` (`V_USB_PROT`) $= 5.00\text{ V} \pm 0.25\text{ V}$, `TP2` (`V_BUS_SYS`) $= 0.000\text{ V}$, and battery current remains **$0.000\text{ mA}$** (confirming zero blind charging at the `BQ25895` $4.208\text{ V}$ hardware default).
2. **`+3V3_DIG` & Standby Cutoff Check:**
   - Set `SW_PWR = ON`. Verify Top-Bezel LED #1 (`D_LED1` Green) illuminates, `TP2` (`V_BUS_SYS`) reads $3.50\text{ V–}4.20\text{ V}$, and **`TP3` (`+3V3_DIG`) reads $3.300\text{ V} \pm 0.016\text{ V}$**.
   - Verify that with `U11` unpopulated, `R_PD_ANA` ($100\text{ k}\Omega$) holds `ANA_PWR_EN` at $0.000\text{ V}$, so **`TP4` (`V_ANA_IN`) reads $0.000\text{ V}$** (Analog Core boots safely OFF).
3. **`MOD_BOOST` (`MT3608`) $14.50\text{ V}$ Trim & Analog Rail Verification:**
   - Temporarily connect a $10\text{ k}\Omega$ resistor between `U11` Socket Pin 9 (`+3V3_DIG`) and Pin 8 (`ANA_PWR_EN`) to turn ON `Q_ANA_N` and `Q_ANA_SW`. Verify **`TP4` (`V_ANA_IN`) $\approx V_{\text{BUS\_SYS}}$**.
   - Place the DMM positive probe on `MOD_BOOST` Pin 3 (`V_BOOST_RAW`) or **`TP5` (`V_14V_FILT`)** (referenced to `AGND` at `R_STAR` Pin 2) and turn the brass screw on the `MT3608` multi-turn potentiometer until **`V_BOOST_RAW` $= 14.50\text{ V} \pm 0.05\text{ V}$** (**`TP5` $= 14.44\text{ V} \pm 0.08\text{ V}$**).
   - Verify the downstream analog regulators:
     - **`TP6` (`+12V_A`):** **$11.92\text{ V to } 12.05\text{ V}$** (`U_REG12` `L7812CV` output after `F1`).
     - **`TP7` (`+9V_REF`):** **$8.92\text{ V to } 9.08\text{ V}$** (`U_REG9` `L78L09ACZ` output; Green `D1` LED ON).
     - **`TP8` (`+9V_LIM`):** **$8.85\text{ V to } 9.00\text{ V}$** (`Q_LIM1` saturated pass output).
     - **`TP9` (`+3V3_ANA`):** **$3.284\text{ V to } 3.316\text{ V}$** (`U_LDO_ANA` `MCP1826S-3302E/AB` output).
     - **`TP10` (`+3V3_ADC`):** **$3.283\text{ V to } 3.315\text{ V}$** (after `R_ADC_F = 10 Ω`).
4. **15.12 mA Active Current Limiter & `FAULT_OVC_N` Hardware Trip Verification:**
   - Verify **`TP16` (`FAULT_OVC_N`)** idles at **$+3.300\text{ V}$ (`HIGH`)**.
   - Briefly connect a $100\,\Omega$ $0.5\text{ W}$ test load in series with a milliammeter from **`TP8` (`+9V_LIM`)** to **`AGND`**.
   - **Pass Criterion:** Short-circuit current folds back and clamps at **$15.12\text{ mA} \pm 1.50\text{ mA}$**, and **`TP16` (`FAULT_OVC_N`) drops to $\le 0.25\text{ V}$ (`LOW`)**.
   - Remove the test load and temporary `ANA_PWR_EN` jumper, and switch `SW_PWR = OFF`.

---

## 3. Phase B Bring-Up — Jumper-Isolated Dual-MCU Flashing & `BQ25895` $4.192\text{ V}$ Lock

1. **Flashing `U_MCU2` (`ESP32-C3 SuperMini` — UI/Comms/Charger Master):**
   - **Ensure jumper `JP_M2_3V3` is REMOVED (Open)** so the ESP32-C3 SuperMini's onboard LDO cannot contend with `U_LDO_DIG`.
   - Seat `U_MCU2` into `Socket_ESP32_C3_SuperMini` (USB-C facing top `-Y`, red ceramic antenna facing bottom `+Y` over its keep-out zone) and connect a USB-C data cable to `U_MCU2` to flash the MCU 2 UI/Comms firmware.
   - Unplug the programming cable from `U_MCU2` and **install the 2-pin shunt on `JP_M2_3V3`**.
2. **Flashing `U_MCU1` (`WEMOS LOLIN32 Lite` — Quiet Analog Metrology Slave):**
   - **Ensure jumper `JP_M1_3V3` is REMOVED (Open)** so the LOLIN32 Lite's onboard `ME6211` LDO cannot back-feed `+3V3_ANA`.
   - Seat `U_MCU1` into `Socket_Wemos_Lolin32_Lite` (meander antenna facing top `-Y` over its keep-out zone, USB-C at bottom `+Y`) and flash the MCU 1 Metrology firmware via USB-C.
   - Unplug the programming cable from `U_MCU1` and **install the 2-pin shunt on `JP_M1_3V3`**.
3. **Verifying `BQ25895` $4.192\text{ V}$ Non-HV Register Programming:**
   - Seat `U11` (`MCP23017-E/SP`), `MOD_TFT1`, and `MOD_TFT2`. Throw `SW_PWR = ON` with USB-C plugged into `J_USB_C`.
   - Confirm on `MOD_TFT2`'s boot diagnostic banner (or via $\text{I}^2\text{C}$ readback of `0x6A`) that `U_MCU2` has written **`REG06 = 0x58` ($V_{\text{REG}} = 4.192\text{ V} \le 4.200\text{ V}$)**, **`REG07 = 0x8D` (Watchdog Disabled)**, **`REG04 = 0x20` ($I_{\text{CHG}} = 2.048\text{ A}$ clamped $\le 1\text{C}$)**, and **`REG05 = 0x11` ($I_{\text{TERM}} = 128\text{ mA}$)**.

---

## 4. Phase C Bring-Up — AFE Socket Population, Opto-UART Smoke Test & 2-Point Self-Calibration

1. **Populating the Analog Front-End Sockets:**
   - With `SW_PWR = OFF`, seat all remaining socketed ICs observing Pin 1 notch orientation: `U_ISO1, U_ISO2` (`H11L1M`), `U9` (`MCP4822-E/P`), `U5` (`TLC2272ACP`), `U4` (`LMC6484IN`), `U10` (`DG409DJ`), `U6, U7` (`74HC595N`), `U8` (`DG412LDJ`), `U1..U3` (`DG408DJ`), and `MOD_ADS` (`ADS1115`).
2. **Boot-Safe Hi-Z & Opto-UART Handshake Smoke Test:**
   - Set `SW_STBY = ACTIVE` (`HIGH`) and `SW_PWR = ON`.
   - Verify **`TP15` (`SR_OE_N`)** powers up at **$+3.298\text{ V}$ (`HIGH`)**, keeping all 27 analog matrix switches strictly `OPEN`, and **`TP13` (`TP1_DUT..TP3_DUT`) reads $0.000\text{ V}$**.
   - Verify `U_MCU2` dispatches `CMD_PING (0x01)` across `U_ISO2` and receives `EVT_ACK (0x80)` across `U_ISO1` within $< 15\text{ ms}$, displaying ambient temperature from `J_TEMP` (`DS18B20`, $\sim 24.0^\circ\text{C}$) on `MOD_TFT2`.
3. **Executing the 2-Point Internal R.G. Keen & Kelvin Self-Calibration (`CMD_CALIBRATE 0x12`):**
   - Ensure `J_ZIF`, `J_DUT3`, `J_KELVIN`, and `J_TERM5` are **completely empty (Open Socket)**.
   - Press `MENU` / `CAL` on the front panel to trigger `CMD_CALIBRATE (0x12)` with `save_nvs = 1`:
     - **Point 1 (Zero-Current Offset & Open-Socket Leakage Tare):** Firmware closes `S7` (`AGND`) on each channel `ch = 1..3` to measure and store the exact combined offset of `U4` (`LMC6484IN`), the `1.50k / 499R` divider, and `MOD_ADS`, then opens all switches (`0x0000`) to verify open-socket leakage is $< 0.5\text{ nA}$.
     - **Point 2 (Closed-Loop DAC Ratio & Switch $R_{\text{ON}}$ Loopback Calibration):** Firmware steps `V_SRC_A` and `V_SRC_B` while reading back `U10` (`S1A, S2A, S3A`), then closes `S2` (`R_KA = 2.49 kΩ 0.1%`) against `U8` (`DG412LDJ` $20\,\Omega$ `AGND` clamp) on each channel to measure the exact individual switch on-resistance $R_{\text{ON,S2}}(\text{cal})$ and $R_{\text{ON,U8}}(\text{cal})$ to within **$\pm 0.5\,\Omega$**, committing all calibration constants to `U_MCU1` NVS flash.
   - Insert the Golden-Sample reference resistors ($100.0\,\Omega$, $2.490\text{ k}\Omega$, $100.0\text{ k}\Omega$ $0.1\%$) and a reference PNP Germanium transistor (`OC44` / `AC128`) into `J_ZIF` to confirm True-$h_{\text{FE}}$ and $I_{\text{CEO}}$ readout accuracy within $\pm 0.25\%$.
