# Phase 2 Deliverable — Modified R.G. Keen Shunt Scaling, Self-Heating & Complete Metrology Error Budget (`p2_rg_keen_shunt_math.md`)

**Project:** KAOTI AUDIO — Rev 5.0 Dual-MCU Germanium & Universal Parametric Characterization Engine  
**Subsystem:** Quantitative Error-Budget Derivation for `ADS1115` 5-Range PGA Scaling, `0.1%` R.G. Keen Shunts (`470 Ω`, `2.49 kΩ`, `100 kΩ`, `2.20 MΩ`), Live `DG408DJ` $R_{\text{ON}}$ Tare Calibration, and Ratiometric True-$h_{FE}$ Extraction  
**Cross-Reference:** Harmonized with `/Users/kaoti/Desktop/PCE1/artifacts/ge_tester/p1_afe_matrix_topology.md`, `p1_dut_failsafe_math.md`, `p2_afe_ads1115_netlist.md`, and `gate_1_audit.md`.

---

## 1. Complete `ADS1115` PGA Scaling & Current-Shunt Resolution Matrix

In Block 6 (`p2_afe_ads1115_netlist.md`), each DUT terminal `TP1_DUT..TP3_DUT` and the live calibration multiplexer output `CAL_MUX_OUT` (`U10 DG409DJ` Pin 8) are buffered at unity gain by `U4` (`LMC6484IN`, $I_{IB} = 20\text{ fA}$ typ) and attenuated by a matched $0.1\%$ precision $4:1$ divider (`R_D1A..R_D4A = 1.50 kΩ`, `R_D1B..R_D4B = 499 Ω`):

$$G_{\text{DIV}} = \frac{R_{DA} + R_{DB}}{R_{DB}} = \frac{1500.0\,\Omega + 499.0\,\Omega}{499.0\,\Omega} = \frac{1999}{499} = \mathbf{4.00601202\text{ V/V}}, \qquad \alpha_{\text{DIV}} = \frac{1}{G_{\text{DIV}}} = \mathbf{0.24962481\text{ V/V}}$$

For any selected `ADS1115` Programmable Gain Amplifier full-scale setting $V_{\text{FS,PGA}} \in \{4.096\text{ V}, 2.048\text{ V}, 1.024\text{ V}, 0.512\text{ V}, 0.256\text{ V}\}$, the 16-bit signed output code ($32\,768$ positive codes) has an ADC-pin LSB step size $V_{\text{LSB,ADC}}$ and a DUT-referred LSB step size $V_{\text{LSB,DUT}}$ given by:

$$V_{\text{LSB,ADC}} = \frac{V_{\text{FS,PGA}}}{32\,768}, \qquad V_{\text{LSB,DUT}} = V_{\text{LSB,ADC}} \times G_{\text{DIV}} = \frac{V_{\text{FS,PGA}}}{32\,768} \times \frac{1999}{499}$$

### 1.1 Exact Shunt Current Resolution Across All 5 `ADS1115` PGA Ranges
When measuring current $I_{\text{shunt}}$ via the Kelvin node voltage $V_{\text{TP}}$ across an internal matrix channel (`S1..S6`), we distinguish two resistance definitions:
1. **Intrinsic Precision Shunt Resistance ($R_{\text{shunt}}$):** The bare $0.1\%$ resistor value ($R_{\text{LA/LG}} = 470.0\,\Omega$, $R_{\text{KA/KG}} = 2\,490.0\,\Omega$, $R_{\text{MB}} = 100\,000\,\Omega$, $R_{\text{KB}} = 2\,200\,000\,\Omega$).
2. **Total Calibrated Path Resistance ($R_{\text{eff}} = R_{\text{shunt}} + R_{\text{PR}} + R_{\text{ON,cal}}$):** Because the Kelvin sense buffer taps `TP{ch}_DUT` on the **DUT side** of $R_{\text{PR}} = 100.0\,\Omega$ ($0.1\%$) and `U1..U3` (`DG408DJ`, $R_{\text{ON,cal}} \approx 50.0\,\Omega$), any current flowing between the force rail (`V_SRC_A`, `V_SRC_B`, or `AGND`) and `TP{ch}_DUT` drops voltage across the total series path $R_{\text{eff}} = R_{\text{shunt}} + 100.0\,\Omega + R_{\text{ON,cal}}$.
   - For the R.G. Keen $2.49\text{ k}\Omega$ channel (`S2` / `S6`): $R_{\text{shunt}} = 2\,490.0\,\Omega$, and $R_{\text{eff}} = 2\,490.0 + 100.0 + 50.0 = \mathbf{2\,640.0\,\Omega}$.

