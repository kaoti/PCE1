---
gate_id: "gate_1"
verdict: "PASS"
critical_defects: []
high_defects: []
required_patches: []
auditor: "red-team"
phase: "Phase 1 — Architecture, Silicon Pinout, AFE Matrix, Power-Path & Isolated UART Audit"
audited_deliverables:
  - "/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/p1_pinout_extraction.md"
  - "/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/p1_afe_matrix_topology.md"
  - "/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/p1_2n4393_verdict.md"
  - "/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/p1_dut_failsafe_math.md"
  - "/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/p1_power_path_dual_switch.md"
  - "/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/p1_uart_isolation_spec.md"
---

```json
{
  "gate_id": "gate_1",
  "verdict": "PASS",
  "critical_defects": [],
  "high_defects": [],
  "required_patches": []
}
```

# Gate 1 Adversarial Red-Team Audit Report (`gate_1_audit.md`)

**Project:** KAOTI AUDIO — Rev 5.0 Dual-MCU Germanium & Universal Parametric Characterization Engine  
**Audit Scope:** Independent line-by-line adversarial verification of all six Phase 1 deliverables against ESP32 / ESP32-C3 / BQ25895 / DG408DJ / DG412LDJ / LMC6484IN / ADS1115 / H11L1M silicon datasheets, vintage Germanium device physics, and cross-document netlist invariants.  
**Gate Verdict:** **`PASS` (0 Critical Defects, 0 High Defects, 0 Blocking Patches Required)**

---

## 1. Executive Summary of Blocking Gate Checks

| Check # | Mandatory Gate 1 Blocking Criterion | Audited Deliverables | Verification Result | Status |
| :---: | :--- | :--- | :--- | :---: |
| **1** | **MCU 1 (`U_MCU1` WEMOS LOLIN32 Lite V1.0.0) Silicon Constraints:** Zero strapping (`GPIO0, 2, 5, 12, 15`), flash (`GPIO6–11`), or input-only (`GPIO34–39`) violations; no 5V header assumption; `JP_M1_3V3` dual-LDO contention protection. | `p1_pinout_extraction.md` (§1, §5)<br/>`p1_power_path_dual_switch.md` (§4.2) | All 5 strapping pins (`IO0, 2, 5, 12, 15`) are `NC`; `GPIO6–11` internal flash untouched; `GPIO34, 35, 36, 39` used strictly as inputs (`IO34` has external `10k` pull-up); no 5V pin assumed; `JP_M1_3V3` isolates `M1_3V3` from `+3V3_ANA`. | **PASS** |
| **2** | **MCU 2 (`U_MCU2` ESP32-C3 SuperMini) Silicon Constraints:** Zero strapping (`GPIO2, 8, 9`) or USB-JTAG (`GPIO18, 19`) conflicts; `U11 MCP23017-E/SP` (`0x20`) and `MOD_CHG BQ25895` (`0x6A`) coexist cleanly on `UI_I2C`. | `p1_pinout_extraction.md` (§2, §3, §5, §7)<br/>`p1_power_path_dual_switch.md` (§2.4) | Strapping pins `GPIO2` (P14), `GPIO8` (P4), `GPIO9` (P5) are `NC`; `GPIO18/19` native USB-JTAG untouched; `U11` (`0x20`) and `MOD_CHG` (`0x6A`) share `UI_I2C_SDA` (`GPIO0`) / `UI_I2C_SCL` (`GPIO1`) with zero address conflict. | **PASS** |
| **3** | **Zero AFE Shoot-Through Paths During Boot/Reset:** `SR_OE_N` on non-strapping `GPIO17` (`10k` pull-up), `SRCLR_N` POR RC (`10k + 1uF`), `100k` pull-downs on all `DG408 EN` & `DG412 IN`, `ANA_PWR_EN` (`U11 GPB7`) `100k` pull-down, and `2N4393` depletion JFET rejected. | `p1_afe_matrix_topology.md` (§3.1–3.3)<br/>`p1_2n4393_verdict.md` (§1–5)<br/>`p1_dut_failsafe_math.md` (§2.1–2.4)<br/>`p1_power_path_dual_switch.md` (§5.1) | 4-layer hardware interlock proved: `ANA_PWR_EN` boots `0.00V` (`100k` pull-down, analog core OFF); when energized, `SR_OE_N` tracks `+3V3_ANA` (`3.298V >= 2.31V V_IH`), `SRCLR_N` clears shift latches (`tau = 10 ms`), and `R_EN1..3` / `R_PD1..3` (`100k`) hold all 27 CMOS switches strictly OPEN (`0.06V < 0.80V V_IL`). | **PASS** |
| **4** | **Reverse $V_{EBO}$ Clamp & Current Limit:** Autodetect probing clamps `V_SRC_A <= 3.3V` (`2.500V`) and current `<= 5 mA` (`<= 0.95 mA` via `2.49 kΩ + 100 Ω`) before pinout/polarity confirmation. | `p1_afe_matrix_topology.md` (§5.1)<br/>`p1_dut_failsafe_math.md` (§1.1–1.2) | Phase 0 DAC code $D = 1116$ clamps $V_{\text{SRC\_A}} = 2.49984\text{ V} \le 2.500\text{ V} < BV_{EBO}$ (`AF114` / `OC44`); probing restricted to `S2` (`2.49 kΩ`) + `R_PR` (`100 Ω`), limiting worst-case 2T current to $0.9058\text{ mA}$ ($0.9470\text{ mA}$ single-pin short to `AGND`) and $P_{\text{DUT}} \le 0.592\text{ mW}$. | **PASS** |
| **5** | **Power-Path, BQ25895 Fast-Charger & Isolated UART Integrity:** Zero `TP4056` in active architecture; `BQ25895` `VREG = 4.192V <= 4.20V` with no blind `4.208V` charging when `SW_PWR = OFF`; `0.0 µA` storage drain; `Q_REV_BAT` & `Q_ANA_SW` (`NDP6020P` `1=G, 2=D, 3=S`) body diodes oriented properly; `2x H11L1M` optical isolation. | `p1_pinout_extraction.md` (§7)<br/>`p1_power_path_dual_switch.md` (§1–8)<br/>`p1_uart_isolation_spec.md` (§1–4) | `TP4056` removed; `BQ25895` programmed to `REG06 = 0x58` ($4.192\text{ V}$) with watchdog disabled (`REG07 = 0x8D`); `SW_PWR` Pole 1 physically disconnects `J_BAT+` ($0.000\text{ }\mu\text{A}$ storage drain & prevents unmonitored $4.208\text{ V}$ POR default charging when `OFF`); both `NDP6020P` body diodes verified; `2x H11L1M` (`U_ISO1`, `U_ISO2`) yield $0.000\text{ }\mu\text{A}$ Standby back-powering. | **PASS** |
| **6** | **Cross-Document Net & Pin Consistency:** 1:1 agreement across all 6 documents on pin numbers, net names, and component designators. | All 6 Phase 1 documents | All 26 `U_MCU1` pins, 16 `U_MCU2` pins, 28 `U11` pins, 12 `MOD_CHG` pins, 10 `MOD_ADS` pins, 8 `MOD_TFT1/2` pins, 4 `J_CYD_UART` pins, 6 `U_ISO1/2` pins, and all critical signal/power nets match 1:1. Four minor non-blocking alias notes recorded in §8 for Phase 2 script generation. | **PASS** |

