# Phase 2 — Power-Path, Dual Critically Damped LC Pi-Filters & 15 mA Limiter Pin-by-Pin Netlist & Transfer Function Analysis (`p2_power_pifilter_netlist.md`)

**Document ID:** `p2_power_pifilter_netlist.md`  
**Subsystem:** Block 1 (USB-C, 3.7 V Non-HV LiPo Protection, TI `BQ25895` NVDC Fast-Charger & `+3V3_DIG` Ultra-LDO) and Block 2 (Switchable Analog Power, Dual Critically Damped LC Pi-Filters, `+12V_A` / `+9V_LIM` / `+3V3_ANA` / `+3V3_ADC` Rails, 15 mA Active Limiter & Star Ground)  
**Architecture Revision:** Rev 5.0 (Harmonized with `gate_1_audit.md` §7/§8 Canonical Net & Designator Standards)

---

## 1. Canonical Designator & Net Harmonization Summary (Blocks 1 & 2)

All net names and component reference designators in this netlist strictly implement the four canonical standardizations established in `gate_1_audit.md` (§7 & §8):
1. **Charger Input & Battery Protection Chain:**
   - `J_USB_C` (`VBUS`) $\longrightarrow$ `F_USB` $\longrightarrow$ **`V_USB_PROT`** (`D_TVS_USB` Pin 1 & `MOD_CHG` Pin 1 `VBUS`).
   - `J_BAT` (`Pin 1`) $\longrightarrow$ **`J_BAT+`** $\longrightarrow$ `SW_PWR` (Pole 1: Pin 2 COM1 $\to$ Pin 3 ON) $\longrightarrow$ **`V_BAT_SW`** (`Q_REV_BAT` Pin 2 Drain).
   - `Q_REV_BAT` (Pin 3 Source) $\longrightarrow$ **`V_BAT_PROT`** (`MOD_CHG` Pin 5 `BAT`).
   - `MOD_CHG` (Pin 4 `SYS`) $\longrightarrow$ **`V_SYS_RAW`** $\longrightarrow$ `SW_PWR` (Pole 2: Pin 5 COM2 $\to$ Pin 6 ON) $\longrightarrow$ **`V_BUS_SYS`**.
2. **Standardized Pull-Up / Indicator Resistors:**
   - **`R_LED_CHG`** ($1.5\text{ k}\Omega$, $1\%$, $0.25\text{ W}$): `+3V3_DIG` $\to$ `LED_CHG_A` (`LED_CHG` Anode; Cathode to `MOD_CHG` Pin 10 `STAT` on net **`CHG_STAT_N`**).
   - **`R_PU_OVC`** ($10\text{ k}\Omega$, $1\%$, $0.25\text{ W}$): `+3V3_ANA` $\to$ **`FAULT_OVC_N`** (`Q_OVC` Pin 3 Collector & `U_MCU1` Pin 8 `GPIO25`).
3. **Partitioned Ground Domains & Star Bridge:**
   - **`DGND`**: Digital, RF, Charger, Display, and Buzzer ground plane.
   - **`AGND`**: Quiet Analog Metrology ground plane.
   - **`R_STAR`** (`0R_STAR_BRIDGE`, $0\,\Omega$ Metal-Film Axial Jumper, $\le 10\text{ m}\Omega$): Pin 1 = **`DGND`**, Pin 2 = **`AGND`**.

---

## 2. Block 1 — Complete Pin-by-Pin Component Netlist (Power, USB-C, 3.7 V Non-HV LiPo Protection, `BQ25895` NVDC Charger & `+3V3_DIG` Ultra-LDO)

### 2.1 Block 1 Master Component & Pin Table (100% THT / Socketed Module)

