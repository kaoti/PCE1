# Phase 1 — Pinout Extraction, Rev 5.0 Dual-MCU Ge Parametric Characterization Engine

Sources: assignment #1 (authoritative Rev 5.0 map); baseline cross-check against `scripts/build_libraries.py` (symbol `Wemos_Lolin32_Lite`, footprint `Socket_Wemos_Lolin32_Lite`) and `scripts/build_design.py` (`U_ESP`, `U11`).

## 0. Baseline cross-verification

| Item | Baseline (scripts) | Rev 5.0 | Result |
|---|---|---|---|
| LOLIN32 Lite footprint | 2 columns × 13 pads, 2.54 mm pitch, columns 22.86 mm apart; pads 1–13 left, 14–26 right, pad 1 square | 26-pin 2x13 @ 22.86 mm | MATCH |
| LOLIN32 Lite symbol pin numbers/names 1–26 | VP/36, VN/39, EN, IO34, IO35, IO32, IO33, 25, 26, IO27, IO14, IO12, GND / 3V3, IO22, IO19, IO23, IO18, IO5, IO17, IO16, IO4, IO0, IO2, IO15, IO13 | identical GPIO per pin | MATCH (all 26) |
| Strapping IO0/2/5/12/15 | `None` (NC) in `U_ESP` | NC | MATCH |
| Net names on U_ESP | Rev 4 nets (`MCP_INT`, `TFT2_CS`, `TFT_DC`, `SPI_MOSI`, `I2C_SDA`, `TFT1_CS`, pin 26 spare…) | Rev 5 nets below | **Scripts need re-netting (Rev 5 differs in pins 6–11, 16–17, 21, 26)** |
| MCP23017 `U11` net map in `build_design.py` | Legacy ad-hoc mapping (A0–A2 → GND, RESET → +3V3, INT pins used) | Rev 5 map in §3 | **Must be rewritten.** Pin numbers below are per datasheet DIP-28 |
| DIP-28 footprint | `Socket_DIP-28_W7.62mm` | DIP-28 | MATCH |

Baseline has no symbol/footprint for the ESP32-C3 SuperMini, JP_M1_3V3/JP_M2_3V3 or J_CYD_UART. These are new parts (see §2, §4). The existing `Header_1x04_P2.54mm` is reusable for J_CYD_UART, and `Header_1x02_P2.54mm` for the jumpers.

## 1. MCU 1 — `U_MCU1` WEMOS LOLIN32 Lite V1.0.0 (ESP32 REV1, USB-C)

Footprint: `Socket_Wemos_Lolin32_Lite`. Quiet analog core, Wi-Fi/BT OFF. There is no 5V header pin. GPIO6–11 are internal flash and are not broken out.

### Left header, pins 1–13

| Pin | Silk | GPIO | Net | Dir | Passive / note |
|---|---|---|---|---|---|
| 1 | VP/36 | 36 | `FAST_TP1` | in | input-only |
| 2 | VN/39 | 39 | `FAST_TP2` | in | input-only |
| 3 | EN | — | NC | — | reset, left alone |
| 4 | IO34 | 34 | `ADS_ALRT` | in | input-only; 10k pull-up to `+3V3_ANA` (external, as 34 has no internal pull) |
| 5 | IO35 | 35 | `FAST_TP3` | in | input-only |
| 6 | IO32 | 32 | `CAL_A0` | out | |
| 7 | IO33 | 33 | `CAL_A1` | out | |
| 8 | IO25 | 25 | `FAULT_OVC_N` | in | |
| 9 | IO26 | 26 | `DAC_CS` | out | 10k pull-up to `+3V3_ANA` |
| 10 | IO27 | 27 | `AFE_SPI_MOSI` | out | |
| 11 | IO14 | 14 | `AFE_SPI_SCK` | out | |
| 12 | IO12 | 12 | NC | — | STRAPPING (MTDI, flash voltage), untouched |
| 13 | GND | — | `AGND` | pwr | |

### Right header, pins 14–26

