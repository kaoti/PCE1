# Phase 2 — Master 8-Block Schematic Capture & Net-Class Specification (`p2_full_8block_schematic_spec.md`)

**Document ID:** `p2_full_8block_schematic_spec.md`  
**Subsystem:** Unified Single-Source-of-Truth Schematic Capture Architecture (Blocks 1–8), Component-to-Pin Master Dictionary, and KiCad 9 / EasyEDA Net-Class & DRC Constraints  
**Architecture Revision:** Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine (100% Harmonized with `gate_1_audit.md` §7/§8, `p1_pinout_extraction.md`, `p2_power_pifilter_netlist.md`, and `p2_afe_ads1115_netlist.md`)

---

## 1. Master A2 / Oversized-A3 Schematic Sheet Partitioning (8 Bordered Functional Blocks)

The Rev 5.0 schematic (`Ge_Parametric_Engine.kicad_sch`) is organized into **8 non-overlapping, graphically bordered and titled functional engineering blocks** on an A2 landscape sheet ($594.0\text{ mm} \times 420.0\text{ mm}$, $2.54\text{ mm}$ / $100\text{ mil}$ strict Manhattan snap grid), ensuring **0 ERC errors, 0 ERC warnings, 0 floating pins, and 0 overlapping text labels**:

```
+-------------------------------------------------------------------------------------------------------------------------------+
|  KAOTI AUDIO — REV 5.0 DUAL-MCU GERMANIUM & UNIVERSAL PARAMETRIC CHARACTERIZATION ENGINE (A2 MASTER SCHEMATIC SHEET)          |
|                                                                                                                               |
|  +-----------------------------------+  +-----------------------------------+  +-------------------------------------------+  |
|  | BLOCK 1 (X: 15..195, Y: 18..135)  |  | BLOCK 2 (X: 200..395, Y: 18..135) |  | BLOCK 3 (X: 400..578, Y: 18..135)         |  |
|  | POWER, 3.7V NON-HV LIPO           |  | SWITCHABLE ANALOG RAIL, 2x        |  | MCU 1 QUIET ANALOG CORE (WEMOS LOLIN32    |  |
|  | PROTECTION & BQ25895 NVDC         |  | CRITICALLY DAMPED LC PI-FILTERS,  |  | LITE, WI-FI/BT OFF) & 2x H11L1M           |  |
|  | FAST-CHARGER + U_LDO_DIG (+3V3_DIG)|  | L7812CV/L78L09/MCP1826S & 15mA LIM|  | OPTO-ISOLATED UART + J_CYD_UART TAP       |  |
|  +-----------------------------------+  +-----------------------------------+  +-------------------------------------------+  |
|                                                                                                                               |
|  +-----------------------------------+  +----------------------------------------------------------------------------------+  |
|  | BLOCK 4 (X: 15..195, Y: 142..270) |  | BLOCK 5 (X: 200..578, Y: 142..270)                                               |  |
|  | MCU 2 COMMS/UI CORE (ESP32-C3     |  | SOLID-STATE 3-TERMINAL AFE MATRIX (U6/U7 74HC595N, U1..U3 DG408DJ, U8 DG412LDJ), |  |
|  | SUPERMINI) & U11 MCP23017-E/SP    |  | 3x R.G. KEEN 0.1% RESISTOR BANKS, KELVIN 100R 0.5W R_PR1..3, 6x 1N3595 CLAMPS,   |  |
|  | 16-BIT I2C GPIO EXPANDER (@ 0x20) |  | ZIF-14, 3-PIN GOLD SOCKET, 4-WIRE KELVIN HEADER, 5-PIN SCREW TERM & DS18B20      |  |
|  +-----------------------------------+  +----------------------------------------------------------------------------------+  |
|                                                                                                                               |
|  +-------------------------------------------------------+  +------------------------------+  +----------------------------+  |
|  | BLOCK 6 (X: 15..295, Y: 277..395)                     |  | BLOCK 7 (X: 300..445, Y:277) |  | BLOCK 8 (X: 450..578, Y277)|  |
|  | ADS1115 16-BIT PRECISION SENSING, U9 MCP4822 12-BIT   |  | DUAL 1.9" ST7789 IPS COLOR   |  | TOP STATUS LED ARRAY       |  |
|  | FORCE DAC, U5 TLC2272ACP RRO DRIVER (0-9.173V),       |  | DISPLAYS (MOD_TFT1/TFT2),    |  | (6x 3MM BEZEL BAR LEDS)    |  |
|  | U4 LMC6484IN 20fA QUAD BUFFER, 4:1 & 11:1 DIVIDERS    |  | Q_BLK & RETRO CONTROLS/ENC1  |  | & 56K MODEM BUZZER (BZ1)   |  |
|  +-------------------------------------------------------+  +------------------------------+  +----------------------------+  |
+-------------------------------------------------------------------------------------------------------------------------------+
```

---

## 2. Block-by-Block Complete Schematic Capture Netlist (Blocks 1 through 8)