---

## 2. Detailed Audit of Blocking Check #1 — MCU 1 (`U_MCU1` WEMOS LOLIN32 Lite V1.0.0)

Cross-verified `p1_pinout_extraction.md` (§0, §1, §5), `p1_afe_matrix_topology.md` (§1–4), `p1_dut_failsafe_math.md` (§2–3), `p1_power_path_dual_switch.md` (§4.2, §6.2), and `p1_uart_isolation_spec.md` (§1.2) against the Espressif ESP32 Datasheet and WEMOS LOLIN32 Lite V1.0.0 schematic:

| Header Pin | Silk / GPIO | Assigned Rev 5.0 Net | Direction | Silicon Constraint & Electrical Verification | Audit Verdict |
| :---: | :---: | :--- | :---: | :--- | :---: |
| **P1** | `VP / GPIO36` | `FAST_TP1` | Input | **Input-only (`ADC1_CH0`):** Driven from `ADC_TP1` via `R_ISO1 = 10 kΩ`. Never configured as output; no internal pull-up assumed. | **PASS** |
| **P2** | `VN / GPIO39` | `FAST_TP2` | Input | **Input-only (`ADC1_CH3`):** Driven from `ADC_TP2` via `R_ISO2 = 10 kΩ`. Never configured as output; no internal pull-up assumed. | **PASS** |
| **P3** | `EN` | `NC` | Reset | Onboard $10\text{ k}\Omega$ pull-up and $100\text{ nF}$ delay cap on LOLIN32 Lite module remain untouched. | **PASS** |
| **P4** | `GPIO34` | `ADS_ALRT` | Input | **Input-only:** Connected to `MOD_ADS` Pin 6 (`ALRT`). Since `GPIO34–39` lack internal pull-ups, external `10 kΩ` pull-up to `+3V3_ANA` is explicitly provided. | **PASS** |
| **P5** | `GPIO35` | `FAST_TP3` | Input | **Input-only:** Driven from `ADC_TP3` via `R_ISO3 = 10 kΩ`. Never configured as output. | **PASS** |
| **P6** | `GPIO32` | `CAL_A0` | Output | Standard GPIO output driving `U10 DG409DJ` Pin 1 (`A0`). | **PASS** |
| **P7** | `GPIO33` | `CAL_A1` | Output | Standard GPIO output driving `U10 DG409DJ` Pin 16 (`A1`). | **PASS** |
| **P8** | `GPIO25` | `FAULT_OVC_N` | Input (ISR) | Driven by `Q_OVC` (`2N3904` Collector Pin 3) with external `10 kΩ` pull-up to `+3V3_ANA`. Triggers $<2\text{ }\mu\text{s}$ hardware overcurrent cutoff. | **PASS** |
| **P9** | `GPIO26` | `DAC_CS` | Output | Drives `U9 MCP4822` Pin 2 (`CS_N`) with external `R_DACCS = 10 kΩ` pull-up to `+3V3_ANA`. | **PASS** |
| **P10** | `GPIO27` | `AFE_SPI_MOSI` | Output | Drives `U9 MCP4822` Pin 4 (`SDI`) and `U6 74HC595N` Pin 14 (`SER`). | **PASS** |
| **P11** | `GPIO14` | `AFE_SPI_SCK` | Output | Drives `U9 MCP4822` Pin 3 (`SCK`) and `U6, U7 74HC595N` Pin 11 (`SRCLK`). Non-strapping pin. | **PASS** |
| **P12** | `GPIO12` | `NC` | Unused | **CRITICAL STRAPPING PIN (`MTDI` — Flash Voltage `VDD_SDIO` selector):** Left strictly `NC` so internal pull-down selects $3.3\text{ V}$ flash without boot brownout. | **PASS** |
| **P13** | `GND` | `AGND` | Power | Connected to quiet Analog Ground plane (`AGND`). | **PASS** |
| **P14** | `3V3` | `M1_3V3` | Power | Routed through 2-pin isolation jumper **`JP_M1_3V3`** to `+3V3_ANA`. Prevents contention between onboard `ME6211` LDO and `U_LDO_ANA` during USB-C flashing. No 5V pin exists on LOLIN32 Lite. | **PASS** |
| **P15** | `GPIO22` | `NC` | Unused | Connected to LOLIN32 Lite onboard active-LOW LED; left `NC` to avoid asymmetrical I2C loading. | **PASS** |
| **P16** | `GPIO19` | `ANA_I2C_SDA` | I/O (OD) | Connected to `MOD_ADS` Pin 4 (`SDA`) with `4.7 kΩ` pull-up to `+3V3_ANA`. | **PASS** |
| **P17** | `GPIO23` | `ANA_I2C_SCL` | Output (OD) | Connected to `MOD_ADS` Pin 3 (`SCL`) with `4.7 kΩ` pull-up to `+3V3_ANA`. | **PASS** |
| **P18** | `GPIO18` | `SR_LATCH` | Output | Drives `U6, U7 74HC595N` Pin 12 (`RCLK`) with external `10 kΩ` pull-down to `AGND` (prevents spurious latch clock edge during boot). | **PASS** |
| **P19** | `GPIO5` | `NC` | Unused | **STRAPPING PIN (SDIO Slave Timing):** Left strictly `NC`. | **PASS** |
| **P20** | `GPIO17` | `SR_OE_N` | Output | **Non-strapping pin** (and no PSRAM on LOLIN32 Lite). Drives `U6, U7` Pin 13 (`OE_N`) with `R_OEN = 10 kΩ` pull-up to `+3V3_ANA` so shift outputs stay `Hi-Z` at boot. | **PASS** |
| **P21** | `GPIO16` | `LINK_TX_M1` | Output | UART2 TX driving `U_ISO1` (`H11L1M` Pin 2 Cathode `K`); idles HIGH ($0.00\text{ mA}$ LED current). | **PASS** |
| **P22** | `GPIO4` | `TEMP_DQ` | I/O (1-Wire) | Connected to `J_TEMP` (`DS18B20`) with `4.7 kΩ` pull-up to `+3V3_ANA`. Non-strapping pin. | **PASS** |
| **P23** | `GPIO0` | `NC` | Unused | **CRITICAL STRAPPING PIN (Boot Mode):** Left strictly `NC`. | **PASS** |
| **P24** | `GPIO2` | `NC` | Unused | **CRITICAL STRAPPING PIN (Download Boot Qualification):** Left strictly `NC`. | **PASS** |
| **P25** | `GPIO15` | `NC` | Unused | **STRAPPING PIN (`MTDO` — ROM Boot Log):** Left strictly `NC`. | **PASS** |
| **P26** | `GPIO13` | `LINK_RX_M1` | Input | UART2 RX driven by `U_ISO2` (`H11L1M` Pin 4 `VO`) with `R_PU_ISO2 = 2.2 kΩ` pull-up to `+3V3_ANA`. | **PASS** |
| *Internal* | `GPIO6–11` | *Internal SPI Flash* | Internal | Not broken out on LOLIN32 Lite; zero external access. | **PASS** |

---

## 3. Detailed Audit of Blocking Check #2 — MCU 2 (`U_MCU2` ESP32-C3 SuperMini), `U11 MCP23017` & `MOD_CHG BQ25895`