| RefDes | Part Number / Value | Tolerance & Rating | KiCad 9 Footprint (`Ge_Engine.pretty`) | Pin-by-Pin Net Connections (`Pin#: NetName`) | Functional Role & Assembly Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`J_USB_C`** | `USBC_Breakout_6P` | $5\text{ V}/3.0\text{ A}$ ($9\text{–}12\text{ V}$ HVDCP capable) | `Socket_USBC_Breakout` (`Header_1x06_P2.54mm`) | `1: V_USB_RAW`, `2: DGND`, `3: USB_CC1`, `4: USB_CC2`, `5: USB_DP_CHG`, `6: USB_DM_CHG` | Rear/side THT USB-C power input breakout socket |
| **`R_CC1`** | `5.1k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: USB_CC1`, `2: DGND` | Dedicated USB Type-C UFP $5\text{ V}/3\text{ A}$ Rd pull-down #1 |
| **`R_CC2`** | `5.1k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: USB_CC2`, `2: DGND` | Dedicated USB Type-C UFP $5\text{ V}/3\text{ A}$ Rd pull-down #2 |
| **`F_USB`** | `RXEF175` (or `RXEF110`) | $I_{\text{hold}}=1.75\text{ A}$, $I_{\text{trip}}=3.50\text{ A}$, $60\text{ V}$ | `Fuse_PTC_P5.00mm` | `1: V_USB_RAW`, `2: V_USB_PROT` | Radial resettable PolySwitch PPTC input overcurrent fuse |
| **`D_TVS_USB`** | `P6KE15A` (or `1N5919B` 5.6V) | $V_{\text{RWM}}=12.8\text{ V}$, $600\text{ W}$ peak ($3\text{ W}$ Zener) | `D_DO-15_P10.16mm` (`D_DO-41_P10.16mm`) | `1 (Cathode): V_USB_PROT`, `2 (Anode): DGND` | Clamps USB hot-plug inductive ringing below `BQ25895` $22\text{ V}$ abs max |
| **`C_USB_IN`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic | `C_Disc_P5.00mm` | `1: V_USB_PROT`, `2: DGND` | High-frequency input bypass at `MOD_CHG` Pin 1 (`VBUS`) |
| **`J_BAT`** | `BAT_LIPO_1S_3V7` | $3.7\text{ V}$ Non-HV LiPo (**$4.20\text{ V}$ Max!**), $3\text{ A}$ | `Header_1x02_P2.54mm` | `1 (+): J_BAT+`, `2 (-): DGND` | Polarized 2-pin THT header for 1S 3.7V Non-HV LiPo pack |
| **`J_NTC`** | `BAT_NTC_10K` | $10\text{ k}\Omega$ ($\beta=3435\text{ K}$) JEITA Probe | `Header_1x02_P2.54mm` | `1: BAT_NTC`, `2: DGND` | Optional external 10k NTC pack thermal probe header (module has onboard bias) |
| **`SW_PWR`** | `DPDT_3A_MASTER_PWR` | $\ge 2.0\text{ A}$ ($3.0\text{ A} \text{ @ } 28\text{ VDC}$), $\le 20\text{ m}\Omega$ | `SW_Slide_DPDT_THT` | **Pole 1 (Bat B+):** `1 (OFF): NC`, `2 (COM1): J_BAT+`, `3 (ON): V_BAT_SW`<br/>**Pole 2 (Sys):** `4 (OFF): V_SYS_BLEED`, `5 (COM2): V_SYS_RAW`, `6 (ON): V_BUS_SYS` | Hard Master Power Switch: Pole 1 physically breaks `J_BAT+` (**$0.000\text{ }\mu\text{A}$** storage drain & blocks blind $4.208\text{ V}$ default charging when `OFF`) |
| **`R_BLEED_SYS`** | `10k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: V_SYS_BLEED`, `2: DGND` | Discharges system bus rails cleanly when `SW_PWR` is thrown to `OFF` |
| **`Q_REV_BAT`** | `NDP6020P` | $-20\text{ V}$, $-24\text{ A}$, $R_{\text{DS(on)}} \approx 52\text{ m}\Omega$, $V_{GSS}=\pm 8\text{ V}$ | `TO-220-3_Vertical` | `1 (Gate): Q_REV_G`, `2 (Drain / Tab): V_BAT_SW`, `3 (Source): V_BAT_PROT` | Zero-drop reverse-battery protection P-MOSFET (Body diode Anode=Pin 2 Drain $\to$ Cathode=Pin 3 Source) |
| **`R_G_REV`** | `1.0k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: Q_REV_G`, `2: DGND` | Limits `Q_REV_BAT` transient gate current during battery hot-plug |
| **`MOD_CHG`** | `BQ25895_Module` | TI `BQ25895` $1.5\text{ MHz}$ Buck NVDC Charger (`0x6A`) | `Socket_BQ25895_Charger` ($2\times 1\times 06$ @ $15.24\text{ mm}$) | `1 (VBUS): V_USB_PROT`, `2 (PMID): PMID_BYP`, `3 (GND): DGND`, `4 (SYS): V_SYS_RAW`, `5 (BAT): V_BAT_PROT`, `6 (TS): BAT_NTC`, `7 (SCL): UI_I2C_SCL`, `8 (SDA): UI_I2C_SDA`, `9 (INT): NC`, `10 (STAT): CHG_STAT_N`, `11 (CE_N): DGND`, `12 (OTG): DGND` | Socketed 12-pin switching NVDC charger; firmware programs $V_{\text{REG}}=\mathbf{4.192\text{ V}}$ (`REG06=0x58`), $I_{\text{CHG}}=2.048\text{ A}$, $I_{\text{TERM}}=128\text{ mA}$ |
| **`C_PMID`** | `10uF_25V` | $\pm 20\%$, $25\text{ V}$ Radial Electrolytic / MLCC | `CP_Radial_D8_P3.5mm` | `1 (+): PMID_BYP`, `2 (-): DGND` | Auxiliary external bulk decoupling on `MOD_CHG` Pin 2 (`PMID`) |
| **`C_BAT_BYP`** | `100uF_16V` | $\pm 20\%$, $16\text{ V}$ Low-ESR Electrolytic | `CP_Radial_D8_P3.5mm` | `1 (+): V_BAT_PROT`, `2 (-): DGND` | Bulk battery reservoir capacitor at `MOD_CHG` Pin 5 (`BAT`) |
| **`C_SYS1`** | `100uF_16V` | $\pm 20\%$, $16\text{ V}$ Low-ESR Electrolytic | `CP_Radial_D8_P3.5mm` | `1 (+): V_BUS_SYS`, `2 (-): DGND` | Primary bulk reservoir on switched system bus `V_BUS_SYS` |
| **`C_SYS2`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic | `C_Disc_P5.00mm` | `1: V_BUS_SYS`, `2: DGND` | High-frequency ceramic bypass at `U_LDO_DIG` Pin 1 (`VIN`) |
| **`U_LDO_DIG`** | `MCP1826S-3302E/AB` | $3.30\text{ V} \pm 0.5\%$, $1.0\text{ A}$, $V_{\text{DO}}=250\text{ mV}$ @ $1\text{ A}$ | `TO-220-3_Vertical` | `1 (VIN): V_BUS_SYS`, `2 (GND / Tab): DGND`, `3 (VOUT): +3V3_DIG` | Ultra-low-dropout CMOS digital/RF LDO ($V_{\text{DO}} < 75\text{ mV}$ @ $300\text{ mA}$; regulates down to $3.40\text{ V}$ LiPo) |
| **`C_DIG1`** | `100uF_16V` | $\pm 20\%$, $16\text{ V}$ Low-ESR Electrolytic | `CP_Radial_D8_P3.5mm` | `1 (+): +3V3_DIG`, `2 (-): DGND` | Bulk output reservoir for `+3V3_DIG` (ESP32-C3 Wi-Fi TX burst support) |
| **`C_DIG2`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic | `C_Disc_P5.00mm` | `1: +3V3_DIG`, `2: DGND` | Mandatory low-ESR ceramic output stability capacitor for `U_LDO_DIG` ($\ge 1\text{ }\mu\text{F}$ total on rail) |
| **`R_LED_PWR`** | `1.0k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: +3V3_DIG`, `2: LED_PWR_A` | Sets Top-Bezel LED #1 (`LED_PWR` Green) current to $(3.30-2.10)/1.0\text{k} = 1.20\text{ mA}$ |
| **`LED_PWR`** | `LED_3MM_GREEN` | $V_F \approx 2.1\text{ V}$, $20\text{ mA}$ max | `LED_D3mm_Green` | `1 (Cathode): DGND`, `2 (Anode): LED_PWR_A` | Top-Bezel Status LED #1: Master Digital Power ON (`+3V3_DIG` active) |
| **`R_LED_CHG`** | `1.5k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: +3V3_DIG`, `2: LED_CHG_A` | Sets Top-Bezel LED #2 (`LED_CHG` Amber) current to $(3.30-2.00-0.10)/1.5\text{k} = 0.80\text{ mA}$ |
| **`LED_CHG`** | `LED_3MM_AMBER` | $V_F \approx 2.0\text{ V}$, $20\text{ mA}$ max | `LED_D3mm_Amber` | `1 (Cathode): CHG_STAT_N`, `2 (Anode): LED_CHG_A` | Top-Bezel Status LED #2: Driven by `MOD_CHG` Pin 10 (`STAT` open-drain) |