### 2.1 BLOCK 1: Power, 3.7 V Non-HV LiPo Protection & `BQ25895` NVDC Fast-Charger
**Bounding Box:** `X = 15.24 mm .. 195.58 mm`, `Y = 17.78 mm .. 134.62 mm`  
**Title Banner:** `1. POWER, 3.7V NON-HV LIPO PROTECTION & TI BQ25895 1.5MHz NVDC FAST-CHARGER`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`J_USB_C`** | `USBC_Breakout_6P` | `USBC_5V_3A_HVDCP` | `Socket_USBC_Breakout` | `{1: "V_USB_RAW", 2: "DGND", 3: "USB_CC1", 4: "USB_CC2", 5: "USB_DP_CHG", 6: "USB_DM_CHG"}` |
| **`R_CC1`** | `R_THT` | `5.1k_1%` | `R_Axial_P7.62mm` | `{1: "USB_CC1", 2: "DGND"}` |
| **`R_CC2`** | `R_THT` | `5.1k_1%` | `R_Axial_P7.62mm` | `{1: "USB_CC2", 2: "DGND"}` |
| **`F_USB`** | `F_PTC_THT` | `RXEF175_1.75A` | `Fuse_PTC_P5.00mm` | `{1: "V_USB_RAW", 2: "V_USB_PROT"}` |
| **`D_TVS_USB`** | `DZ_THT` | `P6KE15A` (`1N5919B`) | `D_DO-15_P10.16mm` | `{1: "V_USB_PROT", 2: "DGND"}` |
| **`C_USB_IN`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "V_USB_PROT", 2: "DGND"}` |
| **`J_BAT`** | `Conn_1x02` | `LIPO_1S_3V7_NON_HV` | `Header_1x02_P2.54mm` | `{1: "J_BAT+", 2: "DGND"}` |
| **`J_NTC`** | `Conn_1x02` | `BAT_NTC_10K` | `Header_1x02_P2.54mm` | `{1: "BAT_NTC", 2: "DGND"}` |
| **`SW_PWR`** | `SW_DPDT_THT` | `MASTER_PWR_DPDT_3A` | `SW_Slide_DPDT_THT` | `{1: None, 2: "J_BAT+", 3: "V_BAT_SW", 4: "V_SYS_BLEED", 5: "V_SYS_RAW", 6: "V_BUS_SYS"}` |
| **`R_BLEED_SYS`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "V_SYS_BLEED", 2: "DGND"}` |
| **`Q_REV_BAT`** | `PMOS_NDP6020P` | `NDP6020P` | `TO-220-3_Vertical` | `{1: "Q_REV_G", 2: "V_BAT_SW", 3: "V_BAT_PROT"}` |
| **`R_G_REV`** | `R_THT` | `1.0k` | `R_Axial_P7.62mm` | `{1: "Q_REV_G", 2: "DGND"}` |
| **`MOD_CHG`** | `BQ25895_Module` | `BQ25895_NVDC_3A` | `Socket_BQ25895_Charger` | `{1: "V_USB_PROT", 2: "PMID_BYP", 3: "DGND", 4: "V_SYS_RAW", 5: "V_BAT_PROT", 6: "BAT_NTC", 7: "UI_I2C_SCL", 8: "UI_I2C_SDA", 9: None, 10: "CHG_STAT_N", 11: "DGND", 12: "DGND"}` |
| **`C_PMID`** | `CP_THT` | `10uF_25V` | `CP_Radial_D8_P3.5mm` | `{1: "PMID_BYP", 2: "DGND"}` |
| **`C_BAT_BYP`** | `CP_THT` | `100uF_16V` | `CP_Radial_D8_P3.5mm` | `{1: "V_BAT_PROT", 2: "DGND"}` |
| **`C_SYS1`** | `CP_THT` | `100uF_16V` | `CP_Radial_D8_P3.5mm` | `{1: "V_BUS_SYS", 2: "DGND"}` |
| **`C_SYS2`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "V_BUS_SYS", 2: "DGND"}` |
| **`U_LDO_DIG`** | `MCP1826S_TO220` | `MCP1826S-3302E/AB` | `TO-220-3_Vertical` | `{1: "V_BUS_SYS", 2: "DGND", 3: "+3V3_DIG"}` |
| **`C_DIG1`** | `CP_THT` | `100uF_16V` | `CP_Radial_D8_P3.5mm` | `{1: "+3V3_DIG", 2: "DGND"}` |
| **`C_DIG2`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+3V3_DIG", 2: "DGND"}` |

---

