# Phase 1 — Optical UART Isolation, CYD Expansion Tap & Deterministic Binary Protocol Specification (Rev 5.0)

**Document ID:** `p1_uart_isolation_spec.md`  
**Subsystem:** Inter-MCU Galvanic Optical Isolation (`2x H11L1M`), Contention-Proof `J_CYD_UART` Expansion Header, and Deterministic 115,200-Baud Binary Packet Protocol  
**Architecture Revision:** Rev 5.0 Dual-MCU Germanium Parametric Characterization Engine

---

## 1. Hardware Optical Isolation Architecture (`U_ISO1` & `U_ISO2` — `2x H11L1M` DIP-6)

### 1.1 Engineering Trade-Off Analysis: Direct Copper UART vs. Dual `H11L1M` Optical Isolation

In a dual-MCU precision semiconductor analyzer where **MCU 1 (`U_MCU1` — WEMOS LOLIN32 Lite, Quiet Analog Core on `+3V3_ANA` / `AGND`)** performs sub-nanoampere/microampere Germanium leakage ($I_{\text{CEO}}, I_{\text{CBO}}$) and $16\text{-bit}$ `ADS1115` metrology while **MCU 2 (`U_MCU2` — ESP32-C3 SuperMini, UI / Wi-Fi / BLE / Audio Core on `+3V3_DIG` / `DGND`)** drives $320\text{ mA}$ RF bursts, dual $40\text{ MHz}$ SPI IPS displays, and can completely power down MCU 1 in Standby Mode (`SW_STBY = LOW`), a direct copper UART connection introduces three critical failure modes that **`2x H11L1M` Schmitt-Trigger Optocouplers** completely eliminate:

| Engineering Parameter | Direct Copper UART (`TX` $\rightarrow$ `RX` via $1\text{ k}\Omega$) | Dual `H11L1M` Schmitt-Trigger Optical Isolation (`U_ISO1`, `U_ISO2`) | Quantitative Metrology & Reliability Verdict |
| :--- | :--- | :--- | :--- |
| **1. Standby Mode ESD Diode Back-Powering (`MCU 1 OFF`, `+3V3_ANA = 0.0 V`)** | UART idle is Logic HIGH ($+3.30\text{ V}$). With `U_MCU2` alive ($+3.30\text{ V}$) and `U_MCU1` powered OFF (`+3V3_ANA = 0.0V`), `LINK_TX_M2` forward-biases the upper ESD clamp diode of `U_MCU1 GPIO13` ($V_F \approx 0.55\text{ V}$), injecting $I_{\text{leak}} = \frac{3.30\text{ V} - 0.55\text{ V}}{R_{\text{series}}}$ into `+3V3_ANA`! Even with $1\text{ k}\Omega$, **$2.75\text{ mA}$ continuously back-powers `+3V3_ANA`**, floating the rail to $\sim 1.1\text{ V}$, corrupting `74HC595` POR reset (`C_POR`), and violating the $0.0\text{ mA}$ Standby guarantee. |- **`U_ISO1` (M1 $\rightarrow$ M2):** Detector powered by `+3V3_DIG`; LED powered by `+3V3_ANA`. When MCU 1 is OFF, LED current is $0.00\text{ mA}$, and Detector `VO` idles cleanly at $+3.30\text{ V}$ via `R_PU_ISO1` ($2.2\text{ k}\Omega$ to `+3V3_DIG`).<br>- **`U_ISO2` (M2 $\rightarrow$ M1):** LED powered by `+3V3_DIG` and driven active-LOW by `LINK_TX_M2` (idles HIGH $\rightarrow I_F = 0.00\text{ mA}$). Detector (`Pin 6`) and pull-up `R_PU_ISO2` ($2.2\text{ k}\Omega$) are powered strictly from **`+3V3_ANA`**. When `+3V3_ANA = 0.0V`, `U_ISO2` Detector has **zero power source**, so back-feeding current into `U_MCU1` is strictly **$0.000\ \mu\text{A}$**! | **`2x H11L1M` MANDATORY:** Achieves true **$0.000\ \mu\text{A}$** back-powering in Standby Mode without requiring firmware GPIO pin-mux reconfiguration on MCU 2. |
| **2. Digital Return Current & Ground-Bounce Loop (`DGND` $\leftrightarrow$ `AGND`)** | High-speed digital edges ($t_r \approx 2\text{ ns}$ on ESP32-C3 CMOS outputs) return high-frequency displacement current ($I = C_{\text{load}} \frac{dV}{dt}$) directly across the `AGND`–`DGND` boundary, forming a parallel ground loop with `R_STAR` and injecting common-mode RF hash into `MOD_ADS`. | Optical air-gap isolation ($V_{\text{ISO}} = 5000\text{ V}_{\text{RMS}}$, coupling capacitance $C_{\text{IO}} \approx 0.5\text{ pF}$). LED drive currents return locally within their own supply domain (`U_ISO1` LED returns to `U_MCU1 AGND`; `U_ISO2` LED returns to `U_MCU2 DGND`). | **`2x H11L1M` MANDATORY:** Zero digital displacement current crosses between `DGND` and `AGND`, preserving single-point star grounding at `R_STAR`. |
| **3. Noise Immunity & Edge Restoration** | Standard phototransistor optocouplers (`PC817` / `4N35`) suffer from slow $18\ \mu\text{s}$ storage tails and soft analog ramps that fails at $115,200\text{ bps}$ ($8.68\ \mu\text{s/bit}$). | Integrated high-speed photodetector + **internal Schmitt-trigger hysteresis stage** with open-collector NPN output ($f_{\text{max}} = 1.0\text{ MHz}$ NRZ, $t_r = t_f = 100\text{ ns}$). | **`H11L1M` SELECTED:** Eliminates bit-width distortion and jitter at $115,200\text{ bps}$ while remaining 100% THT (`DIP-6`). |

---

### 1.2 Exact Circuit Topology, DIP-6 Pinout & Non-Inverting Zero-Idle-Current Drive