#### Table 1.1 — Master 5-PGA Voltage & Current Resolution Table (Python-Verified)

| `ADS1115` PGA Range | $V_{\text{LSB,ADC}}$ at ADC Pin | **$V_{\text{LSB,DUT}}$ at DUT Node** | Max Unclipped $V_{\text{DUT}}$ | **`2.49 kΩ` Raw Shunt** ($I_{\text{LSB}} = \frac{V_{\text{LSB,DUT}}}{2490\,\Omega}$) | **`2.49 kΩ` Total Path** ($I_{\text{LSB}} = \frac{V_{\text{LSB,DUT}}}{2640\,\Omega}$) | **`470 Ω` Raw / Path** ($\frac{V_{\text{LSB,DUT}}}{470\,\Omega}$ / $\frac{V_{\text{LSB,DUT}}}{620\,\Omega}$) | **`100 kΩ` Base / Leakage** ($\frac{V_{\text{LSB,DUT}}}{100.15\text{ k}\Omega}$) | **`2.20 MΩ` Ultra-Hi-Z** ($\frac{V_{\text{LSB,DUT}}}{2.20015\text{ M}\Omega}$) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`±4.096 V`** (`001b`) | $125.0000\text{ }\mu\text{V}$ | **$500.752\text{ }\mu\text{V}$** | $12.000\text{ V}^*$ | **$201.105\text{ nA}$** | $189.679\text{ nA}$ | $1\,065.43\text{ nA}$ / $807.66\text{ nA}$ | $5.000\text{ nA}$ | $227.60\text{ pA}$ |
| **`±2.048 V`** (`010b`) | $62.5000\text{ }\mu\text{V}$ | **$250.376\text{ }\mu\text{V}$** | $8.204\text{ V}$ | **$100.553\text{ nA}$** | $94.839\text{ nA}$ | $532.71\text{ nA}$ / $403.83\text{ nA}$ | $2.500\text{ nA}$ | $113.80\text{ pA}$ |
| **`±1.024 V`** (`011b`) | $31.2500\text{ }\mu\text{V}$ | **$125.188\text{ }\mu\text{V}$** | $4.102\text{ V}$ | **$50.276\text{ nA}$** | $47.420\text{ nA}$ | $266.36\text{ nA}$ / $201.92\text{ nA}$ | $1.250\text{ nA}$ | $56.90\text{ pA}$ |
| **`±0.512 V`** (`100b`) | $15.6250\text{ }\mu\text{V}$ | **$62.594\text{ }\mu\text{V}$** | $2.051\text{ V}$ | **$25.138\text{ nA}$** | $23.710\text{ nA}$ | $133.18\text{ nA}$ / $100.96\text{ nA}$ | $625.00\text{ pA}$ | $28.45\text{ pA}$ |
| **`±0.256 V`** (`101b`) | **$7.8125\text{ }\mu\text{V}$** | **$31.297\text{ }\mu\text{V}$** | $1.026\text{ V}$ | **$12.569\text{ nA}$** | **$11.855\text{ nA}$** | **$66.59\text{ nA}$** / **$50.48\text{ nA}$** | **$312.50\text{ pA}$** | **$14.23\text{ pA}$** |

*\*Note: At `PGA = ±4.096 V`, the ADC input is linear up to $V_{\text{DD,ADC}} = 3.300\text{ V}$, while `U4 LMC6484IN` rails at `+12V_A` ($V_{\text{ADC}} = 2.9955\text{ V}$), covering the entire $0.000\text{ V to }12.000\text{ V}$ analog range without saturation.*

> [!TIP]
> **Automatic Dynamic PGA Ranging During Leakage & $h_{FE}$ Reads:**
> During an R.G. Keen PNP $I_{CEO}$ reading across `S6` (`R_KG = 2.49 kΩ`), if a low-leakage Germanium transistor exhibits $I_{CEO} = 40.0\text{ }\mu\text{A}$, the collector voltage is $V_C = 40.0\text{ }\mu\text{A} \times 2640\,\Omega = 105.60\text{ mV}$ ($V_{\text{ADC}} = 26.36\text{ mV} < 256\text{ mV}$). Firmware automatically steps the `ADS1115` from `PGA ±4.096V` down to **`PGA ±0.256V`**, increasing current resolution $16\times$ to **$12.569\text{ nA/LSB}$** ($11.855\text{ nA/LSB}$ effective path) and giving **$8\,908$ digital counts** for a $105.6\text{ mV}$ signal!