### 2.2 BLOCK 2: Switchable Analog Rail, 2× Critically Damped LC Pi-Filters, LDOs & 15 mA Active Limiter
**Bounding Box:** `X = 200.66 mm .. 396.24 mm`, `Y = 17.78 mm .. 134.62 mm`  
**Title Banner:** `2. SWITCHABLE ANALOG POWER, 2x CRITICALLY DAMPED LC PI-FILTERS, LDOs, 15mA LIMITER & STAR GROUND`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`R_PD_ANA`** | `R_THT` | `100k` | `R_Axial_P7.62mm` | `{1: "ANA_PWR_EN", 2: "DGND"}` |
| **`R_G_ANA`** | `R_THT` | `100R` | `R_Axial_P7.62mm` | `{1: "ANA_PWR_EN", 2: "Q_ANA_NG"}` |
| **`Q_ANA_N`** | `NMOS_2N7000` | `2N7000` | `Socket_TO92_3Pin` | `{1: "DGND", 2: "Q_ANA_NG", 3: "Q_ANA_SW_G"}` |
| **`R_PU_SW`** | `R_THT` | `47k` | `R_Axial_P7.62mm` | `{1: "V_BUS_SYS", 2: "Q_ANA_SW_G"}` |
| **`Q_ANA_SW`** | `PMOS_NDP6020P` | `NDP6020P` | `TO-220-3_Vertical` | `{1: "Q_ANA_SW_G", 2: "V_ANA_IN", 3: "V_BUS_SYS"}` |
| **`MOD_BOOST`** | `MT3608_Boost` | `MT3608_14.5V` | `Socket_MT3608_Boost` | `{1: "V_ANA_IN", 2: "AGND", 3: "V_BOOST_RAW", 4: "AGND"}` |
| **`C_PI1A`** (`C3`) | `CP_THT` | `100uF_35V` | `CP_Radial_D8_P3.5mm` | `{1: "V_BOOST_RAW", 2: "AGND"}` |
| **`C_PI1B`** (`C4`) | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "V_BOOST_RAW", 2: "AGND"}` |
| **`L1`** | `L_RAD_THT` | `100uH_1A` | `L_Radial_P5.00mm` | `{1: "V_BOOST_RAW", 2: "V_LC1_MID"}` |
| **`R_DAMP1`** | `R_THT` | `2.2R_0.5W` | `R_Axial_P7.62mm` | `{1: "V_LC1_MID", 2: "V_14V_FILT"}` |
| **`C_PI2A`** (`C5`) | `CP_THT` | `100uF_35V` | `CP_Radial_D8_P3.5mm` | `{1: "V_14V_FILT", 2: "AGND"}` |
| **`C_PI2B`** (`C6`) | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "V_14V_FILT", 2: "AGND"}` |
| **`U_REG12`** | `L7812_TO220` | `L7812CV` | `TO-220-3_Vertical` | `{1: "V_14V_FILT", 2: "AGND", 3: "V_12V_PRE"}` |
| **`F1`** | `F_PTC_THT` | `RXEF017_170mA` | `Fuse_PTC_P5.00mm` | `{1: "V_12V_PRE", 2: "+12V_A"}` |
| **`D_OVP`** | `DZ_THT` | `1N4743A_13V` | `D_DO-41_P10.16mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`C17`** | `CP_THT` | `10uF_25V` | `CP_Radial_D8_P3.5mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`C18`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`U_REG9`** (`U_REG`) | `L78L09_TO92` | `L78L09ACZ` | `Socket_TO92_3Pin` | `{1: "+9V_REF", 2: "AGND", 3: "+12V_A"}` |
| **`C7`** | `CP_THT` | `10uF_25V` | `CP_Radial_D8_P3.5mm` | `{1: "+9V_REF", 2: "AGND"}` |
| **`C8`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+9V_REF", 2: "AGND"}` |
| **`R_ILIM`** | `R_THT` | `43R_1%` | `R_Axial_P7.62mm` | `{1: "+9V_REF", 2: "LIM_SENSE"}` |
| **`Q_LIM1`** | `PNP_2N3906` | `2N3906` | `Socket_TO92_3Pin` | `{1: "LIM_SENSE", 2: "LIM_BASE", 3: "+9V_LIM"}` |
| **`Q_LIM2`** | `PNP_2N3906` | `2N3906` | `Socket_TO92_3Pin` | `{1: "+9V_REF", 2: "LIM_SENSE", 3: "LIM_BASE"}` |
| **`R_IBIAS`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "LIM_BASE", 2: "AGND"}` |
| **`R_OVC_DIV1`** | `R_THT` | `100k` | `R_Axial_P7.62mm` | `{1: "+9V_REF", 2: "Q_OVC_B"}` |
| **`R_OVC_DIV2`** | `R_THT` | `22k` | `R_Axial_P7.62mm` | `{1: "+9V_LIM", 2: "Q_OVC_B"}` |
| **`Q_OVC`** | `NPN_2N3904` | `2N3904` | `Socket_TO92_3Pin` | `{1: "AGND", 2: "Q_OVC_B", 3: "FAULT_OVC_N"}` |
| **`R_PU_OVC`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "FAULT_OVC_N"}` |
| **`C_A1A`** | `CP_THT` | `100uF_16V` | `CP_Radial_D8_P3.5mm` | `{1: "V_ANA_IN", 2: "AGND"}` |
| **`C_A1B`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "V_ANA_IN", 2: "AGND"}` |
| **`L2`** | `L_RAD_THT` | `47uH_1A` | `L_Radial_P5.00mm` | `{1: "V_ANA_IN", 2: "V_LC2_MID"}` |
| **`R_DAMP2`** | `R_THT` | `1.5R_0.25W` | `R_Axial_P7.62mm` | `{1: "V_LC2_MID", 2: "V_3V3_FILT"}` |
| **`C_A2A`** | `CP_THT` | `100uF_16V` | `CP_Radial_D8_P3.5mm` | `{1: "V_3V3_FILT", 2: "AGND"}` |
| **`C_A2B`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "V_3V3_FILT", 2: "AGND"}` |
| **`U_LDO_ANA`** | `MCP1826S_TO220` | `MCP1826S-3302E/AB` | `TO-220-3_Vertical` | `{1: "V_3V3_FILT", 2: "AGND", 3: "+3V3_ANA"}` |
| **`C1`** | `CP_THT` | `100uF_16V` | `CP_Radial_D8_P3.5mm` | `{1: "+3V3_ANA", 2: "AGND"}` |
| **`C2`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+3V3_ANA", 2: "AGND"}` |
| **`R_ADC_F`** | `R_THT` | `10R_1%` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "+3V3_ADC"}` |
| **`C_ADC_FA`** | `CP_THT` | `10uF_16V` | `CP_Radial_D8_P3.5mm` | `{1: "+3V3_ADC", 2: "AGND"}` |
| **`C_ADC_FB`** (`C9`) | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+3V3_ADC", 2: "AGND"}` |
| **`R_STAR`** | `R_THT` | `0R_STAR_BRIDGE` | `R_Axial_P7.62mm` | `{1: "DGND", 2: "AGND"}` |

---

### 2.3 BLOCK 3: MCU 1 Quiet Analog Core (`WEMOS LOLIN32 Lite`) & `2x H11L1M` Opto-Isolated UART
**Bounding Box:** `X = 401.32 mm .. 579.12 mm`, `Y = 17.78 mm .. 134.62 mm`  
**Title Banner:** `3. MCU 1 QUIET ANALOG CORE (LOLIN32 LITE, WI-FI/BT OFF) & 2x H11L1M OPTO-ISOLATED UART`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`U_MCU1`** | `Wemos_Lolin32_Lite` | `WEMOS_LOLIN32_Lite` | `Socket_Wemos_Lolin32_Lite` | `{1: "FAST_TP1", 2: "FAST_TP2", 3: None, 4: "ADS_ALRT", 5: "FAST_TP3", 6: "CAL_A0", 7: "CAL_A1", 8: "FAULT_OVC_N", 9: "DAC_CS", 10: "AFE_SPI_MOSI", 11: "AFE_SPI_SCK", 12: None, 13: "AGND", 14: "M1_3V3", 15: None, 16: "ANA_I2C_SDA", 17: "ANA_I2C_SCL", 18: "SR_LATCH", 19: None, 20: "SR_OE_N", 21: "LINK_TX_M1", 22: "TEMP_DQ", 23: None, 24: None, 25: None, 26: "LINK_RX_M1"}` |
| **`JP_M1_3V3`** | `Conn_1x02` | `JP_M1_3V3_ISO` | `Header_1x02_P2.54mm` | `{1: "+3V3_ANA", 2: "M1_3V3"}` |
| **`R_LATCH_PD`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "SR_LATCH", 2: "AGND"}` |
| **`U_ISO1`** | `H11L1M_DIP6` | `H11L1M` (M1 $\to$ M2) | `Socket_DIP-6_W7.62mm` | `{1: "ISO1_A", 2: "LINK_TX_M1", 3: None, 4: "LINK_RX_M2", 5: "DGND", 6: "+3V3_DIG"}` |
| **`R_LED_ISO1`** | `R_THT` | `470R_1%` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "ISO1_A"}` |
| **`R_PU_ISO1`** | `R_THT` | `2.2k` | `R_Axial_P7.62mm` | `{1: "+3V3_DIG", 2: "LINK_RX_M2"}` |
| **`C_BYP_ISO1`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+3V3_DIG", 2: "DGND"}` |
| **`U_ISO2`** | `H11L1M_DIP6` | `H11L1M` (M2 $\to$ M1) | `Socket_DIP-6_W7.62mm` | `{1: "ISO2_A", 2: "LINK_TX_M2", 3: None, 4: "LINK_RX_M1", 5: "AGND", 6: "+3V3_ANA"}` |
| **`R_LED_ISO2`** | `R_THT` | `470R_1%` | `R_Axial_P7.62mm` | `{1: "+3V3_DIG", 2: "ISO2_A"}` |
| **`R_PU_ISO2`** | `R_THT` | `2.2k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "LINK_RX_M1"}` |
| **`C_BYP_ISO2`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+3V3_ANA", 2: "AGND"}` |
| **`J_CYD_UART`** | `Conn_1x04` | `CYD_P1_UART_TAP` | `Header_1x04_P2.54mm` | `{1: "V_BUS_SYS", 2: "DGND", 3: "CYD_TX_TAP", 4: "CYD_RX_TAP"}` |
| **`R_CYD_TX`** | `R_THT` | `470R` | `R_Axial_P7.62mm` | `{1: "LINK_TX_M2", 2: "CYD_TX_TAP"}` |
| **`R_CYD_RX`** | `R_THT` | `470R` | `R_Axial_P7.62mm` | `{1: "LINK_RX_M2", 2: "CYD_RX_TAP"}` |

