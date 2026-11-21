# Phase 2 Deliverable — Block 5 & Block 6 Complete Pin-by-Pin AFE & ADS1115 Schematic Netlist (`p2_afe_ads1115_netlist.md`)

**Project:** KAOTI AUDIO — Rev 5.0 Dual-MCU Germanium & Universal Parametric Characterization Engine  
**Scope:** 100% Through-Hole (THT) & Machined-Socketed Pin-by-Pin Netlist for **Block 5 (Solid-State 3-Terminal AFE Matrix, Shift Registers & Failsafes)** and **Block 6 (`ADS1115` Precision Sensing, `MCP4822` Force DAC, `LMC6484IN` 20 fA Buffer & `DG409DJ` Cal/HV Mux)**  
**Cross-Reference:** Harmonized 1:1 with `/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/gate_1_audit.md` (§7 & §8 Canonical Net Table), `p1_afe_matrix_topology.md`, `p1_dut_failsafe_math.md`, `scripts/build_libraries.py`, and `scripts/build_design.py`.

---

## 1. Canonical Power, Ground & Inter-Block Interface Nets (Blocks 5 & 6)

All components in Block 5 and Block 6 reside exclusively inside the **Quiet Analog Island (`AGND`)** and interface with `U_MCU1` (Block 3) and the Analog Supply Rails (Block 2) via the following canonical nets:

| Net Name | Net Class | Origin / Driver | Block 5 / Block 6 Destinations |
| :--- | :--- | :--- | :--- |
| **`AGND`** | `ANALOG_RAIL` | Quiet Analog Copper Pour (joined to `DGND` only at `R_STAR`) | All Block 5 & Block 6 ground pins, shunts, and decoupling return pads |
| **`+12V_A`** | `ANALOG_RAIL` | Block 2 `U_REG12` (`L7812CV`) via `F1` (`RXEF017`) + `D_OVP` (`1N4743A`) | `U1..U3` P13, `U8` P13, `U10` P14, `U4` P4, `U5` P8, `D_CH1..3` P1, `D_HVH` P1 |
| **`+9V_LIM`** | `ANALOG_RAIL` | Block 2 `U_REG9` (`L78L09ACZ`) $\to$ `R_ILIM (43R)` + `Q_LIM1/2 (2x 2N3906)` | `U1..U3` (`DG408DJ` Pin 9 `S8`), `U10` (`DG409DJ` Pin 6 `S3A`) |
| **`+3V3_ANA`** | `ANALOG_RAIL` | Block 2 `U_LDO_ANA` (`MCP1826S-3302E/AB` Pin 3) | `U6/U7` P16, `U8` P12 (`VL`), `U9` P1, `U10` P2 (`EN`), pull-ups `R_OEN, R_POR, R_DACCS, R_SDA, R_SCL, R_ALRT, R_TEMP` |
| **`+3V3_ADC`** | `ANALOG_RAIL` | Block 2 `+3V3_ANA` via `R_ADC_F (10R)` + `10uF // 100nF` RC filter | `MOD_ADS` (`ADS1115` Pin 1 `VDD`), `C9` (`100nF`) |
| **`AFE_SPI_SCK`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 11 (`GPIO14`) | `U6` P11 (`SRCLK`), `U7` P11 (`SRCLK`), `U9` P3 (`SCK`) |
| **`AFE_SPI_MOSI`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 10 (`GPIO27`) | `U6` P14 (`SER`), `U9` P4 (`SDI`) |
| **`DAC_CS`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 9 (`GPIO26`) | `U9` P2 (`CS_N`), `R_DACCS` (`10k` to `+3V3_ANA`) |
| **`SR_LATCH`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 18 (`GPIO18`) | `U6` P12 (`RCLK`), `U7` P12 (`RCLK`), `R_LATCH_PD` (`10k` to `AGND`) |
| **`SR_OE_N`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 20 (`GPIO17`, non-strapping) | `U6` P13 (`OE_N`), `U7` P13 (`OE_N`), `R_OEN` (`10k` to `+3V3_ANA`) |
| **`ANA_I2C_SDA`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 16 (`GPIO19`) | `MOD_ADS` P4 (`SDA`), `R_SDA` (`4.7k` to `+3V3_ANA`) |
| **`ANA_I2C_SCL`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 17 (`GPIO23`) | `MOD_ADS` P3 (`SCL`), `R_SCL` (`4.7k` to `+3V3_ANA`) |
| **`ADS_ALRT`** | `DIGITAL_SIGNAL` | Block 6 `MOD_ADS` Pin 6 (`ALRT`) | Block 3 `U_MCU1` Pin 4 (`GPIO34`), `R_ALRT` (`10k` to `+3V3_ANA`) |
| **`CAL_A0`, `CAL_A1`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 6 (`GPIO32`), Pin 7 (`GPIO33`) | `U10` (`DG409DJ`) Pin 1 (`A0`), Pin 16 (`A1`) |
| **`FAST_TP1..3`** | `DIGITAL_SIGNAL` | Block 6 `R_ISO1..3` (`10k` from `ADC_TP1..3`) | Block 3 `U_MCU1` Pin 1 (`GPIO36`), Pin 2 (`GPIO39`), Pin 5 (`GPIO35`) |
| **`TEMP_DQ`** | `DIGITAL_SIGNAL` | Block 3 `U_MCU1` Pin 22 (`GPIO4`) | Block 5 `J_TEMP` Pin 2 (`DQ`), `R_TEMP` (`4.7k` to `+3V3_ANA`) |