| Pin | Silk | GPIO | Net | Dir | Passive / note |
|---|---|---|---|---|---|
| 14 | 3V3 | — | `M1_3V3` → `JP_M1_3V3` (2-pin) → `+3V3_ANA` | pwr | Jumper is removed during USB-C flash to avoid dual-LDO contention |
| 15 | IO22 | 22 | NC | — | onboard LED |
| 16 | IO19 | 19 | `ANA_I2C_SDA` | io | 4.7k pull-up to `+3V3_ANA` |
| 17 | IO23 | 23 | `ANA_I2C_SCL` | out | 4.7k pull-up to `+3V3_ANA` |
| 18 | IO18 | 18 | `SR_LATCH` | out | 10k pull-down to `AGND` |
| 19 | IO5 | 5 | NC | — | STRAPPING, untouched |
| 20 | IO17 | 17 | `SR_OE_N` | out | non-strapping; 10k pull-up to `+3V3_ANA` (outputs disabled at boot) |
| 21 | IO16 | 16 | `LINK_TX_M1` | out | UART2 TX → `U_ISO1` |
| 22 | IO4 | 4 | `TEMP_DQ` | io | 4.7k pull-up to `+3V3_ANA` (1-Wire) |
| 23 | IO0 | 0 | NC | — | STRAPPING, untouched |
| 24 | IO2 | 2 | NC | — | STRAPPING, untouched |
| 25 | IO15 | 15 | NC | — | STRAPPING, untouched |
| 26 | IO13 | 13 | `LINK_RX_M1` | in | UART2 RX ← `U_ISO2` |

Count: 26 pins, with 17 signals, 2 power/ground, and 7 NC (EN, IO12, IO22, IO5, IO0, IO2, IO15).

## 2. MCU 2 — `U_MCU2` ESP32-C3 SuperMini (HZWDONE)

Package: 16-pin 2x8 @ 15.24 mm pitch, 18.0 × 22.52 mm. Top view has USB-C at the top and the red ceramic C3 antenna at the bottom. This is a new footprint; the 2.54 mm pitch rows are 15.24 mm apart.

### Left header, pins 1–8 (top to bottom)

| Pin | GPIO | Net | Dir | Passive / note |
|---|---|---|---|---|
| 1 | GPIO5 | `TFT_DC` | out | |
| 2 | GPIO6 | `UI_SPI_MOSI` | out | |
| 3 | GPIO7 | `TFT1_CS` | out | 10k pull-up to `+3V3_DIG` |
| 4 | GPIO8 | NC | — | STRAPPING + onboard LED, untouched |
| 5 | GPIO9 | NC | — | STRAPPING + BOOT button, untouched |
| 6 | GPIO10 | `TFT2_CS` | out | 10k pull-up to `+3V3_DIG` |
| 7 | GPIO20 | `LINK_RX_M2` | in | ← `U_ISO1` (UART0 RX) |
| 8 | GPIO21 | `LINK_TX_M2` | out | → `U_ISO2` (UART0 TX) |

### Right header, pins 9–16

| Pin | GPIO | Net | Dir | Passive / note |
|---|---|---|---|---|
| 9 | 5V | NC | — | Powered via the 3V3 rail (runs down to 3.40 V LiPo) |
| 10 | GND | `DGND` | pwr | |
| 11 | 3V3 | `M2_3V3` → `JP_M2_3V3` (2-pin) → `+3V3_DIG` | pwr | |
| 12 | GPIO4 | `UI_SPI_SCK` | out | |
| 13 | GPIO3 | `BZ_PWM` | out | LEDC hardware PWM |
| 14 | GPIO2 | NC | — | STRAPPING, untouched |
| 15 | GPIO1 | `UI_I2C_SCL` | out | 4.7k pull-up to `+3V3_DIG` |
| 16 | GPIO0 | `UI_I2C_SDA` | io | 4.7k pull-up to `+3V3_DIG` |

Native USB-JTAG GPIO18/19 are not broken out to the header and stay on USB-C.

Count: 16 pins, with 11 signals, 2 power/ground, and 3 NC strapping pins (GPIO8, GPIO9, GPIO2) plus 5V NC.

## 3. GPIO expander `U11` — MCP23017-E/SP (DIP-28)

Address 0x20 on the MCU 2 UI I2C bus (`UI_I2C_SDA`/`UI_I2C_SCL`). A2/A1/A0 = `DGND`. RESET → `+3V3_DIG` through 10k, with 100nF to `DGND`. Polled at 500 Hz. INTA/INTB are NC (polling, no interrupt lines).

### DIP-28 pin map (datasheet)