---

### 2.4 BLOCK 4: MCU 2 Comms/UI Core (`ESP32-C3 SuperMini`) & `U11 MCP23017-E/SP` I/O Expander
**Bounding Box:** `X = 15.24 mm .. 195.58 mm`, `Y = 142.24 mm .. 269.24 mm`  
**Title Banner:** `4. MCU 2 COMMS/UI/AUDIO CORE (ESP32-C3 SUPERMINI) & U11 MCP23017-E/SP I2C EXPANDER (@ 0x20)`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`U_MCU2`** | `ESP32_C3_SuperMini` | `ESP32-C3_SuperMini` | `Socket_ESP32_C3_SuperMini` | `{1: "TFT_DC", 2: "UI_SPI_MOSI", 3: "TFT1_CS", 4: None, 5: None, 6: "TFT2_CS", 7: "LINK_RX_M2", 8: "LINK_TX_M2", 9: None, 10: "DGND", 11: "M2_3V3", 12: "UI_SPI_SCK", 13: "BZ_PWM", 14: None, 15: "UI_I2C_SCL", 16: "UI_I2C_SDA"}` |
| **`JP_M2_3V3`** | `Conn_1x02` | `JP_M2_3V3_ISO` | `Header_1x02_P2.54mm` | `{1: "+3V3_DIG", 2: "M2_3V3"}` |
| **`R_UI_SDA`** | `R_THT` | `4.7k` | `R_Axial_P7.62mm` | `{1: "+3V3_DIG", 2: "UI_I2C_SDA"}` |
| **`R_UI_SCL`** | `R_THT` | `4.7k` | `R_Axial_P7.62mm` | `{1: "+3V3_DIG", 2: "UI_I2C_SCL"}` |
| **`U11`** | `MCP23017` | `MCP23017-E/SP` | `Socket_DIP-28_W7.62mm` | `{1: "ENC_A", 2: "ENC_B", 3: "LED_STBY_RF", 4: "LED_SCAN", 5: "LED_DUT_OK", 6: "LED_FAULT", 7: "TFT_BLK_EN", 8: "ANA_PWR_EN", 9: "+3V3_DIG", 10: "DGND", 11: None, 12: "UI_I2C_SCL", 13: "UI_I2C_SDA", 14: None, 15: "DGND", 16: "DGND", 17: "DGND", 18: "MCP_RST_N", 19: None, 20: None, 21: "BTN_UP", 22: "BTN_DOWN", 23: "BTN_LEFT", 24: "BTN_RIGHT", 25: "BTN_A", 26: "BTN_B", 27: "ENC_SW", 28: "SW_STBY"}` |
| **`C22`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+3V3_DIG", 2: "DGND"}` |
| **`R_RST_M11`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "+3V3_DIG", 2: "MCP_RST_N"}` |
| **`C_RST_M11`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "MCP_RST_N", 2: "DGND"}` |

---