---

## 2. Live Self-Calibration of `DG408DJ` Switch $R_{\text{ON}}$ via Channel `S7` & `U8`

A known limitation of uncalibrated CMOS multiplexers is that `DG408DJ` on-resistance ($R_{\text{ON}} \approx 50\,\Omega$ typ, $100\,\Omega$ max at $+12\text{ V}$, with $\sim +0.3\%/^\circ\text{C}$ temperature drift) is in series with the $470\,\Omega$ (`S1/S5`) and $2.49\text{ k}\Omega$ (`S2/S6`) shunts. If left uncalibrated, a $\pm 20\,\Omega$ unit-to-unit variation in $R_{\text{ON}}$ would cause a $\pm 0.76\%$ error on the $2.49\text{ k}\Omega$ shunt and a $\pm 3.2\%$ error on the $470\,\Omega$ shunt.

Rev 5.0 eliminates this error prior to every scan with an automatic **Zero-DUT Two-Point Internal Self-Calibration** (requiring no external shorting plug when the ZIF socket is empty at startup, plus per-channel `S2-to-S7` internal loopback even when a DUT is absent):

1. **Step 1 — Rail & Offset Tare via `U10 DG409DJ` + `S7` (`Direct AGND`):**
   - With all DUT terminals idle (`S7 = Direct AGND` enabled on `U1..U3`, zero current flowing), `U4 LMC6484IN` buffers $0.000\text{ V}$ through the $4:1$ dividers into `ADC_TP1..3`.
   - `MOD_ADS` at `PGA ±0.256V` records the static baseline offset voltage $V_{\text{tare},ch}$ ($ch \in \{1,2,3,4\}$) to within $\pm 1\text{ LSB} = \pm 31.30\text{ }\mu\text{V}$ (referred to DUT), completely nulling the `LMC6484IN` input offset voltage ($V_{OS} \approx \pm 0.50\text{ mV}$ typ)!
2. **Step 2 — Direct Extraction of `DG408DJ` $R_{\text{ON}}(T_{\text{amb}})$ and `DG412LDJ` $R_{\text{ON,U8}}(T_{\text{amb}})$:**
   - Because `DG408DJ` channels `S1..S8` on the same monolithic die are matched to within $\Delta R_{\text{ON,match}} \le 1.5\,\Omega$ ($3\%$), and `U8 DG412LDJ` (`SW_G{ch}`) connects diretamente to `TP{ch}_MUX` (the common `D_COM` Pin 8 of `U{ch}`!):
   - Firmware sets `V_SRC_A = 2.500 V` (measured exactly via `U10 S1A`), closes `U{ch}` switch **`S2` (`R_KA = 2490.0 Ω`)** AND closes **`U8` (`SW_G{ch} = 1`)** on the same node `TP{ch}_MUX` (with empty socket or during boot calibration).
   - Current $I_{\text{cal}} = \frac{V_{\text{SRC\_A}}}{R_{\text{KA}} + R_{\text{ON,DG408}} + R_{\text{ON,DG412}}} \approx 0.9766\text{ mA}$ flows from `V_SRC_A` through `R_KA` ($2490\,\Omega$) and `U{ch} S2` ($R_{\text{ON,DG408}}$) into `TP{ch}_MUX`, and returns to `AGND` through `U8` ($R_{\text{ON,DG412}} \approx 20\,\Omega$).
   - With zero current flowing through `R_PR{ch}` into the open socket, $V_{\text{TP}}(ch) = V_{\text{TP\_MUX}}(ch) = I_{\text{cal}} \cdot R_{\text{ON,DG412}}$, directly measuring both $R_{\text{ON,DG412}}$ and changes over temperature!
   - Residual calibrated switch resistance uncertainty after tare is bounded by channel-to-channel matching:
     $$\delta R_{\text{ON,res}} \le \mathbf{\pm 1.0\,\Omega} \quad \left(\mathbf{\pm 0.0379\%} \text{ of } R_{\text{eff,2k49}} = 2640\,\Omega\right)$$