---

## 3. Block 2 — Complete Pin-by-Pin Component Netlist (Switchable Analog Domain, Dual Critically Damped LC Pi-Filters, LDOs, 15 mA Active Limiter & Star Ground)

### 3.1 Block 2 Master Component & Pin Table (100% THT / Socketed Module)

| RefDes | Part Number / Value | Tolerance & Rating | KiCad 9 Footprint (`Ge_Engine.pretty`) | Pin-by-Pin Net Connections (`Pin#: NetName`) | Functional Role & Assembly Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`R_PD_ANA`** | `100k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: ANA_PWR_EN`, `2: DGND` | Pull-down on `U11 GPB7` (`ANA_PWR_EN`); guarantees Analog Core boots **100% OFF** |
| **`R_G_ANA`** | `100R` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: ANA_PWR_EN`, `2: Q_ANA_NG` | Gate damping resistor for `Q_ANA_N` (`2N7000`) |
| **`Q_ANA_N`** | `2N7000` | $60\text{ V}$, $200\text{ mA}$ N-MOSFET ($1=\text{S}, 2=\text{G}, 3=\text{D}$) | `Socket_TO92_3Pin` | `1 (Source): DGND`, `2 (Gate): Q_ANA_NG`, `3 (Drain): Q_ANA_SW_G` | Level-shifting N-MOSFET inverting driver for high-side switch `Q_ANA_SW` |
| **`R_PU_SW`** | `47k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: V_BUS_SYS`, `2: Q_ANA_SW_G` | Holds `Q_ANA_SW` Gate at Source ($V_{GS} = 0.00\text{ V}$) when `Q_ANA_N` is OFF in Standby |
| **`Q_ANA_SW`** | `NDP6020P` | $-20\text{ V}$, $-24\text{ A}$, $R_{\text{DS(on)}} \approx 55\text{ m}\Omega$ ($1=\text{G}, 2=\text{D}, 3=\text{S}$) | `TO-220-3_Vertical` | `1 (Gate): Q_ANA_SW_G`, `2 (Drain / Tab): V_ANA_IN`, `3 (Source): V_BUS_SYS` | High-side Analog Core master power switch ($0.000\text{ mA}$ Standby cutoff; body diode blocks `V_BUS_SYS` $\to$ `V_ANA_IN`) |
| **`MOD_BOOST`** | `MT3608_14.5V` | $1.2\text{ MHz}$ Step-Up Module, Trimmed to **$14.50\text{ V}$** | `Socket_MT3608_Boost` | `1 (VIN+): V_ANA_IN`, `2 (VIN-): AGND`, `3 (VOUT+): V_BOOST_RAW`, `4 (VOUT-): AGND` | Socketed DC-DC boost converter; **trim to $14.50\text{ V}$ before inserting analog ICs** |
| **`C_PI1A`** (`C3`) | `100uF_35V` | $\pm 20\%$, $35\text{ V}$ Radial Electrolytic ($\text{ESR} \approx 0.35\,\Omega$) | `CP_Radial_D8_P3.5mm` | `1 (+): V_BOOST_RAW`, `2 (-): AGND` | Pi-Filter #1 input bulk reservoir capacitor |
| **`C_PI1B`** (`C4`) | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic ($\text{ESR} \approx 0.02\,\Omega$) | `C_Disc_P5.00mm` | `1: V_BOOST_RAW`, `2: AGND` | Pi-Filter #1 input high-frequency ceramic shunt |
| **`L1`** | `100uH_1A` | $\pm 10\%$, $I_{\text{sat}}=1.0\text{ A}$, $\text{DCR}=0.35\,\Omega$ | `L_Radial_P5.00mm` | `1: V_BOOST_RAW`, `2: V_LC1_MID` | Pi-Filter #1 series power inductor |
| **`R_DAMP1`** | `2.2R_0.5W` | $\pm 1\%$, $0.5\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: V_LC1_MID`, `2: V_14V_FILT` | Pi-Filter #1 critical damping resistor ($R_{\text{tot1}} = 2.55\,\Omega \implies \zeta_{\text{tank}} = 0.902$) |
| **`C_PI2A`** (`C5`) | `100uF_35V` | $\pm 20\%$, $35\text{ V}$ Radial Electrolytic ($\text{ESR} \approx 0.35\,\Omega$) | `CP_Radial_D8_P3.5mm` | `1 (+): V_14V_FILT`, `2 (-): AGND` | Pi-Filter #1 output bulk reservoir capacitor |
| **`C_PI2B`** (`C6`) | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic ($\text{ESR} \approx 0.02\,\Omega$) | `C_Disc_P5.00mm` | `1: V_14V_FILT`, `2: AGND` | Pi-Filter #1 output high-frequency ceramic shunt ($-67.0\text{ dB}$ @ $1.2\text{ MHz}$) |
| **`U_REG12`** | `L7812CV` | $+12.00\text{ V} \pm 2\%$, $1.5\text{ A}$, $V_{\text{IN,max}}=35\text{ V}$ | `TO-220-3_Vertical` | `1 (IN): V_14V_FILT`, `2 (GND / Tab): AGND`, `3 (OUT): V_12V_PRE` | Primary HV linear regulator & $28\text{ V}$ `MT3608` runaway shield ($\sim 55\text{ dB}$ PSRR) |
| **`F1`** | `RXEF017` | $I_{\text{hold}}=170\text{ mA}$, $I_{\text{trip}}=340\text{ mA}$, $R_{\text{min}}=3.3\,\Omega$ | `Fuse_PTC_P5.00mm` | `1: V_12V_PRE`, `2: +12V_A` | Secondary analog rail PPTC fuse (trips if `D_OVP` conducts) |
| **`D_OVP`** | `1N4743A_13V` | $V_Z = 13.0\text{ V} \pm 5\%$, $1.0\text{ W}$ DO-41 Zener | `D_DO-41_P10.16mm` | `1 (Cathode): +12V_A`, `2 (Anode): AGND` | Hardened OVP crowbar clamping `+12V_A` $\le 13.65\text{ V}$ under double-fault |
| **`C17`** | `10uF_25V` | $\pm 20\%$, $25\text{ V}$ Radial Electrolytic | `CP_Radial_D8_P3.5mm` | `1 (+): +12V_A`, `2 (-): AGND` | Bulk decoupling on protected `+12V_A` rail |
| **`C18`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic | `C_Disc_P5.00mm` | `1: +12V_A`, `2: AGND` | High-frequency ceramic decoupling on `+12V_A` |
| **`U_REG9`** (`U_REG`) | `L78L09ACZ` | $+9.00\text{ V} \pm 2\%$, $100\text{ mA}$ TO-92 ($1=\text{OUT}, 2=\text{GND}, 3=\text{IN}$) | `Socket_TO92_3Pin` | `1 (OUT): +9V_REF`, `2 (GND): AGND`, `3 (IN): +12V_A` | Precision $+9.00\text{ V}$ reference & DUT sweep pre-regulator ($\sim 45\text{ dB}$ PSRR) |
| **`C7`** | `10uF_25V` | $\pm 20\%$, $25\text{ V}$ Radial Electrolytic | `CP_Radial_D8_P3.5mm` | `1 (+): +9V_REF`, `2 (-): AGND` | Output stability capacitor for `U_REG9` (`+9V_REF`) |
| **`C8`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic | `C_Disc_P5.00mm` | `1: +9V_REF`, `2: AGND` | High-frequency ceramic bypass on `+9V_REF` |
| **`R_ILIM`** | `43R` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: +9V_REF`, `2: LIM_SENSE` | Current-limit sense resistor: $I_{\text{trip}} = V_{\text{EB}} / 43\,\Omega = \mathbf{15.12\text{ mA}}$ @ $25^\circ\text{C}$ |
| **`Q_LIM1`** | `2N3906` | $-40\text{ V}$, $-200\text{ mA}$ PNP ($1=\text{E}, 2=\text{B}, 3=\text{C}$) | `Socket_TO92_3Pin` | `1 (Emitter): LIM_SENSE`, `2 (Base): LIM_BASE`, `3 (Collector): +9V_LIM` | Series pass PNP transistor for $+9\text{V\_LIM}$ rail |
| **`Q_LIM2`** | `2N3906` | $-40\text{ V}$, $-200\text{ mA}$ PNP ($1=\text{E}, 2=\text{B}, 3=\text{C}$) | `Socket_TO92_3Pin` | `1 (Emitter): +9V_REF`, `2 (Base): LIM_SENSE`, `3 (Collector): LIM_BASE` | Current-sense foldback PNP transistor across `R_ILIM` |
| **`R_IBIAS`** | `10k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: LIM_BASE`, `2: AGND` | Pulls $0.83\text{ mA}$ base current from `Q_LIM1` to saturate pass PNP in normal operation |
| **`R_OVC_DIV1`** | `100k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: +9V_REF`, `2: Q_OVC_B` | Upper bias leg for `Q_OVC` fault detector (or driven from `Q_LIM2` / `+9V_LIM` collapse sense) |
| **`R_OVC_DIV2`** | `22k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: +9V_LIM`, `2: Q_OVC_B` | Senses $+9\text{V\_LIM}$ foldback / `Q_LIM2` conduction to switch `Q_OVC` ON at $I \ge 15.12\text{ mA}$ |
| **`Q_OVC`** | `2N3904` | $40\text{ V}$, $200\text{ mA}$ NPN ($1=\text{E}, 2=\text{B}, 3=\text{C}$) | `Socket_TO92_3Pin` | `1 (Emitter): AGND`, `2 (Base): Q_OVC_B`, `3 (Collector): FAULT_OVC_N` | Active-LOW hardware overcurrent interrupt driver ($<2\text{ }\mu\text{s}$ response to `U_MCU1 GPIO25`) |
| **`R_PU_OVC`** | `10k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: +3V3_ANA`, `2: FAULT_OVC_N` | Canonical pull-up on `FAULT_OVC_N` (`gate_1_audit.md` §8 Item 2) |
| **`C_A1A`** | `100uF_16V` | $\pm 20\%$, $16\text{ V}$ Radial Electrolytic ($\text{ESR} \approx 0.35\,\Omega$) | `CP_Radial_D8_P3.5mm` | `1 (+): V_ANA_IN`, `2 (-): AGND` | Pi-Filter #2 input bulk reservoir capacitor |
| **`C_A1B`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic ($\text{ESR} \approx 0.02\,\Omega$) | `C_Disc_P5.00mm` | `1: V_ANA_IN`, `2: AGND` | Pi-Filter #2 input high-frequency ceramic shunt |
| **`L2`** | `47uH_1A` | $\pm 10\%$, $I_{\text{sat}}=1.0\text{ A}$, $\text{DCR}=0.15\,\Omega$ | `L_Radial_P5.00mm` | `1: V_ANA_IN`, `2: V_LC2_MID` | Pi-Filter #2 series power inductor |
| **`R_DAMP2`** | `1.5R_0.25W` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: V_LC2_MID`, `2: V_3V3_FILT` | Pi-Filter #2 critical damping resistor ($R_{\text{tot2}} = 1.65\,\Omega \implies \zeta_{\text{tank}} = 0.851$) |
| **`C_A2A`** | `100uF_16V` | $\pm 20\%$, $16\text{ V}$ Radial Electrolytic ($\text{ESR} \approx 0.35\,\Omega$) | `CP_Radial_D8_P3.5mm` | `1 (+): V_3V3_FILT`, `2 (-): AGND` | Pi-Filter #2 output bulk reservoir capacitor |
| **`C_A2B`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic ($\text{ESR} \approx 0.02\,\Omega$) | `C_Disc_P5.00mm` | `1: V_3V3_FILT`, `2: AGND` | Pi-Filter #2 output high-frequency ceramic shunt ($-62.55\text{ dB}$ @ $1.5\text{ MHz}$) |
| **`U_LDO_ANA`** | `MCP1826S-3302E/AB` | $3.300\text{ V} \pm 0.5\%$, $1.0\text{ A}$, $V_{\text{DO}} \approx 30\text{ mV}$ @ $120\text{ mA}$ | `TO-220-3_Vertical` | `1 (VIN): V_3V3_FILT`, `2 (GND / Tab): AGND`, `3 (VOUT): +3V3_ANA` | Ultra-low-dropout CMOS Analog Core LDO |
| **`C1`** | `100uF_16V` | $\pm 20\%$, $16\text{ V}$ Low-ESR Electrolytic | `CP_Radial_D8_P3.5mm` | `1 (+): +3V3_ANA`, `2 (-): AGND` | Bulk reservoir capacitor on `+3V3_ANA` |
| **`C2`** | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic | `C_Disc_P5.00mm` | `1: +3V3_ANA`, `2: AGND` | Mandatory low-ESR ceramic output stability cap for `U_LDO_ANA` |
| **`R_ADC_F`** | `10R` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: +3V3_ANA`, `2: +3V3_ADC` | Post-LDO RC low-pass filter resistor for `MOD_ADS` (`ADS1115 VDD`, $\Delta V_{\text{DC}} = 1.5\text{ mV}$) |
| **`C_ADC_FA`** | `10uF_16V` | $\pm 20\%$, $16\text{ V}$ Radial Electrolytic ($\text{ESR} \approx 0.20\,\Omega$) | `CP_Radial_D8_P3.5mm` | `1 (+): +3V3_ADC`, `2 (-): AGND` | Post-LDO RC filter bulk capacitor ($f_c = 1575.8\text{ Hz}$) |
| **`C_ADC_FB`** (`C9`) | `100nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic ($\text{ESR} \approx 0.02\,\Omega$) | `C_Disc_P5.00mm` | `1: +3V3_ADC`, `2: AGND` | Local high-frequency decoupling directly across `MOD_ADS` Pins 1 & 2 |
| **`R_STAR`** | `0R_STAR_BRIDGE` | $0\,\Omega$ Metal-Film Star Jumper ($R \le 10\text{ m}\Omega$, $2\text{ A}$) | `R_Axial_P7.62mm` | `1: DGND`, `2: AGND` | Single-point star ground bridge uniting `DGND` and `AGND` at `Q_ANA_SW` boundary |