| DIP pin | Function | Net |
|---|---|---|
| 1 | GPB0 | `ENC_A` |
| 2 | GPB1 | `ENC_B` |
| 3 | GPB2 | `LED_STBY_RF` |
| 4 | GPB3 | `LED_SCAN` |
| 5 | GPB4 | `LED_DUT_OK` |
| 6 | GPB5 | `LED_FAULT` |
| 7 | GPB6 | `TFT_BLK_EN` |
| 8 | GPB7 | `ANA_PWR_EN` |
| 9 | VDD | `+3V3_DIG` (with 100nF decoupling) |
| 10 | VSS | `DGND` |
| 11 | NC | NC |
| 12 | SCL | `UI_I2C_SCL` |
| 13 | SDA | `UI_I2C_SDA` |
| 14 | NC | NC |
| 15 | A0 | `DGND` |
| 16 | A1 | `DGND` |
| 17 | A2 | `DGND` |
| 18 | RESET | `+3V3_DIG` via 10k, 100nF to `DGND` |
| 19 | INTB | NC |
| 20 | INTA | NC |
| 21 | GPA0 | `BTN_UP` |
| 22 | GPA1 | `BTN_DOWN` |
| 23 | GPA2 | `BTN_LEFT` |
| 24 | GPA3 | `BTN_RIGHT` |
| 25 | GPA4 | `BTN_A` |
| 26 | GPA5 | `BTN_B` |
| 27 | GPA6 | `ENC_SW` |
| 28 | GPA7 | `SW_STBY` |

### Port function table