---

## 3. Shunt Wattage, Self-Heating ($\Delta T$) & TCR Drift Under Normal and Fault Currents

All precision shunts in Block 5 and Block 6 use low-TCR ($25\text{ ppm}/^\circ\text{C}$, $0.1\%$) metal-film axial resistors (`R_PR1..3` rated $0.50\text{ W}$, $\theta_{JA} \approx 85^\circ\text{C/W}$; all other $0.1\%$ resistors rated $0.25\text{ W}$ DIN0207, $\theta_{JA} \approx 125^\circ\text{C/W}$).

#### Table 3.1 — Shunt Thermal Dissipation & TCR Resistance Shift Audit

| Resistor Group | Value & Rating | Worst-Case Continuous Current ($I_{\text{max}}$) | Peak Continuous Power ($P_{\text{max}} = I_{\text{max}}^2 R$) | Steady-State Temp Rise ($\Delta T_{\infty} = P \cdot \theta_{JA}$) | $50\text{ ms}$ Pulse Temp Rise ($\Delta T_{50\text{ms}}, \tau_{\text{th}} \approx 12\text{ s}$) | Worst-Case TCR Drift at $25\text{ ppm}/^\circ\text{C}$ (Steady / Pulse) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **`R_PR1..3`** (Kelvin Protection) | $100.0\,\Omega$, **$0.50\text{ W}$**, $0.1\%$ | $15.120\text{ mA}$ (`+9V_LIM` dead-short) | **$22.861\text{ mW}$** ($4.57\%$ of $0.5\text{ W}$) | $+1.943^\circ\text{C}$ | $+0.0081^\circ\text{C}$ | $48.58\text{ ppm}$ ($0.0049\%$) / **$0.20\text{ ppm}$** |
| **`R_LA1..3` / `R_LG1..3`** (`S1 / S5`) | $470.0\,\Omega$, **$0.25\text{ W}$**, $0.1\%$ | $14.795\text{ mA}$ (`9.173V` via $470+150\,\Omega$) | **$102.882\text{ mW}$** ($41.15\%$ of $0.25\text{ W}$) | $+12.860^\circ\text{C}$ | $+0.0535^\circ\text{C}$ | $321.5\text{ ppm}$ ($0.0322\%$) / **$1.34\text{ ppm}$** |
| **`R_KA1..3` / `R_KG1..3`** (`S2 / S6` R.G. Keen) | $2\,490.0\,\Omega$, **$0.25\text{ W}$**, $0.1\%$ | $3.475\text{ mA}$ (`9.173V` via $2490+150\,\Omega$) | **$30.062\text{ mW}$** ($12.02\%$ of $0.25\text{ W}$) | $+3.758^\circ\text{C}$ | $+0.0156^\circ\text{C}$ | $93.94\text{ ppm}$ ($0.0094\%$) / **$0.39\text{ ppm}$** |
| **`R_MB1..3`** (`S3` Base / Leakage) | $100.0\text{ k}\Omega$, **$0.25\text{ W}$**, $0.1\%$ | $91.593\text{ }\mu\text{A}$ (`9.173V` via $100.15\text{ k}\Omega$) | **$0.839\text{ mW}$** ($0.34\%$ of $0.25\text{ W}$) | $+0.105^\circ\text{C}$ | $+0.0004^\circ\text{C}$ | $2.62\text{ ppm}$ ($0.0003\%$) / **$0.01\text{ ppm}$** |
| **`R_KB1..3`** (`S4` Mega-Ohm) | $2.200\text{ M}\Omega$, **$0.25\text{ W}$**, $0.1\%$ | $4.169\text{ }\mu\text{A}$ (`9.173V` via $2.20015\text{ M}\Omega$) | **$0.038\text{ mW}$** ($0.015\%$ of $0.25\text{ W}$) | $+0.005^\circ\text{C}$ | $<0.0001^\circ\text{C}$ | $0.12\text{ ppm}$ ($0.00001\%$) / **$0.00\text{ ppm}$** |
| **`R_D1A..4A` + `R_D1B..4B`** ($4:1$ Dividers) | $1.50\text{ k}\Omega + 499\,\Omega$, **$0.25\text{ W}$**, $0.1\%$ | $4.589\text{ mA}$ (at $V_{\text{BUF}} = 9.173\text{ V}$) | **$31.58\text{ mW}$** (`1.5k`) / **$10.51\text{ mW}$** (`499R`) | $+3.95^\circ\text{C}$ / $+1.31^\circ\text{C}$ | $+0.016^\circ\text{C}$ | Differential $\Delta T = 2.63^\circ\text{C} \implies \mathbf{49.4\text{ ppm}}$ ($0.0049\%$) |