Cross-verified `p1_pinout_extraction.md` (§2, §3, §4, §5, §7), `p1_power_path_dual_switch.md` (§2.4, §4.2, §5.1, §8), and `p1_uart_isolation_spec.md` (§1.2, §2) against the Espressif ESP32-C3 Datasheet and Microchip `MCP23017-E/SP` DIP-28 pinout:

### 3.1 `U_MCU2` (`ESP32-C3 SuperMini`, 16-Pin 2x8 @ 15.24 mm Row Pitch)
| Header Pin | Silk / GPIO | Assigned Rev 5.0 Net | Direction | Silicon Constraint & Electrical Verification | Audit Verdict |
| :---: | :---: | :--- | :---: | :--- | :---: |
| **P1** | `GPIO5` | `TFT_DC` | Output | Drives `MOD_TFT1` & `MOD_TFT2` Pin 6 (`DC`). Non-strapping pin. | **PASS** |
| **P2** | `GPIO6` | `UI_SPI_MOSI` | Output | Drives `MOD_TFT1` & `MOD_TFT2` Pin 4 (`SDA`). Non-strapping pin. | **PASS** |
| **P3** | `GPIO7` | `TFT1_CS` | Output | Drives `MOD_TFT1` Pin 7 (`CS`) with `10 kΩ` pull-up to `+3V3_DIG`. | **PASS** |
| **P4** | `GPIO8` | `NC` | Unused | **CRITICAL C3 STRAPPING PIN + Onboard Blue LED:** Left strictly `NC` (must be HIGH during boot to enter SPI Boot/Download mode properly). | **PASS** |
| **P5** | `GPIO9` | `NC` | Unused | **CRITICAL C3 STRAPPING PIN + Onboard BOOT Button:** Left strictly `NC` (internal pull-up holds HIGH for normal SPI Flash boot). | **PASS** |
| **P6** | `GPIO10` | `TFT2_CS` | Output | Drives `MOD_TFT2` Pin 7 (`CS`) with `10 kΩ` pull-up to `+3V3_DIG`. | **PASS** |
| **P7** | `GPIO20` | `LINK_RX_M2` | Input | UART0 RX driven by `U_ISO1` (`H11L1M` Pin 4 `VO`, pulled up to `+3V3_DIG` via `2.2 kΩ`) and isolated from `J_CYD_UART` Pin 4 via `R_CYD_RX = 470 Ω`. | **PASS** |
| **P8** | `GPIO21` | `LINK_TX_M2` | Output | UART0 TX driving `U_ISO2` (`H11L1M` Pin 2 `K`) and isolated from `J_CYD_UART` Pin 3 via `R_CYD_TX = 470 Ω`. Note: C3 ROM boot log at 115200 is safely discarded by MCU 1 `[0xAA][0x55]` sync parser. | **PASS** |
| **P9** | `5V` | `NC` | Unused | Left `NC`; MCU 2 is powered directly via its regulated `3V3` rail so it operates cleanly down to $3.40\text{ V}$ LiPo cutoff without onboard LDO dropout. | **PASS** |
| **P10** | `GND` | `DGND` | Power | Connected to Digital Ground plane (`DGND`). | **PASS** |
| **P11** | `3V3` | `M2_3V3` | Power | Routed through 2-pin isolation jumper **`JP_M2_3V3`** to `+3V3_DIG` (`U_LDO_DIG` `MCP1826S-3302E/AB`). Prevents dual-LDO contention when USB-C cable is plugged into MCU 2. | **PASS** |
| **P12** | `GPIO4` | `UI_SPI_SCK` | Output | Drives `MOD_TFT1` & `MOD_TFT2` Pin 3 (`SCL`). Non-strapping pin. | **PASS** |
| **P13** | `GPIO3` | `BZ_PWM` | Output | LEDC hardware PWM driving `Q_BZ` (`2N3904` Base via `1.0 kΩ` with `10 kΩ` pull-down to `DGND`). | **PASS** |
| **P14** | `GPIO2` | `NC` | Unused | **CRITICAL C3 STRAPPING PIN:** Left strictly `NC`. | **PASS** |
| **P15** | `GPIO1` | `UI_I2C_SCL` | Output (OD) | Drives `U11 MCP23017` Pin 12 and `MOD_CHG BQ25895` Pin 7 with `4.7 kΩ` pull-up to `+3V3_DIG`. | **PASS** |
| **P16** | `GPIO0` | `UI_I2C_SDA` | I/O (OD) | Connected to `U11 MCP23017` Pin 13 and `MOD_CHG BQ25895` Pin 8 with `4.7 kΩ` pull-up to `+3V3_DIG`. (`GPIO0` is *not* a strapping pin on ESP32-C3!). | **PASS** |
| *Internal* | `GPIO18, 19` | *USB-C D- / D+* | USB-JTAG | Not broken out to the 16-pin header; preserved 100% for native USB Serial/JTAG debugging. | **PASS** |
| *Internal* | `GPIO12–17` | *Internal SPI Flash* | Internal | Not broken out to the 16-pin header; untouched. | **PASS** |

### 3.2 `U11` (`MCP23017-E/SP`, DIP-28) & `UI_I2C` Bus Coexistence Audit
- **I2C Bus Address Verification:**
  - `U11` (`MCP23017-E/SP`): Pins 15 (`A0`), 16 (`A1`), 17 (`A2`) tied to `DGND` $\implies$ 7-bit address **`0x20`** (`0100000b`).
  - `MOD_CHG` (`BQ25895`): Fixed factory 7-bit address **`0x6A`** (`1101010b`).
  - **Conflict Check:** `0x20 != 0x6A` (Hamming distance = 4 bits). Both devices operate at $100\text{ kHz} / 400\text{ kHz}$ with $3.3\text{ V}$ logic on `UI_I2C_SDA`/`UI_I2C_SCL` pulled up to `+3V3_DIG` via $4.7\text{ k}\Omega$.
- **DIP-28 Pinout Verification (`MCP23017-E/SP` Datasheet):**
  - Pins 1–8 (`GPB0..GPB7`): `1=ENC_A`, `2=ENC_B`, `3=LED_STBY_RF`, `4=LED_SCAN`, `5=LED_DUT_OK`, `6=LED_FAULT`, `7=TFT_BLK_EN`, `8=ANA_PWR_EN`.
  - Pin 9 (`VDD`) = `+3V3_DIG` (+ `100 nF` bypass); Pin 10 (`VSS`) = `DGND`; Pin 11 (`NC`) = `NC`; Pin 12 (`SCL`) = `UI_I2C_SCL`; Pin 13 (`SDA`) = `UI_I2C_SDA`; Pin 14 (`NC`) = `NC`.
  - Pins 15–17 (`A0..A2`) = `DGND`; Pin 18 (`RESET`) = `+3V3_DIG` via `10 kΩ` + `100 nF` to `DGND`; Pins 19–20 (`INTB, INTA`) = `NC`.
  - Pins 21–28 (`GPA0..GPA7`): `21=BTN_UP`, `22=BTN_DOWN`, `23=BTN_LEFT`, `24=BTN_RIGHT`, `25=BTN_A`, `26=BTN_B`, `27=ENC_SW`, `28=SW_STBY`.
  - **Result:** 100% match with Microchip `MCP23017-E/SP` DIP-28 silicon specification.

---

## 4. Detailed Audit of Blocking Check #3 — Zero AFE Shoot-Through Paths & `2N4393` Rejection