---

## 4. Symbolic Transfer Function $H(s)$ & Python-Verified Frequency Response (`100 Hz` – `10 MHz`)

### 4.1 Exact $s$-Domain Impedance Model Including Electrolytic & Ceramic Parallel Branches

For each damped LC Pi-filter driven from a low-impedance switching node (`V_BOOST_RAW` from `MOD_BOOST` or `V_ANA_IN` from `Q_ANA_SW`), the input shunt bank ($C_{\text{in,elec}} \parallel C_{\text{in,cer}}$) stiffens the source node while the series damped inductor branch $Z_L(s)$ and output parallel capacitor bank $Z_C(s)$ form the complex voltage transfer function $H(s)$:

$$Z_L(s) = R_{\text{tot}} + sL = (R_{\text{DCR}} + R_{\text{DAMP}}) + sL$$

The output shunt bank consists of a radial aluminum electrolytic capacitor ($C_e = 100\text{ }\mu\text{F}$, $\text{ESR}_e = 0.35\,\Omega$) in parallel with a multilayer ceramic disc capacitor ($C_c = 100\text{ nF}$, $\text{ESR}_c = 0.02\,\Omega$):

$$Z_{C_e}(s) = R_{e} + \frac{1}{sC_e} = \frac{1 + s R_e C_e}{s C_e}, \qquad Z_{C_c}(s) = R_{c} + \frac{1}{sC_c} = \frac{1 + s R_c C_c}{s C_c}$$