---

## 4. Complete Modified R.G. Keen Error Budget ($I_{CEO}$ and True-$h_{FE}$)

### 4.1 Sensitivity Derivation for the $4:1$ Attenuator Ratio ($G_{\text{DIV}} = 1 + \frac{R_{DA}}{R_{DB}}$)
Taking the logarithmic differential of $G_{\text{DIV}} = \frac{R_{DA} + R_{DB}}{R_{DB}}$ with respect to individual resistor tolerances $\frac{\delta R_{DA}}{R_{DA}} \le \pm 0.10\%$ and $\frac{\delta R_{DB}}{R_{DB}} \le \pm 0.10\%$:
$$\frac{\delta G_{\text{DIV}}}{G_{\text{DIV}}} = \left(\frac{R_{DA}}{R_{DA} + R_{DB}}\right) \left(\frac{\delta R_{DA}}{R_{DA}} - \frac{\delta R_{DB}}{R_{DB}}\right) = \left(\frac{1500}{1999}\right)\left(\frac{\delta R_{DA}}{R_{DA}} - \frac{\delta R_{DB}}{R_{DB}}\right) = 0.750375 \left(\frac{\delta R_{DA}}{R_{DA}} - \frac{\delta R_{DB}}{R_{DB}}\right)$$
- **Worst-Case Additive Divider Ratio Error:** $\left|\frac{\delta G_{\text{DIV}}}{G_{\text{DIV}}}\right|_{\text{WC}} = 0.750375 \times (0.10\% + 0.10\%) = \mathbf{\pm 0.1501\%}$
- **Statistical RSS Divider Ratio Error:** $\left|\frac{\delta G_{\text{DIV}}}{G_{\text{DIV}}}\right|_{\text{RSS}} = 0.750375 \times \sqrt{(0.10\%)^2 + (0.10\%)^2} = \mathbf{\pm 0.1061\%}$

---

### 4.2 Error Budget #1: Germanium Open-Base Collector-Emitter Leakage ($I_{CEO}$)
Evaluated at a representative vintage Germanium PNP leakage of **$I_{CEO} = 50.0\text{ }\mu\text{A}$** across the R.G. Keen $2.49\text{ k}\Omega$ collector shunt (`S6`: $R_{\text{eff}} = 2640\,\Omega \implies V_C = 132.00\text{ mV}$, measured on `ADS1115` `PGA = ±0.256 V` where $V_{\text{LSB,DUT}} = 31.297\text{ }\mu\text{V}$):