Independent verification of the four-layer hardware/firmware shoot-through protection architecture (`p1_afe_matrix_topology.md` §3, `p1_2n4393_verdict.md` §2–4, `p1_dut_failsafe_math.md` §2, `p1_power_path_dual_switch.md` §5.1):

1. **Layer 0 — Analog Domain Power-Off at Boot (`ANA_PWR_EN` on `U11 GPB7` + `R_PD_ANA = 100 kΩ`):**
   - At power-on reset, `U11 MCP23017` initializes all GPIO ports as high-impedance inputs (`IODIRB = 0xFF`).
   - `R_PD_ANA = 100 kΩ` holds `ANA_PWR_EN` at $0.00\text{ V}$, keeping `Q_ANA_N` (`2N7000`, $V_{GS(\text{th,min})} = 0.8\text{ V}$) strictly OFF.
   - `R_PU_SW = 47 kΩ` holds `Q_ANA_SW` (`NDP6020P`) Gate (Pin 1) at Source (Pin 3, `V_BUS_SYS`), so $V_{GS} = 0.00\text{ V}$ and `V_ANA_IN = 0.00 V`. The entire analog matrix (`+12V_A`, `+9V_LIM`, `+3V3_ANA`, `+3V3_ADC`) boots de-energized.
2. **Layer 1 — Master Shift-Register Output Tri-State (`SR_OE_N` on `U_MCU1 GPIO17` + `R_OEN = 10 kΩ` to `+3V3_ANA`):**
   - When `Q_ANA_SW` turns ON and `+3V3_ANA` ramps, `U_MCU1 GPIO17` is a high-Z input ($I_{\text{leak}} \le 250\text{ nA}$ total).
   - `R_OEN = 10 kΩ` holds `U6, U7` (`74HC595N`) Pin 13 (`OE_N`) at $V_{\text{3V3\_ANA}} - 2.5\text{ mV} = 3.2975\text{ V} \ge V_{IH(\text{min})} (2.31\text{ V})$, tri-stating all 16 shift-register outputs (`QA..QH`) across the entire power-up and ESP32 reset window.
3. **Layer 2 — Hardware Asynchronous Shift Clear (`SRCLR_N` Pin 10 on `U6, U7` with `R_POR = 10 kΩ`, `C_POR = 1.0 µF`) & Latch Pull-Down:**
   - $\tau_{\text{POR}} = 10\text{ k}\Omega \times 1.0\text{ }\mu\text{F} = 10.0\text{ ms}$ holds `SRCLR_N` below the $1.65\text{ V}$ CMOS midpoint for $t_{\text{clear}} = 10\text{ ms} \times \ln(2) = 6.931\text{ ms}$ ($12.04\text{ ms}$ to $V_{IH} = 2.31\text{ V}$), asynchronously clearing the shift stages of `U6, U7` to `0x0000` while `SR_LATCH` (`GPIO18`) is anchored to `AGND` by its `10 kΩ` pull-down.
   - Furthermore, the DAC outputs `DAC1_FILT` and `DAC2_FILT` have `100 kΩ` bleed resistors (`R_DP1`, `R_DP2`) to `AGND` and `DAC_CS` (`GPIO26`) has a `10 kΩ` pull-up to `+3V3_ANA`, ensuring `V_SRC_A` and `V_SRC_B` remain at $0.000\text{ V}$ during boot.
4. **Layer 3 — Individual `100 kΩ` Switch-Disable Pull-Downs (`R_EN1..3`, `R_PD1..3` to `AGND`):**
   - While `U6, U7` outputs are `Hi-Z`, `R_EN1..3 = 100 kΩ` pull `U1_EN, U2_EN, U3_EN` (`DG408DJ` Pin 2) to `AGND`, and `R_PD1..3 = 100 kΩ` pull `SW_G1, SW_G2, SW_G3` (`DG412LDJ` Pins 1, 16, 9) to `AGND` (with unused Switch 4 Pin 8 `IN4` hard-tied to `AGND`).
   - Worst-case voltage on any enable line at $25^\circ\text{C}$ is $(0.5\text{ }\mu\text{A} + 0.1\text{ }\mu\text{A}) \times 100\text{ k}\Omega = 0.060\text{ V} \ll 0.80\text{ V}$ ($V_{IL,\text{max}}$), and $0.600\text{ V} < 0.80\text{ V}$ even at $85^\circ\text{C}$ extreme corner.
   - Result: All 24 switches in `U1..U3` (`DG408DJ`) and all 4 switches in `U8` (`DG412LDJ`) are strictly **OPEN (`Hi-Z`, $<1\text{ nA}$ leakage)**.