```
====================================================================================================
CHANNEL 1: U_ISO1 (H11L1M DIP-6) — MCU 1 (Quiet Analog Domain) ──► MCU 2 (Digital/UI Domain) + CYD
====================================================================================================
   [ANALOG DOMAIN: +3V3_ANA / AGND]               ║         [DIGITAL DOMAIN: +3V3_DIG / DGND]
                                                  ║
        +3V3_ANA                                  ║              +3V3_DIG
           │                                      ║                 │
        [R_LED_ISO1] (470 Ω 1%)                   ║                 ├───────────┬──────────────────┐
           │                                      ║                 │           │                  │
           ▼                                      ║          [C_BYP_ISO1]  [R_PU_ISO1]             │
      ┌─────────┐                                 ║           (100 nF)      (2.2 kΩ)               │
      │ Pin 1(A)│                                 ║                 │           │                  │
      │   LED   │  hν (Optical Barrier, 0.5 pF)   ║               DGND          │    ┌─────────┐   │
      │ Pin 2(K)│ ═══════════════════════════════►║◄════════════════════════════┼────┤ Pin 4(VO)   │
      └────┬────┘                                 ║                             │    │ Schmitt │   │
           │       Pin 3 = NC (Unconnected)       ║                             │    │Pin 6(VCC)◄──┘
           ▼                                      ║                             │    │Pin 5(GND)───► DGND
   U_MCU1 Pin 21 (GPIO16: LINK_TX_M1, UART2 TX)   ║                             │    └─────────┘
                                                  ║                             │
                                                  ║                             ├──► U_MCU2 Pin 7 (GPIO20: LINK_RX_M2)
                                                  ║                             └──► [R_CYD_RX: 470 Ω] ──► J_CYD_UART Pin 4 (CYD_RX_TAP)

====================================================================================================
CHANNEL 2: U_ISO2 (H11L1M DIP-6) — MCU 2 (Digital/UI Domain) ──► MCU 1 (Quiet Analog Domain)
====================================================================================================
   [DIGITAL DOMAIN: +3V3_DIG / DGND]              ║         [ANALOG DOMAIN: +3V3_ANA / AGND]
                                                  ║
        +3V3_DIG                                  ║              +3V3_ANA (0.0V in Standby Mode!)
           │                                      ║                 │
        [R_LED_ISO2] (470 Ω 1%)                   ║                 ├───────────┬──────────────────┐
           │                                      ║                 │           │                  │
           ▼                                      ║          [C_BYP_ISO2]  [R_PU_ISO2]             │
      ┌─────────┐                                 ║           (100 nF)      (2.2 kΩ)               │
      │ Pin 1(A)│                                 ║                 │           │                  │
      │   LED   │  hν (Optical Barrier, 0.5 pF)   ║               AGND          │    ┌─────────┐   │
      │ Pin 2(K)│ ═══════════════════════════════►║◄════════════════════════════┼────┤ Pin 4(VO)   │
      └────┬────┘                                 ║                             │    │ Schmitt │   │
           │       Pin 3 = NC (Unconnected)       ║                             │    │Pin 6(VCC)◄──┘
           ├──────────────────────────────────────╫──► [R_CYD_TX: 470 Ω]        │    │Pin 5(GND)───► AGND
           │       (To J_CYD_UART Pin 3:          ║     (CYD_TX_TAP)            │    └─────────┘
           ▼        CYD_TX_TAP)                   ║                             │
   U_MCU2 Pin 8 (GPIO21: LINK_TX_M2, UART0 TX)    ║                             └──► U_MCU1 Pin 26 (GPIO13: LINK_RX_M1)
```

#### Exact `H11L1M` (DIP-6, `Socket_DIP-6_W7.62mm`) Pinout & Polarity Audit:

| DIP-6 Pin | Symbol Name | Internal Function | `U_ISO1` (M1 $\rightarrow$ M2) Net Assignment | `U_ISO2` (M2 $\rightarrow$ M1) Net Assignment |
| :---: | :---: | :--- | :--- | :--- |
| **Pin 1** | **`A`** | GaAs IR LED Anode (`+`) | `ISO1_A` (via `R_LED_ISO1 = 470 Ω` to **`+3V3_ANA`**) | `ISO2_A` (via `R_LED_ISO2 = 470 Ω` to **`+3V3_DIG`**) |
| **Pin 2** | **`K`** | GaAs IR LED Cathode (`-`) | **`LINK_TX_M1`** (`U_MCU1` Pin 21 / `GPIO16`) | **`LINK_TX_M2`** (`U_MCU2` Pin 8 / `GPIO21`) |
| **Pin 3** | **`NC`** | No Internal Connection | `NC` (Unconnected pad) | `NC` (Unconnected pad) |
| **Pin 4** | **`VO`** | Open-Collector Schmitt Output (Active-LOW when LED ON) | **`LINK_RX_M2`** (`U_MCU2` Pin 7 / `GPIO20` + `R_PU_ISO1 = 2.2 kΩ` to **`+3V3_DIG`**) | **`LINK_RX_M1`** (`U_MCU1` Pin 26 / `GPIO13` + `R_PU_ISO2 = 2.2 kΩ` to **`+3V3_ANA`**) |
| **Pin 5** | **`GND`** | Detector Ground | **`DGND`** | **`AGND`** |
| **Pin 6** | **`VCC`** | Detector Supply ($3.0\text{ V–}15.0\text{ V}$) | **`+3V3_DIG`** (`C_BYP_ISO1 = 100 nF` to `DGND`) | **`+3V3_ANA`** (`C_BYP_ISO2 = 100 nF` to `AGND`) |

#### Non-Inverting Logic Truth Table & Zero-Idle-Current Proof:
The `H11L1M` photodetector has an **inverting open-collector output (`VO` pulls LOW when the IR LED is illuminated)**. By wiring the IR LED in **cathode-drive (active-LOW sink) configuration** between `+3V3` (via $470\ \Omega$ to Pin 1 `A`) and the MCU `UART_TX` pin (Pin 2 `K`), the two inversions cancel out to yield **100% non-inverting standard 8N1 UART polarity with $0.00\text{ mA}$ LED current during UART idle**:
1. **UART Idle / Stop Bit / Mark (`UART_TX = HIGH = +3.30 V`):**
   - Voltage across LED + resistor: $V_{\text{A}} - V_{\text{K}} = 3.30\text{ V} - 3.30\text{ V} = 0.00\text{ V} < V_F$.
   - **LED Idle Current:** $I_{F(\text{idle})} = \mathbf{0.000\text{ mA}}$ (**Zero static LED power consumption!**).
   - **Detector Output (`Pin 4 VO`):** Internal NPN output transistor is **OFF** (high-impedance); the external $2.2\text{ k}\Omega$ pull-up resistor (`R_PU_ISO1` / `R_PU_ISO2`) holds `UART_RX` at **`HIGH (+3.30 V)`**.
2. **UART Start Bit / Space / Logic `0` (`UART_TX = LOW = V_OL_MCU ≈ 0.05 V`):**
   - The MCU `TX` pin sinks current from `+3V3` through the $470\ \Omega$ resistor and the `H11L1M` IR LED.
   - **Detector Output (`Pin 4 VO`):** Because $I_F > I_{F(\text{on})}$, the `H11L1M` Schmitt trigger fires and saturates its open-collector output transistor, pulling `UART_RX` to **`LOW (V_OL ≈ 0.18 V ≤ 0.40 V max)`**.

---

### 1.3 Quantitative LED Drive Margin, Timing Budget & 115,200 Baud Signal Integrity