| Error Source | Physical Parameter & Mitigation Mechanism | Typical Value ($25^\circ\text{C}$) | Worst-Case Max ($25^\circ\text{C}$) | Relative Error in $I_{CEO}$ (Typical) | Relative Error in $I_{CEO}$ (Worst-Case) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **1. Shunt + `R_PR` Resistance Tolerance** | `R_KG` ($2.49\text{ k}\Omega, 0.1\%$) + `R_PR` ($100\,\Omega, 0.1\%$) | $\pm 0.05\%$ | $\pm 0.1000\%$ | $\pm 0.0500\%$ | $\pm 0.1000\%$ |
| **2. Residual `DG408DJ` $R_{\text{ON}}$ after Tare** | Pre-scan self-calibration removes nominal $50\,\Omega$; residual $\Delta R_{\text{ON}} \le \pm 0.5\text{ to }1.0\,\Omega$ out of $2640\,\Omega$ | $\pm 0.5\,\Omega$ | $\pm 1.0\,\Omega$ | $\pm 0.0189\%$ | $\pm 0.0379\%$ |
| **3. Shunt Self-Heating TCR Drift** | At $50\text{ }\mu\text{A}$, $P = 6.23\text{ }\mu\text{W} \implies \Delta T < 0.001^\circ\text{C}$ ($25\text{ ppm}/^\circ\text{C}$) | $< 0.03\text{ ppm}$ | $1.0\text{ ppm}$ | $\pm 0.0000\%$ | $\pm 0.0001\%$ |
| **4. $4:1$ Sense Divider Ratio (`1.50k / 499R`)** | $\frac{1500}{1999}(\delta R_{DA}/R_{DA} - \delta R_{DB}/R_{DB})$ with $0.1\%$ resistors | $\pm 0.1061\%$ (RSS) | $\pm 0.1501\%$ (WC) | $\pm 0.1061\%$ | $\pm 0.1501\%$ |
| **5. `LMC6484IN` Input Bias Current ($I_{IB}$)** | $I_{IB} = 20\text{ fA}$ typ ($4.0\text{ pA}$ max) out of $I_{CEO} = 50\text{ }\mu\text{A}$ | $20\text{ fA}$ | $4.0\text{ pA}$ | $\pm 4.0 \times 10^{-8}\%$ | $\pm 0.000008\%$ |
| **6. Off-State Matrix & `1N3595` Node Leakage** | `DG408DJ` ($20\text{ pA}$ typ / $0.5\text{ nA}$ max) + `DG412LDJ` ($20\text{ pA}$ / $1.0\text{ nA}$) + `2x 1N3595` ($20\text{ pA}$ / $2.0\text{ nA}$) | $60\text{ pA}$ ($0.060\text{ nA}$) | $3.50\text{ nA}$ | $\pm 0.00012\%$ | $\pm 0.0070\%$ |
| **7. `LMC6484IN` Offset ($V_{OS}$) After Auto-Tare** | Raw $\pm 0.5\text{ mV}$ nulled by `S7` zero-current tare to $\pm 1\text{ LSB}_{\text{DUT}} = \pm 31.30\text{ }\mu\text{V}$ out of $132.0\text{ mV}$ | $\pm 15.6\text{ }\mu\text{V}$ ($\frac{1}{2}\text{ LSB}$) | $\pm 31.30\text{ }\mu\text{V}$ ($1\text{ LSB}$) | $\pm 0.0119\%$ | $\pm 0.0237\%$ |
| **8. `ADS1115` PGA Gain Error & INL** | Factory-trimmed `PGA ±0.256V` gain error ($\pm 0.01\%$ typ, $\pm 0.15\%$ max at $25^\circ\text{C}$) + $\pm 1\text{ LSB}$ INL | $\pm 0.01\% + \frac{1}{2}\text{ LSB}$ | $\pm 0.15\% + 1\text{ LSB}$ | $\pm 0.0219\%$ | $\pm 0.1737\%$ |
| **TOTAL $I_{CEO}$ UNCERTAINTY ($50\text{ }\mu\text{A}$)** | **Combined RSS (Typical) vs. Linear Sum (Worst-Case)** | — | — | **$\mathbf{\pm 0.1205\%}$**<br/>($\pm 60.3\text{ nA}$) | **$\mathbf{\pm 0.4925\%}$**<br/>($\pm 246.3\text{ nA}$) |

---

### 4.3 Error Budget #2: Ratiometric True Current Gain ($h_{FE(\text{true})} = \frac{I_{C(\text{total})} - I_{CEO}}{I_B}$)
Evaluated for a vintage Germanium transistor (`OC44` / `AC128`) with $h_{FE(\text{true})} = 100.0$, $I_B = 25.00\text{ }\mu\text{A}$ (forced via `S3` $R_{\text{MB}} = 100.0\text{ k}\Omega$ with $V_B - V_{\text{SRC\_B}} = 2.5038\text{ V}$), $I_{CEO} = 150.0\text{ }\mu\text{A}$, and $I_{C(\text{total})} = 2\,650.0\text{ }\mu\text{A}$ ($2.650\text{ mA}$ across `S6` $R_{\text{eff},C} = 2640\,\Omega$):

$$\boxed{h_{FE(\text{true})} = \frac{I_{C(\text{total})} - I_{CEO}}{I_B} = \left(\frac{R_{\text{MB},B} + R_{\text{PR},B} + R_{\text{ON},B}}{R_{\text{KG},C} + R_{\text{PR},C} + R_{\text{ON},C}}\right) \cdot \left(\frac{G_{\text{DIV},C}}{G_{\text{DIV},B}}\right) \cdot \left(\frac{N_{\text{ADC},C(\text{total})} - N_{\text{ADC},C(\text{CEO})}}{N_{\text{ADC},B} - N_{\text{ADC,SRC\_B}}}\right)}$$

