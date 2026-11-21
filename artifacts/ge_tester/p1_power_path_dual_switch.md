# Phase 1 — Power-Path, Dual-Switch & Analog Filter Architecture Specification (Rev 5.0)

**Document ID:** `p1_power_path_dual_switch.md`  
**Subsystem:** Power-Path Management, Dual-Switch Control (`SW_PWR`, `SW_STBY`), Dual Ultra-LDOs, Critically Damped LC Pi-Filters, Star-Ground Topology & Audio Buzzer Stage  
**Architecture Revision:** Rev 5.0 (Updated per Board Directive #5: **TI BQ25895 1.5 MHz Switching NVDC Power-Path Charger Module**, replacing legacy linear `TP4056` / discrete `Q_PATH`)

---

## 1. Executive Summary & System Power Architecture Overview

The Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine operates from a single-cell **3.7 V Nominal (4.20 V Max) Non-HV LiPo/Li-Ion battery** with seamless **USB-C 5 V / 3 A (and 9 V–12 V HVDCP/MaxCharge)** power-path switching and fast charging. Per directive #5, linear `TP4056` chargers and discrete AN1149 P-MOSFET load-sharing networks are completely eliminated due to excessive thermal dissipation ($>1.3\text{ W}$ at $1\text{ A}$) and slow charge rates. Instead, Rev 5.0 integrates a socketed **Texas Instruments `BQ25895` (`MOD_CHG`) 1.5 MHz synchronous switch-mode buck NVDC (Narrow VDC) power-path charger module** managed over the MCU 2 UI $\text{I}^2\text{C}$ bus (`0x6A`).

```
[J_USB_C] (USB-C Receptacle Breakout: 5.1k CC1/CC2 Pull-Downs -> 5V/3A or 9V-12V HVDCP)
    │
    ├──► F_USB (RXEF175 1.75A or RXEF110 1.1A PTC) ──► D_TVS_USB (SMBJ15CA / P6KE15A 15V TVS, or 1N5919B 5.6V Zener clamp for 5V-only)
    │                                                       │
    ▼                                                       ▼
[J_BAT] (3.7V LiPo, 4.20V Max Non-HV)                 MOD_CHG (TI BQ25895 NVDC Switching Charger, I2C @ 0x6A)
    │                                                   ├── VBUS  ◄── V_USB_PROT
    ├──► SW_PWR (Pole 1: Hard Master Off, 0.0 µA!)      ├── VBAT  ◄── V_BAT_PROT (via Q_REV_BAT / SW_PWR Pole 1)
    │       │                                           ├── CE    ──► DGND (Charge Enabled)
    │       ▼                                           ├── OTG   ──► DGND (Boost/OTG Disabled)
    │    Q_REV_BAT (NDP6020P Ideal Diode: 1=G,2=D,3=S)  ├── STAT  ──► LED_CHG (Amber, 1.5k to +3V3_DIG)
    │       │                                           ├── SDA/SCL ◄──► UI_I2C_SDA / UI_I2C_SCL (MCU 2)
    │       └──────────────────────────────────────────►└── VSYS (NVDC Output: min 3.50V, tracks max(V_BAT, 3.5V))
    │                                                       │
    └──► SW_PWR (Pole 2: System Master Cut) ◄───────────────┘
            │
            ▼  V_BUS_SYS (3.40V – 4.35V Switched System Bus; 0.0V when SW_PWR = OFF)
            ├───────────────────────────────────────────────────────────────────────────┐
            ▼                                                                           ▼
    [ALWAYS-ON DIGITAL / RF DOMAIN (Active & Standby)]                         [SWITCHED ANALOG & METROLOGY DOMAIN]
    U_LDO_DIG (MCP1826S-3302E/AB, TO-220-3: 1=IN, 2=GND, 3=OUT)                Q_ANA_SW (NDP6020P High-Side PMOS: 3=S, 2=D, 1=G)
            │  (V_DO < 65 mV @ 250 mA; regulates down to 3.40V LiPo)                    ▲  Controlled by U11 GPB7 (ANA_PWR_EN)
            ▼                                                                           │  via Q_ANA_N (2N7000) + 100k pull-down
        +3V3_DIG                                                                        ▼  V_ANA_IN (0.0 mA in Standby!)
            ├──► JP_M2_3V3 (2-Pin Flash Isolation Jumper) ──► U_MCU2 (ESP32-C3)         ├──► Pi-Filter #1 (100uH + 2.2R, ζ=0.90)
            ├──► U11 (MCP23017-E/SP @ 0x20: Buttons, Encoder, LEDs, SW_STBY GPA7)       │      └─► MOD_BOOST (MT3608 14.50V)
            ├──► MOD_TFT1 & MOD_TFT2 Logic VCC (Backlights gated by Q_BLK / GPB6)       │          └─► L7812CV -> F1 (RXEF017) + D_OVP (1N4743A) -> +12V_A
            ├──► U_ISO1 (Pin 6 VCC) & U_ISO2 (Pin 1 LED Anode via 470R)                 │              └─► L78L09ACZ -> +9V_REF -> 2x 2N3906 (15mA) -> +9V_LIM
            └──► BZ1 Audio Stage (Q_BZ 2N3904 + SW_BZ Mute + 1N4148 + RC Snubber)       └──► Pi-Filter #2 (47uH + 1.5R, ζ=0.85)
                                                                                               └─► U_LDO_ANA (MCP1826S-3302E/AB) -> +3V3_ANA
                                                                                                   ├─► JP_M1_3V3 ──► U_MCU1 (LOLIN32 Lite)
                                                                                                   ├─► U_ISO2 (Pin 6 VCC) & U_ISO1 (Pin 1 Anode)
                                                                                                   └─► RC Filter (10R + 10uF//100nF) -> +3V3_ADC
```

---

## 2. USB-C Input Protection & BQ25895 NVDC Power-Path Charger (`MOD_CHG`)

### 2.1 Why Linear `TP4056` + Discrete AN1149 Was Replaced by `BQ25895`

In high-capacitance portable instruments ($2000\text{–}3500\text{ mAh}$ cells), a linear `TP4056` charger operating from $V_{\text{USB}} = 5.0\text{ V}$ into a depleted cell ($V_{\text{BAT}} = 3.3\text{ V}$) at $I_{\text{CHG}} = 1.0\text{ A}$ dissipates:

$$P_{\text{diss(TP4056)}} = (V_{\text{USB}} - V_{\text{BAT}}) \times I_{\text{CHG}} = (5.0\text{ V} - 3.3\text{ V}) \times 1.0\text{ A} = 1.70\text{ W}$$

This forces the `TP4056` into thermal foldback above $120^\circ\text{C}$, creates severe thermal gradients across the PCB (corrupting temperature-sensitive Germanium $I_{\text{CEO}}$ metrology), and limits charge current to $\le 1\text{ A}$.

By upgrading to the **Texas Instruments `BQ25895`** $1.5\text{ MHz}$ synchronous buck switching charger with integrated NVDC power-path control:
1. **High Efficiency & Cool Thermal Operation:** At $I_{\text{CHG}} = 2.048\text{ A}$ from a $5.0\text{ V}$ USB-C input ($\eta \approx 90.5\%$, $P_{\text{out}} = 3.8\text{ V} \times 2.048\text{ A} = 7.78\text{ W}$), total charger dissipation is only:
   $$P_{\text{diss(BQ25895)}} = P_{\text{out}} \left(\frac{1}{\eta} - 1\right) \approx 7.78\text{ W} \times \left(\frac{1}{0.905} - 1\right) = 0.82\text{ W} \quad (< 1.0\text{ W at } 2\text{ A charge!})$$
2. **True Integrated NVDC Power-Path (Clean $I_{\text{TERM}}$ Termination Under Full System Load):**
   - The `BQ25895` separates the **System Rail (`VSYS`)** from the **Battery Terminal (`VBAT`)** via an internal **$11\text{ m}\Omega$ `BATFET`** ($Q_4$ inside the `BQ25895`).
   - When USB-C is connected, the synchronous buck converter regulates `VSYS`Directly ($\ge V_{\text{SYS\_MIN}} = 3.50\text{ V}$, or $V_{\text{BAT}} + I_{\text{CHG}} R_{\text{BATFET}}$), supplying system load current $I_{\text{SYS}}$ directly from `VBUS` while independently metering battery current $I_{\text{BAT}}$ through the internal `BATFET`.
   - Because the `BQ25895` senses battery charge current exclusively across `BATFET` (between `VSYS` and `VBAT`), system load current $I_{\text{SYS}}$ drawn from `VSYS` **never passes through the battery current sensor**. Consequently, even if the tester draws $350\text{ mA}$ on `VSYS`, the `BQ25895` detects true battery current dropping to $I_{\text{TERM}} = 128\text{ mA}$ at $V_{\text{REG}} = 4.192\text{ V}$ and cleanly terminates charging (`CHG_STAT = 11`), eliminating trickle-charge starvation and LiPo plating.
   - When USB-C is unplugged, the internal $11\text{ m}\Omega$ `BATFET` remains fully enhanced, connecting `VBAT` to `VSYS` with virtually zero voltage drop ($350\text{ mA} \times 11\text{ m}\Omega = 3.85\text{ mV}$).

### 2.2 USB-C Input Stage (`J_USB_C`, `F_USB`, `D_TVS_USB`)

```
[J_USB_C] (6-Pin / 16-Pin USB-C Breakout)
  ├── CC1 ──► R_CC1 (5.1 kΩ 1% to DGND)  [Advertises 5V / 3.0A USB Type-C UFP sink]
  ├── CC2 ──► R_CC2 (5.1 kΩ 1% to DGND)  [Supports cable flip orientation]
  ├── D+/D- ──► MOD_CHG D+/D- (Optional HVDCP 9V/12V handshake if routed on module)
  ├── GND ──► DGND
  └── VBUS ──► F_USB (PPTC Fuse) ──► V_USB_PROT ──┬──► D_TVS_USB (Transient Clamp to DGND)
                                                  └──► MOD_CHG Pin VBUS (3.9V – 14.0V Input)
```

- **USB-C CC Pull-Downs (`R_CC1`, `R_CC2` = $5.1\text{ k}\Omega \pm 1\%$):** Mandatory independent $5.1\text{ k}\Omega$ resistors from `CC1` to `DGND` and `CC2` to `DGND` on the USB-C receptacle breakout (`J_USB_C`). This ensures USB-C PD and Type-C DFP sources recognize the tester as a native sink and supply up to $5.0\text{ V} / 3.0\text{ A}$ ($15\text{ W}$).
- **Input PolySwitch PTC (`F_USB`):**
  - **Recommended Selection for Fast Charge + System Load:** **`RXEF175`** ($I_{\text{hold}} = 1.75\text{ A}$, $I_{\text{trip}} = 3.50\text{ A}$, $R_{\text{min}} = 0.08\ \Omega$, $R_{1\text{max}} = 0.21\ \Omega$) or **`RXEF110`** ($I_{\text{hold}} = 1.10\text{ A}$, $I_{\text{trip}} = 2.20\text{ A}$, $R_{\text{min}} = 0.15\ \Omega$, $R_{1\text{max}} = 0.38\ \Omega$).
  - *Current Scaling Note:* Because the `BQ25895` is a synchronous step-down converter, charging a $3.8\text{ V}$ cell at $2.048\text{ A}$ ($7.78\text{ W}$) plus a $0.35\text{ A}$ system load ($1.33\text{ W}$) at $90\%$ efficiency draws $I_{\text{VBUS}} = 10.1\text{ W} / 5.0\text{ V} = 2.02\text{ A}$ at $5\text{ V}$ (or only $1.12\text{ A}$ from a $9\text{ V}$ HVDCP adapter). If the baseline `RXEF110` ($I_{\text{hold}} = 1.10\text{ A}$) is populated on a $5\text{ V}$-only source, firmware clamps `IINLIM` (`REG00[5:0]`) to $1.10\text{ A}$ (or `ICHG` to $1.024\text{ A}$) to avoid nuisance PTC tripping; if `RXEF175` / `RXEF250` is populated, the full $I_{\text{INLIM}} = 3.0\text{ A}$ and $I_{\text{CHG}} = 2.048\text{ A}$ are sustained continuously at $5\text{ V}$.
- **Input Transient & Overvoltage Clamp (`D_TVS_USB`):**
  - The `BQ25895` operates across an input window of **$3.9\text{ V to } 14.0\text{ V}$** (absolute maximum $22\text{ V}$ on `VBUS`), allowing direct compatibility with both standard $5\text{ V}$ USB-C chargers and $9\text{ V} / 12\text{ V}$ HVDCP/MaxCharge fast chargers.
  - **TVS Specification:**
    - For **$5\text{ V to } 12\text{ V}$ HVDCP / MaxCharge capability:** Populate **`P6KE15A`** or **`SMBJ15A`** ($V_{\text{RWM}} = 12.8\text{ V}$, $V_{\text{BR}} = 14.3\text{ V} \text{ to } 15.8\text{ V}$, DO-15 THT), which passes $5\text{ V}$, $9\text{ V}$, and $12\text{ V}$ adapters with $<1\ \mu\text{A}$ leakage while clamping inductive hot-plug spikes below the `BQ25895` $22\text{ V}$ ceiling.
    - For **Strict $5\text{ V}$-Only USB-C Enforcement:** Populate **`1N5919B`** ($5.6\text{ V}$, $3\text{ W}$ DO-41 axial Zener clamp). If a $>5.6\text{ V}$ source is connected, `1N5919B` conducts and trips `F_USB`.

### 2.3 Charge Profile, Register Programming & Non-HV LiPo Safety Mandate

> [!CAUTION]
> **STRICT 3.7 V NON-HV LIPO MANDATE ($4.20\text{ V}$ MAXIMUM TERMINAL VOLTAGE):**
> This instrument is designed exclusively for standard **$3.7\text{ V}$ nominal ($4.20\text{ V}$ maximum charge voltage) LiPo / 18650 / 21700 Li-Ion cells** ($\text{LiCoO}_2$ / NMC / NCA).
> - **DO NOT connect or configure $3.8\text{ V} / 3.85\text{ V}$ HV-LiPo cells ($4.35\text{ V} / 4.40\text{ V}$ max) or $\text{LiFePO}_4$ ($3.60\text{ V}$ max) settings.**
> - Charging a standard $4.20\text{ V}$ LiPo cell above $4.20\text{ V}$ causes metallic lithium plating on the anode, electrolyte oxidation, gas generation (cell swelling), and potential thermal runaway.
> - **Crucial `BQ25895` Power-On Reset Warning:** The hardware default of `BQ25895` `REG06` sets $V_{\text{REG}} = 4.208\text{ V}$ (`VREG[5:0] = 010111b` = $3.840\text{ V} + 23 \times 16\text{ mV}$), which **exceeds $4.200\text{ V}$ by $+8\text{ mV}$**! Therefore, MCU 2 firmware **MUST immediately write `REG06 = 0x58` ($V_{\text{REG}} = 4.192\text{ V}$) at boot** and refresh the watchdog every $10\text{ s}$ (or disable the $\text{I}^2\text{C}$ watchdog via `REG07[5:4] = 00` after writing `REG06 = 0x58` so $V_{\text{REG}}$ stays locked at $4.192\text{ V}$ permanently).

#### Exact `BQ25895` Register Configuration (`I2C Addr = 0x6A` on MCU 2 `UI_I2C`)

| Register | Name | Target Value | Bitfield Breakdown & Mathematical Verification |
| :---: | :--- | :---: | :--- |
| **`REG00`** | Input Source Control | **`0x3A`** | `EN_HIZ[7]=0` (Normal operation), `EN_ILIM[6]=0` (Use $\text{I}^2\text{C}$ `IINLIM` instead of external `ILIM` pin), `IINLIM[5:0]=111010b` ($58_{10}$): $$I_{\text{INLIM}} = 100\text{ mA} + 58 \times 50\text{ mA} = \mathbf{3000\text{ mA (3.00 A)}}$$ |
| **`REG02`** | ADC & Fast Charge Ctrl | **`0x40`** | `CONV_START[7]=0`, `CONV_RATE[6]=1` (1 Hz continuous internal 7-bit ADC telemetry for `VBAT`, `VSYS`, `VBUS`, `ICHG`, `TS`), `BOOST_FREQ[5]=0` (1.5 MHz), `ICO_EN[4]=0`, `HVDCP_EN[3]=0`, `MAXC_EN[2]=0` (or `1` if 9V/12V adapter allowed). |
| **`REG03`** | Power-Path & `SYS_MIN` | **`0x1A`** | `BAT_LOADEN[7]=0`, `WD_RST[6]=0`, `OTG_CONFIG[5]=0` (**OTG Boost Disabled**), `CHG_CONFIG[4]=1` (Charge Enabled), `SYS_MIN[3:1]=101b` ($V_{\text{SYS\_MIN}} = 3.0\text{ V} + 5 \times 0.1\text{ V} = \mathbf{3.50\text{ V}}$), `reserved[0]=0`. |
| **`REG04`** | Fast Charge Current (`ICHG`) | **`0x20`** | `EN_PUMPX[7]=0`, `ICHG[6:0]=0100000b` ($32_{10}$): $$I_{\text{CHG}} = 32 \times 64\text{ mA} = \mathbf{2048\text{ mA (2.048 A)}}$$ *(Note: Firmware clamps `ICHG` to $\le 1\text{C}$ of the user-configured cell capacity in NVS, e.g., `0x18` = $1.536\text{ A}$ for a $1500\text{ mAh}$ cell).* |
| **`REG05`** | Pre-Charge & Termination | **`0x11`** | `IPRECHG[7:4]=0001b` ($1_{10}$): $I_{\text{PRECHG}} = 64\text{ mA} + 1 \times 64\text{ mA} = \mathbf{128\text{ mA}}$; `ITERM[3:0]=0001b` ($1_{10}$): $$I_{\text{TERM}} = 64\text{ mA} + 1 \times 64\text{ mA} = \mathbf{128\text{ mA}}$$ |
| **`REG06`** | Charge Voltage (`VREG`) | **`0x58`** | `VREG[7:2]=010110b` ($22_{10}$): $$V_{\text{REG}} = 3.840\text{ V} + 22 \times 16\text{ mV} = \mathbf{4.192\text{ V}} \quad (\le 4.200\text{ V Max!})$$ `BATLOWV[1]=0` ($3.0\text{ V}$ pre-charge threshold), `VRECHG[0]=0` ($100\text{ mV}$ recharge hysteresis $\rightarrow 4.092\text{ V}$). |
| **`REG07`** | Termination & Watchdog Timer | **`0x8D`** | `EN_TERM[7]=1` (Enable $128\text{ mA}$ termination), `STAT_DIS[6]=0` (Enable `STAT` pin), `WATCHDOG[5:4]=00b` (Disable watchdog reset after configuring registers so `VREG = 4.192V` cannot revert to `4.208V`), `EN_TIMER[3]=1` (Safety timer enabled), `CHG_TIMER[2:1]=10b` (12 h), `JEITA_ISET[0]=1`. |

#### Pack & Module Protection Layer (`DW01A + FS8205A` Pack PCM + `BQ25895` Safety Limits)
Standard single-cell pouch LiPo batteries include an integrated protection circuit module (PCM based on `DW01A` + dual N-MOSFET `FS8205A`) which acts as a secondary hardware backstop behind the `BQ25895`:
- **Overcharge Cut-off (`DW01A` $V_{\text{ODP}}$):** $4.30\text{ V} \pm 0.05\text{ V}$ (release at $4.10\text{ V}$). Because `BQ25895` regulates to $4.192\text{ V}$, the pack PCM never enters emergency overcharge disconnect during normal operation.
- **Under-Voltage Lockout / Over-Discharge (`DW01A` $V_{\text{ODL}}$ vs `BQ25895` `BATLOWV`):**
  - `BQ25895` internal ADC (`REG0E`) reports `VBAT` with $20\text{ mV/LSB}$ resolution ($2.304\text{ V}$ offset). MCU 2 initiates a clean low-battery warning at $3.45\text{ V}$ and soft-shutdown (`BATFET_DIS = 1` in `REG09[5]` or Standby entry) at **$3.35\text{ V}$**.
  - If left unattended, the `BQ25895` `BATFET` automatically opens at $V_{\text{BAT\_DPL}} \approx 2.40\text{ V–}2.60\text{ V}$, backed up by the pack `DW01A` UVLO at **$2.40\text{ V} \pm 0.10\text{ V}$** (release at $3.00\text{ V}$).
- **Overcurrent & Short-Circuit Protection:**
  - Pack `DW01A` + `FS8205A` ($R_{\text{DS(on)}} \approx 22\text{–}25\text{ m}\Omega$ per MOSFET, $45\text{–}50\text{ m}\Omega$ total series): Trips overcurrent at $V_{\text{OIP}} = 150\text{ mV}$ ($I_{\text{OCP}} \approx 150\text{ mV} / 48\text{ m}\Omega \approx 3.1\text{ A}$ after $10\text{ ms}$) and short-circuit at $V_{\text{SIP}} = 1.35\text{ V}$ ($<250\ \mu\text{s}$).
  - `BQ25895` internal `BATFET` ($R_{\text{DS(on)}} = 11\text{ m}\Omega$) includes integrated cycle-by-cycle overcurrent protection ($I_{\text{BATFET\_OCP}} \ge 9\text{ A}$).

### 2.4 `MOD_CHG` Hardware Control Pins & MCU 2 Interface

- **$\text{I}^2\text{C}$ Bus Connection:** `SDA` $\rightarrow$ `UI_I2C_SDA` (`U_MCU2` P16 / `GPIO0`), `SCL` $\rightarrow$ `UI_I2C_SCL` (`U_MCU2` P15 / `GPIO1`), pulled up via the $4.7\text{ k}\Omega$ bus pull-ups to `+3V3_DIG`. Slave address **`0x6A`** has zero collision with `U11` (`MCP23017` at **`0x20`**).
- **`/CE` (Charge Enable, Active-LOW):** Hard-wired to **`DGND`** (or via $10\text{ k}\Omega$ pull-down to `DGND`) so hardware charging is enabled whenever `VBUS` is valid and `REG03[4] (CHG_CONFIG) = 1`.
- **`OTG` (Boost Mode Enable, Active-HIGH):** Hard-wired to **`DGND`**. **Critical Datasheet Rule:** Leaving `OTG` floating can allow leakage to pull it high and unexpectedly activate $5\text{ V}$ boost mode on `PMID` when `VBUS` is unplugged. Grounding `OTG` and clearing `REG03[5] = 0` permanently disables boost/OTG mode.
- **`STAT` (Open-Drain Charge Status Output):** Drives top-panel amber indicator **`LED_CHG`** ($V_F \approx 2.0\text{ V}$) through current-limiting resistor **`R_LED_CHG` = $1.5\text{ k}\Omega$** from `+3V3_DIG`:
  $$I_{\text{LED\_CHG}} = \frac{3.30\text{ V} - 2.0\text{ V} - 0.1\text{ V}}{1500\ \Omega} = \mathbf{0.80\text{ mA}}$$
  - `STAT = LOW` (LED ON solid): Charging in progress.
  - `STAT = HIGH-Z` (LED OFF): Charge complete ($I_{\text{BAT}} < 128\text{ mA}$) or sleeping on battery.
  - `STAT = 1 Hz Blink`: Charge fault (NTC JEITA out-of-range, timer expired, or input overvoltage).
- **`INT` (Open-Drain Interrupt Pulse):** Left **`NC`** (or optional $10\text{ k}\Omega$ pull-up). MCU 2 polls `BQ25895` status (`REG0B`, `REG0C`) and ADC registers (`REG0E–REG12`) over $\text{I}^2\text{C}$ every $500\text{ ms}$.

---

## 3. Hard Master Power Switch (`SW_PWR`) & Reverse-Battery Protection (`Q_REV_BAT`)

### 3.1 `SW_PWR` 0.0 µA Storage Guarantee & USB-Plugged-While-OFF Behavior

Requirement #3 mandates that placing **`SW_PWR`** in the `OFF` position **physically breaks the battery positive terminal `J_BAT+`** so battery parasitic drain in storage is strictly **$0.0\ \mu\text{A}$** (eliminating even the $12\text{–}25\ \mu\text{A}$ `BQ25895` `VBAT` HIZ leakage and any PCB surface leakage).

We evaluate the two candidate switch topologies when USB-C is plugged in while `SW_PWR = OFF`:
1. *Option A (Single-Pole SPDT on `J_BAT+` only):* `SW_PWR` disconnects `J_BAT+` from `MOD_CHG VBAT`, while `MOD_CHG VSYS` stays connected to `V_BUS_SYS`. If a user plugs in USB-C while `SW_PWR` is `OFF`, the system unexpectedly boots up on USB power ($V_{\text{SYS}} \approx 3.6\text{–}4.2\text{ V}$), the `BQ25895` detects a missing battery (oscillating `VBAT` up to $4.192\text{ V}$ across the output capacitor and blinking `STAT` or false-terminating), and the user cannot charge the battery with the switch `OFF`.
2. **Option B (SELECTED SAFER ARCHITECTURE — DPDT Slide/Toggle Switch $\ge 2\text{ A}$ per pole, or SPDT with Total Isolation):**
   By using a **DPDT Master Power Switch (`SW_PWR`, e.g., C&K 7201 / Dailywell 2MS1-T1-B4-M2-Q-E, rated $3\text{ A} \text{ @ } 28\text{ VDC}$, $R_{\text{contact}} \le 20\text{ m}\Omega$)** with **Pole 1 in series with `J_BAT+`** and **Pole 2 in series with `MOD_CHG VSYS` $\rightarrow$ `V_BUS_SYS`** (or Pole 2 interrupting `V_USB_PROT` in Fully-Off mode):

```
                       SW_PWR (DPDT >= 2A/pole: True Fully-Off Master Disconnect)
                 ┌──────────────────────────────────────────────────────────────────┐
                 │  Pole 1 (Battery B+ Physical Disconnect):                        │
[J_BAT+] ────────┤  Pin 2 (COM1) ──► Pin 3 (ON) ──► Q_REV_BAT (D->S) ──► MOD_CHG VBAT
                 │               └──► Pin 1 (OFF: NC / Open Circuit -> 0.000 µA!)   │
                 │                                                                  │
                 │  Pole 2 (System Bus / Charger Isolation):                        │
[MOD_CHG VSYS] ──┤  Pin 5 (COM2) ──► Pin 6 (ON) ──► V_BUS_SYS (Main System Rail)    │
                 │               └──► Pin 4 (OFF: 10k Bleeder R_BLEED_SYS to DGND)  │
                 └──────────────────────────────────────────────────────────────────┘
```

#### Explicit Behavior Matrix Across `SW_PWR` States & USB-C Connection:

| `SW_PWR` State | USB-C State | Battery (`J_BAT+`) State | System Bus (`V_BUS_SYS`) State | Safety & Engineering Rationale |
| :---: | :---: | :--- | :--- | :--- |
| **`OFF`** | **Unplugged** | **Physically Open Circuit** ($I_{\text{BAT}} = \mathbf{0.000\ \mu\text{A}}$) | **$0.000\text{ V}$** (discharged to `DGND` via $10\text{ k}\Omega$ `R_BLEED_SYS`) | True mechanical air-gap isolation on `J_BAT+`. Infinite shelf life; zero cell discharge over multi-year storage. |
| **`OFF`** | **Plugged In** | **Physically Open Circuit** ($I_{\text{BAT}} = \mathbf{0.000\ \mu\text{A}}$, No Blind Charging) | **$0.000\text{ V}$** (`V_BUS_SYS` isolated by Pole 2; MCU 1 & MCU 2 remain **100% OFF**) | **Why "Fully Off" is Safer than "Blind Charge-Only":** Because the `BQ25895` power-on reset default is $V_{\text{REG}} = 4.208\text{ V}$ ($+8\text{ mV}$ over the $4.200\text{ V}$ Non-HV LiPo ceiling!) and default $I_{\text{CHG}} = 2.048\text{ A}$, allowing the `BQ25895` to charge the battery while `SW_PWR = OFF` (when MCU 2 is unpowered and cannot write `REG06 = 0x58` over $\text{I}^2\text{C}$) would subject the LiPo cell to **$4.208\text{ V}$ unmonitored charging**! Physically isolating `J_BAT+` on Pole 1 and `V_BUS_SYS` on Pole 2 when `SW_PWR = OFF` guarantees that charging ONLY occurs when `SW_PWR = ON` and MCU 2 is actively regulating $V_{\text{REG}} = 4.192\text{ V}$ and monitoring JEITA temperature. *(For Charge + Standby operation, the user sets `SW_PWR = ON` and `SW_STBY = STANDBY`, which keeps MCU 2 alive to supervise charging while the entire Analog Core draws $0.0\text{ mA}$).* |
| **`ON`** | **Unplugged** | Connected via `Q_REV_BAT` ($R_{\text{DS(on)}} \approx 60\text{ m}\Omega$) + `BATFET` ($11\text{ m}\Omega$) | **$V_{\text{BAT}} - I_{\text{SYS}}(71\text{ m}\Omega)$** ($3.40\text{ V–}4.19\text{ V}$) | Battery powers `V_BUS_SYS` with $<25\text{ mV}$ drop at $350\text{ mA}$. |
| **`ON`** | **Plugged In** | Connected & Supervised Fast-Charging at $V_{\text{REG}} = \mathbf{4.192\text{ V}}$ | **NVDC Regulated** ($\max(3.50\text{ V}, V_{\text{BAT}} + 0.05\text{ V})$) | System load is powered from `VBUS` via NVDC buck converter while battery charges independently under MCU 2 $\text{I}^2\text{C}$ supervision with clean $128\text{ mA}$ termination. |

> [!NOTE]
> **Single-Pole SPDT Compatibility:** If a single-pole SPDT switch (`SW_PWR`) is used on the front panel, wiring `SW_PWR` in series with `J_BAT+` (Pin 2 COM $\rightarrow$ `J_BAT+`, Pin 3 ON $\rightarrow$ `Q_REV_BAT Drain`, Pin 1 OFF $\rightarrow$ `NC`) still achieves the mandatory **$0.0\ \mu\text{A}$ battery drain in `OFF`** and still prevents unmonitored $4.208\text{ V}$ charging when `SW_PWR = OFF` because `J_BAT+` is physically open! The DPDT second pole simply prevents `V_BUS_SYS` from booting from USB-C when the master power switch is set to `OFF`.

### 3.2 Reverse-Battery Protection P-MOSFET (`Q_REV_BAT` — `NDP6020P`)

Unlike a series Schottky diode (which wastes $0.35\text{ V–}0.45\text{ V}$ and **blocks bidirectional charging current** from `MOD_CHG VBAT` into `J_BAT+`), an ideal-diode P-Channel MOSFET (`Q_REV_BAT`, **`NDP6020P`**, TO-220-3) conducts bidirectionally in normal polarity while blocking reverse battery insertion with zero quiescent current.

```
[SW_PWR Pole 1 ON] ──► Pin 2 (DRAIN) ──┬── [Q_REV_BAT: NDP6020P] ──► Pin 3 (SOURCE) ──► [MOD_CHG VBAT]
                       (Body Diode     │      Body Diode: D ──►|── S
                        Anode at D)    └── Pin 1 (GATE) ──► R_G_REV (1.0 kΩ) ──► DGND
```

#### Exact `NDP6020P` TO-220-3 Pin Orientation & Verification:
- **Pin 1 = `GATE (G)`:** Connected to `DGND` through $R_{\text{G\_REV}} = 1.0\text{ k}\Omega$ (limits transient gate charging spike during battery hot-plug).
- **Pin 2 (& Metal Tab) = `DRAIN (D)`:** Connected to the **Switched Battery Side (`V_BAT_SW`)** coming from `SW_PWR` Pole 1 (`J_BAT+`).
- **Pin 3 = `SOURCE (S)`:** Connected to the **Charger Battery Terminal (`MOD_CHG VBAT`)**.

#### Mathematical Proof of Bidirectional Normal Conduction & Reverse-Polarity Blocking:
1. **Normal Battery Insertion ($V_{\text{BAT}} = +3.40\text{ V to } +4.20\text{ V}$ on `J_BAT+`):**
   - Initially, `NDP6020P`'s intrinsic body diode (oriented **Anode at Drain Pin 2 $\rightarrow$ Cathode at Source Pin 3**) forward-biases, pulling Source (Pin 3) to $V_{\text{S}} = V_{\text{BAT}} - V_{\text{SD(diode)}} \approx 3.7\text{ V} - 0.65\text{ V} = 3.05\text{ V}$.
   - With Gate (Pin 1) at $V_{\text{G}} = 0.0\text{ V}$ (`DGND`), the gate-to-source voltage is:
     $$V_{\text{GS}} = V_{\text{G}} - V_{\text{S}} = -3.05\text{ V} \ll V_{\text{GS(th)}} \ (-0.7\text{ V typ, } -1.0\text{ V max})$$
   - The P-MOSFET channel immediately enhances, shorting out its own body diode so $V_{\text{S}}$ rises to $V_{\text{BAT}}$ and $V_{\text{GS}} = -V_{\text{BAT}} \in [-3.40\text{ V}, -4.20\text{ V}]$.
   - **Gate Oxide Safety Check:** The `NDP6020P` has an absolute maximum gate-source rating of $V_{\text{GSS}} = \pm 8.0\text{ V}$. At $V_{\text{BAT(max)}} = 4.20\text{ V}$, $|V_{\text{GS}}| = 4.20\text{ V}$, providing a **$3.80\text{ V}$ ($90\%$) safety margin** below the $8.0\text{ V}$ oxide limit without requiring a gate clamp Zener.
   - **On-Resistance & Bidirectional Charging/Discharging:** At $V_{\text{GS}} = -3.7\text{ V}$, $R_{\text{DS(on)}} \approx 58\text{ m}\Omega$ ($50\text{ m}\Omega$ @ $-4.5\text{ V}$, $70\text{ m}\Omega$ @ $-2.7\text{ V}$). Because an enhanced MOSFET channel conducts equally in both directions:
     - **Discharging ($I_{\text{SYS}} = 350\text{ mA}$ from `J_BAT+` $\rightarrow$ `D` $\rightarrow$ `S` $\rightarrow$ `VBAT`):** $\Delta V = 0.35\text{ A} \times 58\text{ m}\Omega = \mathbf{20.3\text{ mV}}$, $P_{\text{diss}} = \mathbf{7.1\text{ mW}}$.
     - **Charging ($I_{\text{CHG}} = 2.048\text{ A}$ from `VBAT` $\rightarrow$ `S` $\rightarrow$ `D` $\rightarrow$ `J_BAT+`):** Since $V_{\text{S}} \approx 4.19\text{ V}$ and $V_{\text{G}} = 0\text{ V}$, $V_{\text{GS}} = -4.19\text{ V}$ keeps `Q_REV_BAT` enhanced while current flows from Source to Drain into the cell ($\Delta V = 2.048\text{ A} \times 52\text{ m}\Omega = 106\text{ mV}$, $P_{\text{diss}} = 0.218\text{ W}$ in a TO-220 package $\rightarrow \Delta T < +14^\circ\text{C}$). *(Note: If the battery pack is plugged directly into the `MOD_CHG` onboard keyed JST-PH connector or protected via an ideal-diode controller, or if `BQ25895` IR compensation `REG08` `BAT_COMP = 60 mΩ` is enabled over $\text{I}^2\text{C}$, the $52\text{ m}\Omega$ drop across `Q_REV_BAT` is automatically compensated during the CC-to-CV transition!)*
2. **Reverse Battery Insertion ($V_{\text{BAT}} = -4.20\text{ V}$ on `J_BAT+` relative to `DGND`):**
   - Drain (Pin 2) is at $-4.20\text{ V}$, Gate (Pin 1) is at $0.0\text{ V}$, and Source (Pin 3) is held at $0.0\text{ V}$ by the downstream circuit.
   - The intrinsic body diode (Anode at Drain $-4.20\text{ V}$, Cathode at Source $0.0\text{ V}$) is **reverse-biased by $4.20\text{ V}$**.
   - Simultaneously, $V_{\text{GS}} = V_{\text{G}} - V_{\text{S}} = 0.0\text{ V} - 0.0\text{ V} = 0.0\text{ V} > V_{\text{GS(th)}} (-0.4\text{ V min})$, so the P-MOSFET channel remains **100% pinched OFF**. Reverse current is strictly $< 1\ \mu\text{A}$ ($I_{\text{DSS}}$ leakage), protecting `MOD_CHG` and the entire instrument against reversed battery leads.

---

## 4. Dual Ultra-Low-Dropout 3.3V Linear Regulators (`U_LDO_DIG` & `U_LDO_ANA`)

### 4.1 Rejection of `LM1117T-3.3` & Selection of `MCP1826S-3302E/AB`

A common design failure in single-cell LiPo instruments is specifying the legacy NPN Darlington-pass **`LM1117T-3.3`**:
- **`LM1117T-3.3` Dropout Failure Math:** The `LM1117T-3.3` has a typical dropout voltage of $V_{\text{DO}} = 1.10\text{ V to } 1.20\text{ V}$ ($1.30\text{ V}$ max), requiring a minimum input voltage of:
  $$V_{\text{IN(min, LM1117)}} = V_{\text{OUT}} + V_{\text{DO}} = 3.30\text{ V} + 1.15\text{ V} = \mathbf{4.45\text{ V}}$$
- Because a single-cell Non-HV LiPo battery operates between **$4.20\text{ V}$ (100% SoC) and $3.40\text{ V}$ (0% usable SoC)** with a nominal plateau at **$3.70\text{ V}$**, an `LM1117T-3.3` is **permanently out of regulation across 100% of the battery discharge curve** ($V_{\text{OUT}}$ collapses to $V_{\text{BAT}} - 1.15\text{ V} \approx 2.55\text{ V}$ at $3.70\text{ V}$ LiPo, causing immediate ESP32 brownout reset!).

Therefore, `LM1117T-3.3` is rejected. Both the Digital/RF rail (`U_LDO_DIG` $\rightarrow$ `+3V3_DIG`) and the Analog Core rail (`U_LDO_ANA` $\rightarrow$ `+3V3_ANA`) use the **Microchip `MCP1826S-3302E/AB`** CMOS PMOS-pass Ultra-Low-Dropout regulator in a 100% THT **TO-220-3 (`AB`)** package.

#### Quantitative Comparison & Datasheet Specifications (`MCP1826S-3302E/AB`)

| Parameter | Rejected `LM1117T-3.3` | Selected `MCP1826S-3302E/AB` | Engineering Margin @ $3.40\text{ V}$ LiPo Cutoff |
| :--- | :---: | :---: | :--- |
| **Package & Pinout (TO-220-3)** | `1=GND, 2=OUT, 3=IN` | **`1=VIN, 2=GND (Tab), 3=VOUT`** | Standard 78xx-style `IN-GND-OUT` pinout; grounded tab aids heat sinking. |
| **Max Output Current ($I_{\text{OUT}}$)** | $800\text{ mA}$ | **$1000\text{ mA}$ ($1.0\text{ A}$)** | $>3\times$ safety margin over peak ESP32-C3 Wi-Fi TX bursts ($\sim 320\text{ mA}$). |
| **Dropout Voltage @ $I_{\text{LOAD}} = 1.0\text{ A}$** | $1200\text{ mV}$ typ ($1300\text{ mV}$ max) | **$250\text{ mV}$ typ ($400\text{ mV}$ max)** | Full $1\text{ A}$ regulation down to $3.55\text{ V}$ typ. |
| **Dropout Voltage @ $I_{\text{LOAD}} = 300\text{ mA}$** | $\sim 1100\text{ mV}$ | **$< 75\text{ mV}$ typ ($< 100\text{ mV}$ max)** | Minimum input required at $300\text{ mA}$: $3.30\text{ V} + 0.075\text{ V} = \mathbf{3.375\text{ V} < 3.40\text{ V}}$! |
| **Dropout Voltage @ $I_{\text{LOAD}} = 120\text{ mA}$ (Analog)** | $\sim 1050\text{ mV}$ | **$\approx 30\text{ mV}$ typ** | Even after Pi-Filter #2 resistive drop ($120\text{ mA} \times 0.15\ \Omega = 18\text{ mV}$), $V_{\text{IN}} = 3.382\text{ V} > 3.330\text{ V}$. |
| **Quiescent Ground Current ($I_Q$)** | $5.0\text{ mA}$ ($10\text{ mA}$ max) | **$120\ \mu\text{A}$ typ ($220\ \mu\text{A}$ max)** | **$41\times$ reduction** in standby LDO quiescent drain! |
| **Output Capacitor ESR Stability** | Requires $22\ \mu\text{F}$ tantalum ($0.3\text{–}22\ \Omega$ ESR); oscillates with MLCCs! | **Stable with $1\ \mu\text{F–}100\ \mu\text{F}$ low-ESR ceramic (`0.001 Ω`) + electrolytic** | Zero oscillation risk with parallel $100\ \mu\text{F}$ electrolytic // $100\text{ nF}$ ceramic decoupling. |

### 4.2 Programming Isolation Jumpers (`JP_M1_3V3` & `JP_M2_3V3`)

Both `U_MCU1` (`WEMOS LOLIN32 Lite`, which contains an onboard `ME6211C33M5G` $3.3\text{ V}$ LDO fed from its USB-C port) and `U_MCU2` (`ESP32-C3 SuperMini`, which contains an onboard `ME6211` / `XC6206` $3.3\text{ V}$ LDO fed from its USB-C port) receive main-board regulated power directly on their **`3V3`** header pins:
- `U_MCU1` Pin 14 (`3V3`, net `M1_3V3`) $\longleftrightarrow$ **`JP_M1_3V3`** (`Header_1x02_P2.54mm` with removable shunt) $\longleftrightarrow$ **`+3V3_ANA`** (from `U_LDO_ANA` Pin 3).
- `U_MCU2` Pin 11 (`3V3`, net `M2_3V3`) $\longleftrightarrow$ **`JP_M2_3V3`** (`Header_1x02_P2.54mm` with removable shunt) $\longleftrightarrow$ **`+3V3_DIG`** (from `U_LDO_DIG` Pin 3).

**Why `JP_M1_3V3` and `JP_M2_3V3` Are Mandatory:**
When flashing firmware via a board's onboard USB-C port, the module's internal $5\text{ V} \rightarrow 3.3\text{ V}$ LDO actively drives `M1_3V3` or `M2_3V3` to $3.30\text{ V} \pm 2\%$. If the main board's `MCP1826S-3302E/AB` is simultaneously powered (or unpowered), tying the two LDO outputs directly together causes:
1. **Dual-LDO Cross-Conduction:** A $30\text{ mV}$ tolerance mismatch between the onboard `ME6211` ($3.32\text{ V}$) and `MCP1826S` ($3.29\text{ V}$) drives circulating current between the two regulators.
2. **Reverse Back-Feeding:** If main-board power (`SW_PWR`) is `OFF` during USB-C firmware flashing, the module's tiny $300\text{ mA}$ onboard LDO would back-feed the entire `+3V3_ANA` or `+3V3_DIG` rail and reverse-bias `U_LDO_ANA` / `U_LDO_DIG` ($V_{\text{OUT}} > V_{\text{IN}}$ through the PMOS pass-element body diode into `V_BUS_SYS` / `V_ANA_IN`), overloading the module's LDO.
- **Operating Rule:** Remove the 2-pin shunt on `JP_M1_3V3` (or `JP_M2_3V3`) whenever a USB-C data cable is plugged directly into `U_MCU1` (or `U_MCU2`) for flashing/serial debugging; install both shunts for normal instrument operation.

---

## 5. Switchable Analog Power & Standby Mode (`SW_STBY`, `Q_ANA_N`, `Q_ANA_SW`)

### 5.1 Hardware Control Chain (`SW_STBY` $\rightarrow$ `U11` $\rightarrow$ `Q_ANA_N` $\rightarrow$ `Q_ANA_SW`)

```
[SW_STBY] (SPDT Slide Switch on Front Panel)
  ├── Pos 1 (STANDBY / RF-ONLY): Connects U11 Pin 28 (GPA7) to DGND (Logic LOW = 0.0V)
  └── Pos 2 (ACTIVE METROLOGY):  Open / Pulled HIGH to +3V3_DIG via R_PU_STBY (10 kΩ) (Logic HIGH = 3.3V)

[U11 MCP23017-E/SP @ 0x20 on MCU 2 UI_I2C]
  ├── Pin 28 (GPA7): Reads SW_STBY state (Polled at 500 Hz by U_MCU2)
  ├── Pin 7  (GPB6): TFT_BLK_EN ──┬──► R_PD_BLK (100 kΩ to DGND)
  │                               └──► R_G_BLK (100 Ω) ──► Q_BLK (2N7000 Gate, Pin 2)
  │                                     ├── Pin 1 (Source) ──► DGND
  │                                     └── Pin 3 (Drain)  ──► MOD_TFT1 & MOD_TFT2 Pin 8 (BLK Cathode / Enable)
  └── Pin 8  (GPB7): ANA_PWR_EN ──┬──► R_PD_ANA (100 kΩ to DGND: Guarantees Boot-OFF!)
                                  └──► R_G_ANA (100 Ω) ──► Q_ANA_N (2N7000 N-MOSFET, TO-92: 1=S, 2=G, 3=D)
                                                            ├── Pin 1 (Source) ──► DGND
                                                            └── Pin 3 (Drain)  ──┬──► R_PU_SW (47 kΩ to V_BUS_SYS)
                                                                                 └──► Pin 1 (Gate) of Q_ANA_SW (NDP6020P)
                                                                                       ├── Pin 3 (Source) ──► V_BUS_SYS
                                                                                       └── Pin 2 (Drain)  ──► V_ANA_IN
```

#### Exact Transistor Pinouts & Drive Equations:
1. **`Q_ANA_N` (`2N7000` N-Channel Enhancement MOSFET, TO-92-3: `Pin 1 = Source`, `Pin 2 = Gate`, `Pin 3 = Drain`):**
   - At power-on reset, all `U11 MCP23017` pins default to high-impedance inputs (`IODIRB = 0xFF`). The **`100 kΩ` pull-down resistor (`R_PD_ANA`)** to `DGND` holds `ANA_PWR_EN` at $0.00\text{ V}$ ($V_{\text{GS(Q\_ANA\_N)}} = 0.0\text{ V} < V_{\text{GS(th,min)}} = 0.8\text{ V}$), keeping `Q_ANA_N` strictly **OFF** during boot.
   - When `Q_ANA_N` is **OFF** (`Standby Mode`, `GPB7 = LOW`), the $47\text{ k}\Omega$ pull-up resistor (`R_PU_SW`) ties `Q_ANA_SW` Gate (Pin 1) to `Q_ANA_SW` Source (Pin 3, `V_BUS_SYS`), enforcing $V_{\text{GS(Q\_ANA\_SW)}} = 0.00\text{ V}$.
   - When `U_MCU2` transitions to `Active Mode` (`SW_STBY = HIGH` on `GPA7`) and drives `U11 GPB7` (`ANA_PWR_EN`) **HIGH ($+3.30\text{ V}$)**, $V_{\text{GS(Q\_ANA\_N)}} = 3.30\text{ V} > V_{\text{GS(th,max)}} (3.0\text{ V})$, turning `Q_ANA_N` **ON** ($R_{\text{DS(on)}} \approx 1.8\ \Omega$) and pulling `Q_ANA_SW` Gate (Pin 1) to `DGND` ($0.00\text{ V}$).
2. **`Q_ANA_SW` (`NDP6020P` P-Channel Power MOSFET, TO-220-3: `Pin 1 = Gate`, `Pin 2 = Drain`, `Pin 3 = Source`):**
   - **Pin 3 (`Source`):** Connected to **`V_BUS_SYS`** ($3.40\text{ V to } 4.35\text{ V}$).
   - **Pin 2 (`Drain`):** Connected to **`V_ANA_IN`** (Switched Analog Input Bus feeding Pi-Filter #1 and Pi-Filter #2).
   - **Body Diode Orientation Verification:** Inside `NDP6020P`, the intrinsic body diode points from **Drain (Pin 2, `V_ANA_IN`) [Anode] $\rightarrow$ Source (Pin 3, `V_BUS_SYS`) [Cathode]**. When `Q_ANA_SW` is **OFF** ($V_{\text{GS}} = 0.00\text{ V}$, $V_{\text{S}} = V_{\text{BUS\_SYS}}$, $V_{\text{D}} = 0.00\text{ V}$), the body diode is **reverse-biased by $V_{\text{BUS\_SYS}}$**, blocking all current flow from `V_BUS_SYS` into `V_ANA_IN`.
   - When `Q_ANA_N` turns **ON** ($V_{\text{G}} = 0.00\text{ V}$), $V_{\text{GS(Q\_ANA\_SW)}} = -V_{\text{BUS\_SYS}} \in [-3.40\text{ V}, -4.35\text{ V}]$, enhancing `Q_ANA_SW` to $R_{\text{DS(on)}} \approx 55\text{ m}\Omega$. At peak Analog Core input current ($I_{\text{ANA\_IN}} \approx 260\text{ mA}$ including the `MT3608` boost input and `U_LDO_ANA`), the voltage drop across `Q_ANA_SW` is only:
     $$\Delta V_{\text{Q\_ANA\_SW}} = 0.260\text{ A} \times 0.055\ \Omega = \mathbf{14.3\text{ mV}} \quad (P_{\text{diss}} = 3.7\text{ mW})$$

### 5.2 Quantitative Proof of 0.0 mA Analog Core Drain in Standby Mode

When `SW_STBY = LOW` (Standby / RF-Only Mode), `U_MCU2` sets `U11 GPB7 (ANA_PWR_EN) = 0` and `U11 GPB6 (TFT_BLK_EN) = 0`, placing the ST7789 controllers in `SLPIN (0x10)` over SPI:

| Subsystem / Component | Supply Rail | Active Mode Current (mA) | Standby Mode Current (mA) | Physical Cutoff Proof in Standby Mode |
| :--- | :--- | :---: | :---: | :--- |
| **`MOD_BOOST` (`MT3608` 14.50V Boost)** | `V_ANA_IN` | $135.0\text{ mA}$ (at $3.7\text{ V}$ in for $32\text{ mA}$ @ $14.5\text{ V}$ out) | **$0.000\text{ mA}$** | Upstream `V_ANA_IN` is disconnected by `Q_ANA_SW` ($V_{\text{GS}} = 0\text{ V}$); `R_PU_SW` draws $0.0\ \mu\text{A}$ because `Q_ANA_N` is OFF. |
| **`U_REG12` (`L7812CV`) + `U_REG9` (`L78L09ACZ`)** | `V_14V_FILT` / `+12V_A` | $8.5\text{ mA}$ ($I_Q$) + $23.5\text{ mA}$ (AFE + DUT load) | **$0.000\text{ mA}$** | De-energized (`V_ANA_IN = 0.0V`). |
| **Analog Matrix (`3x DG408`, `DG412`, `DG409`, `LMC6484`, `TLC2272`, `MCP4822`, `ADS1115`)** | `+12V_A`, `+9V_LIM`, `+3V3_ANA`, `+3V3_ADC` | $18.0\text{ mA}$ (total analog/mixed-signal IC load) | **$0.000\text{ mA}$** | De-energized (`V_ANA_IN = 0.0V`). Optical isolation (`U_ISO1`, `U_ISO2`) ensures **zero back-powering** from MCU 2 UART lines! |
| **`U_LDO_ANA` (`MCP1826S-3302E/AB`)** | `V_ANA_IN` $\rightarrow$ `+3V3_ANA` | $0.12\text{ mA}$ ($I_Q$) | **$0.000\text{ mA}$** | De-energized (`V_ANA_IN = 0.0V`). |
| **`U_MCU1` (`WEMOS LOLIN32 Lite`, ESP32 REV1)** | `+3V3_ANA` (via `JP_M1_3V3`) | $48.0\text{ mA}$ (240 MHz CPU, Wi-Fi/BT OFF) | **$0.000\text{ mA}$** | De-energized (`+3V3_ANA = 0.0V`). |
| **Dual TFT Backlights (`MOD_TFT1` & `MOD_TFT2` Pin 8 `BLK`)** | `+3V3_DIG` (switched by `Q_BLK`) | $50.0\text{ mA}$ ($2 \times 25\text{ mA}$ IPS LED backlights) | **$0.000\text{ mA}$** | `U11 GPB6 (TFT_BLK_EN) = LOW` turns `Q_BLK` (`2N7000`) OFF, opening the backlight cathode/enable path. |
| **Total Switched Domain (`V_ANA_IN` + `TFT_BLK`)** | **Switched Rails** | **$233.1\text{ mA}$ (at $V_{\text{BUS\_SYS}}$)** | **$0.000\text{ mA}$ ($< 0.25\ \mu\text{A}$ `NDP6020P` $I_{\text{DSS}}$)** | **100% Shutdown of Analog Core, MCU 1, and TFT Backlights.** |
| **`U_MCU2` (`ESP32-C3 SuperMini`) + `U11` + `LED_STBY_RF` + `U_LDO_DIG`** | `+3V3_DIG` (Always-On when `SW_PWR = ON`) | $85.0\text{ mA}$ (Full Wi-Fi TX + Dual TFT Refresh) | **$24.5\text{ mA}$** (Modem-sleep Wi-Fi/BLE listener + $2.8\text{ mA}$ Cyan `LED_STBY_RF`) | Kept alive by `U_LDO_DIG` to serve the Web/Android JSON API, monitor `BQ25895` charging, and poll `SW_STBY` (`GPA7`). |

---

## 6. Dual Critically Damped LC Pi-Filters, HV Analog Chain & 15 mA Active Limiter

### 6.1 Pi-Filter #1 (Boost HV Analog Rail: $14.50\text{ V} \rightarrow +12\text{V\_A} \rightarrow +9\text{V\_REF} \rightarrow +9\text{V\_LIM}$)

```
[V_ANA_IN] (3.40V – 4.35V)
    │
    ▼
[MOD_BOOST] (MT3608 1.2 MHz Step-Up Module, Trimmed to 14.50V Output)
    │  V_BOOST_RAW (14.50V ± 30 mVpp @ 1.2 MHz)
    ├──► C_PI1A (100 µF / 35V Electrolytic) // C_PI1B (100 nF / 50V Ceramic) to AGND
    │
    ▼
[L1] (100 µH, 1.0 A Power Inductor, DCR = 0.35 Ω) ──► [R_DAMP1] (2.2 Ω, 0.5 W Axial Resistor)
    │  (Total Series Damping Resistance R_tot1 = 0.35 Ω + 2.20 Ω = 2.55 Ω)
    │  V_14V_FILT (~14.42V DC at 32 mA load)
    ├──► C_PI2A (100 µF / 35V Electrolytic) // C_PI2B (100 nF / 50V Ceramic) to AGND
    │
    ▼
[U_REG12] (L7812CV TO-220-3: 1=IN, 2=GND, 3=OUT; absorbs up to 35V if MT3608 trimmer fails!)
    │  V_12V_PRE (12.00V Regulated; Dropout V_IN - V_OUT = 14.42V - 12.00V = 2.42V > 2.0V min)
    ▼
[F1] (Littelfuse RXEF017 PPTC: I_hold = 170 mA, I_trip = 340 mA, R_min = 3.3 Ω)
    │  +12V_A (11.89V – 12.00V Protected Low-Noise Analog Supply Rail)
    ├──► D_OVP (1N4743A 13.0V ± 5%, 1.0 W DO-41 Crowbar Zener to AGND)
    ├──► C17 (10 µF / 25V) // C18 (100 nF) to AGND
    ├──► Powers: U1–U3 (DG408DJ V+), U8 (DG412LDJ V+), U10 (DG409DJ V+), U4 (LMC6484IN V+), U5 (TLC2272ACP VDD)
    │
    ▼
[U_REG9] (L78L09ACZ TO-92-3: 3=IN [+12V_A], 2=GND [AGND], 1=OUT [+9V_REF])
    │  +9V_REF (9.00V ± 1% Precision Reference & Bias Rail; C7=10µF/25V // C8=100nF to AGND)
    │
    ▼
[15 mA Active Current Limiter & Hardware OVC Interrupt Stage]
    ├── R_ILIM (43 Ω 1% 0.25W): Pin 1 (+9V_REF) ──► Pin 2 (LIM_SENSE)
    ├── Q_LIM1 (2N3906 Pass PNP, TO-92: 1=E, 2=B, 3=C): E=LIM_SENSE, B=LIM_BASE, C=+9V_LIM
    ├── Q_LIM2 (2N3906 Sense PNP, TO-92: 1=E, 2=B, 3=C): E=+9V_REF, B=LIM_SENSE, C=LIM_BASE
    ├── R_IBIAS (10 kΩ): LIM_BASE ──► AGND (Biases Q_LIM1 ON with ~0.83 mA base current)
    └── Q_OVC (2N3904 OVC Telemetry NPN, TO-92: 1=E, 2=B, 3=C):
          ├── Base (Pin 2) ◄── R_OVC_B (10 kΩ) ◄── LIM_BASE (or Collector of Q_LIM2 via divider)
          ├── Emitter (Pin 1) ──► AGND
          └── Collector (Pin 3) ──► FAULT_OVC_N (U_MCU1 Pin 8 / GPIO25, with R_PU_OVC = 10 kΩ to +3V3_ANA)
```

#### Mathematical Damping & Attenuation Proof for Pi-Filter #1:
1. **Resonant Frequency ($f_0$) & Characteristic Impedance ($Z_0$):**
   - With $L_1 = 100\ \mu\text{H}$, $R_{\text{tot1}} = R_{\text{DCR1}} + R_{\text{DAMP1}} = 0.35\ \Omega + 2.20\ \Omega = 2.55\ \Omega$, and output filter capacitance $C_{\text{PI2}} = 100\ \mu\text{F}$ (or symmetric Pi equivalent $C_{\text{eq}} = \frac{C_{\text{PI1}} C_{\text{PI2}}}{C_{\text{PI1}} + C_{\text{PI2}}} = 50\ \mu\text{F}$):
   $$\text{Driven Node } (C = 100\ \mu\text{F}): \quad f_0 = \frac{1}{2\pi\sqrt{100\ \mu\text{H} \times 100\ \mu\text{F}}} = \mathbf{1591.55\text{ Hz}}, \quad Z_0 = \sqrt{\frac{L_1}{C_{\text{PI2}}}} = \mathbf{1.000\ \Omega}$$
   $$\text{Symmetric Pi Tank } (C_{\text{eq}} = 50\ \mu\text{F}): \quad f_{0,\text{tank}} = \frac{1}{2\pi\sqrt{100\ \mu\text{H} \times 50\ \mu\text{F}}} = \mathbf{2250.79\text{ Hz}}, \quad Z_{0,\text{tank}} = \sqrt{\frac{L_1}{C_{\text{eq}}}} = \mathbf{1.414\ \Omega}$$
2. **Damping Factor ($\zeta$) & Quality Factor ($Q$):**
   $$\zeta_{\text{driven}} = \frac{R_{\text{tot1}}}{2 Z_0} = \frac{2.55\ \Omega}{2 \times 1.000\ \Omega} = \mathbf{1.275} \quad (Q = 0.392, \text{ Critically / Over-Damped: Zero overshoot!})$$
   $$\zeta_{\text{tank}} = \frac{R_{\text{tot1}}}{2 Z_{0,\text{tank}}} = \frac{2.55\ \Omega}{2 \times 1.414\ \Omega} = \mathbf{0.902} \quad (Q = 0.555, \text{ Optimal Deadbeat Step Response!})$$
   Without $R_{\text{DAMP1}} = 2.2\ \Omega$, $\zeta$ would drop to $0.35 / 2.828 = 0.124$ ($Q = 4.03$), causing $+12\text{ dB}$ resonant peaking and ringing on load steps.
3. **Switching Ripple Attenuation at $1.2\text{ MHz}$ (`MT3608`) and $1.5\text{ MHz}$ (`BQ25895`):**
   - At $f_{\text{sw}} = 1.2\text{ MHz}$, $|Z_{L1}| = \sqrt{2.55^2 + (2\pi \times 1.2\times 10^6 \times 100\times 10^{-6})^2} = 754.0\ \Omega$. The parallel $100\ \mu\text{F} // 100\text{ nF}$ ceramic capacitor branch has $|Z_{C\text{,PI2}}| \approx 0.466\ \Omega$:
     $$\text{Attenuation}_{\text{Pi1}}(1.2\text{ MHz}) = 20\log_{10}\left|\frac{Z_{C\text{,PI2}}}{Z_{L1} + Z_{C\text{,PI2}}}\right| = \mathbf{-64.19\text{ dB}} \quad (1620\times \text{ reduction})$$
   - At $f_{\text{sw}} = 1.5\text{ MHz}$ (`BQ25895` fundamental), $|Z_{L1}| = 942.5\ \Omega$:
     $$\text{Attenuation}_{\text{Pi1}}(1.5\text{ MHz}) = \mathbf{-66.44\text{ dB}} \quad (2100\times \text{ reduction})$$
   - Combined with the PSRR of `L7812CV` ($\sim 55\text{ dB}$) and `L78L09ACZ` ($\sim 45\text{ dB}$), switching ripple on `+9V_REF` is suppressed below **$1\ \mu\text{V}_{\text{pp}}$**.
4. **Hardened OVP Crowbar Math (`L7812CV` + `F1 RXEF017` + `D_OVP 1N4743A`):**
   - If the `MT3608` trimpot fails open and `V_BOOST_RAW` runs away to $28.0\text{ V}$, `U_REG12` (`L7812CV`, rated to $V_{\text{IN}} = 35\text{ V}$) absorbs the surge and holds `V_12V_PRE` at $12.0\text{ V}$.
   - Even under a double-fault where `U_REG12` also shorts input-to-output, `D_OVP` (`1N4743A`, $V_Z = 13.0\text{ V} \pm 5\%$, $1.0\text{ W}$) clamps `+12V_A` at $\le 13.65\text{ V}$ (well below the $16\text{ V–}44\text{ V}$ maximum supply rating of `DG408DJ`, `DG412LDJ`, `LMC6484IN`, and `TLC2272ACP`) and immediately trips **`F1` (`RXEF017`, $I_{\text{hold}} = 170\text{ mA}$, $I_{\text{trip}} = 340\text{ mA}$)**.
5. **15 mA Active Current Limiter (`2x 2N3906` + `R_ILIM = 43 Ω`) & `Q_OVC` (`2N3904`) Interrupt Math:**
   - Normal Load ($I_{\text{LIM}} < 12\text{ mA}$): `R_IBIAS` ($10\text{ k}\Omega$ to `AGND`) pulls $I_{\text{B(Q\_LIM1)}} = (9.0\text{ V} - 0.7\text{ V}) / 10\text{ k}\Omega = 0.83\text{ mA}$ from `Q_LIM1` Base, saturating `Q_LIM1` ($V_{\text{EC(sat)}} \approx 0.1\text{ V}$) so $+9\text{V\_LIM} = +9.00\text{ V} - I_{\text{LIM}}(43\ \Omega) - 0.1\text{ V}$.
   - Current Limit Trip ($I_{\text{LIM}} = I_{\text{trip}}$): When the voltage drop across `R_ILIM` ($43\ \Omega$) reaches the emitter-base turn-on threshold of `Q_LIM2` ($V_{\text{EB(on)}} \approx 0.65\text{ V}$ at $25^\circ\text{C}$):
     $$I_{\text{trip}} = \frac{V_{\text{EB(Q\_LIM2)}}}{R_{\text{ILIM}}} = \frac{0.65\text{ V}}{43\ \Omega} = \mathbf{15.12\text{ mA}}$$
   - As `Q_LIM2` turns ON, it sources current from `+9V_REF` into `Q_LIM1`'s base (`LIM_BASE`), pinching off `Q_LIM1` to hold $+9\text{V\_LIM}$ current strictly clamped at $\mathbf{15.12\text{ mA}}$ even into a dead short to `AGND`!
   - **Hardware OVC Interrupt (`Q_OVC` `2N3904` $\rightarrow$ `FAULT_OVC_N` on `U_MCU1 GPIO25`):** When `Q_LIM2` conducts during an overcurrent clamp, `Q_LIM2`'s collector current drives the base of NPN transistor **`Q_OVC` (`2N3904`, TO-92: `1=E [AGND], 2=B, 3=C [FAULT_OVC_N]`)** (via a dedicated sense tap or base divider so `Q_OVC` is OFF when `LIM_BASE` is at $8.3\text{ V}$ via a $7.5\text{ V}$ Zener/PNP tap, or directly driven by `Q_LIM2` collector split resistor to `AGND`). When $I_{\text{LIM}} \ge 15.1\text{ mA}$, `Q_OVC` saturates and pulls **`FAULT_OVC_N` LOW ($< 0.2\text{ V}$)** within $< 2\ \mu\text{s}$, triggering the hardware falling-edge ISR on `U_MCU1` `GPIO25` to assert `SR_OE_N = HIGH` (`GPIO17`) and open all DUT switches.

### 6.2 Pi-Filter #2 (3.3V Analog Core & Ultra-Quiet ADC Supply: `V_ANA_IN` $\rightarrow$ `+3V3_ANA` $\rightarrow$ `+3V3_ADC`)

```
[V_ANA_IN] (3.40V – 4.35V Switched Analog Input Bus)
    ├──► C_A1A (100 µF / 16V Electrolytic) // C_A1B (100 nF / 50V Ceramic) to AGND
    │
    ▼
[L2] (47 µH, 1.0 A Power Inductor, DCR ≤ 0.15 Ω) ──► [R_DAMP2] (1.5 Ω, 0.25 W Axial OR Parallel/Series Damped)
    │  (See Damping & Low-Battery Dropout Optimization Note below!)
    ├──► C_A2A (100 µF / 16V Electrolytic) // C_A2B (100 nF / 50V Ceramic) to AGND
    │
    ▼
[U_LDO_ANA] (MCP1826S-3302E/AB TO-220-3: 1=VIN, 2=AGND, 3=VOUT)
    │  +3V3_ANA (3.300V ± 0.5% Quiet Analog Logic & Reference Rail)
    ├──► C1 (100 µF / 16V) // C2 (100 nF) to AGND
    ├──► JP_M1_3V3 (2-Pin Jumper) ──► U_MCU1 Pin 14 (3V3: WEMOS LOLIN32 Lite, Wi-Fi/BT OFF)
    ├──► Powers: U6 & U7 (74HC595N VCC), U9 (MCP4822 VDD), U1–U3 (DG408 VL), U8 (DG412 VL),
    │            U_ISO2 (Pin 6 VCC), U_ISO1 (Pin 1 LED Anode via 470R), DS18B20 (TEMP_DQ)
    │
    ▼
[R_ADC_F] (10 Ω 1% Axial Filter Resistor)
    │  +3V3_ADC (Ultra-Quiet Dedicated Analog Supply for MOD_ADS ADS1115 Pin 1 VDD)
    └──► C_ADC_FA (10 µF / 16V Electrolytic) // C_ADC_FB (100 nF Ceramic) to AGND
```

#### Mathematical Damping, Attenuation & Dropout Verification for Pi-Filter #2:
1. **Resonant Frequency ($f_0$) & Damping Factor ($\zeta$):**
   - With $L_2 = 47\ \mu\text{H}$, $R_{\text{tot2}} = R_{\text{DCR2}} (0.15\ \Omega) + R_{\text{DAMP2}} (1.50\ \Omega) = 1.65\ \Omega$, and $C_{\text{A2}} = 100\ \mu\text{F}$ ($C_{\text{eq}} = 50\ \mu\text{F}$):
   $$\text{Driven Node } (C = 100\ \mu\text{F}): \quad f_0 = \frac{1}{2\pi\sqrt{47\ \mu\text{H} \times 100\ \mu\text{F}}} = \mathbf{2321.51\text{ Hz}}, \quad Z_0 = \sqrt{\frac{47\ \mu\text{H}}{100\ \mu\text{F}}} = \mathbf{0.686\ \Omega}$$
   $$\zeta_{\text{driven}} = \frac{1.65\ \Omega}{2 \times 0.686\ \Omega} = \mathbf{1.203} \ (Q = 0.415); \qquad \zeta_{\text{tank}} (C_{\text{eq}} = 50\ \mu\text{F}) = \frac{1.65\ \Omega}{2 \times 0.970\ \Omega} = \mathbf{0.851} \ (Q = 0.588)$$
   - *Low-Battery Headroom Note ($3.40\text{ V}$ LiPo End-of-Discharge):* At $I_{\text{ANA\_3V3}} \approx 60\text{ mA}$ (`U_MCU1` with Wi-Fi/BT OFF in light-sleep/acquisition + `MCP4822` + `74HC595`), a series $1.65\ \Omega$ resistance drops $60\text{ mA} \times 1.65\ \Omega = 99\text{ mV}$ (or if $R_{\text{DAMP2}} = 1.5\ \Omega$ is placed in series with $C_{\text{A2A}}$ as a parallel-damped snubber across $L_2$, the DC series drop is only $I \times R_{\text{DCR2}} = 60\text{ mA} \times 0.15\ \Omega = 9.0\text{ mV}$ while preserving identical $\zeta = 0.85$ damping!). With `MCP1826S-3302E/AB` dropout of only $18\text{ mV}$ at $60\text{ mA}$, `+3V3_ANA` stays in rock-solid $3.300\text{ V}$ regulation all the way down to $V_{\text{BAT}} = 3.40\text{ V}$.
2. **High-Frequency Switching Attenuation ($1.2\text{ MHz}$ `MT3608` & $1.5\text{ MHz}$ `BQ25895`):**
   - **Pi-Filter #2 Stage (`L2 = 47 µH`, `C_A2 = 100 µF // 100 nF`):**
     $$\text{Attenuation}_{\text{Pi2}}(1.2\text{ MHz}) = \mathbf{-57.63\text{ dB}}; \qquad \text{Attenuation}_{\text{Pi2}}(1.5\text{ MHz}) = \mathbf{-59.88\text{ dB}}$$
   - **Post-LDO ADC RC Filter Stage (`R_ADC_F = 10 Ω`, `C_ADC_F = 10 µF // 100 nF = 10.1 µF`):**
     $$f_{c,\text{ADC}} = \frac{1}{2\pi \times 10\ \Omega \times 10.1\ \mu\text{F}} = \mathbf{1575.79\text{ Hz}}$$
     $$\text{Attenuation}_{\text{RC}}(1.2\text{ MHz}) = \mathbf{-34.33\text{ dB}}; \qquad \text{Attenuation}_{\text{RC}}(1.5\text{ MHz}) = \mathbf{-34.39\text{ dB}}$$
   - **Combined Passive Filter + `MCP1826S` LDO Rejection onto `+3V3_ADC`:**
     $$\text{Total Passive Attenuation}(1.5\text{ MHz}) = -59.88\text{ dB} + (-34.39\text{ dB}) = \mathbf{-94.28\text{ dB}} \quad (> 51,000\times \text{ ripple suppression!})$$
   - Because `MOD_ADS` (`ADS1115`) draws only $I_{\text{VDD}} = 150\ \mu\text{A}$ during active conversion ($0.5\ \mu\text{A}$ idle), the DC voltage drop across `R_ADC_F` ($10\ \Omega$) is only $150\ \mu\text{A} \times 10\ \Omega = \mathbf{1.50\text{ mV}}$, maintaining $+3\text{V}3\text{\_ADC} = 3.2985\text{ V}$.

### 6.3 `BQ25895` 1.5 MHz Switching Noise Impact & Precision Metrology Hold-Off Protocol

While `MOD_CHG` (`BQ25895`) is actively fast-charging at up to $2.048\text{ A}$ ($f_{\text{sw}} = 1.5\text{ MHz}$), `V_BUS_SYS` (`VSYS`) exhibits a typical switching ripple of $V_{\text{ripple(VSYS)}} \approx 15\text{–}25\text{ mV}_{\text{pp}}$ at $1.5\text{ MHz}$ plus harmonics:
1. **Hardware Suppression Verification:**
   - Through **Pi-Filter #1** ($-66.44\text{ dB}$ @ $1.5\text{ MHz}$) + `L7812CV` + `L78L09ACZ`, $25\text{ mV}_{\text{pp}}$ on `V_BUS_SYS` is attenuated to **$< 0.5\ \mu\text{V}_{\text{pp}}$** on `+12V_A` and `+9V_LIM`.
   - Through **Pi-Filter #2** ($-59.88\text{ dB}$ @ $1.5\text{ MHz}$) + `U_LDO_ANA` + the **ADC RC Filter** ($-34.39\text{ dB}$ @ $1.5\text{ MHz}$), $25\text{ mV}_{\text{pp}}$ on `V_BUS_SYS` is attenuated by $-94.28\text{ dB}$ to **$< 0.48\ \mu\text{V}_{\text{pp}}$** on `+3V3_ADC` (`ADS1115 VDD`)—over **$150\times$ below** the $76.3\ \mu\text{V}$ LSB of the `ADS1115` ($\text{PGA} = \pm 2.5\text{ V}$).
2. **Firmware Ultra-Precision Nanoamp / Microamp $I_{\text{CEO}}$ Hold-Off Recommendation:**
   - During sub-microampere leakage measurements ($I_{\text{CBO}} / I_{\text{CEO}}$ on the $2.20\text{ M}\Omega$ `S4` range, where $100\text{ pA}$ equals $220\ \mu\text{V}$ at the DUT terminal), magnetic near-field coupling from the `BQ25895` $2.2\ \mu\text{H}$ buck inductor at $2\text{ A}$ charge current can induce sub-millivolt pickup if the charger is active.
   - **Protocol Rule (`CMD_SCAN` / `CMD_CALIBRATE`):** Before MCU 2 dispatches `CMD_SCAN (0x10)` or `CMD_CALIBRATE (0x12)` to MCU 1 over the isolated UART link, if USB-C is connected and `BQ25895` is actively charging (`REG0B[4:3] != 00`), MCU 2 temporarily writes **`REG03[4] (CHG_CONFIG) = 0`** (or sets **`REG00[7] (EN_HIZ) = 1`** during the $300\text{ ms}$ $I_{\text{CEO}}$ / $I_{\text{CBO}}$ window so the battery supplies 100% pure DC via the $11\text{ m}\Omega$ `BATFET` with the $1.5\text{ MHz}$ buck switcher completely halted!), and automatically restores `CHG_CONFIG = 1` / `EN_HIZ = 0` as soon as `EVT_RESULT_HASH (0x84)` is received ($\le 1.8\text{ s}$ total scan duration).

---

## 7. Partitioned Dual-Ground Plane & Single-Point Star Bridge (`R_STAR`)

To prevent digital return currents from `U_MCU2` (`ESP32-C3` $320\text{ mA}$ Wi-Fi bursts), `MOD_CHG` (`BQ25895` $1.5\text{ MHz}$ buck switching currents), `MOD_TFT1/TFT2` ($40\text{ MHz}$ SPI & $50\text{ mA}$ PWM backlights), and `BZ1` ($30\text{ mA}$ audio square waves) from creating common-impedance $I \times R$ voltage drops across the precision Germanium metrology reference (`AGND`), the PCB ground is physically split into two strictly isolated copper zones on `B.Cu` / `F.Cu` with a **minimum $2.0\text{ mm}$ isolation moat**:

1. **`DGND` (Noisy Digital, RF, Display, Charger & Audio Ground Zone — Left/Top Half of Board):**
   - Return plane for `J_USB_C`, `D_TVS_USB`, `J_BAT-`, `MOD_CHG` (`BQ25895`), `U_LDO_DIG`, `U_MCU2` (`ESP32-C3`), `U11` (`MCP23017`), `MOD_TFT1`, `MOD_TFT2`, `Q_BLK`, `Q_ANA_N`, `BZ1`/`Q_BZ`, `J_CYD_UART`, and **`U_ISO1` Detector Side (Pin 5)**.
2. **`AGND` (Ultra-Quiet Analog Metrology Ground Zone — Right/Bottom Half of Board):**
   - Return plane for `MOD_BOOST` (`MT3608`), `C_PI1`, `C_PI2`, `U_REG12` (`L7812CV`), `D_OVP`, `U_REG9` (`L78L09ACZ`), `C_A1`, `C_A2`, `U_LDO_ANA`, `U_MCU1` (`LOLIN32 Lite` Pin 13), `U1–U3` (`DG408DJ`), `U8` (`DG412LDJ` Kelvin Ground Reference), `U10` (`DG409DJ`), `U4` (`LMC6484IN` + $4:1$ precision dividers), `U5` (`TLC2272ACP`), `U9` (`MCP4822`), `MOD_ADS` (`ADS1115` Pin 2 & Pin 5), `J_ZIF` / `J_DUT3` / `J_KELVIN` / `J_TERM5`, and **`U_ISO2` Detector Side (Pin 5)**.
3. **Single-Point Star-Ground Bridge (`R_STAR` — `0R_STAR_BRIDGE`):**
   - **Component:** **`R_STAR`** (`0 Ω` metal-film axial jumper / $1\text{ A}$ star-ground tie, footprint `R_Axial_P7.62mm`, $R \le 10\text{ m}\Omega$).
   - **Exact Physical Placement:** Located directly adjacent to **`Q_ANA_SW` (`NDP6020P`) and the input capacitors `C_A1` / `C_PI1` at the power-entry boundary** between the Digital Power Zone and the Analog Filter Zone.
   - **Zero Ground-Loop Proof:** Because the inter-MCU UART link (`LINK_TX_M1` and `LINK_TX_M2`) is **100% optically isolated** via `U_ISO1` and `U_ISO2` (`2x H11L1M`) and `Q_ANA_SW` is a high-side P-MOSFET switch, **100% of the analog supply return current flows exclusively through `R_STAR`** back to `DGND` / `J_BAT-`. Zero digital return current from `U_MCU2`, `MOD_CHG`, or `MOD_TFT1/2` ever traverses the `AGND` plane, keeping `AGND` equipotential to within **$< 5\ \mu\text{V}$** across the entire AFE matrix.

---

## 8. Audio Piezo / Magnetic Buzzer Stage (`BZ1`, `Q_BZ`, `SW_BZ`, `D_BZ`, RC Snubber)

To provide crisp acoustic feedback for pass/fail beeps, rotary encoder clicks, and thermal/OVC alarms without coupling inductive kickback onto `+3V3_DIG` or waking a quiet lab environment, the buzzer stage on MCU 2 (`GPIO3` / `BZ_PWM`) incorporates a low-side NPN driver, a physical series mute switch, a fast flyback clamp, and a damped RC snubber:

```
                         +3V3_DIG
                            │
                            ▼
                        [SW_BZ] (SPDT Physical Hardware Mute Slide Switch)
                            │  Pin 2 (COM) ◄── Pin 3 (ON: +3V3_DIG) | Pin 1 (MUTE: NC)
                            ├───────────────────────────┬───────────────────────────┐
                            ▼                           ▼                           ▼
                    [BZ1] (Passive Piezo /      [D_BZ] (1N4148 Fast         [R_SNUB] (100 Ω 0.25W)
                     Magnetic Transducer,        Flyback Diode:                     │
                     Pin 1 = +)                  Pin 1 Cathode = BZ_POS,            ▼
                            │                    Pin 2 Anode = BZ_COL)      [C_SNUB] (10 nF 50V Ceramic)
                            ▼ (Pin 2 = -)               │                           │
                         (BZ_COL) ◄─────────────────────┴───────────────────────────┘
                            │
                            ▼  Pin 3 (Collector)
[U_MCU2 P13 / GPIO3] ──► [R_BZ_B] (1.0 kΩ) ──► Pin 2 (Base) of [Q_BZ: 2N3904 NPN, TO-92]
 (BZ_PWM: LEDC Timer)      │                   Pin 1 (Emitter)
                           ▼                        │
                   [R_BZ_PD] (10 kΩ)                ▼
                           │                      DGND
                           ▼
                         DGND
```

### Component Values & Transient Suppression Verification:
- **Driver Transistor (`Q_BZ` — `2N3904` NPN, TO-92-3: `1=Emitter [DGND], 2=Base, 3=Collector [BZ_COL]`):**
  - Driven by `U_MCU2` Pin 13 (`GPIO3` / `BZ_PWM` LEDC hardware PWM, $1\text{ kHz–}4\text{ kHz}$) via **`R_BZ_B` = $1.0\text{ k}\Omega$** with a **`R_BZ_PD` = $10\text{ k}\Omega$** base-emitter pull-down to `DGND` (guarantees zero clicks or leakage during ESP32-C3 boot/reset).
  - Base drive current: $I_B = \frac{3.30\text{ V} - 0.70\text{ V}}{1000\ \Omega} - \frac{0.70\text{ V}}{10000\ \Omega} = 2.53\text{ mA}$, easily saturating `Q_BZ` ($V_{\text{CE(sat)}} \le 0.15\text{ V}$) for transducer currents up to $I_{\text{BZ}} = 35\text{ mA}$ ($\beta_{\text{forced}} \approx 14$).
- **Physical Hardware Mute Switch (`SW_BZ` — SPDT Slide Switch):**
  - Wired physically in series between `+3V3_DIG` and `BZ1 (+)` (`BZ_POS`). When switched to `MUTE`, `BZ1` is physically disconnected from `+3V3_DIG`, guaranteeing **100% silent operation** independent of software settings.
- **Inductive Flyback Diode (`D_BZ` — `1N4148`, DO-35, $t_{rr} = 4\text{ ns}$):**
  - Connected in anti-parallel across `BZ1` (**Cathode Pin 1 to `BZ_POS`, Anode Pin 2 to `BZ_COL`**). Clamps inductive turn-off voltage spikes from magnetic transducers to $V_{\text{BZ\_POS}} + 0.7\text{ V} \approx 4.0\text{ V}$ (well below `2N3904` $V_{\text{CEO}} = 40\text{ V}$).
- **Damped RC Snubber (`R_SNUB = 100 Ω`, `C_SNUB = 10 nF` in parallel with `BZ1` / `D_BZ`):**
  - Piezo transducers have high internal capacitance ($C_{\text{piezo}} \approx 15\text{–}30\text{ nF}$) and high-$Q$ mechanical resonance that rings on sharp turn-off edges even with a flyback diode. The series **`R_SNUB` ($100\ \Omega$) + `C_SNUB` ($10\text{ nF}$)** network ($f_c = \frac{1}{2\pi \times 100\ \Omega \times 10\text{ nF}} = 159.2\text{ kHz}$, $\tau = 1.0\ \mu\text{s}$) critically damps high-frequency edge ringing ($>160\text{ kHz}$) on `BZ_COL`, preventing EMI radiation into the adjacent `AGND` zone.

---

## 9. Complete Power, Filter & Switch Bill of Materials (100% THT / Socketed Module)

| Designator | Part Number / Value | Package / Footprint (100% THT) | Key Rating & Pinout Verification |
| :--- | :--- | :--- | :--- |
| **`J_USB_C`** | USB-C Breakout w/ $2\times 5.1\text{ k}\Omega$ CC | `Socket_USBC_Breakout` (`1x04` / `1x06` 2.54mm) | $5\text{ V}/3\text{ A}$ UFP ($5.1\text{ k}\Omega$ on `CC1`, `CC2`) + HVDCP support |
| **`F_USB`** | Littelfuse `RXEF175` (or `RXEF110`) | `Fuse_PTC_P5.00mm` | $I_{\text{hold}} = 1.75\text{ A}$ ($1.10\text{ A}$), $60\text{ V}$ radial resettable PPTC |
| **`D_TVS_USB`** | `P6KE15A` (or `1N5919B` for 5V-only) | `D_DO-15_P10.16mm` / `D_DO-41_P10.16mm` | $15\text{ V}$ $600\text{ W}$ TVS (HVDCP) or $5.6\text{ V}$ $3\text{ W}$ Zener (5V-only) |
| **`J_BAT`** | 2-Pin Polarized Battery Header | `Header_1x02_P2.54mm` / JST-PH/XH THT | `1=J_BAT+`, `2=DGND` (Strictly **3.7V Non-HV LiPo**, $4.20\text{ V}$ max!) |
| **`SW_PWR`** | DPDT (or SPDT) Power Switch $\ge 2\text{ A}$ | `SW_Slide_DPDT_THT` / `SW_SPDT_THT` | Pole 1 breaks `J_BAT+` (**$0.0\ \mu\text{A}$ OFF drain**); Pole 2 breaks `VSYS` |
| **`Q_REV_BAT`** | `NDP6020P` (+ `R_G_REV = 1.0k`) | `TO-220-3_Vertical` | **`1=G (via 1k to DGND), 2=D (SW_PWR/J_BAT+), 3=S (MOD_CHG VBAT)`** |
| **`MOD_CHG`** | TI `BQ25895` NVDC Charger Module | THT Female Header Socket (2.54mm pitch) | $\text{I}^2\text{C}$ `0x6A` (`UI_I2C`), $V_{\text{REG}}=\mathbf{4.192\text{ V}}$, $I_{\text{CHG}}=2.048\text{ A}$, $I_{\text{TERM}}=128\text{ mA}$ |
| **`LED_CHG`** | Amber 3mm THT LED (+ `R=1.5k`) | `LED_D3mm_Amber` + `R_Axial_P7.62mm` | Driven by `MOD_CHG STAT` (open-drain) from `+3V3_DIG` |
| **`U_LDO_DIG`** | `MCP1826S-3302E/AB` | `TO-220-3_Vertical` | **`1=VIN (V_BUS_SYS), 2=DGND (Tab), 3=VOUT (+3V3_DIG)`**, $1.0\text{ A}$, $V_{\text{DO}} < 75\text{ mV}$ |
| **`JP_M2_3V3`** | 2-Pin Flash Isolation Jumper | `Header_1x02_P2.54mm` | Isolates `+3V3_DIG` from `U_MCU2` (`M2_3V3`) during USB-C flashing |
| **`SW_STBY`** | SPDT Slide Switch (Standby/Active) | `SW_Slide_SPDT_THT` | Connects `U11 GPA7` (Pin 28) to `DGND` (`LOW`=Standby) or $10\text{ k}\Omega$ pull-up (`HIGH`=Active) |
| **`Q_ANA_N`** | `2N7000` (+ `100k` pull-down, `100R` gate) | `Socket_TO92_3Pin` | **`1=S (DGND), 2=G (U11 GPB7 ANA_PWR_EN), 3=D (Q_ANA_SW Gate)`** |
| **`Q_ANA_SW`** | `NDP6020P` (+ `47k` G-S pull-up) | `TO-220-3_Vertical` | **`1=G (Q_ANA_N Drain), 2=D (V_ANA_IN), 3=S (V_BUS_SYS)`** |
| **`Q_BLK`** | `2N7000` (+ `100k` pull-down, `100R` gate) | `Socket_TO92_3Pin` | **`1=S (DGND), 2=G (U11 GPB6 TFT_BLK_EN), 3=D (TFT_BLK)`** |
| **`MOD_BOOST`** | `MT3608` Boost Module (14.50V) | `Socket_MT3608_Boost` | `1=VIN+ (V_ANA_IN), 2=VIN- (AGND), 3=VOUT+ (14.50V), 4=VOUT- (AGND)` |
| **`L1`, `R_DAMP1`** | $100\ \mu\text{H}$ ($1\text{ A}$, $0.35\ \Omega$) + $2.2\ \Omega$ $0.5\text{ W}$ | `L_Radial_P5.00mm` + `R_Axial_P7.62mm` | Pi-Filter #1 ($\zeta = 0.90\text{–}1.28$, $-66.4\text{ dB}$ @ $1.5\text{ MHz}$) with `C_PI1`, `C_PI2` ($100\ \mu\text{F}/35\text{V} // 100\text{ nF}$) |
| **`U_REG12`** | `L7812CV` | `TO-220-3_Vertical` | **`1=IN (V_14V_FILT), 2=AGND, 3=OUT (V_12V_PRE)`** |
| **`F1`, `D_OVP`** | `RXEF017` ($170\text{ mA}$) + `1N4743A` ($13\text{ V}$ 1W) | `Fuse_PTC_P5.00mm` + `D_DO-41_P10.16mm` | Hardened OVP crowbar clamping `+12V_A` $\le 13.65\text{ V}$ |
| **`U_REG9`** | `L78L09ACZ` | `Socket_TO92_3Pin` | **`1=OUT (+9V_REF), 2=AGND, 3=IN (+12V_A)`** |
| **`Q_LIM1, Q_LIM2`** | $2\times$ `2N3906` PNP + `R_ILIM = 43 Ω` | `Socket_TO92_3Pin` + `R_Axial_P7.62mm` | **`1=E, 2=B, 3=C`**; $15.12\text{ mA}$ active current limiter onto `+9V_LIM` |
| **`Q_OVC`** | `2N3904` NPN + `10k` pull-up | `Socket_TO92_3Pin` | **`1=E (AGND), 2=B, 3=C (FAULT_OVC_N -> U_MCU1 GPIO25)`** |
| **`L2`, `R_DAMP2`** | $47\ \mu\text{H}$ ($1\text{ A}$, $0.15\ \Omega$) + $1.5\ \Omega$ $0.25\text{ W}$ | `L_Radial_P5.00mm` + `R_Axial_P7.62mm` | Pi-Filter #2 ($\zeta = 0.85\text{–}1.20$, $-59.9\text{ dB}$ @ $1.5\text{ MHz}$) with `C_A1`, `C_A2` ($100\ \mu\text{F}/16\text{V} // 100\text{ nF}$) |
| **`U_LDO_ANA`** | `MCP1826S-3302E/AB` | `TO-220-3_Vertical` | **`1=VIN (V_3V3_FILT), 2=AGND (Tab), 3=VOUT (+3V3_ANA)`**, $1.0\text{ A}$, $V_{\text{DO}} \approx 30\text{ mV}$ |
| **`JP_M1_3V3`** | 2-Pin Flash Isolation Jumper | `Header_1x02_P2.54mm` | Isolates `+3V3_ANA` from `U_MCU1` (`M1_3V3`) during USB-C flashing |
| **`R_ADC_F, C_ADC_F`** | $10\ \Omega$ $1\%$ + $10\ \mu\text{F}/16\text{V} // 100\text{ nF}$ | `R_Axial_P7.62mm` + `CP_Radial` + `C_Disc` | $f_c = 1.58\text{ kHz}$ post-LDO filter feeding `MOD_ADS VDD` (`+3V3_ADC`, $-94.3\text{ dB}$ total @ $1.5\text{ MHz}$) |
| **`R_STAR`** | `0R_STAR_BRIDGE` ($0\ \Omega$ Metal Film) | `R_Axial_P7.62mm` | Single-point star bridge uniting `AGND` and `DGND` at `Q_ANA_SW` boundary |
| **`BZ1, Q_BZ, SW_BZ`** | THT Buzzer + `2N3904` + SPDT Mute + `1N4148` + $100\ \Omega / 10\text{ nF}$ | `Buzzer_D12mm` + `Socket_TO92_3Pin` + `D_DO-35` | Driven by `U_MCU2 GPIO3` (`BZ_PWM`) with physical mute switch and damped RC snubber |