1. **Forward LED Current ($I_F$) vs. Turn-On Threshold ($I_{F(\text{on})}$):**
   - From the onsemi `H11L1M` datasheet, the GaAs IR emitter forward voltage at $I_F \approx 4\text{–}5\text{ mA}$ is $V_F = 1.20\text{ V}$ typ ($1.50\text{ V}$ max across $-40^\circ\text{C}$ to $+85^\circ\text{C}$), and the ESP32/ESP32-C3 GPIO low-level output voltage at $4.4\text{ mA}$ sink is $V_{\text{OL(MCU)}} \approx 0.05\text{ V}$:
     $$I_{F(\text{typ})} = \frac{V_{\text{CC}} - V_{F(\text{typ})} - V_{\text{OL(MCU)}}}{R_{\text{LED}}} = \frac{3.30\text{ V} - 1.20\text{ V} - 0.05\text{ V}}{470\ \Omega} = \frac{2.05\text{ V}}{470\ \Omega} = \mathbf{4.362\text{ mA}}$$
     $$I_{F(\text{min, worst-case})} = \frac{3.25\text{ V} - 1.50\text{ V} - 0.10\text{ V}}{474.7\ \Omega} = \mathbf{3.476\text{ mA}}$$
   - **Trigger Overdrive Margin:** The `H11L1M` guaranteed maximum turn-on threshold current is **$I_{F(\text{on,max})} = 1.60\text{ mA}$** (typical $0.8\text{ mA}$). Operating at $I_F = 4.36\text{ mA}$ provides an overdrive factor of:
     $$\text{Overdrive Ratio} = \frac{I_{F(\text{typ})}}{I_{F(\text{on,max})}} = \frac{4.362\text{ mA}}{1.60\text{ mA}} = \mathbf{2.73\times} \quad (\text{or } 2.17\times \text{ worst-case cold/aged CTR})$$
     This guarantees snappy Schmitt-trigger switching over a $>20\text{-year}$ LED CTR aging lifecycle while keeping average LED current during a $50\%$ duty packet transmission at only $2.18\text{ mA}$ ($0.00\text{ mA}$ idle).
2. **Pull-Up Resistor (`R_PU_ISO = 2.2 kΩ`) & Rise/Fall Time Budget at $115,200\text{ bps}$:**
   - **Collector Sink Current (`VO = LOW`):** When `Pin 4 (VO)` pulls LOW ($V_{\text{OL}} \approx 0.18\text{ V}$), the collector sinks:
     $$I_{\text{C(sink)}} = \frac{3.30\text{ V} - 0.18\text{ V}}{2200\ \Omega} = \mathbf{1.418\text{ mA}} \quad (\ll 16.0\text{ mA max rated } I_{\text{OL}})$$
   - **Output Fall & Rise Times ($t_f, t_r$):**
     - Intrinsic `H11L1M` active pull-down fall time: **$t_f = 100\text{ ns}$ ($0.10\ \mu\text{s}$)**.
     - Passive pull-up rise time with $R_{\text{PU}} = 2.2\text{ k}\Omega$ and total node capacitance $C_{\text{node}} = C_{\text{VO}} (5\text{ pF}) + C_{\text{trace}} (4\text{ pF}) + C_{\text{GPIO}} (5\text{ pF}) = 14\text{ pF}$:
       $$\tau_{\text{RC}} = R_{\text{PU}} \times C_{\text{node}} = 2200\ \Omega \times 14\text{ pF} = 30.8\text{ ns} \implies t_{r(10\%\text{–}90\%)} = \sqrt{t_{r(\text{int})}^2 + (2.2\,\tau_{\text{RC}})^2} = \sqrt{(100\text{ ns})^2 + (67.8\text{ ns})^2} = \mathbf{120.8\text{ ns}} \ (0.121\ \mu\text{s})$$
   - **Propagation Delay Symmetry & Pulse-Width Distortion at $115,200\text{ bps}$ ($T_{\text{bit}} = 8.6806\ \mu\text{s}$):**
     - From the `H11L1M` datasheet ($f_{\text{max}} = 1.0\text{ MHz}$ NRZ), turn-on delay is $t_{\text{on}} = 1.0\ \mu\text{s}$ typ and turn-off delay is $t_{\text{off}} = 1.2\ \mu\text{s}$ typ.
     - Net pulse-width distortion ($\text{PWD} = |t_{\text{off}} - t_{\text{on}}|$):
       $$\text{PWD} = |1.20\ \mu\text{s} - 1.00\ \mu\text{s}| = \mathbf{0.20\ \mu\text{s}}$$
     - As a percentage of the $115,200\text{ bps}$ bit period ($T_{\text{bit}} = \frac{1}{115200} = 8.6806\ \mu\text{s}$):
       $$\text{Bit Timing Skew} = \frac{0.20\ \mu\text{s}}{8.6806\ \mu\text{s}} \times 100\% = \mathbf{2.30\%} \quad (\text{vs. } \pm 45\% \text{ UART mid-bit sampling window!})$$
     - Total bandwidth utilization: $115.2\text{ kbps}$ NRZ corresponds to a $57.6\text{ kHz}$ fundamental square wave, which is **$17.36\times$ below the $1.0\text{ MHz}$ rated cutoff** of the `H11L1M`.

---

## 2. Safe Integration of Optional `J_CYD_UART` Expansion Header (Zero Bus Contention Proof)

To support an optional external **ESP32-2432S028R ("Cheap Yellow Display" / CYD)** 2.8-inch touchscreen companion plug-in via its 4-pin `P1` UART connector (in addition to wireless Wi-Fi JSON polling), the main board provides the 4-pin THT header **`J_CYD_UART` (`Header_1x04_P2.54mm`)** inside the Digital/UI zone:

```
[J_CYD_UART] (4-Pin 2.54mm THT Header, Matching CYD P1 Connector Order)
  ├── Pin 1: V_BUS_SYS  (3.40V – 4.35V System Power Rail; feeds CYD 5V/VIN input)
  ├── Pin 2: DGND       (Digital Ground Reference)
  ├── Pin 3: CYD_TX_TAP ───► [R_CYD_TX = 470 Ω] ───► LINK_TX_M2 (U_MCU2 Pin 8 / GPIO21 & U_ISO2 Pin 2 Cathode)
  └── Pin 4: CYD_RX_TAP ───► [R_CYD_RX = 470 Ω] ───► LINK_RX_M2 (U_MCU2 Pin 7 / GPIO20 & U_ISO1 Pin 4 VO)
```