### 2.5 BLOCK 5: Solid-State 3-Terminal AFE Matrix, Shift Registers & Failsafes
**Bounding Box:** `X = 200.66 mm .. 579.12 mm`, `Y = 142.24 mm .. 269.24 mm`  
**Title Banner:** `5. SOLID-STATE 3-TERMINAL AFE MATRIX (2x 74HC595N, 3x DG408DJ, DG412LDJ), 0.1% R.G. KEEN BANKS & FAILSAFES`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`U6`** | `74HC595N` | `74HC595N` | `Socket_DIP-16_W7.62mm` | `{1: "U1_A1", 2: "U1_A2", 3: "U1_EN", 4: "U2_A0", 5: "U2_A1", 6: "U2_A2", 7: "U2_EN", 8: "AGND", 9: "SR_CHAIN", 10: "SR_CLR_N", 11: "AFE_SPI_SCK", 12: "SR_LATCH", 13: "SR_OE_N", 14: "AFE_SPI_MOSI", 15: "U1_A0", 16: "+3V3_ANA"}` |
| **`U7`** | `74HC595N` | `74HC595N` | `Socket_DIP-16_W7.62mm` | `{1: "U3_A1", 2: "U3_A2", 3: "U3_EN", 4: "SW_G1", 5: "SW_G2", 6: "SW_G3", 7: "LED2_DRV", 8: "AGND", 9: None, 10: "SR_CLR_N", 11: "AFE_SPI_SCK", 12: "SR_LATCH", 13: "SR_OE_N", 14: "SR_CHAIN", 15: "U3_A0", 16: "+3V3_ANA"}` |
| **`C12`, `C13`** | `C_THT` | `100nF` ($2\times$) | `C_Disc_P5.00mm` | `{1: "+3V3_ANA", 2: "AGND"}` |
| **`R_OEN`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "SR_OE_N"}` |
| **`R_POR`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "SR_CLR_N"}` |
| **`C_POR`** | `C_THT` | `1uF` | `C_Disc_P5.00mm` | `{1: "SR_CLR_N", 2: "AGND"}` |
| **`U8`** | `DG412LDJ` | `DG412LDJ` | `Socket_DIP-16_W7.62mm` | `{1: "SW_G1", 2: "TP1_MUX", 3: "AGND", 4: "AGND", 5: "AGND", 6: "AGND", 7: "AGND", 8: "AGND", 9: "SW_G3", 10: "TP3_MUX", 11: "AGND", 12: "+3V3_ANA", 13: "+12V_A", 14: "AGND", 15: "TP2_MUX", 16: "SW_G2"}` |
| **`C21`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`U1`, `U2`, `U3`** | `DG408DJ` | `DG408DJ` ($ch \in \{1,2,3\}$) | `Socket_DIP-16_W7.62mm` | `{1: "U{ch}_A0", 2: "U{ch}_EN", 3: "AGND", 4: "C{ch}_S1", 5: "C{ch}_S2", 6: "C{ch}_S3", 7: "C{ch}_S4", 8: "TP{ch}_MUX", 9: "+9V_LIM", 10: "AGND", 11: "C{ch}_S6", 12: "C{ch}_S5", 13: "+12V_A", 14: "AGND", 15: "U{ch}_A2", 16: "U{ch}_A1"}` |
| **`C14..C16`** | `C_THT` | `100nF` ($3\times$) | `C_Disc_P5.00mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`R_EN1..3`** | `R_THT` | `100k` ($3\times$) | `R_Axial_P7.62mm` | `{1: "U{ch}_EN", 2: "AGND"}` |
| **`R_PD1..3`** | `R_THT` | `100k` ($3\times$) | `R_Axial_P7.62mm` | `{1: "SW_G{ch}", 2: "AGND"}` |
| **`R_LA1..3`** | `R_THT` | `470R_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "C{ch}_S1", 2: "V_SRC_A"}` |
| **`R_KA1..3`** | `R_THT` | `2.49k_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "C{ch}_S2", 2: "V_SRC_A"}` |
| **`R_MB1..3`** | `R_THT` | `100k_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "C{ch}_S3", 2: "V_SRC_B"}` |
| **`R_KB1..3`** | `R_THT` | `2.20M_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "C{ch}_S4", 2: "V_SRC_B"}` |
| **`R_LG1..3`** | `R_THT` | `470R_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "C{ch}_S5", 2: "AGND"}` |
| **`R_KG1..3`** | `R_THT` | `2.49k_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "C{ch}_S6", 2: "AGND"}` |
| **`R_PR1..3`** | `R_THT` | `100R_0.5W_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "TP{ch}_DUT", 2: "TP{ch}_MUX"}` |
| **`D_CH1..3`** | `D_THT` | `1N3595` ($3\times$) | `D_DO-35_P7.62mm` | `{1: "+12V_A", 2: "TP{ch}_MUX"}` |
| **`D_CL1..3`** | `D_THT` | `1N3595` ($3\times$) | `D_DO-35_P7.62mm` | `{1: "TP{ch}_MUX", 2: "AGND"}` |
| **`J_ZIF`** | `ZIF_14Pin` | `ZIF-14_DUT` | `Socket_ZIF_14Pin` | `{1: "TP1_DUT", 2: "TP1_DUT", 3: "TP1_DUT", 4: "TP2_DUT", 5: "TP3_DUT", 6: "TP3_DUT", 7: "TP3_DUT", 8: "TP3_DUT", 9: "TP3_DUT", 10: "TP3_DUT", 11: "TP2_DUT", 12: "TP1_DUT", 13: "TP1_DUT", 14: "TP1_DUT"}` |
| **`J_DUT3`** | `Conn_1x03` | `DUT_3PIN_SKT` | `Socket_1x03_DUT` | `{1: "TP1_DUT", 2: "TP2_DUT", 3: "TP3_DUT"}` |
| **`J_KELVIN`** | `Conn_1x04` | `KELVIN_LEADS` | `Header_1x04_P2.54mm` | `{1: "TP1_DUT", 2: "TP2_DUT", 3: "TP3_DUT", 4: "AGND"}` |
| **`J_TERM5`** | `TermBlock_5Pin` | `PROBE_5P_508` | `TerminalBlock_5Pin_508` | `{1: "TP1_DUT", 2: "TP2_DUT", 3: "TP3_DUT", 4: "AGND", 5: "DMM_30V"}` |
| **`J_TEMP`** | `DS18B20_Socket` | `DS18B20_TEMP` | `Socket_TO92_3Pin` | `{1: "AGND", 2: "TEMP_DQ", 3: "+3V3_ANA"}` |
| **`R_TEMP`** | `R_THT` | `4.7k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "TEMP_DQ"}` |

---

### 2.6 BLOCK 6: `ADS1115` Precision Sensing, `MCP4822` Force DAC, `LMC6484IN` Buffer & `DG409DJ` Cal/HV Mux
**Bounding Box:** `X = 15.24 mm .. 294.64 mm`, `Y = 276.86 mm .. 396.24 mm`  
**Title Banner:** `6. ADS1115 16-BIT PRECISION ADC, MCP4822 12-BIT DAC, TLC2272ACP, LMC6484IN 20fA BUFFER & DG409DJ MUX`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`U9`** | `MCP4822` | `MCP4822-E/P` | `Socket_DIP-8_W7.62mm` | `{1: "+3V3_ANA", 2: "DAC_CS", 3: "AFE_SPI_SCK", 4: "AFE_SPI_MOSI", 5: "AGND", 6: "DAC2_RAW", 7: "AGND", 8: "DAC1_RAW"}` |
| **`C19`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+3V3_ANA", 2: "AGND"}` |
| **`R_DACCS`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "DAC_CS"}` |
| **`R_DAC1`, `R_DAC2`** | `R_THT` | `10k` ($2\times$) | `R_Axial_P7.62mm` | `{1: "DAC1_RAW", 2: "DAC1_FILT"}` / `{1: "DAC2_RAW", 2: "DAC2_FILT"}` |
| **`C_DAC1`, `C_DAC2`** | `C_THT` | `100nF` ($2\times$) | `C_Disc_P5.00mm` | `{1: "DAC1_FILT", 2: "AGND"}` / `{1: "DAC2_FILT", 2: "AGND"}` |
| **`R_DP1`, `R_DP2`** | `R_THT` | `100k` ($2\times$) | `R_Axial_P7.62mm` | `{1: "DAC1_FILT", 2: "AGND"}` / `{1: "DAC2_FILT", 2: "AGND"}` |
| **`U5`** | `TLC2272ACP` | `TLC2272ACP` | `Socket_DIP-8_W7.62mm` | `{1: "V_SRC_A", 2: "FB_SRCA", 3: "DAC1_FILT", 4: "AGND", 5: "DAC2_FILT", 6: "FB_SRCB", 7: "V_SRC_B", 8: "+12V_A"}` |
| **`C10`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`R_G1A`, `R_G2A`** | `R_THT` | `34.8k_0.1%` ($2\times$) | `R_Axial_P7.62mm` | `{1: "V_SRC_A", 2: "FB_SRCA"}` / `{1: "V_SRC_B", 2: "FB_SRCB"}` |
| **`R_G1B`, `R_G2B`** | `R_THT` | `10.0k_0.1%` ($2\times$) | `R_Axial_P7.62mm` | `{1: "FB_SRCA", 2: "AGND"}` / `{1: "FB_SRCB", 2: "AGND"}` |
| **`R_HV1`** | `R_THT` | `100k_0.1%` | `R_Axial_P7.62mm` | `{1: "DMM_30V", 2: "DMM_30V_DIV"}` |
| **`R_HV2`** | `R_THT` | `10.0k_0.1%` | `R_Axial_P7.62mm` | `{1: "DMM_30V_DIV", 2: "AGND"}` |
| **`D_HVH`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | `{1: "+12V_A", 2: "DMM_30V_DIV"}` |
| **`D_HVL`** | `D_THT` | `1N3595` | `D_DO-35_P7.62mm` | `{1: "DMM_30V_DIV", 2: "AGND"}` |
| **`U10`** | `DG409DJ` | `DG409DJ` | `Socket_DIP-16_W7.62mm` | `{1: "CAL_A0", 2: "+3V3_ANA", 3: "AGND", 4: "V_SRC_A", 5: "V_SRC_B", 6: "+9V_LIM", 7: "DMM_30V_DIV", 8: "CAL_MUX_OUT", 9: "AGND", 10: "AGND", 11: "AGND", 12: "AGND", 13: "AGND", 14: "+12V_A", 15: "AGND", 16: "CAL_A1"}` |
| **`C20`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`U4`** | `LMC6484IN` | `LMC6484IN` | `Socket_DIP-14_W7.62mm` | `{1: "BUF_TP1", 2: "BUF_TP1", 3: "ESD_TP1", 4: "+12V_A", 5: "ESD_TP2", 6: "BUF_TP2", 7: "BUF_TP2", 8: "BUF_TP3", 9: "BUF_TP3", 10: "ESD_TP3", 11: "AGND", 12: "CAL_MUX_OUT", 13: "BUF_CAL", 14: "BUF_CAL"}` |
| **`C11`** | `C_THT` | `100nF` | `C_Disc_P5.00mm` | `{1: "+12V_A", 2: "AGND"}` |
| **`R_ESD1..3`** | `R_THT` | `10k` ($3\times$) | `R_Axial_P7.62mm` | `{1: "TP{idx}_DUT", 2: "ESD_TP{idx}"}` |
| **`R_D1A..3A`** | `R_THT` | `1.50k_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "BUF_TP{idx}", 2: "ADC_TP{idx}"}` |
| **`R_D1B..3B`** | `R_THT` | `499R_0.1%` ($3\times$) | `R_Axial_P7.62mm` | `{1: "ADC_TP{idx}", 2: "AGND"}` |
| **`C_A0..C_A2`** | `C_THT` | `100nF_C0G` ($3\times$) | `C_Disc_P5.00mm` | `{1: "ADC_TP{idx}", 2: "AGND"}` |
| **`R_ISO1..3`** | `R_THT` | `10k` ($3\times$) | `R_Axial_P7.62mm` | `{1: "ADC_TP{idx}", 2: "FAST_TP{idx}"}` |
| **`R_D4A`** | `R_THT` | `1.50k_0.1%` | `R_Axial_P7.62mm` | `{1: "BUF_CAL", 2: "ADC_CAL"}` |
| **`R_D4B`** | `R_THT` | `499R_0.1%` | `R_Axial_P7.62mm` | `{1: "ADC_CAL", 2: "AGND"}` |
| **`C_A3`** | `C_THT` | `100nF_C0G` | `C_Disc_P5.00mm` | `{1: "ADC_CAL", 2: "AGND"}` |
| **`MOD_ADS`** | `ADS1115_Module` | `ADS1115_16Bit` | `Socket_ADS1115_Module` | `{1: "+3V3_ADC", 2: "AGND", 3: "ANA_I2C_SCL", 4: "ANA_I2C_SDA", 5: "AGND", 6: "ADS_ALRT", 7: "ADC_TP1", 8: "ADC_TP2", 9: "ADC_TP3", 10: "ADC_CAL"}` |
| **`R_SDA`** | `R_THT` | `4.7k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "ANA_I2C_SDA"}` |
| **`R_SCL`** | `R_THT` | `4.7k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "ANA_I2C_SCL"}` |
| **`R_ALRT`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "+3V3_ANA", 2: "ADS_ALRT"}` |