$$Z_C(s) = Z_{C_e}(s) \parallel Z_{C_c}(s) = \frac{(1 + s R_e C_e)(1 + s R_c C_c)}{s\left[(C_e + C_c) + s(R_e + R_c)C_e C_c\right]}$$

Consequently, the complete closed-form transfer function $H(s) = \frac{V_{\text{out}}(s)}{V_{\text{in}}(s)}$ is:

$$\boxed{H(s) = \frac{Z_C(s)}{Z_L(s) + Z_C(s)} = \frac{(1 + s R_e C_e)(1 + s R_c C_c)}{(1 + s R_e C_e)(1 + s R_c C_c) + s(R_{\text{tot}} + sL)\left[(C_e + C_c) + s(R_e + R_c)C_e C_c\right]}}$$

In the low-to-mid frequency passband/resonance region ($f < 20\text{ kHz}$, where $C_e \gg C_c$), this reduces to the classic second-order damped low-pass transfer function:

$$H_{\text{LF}}(s) \approx \frac{1 + s R_e C_e}{s^2 L C_e + s(R_{\text{tot}} + R_e)C_e + 1} = \frac{1 + s/\omega_{z}}{s^2/\omega_0^2 + 2\zeta(s/\omega_0) + 1}$$

where:
- **Pi-Filter #1 ($L_1 = 100\text{ }\mu\text{H}$, $R_{\text{tot1}} = 0.35 + 2.20 = 2.55\,\Omega$, $C_{\text{PI2}} = 100.1\text{ }\mu\text{F}$):**
  - Driven-node natural frequency: $f_{0,1} = \mathbf{1591.55\text{ Hz}}$ ($Z_{0,1} = 1.000\,\Omega$, $\zeta_{\text{driven,1}} = \mathbf{1.275}$, $Q_1 = \mathbf{0.392}$).
  - Symmetric tank natural frequency ($C_{\text{eq}} = 50\text{ }\mu\text{F}$): $f_{\text{tank,1}} = \mathbf{2250.79\text{ Hz}}$ ($Z_{0,\text{tank,1}} = 1.414\,\Omega$, $\zeta_{\text{tank,1}} = \mathbf{0.902}$, $Q_{\text{tank,1}} = \mathbf{0.555}$).