---

## 2. Block 5 Netlist — Solid-State 3-Terminal AFE Matrix, Shift Registers & Failsafes

### 2.1 Glitch-Free 16-Bit Shift Register Control Bus (`U6`, `U7` + Failsafe Passives)

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U6`** | `74HC595N` | `74HC595N` (Low Byte: `TP1` & `TP2` Mux Control) | `Socket_DIP-16_W7.62mm` | CMOS 3.3V Logic, Machined DIP-16 Socket | `1(QB): U1_A1`, `2(QC): U1_A2`, `3(QD): U1_EN`, `4(QE): U2_A0`, `5(QF): U2_A1`, `6(QG): U2_A2`, `7(QH): U2_EN`, `8(GND): AGND`, `9(QH'): SR_CHAIN`, `10(SRCLR_N): SR_CLR_N`, `11(SRCLK): AFE_SPI_SCK`, `12(RCLK): SR_LATCH`, `13(OE_N): SR_OE_N`, `14(SER): AFE_SPI_MOSI`, `15(QA): U1_A0`, `16(VCC): +3V3_ANA` |
| **`U7`** | `74HC595N` | `74HC595N` (High Byte: `TP3` Mux, `U8` Clamps & `D2`) | `Socket_DIP-16_W7.62mm` | CMOS 3.3V Logic, Machined DIP-16 Socket | `1(QB): U3_A1`, `2(QC): U3_A2`, `3(QD): U3_EN`, `4(QE): SW_G1`, `5(QF): SW_G2`, `6(QG): SW_G3`, `7(QH): LED2_DRV`, `8(GND): AGND`, `9(QH'): NC`, `10(SRCLR_N): SR_CLR_N`, `11(SRCLK): AFE_SPI_SCK`, `12(RCLK): SR_LATCH`, `13(OE_N): SR_OE_N`, `14(SER): SR_CHAIN`, `15(QA): U3_A0`, `16(VCC): +3V3_ANA` |
| **`C12`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +3V3_ANA`, `2: AGND` (Local bypass for `U6` Pin 16) |
| **`C13`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +3V3_ANA`, `2: AGND` (Local bypass for `U7` Pin 16) |
| **`R_OEN`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: +3V3_ANA`, `2: SR_OE_N` (Holds `U6/U7` outputs `Hi-Z` at boot/reset) |
| **`R_LATCH_PD`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: SR_LATCH`, `2: AGND` (Prevents spurious `RCLK` latch edges at boot) |
| **`R_POR`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: +3V3_ANA`, `2: SR_CLR_N` ($\tau_{\text{POR}} = 10.0\text{ ms}$ hardware shift clear) |
| **`C_POR`** | `C_THT` | `1uF` (`1.0 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: SR_CLR_N`, `2: AGND` ($t_{\text{clear}} = 6.93\text{ ms}$ below $1.65\text{ V}$) |
| **`R_LED2`** | `R_THT` | `1k` (`1.00 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: LED2_DRV`, `2: LED2_A` |
| **`D2`** | `LED_THT` | `TEST_ACT` (Blue 3mm LED) | `LED_D3mm_Blue` | $V_F \approx 3.0\text{ V}$, $I_F \approx 0.3\text{ mA}$ | `1(K): AGND`, `2(A): LED2_A` |

---

### 2.2 Ultra-Low-Leakage Quad Ground Clamp (`U8` — `DG412LDJ`)

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U8`** | `DG412LDJ` | `DG412LDJ` (Quad SPST Normally-Open CMOS Switch) | `Socket_DIP-16_W7.62mm` | $R_{\text{ON}} = 20\,\Omega$ typ, $I_{\text{off}} < 1\text{ nA}$, $+12\text{V}$ Supply | `1(IN1): SW_G1`, `2(D1): TP1_MUX`, `3(S1): AGND`, `4(V-): AGND`, `5(GND): AGND`, `6(S4): AGND`, `7(D4): AGND`, `8(IN4): AGND`, `9(IN3): SW_G3`, `10(D3): TP3_MUX`, `11(S3): AGND`, `12(VL): +3V3_ANA`, `13(V+): +12V_A`, `14(S2): AGND`, `15(D2): TP2_MUX`, `16(IN2): SW_G2` |
| **`C21`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +12V_A`, `2: AGND` (Local bypass for `U8` Pin 13) |
| **`R_PD1`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: SW_G1`, `2: AGND` (Holds `U8` Switch 1 OPEN during `SR_OE_N` Hi-Z) |
| **`R_PD2`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: SW_G2`, `2: AGND` (Holds `U8` Switch 2 OPEN during `SR_OE_N` Hi-Z) |
| **`R_PD3`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: SW_G3`, `2: AGND` (Holds `U8` Switch 3 OPEN during `SR_OE_N` Hi-Z) |

---

### 2.3 Three Symmetric Solid-State DUT Channels (`U1..U3 DG408DJ`, `0.1%` R.G. Keen Banks, `R_PR1..3`, `1N3595` Clamps)

> [!NOTE]
> **Canonical Pin 12 Rule (per `gate_1_audit.md` §8 Item 3):** On `U1, U2, U3` (`DG408DJ`), Pin 12 is analog switch terminal **`S5`** (`C{ch}_S5` $\to$ `R_LG{ch} = 470 Ω` to `AGND`). Only `U8` (`DG412LDJ`) has a `VL` logic reference on Pin 12 (`+3V3_ANA`).

#### Channel 1 — `TP1_DUT` (`U1` and Associated Precision Passives)

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U1`** | `DG408DJ` | `DG408DJ` (8:1 CMOS Analog Multiplexer, Ch 1) | `Socket_DIP-16_W7.62mm` | $I_{\text{off}} \approx 20\text{ pA}$ typ, $R_{\text{ON}} \approx 50\,\Omega$, BBM | `1(A0): U1_A0`, `2(EN): U1_EN`, `3(V-): AGND`, `4(S1): C1_S1`, `5(S2): C1_S2`, `6(S3): C1_S3`, `7(S4): C1_S4`, `8(D_COM): TP1_MUX`, `9(S8): +9V_LIM`, `10(S7): AGND`, `11(S6): C1_S6`, `12(S5): C1_S5`, `13(V+): +12V_A`, `14(GND): AGND`, `15(A2): U1_A2`, `16(A1): U1_A1` |
| **`C14`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +12V_A`, `2: AGND` (Local bypass for `U1` Pin 13) |
| **`R_EN1`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: U1_EN`, `2: AGND` (Forces `U1` all-open during `SR_OE_N` Hi-Z) |
| **`R_PR1`** | `R_THT` | `100R_0.5W_0.1%` (`100 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.5W` ($500\text{ mW}$)** Metal Film | `1: TP1_DUT` (Kelvin Sense Side), `2: TP1_MUX` (Mux Force Side) |
| **`D_CH1`** | `D_THT` | `1N3595` (Ultra-Low-Leakage Clamp) | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$ ($<10\text{ pA}$ typ), $150\text{V}$ | `1(K): +12V_A`, `2(A): TP1_MUX` (Upper rail clamp) |
| **`D_CL1`** | `D_THT` | `1N3595` (Ultra-Low-Leakage Clamp) | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$ ($<10\text{ pA}$ typ), $150\text{V}$ | `1(K): TP1_MUX`, `2(A): AGND` (Lower rail clamp) |
| **`R_LA1`** | `R_THT` | `470R_0.1%` (`470 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C1_S1`, `2: V_SRC_A` (`S1` high-current source shunt) |
| **`R_KA1`** | `R_THT` | `2.49k_0.1%` (`2.49 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C1_S2`, `2: V_SRC_A` (`S2` R.G. Keen / Safe Probe source shunt) |
| **`R_MB1`** | `R_THT` | `100k_0.1%` (`100.0 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C1_S3`, `2: V_SRC_B` (`S3` R.G. Keen precision base current shunt) |
| **`R_KB1`** | `R_THT` | `2.20M_0.1%` (`2.20 MΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 50\text{ ppm}/^\circ\text{C}$ | `1: C1_S4`, `2: V_SRC_B` (`S4` Darlington / Mega-Ohm source shunt) |
| **`R_LG1`** | `R_THT` | `470R_0.1%` (`470 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C1_S5`, `2: AGND` (`S5` high-current ground sink shunt) |
| **`R_KG1`** | `R_THT` | `2.49k_0.1%` (`2.49 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C1_S6`, `2: AGND` (`S6` R.G. Keen PNP collector / $I_{CEO}$ sink shunt) |

#### Channel 2 — `TP2_DUT` (`U2` and Associated Precision Passives)

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U2`** | `DG408DJ` | `DG408DJ` (8:1 CMOS Analog Multiplexer, Ch 2) | `Socket_DIP-16_W7.62mm` | $I_{\text{off}} \approx 20\text{ pA}$ typ, $R_{\text{ON}} \approx 50\,\Omega$, BBM | `1(A0): U2_A0`, `2(EN): U2_EN`, `3(V-): AGND`, `4(S1): C2_S1`, `5(S2): C2_S2`, `6(S3): C2_S3`, `7(S4): C2_S4`, `8(D_COM): TP2_MUX`, `9(S8): +9V_LIM`, `10(S7): AGND`, `11(S6): C2_S6`, `12(S5): C2_S5`, `13(V+): +12V_A`, `14(GND): AGND`, `15(A2): U2_A2`, `16(A1): U2_A1` |
| **`C15`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +12V_A`, `2: AGND` (Local bypass for `U2` Pin 13) |
| **`R_EN2`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: U2_EN`, `2: AGND` |
| **`R_PR2`** | `R_THT` | `100R_0.5W_0.1%` (`100 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.5W` ($500\text{ mW}$)** Metal Film | `1: TP2_DUT`, `2: TP2_MUX` |
| **`D_CH2`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$, $150\text{V}$ | `1(K): +12V_A`, `2(A): TP2_MUX` |
| **`D_CL2`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$, $150\text{V}$ | `1(K): TP2_MUX`, `2(A): AGND` |
| **`R_LA2`** | `R_THT` | `470R_0.1%` (`470 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C2_S1`, `2: V_SRC_A` |
| **`R_KA2`** | `R_THT` | `2.49k_0.1%` (`2.49 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C2_S2`, `2: V_SRC_A` |
| **`R_MB2`** | `R_THT` | `100k_0.1%` (`100.0 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C2_S3`, `2: V_SRC_B` |
| **`R_KB2`** | `R_THT` | `2.20M_0.1%` (`2.20 MΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 50\text{ ppm}/^\circ\text{C}$ | `1: C2_S4`, `2: V_SRC_B` |
| **`R_LG2`** | `R_THT` | `470R_0.1%` (`470 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C2_S5`, `2: AGND` |
| **`R_KG2`** | `R_THT` | `2.49k_0.1%` (`2.49 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C2_S6`, `2: AGND` |

#### Channel 3 — `TP3_DUT` (`U3` and Associated Precision Passives)

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U3`** | `DG408DJ` | `DG408DJ` (8:1 CMOS Analog Multiplexer, Ch 3) | `Socket_DIP-16_W7.62mm` | $I_{\text{off}} \approx 20\text{ pA}$ typ, $R_{\text{ON}} \approx 50\,\Omega$, BBM | `1(A0): U3_A0`, `2(EN): U3_EN`, `3(V-): AGND`, `4(S1): C3_S1`, `5(S2): C3_S2`, `6(S3): C3_S3`, `7(S4): C3_S4`, `8(D_COM): TP3_MUX`, `9(S8): +9V_LIM`, `10(S7): AGND`, `11(S6): C3_S6`, `12(S5): C3_S5`, `13(V+): +12V_A`, `14(GND): AGND`, `15(A2): U3_A2`, `16(A1): U3_A1` |
| **`C16`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +12V_A`, `2: AGND` (Local bypass for `U3` Pin 13) |
| **`R_EN3`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: U3_EN`, `2: AGND` |
| **`R_PR3`** | `R_THT` | `100R_0.5W_0.1%` (`100 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.5W` ($500\text{ mW}$)** Metal Film | `1: TP3_DUT`, `2: TP3_MUX` |
| **`D_CH3`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$, $150\text{V}$ | `1(K): +12V_A`, `2(A): TP3_MUX` |
| **`D_CL3`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$, $150\text{V}$ | `1(K): TP3_MUX`, `2(A): AGND` |
| **`R_LA3`** | `R_THT` | `470R_0.1%` (`470 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C3_S1`, `2: V_SRC_A` |
| **`R_KA3`** | `R_THT` | `2.49k_0.1%` (`2.49 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C3_S2`, `2: V_SRC_A` |
| **`R_MB3`** | `R_THT` | `100k_0.1%` (`100.0 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C3_S3`, `2: V_SRC_B` |
| **`R_KB3`** | `R_THT` | `2.20M_0.1%` (`2.20 MΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 50\text{ ppm}/^\circ\text{C}$ | `1: C3_S4`, `2: V_SRC_B` |
| **`R_LG3`** | `R_THT` | `470R_0.1%` (`470 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C3_S5`, `2: AGND` |
| **`R_KG3`** | `R_THT` | `2.49k_0.1%` (`2.49 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: C3_S6`, `2: AGND` |

---

### 2.4 DUT Socket Bay & Ambient Temperature Probe (`J_ZIF`, `J_DUT3`, `J_KELVIN`, `J_TERM5`, `J_TEMP`)

All four DUT fixture interfaces connect directly to the **Kelvin sense side** (`TP1_DUT`, `TP2_DUT`, `TP3_DUT`) of `R_PR1..3`:

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`J_ZIF`** | `ZIF_14Pin` | `ZIF-14_DUT` (14-Pin Zero-Insertion-Force Test Socket) | `Socket_ZIF_14Pin` | Gold/Tin Contacts, $7.62\text{ mm}$ Row Pitch | `1: TP1_DUT`, `2: TP1_DUT`, `3: TP1_DUT`, `4: TP2_DUT`, `5: TP3_DUT`, `6: TP3_DUT`, `7: TP3_DUT`, `8: TP3_DUT`, `9: TP3_DUT`, `10: TP3_DUT`, `11: TP2_DUT`, `12: TP1_DUT`, `13: TP1_DUT`, `14: TP1_DUT` |
| **`J_DUT3`** | `Conn_1x03` | `DUT_3PIN_SKT` (Machined 3-Pin TO-18/TO-92 Socket) | `Socket_1x03_DUT` | Machined Gold Collet, $2.54\text{ mm}$ Pitch | `1: TP1_DUT`, `2: TP2_DUT`, `3: TP3_DUT` |
| **`J_KELVIN`** | `Conn_1x04` | `KELVIN_LEADS` (4-Pin Alligator / Micro-Grabber Header) | `Header_1x04_P2.54mm` | $2.54\text{ mm}$ Vertical Header | `1: TP1_DUT`, `2: TP2_DUT`, `3: TP3_DUT`, `4: AGND` |
| **`J_TERM5`** | `TermBlock_5Pin` | `PROBE_5P_508` (5-Pin $5.08\text{ mm}$ Phoenix Screw Terminal) | `TerminalBlock_5Pin_508` | $15\text{ A}$, $300\text{ V}$ Screw Clamps | `1(TP1): TP1_DUT`, `2(TP2): TP2_DUT`, `3(TP3): TP3_DUT`, `4(COM): AGND`, `5(V30V): DMM_30V` |
| **`J_TEMP`** | `DS18B20_Socket` | `DS18B20_TEMP` (1-Wire Digital Thermometer next to ZIF) | `Socket_TO92_3Pin` | $\pm 0.5^\circ\text{C}$ Accuracy, TO-92 in 3-Pin Socket | `1(GND): AGND`, `2(DQ): TEMP_DQ`, `3(VDD): +3V3_ANA` |
| **`R_TEMP`** | `R_THT` | `4.7k` (`4.70 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: +3V3_ANA`, `2: TEMP_DQ` (1-Wire bus pull-up) |

---

## 3. Block 6 Netlist — `ADS1115` Precision Sensing, `MCP4822` Force DAC, `LMC6484IN` Buffer & `DG409DJ` Cal/HV Mux

### 3.1 12-Bit Dual SPI Force DAC & `4.480x` Rail-to-Rail Output Driver (`U9` + `U5`)

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U9`** | `MCP4822` | `MCP4822-E/P` (Dual 12-Bit SPI DAC, $2.048\text{ V}$ Int $V_{\text{REF}}$) | `Socket_DIP-8_W7.62mm` | 12-Bit, $50\text{ ppm}/^\circ\text{C}$ $V_{\text{REF}}$, Machined DIP-8 | `1(VDD): +3V3_ANA`, `2(CS): DAC_CS`, `3(SCK): AFE_SPI_SCK`, `4(SDI): AFE_SPI_MOSI`, `5(LDAC): AGND`, `6(VOUTB): DAC2_RAW`, `7(VSS): AGND`, `8(VOUTA): DAC1_RAW` |
| **`C19`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +3V3_ANA`, `2: AGND` (Local bypass for `U9` Pin 1) |
| **`R_DACCS`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: +3V3_ANA`, `2: DAC_CS` (Deselects `U9` during MCU boot) |
| **`R_DAC1`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: DAC1_RAW`, `2: DAC1_FILT` ($f_c = 159.15\text{ Hz}$ LPF with `C_DAC1`) |
| **`C_DAC1`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: DAC1_FILT`, `2: AGND` |
| **`R_DP1`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: DAC1_FILT`, `2: AGND` (Boot $0.000\text{ V}$ anchor) |
| **`R_DAC2`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: DAC2_RAW`, `2: DAC2_FILT` ($f_c = 159.15\text{ Hz}$ LPF with `C_DAC2`) |
| **`C_DAC2`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: DAC2_FILT`, `2: AGND` |
| **`R_DP2`** | `R_THT` | `100k` (`100 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: DAC2_FILT`, `2: AGND` (Boot $0.000\text{ V}$ anchor) |
| **`U5`** | `TLC2272ACP` | `TLC2272ACP` (Dual Rail-to-Rail Output LinCMOS Op-Amp) | `Socket_DIP-8_W7.62mm` | $I_{IB} \approx 1\text{ pA}$, $+12\text{V}$ Single Supply, DIP-8 | `1(1OUT): V_SRC_A`, `2(1IN-): FB_SRCA`, `3(1IN+): DAC1_FILT`, `4(GND): AGND`, `5(2IN+): DAC2_FILT`, `6(2IN-): FB_SRCB`, `7(2OUT): V_SRC_B`, `8(VCC): +12V_A` |
| **`C10`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +12V_A`, `2: AGND` (Local bypass for `U5` Pin 8) |
| **`R_G1A`** | `R_THT` | `34.8k_0.1%` (`34.8 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: V_SRC_A`, `2: FB_SRCA` (Feedback top resistor, $G = 4.480\times$) |
| **`R_G1B`** | `R_THT` | `10.0k_0.1%` (`10.0 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: FB_SRCA`, `2: AGND` (Feedback bottom resistor, $G = 4.480\times$) |
| **`R_G2A`** | `R_THT` | `34.8k_0.1%` (`34.8 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: V_SRC_B`, `2: FB_SRCB` (Feedback top resistor, $G = 4.480\times$) |
| **`R_G2B`** | `R_THT` | `10.0k_0.1%` (`10.0 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: FB_SRCB`, `2: AGND` (Feedback bottom resistor, $G = 4.480\times$) |

---

### 3.2 Live Calibration & Protected $0\text{–}33\text{ V}$ HV DMM Multiplexer (`U10 DG409DJ` + `11:1` Attenuator)

> [!NOTE]
> **Canonical `U10` Hardware Address Mapping (per `gate_1_audit.md` §8 Item 4):** `CAL_A1:CAL_A0` (`GPIO33:GPIO32`) selects `0 (00b) = V_SRC_A` (`S1A` Pin 4), `1 (01b) = V_SRC_B` (`S2A` Pin 5), `2 (10b) = +9V_LIM` (`S3A` Pin 6), and `3 (11b) = DMM_30V_DIV` (`S4A` Pin 7).

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U10`** | `DG409DJ` | `DG409DJ` (Dual 4:1 CMOS Analog Multiplexer) | `Socket_DIP-16_W7.62mm` | $I_{\text{off}} \approx 20\text{ pA}$ typ, $+12\text{V}$ Supply, DIP-16 | `1(A0): CAL_A0`, `2(EN): +3V3_ANA`, `3(V-): AGND`, `4(S1A): V_SRC_A`, `5(S2A): V_SRC_B`, `6(S3A): +9V_LIM`, `7(S4A): DMM_30V_DIV`, `8(DA): CAL_MUX_OUT`, `9(DB): AGND`, `10(S4B): AGND`, `11(S3B): AGND`, `12(S2B): AGND`, `13(S1B): AGND`, `14(V+): +12V_A`, `15(GND): AGND`, `16(A1): CAL_A1` |
| **`C20`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +12V_A`, `2: AGND` (Local bypass for `U10` Pin 14) |
| **`R_HV1`** | `R_THT` | `100k_0.1%` (`100.0 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: DMM_30V`, `2: DMM_30V_DIV` ($11:1$ HV DMM top attenuator resistor) |
| **`R_HV2`** | `R_THT` | `10.0k_0.1%` (`10.0 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: DMM_30V_DIV`, `2: AGND` ($11:1$ HV DMM bottom attenuator resistor) |
| **`D_HVH`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$, $150\text{V}$ | `1(K): +12V_A`, `2(A): DMM_30V_DIV` (Clamps `DMM_30V_DIV` $\le +12.65\text{ V}$) |
| **`D_HVL`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | $I_R \le 1.0\text{ nA}$, $150\text{V}$ | `1(K): DMM_30V_DIV`, `2(A): AGND` (Clamps `DMM_30V_DIV` $\ge -0.65\text{ V}$) |

---

### 3.3 Quad 20 fA Ultra-High-Z Sense Buffer (`U4 LMC6484IN`), `4:1` Dividers (`1.50k / 499R 0.1%`), C0G Filters & `MOD_ADS` (`ADS1115`)

| RefDes | Symbol (`Ge_Engine`) | Value / Part Number | THT Footprint (`Ge_Engine`) | Tolerance / Rating | Pin-by-Pin Net Connections (`Pin: Net`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`U4`** | `LMC6484IN` | `LMC6484IN` (Quad CMOS Rail-to-Rail I/O Op-Amp, $20\text{ fA}$ $I_{IB}$) | `Socket_DIP-14_W7.62mm` | $I_{IB} = 20\text{ fA}$ typ, $+12\text{V}$ Single Supply, DIP-14 | `1(1OUT): BUF_TP1`, `2(1IN-): BUF_TP1`, `3(1IN+): ESD_TP1`, `4(VDD): +12V_A`, `5(2IN+): ESD_TP2`, `6(2IN-): BUF_TP2`, `7(2OUT): BUF_TP2`, `8(3OUT): BUF_TP3`, `9(3IN-): BUF_TP3`, `10(3IN+): ESD_TP3`, `11(GND): AGND`, `12(4IN+): CAL_MUX_OUT`, `13(4IN-): BUF_CAL`, `14(4OUT): BUF_CAL` |
| **`C11`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +12V_A`, `2: AGND` (Local bypass for `U4` Pin 4) |
| **`R_ESD1`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: TP1_DUT`, `2: ESD_TP1` (Buffer input ESD current limiter, $0.2\text{ nV}$ drop @ $20\text{ fA}$) |
| **`R_ESD2`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: TP2_DUT`, `2: ESD_TP2` |
| **`R_ESD3`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: TP3_DUT`, `2: ESD_TP3` |
| **`R_D1A`** | `R_THT` | `1.50k_0.1%` (`1.50 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: BUF_TP1`, `2: ADC_TP1` ($4.006012:1$ precision divider top, Ch 1) |
| **`R_D1B`** | `R_THT` | `499R_0.1%` (`499 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: ADC_TP1`, `2: AGND` ($4.006012:1$ precision divider bottom, Ch 1) |
| **`C_A0`** | `C_THT` | `100nF_C0G` (`0.1 µF` C0G/NP0) | `C_Disc_P5.00mm` | `5%`, `50V`, **Zero-Piezo C0G/NP0** | `1: ADC_TP1`, `2: AGND` ($f_c = 4.251\text{ kHz}$ anti-aliasing filter, Ch 1) |
| **`R_ISO1`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: ADC_TP1`, `2: FAST_TP1` (Fast RC timing tap to `U_MCU1 GPIO36`) |
| **`R_D2A`** | `R_THT` | `1.50k_0.1%` (`1.50 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: BUF_TP2`, `2: ADC_TP2` ($4.006012:1$ precision divider top, Ch 2) |
| **`R_D2B`** | `R_THT` | `499R_0.1%` (`499 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: ADC_TP2`, `2: AGND` ($4.006012:1$ precision divider bottom, Ch 2) |
| **`C_A1`** | `C_THT` | `100nF_C0G` (`0.1 µF` C0G/NP0) | `C_Disc_P5.00mm` | `5%`, `50V`, **Zero-Piezo C0G/NP0** | `1: ADC_TP2`, `2: AGND` ($f_c = 4.251\text{ kHz}$ anti-aliasing filter, Ch 2) |
| **`R_ISO2`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: ADC_TP2`, `2: FAST_TP2` (Fast RC timing tap to `U_MCU1 GPIO39`) |
| **`R_D3A`** | `R_THT` | `1.50k_0.1%` (`1.50 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: BUF_TP3`, `2: ADC_TP3` ($4.006012:1$ precision divider top, Ch 3) |
| **`R_D3B`** | `R_THT` | `499R_0.1%` (`499 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: ADC_TP3`, `2: AGND` ($4.006012:1$ precision divider bottom, Ch 3) |
| **`C_A2`** | `C_THT` | `100nF_C0G` (`0.1 µF` C0G/NP0) | `C_Disc_P5.00mm` | `5%`, `50V`, **Zero-Piezo C0G/NP0** | `1: ADC_TP3`, `2: AGND` ($f_c = 4.251\text{ kHz}$ anti-aliasing filter, Ch 3) |
| **`R_ISO3`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: ADC_TP3`, `2: FAST_TP3` (Fast RC timing tap to `U_MCU1 GPIO35`) |
| **`R_D4A`** | `R_THT` | `1.50k_0.1%` (`1.50 kΩ`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: BUF_CAL`, `2: ADC_CAL` ($4.006012:1$ precision divider top, Cal/HV Ch 4) |
| **`R_D4B`** | `R_THT` | `499R_0.1%` (`499 Ω`) | `R_Axial_P7.62mm` | **`0.1%`, `0.25W`**, $\le 25\text{ ppm}/^\circ\text{C}$ | `1: ADC_CAL`, `2: AGND` ($4.006012:1$ precision divider bottom, Cal/HV Ch 4) |
| **`C_A3`** | `C_THT` | `100nF_C0G` (`0.1 µF` C0G/NP0) | `C_Disc_P5.00mm` | `5%`, `50V`, **Zero-Piezo C0G/NP0** | `1: ADC_CAL`, `2: AGND` ($f_c = 4.251\text{ kHz}$ anti-aliasing filter, Cal/HV Ch 4) |
| **`MOD_ADS`** | `ADS1115_Module` | `ADS1115_16Bit` (16-Bit I2C Delta-Sigma ADC Breakout) | `Socket_ADS1115_Module` | 16-Bit, PGA $\pm 0.256\text{V}..\pm 4.096\text{V}$, `1x10` Female Socket | `1(VDD): +3V3_ADC`, `2(GND): AGND`, `3(SCL): ANA_I2C_SCL`, `4(SDA): ANA_I2C_SDA`, `5(ADDR): AGND` (`0x48`), `6(ALRT): ADS_ALRT`, `7(A0): ADC_TP1`, `8(A1): ADC_TP2`, `9(A2): ADC_TP3`, `10(A3): ADC_CAL` |
| **`C9`** | `C_THT` | `100nF` (`0.1 µF`) | `C_Disc_P5.00mm` | `10%`, `50V`, X7R Ceramic | `1: +3V3_ADC`, `2: AGND` (Local bypass at `MOD_ADS` Pin 1) |
| **`R_SDA`** | `R_THT` | `4.7k` (`4.70 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: +3V3_ANA`, `2: ANA_I2C_SDA` (Dedicated Analog I2C SDA pull-up) |
| **`R_SCL`** | `R_THT` | `4.7k` (`4.70 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: +3V3_ANA`, `2: ANA_I2C_SCL` (Dedicated Analog I2C SCL pull-up) |
| **`R_ALRT`** | `R_THT` | `10k` (`10.0 kΩ`) | `R_Axial_P7.62mm` | `1%`, `0.25W` Metal Film | `1: +3V3_ANA`, `2: ADS_ALRT` (Pull-up for open-drain `ALRT/RDY` into `GPIO34`) |

---

## 4. Block 5 & Block 6 Component Summary & Verification Audit

- **Total Active / Socketed ICs & Modules in Blocks 5 & 6:** `10` (`U1, U2, U3 DG408DJ`, `U8 DG412LDJ`, `U6, U7 74HC595N`, `U9 MCP4822-E/P`, `U5 TLC2272ACP`, `U10 DG409DJ`, `U4 LMC6484IN`, plus `MOD_ADS ADS1115_Module` and `J_TEMP DS18B20`).
- **Total `0.1%` Precision Metal-Film Resistors in Blocks 5 & 6:** `35`
  - `3x` `100 Ω 0.5W 0.1%` (`R_PR1..3`)
  - `6x` `470 Ω 0.25W 0.1%` (`R_LA1..3`, `R_LG1..3`)
  - `6x` `2.49 kΩ 0.25W 0.1%` (`R_KA1..3`, `R_KG1..3`)
  - `4x` `100.0 kΩ 0.25W 0.1%` (`R_MB1..3`, `R_HV1`)
  - `3x` `2.20 MΩ 0.25W 0.1%` (`R_KB1..3`)
  - `2x` `34.8 kΩ 0.25W 0.1%` (`R_G1A`, `R_G2A`)
  - `3x` `10.0 kΩ 0.25W 0.1%` (`R_G1B`, `R_G2B`, `R_HV2`)
  - `4x` `1.50 kΩ 0.25W 0.1%` (`R_D1A..R_D4A`)
  - `4x` `499 Ω 0.25W 0.1%` (`R_D1B..R_D4B`)
- **Total Ultra-Low-Leakage `<1 nA` Clamps (`1N3595` DO-35):** `8` (`D_CH1..3`, `D_CL1..3`, `D_HVH`, `D_HVL`).
- **Total Decoupling & Filter Capacitors in Blocks 5 & 6:** `17` (`10x 100nF X7R` bypass `C9..C16, C19..C21`; `4x 100nF C0G` anti-aliasing `C_A0..C_A3`; `2x 100nF X7R` DAC LPF `C_DAC1..2`; `1x 1uF X7R` POR `C_POR`).
- **Unconnected / Ambiguous Pins:** **`0`** (Every unused mux channel on `U8` and `U10` is explicitly tied to `AGND`; `U7` Pin 9 cascade output `QH'` is explicitly marked `NC`).