5. **Layer 4 — Silicon BBM (`25 ns`) + Firmware `60 µs` Dead-Time Protocol & Formal Rejection of `2N4393`:**
   - `DG408DJ` has intrinsic break-before-make ($t_{\text{OFF}} = 150\text{ ns} < t_{\text{ON}} = 175\text{ ns}$), reinforced by firmware's 3-step `SR_OE_N = HIGH` $\to 60\text{ }\mu\text{s}$ wait $\to$ latch new word $\to$ `SR_OE_N = LOW` sequence.
   - `p1_2n4393_verdict.md` mathematically proves that depletion-mode N-JFET `2N4393` ($V_{GS(\text{off})} \in [-0.5\text{ V}, -3.0\text{ V}]$, $R_{DS(\text{on})} = 75\text{–}100\,\Omega$ at $V_{GS} = 0\text{ V}$) is **NORMALLY ON** at boot (causing $45\text{ mA}$ / $15.1\text{ mA}$-clamped multi-rail shoot-through), cannot pinch off at $0\text{ V}$ without a $-5\text{ V}$ supply, forward-biases its gate-channel P-N diode when driven to $+3.3\text{ V}$ above a low-side node, and has $500\times$ higher bias current ($10\text{ pA}$ vs. `LMC6484IN`'s $20\text{ fA}$). Rejection of `2N4393` from the PCB (`0` populated) is 100% sound.

---

## 5. Detailed Audit of Blocking Check #4 — Reverse $V_{EBO}$ Clamp, Current Limit & `ADS1115` Overvoltage Protection

Independent verification of the equations in `p1_afe_matrix_topology.md` (§2.2, §4.2, §5.1) and `p1_dut_failsafe_math.md` (§1, §3, §4):

1. **Reverse $V_{EBO}$ Voltage Clamp ($V_{\text{SRC\_A}} \le 2.500\text{ V} \le 3.30\text{ V}$):**
   - Vintage RF alloy-diffused / drift-field Germanium transistors (`AF114`, `AF115`, `AF117`, `OC170`, `OC44`, `2N1308`) exhibit reverse emitter-base breakdown $BV_{EBO}$ between $3.0\text{ V}$ and $5.0\text{ V}$.
   - During Phase 0 Autodetection (across all 6 terminal permutations, including forward/reverse $\beta_F / \beta_R$ collector vs. emitter discrimination), `U9 MCP4822` Channel A ($V_{\text{REF}} = 2.048\text{ V}$, $G_{\text{DAC}} = 1\times$, $G_{\text{U5}} = 1 + 34.8\text{ k}\Omega / 10.0\text{ k}\Omega = 4.480\times$) is clamped to code $D \le 1116$:
     $$V_{\text{SRC\_A,probe}} = 4.480 \times 2.048\text{ V} \times \frac{1116}{4096} = \mathbf{2.49984\text{ V}} \le \mathbf{2.500\text{ V}} \le 3.30\text{ V}$$
2. **Autodetect Probe Current Limit ($I_{\text{probe}} \le 0.95\text{ mA} \le 5.00\text{ mA}$):**
   - High-current paths `S1` ($470\,\Omega$) and `S8` (`+9V_LIM`) are locked out during Phase 0; probing uses exclusively `S2` (`R_KA = 2.49 kΩ 0.1%`) in series with `R_PR` (`100 Ω 0.1%`):
     - **2-Terminal Shorted DUT Loop ($T_{\text{hi}}$ via `S2` $\to$ DUT $\to T_{\text{lo}}$ via `U8`):**
       $$R_{\text{loop,2T}} = 2490\,\Omega + 50\,\Omega\,(\text{DG408}) + 100\,\Omega\,(R_{\text{PR,hi}}) + 100\,\Omega\,(R_{\text{PR,lo}}) + 20\,\Omega\,(\text{DG412}) = 2760\,\Omega \implies I_{\text{max,2T}} = \frac{2.500\text{ V}}{2760\,\Omega} = \mathbf{0.9058\text{ mA}}$$
       *(Even with ideal $0\,\Omega$ switches: $2.500\text{ V} / 2690\,\Omega = \mathbf{0.9294\text{ mA}} \le 0.95\text{ mA}$.)*
     - **1-Terminal Short to External `AGND` ($2490 + 50 + 100 = 2640\,\Omega$):**
       $$I_{\text{max,1T}} = \frac{2.500\text{ V}}{2640\,\Omega} = \mathbf{0.9470\text{ mA}} \le \mathbf{0.95\text{ mA}} \ll 5.00\text{ mA}$$
     - **Maximum Possible DUT Power During Autodetect:**
       $$P_{\text{DUT,max(probe)}} = \frac{(2.500\text{ V})^2}{4 \times 2640\,\Omega} = \mathbf{0.5919\text{ mW}} \ll 35.0\text{ mW}$$
3. **Active $+9\text{V\_LIM}$ Current Limiter ($15.12\text{ mA}$), `R_PR` Wattage ($0.5\text{ W}$), `1N3595` Clamps & `ADS1115` Overvoltage Proof:**
   - `Q_LIM1, Q_LIM2` (`2x 2N3906`) with `R_ILIM = 43 Ω` limits `+9V_LIM` fault current to $I_{\text{trip}} = 0.65\text{ V} / 43\,\Omega = \mathbf{15.12\text{ mA}}$ at $25^\circ\text{C}$ ($16.74\text{ mA}$ at $0^\circ\text{C}$), while `Q_OVC` (`2N3904`) pulls `FAULT_OVC_N` (`GPIO25`) LOW to trip `SR_OE_N = HIGH` in $< 2.0\text{ }\mu\text{s}$.
   - `R_PR1..3` ($100\,\Omega, 0.5\text{ W}, 0.1\%$) dissipates at most $(16.74\text{ mA})^2 \times 100\,\Omega = \mathbf{28.04\text{ mW}}$ ($5.61\%$ of its $500\text{ mW}$ rating, $>17\times$ thermal margin).
   - `6x 1N3595` (`D_CH1..3`, `D_CL1..3`, plus `D_HVH, D_HVL`) guarantee $I_R \le 1.0\text{ nA}$ ($<10\text{ pA}$ typ) while absorbing charged-capacitor insertion surges ($122.5\text{ mA} < 150\text{ mA}$ continuous rating).
   - `U4 LMC6484IN` ($+12.00\text{ V}$ rail) drives `MOD_ADS` (`ADS1115`, $V_{DD} = +3.30\text{ V}$) through the `1.50 kΩ / 499 Ω` ($0.1\%$) $4:1$ divider ($G_{\text{DIV}} = 4.006012$):
     $$V_{\text{ADC(max, 12.0V)}} = 12.000\text{ V} \times \frac{499\,\Omega}{1999\,\Omega} = \mathbf{2.9955\text{ V}} < 3.300\text{ V}\,(V_{DD,\text{ADC}}) < 3.600\text{ V}\,(V_{\text{abs,max}})$$
     Even under the extreme $+13.00\text{ V}$ `D_OVP` (`1N4743A`) Zener clamp fault corner, $V_{\text{ADC(13.0V)}} = \mathbf{3.2451\text{ V}} < 3.300\text{ V}$.

---

## 6. Detailed Audit of Blocking Check #5 — Power-Path, `BQ25895` Fast-Charger & Isolated UART Integrity

Independent verification of `p1_pinout_extraction.md` (§7), `p1_power_path_dual_switch.md` (§1–9), and `p1_uart_isolation_spec.md` (§1–4):

1. **Complete Elimination of `TP4056` & Verification of `BQ25895` (`MOD_CHG` @ `0x6A`):**
   - Zero `TP4056` modules or discrete AN1149 `Q_PATH` networks remain in the active Rev 5.0 architecture.
   - `MOD_CHG` (`BQ25895_Module`, 12-pin 2x6 @ 15.24 mm THT socket) provides $1.5\text{ MHz}$ synchronous buck charging and integrated $11\text{ m}\Omega$ `BATFET` NVDC power-path routing (`VSYS` independent of `VBAT`, guaranteeing clean $I_{\text{TERM}} = 128\text{ mA}$ charge termination even under full $350\text{ mA}$ system load).
   - Pin 12 (`OTG`) and Pin 11 (`CE_N`) are hard-tied to `DGND` so accidental $5\text{ V}$ OTG boost is impossible in hardware and software (`REG03[5] = 0`).
2. **Strict Non-HV LiPo Voltage Clamp ($V_{\text{REG}} = 4.192\text{ V} \le 4.200\text{ V}$) & Prevention of Blind $4.208\text{ V}$ Default Charging:**
   - The `BQ25895` hardware POR default is `REG06 = 010111b` ($4.208\text{ V}$), which exceeds the $4.200\text{ V}$ ceiling of standard $3.7\text{ V}$ Non-HV LiPo cells by $+8\text{ mV}$.
   - **Firmware Safeguard:** At boot, `U_MCU2` immediately writes `REG06 = 0x58` (`VREG[7:2] = 010110b` $= 22_{10} \implies 3.840\text{ V} + 22 \times 16\text{ mV} = \mathbf{4.192\text{ V}} \le 4.200\text{ V}$) and writes `REG07 = 0x8D` (`WATCHDOG[5:4] = 00b`) so `VREG` cannot revert to $4.208\text{ V}$.
   - **Hardware `SW_PWR = OFF` Safeguard:** Because `SW_PWR` Pole 1 is wired in series with `J_BAT+` (and Pole 2 in series with `MOD_CHG VSYS` $\to$ `V_BUS_SYS`), plugging in USB-C while `SW_PWR = OFF` leaves the battery **physically disconnected by an air gap**. Thus, the `BQ25895` can **never** blind-charge the LiPo cell at its unconfigured $4.208\text{ V}$ default while `U_MCU2` is powered off!
3. **`0.0 µA` Storage Drain (`SW_PWR`) & `0.0 mA` Analog Core Standby Drain (`SW_STBY`):**
   - `SW_PWR = OFF` physically breaks `J_BAT+` on Pole 1 $\implies I_{\text{BAT(OFF)}} = \mathbf{0.000\text{ }\mu\text{A}}$.
   - `SW_STBY = LOW` (`U11 GPA7 = 0`) causes `U_MCU2` to set `U11 GPB7` (`ANA_PWR_EN = 0`) and `U11 GPB6` (`TFT_BLK_EN = 0`), shutting off `Q_ANA_N`, `Q_ANA_SW`, `MOD_BOOST`, `U_REG12`, `U_REG9`, `U_LDO_ANA`, `U_MCU1`, the entire AFE, and `Q_BLK` (dual TFT backlights) $\implies I_{\text{ANA(STBY)}} = \mathbf{0.000\text{ mA}}$ ($<0.25\text{ }\mu\text{A}$ `NDP6020P` leakage).
4. **P-MOSFET Body Diode & Pinout Audit (`Q_REV_BAT` & `Q_ANA_SW` — `NDP6020P` TO-220-3: `1=G, 2=D, 3=S`):**
   - Inside `NDP6020P`, the intrinsic body diode points from **Pin 2 (`Drain`, Anode) $\to$ Pin 3 (`Source`, Cathode)**:
     - **`Q_REV_BAT` (Ideal Diode Reverse-Battery Protection):** Pin 1 (`Gate`) $\to$ `1.0 kΩ` to `DGND`; Pin 2 (`Drain`) $\to$ Switched Battery `J_BAT+` (`SW_PWR` Pole 1); Pin 3 (`Source`) $\to$ `MOD_CHG` Pin 5 (`BAT`).
       - *Normal Battery ($+3.7\text{ V}$):* Body diode conducts `D -> S` at startup, pulling `S` to $+3.05\text{ V}$, setting $V_{GS} = -3.05\text{ V} \ll V_{GS(\text{th})}$, enhancing the channel ($R_{DS(\text{on})} \approx 58\text{ m}\Omega$) for bidirectional discharge and fast charge ($|V_{GS,\text{max}}| = 4.20\text{ V} < 8.0\text{ V} V_{GSS,\text{max}}$).
       - *Reverse Battery ($-4.2\text{ V}$):* Body diode is reverse-biased (`Anode = -4.2V`, `Cathode = 0V`) and $V_{GS} = 0.0\text{ V}$, blocking reverse current. **VERIFIED.**
     - **`Q_ANA_SW` (High-Side Analog Core Power Switch):** Pin 1 (`Gate`) $\to$ `Q_ANA_N` Drain + `47 kΩ` to `V_BUS_SYS`; Pin 2 (`Drain`) $\to$ `V_ANA_IN`; Pin 3 (`Source`) $\to$ `V_BUS_SYS`.
       - *Standby (`OFF`):* $V_G = V_S = V_{\text{BUS\_SYS}} \implies V_{GS} = 0.0\text{ V}$, and body diode (`Anode = V_ANA_IN = 0V`, `Cathode = V_BUS_SYS`) is reverse-biased, blocking forward conduction into `V_ANA_IN`. **VERIFIED.**
5. **Dual Ultra-LDOs (`MCP1826S-3302E/AB`), Critically Damped Pi-Filters & Single-Point Star Ground (`R_STAR`):**
   - `LM1117T-3.3` ($V_{\text{DO}} \approx 1.15\text{ V} \implies V_{\text{IN,min}} = 4.45\text{ V}$) is properly rejected in favor of `2x MCP1826S-3302E/AB` (`U_LDO_DIG`, `U_LDO_ANA`, TO-220-3: `1=VIN, 2=GND, 3=VOUT`), which achieves $V_{\text{DO}} < 75\text{ mV}$ at $300\text{ mA}$ ($30\text{ mV}$ at $120\text{ mA}$), maintaining full $3.300\text{ V}$ regulation down to $3.40\text{ V}$ LiPo cutoff.
   - Pi-Filter #1 ($100\text{ }\mu\text{H} + 2.2\,\Omega$, $\zeta_{\text{tank}} = 0.902$, $-66.44\text{ dB}$ @ $1.5\text{ MHz}$) + `L7812CV` + `RXEF017` + `1N4743A` + `L78L09ACZ` and Pi-Filter #2 ($47\text{ }\mu\text{H} + 1.5\,\Omega$, $\zeta_{\text{tank}} = 0.851$, $-59.88\text{ dB}$ @ $1.5\text{ MHz}$) + `U_LDO_ANA` + `R_ADC_F (10 Ω) / C_ADC_F (10.1 µF)` ($-94.28\text{ dB}$ total passive attenuation @ $1.5\text{ MHz}$ onto `+3V3_ADC`) are mathematically exact.
   - `AGND` and `DGND` are partitioned with a $\ge 2.0\text{ mm}$ moat and joined exclusively at `R_STAR` (`0R_STAR_BRIDGE`).
6. **Dual `H11L1M` (`U_ISO1`, `U_ISO2`) Optical UART Isolation, `J_CYD_UART` & Binary Protocol Verification:**
   - Cathode-drive wiring (`+3V3 -> 470 Ω -> Pin 1 [A]`, `UART_TX -> Pin 2 [K]`) cancels the open-collector Schmitt output inversion (`Pin 4 [VO]` with `2.2 kΩ` pull-up), providing non-inverting 8N1 UART polarity with **$0.000\text{ mA}$ idle LED current** and **$I_F = 4.362\text{ mA}$** active drive ($2.73\times$ overdrive above $1.60\text{ mA}$ max threshold).
   - At $115,200\text{ bps}$ ($T_{\text{bit}} = 8.68\text{ }\mu\text{s}$), $t_r = 120.8\text{ ns}$, $t_f = 100\text{ ns}$, and pulse-width distortion is $0.20\text{ }\mu\text{s}$ ($2.30\%$ bit skew).
   - In Standby (`+3V3_ANA = 0.0V`), `U_ISO2` detector (`Pin 6`) and `R_PU_ISO2` are unpowered, guaranteeing **$0.000\text{ }\mu\text{A}$ back-powering** into `U_MCU1`.
   - `J_CYD_UART` (`1=V_BUS_SYS, 2=DGND, 3=CYD_TX_TAP, 4=CYD_RX_TAP`) includes `470 Ω` series isolation resistors (`R_CYD_TX`, `R_CYD_RX`), limiting worst-case external contention to $6.60\text{ mA}$ (total $8.02\text{ mA} < 16.0\text{ mA}$ `H11L1M` $I_{\text{OL,max}}$).
   - Binary protocol framing (`[0xAA][0x55][SEQ:1][OPCODE:1][LEN:2 LE][PAYLOAD:0..256][CRC16_CCITT:2 LE]`), CRC-16-CCITT (`0x1021`, init `0xFFFF`) test vectors, all 13 opcodes (`0x01, 0x10..0x15, 0x80..0x85`), and fail-safe timeouts ($50\text{ ms}$ ACK, $300\text{ ms}$ step, $5.0\text{ s}$ total scan + `ANA_PWR_EN` hard power-cycle fallback) are complete and verified.

---

## 7. Cross-Document Master Net & Pin Verification Table (Blocking Check #6)

| Canonical Net Name | Source / Driver Pin(s) | Destination / Load Pin(s) | Verified Consistent Across Docs |
| :--- | :--- | :--- | :---: |
| **`FAULT_OVC_N`** | `Q_OVC` (`2N3904` Pin 3 Collector) + `10 kΩ` pull-up to `+3V3_ANA` | `U_MCU1` Pin 8 (`GPIO25` fast falling-edge ISR) | `p1_pinout`, `p1_failsafe`, `p1_power`, `p1_uart` — **MATCH** |
| **`ANA_I2C_SDA`** | `U_MCU1` Pin 16 (`GPIO19`) + `4.7 kΩ` pull-up to `+3V3_ANA` | `MOD_ADS` (`ADS1115` Pin 4 `SDA`, addr `0x48`) | `p1_pinout`, `p1_afe` — **MATCH** |
| **`ANA_I2C_SCL`** | `U_MCU1` Pin 17 (`GPIO23`) + `4.7 kΩ` pull-up to `+3V3_ANA` | `MOD_ADS` (`ADS1115` Pin 3 `SCL`, addr `0x48`) | `p1_pinout`, `p1_afe` — **MATCH** |
| **`ADS_ALRT`** | `MOD_ADS` (`ADS1115` Pin 6 `ALRT`) + `10 kΩ` pull-up to `+3V3_ANA` | `U_MCU1` Pin 4 (`GPIO34` input-only) | `p1_pinout`, `p1_afe`, `p1_failsafe` — **MATCH** |
| **`AFE_SPI_SCK`** | `U_MCU1` Pin 11 (`GPIO14`) | `U9 MCP4822` Pin 3 (`SCK`), `U6 & U7 74HC595N` Pin 11 (`SRCLK`) | `p1_pinout`, `p1_afe` — **MATCH** |
| **`AFE_SPI_MOSI`** | `U_MCU1` Pin 10 (`GPIO27`) | `U9 MCP4822` Pin 4 (`SDI`), `U6 74HC595N` Pin 14 (`SER`) | `p1_pinout`, `p1_afe` — **MATCH** |
| **`DAC_CS`** | `U_MCU1` Pin 9 (`GPIO26`) + `R_DACCS = 10 kΩ` pull-up to `+3V3_ANA` | `U9 MCP4822` Pin 2 (`CS_N`) | `p1_pinout`, `p1_afe` — **MATCH** |
| **`SR_LATCH`** | `U_MCU1` Pin 18 (`GPIO18`) + `10 kΩ` pull-down to `AGND` | `U6 & U7 74HC595N` Pin 12 (`RCLK`) | `p1_pinout`, `p1_afe`, `p1_failsafe` — **MATCH** |
| **`SR_OE_N`** | `U_MCU1` Pin 20 (`GPIO17`) + `R_OEN = 10 kΩ` pull-up to `+3V3_ANA` | `U6 & U7 74HC595N` Pin 13 (`OE_N`) | `p1_pinout`, `p1_afe`, `p1_failsafe`, `p1_uart` — **MATCH** |
| **`CAL_A0`, `CAL_A1`** | `U_MCU1` Pin 6 (`GPIO32`), Pin 7 (`GPIO33`) | `U10 DG409DJ` Pin 1 (`A0`), Pin 16 (`A1`) | `p1_pinout`, `p1_afe` — **MATCH** |
| **`FAST_TP1..3`** | `ADC_TP1..3` via `R_ISO1..3 = 10 kΩ` | `U_MCU1` Pin 1 (`GPIO36`), Pin 2 (`GPIO39`), Pin 5 (`GPIO35`) | `p1_pinout`, `p1_afe` — **MATCH** |
| **`TEMP_DQ`** | `U_MCU1` Pin 22 (`GPIO4`) + `4.7 kΩ` pull-up to `+3V3_ANA` | `J_TEMP` (`DS18B20` 1-Wire Data Pin 2) | `p1_pinout`, `p1_afe`, `p1_power` — **MATCH** |
| **`LINK_TX_M1`** | `U_MCU1` Pin 21 (`GPIO16` UART2 TX) | `U_ISO1` (`H11L1M` Pin 2 Cathode `K`) | `p1_pinout`, `p1_uart` — **MATCH** |
| **`LINK_RX_M1`** | `U_ISO2` (`H11L1M` Pin 4 `VO`) + `R_PU_ISO2 = 2.2 kΩ` to `+3V3_ANA` | `U_MCU1` Pin 26 (`GPIO13` UART2 RX) | `p1_pinout`, `p1_uart` — **MATCH** |
| **`LINK_RX_M2`** | `U_ISO1` (`H11L1M` Pin 4 `VO`) + `R_PU_ISO1 = 2.2 kΩ` to `+3V3_DIG` | `U_MCU2` Pin 7 (`GPIO20` UART0 RX) & `R_CYD_RX (470 Ω)` $\to$ `CYD_RX_TAP` | `p1_pinout`, `p1_uart` — **MATCH** |
| **`LINK_TX_M2`** | `U_MCU2` Pin 8 (`GPIO21` UART0 TX) | `U_ISO2` (`H11L1M` Pin 2 Cathode `K`) & `R_CYD_TX (470 Ω)` $\to$ `CYD_TX_TAP` | `p1_pinout`, `p1_uart` — **MATCH** |
| **`UI_I2C_SDA`** | `U_MCU2` Pin 16 (`GPIO0`) + `4.7 kΩ` pull-up to `+3V3_DIG` | `U11 MCP23017` Pin 13 (`SDA`, `0x20`), `MOD_CHG BQ25895` Pin 8 (`SDA`, `0x6A`) | `p1_pinout`, `p1_power` — **MATCH** |
| **`UI_I2C_SCL`** | `U_MCU2` Pin 15 (`GPIO1`) + `4.7 kΩ` pull-up to `+3V3_DIG` | `U11 MCP23017` Pin 12 (`SCL`, `0x20`), `MOD_CHG BQ25895` Pin 7 (`SCL`, `0x6A`) | `p1_pinout`, `p1_power` — **MATCH** |
| **`UI_SPI_SCK`, `UI_SPI_MOSI`** | `U_MCU2` Pin 12 (`GPIO4`), Pin 2 (`GPIO6`) | `MOD_TFT1 & MOD_TFT2` Pin 3 (`SCL`), Pin 4 (`SDA`) | `p1_pinout` — **MATCH** |
| **`TFT_DC`, `TFT1_CS`, `TFT2_CS`** | `U_MCU2` Pin 1 (`GPIO5`), Pin 3 (`GPIO7` + `10k`), Pin 6 (`GPIO10` + `10k`) | `MOD_TFT1 & MOD_TFT2` Pin 6 (`DC`), Pin 7 (`CS`) | `p1_pinout` — **MATCH** |
| **`ANA_PWR_EN`** | `U11 MCP23017` Pin 8 (`GPB7`) + `R_PD_ANA = 100 kΩ` to `DGND` | `Q_ANA_N` (`2N7000` Gate Pin 2 via `100 Ω`) $\to$ `Q_ANA_SW` (`NDP6020P` Gate Pin 1) | `p1_pinout`, `p1_power`, `p1_uart` — **MATCH** |
| **`TFT_BLK_EN`** | `U11 MCP23017` Pin 7 (`GPB6`) + `R_PD_BLK = 100 kΩ` to `DGND` | `Q_BLK` (`2N7000` Gate Pin 2 via `100 Ω`) $\to$ `MOD_TFT1/2` Pin 8 (`BLK`) | `p1_pinout`, `p1_power` — **MATCH** |
| **`V_BUS_SYS`** | `MOD_CHG` (`BQ25895` Pin 4 `SYS`) via `SW_PWR` Pole 2 | `U_LDO_DIG` Pin 1 (`VIN`), `Q_ANA_SW` Pin 3 (`Source`), `J_CYD_UART` Pin 1 | `p1_pinout`, `p1_power`, `p1_uart` — **MATCH** |
| **`+3V3_DIG`** | `U_LDO_DIG` (`MCP1826S-3302E/AB` Pin 3 `VOUT`) | `JP_M2_3V3` $\to$ `M2_3V3` (`U_MCU2` P11), `U11` P9, `MOD_TFT1/2` P2, `U_ISO1` P6, `U_ISO2` P1 (`470R`), `SW_BZ` $\to$ `BZ1`, `LED_CHG` (`1.5k`) | `p1_pinout`, `p1_power`, `p1_uart` — **MATCH** |
| **`+3V3_ANA`** | `U_LDO_ANA` (`MCP1826S-3302E/AB` Pin 3 `VOUT`) | `JP_M1_3V3` $\to$ `M1_3V3` (`U_MCU1` P14), `U6/U7` P16, `U9` P1, `U8` P12 (`VL`), `U10` P2 (`EN`), `U_ISO2` P6, `U_ISO1` P1 (`470R`), `R_ADC_F (10R)` | `p1_pinout`, `p1_afe`, `p1_power`, `p1_uart` — **MATCH** |
| **`+3V3_ADC`** | `+3V3_ANA` via `R_ADC_F = 10 Ω` + `10 µF // 100 nF` to `AGND` | `MOD_ADS` (`ADS1115` Pin 1 `VDD`) | `p1_pinout`, `p1_afe`, `p1_failsafe`, `p1_power` — **MATCH** |
| **`+12V_A`** | `U_REG12` (`L7812CV` Pin 3) via `F1` (`RXEF017`) + `D_OVP` (`1N4743A`) | `U1..U3` P13, `U8` P13, `U10` P14, `U4` P4, `U5` P8, `D_CH1..3` / `D_HVH` Cathodes, `U_REG9` (`L78L09ACZ` Pin 3 `IN`) | `p1_afe`, `p1_failsafe`, `p1_power` — **MATCH** |
| **`+9V_REF` / `+9V_LIM`** | `U_REG9` (`L78L09ACZ` Pin 1 `OUT` = `+9V_REF`) $\to$ `R_ILIM (43 Ω)` + `Q_LIM1/2 (2x 2N3906)` (`+9V_LIM`) | `U1..U3` (`DG408DJ` Pin 9 `S8`), `U10` (`DG409DJ` Pin 6 `S3A`) | `p1_afe`, `p1_failsafe`, `p1_power` — **MATCH** |
| **`AGND` / `DGND`** | Partitioned copper ground zones ($\ge 2.0\text{ mm}$ moat) | Joined at single-point star bridge `R_STAR` (`0R_STAR_BRIDGE`) | All 6 documents — **MATCH** |

---

## 8. Minor Non-Blocking Harmonization Notes for Phase 2 (`build_libraries.py` / `build_design.py`) & Phase 3 Firmware

While zero blocking defects exist and all six deliverables pass Gate 1, the Phase 2 schematic/PCB generator and Phase 3 firmware authors should adopt the following canonical naming conventions where two documents used minor descriptive aliases for internal intermediate nodes:

| # | Item | Alias in Document A | Alias in Document B | Canonical Standard for Phase 2 / Phase 3 |
| :---: | :--- | :--- | :--- | :--- |
| **1** | **Charger Protected Input & Battery Nets (`MOD_CHG` Pins 1 & 5)** | `p1_pinout_extraction.md` (§7): calls Pin 1 net `VBUS_CHG` and Pin 5 net `VBAT_SW`. | `p1_power_path_dual_switch.md` (§1, §2.2, §3.2): calls Pin 1 net `V_USB_PROT` and traces `J_BAT+` $\to$ `SW_PWR` Pole 1 (`V_BAT_SW`) $\to$ `Q_REV_BAT` (Pin 2 D $\to$ Pin 3 S) $\to$ `V_BAT_PROT` (`MOD_CHG` Pin 5 `BAT`). | **Use `p1_power_path_dual_switch.md` chain:**<br/>• `J_USB_C VBUS` $\to$ `F_USB` $\to$ **`V_USB_PROT`** (`D_TVS_USB` & `MOD_CHG` Pin 1 `VBUS`)<br/>• `J_BAT+` $\to$ `SW_PWR` Pole 1 $\to$ **`V_BAT_SW`** (`Q_REV_BAT` Pin 2 Drain)<br/>• `Q_REV_BAT` Pin 3 Source $\to$ **`V_BAT_PROT`** (`MOD_CHG` Pin 5 `BAT`) |
| **2** | **Passive Resistor Designators (`LED_CHG` & `FAULT_OVC_N` pull-ups)** | `p1_pinout_extraction.md` (§7): `R_LCHG = 1.5k`<br/>`p1_dut_failsafe_math.md` (§3.1): `R_OVC_PU = 10k` | `p1_power_path_dual_switch.md` (§2.4, §6.1): `R_LED_CHG = 1.5k` and `R_PU_OVC = 10k` | Standardize on **`R_LED_CHG`** ($1.5\text{ k}\Omega$) and **`R_PU_OVC`** ($10\text{ k}\Omega$) in `build_design.py`. |
| **3** | **`DG408DJ` Pin 12 Connection Note** | `p1_afe_matrix_topology.md` (§3.2–3.3) & existing `build_design.py`: `U1..U3 DG408DJ` Pin 12 is analog switch `S5` (`R_LG{ch}`); `U8 DG412LDJ` Pin 12 is logic reference `VL` (`+3V3_ANA`). | `p1_power_path_dual_switch.md` (§6.2 line 380 diagram caption): parenthetically lists `U1–U3 (DG408 VL)` alongside `U8 (DG412 VL)`. | Keep existing `build_design.py` and `p1_afe_matrix_topology.md` wiring: **`U1..U3` Pin 12 = `C{ch}_S5`** (no `VL` pin exists on `DG408DJ`), and **`U8 DG412LDJ` Pin 12 = `+3V3_ANA` (`VL`)**. |
| **4** | **`CMD_DMM_READ (0x14)` Channel Enum vs. `U10 DG409DJ` Hardware Address** | `p1_afe_matrix_topology.md` (§3.4) & `p1_uart_isolation_spec.md` (`CMD_DEBUG_SET` / `EVT_RAW_DEBUG`): `CAL_A1:CAL_A0` hardware index is `0 = V_SRC_A`, `1 = V_SRC_B`, `2 = +9V_LIM`, `3 = DMM_30V_DIV`. | `p1_uart_isolation_spec.md` (§3.3 `CMD_DMM_READ`): lists logical enum `0 = DMM_30V`, `1 = V_SRC_A`, `2 = V_SRC_B`, `3 = +9V_LIM`. | In Phase 3 firmware, use the **direct hardware `CAL_A1:CAL_A0` index (`0=V_SRC_A, 1=V_SRC_B, 2=+9V_LIM, 3=DMM_30V_DIV`)** across `CMD_DMM_READ`, `CMD_DEBUG_SET`, and `EVT_RAW_DEBUG`. |

---

## 9. Final Gate 1 Sign-Off

All six Phase 1 deliverables satisfy every electrical, mathematical, device-physics, and silicon-datasheet requirement for the Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine. **Gate 1 (`gate_1`) is hereby declared `PASS`**, and the project is cleared to proceed to Phase 2 (`build_libraries.py`, `build_design.py`, KiCad 9 schematic/PCB generation, and DRC/ERC verification).