- **Pi-Filter #2 ($L_2 = 47\text{ }\mu\text{H}$, $R_{\text{tot2}} = 0.15 + 1.50 = 1.65\,\Omega$, $C_{\text{A2}} = 100.1\text{ }\mu\text{F}$):**
  - Driven-node natural frequency: $f_{0,2} = \mathbf{2321.51\text{ Hz}}$ ($Z_{0,2} = 0.686\,\Omega$, $\zeta_{\text{driven,2}} = \mathbf{1.203}$, $Q_2 = \mathbf{0.415}$).
  - Symmetric tank natural frequency ($C_{\text{eq}} = 50\text{ }\mu\text{F}$): $f_{\text{tank,2}} = \mathbf{3283.12\text{ Hz}}$ ($Z_{0,\text{tank,2}} = 0.970\,\Omega$, $\zeta_{\text{tank,2}} = \mathbf{0.851}$, $Q_{\text{tank,2}} = \mathbf{0.588}$).
- **Post-LDO ADC RC Filter ($R_{\text{ADC\_F}} = 10.0\,\Omega$, $C_{\text{ADC\_F}} = 10\text{ }\mu\text{F} \parallel 100\text{ nF} = 10.1\text{ }\mu\text{F}$):**
  - Corner frequency: $f_{c,\text{ADC}} = \frac{1}{2\pi (10.0\,\Omega)(10.1\text{ }\mu\text{F})} = \mathbf{1575.79\text{ Hz}}$.

### 4.2 Python-Verified Frequency Response Table (`100 Hz` to `10 MHz`)