| Port bit | Net | Field circuit |
|---|---|---|
| GPA0–3 | `BTN_UP/DOWN/LEFT/RIGHT` | active-LOW, 10k pull-up + 100nF RC debounce |
| GPA4, GPA5 | `BTN_A`, `BTN_B` | active-LOW, 10k + 100nF |
| GPA6 | `ENC_SW` | EC11 push button, 10k + 100nF |
| GPA7 | `SW_STBY` | slide switch: LOW = Standby/RF, HIGH = Active; 10k pull-up |
| GPB0, GPB1 | `ENC_A`, `ENC_B` | EC11 quadrature, 10k pull-up + 10nF RC debounce |
| GPB2 | `LED_STBY_RF` (Cyan, top LED #3) | 470R to `DGND` |
| GPB3 | `LED_SCAN` (Blue, #4) | 470R to `DGND` |
| GPB4 | `LED_DUT_OK` (Green, #5) | 470R to `DGND` |
| GPB5 | `LED_FAULT` (Red, #6) | 470R to `DGND` |
| GPB6 | `TFT_BLK_EN` | drives `Q_BLK` (2N7000 low-side / BLK enable); LOW in Standby |
| GPB7 | `ANA_PWR_EN` | 100k pull-down to `DGND`; drives `Q_ANA_N` (2N7000) → `Q_ANA_SW` (NDP6020P high-side). Boots OFF |

All MCP23017 pins power up as inputs, so `ANA_PWR_EN` stays OFF through the 100k pull-down. Firmware must write the GPB7 latch to 0 before setting IODIR as an output.

## 4. Peripherals

### `MOD_TFT1`, `MOD_TFT2` — EstarDyn 1.9" IPS ST7789 SPI (8-pin)

| Pin | Name | MOD_TFT1 net | MOD_TFT2 net |
|---|---|---|---|
| 1 | GND | `DGND` | `DGND` |
| 2 | VCC | `+3V3_DIG` | `+3V3_DIG` |
| 3 | SCL | `UI_SPI_SCK` | `UI_SPI_SCK` |
| 4 | SDA | `UI_SPI_MOSI` | `UI_SPI_MOSI` |
| 5 | RES | `TFT_RES` | `TFT_RES` |
| 6 | DC | `TFT_DC` | `TFT_DC` |
| 7 | CS | `TFT1_CS` | `TFT2_CS` |
| 8 | BLK | `TFT_BLK` (switched by `Q_BLK`) | `TFT_BLK` |

`TFT_RES` is a hardware RC: 10k to `+3V3_DIG` and 1µF to `DGND`. This replaces the legacy MCP23017-driven reset.

### `MOD_ADS` — ADS1115 10-pin breakout (matches baseline symbol `ADS1115_Module`)

| Pin | Name | Net |
|---|---|---|
| 1 | VDD | `+3V3_ADC` |
| 2 | GND | `AGND` |
| 3 | SCL | `ANA_I2C_SCL` |
| 4 | SDA | `ANA_I2C_SDA` |
| 5 | ADDR | `AGND` (0x48) |
| 6 | ALRT | `ADS_ALRT` (→ MCU1 IO34) |
| 7 | A0 | `ADC_TP1` |
| 8 | A1 | `ADC_TP2` |
| 9 | A2 | `ADC_TP3` |
| 10 | A3 | `ADC_CAL` |

### `J_CYD_UART` — 4-pin THT header (`Header_1x04_P2.54mm`)

| Pin | Net |
|---|---|
| 1 | `V_BUS_SYS` |
| 2 | `DGND` |
| 3 | `CYD_TX_TAP`, via 470R from `LINK_TX_M2` |
| 4 | `CYD_RX_TAP`, via 470R from `LINK_RX_M2` |

The pin order matches the ESP32-2432S028R (CYD) P1 UART header, and the tap supports the Wi-Fi JSON option. The 470R resistors isolate the tap so an attached CYD cannot overdrive the link.

## 5. Silicon-datasheet constraint compliance audit

| # | Constraint | Pins | Rev 5.0 usage | Verdict |
|---|---|---|---|---|
| 1 | ESP32 strapping: IO0 (boot mode) | MCU1 P23 | NC | PASS |
| 2 | ESP32 strapping: IO2 | MCU1 P24 | NC | PASS |
| 3 | ESP32 strapping: IO5 (SDIO timing) | MCU1 P19 | NC | PASS |
| 4 | ESP32 strapping: IO12/MTDI (flash voltage; a high level at boot can brown out) | MCU1 P12 | NC | PASS |
| 5 | ESP32 strapping: IO15 (boot log, MTDO) | MCU1 P25 | NC | PASS |
| 6 | Flash GPIO6–11 | not on header | unused | PASS |
| 7 | Input-only GPIO34/35/36/39 are never driven | P4 `ADS_ALRT`, P5, P1, P2 | inputs only, no internal pull; external 10k on IO34 | PASS |
| 8 | IO17/IO16 free of PSRAM conflict (LOLIN32 Lite has no PSRAM) | P20, P21 | used as `SR_OE_N` and UART2 TX | PASS |
| 9 | JTAG pins IO12/13/14/15 | P12 NC, P26 `LINK_RX_M1`, P11 `AFE_SPI_SCK`, P25 NC | only IO13/14 are used, as GPIO; no JTAG debug in use | PASS (note) |
| 10 | EN pin left alone | MCU1 P3 | NC | PASS |
| 11 | LOLIN32 Lite has no 5V pin; no 5V net connected | P14 is 3V3 only | `JP_M1_3V3` isolates the LDO from `+3V3_ANA` during USB flash | PASS |
| 12 | C3 strapping GPIO2 | MCU2 P14 | NC | PASS |
| 13 | C3 strapping GPIO8 (also LED) | MCU2 P4 | NC | PASS |
| 14 | C3 strapping GPIO9 (also BOOT) | MCU2 P5 | NC | PASS |
| 15 | C3 USB-JTAG GPIO18/19 | not on header | untouched, USB-C native | PASS |
| 16 | C3 SPI-flash GPIO12–17 | not on SuperMini header | unused | PASS |
| 17 | C3 GPIO0/GPIO1 (I2C) are not strapping or USB | P16, P15 | I2C with 4.7k pull-ups | PASS |
| 18 | C3 GPIO20/21 (UART0 default pins) | P7, P8 | used as `LINK_RX_M2`/`LINK_TX_M2` to the isolators | PASS (note: ROM boot log prints on U0TXD, GPIO21 at 115200; U_ISO2 must tolerate it, and CYD tap shows it) |
| 19 | C3 5V pin | P9 | NC | PASS |
| 20 | Boot-safe outputs: `ANA_PWR_EN` default OFF (100k pull-down; MCP23017 powers up as inputs) | U11 GPB7 | OFF at boot | PASS |
| 21 | Unused MCU1 `GPIO22` onboard LED | P15 | NC | PASS |
| 22 | Pin-level conflicts | all nets | no GPIO appears on two nets; the SPI buses are separate (`AFE_SPI_*` on MCU1, `UI_SPI_*` on MCU2) | PASS |

**Violations: 0** on strapping, flash, input-only and USB-JTAG pins.

## 6. Notes for downstream phases

- `build_design.py` `U_ESP` and `U11` carry the Rev 4 net map. Both need a full re-net to match §1 and §3, plus new parts `U_MCU2`, `JP_M1_3V3`, `JP_M2_3V3`, `J_CYD_UART`, and `MOD_CHG` (`BQ25895_Module` replacing `TP4056_Module`).
- The existing `Wemos_Lolin32_Lite` symbol and footprint need no pin changes. Only the symbol pin names for 8 and 9 (`DAC1/25`, `DAC2/26`) differ cosmetically from the Rev 5 names.
- A new symbol and footprint are needed for the C3 SuperMini: 2 × 8 pads, 15.24 mm between rows, 18.0 × 22.52 mm body, with USB-C and ANTENNA keep-out silk.
- Replace `TP4056_Module` / `Socket_TP4056_Charger` with `BQ25895_Module` / `Socket_BQ25895_Charger` (2 × 1x06 2.54 mm female headers at 15.24 mm row spacing, 20.0 × 24.0 mm body).

## 7. Charger module: BQ25895 (`MOD_CHG` — replaces TP4056)

Per architecture directive #5, the TP4056 linear charger (`MOD_CHG`) and discrete AN1149 load-sharing network are removed and replaced by a socketed **TI BQ25895** 1.5 MHz synchronous switch-mode buck charger and Narrow-VDC (NVDC) power-path module (`0x6A` fixed 7-bit I2C address on the MCU2 UI I2C bus, no conflict with `U11` MCP23017 at `0x20`).

### Commercial Module Survey & Socketable Geometry

1. **EiCut TI BQ25895 Mini DB (SKU: `E0784`) / AliExpress "CJMCU-25895 / BQ25895 Mini Evaluation Breakout":**
   - **Form Factor:** Dual-row DIP-12 style breakout module (**2 × 1x06 headers on a 2.54 mm / 0.1" pitch grid**, **15.24 mm / 0.600" row spacing**, board outline **20.0 mm × 24.0 mm**).
   - **Onboard Passives Included on Module:** BQ25895RTWR (WQFN-24 4×4 mm with thermal via array), 1.0 µH / 5A shielded power inductor (`SW`), 47 nF bootstrap cap (`BTST`), 4.7 µF `REGN` LDO bypass cap, 10 µF `PMID` decoupling + 10 µF `VBUS` + 22 µF `SYS` + 22 µF `BAT` MLCCs, and 130 Ω `RILIM` hardware current-limit resistor.
   - **Secondary commercial option:** **ClimateGuard `CG_Charger_BQ` (`CG_bq`)** (Tindie / GitHub `climateguard/CG_charger_BQ`, 19.0 × 22.0 mm, 2.54 mm edge headers). Because `CG_bq` integrates an auxiliary 5V boost / 3.3V LDO wrapper rather than exposing raw `SYS` and `STAT` directly, the **12-pin BQ25895 Mini DB (`E0784` / 2×6 @ 15.24 mm)** is selected as the canonical socketed footprint (`Socket_BQ25895_Charger`) on two standard 1x06 2.54 mm THT female pin sockets.

### `MOD_CHG` (`BQ25895_Module`) 12-Pin THT Socket Pinout (2×6 @ 2.54 mm pitch, 15.24 mm row spacing)

*Left Header (Pins 1–6, Top to Bottom — Power & Battery Interface, 1.05 mm drill / 1.9 mm pad for 3A rating):*

| Pin | Silk | BQ25895 IC Pin(s) | Net | Dir | Circuit Connection & Notes |
|---|---|---|---|---|---|
| 1 | `VBUS` | Pin 1 (`VBUS`) | `VBUS_CHG` | pwr_in | 3.9V–14V input from USB-C receptacle (with 5.1k CC1/CC2 pull-downs for 5V/3A, or 9V/12V HVDCP) via `F_USB` 3A PTC + `D_TVS_USB` SMBJ15A |
| 2 | `PMID` | Pin 23 (`PMID`) | `PMID_BYP` | pwr | Internal RBFET / boost node (decoupled onboard with 10 µF MLCC; external 10 µF ceramic to `DGND` optional, OTG disabled) |
| 3 | `GND` | Pins 17, 18, 21 (`PGND/GND`) | `DGND` | pwr | High-current power ground (paired with Pin 4 for low-impedance return) |
| 4 | `SYS` | Pins 15, 16 (`SYS`) | `V_BUS_SYS` | pwr_out | NVDC regulated system output (`VSYS_MIN` = 3.5V default; tracks `VBAT` up to 4.192V) feeding `U_LDO_DIG`, `Q_ANA_SW`, and analog boost Pi-filter |
| 5 | `BAT` | Pins 13, 14 (`BAT`) | `VBAT_SW` | pwr_io | Connects to 1S Li-Ion/Li-Po positive terminal via `SW_PWR` physical hard-disconnect switch (`0.0 µA` off-state drain) |
| 6 | `TS` | Pin 11 (`TS`) | `BAT_NTC` | in | JEITA battery pack 10k NTC thermistor divider from onboard `REGN` (or onboard 10k/10k 50% `REGN` bias network when using 2-wire pack) |

*Right Header (Pins 7–12, Top to Bottom — MCU2 I2C, Status & Control Interface):*

| Pin | Silk | BQ25895 IC Pin(s) | Net | Dir | Circuit Connection & Notes |
|---|---|---|---|---|---|
| 7 | `SCL` | Pin 4 (`SCL`) | `UI_I2C_SCL` | in | Connected to `U_MCU2` `GPIO1` (Pin 15) with 4.7k pull-up to `+3V3_DIG` (I2C 7-bit address `0x6A`) |
| 8 | `SDA` | Pin 3 (`SDA`) | `UI_I2C_SDA` | io | Connected to `U_MCU2` `GPIO0` (Pin 16) with 4.7k pull-up to `+3V3_DIG` |
| 9 | `INT` | Pin 7 (`INT`) | `NC` (or `CHG_INT`) | out (OD) | Open-drain 256 µs fault/status pulse; left `NC` on PCB — `U_MCU2` polls `REG0B..REG14` over `UI_I2C` |
| 10 | `STAT` | Pin 5 (`STAT`) | `CHG_STAT_N` | out (OD) | Open-drain status output: LOW during charging, HIGH when complete/disabled, 1 Hz blink on fault. Drives top-panel amber `LED_CHG` via `R_LCHG = 1.5k` from `+3V3_DIG` |
| 11 | `CE_N` | Pin 6 (`/CE`) | `DGND` | in | Active-LOW Charge Enable: hard-tied to `DGND` (charging enabled in hardware; software controls `CHG_CONFIG` bit in `REG03[4]`) |
| 12 | `OTG` | Pin 7/24 (`OTG`) | `DGND` | in | Boost/OTG Enable: **MUST be hard-tied to `DGND`** (never left floating, preventing accidental 5V boost trigger on `PMID/VBUS`) |

### MCU 2 (`U_MCU2`) I2C & Register Configuration Summary for BQ25895

- **I2C Bus Assignment:** `UI_I2C_SDA` (`GPIO0`) / `UI_I2C_SCL` (`GPIO1`) on `U_MCU2`. Slave devices on `UI_I2C`:
  - `0x20`: `U11` (`MCP23017-E/SP` front-panel GPIO expander)
  - `0x6A`: `MOD_CHG` (`BQ25895` switching charger & NVDC power-path manager; write `0xD4`, read `0xD5`)
- **Mandatory Boot Register Initialization (`U_MCU2` firmware):**
  1. `REG06` (`VREG[7:2]`): Program `VREG = 4.192 V` (`3.840V + 22 × 16mV = 4.192V`, code `010110b`) so cell voltage strictly satisfies `≤ 4.20V` (overriding the 4.208V power-on default).
  2. `REG00` (`EN_HIZ[7]`, `EN_ILIM[6]`, `IINLIM[5:0]`): Program `IINLIM = 3.00 A` (code `111010b`, `100mA + 58 × 50mA`). During ultra-low-leakage Germanium `I_CEO` (<1 µA) measurements while USB is plugged in, `U_MCU2` can optionally assert `EN_HIZ = 1` (or pause charging via `REG03[4] = 0`) for the 50 ms ADC integration window to eliminate 1.5 MHz buck switching ripple completely.
  3. `REG03` (`WD_RST[6]`, `OTG_CONFIG[5] = 0`, `CHG_CONFIG[4] = 1`, `SYS_MIN[3:1] = 101b` for 3.5V): Disable OTG boost in software as well as via Pin 12 (`OTG = DGND`).
  4. `REG04` (`ICHG[6:0]`): Default `2.048 A` (`32 × 64mA`), clamped by firmware to `≤ 1C` of the user-configured cell capacity.
  5. `REG05` (`IPRECHG[7:4] = 128 mA`, `ITERM[3:0] = 128 mA`).
  6. `REG07` (`WATCHDOG[5:4] = 00b` to disable I2C watchdog reset of `VREG`, or periodic `WD_RST` kick from `U_MCU2` every 10 s).
  7. `REG02` (`CONV_START[7]`, `CONV_RATE[6] = 1`): Enable 1 Hz internal 7-bit ADC telemetry for live `VBAT` (`REG0E`), `VSYS` (`REG0F`), `VBUS` (`REG11`), and `ICHG` (`REG12`) readout on `MOD_TFT2`.