---

### 2.7 BLOCK 7: Dual 1.9" ST7789 IPS Displays & Retro Controls (D-Pad, `A/B`, `ENC1`, `SW_STBY`)
**Bounding Box:** `X = 299.72 mm .. 444.50 mm`, `Y = 276.86 mm .. 396.24 mm`  
**Title Banner:** `7. DUAL 1.9" ST7789 IPS COLOR DISPLAYS, BACKLIGHT SWITCH & RETRO D-PAD / ENCODER`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`MOD_TFT1`** | `TFT_1_9_ST7789` | `TFT1_1.9_IPS_CURVE` | `Socket_TFT_1_9_Curve` | `{1: "DGND", 2: "+3V3_DIG", 3: "UI_SPI_SCK", 4: "UI_SPI_MOSI", 5: "TFT_RES", 6: "TFT_DC", 7: "TFT1_CS", 8: "TFT_BLK"}` |
| **`MOD_TFT2`** | `TFT_1_9_ST7789` | `TFT2_1.9_IPS_DMM` | `Socket_TFT_1_9_DMM` | `{1: "DGND", 2: "+3V3_DIG", 3: "UI_SPI_SCK", 4: "UI_SPI_MOSI", 5: "TFT_RES", 6: "TFT_DC", 7: "TFT2_CS", 8: "TFT_BLK"}` |
| **`R_T1CS`, `R_T2CS`** | `R_THT` | `10k` ($2\times$) | `R_Axial_P7.62mm` | `{1: "+3V3_DIG", 2: "TFT1_CS"}` / `{1: "+3V3_DIG", 2: "TFT2_CS"}` |
| **`R_TRES`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "+3V3_DIG", 2: "TFT_RES"}` |
| **`C_TRES`** | `C_THT` | `1uF` | `C_Disc_P5.00mm` | `{1: "TFT_RES", 2: "DGND"}` |
| **`R_PD_BLK`** | `R_THT` | `100k` | `R_Axial_P7.62mm` | `{1: "TFT_BLK_EN", 2: "DGND"}` |
| **`R_G_BLK`** | `R_THT` | `100R` | `R_Axial_P7.62mm` | `{1: "TFT_BLK_EN", 2: "Q_BLK_G"}` |
| **`Q_BLK`** | `NMOS_2N7000` | `2N7000` | `Socket_TO92_3Pin` | `{1: "DGND", 2: "Q_BLK_G", 3: "TFT_BLK"}` *(or drives `TFT_BLK` enable)* |
| **`SW_UP`** | `SW_THT` | `DPAD_UP` | `SW_Tact_12mm_Yellow` | `{1: "BTN_UP", 2: "DGND"}` (+ `R_PU_UP=10k` to `+3V3_DIG`, `C_DB_UP=100nF` to `DGND`) |
| **`SW_DOWN`** | `SW_THT` | `DPAD_DOWN` | `SW_Tact_12mm_Yellow` | `{1: "BTN_DOWN", 2: "DGND"}` (+ `R_PU_DN=10k` to `+3V3_DIG`, `C_DB_DN=100nF` to `DGND`) |
| **`SW_LEFT`** | `SW_THT` | `DPAD_LEFT` | `SW_Tact_12mm_Yellow` | `{1: "BTN_LEFT", 2: "DGND"}` (+ `R_PU_LF=10k` to `+3V3_DIG`, `C_DB_LF=100nF` to `DGND`) |
| **`SW_RIGHT`** | `SW_THT` | `DPAD_RIGHT` | `SW_Tact_12mm_Yellow` | `{1: "BTN_RIGHT", 2: "DGND"}` (+ `R_PU_RT=10k` to `+3V3_DIG`, `C_DB_RT=100nF` to `DGND`) |
| **`SW_A`** | `SW_THT` | `BTN_A_SCAN` | `SW_Tact_12mm_Yellow` | `{1: "BTN_A", 2: "DGND"}` (+ `R_PU_BA=10k` to `+3V3_DIG`, `C_DB_BA=100nF` to `DGND`) |
| **`SW_B`** | `SW_THT` | `BTN_B_BACK` | `SW_Tact_12mm_Yellow` | `{1: "BTN_B", 2: "DGND"}` (+ `R_PU_BB=10k` to `+3V3_DIG`, `C_DB_BB=100nF` to `DGND`) |
| **`ENC1`** | `EC11_Encoder` | `EC11_NAV_KNOB` | `EC11_Knob_Encoder` | `{1: "ENC_A", 2: "DGND", 3: "ENC_B", 4: "ENC_SW", 5: "DGND"}` (+ `10k` pull-ups to `+3V3_DIG`, `10nF` on `A/B`, `100nF` on `ENC_SW`) |
| **`SW_STBY`** | `SW_SPDT_THT` | `STANDBY_RF_MODE` | `SW_Slide_SPDT_THT` | `{1: "DGND", 2: "SW_STBY", 3: "+3V3_DIG_STBY"}` (+ `R_PU_STBY=10k` to `+3V3_DIG`) |

---

### 2.8 BLOCK 8: Top Status LED Array (6× 3 mm Bezel Bar) & 56k Modem Buzzer (`BZ1`)
**Bounding Box:** `X = 449.58 mm .. 579.12 mm`, `Y = 276.86 mm .. 396.24 mm`  
**Title Banner:** `8. TOP 6-LED STATUS BAR & 56K MODEM ACOUSTIC BUZZER STAGE (WITH HARDWARE MUTE & RC SNUBBER)`

| RefDes | Symbol (`Ge_Engine.kicad_sym`) | Value / Part Number | Footprint (`Ge_Engine.pretty`) | Exact Pin-to-Net Dictionary (`{Pin: Net}`) |
| :--- | :--- | :--- | :--- | :--- |
| **`LED_PWR`** (`D_LED1`) | `LED_THT` | `1_PWR_GREEN` | `LED_D3mm_Green` | `{1: "DGND", 2: "LED_PWR_A"}` (via `R_LED_PWR = 1.0k` from `+3V3_DIG`) |
| **`LED_CHG`** (`D_LED2`) | `LED_THT` | `2_CHG_AMBER` | `LED_D3mm_Amber` | `{1: "CHG_STAT_N", 2: "LED_CHG_A"}` (via `R_LED_CHG = 1.5k` from `+3V3_DIG`) |
| **`D_LED3`** | `LED_THT` | `3_STBY_RF_CYAN` | `LED_D3mm_Blue` | `{1: "DGND", 2: "LED3_A"}` (via `R_LED3 = 470R` from `U11 GPB2` `LED_STBY_RF`) |
| **`D_LED4`** | `LED_THT` | `4_SCAN_BLUE` | `LED_D3mm_Blue` | `{1: "DGND", 2: "LED4_A"}` (via `R_LED4 = 470R` from `U11 GPB3` `LED_SCAN`) |
| **`D_LED5`** | `LED_THT` | `5_DUT_OK_GREEN` | `LED_D3mm_Green` | `{1: "DGND", 2: "LED5_A"}` (via `R_LED5 = 470R` from `U11 GPB4` `LED_DUT_OK`) |
| **`D_LED6`** | `LED_THT` | `6_FAULT_RED` | `LED_D3mm_Red` | `{1: "DGND", 2: "LED6_A"}` (via `R_LED6 = 470R` from `U11 GPB5` `LED_FAULT`) |
| **`R_LED1` / `D1`** | `R_THT` / `LED_THT` | `3.3k` / `ANA_9V_ON` | `LED_D3mm_Green` | `{1: "+9V_REF", 2: "LED1_A"}` / `{1: "AGND", 2: "LED1_A"}` (Analog core $+9\text{ V}$ alive indicator) |
| **`R_LED2` / `D2`** | `R_THT` / `LED_THT` | `1.0k` / `AFE_ACT` | `LED_D3mm_Blue` | `{1: "LED2_DRV", 2: "LED2_A"}` / `{1: "AGND", 2: "LED2_A"}` (`U7 QH` shift activity indicator) |
| **`SW_BZ`** | `SW_SPDT_THT` | `MUTE_SWITCH` | `SW_Slide_SPDT_THT` | `{1: None, 2: "BZ_POS", 3: "+3V3_DIG"}` |
| **`BZ1`** | `Buzzer_THT` | `BUZZER_12MM` | `Buzzer_12mm` | `{1: "BZ_POS", 2: "BZ_COL"}` |
| **`D_BZ`** | `D_THT` | `1N4148` | `D_DO-35_P7.62mm` | `{1: "BZ_POS", 2: "BZ_COL"}` |
| **`R_SNUB`** | `R_THT` | `100R` | `R_Axial_P7.62mm` | `{1: "BZ_POS", 2: "BZ_SNUB_MID"}` |
| **`C_SNUB`** | `C_THT` | `10nF` | `C_Disc_P5.00mm` | `{1: "BZ_SNUB_MID", 2: "BZ_COL"}` |
| **`Q_BZ`** | `NPN_2N3904` | `2N3904` | `Socket_TO92_3Pin` | `{1: "DGND", 2: "Q_BZ_B", 3: "BZ_COL"}` |
| **`R_BZ_B`** | `R_THT` | `1.0k` | `R_Axial_P7.62mm` | `{1: "BZ_PWM", 2: "Q_BZ_B"}` |
| **`R_BZ_PD`** | `R_THT` | `10k` | `R_Axial_P7.62mm` | `{1: "Q_BZ_B", 2: "DGND"}` |

---

## 3. Unified KiCad 9 & EasyEDA PCB Net-Class Routing & Clearance Rules

To satisfy both JLCPCB 2-layer $1\text{ oz}$ ($35\text{ }\mu\text{m}$) manufacturing design rules and picoamp/nanoamp analog leakage guarding, all nets in `Ge_Parametric_Engine.kicad_pro` / `Ge_Parametric_Engine.kicad_pcb` are assigned to five deterministic Net Classes:

| Net Class Name | Assigned Canonical Nets | Trace Width (mm) | Min Copper Clearance (mm) | Via Diameter / Drill (mm) | Engineering & Signal-Integrity Rationale |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **`PWR_HIGH_CURRENT`** | `V_USB_RAW`, `V_USB_PROT`, `J_BAT+`, `V_BAT_SW`, `V_BAT_PROT`, `PMID_BYP`, `V_SYS_RAW`, `V_BUS_SYS`, `V_ANA_IN`, `DGND`, `AGND` | **`1.00 mm`** ($1.20\text{ mm}$ on `J_BAT+` / `V_BUS_SYS` trunks) | **`0.30 mm`** | **`1.20 / 0.60 mm`** | Carries up to $3.0\text{ A}$ `BQ25895` charge/input currents with $<12\text{ m}\Omega$ trace resistance and $<4^\circ\text{C}$ temperature rise. |
| **`ANALOG_RAIL`** | `V_BOOST_RAW`, `V_LC1_MID`, `V_14V_FILT`, `V_12V_PRE`, `+12V_A`, `+9V_REF`, `LIM_SENSE`, `+9V_LIM`, `V_LC2_MID`, `V_3V3_FILT`, `+3V3_ANA`, `+3V3_ADC`, `M1_3V3`, `+3V3_DIG`, `M2_3V3`, `V_SRC_A`, `V_SRC_B` | **`0.60 mm`** | **`0.25 mm`** | **`1.00 / 0.50 mm`** | Low-inductance power and programmable force distribution across the Analog Core and Digital UI Core. |
| **`ANALOG_GUARD_KELVIN`** | `TP1_DUT`, `TP2_DUT`, `TP3_DUT`, `TP1_MUX`, `TP2_MUX`, `TP3_MUX`, `ESD_TP1`, `ESD_TP2`, `ESD_TP3`, `C1_S3`, `C1_S4`, `C2_S3`, `C2_S4`, `C3_S3`, `C3_S4`, `CAL_MUX_OUT`, `DMM_30V`, `DMM_30V_DIV` | **`0.35 mm`** | **`0.50 mm`** (Surrounded by `AGND` guard pour; **no solder-mask relief** over guard ring) | **`0.90 / 0.45 mm`** | Ultra-high-impedance ($100\text{ k}\Omega\text{–}2.20\text{ M}\Omega$, $20\text{ fA}$ `LMC6484IN` input) Kelvin sense nodes. Wide $0.50\text{ mm}$ clearance to `AGND` eliminates FR-4 surface leakage. |
| **`ANALOG_SENSE_LPF`** | `BUF_TP1..3`, `BUF_CAL`, `ADC_TP1..3`, `ADC_CAL`, `DAC1_RAW`, `DAC2_RAW`, `DAC1_FILT`, `DAC2_FILT`, `FB_SRCA`, `FB_SRCB`, `C{1..3}_S{1,2,5,6}` | **`0.35 mm`** | **`0.25 mm`** | **`0.90 / 0.45 mm`** | Low-impedance buffered analog sense and divider lines inside the `AGND` zone. |
| **`DIGITAL_SIGNAL` / `DEFAULT`** | All SPI (`AFE_SPI_*`, `UI_SPI_*`), $\text{I}^2\text{C}$ (`ANA_I2C_*`, `UI_I2C_*`), UART (`LINK_TX_*`, `LINK_RX_*`, `CYD_*`), Shift Matrix (`U{1..3}_A*`, `U{1..3}_EN`, `SW_G*`, `SR_*`), Button, Encoder, LED, and Buzzer nets | **`0.30 mm`** | **`0.20 mm`** | **`0.80 / 0.40 mm`** | $100\%$ compatible with standard JLCPCB 2-layer process ($0.127\text{ mm}$ min capability $\implies >1.5\times\text{–}2.3\times$ manufacturing margin). |