### Mathematical Proof of Zero Contention Against `U_ISO1`, `U_ISO2`, and `U_MCU2`:
1. **Scenario A — CYD Connected as Passive Hardware Sniffer / Monitor (`CYD_RX` listening on Pin 3 or Pin 4):**
   - A high-impedance CMOS UART input on the external CYD draws $< 1\ \mu\text{A}$ through `R_CYD_TX` ($470\ \Omega$) or `R_CYD_RX` ($470\ \Omega$), causing $< 0.5\text{ mV}$ loading on `LINK_TX_M2` and `LINK_RX_M2`.
   - Note from `@scout` (#3): At power-up, ESP32-C3 outputs its ROM bootloader log on `GPIO21` (`LINK_TX_M2`) at $115,200\text{ baud}$. `U_ISO2` cleanly passes (or ignores when MCU 1 is in Standby) those edges, and MCU 1's binary parser discards any non-`[0xAA][0x55]` ASCII boot bytes while `J_CYD_UART Pin 3` provides a live hardware diagnostic tap of MCU 2's boot and command stream!
2. **Scenario B — CYD Actively Driving `J_CYD_UART Pin 4` (`CYD_RX_TAP` = $+3.30\text{ V}$ HIGH) While `U_ISO1` (`Pin 4 VO`) Pulls `LINK_RX_M2` LOW ($V_{\text{OL}} \approx 0.18\text{ V}$):**
   - Suppose an external CYD cable is miswired or configured to drive `CYD_RX_TAP` HIGH ($+3.30\text{ V}$) at the exact instant `U_ISO1` (`H11L1M` Pin 4 open-collector output) asserts a Start/Low bit on `LINK_RX_M2`:
   - Current from the CYD driver is limited by **`R_CYD_RX` ($470\ \Omega$)** to:
     $$I_{\text{CYD\_clash}} = \frac{3.30\text{ V} - V_{\text{OL(ISO1)}}}{R_{\text{CYD\_RX}}} = \frac{3.30\text{ V} - 0.20\text{ V}}{470\ \Omega} = \mathbf{6.60\text{ mA}}$$
   - Combined with the $1.42\text{ mA}$ from `R_PU_ISO1` ($2.2\text{ k}\Omega$), the total sink current into `U_ISO1 Pin 4 (VO)` is $6.60\text{ mA} + 1.42\text{ mA} = \mathbf{8.02\text{ mA}}$, which is **half of the `H11L1M` maximum rated collector sink current ($I_{\text{OL,max}} = 16.0\text{ mA}$)**!
   - Furthermore, because `U_MCU2 Pin 7` (`GPIO20` / `LINK_RX_M2`) is directly tied to `U_ISO1 Pin 4 (VO)` *upstream* of `R_CYD_RX` ($470\ \Omega$), `U_ISO1` holds `LINK_RX_M2` at $V_{\text{OL}} \le 0.25\text{ V}$ ($< V_{\text{IL,max}} = 0.825\text{ V}$), so **MCU 1 $\rightarrow$ MCU 2 telemetry continues to decode with zero bit errors even if an external CYD actively fights `J_CYD_UART Pin 4`!**
3. **Scenario C — Worse-Case Output-to-Output Clash on `J_CYD_UART Pin 3` (`CYD_TX_TAP`) vs. `U_MCU2 GPIO21` (`LINK_TX_M2`):**
   - Even if an external device shorts `J_CYD_UART Pin 3` directly to `DGND` or `+3V3_DIG` while `U_MCU2 GPIO21` drives the opposite logic state, **`R_CYD_TX` ($470\ \Omega$)** clamps the fault current to:
     $$I_{\text{short,max}} = \frac{3.30\text{ V}}{470\ \Omega} = \mathbf{7.02\text{ mA}} \quad (\ll 40\text{ mA ESP32-C3 GPIO max sink/source rating})$$
   - Thus, both `U_MCU2` and `U_ISO1`/`U_ISO2` are 100% electrically immune to any miswiring or contention on `J_CYD_UART`.

---

## 3. Deterministic Binary Inter-MCU UART Protocol (`115,200` Baud, `8N1`)

### 3.1 Framing Layout & Endianness Specification

All communication between **MCU 2 (Master UI/Comms Controller)** and **MCU 1 (Slave Quiet Analog Metrology Engine)** uses a deterministic, fixed-header binary packet framing format at **`115,200` baud, `8` data bits, `No` parity, `1` stop bit (`8N1`)**. All multi-byte integer and IEEE-754 floating-point fields are encoded in **Little-Endian (`LE`, least-significant byte first)** native to Both Xtensa LX6 (`ESP32`) and RISC-V (`ESP32-C3`).

```
+---------+---------+---------+---------+-------------+-----------------------+-------------------+
| Byte 0  | Byte 1  | Byte 2  | Byte 3  | Bytes 4..5  | Bytes 6 .. (6+LEN-1)  | Bytes (6+LEN)..+1 |
+---------+---------+---------+---------+-------------+-----------------------+-------------------+
|  0xAA   |  0x55   |   SEQ   | OPCODE  |   LEN (LE)  |    PAYLOAD (0..256)   |  CRC16_CCITT (LE) |
| SYNC_H  | SYNC_L  | uint8_t | uint8_t |  uint16_t   |      uint8_t[LEN]     |     uint16_t      |
+---------+---------+---------+---------+-------------+-----------------------+-------------------+
|<-- 2-Byte Sync -->|<--------- Covered by CRC-16-CCITT (4 + LEN Bytes) ----->|<-- 2-Byte CRC --->|
```

| Field Offset | Field Name | Type | Size (Bytes) | Valid Range / Value | Functional Description |
| :---: | :--- | :---: | :---: | :--- | :--- |
| **`0`** | `SYNC_0` | `uint8_t` | `1` | **`0xAA`** (`10101010b`) | Alternating-bit clock-recovery preamble byte 0. |
| **`1`** | `SYNC_1` | `uint8_t` | `1` | **`0x55`** (`01010101b`) | Complementary magic sync byte 1 (rejects ESP32-C3 ASCII boot logs). |
| **`2`** | `SEQ` | `uint8_t` | `1` | `0x00 .. 0xFF` | Monotonically incrementing sequence number (`SEQ` of response matches `SEQ` of command). |
| **`3`** | `OPCODE` | `uint8_t` | `1` | `0x01..0x15` (CMD) / `0x80..0x85` (EVT) | Command (`Bit 7 = 0`, M2 $\rightarrow$ M1) or Event/Response (`Bit 7 = 1`, M1 $\rightarrow$ M2). |
| **`4..5`** | `LEN` | `uint16_t` (LE) | `2` | `0x0000 .. 0x0100` (`0..256`) | Exact payload length in bytes (`LEN > 256` is immediately rejected by parser). |
| **`6..(5+LEN)`** | `PAYLOAD` | `uint8_t[LEN]` | `0..256` | Opcode-specific struct | Packed little-endian payload struct (`__attribute__((packed))`). |
| **`6+LEN..7+LEN`** | `CRC16_CCITT` | `uint16_t` (LE) | `2` | `0x0000 .. 0xFFFF` | CRC-16-CCITT (`poly=0x1021`, `init=0xFFFF`) computed over **`SEQ`, `OPCODE`, `LEN[2]`, and `PAYLOAD[LEN]`**. |

---

### 3.2 CRC-16-CCITT Mathematical Specification & Reference Implementation

- **Polynomial:** $G(x) = x^{16} + x^{12} + x^5 + 1 \implies \mathbf{\text{0x1021}}$
- **Initial Value:** **`0xFFFF`**
- **Input / Output Reflection:** `RefIn = false`, `RefOut = false`, `XorOut = 0x0000` (Standard CRC-16/CCITT-FALSE)
- **Covered Bytes:** Byte 2 (`SEQ`) through Byte `5 + LEN` (`PAYLOAD[LEN-1]`). *(Note: Because `SYNC_0 = 0xAA` and `SYNC_1 = 0x55` are constant literals verified by the state machine before CRC accumulation begins, computing CRC over `SEQ..PAYLOAD` is injective; both C reference code and pre-computed test vectors are provided below).*
- **Wire Byte Order:** **Little-Endian (`LE`)** — `[CRC_LSB = crc & 0xFF]` followed by `[CRC_MSB = (crc >> 8) & 0xFF]`.

```c
// Reference C/C++ Implementation (Identical on MCU 1 ESP32 & MCU 2 ESP32-C3)
uint16_t crc16_ccitt_compute(const uint8_t *buf, uint16_t len) {
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)buf[i] << 8);
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x8000U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc; // Appended on wire as: [crc & 0xFF], [(crc >> 8) & 0xFF]
}
```

#### Python-Verified Wire Packet Test Vectors:
- **`CMD_PING (0x01)`** (`SEQ=0x01`, `LEN=4`, `Payload=05 00 00 00`):  
  `AA 55 01 01 04 00 05 00 00 00 68 74` (`CRC16 = 0x7468` $\rightarrow$ LE bytes `68 74`).
- **`CMD_SCAN (0x10)`** (`SEQ=0x02`, `LEN=6`, `Payload=00 01 DC 05 23 00`):  
  `AA 55 02 10 06 00 00 01 DC 05 23 00 A3 19` (`CRC16 = 0x19A3` $\rightarrow$ LE bytes `A3 19`).
- **`CMD_ABORT (0x11)`** (`SEQ=0x03`, `LEN=1`, `Payload=01`):  
  `AA 55 03 11 01 00 01 DC B5` (`CRC16 = 0xB5DC` $\rightarrow$ LE bytes `DC B5`).
- **`EVT_ACK (0x80)`** (`SEQ=0x01`, `LEN=2`, `Payload=01 00`):  
  `AA 55 01 80 02 00 01 00 39 B7` (`CRC16 = 0xB739` $\rightarrow$ LE bytes `39 B7`).

---

### 3.3 Command Payload Schemas (MCU 2 Master $\rightarrow$ MCU 1 Slave: `0x01` – `0x15`)

#### 1. `CMD_PING` (`OPCODE = 0x01`, `LEN = 4` Bytes)
Health check, link latency verification, and protocol revision handshake after `Q_ANA_SW` powers up MCU 1.

| Byte Offset | Field Name | C Type | Value / Units | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `proto_ver` | `uint8_t` | `0x05` (Rev 5.0) | Binary protocol version major identifier. |
| `1` | `flags` | `uint8_t` | Bit 0: `REQ_STATUS` | `0x01` requests current MCU 1 `DS18B20` temp & rail status in `EVT_ACK`. |
| `2..3` | `uptime_s_lo` | `uint16_t` (LE) | Seconds | Lower 16 bits of MCU 2 uptime for RTT echo verification. |

#### 2. `CMD_SCAN` (`OPCODE = 0x10`, `LEN = 6` Bytes)
Triggers the complete 6-permutation R.G. Keen True-$h_{\text{FE}}$, leakage ($I_{\text{CEO}}, I_{\text{CBO}}$), $V_{\text{BE}}$, $V_{\text{CE(sat)}}$, thermal drift $\Delta\%$, and component auto-identification sequence.

| Byte Offset | Field Name | C Type | Value / Default | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `scan_mode` | `uint8_t` | `0`=Auto, `1`=Ge-BJT Force, `2`=Si-BJT, `3`=JFET/MOS, `4`=Diode/Zener, `5`=R/C | Target DUT characterization algorithm mode. |
| `1` | `avg_samples` | `uint8_t` | `1..16` (Default `4`) | Number of `ADS1115` oversampled reads per matrix permutation step. |
| `2..3` | `thermal_dwell_ms` | `uint16_t` (LE) | `1500` ms (`0x05DC`) | Stabilization window for Germanium thermal drift $\Delta h_{\text{FE}}\%$ measurement. |
| `4` | `max_pdiss_mw` | `uint8_t` | `35` mW (`0x23`) | Software power dissipation abort ceiling for vintage Germanium DUTs. |
| `5` | `scan_flags` | `uint8_t` | Bit 0: `STREAM_RAW_DBG`, Bit 1: `KELVIN_4W_EN` | When Bit 0 = `1`, MCU 1 emits `EVT_RAW_DEBUG (0x82)` at each step. |

#### 3. `CMD_ABORT` (`OPCODE = 0x11`, `LEN = 1` Byte)
Immediate emergency stop command. Upon receipt, MCU 1 immediately asserts `SR_OE_N = HIGH` (`GPIO17`), opens all `DG408` switches, closes `U8 DG412LDJ` (`AGND` discharge clamps), sets both `MCP4822` DAC channels to $0.000\text{ V}$, and returns `EVT_ACK (0x80)`.

| Byte Offset | Field Name | C Type | Value | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `abort_reason` | `uint8_t` | `0x01`=User Cancel, `0x02`=Timeout, `0x03`=Standby Entry | Logged in `EVT_ACK` telemetry. |

#### 4. `CMD_CALIBRATE` (`OPCODE = 0x12`, `LEN = 4` Bytes)
Executes live self-calibration across `U10 DG409DJ` channels (`V_SRC_A`, `V_SRC_B`, `+9V_LIM`, `AGND`) and measures individual `DG408` + `100 Ω R_PR` Kelvin Null resistance offsets.

| Byte Offset | Field Name | C Type | Value | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `cal_target` | `uint8_t` | `0x00`=Full Suite, `0x01`=DAC/Divider Gain, `0x02`=Kelvin R_ON, `0x03`=Open-Socket Leakage | Calibration routine selector. |
| `1` | `save_nvs` | `uint8_t` | `0`=RAM Only, `1`=Commit to MCU 1 NVS | Persist calibration constants to flash upon pass. |
| `2..3` | `ref_9v_mv` | `uint16_t` (LE) | `9000` mV (`0x2328`) | External DMM reference override (or `0x0000` for internal ratio cal). |

#### 5. `CMD_CURVE_SWEEP` (`OPCODE = 0x13`, `LEN = 12` Bytes)
Executes a multi-point $I_C\text{ vs. } V_{\text{CE}}$ (at stepped $I_B$), JFET $I_D\text{ vs. } V_{\text{GS}}$, or Diode/Zener $I_F\text{ vs. } V_F$ curve-tracer sweep, streaming `EVT_CURVE_PT (0x83)` packets back to MCU 2.

| Byte Offset | Field Name | C Type | Value / Units | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `sweep_type` | `uint8_t` | `0`=BJT $I_C(V_{\text{CE}}, I_B)$, `1`=FET $I_D(V_{\text{GS}})$, `2`=Diode/Zener $I(V)$ | Curve tracer family mode. |
| `1` | `pin_cfg` | `uint8_t` | `0..5` (Permutation index of `TP1, TP2, TP3` as `E/B/C` or `G/D/S`) | Pin orientation determined by prior `CMD_SCAN` or manual override. |
| `2` | `polarity` | `uint8_t` | `0`=PNP / P-Ch, `1`=NPN / N-Ch | Sets high-side vs. low-side `V_SRC_A` / `V_SRC_B` drive polarity. |
| `3` | `num_families` | `uint8_t` | `1..4` (Default `4` base/gate steps) | Number of $I_B$ or $V_{\text{GS}}$ family curves. |
| `4` | `pts_per_curve` | `uint8_t` | `8..32` (Default `16`) | Number of $V_{\text{CE}}$ / $V_{\text{DS}}$ steps per curve. |
| `5` | `max_pdiss_mw` | `uint8_t` | `35` mW | Dynamic hyperbola power-limit cutoff ($V_{\text{CE}} \times I_C \le 35\text{ mW}$). |
| `6..7` | `v_start_mv` | `uint16_t` (LE) | `0..9000` mV | Sweep start voltage for `V_SRC_A`. |
| `8..9` | `v_stop_mv` | `uint16_t` (LE) | `100..9000` mV | Sweep stop voltage for `V_SRC_A`. |
| `10..11` | `ib_step_ua` | `uint16_t` (LE) | `5..500` µA | Base current step $\Delta I_B$ (or $\Delta V_{\text{GS}}$ in mV for FET mode). |

#### 6. `CMD_DMM_READ` (`OPCODE = 0x14`, `LEN = 2` Bytes)
Routes the protected $0\text{–}33\text{ V}$ external DMM attenuator (`J_TERM5 Pin 5` $\rightarrow 100\text{ k}\Omega / 10.0\text{ k}\Omega$ $11:1$ divider $\rightarrow$ `U10 DG409DJ S4A`) to `ADS1115 A3` (`ADC_CAL`).

| Byte Offset | Field Name | C Type | Value | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `dmm_ch` | `uint8_t` | `0`=`DMM_30V` ($11:1$), `1`=`V_SRC_A`, `2`=`V_SRC_B`, `3`=`+9V_LIM` | Selects `U10 DG409DJ` address (`CAL_A1:CAL_A0`). |
| `1` | `num_samples` | `uint8_t` | `1..64` (Default `8`) | Number of $16\text{-bit}$ `ADS1115` conversions to average. |

#### 7. `CMD_DEBUG_SET` (`OPCODE = 0x15`, `LEN = 8` Bytes)
Manual hardware-in-the-loop (HIL) diagnostic override allowing the technician to directly set the 16-bit `74HC595` matrix word (`U6:U7`), `U10` calibration mux, and `MCP4822` DAC channels (`V_SRC_A`, `V_SRC_B`), returning an immediate `EVT_RAW_DEBUG (0x82)` snapshot. *(Note: Firmware still enforces the anti-shoot-through validity mask on the 16-bit matrix word!)*

| Byte Offset | Field Name | C Type | Value / Range | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0..1` | `sr_matrix_16` | `uint16_t` (LE) | `0x0000 .. 0xFFFF` | 16-bit shift register state (`U7 QH..QA` : `U6 QH..QA`). |
| `2..3` | `dac_a_mv` | `uint16_t` (LE) | `0 .. 9173` mV | Target `V_SRC_A` output voltage (`U9 DAC_A` $\times 4.480$). |
| `4..5` | `dac_b_mv` | `uint16_t` (LE) | `0 .. 9173` mV | Target `V_SRC_B` output voltage (`U9 DAC_B` $\times 4.480$). |
| `6` | `cal_mux_sel` | `uint8_t` | `0..3` (`CAL_A1:CAL_A0`) | `U10 DG409DJ` channel selection (`0..3`). |
| `7` | `sr_oe_enable` | `uint8_t` | `0`=Outputs Disabled (`SR_OE_N=1`), `1`=Enabled (`SR_OE_N=0`) | Controls `GPIO17` (`SR_OE_N`). |

---

### 3.4 Event & Response Payload Schemas (MCU 1 Slave $\rightarrow$ MCU 2 Master: `0x80` – `0x85`)

#### 1. `EVT_ACK` (`OPCODE = 0x80`, `LEN = 8` Bytes)
Immediate positive acknowledgment sent within $\le 15\text{ ms}$ of receiving any valid command (or as the completion response for `CMD_PING`, `CMD_ABORT`, `CMD_CALIBRATE`, and `CMD_DMM_READ`).

| Byte Offset | Field Name | C Type | Units | Description |
| :---: | :--- | :---: | :---: | :--- |
| `0` | `acked_opcode` | `uint8_t` | Opcode | Echoes the `OPCODE` of the command being acknowledged. |
| `1` | `mcu1_state` | `uint8_t` | Enum | `0`=IDLE, `1`=SCANNING, `2`=SWEEPING, `3`=CALIBRATING, `4`=FAULT_LOCKED. |
| `2..3` | `temp_deci_c` | `int16_t` (LE) | $0.01^\circ\text{C}$ | Current `DS18B20` ambient/DUT temperature (e.g., `2450` = $24.50^\circ\text{C}$). |
| `4..7` | `aux_value_uv` | `int32_t` (LE) | $\mu\text{V}$ | For `CMD_DMM_READ`: calibrated voltage in $\mu\text{V}$ ($0\text{–}33,000,000\ \mu\text{V}$); for `CMD_PING`: MCU 1 uptime in ms. |

#### 2. `EVT_NACK` (`OPCODE = 0x81`, `LEN = 4` Bytes)
Negative acknowledgment transmitted whenever a packet fails CRC-16-CCITT verification, contains an out-of-range parameter, or violates an AFE safety interlock.

| Byte Offset | Field Name | C Type | Error Code Values |
| :---: | :--- | :---: | :--- |
| `0` | `rejected_opcode` | `uint8_t` | Echoes the offending `OPCODE` (or `0xFF` if header/CRC corrupted). |
| `1` | `err_code` | `uint8_t` | `0x01`=`ERR_CRC_MISMATCH`, `0x02`=`ERR_BAD_LEN`, `0x03`=`ERR_UNKNOWN_OPCODE`, `0x04`=`ERR_BUSY_SCANNING`, `0x05`=`ERR_UNSAFE_MATRIX_SHOOT_THROUGH`, `0x06`=`ERR_VEBO_LIMIT_EXCEEDED`, `0x07`=`ERR_OVC_FAULT_ACTIVE`. |
| `2..3` | `err_detail` | `uint16_t` (LE) | Expected vs. received CRC-16, or offending parameter index. |

#### 3. `EVT_RAW_DEBUG` (`OPCODE = 0x82`, `LEN = 24` Bytes)
Complete raw metrology snapshot emitted after `CMD_DEBUG_SET (0x15)` or at each state-machine step when `CMD_SCAN` `STREAM_RAW_DBG` is enabled.

| Byte Offset | Field Name | C Type | Units | Description |
| :---: | :--- | :---: | :---: | :--- |
| `0` | `step_id` | `uint8_t` | `0..31` | Current R.G. Keen state-machine step index. |
| `1` | `cal_mux_ch` | `uint8_t` | `0..3` | Active `U10 DG409DJ` channel (`0=V_SRC_A`, `1=V_SRC_B`, `2=+9V_LIM`, `3=DMM_30V`). |
| `2..3` | `sr_matrix_16` | `uint16_t` (LE) | Bitmask | Active 16-bit `74HC595` switch word (`U7:U6`). |
| `4..5` | `dac_a_code` | `uint16_t` (LE) | `0..4095` | 12-bit code written to `U9 MCP4822` Channel A. |
| `6..7` | `dac_b_code` | `uint16_t` (LE) | `0..4095` | 12-bit code written to `U9 MCP4822` Channel B. |
| `8..11` | `v_tp1_uv` | `int32_t` (LE) | $\mu\text{V}$ | Calibrated node voltage at `TP1_DUT` (`ADS1115 A0` $\times 4.006012$). |
| `12..15` | `v_tp2_uv` | `int32_t` (LE) | $\mu\text{V}$ | Calibrated node voltage at `TP2_DUT` (`ADS1115 A1` $\times 4.006012$). |
| `16..19` | `v_tp3_uv` | `int32_t` (LE) | $\mu\text{V}$ | Calibrated node voltage at `TP3_DUT` (`ADS1115 A2` $\times 4.006012$). |
| `20..23` | `v_cal_uv` | `int32_t` (LE) | $\mu\text{V}$ | Calibrated node voltage at `ADC_CAL` (`ADS1115 A3` $\times 4.006012$). |

#### 4. `EVT_CURVE_PT` (`OPCODE = 0x83`, `LEN = 16` Bytes)
Streamed data point emitted during `CMD_CURVE_SWEEP (0x13)`.

| Byte Offset | Field Name | C Type | Units | Description |
| :---: | :--- | :---: | :---: | :--- |
| `0` | `family_idx` | `uint8_t` | `0..3` | Family curve index ($I_B$ or $V_{\text{GS}}$ step number). |
| `1` | `pt_idx` | `uint8_t` | `0..31` | Point index along current curve (Bit 7 = `1` marks final point of entire sweep). |
| `2..3` | `v_ce_mv` | `int16_t` (LE) | $\text{mV}$ | Measured $V_{\text{CE}}$, $V_{\text{DS}}$, or $V_{\text{AK}}$ across DUT terminals. |
| `4..7` | `i_c_na` | `int32_t` (LE) | $\text{nA}$ | Measured collector/drain/anode current $I_C$ in nanoamperes ($0\text{–}15,000,000\text{ nA}$). |
| `8..11` | `i_b_na` | `int32_t` (LE) | $\text{nA}$ | True injected base current $I_B$ in nanoamperes (or $V_{\text{GS}}$ in $\mu\text{V}$ for FETs). |
| `12..13` | `hfe_x10` | `uint16_t` (LE) | $0.1\times$ | Instantaneous DC gain at this operating point ($h_{\text{FE}} \times 10$). |
| `14..15` | `pdiss_uw` | `uint16_t` (LE) | $\mu\text{W}$ | Instantaneous DUT power dissipation ($V_{\text{CE}} \times I_C$ in microwatts). |

#### 5. `EVT_RESULT_HASH` (`OPCODE = 0x84`, `LEN = 64` Bytes)
Final comprehensive characterization report packet emitted at the conclusion of `CMD_SCAN (0x10)`, containing all extracted Germanium/Silicon parametric metrics and a deterministic **FNV-1a / SHA-folded 32-bit integrity hash** for Web/Android database logging.

| Byte Offset | Field Name | C Type | Units | Description |
| :---: | :--- | :---: | :---: | :--- |
| `0` | `dut_type` | `uint8_t` | Enum | `0`=None/Open, `1`=Ge-PNP, `2`=Ge-NPN, `3`=Si-PNP, `4`=Si-NPN, `5`=N-JFET, `6`=P-JFET, `7`=N-MOS, `8`=P-MOS, `9`=Ge-Diode, `10`=Si-Diode, `11`=Schottky, `12`=Zener, `13`=Resistor, `14`=Capacitor, `15`=Shorted/Damaged. |
| `1` | `pinmap_tp123` | `uint8_t` | Nibbles | Pin assignment for `TP1, TP2, TP3` (e.g., `0x123` = `E-B-C`, `0x456` = `G-D-S`, `0x780` = `A-K-NC`). |
| `2` | `quality_grade` | `uint8_t` | Enum | `0`=Fault/Short, `1`=Leaky/Out-of-Spec, `2`=Standard, `3`=Boutique Pedal Grade, `4`=Museum/Master Reference. |
| `3` | `recommend_circuit` | `uint8_t` | Enum | `0`=N/A, `1`=Fuzz Face Q1 (Low $h_{\text{FE}}$ $70\text{–}85$), `2`=Fuzz Face Q2 ($110\text{–}130$), `3`=Tone Bender MkI/II, `4`=Rangemaster Treble Booster ($I_{\text{CEO}} < 150\ \mu\text{A}$, $h_{\text{FE}} \approx 90$), `5`=RF/IF Mixer. |
| `4..7` | `hfe_true` | `float` (LE) | Ratio | **R.G. Keen True DC Current Gain:** $h_{\text{FE(true)}} = \frac{I_{C(\text{total})} - I_{\text{CEO}}}{I_B}$. |
| `8..11` | `hfe_apparent` | `float` (LE) | Ratio | Uncorrected apparent gain $I_{C(\text{total})} / I_B$ (shows leakage inflation). |
| `12..15` | `iceo_na` | `int32_t` (LE) | $\text{nA}$ | Open-base collector-emitter leakage current $I_{\text{CEO}}$ in nanoamperes. |
| `16..19` | `icbo_na` | `int32_t` (LE) | $\text{nA}$ | Open-emitter collector-base junction leakage $I_{\text{CBO}}$ in nanoamperes. |
| `20..23` | `vbe_uv` | `int32_t` (LE) | $\mu\text{V}$ | Forward base-emitter (or $V_F$ / $V_{\text{GS(off)}}$) voltage in microvolts (Ge: $120,000\text{–}280,000\ \mu\text{V}$). |
| `24..27` | `vcesat_uv` | `int32_t` (LE) | $\mu\text{V}$ | Collector-emitter saturation voltage $V_{\text{CE(sat)}}$ at $I_C = 5\text{ mA}, I_B = 0.5\text{ mA}$. |
| `28..31` | `ic_test_na` | `int32_t` (LE) | $\text{nA}$ | Collector current $I_C$ at the characterization bias point (or $I_{\text{DSS}}$ for JFETs). |
| `32..35` | `ib_test_na` | `int32_t` (LE) | $\text{nA}$ | Base current $I_B$ at the characterization bias point (or $g_m$ in $\mu\text{S}$ for JFETs). |
| `36..39` | `thermal_drift_pct` | `float` (LE) | $\%$ | $\Delta h_{\text{FE}}\%$ drift over the $1.5\text{ s}$ stabilization window. |
| `40..43` | `aux_param_1` | `float` (LE) | $\Omega$ / $\text{nF}$ / $\text{V}$ | Resistance ($\Omega$), Capacitance ($\text{nF}$), Zener $V_Z$ ($\text{V}$), or JFET $R_{\text{DS(on)}}$ ($\Omega$). |
| `44..47` | `aux_param_2` | `float` (LE) | $\text{V}$ / $\text{mA/V}$ | Early voltage estimate $V_A$, JFET transconductance $g_m$ ($\text{mS}$), or ESR. |
| `48..49` | `temp_deci_c` | `int16_t` (LE) | $0.01^\circ\text{C}$ | Ambient/DUT temperature from `DS18B20` at instant of capture (`2500` = $25.00^\circ\text{C}$). |
| `50..51` | `vref_9v_mv` | `uint16_t` (LE) | $\text{mV}$ | Live calibrated `+9V_LIM` rail reading during test (`8990..9010` mV). |
| `52..55` | `scan_time_ms` | `uint32_t` (LE) | $\text{ms}$ | Total execution duration of the scan on MCU 1. |
| `56..59` | `seq_timestamp` | `uint32_t` (LE) | Seconds | Monotonic test counter / epoch tag. |
| `60..63` | `result_hash32` | `uint32_t` (LE) | FNV-1a 32 | Deterministic 32-bit FNV-1a (`init=0x811C9DC5`, `prime=0x01000193`) over Bytes `0..59`. |

#### 6. `EVT_FAULT` (`OPCODE = 0x85`, `LEN = 12` Bytes)
Asynchronous high-priority emergency telemetry emitted immediately whenever hardware `FAULT_OVC_N` (`GPIO25` $15.1\text{ mA}$ interrupt) triggers, DUT power exceeds $35\text{ mW}$, or a rail collapses.

| Byte Offset | Field Name | C Type | Description |
| :---: | :--- | :---: | :--- |
| `0` | `fault_code` | `uint8_t` | `0x01`=`FAULT_HW_OVC_15MA` (`GPIO25`), `0x02`=`FAULT_SW_PDISS_35MW`, `0x03`=`FAULT_9V_RAIL_COLLAPSE`, `0x04`=`FAULT_ADS1115_I2C_TIMEOUT`, `0x05`=`FAULT_THERMAL_RUNAWAY`. |
| `1` | `aborted_step` | `uint8_t` | State-machine step active when fault occurred (`SR_OE_N` already driven HIGH!). |
| `2..3` | `sr_state_at_fault` | `uint16_t` (LE) | 16-bit `74HC595` matrix word active immediately prior to hardware cutoff. |
| `4..5` | `v_9v_lim_mv` | `uint16_t` (LE) | Measured `+9V_LIM` rail voltage at fault trigger. |
| `6..7` | `est_current_ua` | `uint16_t` (LE) | Estimated fault current in microamperes ($\ge 15,120\ \mu\text{A}$ for OVC trip). |
| `8..11` | `v_dut_max_uv` | `int32_t` (LE) | Highest terminal voltage (`TP1..3`) recorded at fault instant. |

---

## 4. Timeout, Retry, Watchdog & Fail-Safe Abort State Machine Rules

```
[MCU 2: UI / Comms Master]                                         [MCU 1: Quiet Analog Slave]
           │                                                                   │
           │  1. Verify SW_STBY == HIGH (Active)                               │
           │     Set U11 GPB7 (ANA_PWR_EN) = HIGH -> Q_ANA_SW ON               │
           │     Wait 80 ms POR settle; pause BQ25895 switching (EN_HIZ/CE)    │
           │                                                                   │
           ├─────────────────── CMD_PING (SEQ=1, OP=0x01) ────────────────────►│
           │◄────────────────── EVT_ACK  (SEQ=1, OP=0x80) ─────────────────────┤  (<= 15 ms ACK window)
           │                                                                   │
           ├─────────────────── CMD_SCAN (SEQ=2, OP=0x10) ────────────────────►│
           │◄────────────────── EVT_ACK  (SEQ=2, OP=0x80, state=SCANNING) ─────┤  (Step Timer = 300 ms,
           │                                                                   │   Total Scan Timer = 5.0 s)
           │◄────────────────── [Optional EVT_RAW_DEBUG (0x82) every <300ms] ──┤
           │◄────────────────── EVT_RESULT_HASH (SEQ=2, OP=0x84, 64 Bytes) ────┤  (Typical scan: 1.65 s)
           │                                                                   │
           │  2. Resume BQ25895 fast charging; render Dual TFT + Web JSON      │
```

### Strict Timing & Recovery Parameters:
1. **Command Acknowledgment Timeout ($T_{\text{ACK}} = 50\text{ ms}$):**
   - Every command transmitted by MCU 2 MUST receive an `EVT_ACK (0x80)` (or `EVT_NACK (0x81)`) matching `SEQ` within **$50\text{ ms}$**.
2. **Per-Step Watchdog Timeout ($T_{\text{STEP\_MAX}} = 300\text{ ms}$):**
   - During multi-step operations (`CMD_SCAN`, `CMD_CALIBRATE`, `CMD_CURVE_SWEEP`), no single metrology permutation or curve point may exceed **$300\text{ ms}$** without progress (for the $1500\text{ ms}$ thermal drift step, MCU 1 emits periodic $250\text{ ms}$ keep-alive `EVT_RAW_DEBUG` / sub-step heartbeats so the $300\text{ ms}$ step watchdog never lapses during healthy operation).
   - **On MCU 1 (Internal Hardware Timer Watchdog):** If any single matrix step remains energized for $> 300\text{ ms}$ due to an $\text{I}^2\text{C}$ hang or firmware stall, MCU 1's hardware Timer0 ISR automatically asserts **`SR_OE_N = HIGH` (`GPIO17`)**, sets DAC outputs to $0\text{ V}$, and emits `EVT_FAULT (0x85)`.
   - **On MCU 2 (Master Step Timeout):** If $> 300\text{ ms}$ elapses between expected step/curve packets, MCU 2 immediately transmits `CMD_ABORT (0x11)`.
3. **Total Scan Hard Watchdog Timeout ($T_{\text{SCAN\_MAX}} = 5000\text{ ms}$ / $5.0\text{ s}$):**
   - From the instant `CMD_SCAN (0x10)` or `CMD_CURVE_SWEEP (0x13)` is dispatched, the complete transaction (`EVT_RESULT_HASH` or final `EVT_CURVE_PT`) **MUST complete within $5.000\text{ seconds}$ ($5000\text{ ms}$)**.
   - If $T_{\text{elapsed}} \ge 5000\text{ ms}$ without completion:
     1. MCU 2 immediately sends **`CMD_ABORT (0x11)`**.
     2. If MCU 1 fails to acknowledge `CMD_ABORT` within $50\text{ ms}$, MCU 2 executes a **Hard Analog Power Cycle** by pulling **`U11 GPB7` (`ANA_PWR_EN`) LOW for $200\text{ ms}$**, which turns OFF `Q_ANA_SW` (`NDP6020P`) and removes all power (`V_ANA_IN = 0.0 V`) from `MOD_BOOST`, `+12V_A`, `+9V_LIM`, `+3V3_ANA`, and `U_MCU1`, physically guaranteeing zero DUT current!
     3. MCU 2 then illuminates `LED_FAULT` (`U11 GPB5`, Red), sounds a 3-beep fault alert on `BZ1`, and displays `LINK / AFE TIMEOUT - SAFE SHUTDOWN` on `MOD_TFT1`.
4. **Deterministic Retry & Framing Resynchronization Rules:**
   - **Maximum Retries (`N_RETRY_MAX = 2`):** On a CRC error (`EVT_NACK` with `ERR_CRC_MISMATCH`) or a $50\text{ ms}$ `T_ACK` timeout, MCU 2 retransmits the command with the **same `SEQ` number** up to **$2$ times** ($3$ total attempts).
   - **Idempotency:** If MCU 1 receives a duplicate `SEQ` matching the last successfully executed non-mutating command, it re-sends the cached `EVT_ACK` without re-triggering the analog sweep.
   - **Byte-Stream Sliding-Window Sync Recovery:** If noise or a partial reset corrupts a frame on the wire, the receiver state machine (`WAIT_AA` $\rightarrow$ `WAIT_55` $\rightarrow$ `READ_HDR_4B` $\rightarrow$ `READ_PAYLOAD` $\rightarrow$ `CHECK_CRC16`) enforces an **inter-byte idle timeout of $10\text{ ms}$** ($\sim 115$ bit periods at $115,200\text{ bps}$). Any gap $> 10\text{ ms}$ mid-packet automatically resets the parser state back to `WAIT_AA`, preventing deadlocks on truncated frames.