| Frequency $f$ (Hz) | Pi-Filter #1 (`100µH + 2.55Ω`) $|H_1(j\omega)|$ (dB) | Pi-Filter #1 Phase $\angle H_1$ (deg) | Pi-Filter #2 (`47µH + 1.65Ω`) $|H_2(j\omega)|$ (dB) | Pi-Filter #2 Phase $\angle H_2$ (deg) | ADC RC Filter (`10Ω + 10.1µF`) $|H_{\text{RC}}|$ (dB) | Combined Passive Chain Pi-2 + RC (dB) | Physical Significance / Noise Source |
| ---: | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **`100.00 Hz`** | `-0.11 dB` | `-9.12°` | `-0.05 dB` | `-5.92°` | `-0.02 dB` | `-0.07 dB` | $100/120\text{ Hz}$ line-rectifier ripple passband |
| **`200.00 Hz`** | `-0.41 dB` | `-17.82°` | `-0.20 dB` | `-11.70°` | `-0.07 dB` | `-0.27 dB` | Monotonic roll-off (**$0.00\text{ dB}$ resonant peaking!**) |
| **`500.00 Hz`** | `-2.11 dB` | `-39.06°` | `-1.10 dB` | `-27.13°` | `-0.43 dB` | `-1.53 dB` | MCP23017 $500\text{ Hz}$ UI polling fundamental |
| **`1,000.00 Hz`** | `-5.47 dB` | `-59.26°` | `-3.31 dB` | `-44.68°` | `-1.52 dB` | `-4.82 dB` | $1\text{ kHz}$ Buzzer PWM audio fundamental |
| **`1,591.55 Hz`** | **`-8.75 dB`** | `-70.75°` | `-5.82 dB` | `-55.91°` | `-3.14 dB` | `-8.96 dB` | **Pi-Filter #1 Driven-Node $f_0$** ($\zeta = 1.275$, critically damped) |
| **`2,250.79 Hz`** | **`-11.56 dB`** | `-77.41°` | `-8.09 dB` | `-62.49°` | `-4.94 dB` | `-13.03 dB` | **Pi-Filter #1 Symmetric Tank $f_{\text{tank}}$** ($\zeta = 0.902$) |
| **`2,321.51 Hz`** | `-11.83 dB` | `-77.92°` | **`-8.30 dB`** | `-63.00°` | `-5.12 dB` | `-13.42 dB` | **Pi-Filter #2 Driven-Node $f_0$** ($\zeta = 1.203$, critically damped) |
| **`3,283.12 Hz`** | `-14.85 dB` | `-82.78°` | **`-10.74 dB`** | `-67.85°` | `-7.41 dB` | `-18.15 dB` | **Pi-Filter #2 Symmetric Tank $f_{\text{tank}}$** ($\zeta = 0.851$) |
| **`5,000.00 Hz`** | `-18.65 dB` | `-86.59°` | `-13.78 dB` | `-72.43°` | `-10.58 dB` | `-24.36 dB` | Audio harmonic band |
| **`10,000.00 Hz`** | `-24.93 dB` | `-89.24°` | `-19.03 dB` | `-78.99°` | `-16.25 dB` | `-35.28 dB` | Electrolytic ESR zero transition ($100\text{ nF}$ takes over) |
| **`50,000.00 Hz`** | `-39.06 dB` | `-90.55°` | `-32.54 dB` | `-88.10°` | `-28.76 dB` | `-61.29 dB` | $57.6\text{ kHz}$ `H11L1M` UART $115,200\text{ bps}$ fundamental |
| **`100,000.00 Hz`** | `-45.09 dB` | `-91.22°` | `-38.54 dB` | `-89.99°` | `-32.11 dB` | `-70.64 dB` | Standard $\text{I}^2\text{C}$ clock fundamental |
| **`500,000.00 Hz`** | `-59.13 dB` | `-96.26°` | `-52.57 dB` | `-96.01°` | `-34.15 dB` | `-86.71 dB` | Fast $\text{I}^2\text{C}$ / PFM sub-harmonic band |
| **`1,000,000.00 Hz`** | `-65.32 dB` | `-102.35°` | `-58.76 dB` | `-102.23°` | `-34.28 dB` | `-93.04 dB` | $1.0\text{ MHz}$ `H11L1M` max frequency rating |
| **`1,200,000.00 Hz`** | **`-67.00 dB`** | `-104.71°` | **`-60.44 dB`** | `-104.61°` | **`-34.33 dB`** | **`-94.76 dB`** | **`MT3608` $1.20\text{ MHz}$ Boost Switching Fundamental** |
| **`1,500,000.00 Hz`** | **`-69.11 dB`** | `-108.13°` | **`-62.55 dB`** | `-108.05°` | **`-34.39 dB`** | **`-96.94 dB`** | **`BQ25895` $1.50\text{ MHz}$ Buck Charger Fundamental** |
| **`3,000,000.00 Hz`** | `-76.35 dB` | `-122.71°` | `-69.79 dB` | `-122.67°` | `-34.88 dB` | `-104.67 dB` | `BQ25895` 2nd Harmonic ($3.0\text{ MHz}$) |
| **`5,000,000.00 Hz`** | `-82.76 dB` | `-135.67°` | `-76.20 dB` | `-135.65°` | `-35.84 dB` | `-112.04 dB` | High-order switching harmonics |
| **`10,000,000.00 Hz`** | `-93.08 dB` | `-149.54°` | `-86.52 dB` | `-149.53°` | `-38.65 dB` | `-125.17 dB` | SPI clock harmonics |

> [!TIP]
> **Model Precision Note:** At $1.2\text{ MHz}$ and $1.5\text{ MHz}$, the composite $100\text{ }\mu\text{F} (\text{ESR}=0.35\,\Omega) \parallel 100\text{ nF} (\text{ESR}=0.02\,\Omega)$ shunt model yields **$-67.00\text{ dB} / -69.11\text{ dB}$** on Pi-Filter #1 and **$-60.44\text{ dB} / -62.55\text{ dB}$** on Pi-Filter #2 (or conservatively **$-64.19\text{ dB} / -66.44\text{ dB}$** on Pi-1 and **$-57.63\text{ dB} / -59.88\text{ dB}$** on Pi-2 when including $0.50\,\Omega$ electrolytic ESR + THT lead inductance). In both models, total passive attenuation onto `+3V3_ADC` at $1.5\text{ MHz}$ exceeds **$-94.28\text{ dB}$ ($>51,000\times$)** prior to `U_LDO_ANA` PSRR!

---

## 5. Audio Buzzer RC Snubber (`R_SNUB`, `C_SNUB`, `D_BZ`) & Transient Suppression Sizing