Notice **three profound architectural cancellations** in this equation:
1. **100% Cancellation of Collector Op-Amp Offset ($V_{OS,C}$):** Because $N_{\text{ADC},C(\text{total})}$ and $N_{\text{ADC},C(\text{CEO})}$ are digitized through the **exact same collector sense channel `TP{C}_DUT`** at the same PGA setting, the static offset $V_{OS,C}$ of `U4` (`LMC6484IN`) cancels identically in the subtraction $N_{\text{ADC},C(\text{total})} - N_{\text{ADC},C(\text{CEO})}$!
2. **100% Cancellation of `ADS1115` Internal Voltage Reference Drift:** Because both the collector differential counts (numerator) and the base current differential counts (denominator, where $V_{\text{SRC\_B}}$ is read back live via `U10 S2A` into the same `ADS1115`!) scale with the same internal `ADS1115` reference voltage $V_{\text{REF,ADS}}$, any absolute reference tolerance or thermal drift cancels completely in the ratio!
3. **100% Cancellation of $I_{CEO}$ Leakage Inflation:** Unlike simple component testers that compute $h_{FE(\text{false})} = \frac{I_{C(\text{total})}}{I_B} = \frac{2650\text{ }\mu\text{A}}{25\text{ }\mu\text{A}} = 106.0$ (or up to $160+$ when $I_{CEO} = 1.5\text{ mA}$, a $+60\%$ error!), subtracting $I_{CEO}$ measured milliseconds earlier on the identical thermal and voltage operating point yields the true small-signal/DC current gain $h_{FE(\text{true})} = 100.0$.

| Error Source in Ratiometric $h_{FE(\text{true})}$ | Mathematical Contribution | Typical RSS Component | Worst-Case Max Component |
| :--- | :--- | :---: | :---: |
| **1. Collector-to-Base Shunt Ratio (`R_MB` / `R_KG`)** | Two independent $0.1\%$ metal-film resistors ($100\text{ k}\Omega$ vs. $2.49\text{ k}\Omega$) | $\sqrt{0.10^2 + 0.10^2} = \pm 0.1414\%$ | $0.10\% + 0.10\% = \pm 0.2000\%$ |
| **2. Residual Calibrated Switch $R_{\text{ON}}$ Error** | $\pm 0.5\,\Omega$ typ ($\pm 1.0\,\Omega$ max) on $2640\,\Omega$ collector path; negligible ($0.001\%$) on $100.15\text{ k}\Omega$ base path | $\pm 0.0189\%$ | $\pm 0.0379\%$ |
| **3. Relative $4:1$ Divider Mismatch ($G_{\text{DIV},C} / G_{\text{DIV},B}$)** | Ratio of two $0.1\%$ dividers (or nulled via `U10` cross-channel calibration) | $\sqrt{0.1061^2 + 0.1061^2} = \pm 0.1501\%$ | $0.1501\% + 0.1501\% = \pm 0.3002\%$ |
| **4. Base Sense vs. `CAL` Op-Amp Offset Residual** | Post-tare residual $\pm 1\text{ LSB} = \pm 250.4\text{ }\mu\text{V}$ (`PGA ±2.048V`) out of $V_B - V_{\text{SRC\_B}} = 2.5038\text{ V}$ | $\pm 0.0100\%$ | $\pm 0.0200\%$ |
| **5. Base Terminal Off-Switch / Diode Leakage** | $60\text{ pA}$ typ ($3.5\text{ nA}$ max) out of $I_B = 25.00\text{ }\mu\text{A}$ | $\pm 0.00024\%$ | $\pm 0.0140\%$ |
| **6. `ADS1115` Differential Nonlinearity & Quantization** | $\pm 1\text{ LSB}$ across $\Delta V_C = 6.60\text{ V}$ ($26\,360$ counts) and $\Delta V_B = 2.50\text{ V}$ ($10\,000$ counts) | $\pm 0.0107\%$ | $\pm 0.0250\%$ |
| **TOTAL TRUE-$h_{FE}$ UNCERTAINTY** | **Combined RSS (Typical) vs. Linear Sum (Worst-Case)** | **$\mathbf{\pm 0.2076\%}$**<br/>($\pm 0.21\,h_{FE}$ at $\beta=100$) | **$\mathbf{\pm 0.5971\%}$**<br/>($\pm 0.60\,h_{FE}$ at $\beta=100$) |