### 5.1 Buzzer Stage Pin-by-Pin Netlist (Block 8 Audio Subsystem)

| RefDes | Part Number / Value | Tolerance & Rating | KiCad 9 Footprint (`Ge_Engine.pretty`) | Pin-by-Pin Net Connections (`Pin#: NetName`) | Functional Role & Sizing Proof |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`SW_BZ`** | `SPDT_MUTE_SLIDE` | $500\text{ mA} \text{ @ } 24\text{ VDC}$ Slide Switch | `SW_Slide_SPDT_THT` | `1 (MUTE): NC`, `2 (COM): BZ_POS`, `3 (ON): +3V3_DIG` | Physical series mute switch; breaks `+3V3_DIG` feed to `BZ1` |
| **`BZ1`** | `BUZZER_12MM_PASSIVE` | $3.3\text{ V}$, $30\text{ mA}$ Magnetic/Piezo ($R_{\text{coil}} \approx 42\,\Omega, L \approx 4.5\text{ mH}$ or $C_p \approx 20\text{ nF}$) | `Buzzer_12mm` | `1 (+): BZ_POS`, `2 (-): BZ_COL` | Acoustic continuity, pass/fail, and 56k-modem signature transducer |
| **`D_BZ`** | `1N4148` | $V_{\text{RRM}}=100\text{ V}$, $I_F=200\text{ mA}$, $t_{\text{rr}}=4\text{ ns}$ | `D_DO-35_P7.62mm` | `1 (Cathode): BZ_POS`, `2 (Anode): BZ_COL` | Fast inductive flyback clamp across `BZ1` ($V_{\text{clamp}} = 3.3\text{ V} + 0.7\text{ V} = 4.0\text{ V}$) |
| **`R_SNUB`** | `100R` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: BZ_POS`, `2: BZ_SNUB_MID` | Damps high-frequency LC/piezo turn-off ringing ($\zeta_{\text{snub}} > 1.0$) |
| **`C_SNUB`** | `10nF` | $\pm 10\%$, $50\text{ V}$ X7R Ceramic | `C_Disc_P5.00mm` | `1: BZ_SNUB_MID`, `2: BZ_COL` | Absorbs inductive/piezo edge energy ($\tau_{\text{snub}} = 1.0\text{ }\mu\text{s}$, $f_c = 159.2\text{ kHz}$) |
| **`Q_BZ`** | `2N3904` | $40\text{ V}$, $200\text{ mA}$ NPN ($1=\text{E}, 2=\text{B}, 3=\text{C}$) | `Socket_TO92_3Pin` | `1 (Emitter): DGND`, `2 (Base): Q_BZ_B`, `3 (Collector): BZ_COL` | Low-side buzzer driver ($V_{\text{CE(sat)}} \le 0.15\text{ V}$ at $I_C = 35\text{ mA}$) |
| **`R_BZ_B`** (`R_BZ`) | `1.0k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: BZ_PWM`, `2: Q_BZ_B` | Driven by `U_MCU2` Pin 13 (`GPIO3` LEDC PWM); $I_B = 2.53\text{ mA}$ |
| **`R_BZ_PD`** | `10k` | $\pm 1\%$, $0.25\text{ W}$ Metal Film | `R_Axial_P7.62mm` | `1: Q_BZ_B`, `2: DGND` | Base pull-down preventing boot/reset clicks |

### 5.2 Snubber & Flyback Transient Suppression Math
1. **Stored Inductive / Capacitive Energy per PWM Cycle ($f_{\text{PWM}} = 2.4\text{ kHz}$):**
   - For a magnetic coil transducer ($L_{\text{BZ}} \approx 4.5\text{ mH}$, $I_{\text{peak}} \approx 35\text{ mA}$):
     $$E_{\text{ind}} = \frac{1}{2} L_{\text{BZ}} I_{\text{peak}}^2 = 0.5 \times (4.5\times 10^{-3}\text{ H}) \times (0.035\text{ A})^2 = \mathbf{2.756\text{ }\mu\text{J}}$$
   - Upon `Q_BZ` turn-off, `D_BZ` (`1N4148`, $t_{\text{rr}} = 4\text{ ns}$) conducts the $35\text{ mA}$ circulating current within nanoseconds, clamping `BZ_COL` to $+3.30\text{ V} + 0.70\text{ V} = \mathbf{4.00\text{ V}}$ ($10\times$ below `2N3904` $BV_{\text{CEO}} = 40\text{ V}$).
2. **RC Snubber Damping of Parasitic Ringing (`R_SNUB = 100 Ω`, `C_SNUB = 10 nF`):**
   - For parasitic leakage inductance $L_{\text{leak}} \approx 5\text{ }\mu\text{H}$ and collector/piezo capacitance $C_p$, the snubber characteristic impedance is $\sqrt{L_{\text{leak}} / C_{\text{SNUB}}} = \sqrt{5\text{ }\mu\text{H} / 10\text{ nF}} = 22.36\,\Omega$.
   - Selecting `R_SNUB = 100 Ω` overdamps the turn-off transient ($\zeta = \frac{100}{2 \times 22.36} = \mathbf{2.24}$) while dissipating negligible power at $f_{\text{PWM}} = 4.0\text{ kHz}$ max:
     $$P_{\text{R\_SNUB}} = C_{\text{SNUB}} \, V_{\text{DD}}^2 \, f_{\text{PWM}} = (10\times 10^{-9}\text{ F}) \times (3.30\text{ V})^2 \times 4000\text{ Hz} = \mathbf{0.436\text{ mW}} \ll 250\text{ mW rating}$$
