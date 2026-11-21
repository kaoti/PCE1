# Phase 1 Deliverable — Quantitative Silicon Audit & Architectural Rejection of `2N4393` (`p1_2n4393_verdict.md`)

**Project:** KAOTI AUDIO — Rev 5.0 Dual-MCU Germanium & Universal Parametric Characterization Engine  
**Subject:** Formal Device Physics & Circuit Analysis of the `2N4393` Depletion-Mode N-Channel JFET vs. `DG408DJ` / `DG412LDJ` Monolithic CMOS Analog Switches and `LMC6484IN` 20 fA CMOS Buffer  
**Verdict:** **REJECTED** for all single-rail switching matrix and front-end buffer sockets (`0` populated on Rev 5.0 PCB; documented as an optional Golden-Sample JFET calibration DUT only).

---

## 1. Datasheet Parameter Baseline: `2N4393` Depletion-Mode N-JFET

The `2N4393` (TO-18 / TO-92 N-channel silicon junction field-effect transistor, historically used in dual-supply $\pm 15\text{ V}$ analog choppers and sample-and-hold circuits) has the following JEDEC datasheet specifications at $T_A = 25^\circ\text{C}$:

| Parameter | Symbol | Test Condition | Min | Typ | Max | Unit |
| :--- | :---: | :--- | :---: | :---: | :---: | :---: |
| **Gate-Source Cutoff (Pinch-Off) Voltage** | $V_{GS(\text{off})}$ | $V_{DS} = 15\text{ V}, I_D = 10\text{ nA}$ | **$-0.5$** | $-1.7$ | **$-3.0$** | $\text{V}$ |
| **Zero-Gate-Voltage Drain Current** | $I_{DSS}$ | $V_{DS} = 15\text{ V}, V_{GS} = 0\text{ V}$ | **$5.0$** | $15.0$ | **$30.0$** | $\text{mA}$ |
| **Static Drain-Source On-Resistance** | $R_{DS(\text{on})}$ | $V_{GS} = 0\text{ V}, I_D = 1\text{ mA}$ | — | $75$ | **$100$** | $\Omega$ |
| **Channel Off-Leakage Current** | $I_{D(\text{off})}$ | $V_{DS} = 15\text{ V}, V_{GS} = -5.0\text{ V}$ | — | $10$ | **$100$** | $\text{pA}$ |
| **Gate Reverse Leakage Current** | $I_{GSS}$ | $V_{GS} = -20\text{ V}, V_{DS} = 0\text{ V}$ | — | $-10$ | **$-100$** | $\text{pA}$ |
| **Forward Gate-Channel Junction Drop** | $V_{GS(F)}$ | $I_G = +1.0\text{ mA}$ (Forward PN Diode!) | $+0.55$ | $+0.65$ | $+0.75$ | $\text{V}$ |
| **Gate-Source Breakdown Voltage** | $BV_{GSS}$ | $I_G = -1.0\text{ }\mu\text{A}, V_{DS} = 0\text{ V}$ | $-40$ | — | — | $\text{V}$ |

---

## 2. Mathematical Proof of Failure #1: Catastrophic Power-On Shoot-Through (`NORMALLY ON` at $V_{GS} = 0\text{ V}$)

Unlike an enhancement-mode MOSFET or a CMOS transmission gate with an active-HIGH enable (`DG408DJ`, `DG412LDJ`), the `2N4393` is a **depletion-mode N-channel JFET**. Its drain-source current $I_D$ obeys Shockley's equation in saturation and the linear triode relationship near the origin:

$$I_D = I_{DSS}\left(1 - \frac{V_{GS}}{V_{GS(\text{off})}}\right)^2 \quad (\text{saturation}), \qquad R_{DS}(V_{GS}) \approx \frac{R_{DS(\text{on})}}{1 - \frac{V_{GS}}{V_{GS(\text{off})}}} \quad (\text{triode, } |V_{DS}| \ll |V_{GS} - V_{GS(\text{off})}|)$$

### 2.1 Boot / Reset State Analysis ($V_G = 0\text{ V}, V_S = 0\text{ V} \implies V_{GS} = 0\text{ V}$)
During power-up, MCU reset, or shift-register clear (`SRCLR_N = LOW` or `100 kΩ` pull-downs holding control nets at `AGND = 0.000 V`), every gate control line sits at $V_G = 0.000\text{ V}$.
- For any switch connected to or near `AGND` ($V_S = 0.000\text{ V}$):
  $$V_{GS} = V_G - V_S = 0.000\text{ V} - 0.000\text{ V} = \mathbf{0.000\text{ V}}$$
- Substituting $V_{GS} = 0\text{ V}$ into the `2N4393` channel equation yields:
  $$R_{DS}(V_{GS} = 0\text{ V}) = R_{DS(\text{on})} \approx \mathbf{75\,\Omega \text{ to } 100\,\Omega} \quad (\mathbf{\text{FULLY CONDUCTING / NORMALLY ON!}})$$

### 2.2 Multi-Rail Dead-Short Shoot-Through Calculation
If `2N4393` JFETs were used to form the 8-channel per-terminal matrix (`S1..S8`) and ground clamps across `TP1_DUT..TP3_DUT`:
1. **Simultaneous Conduction of All 8 Channels per Terminal:**
   At $t = 0^+$ when `+12V_A` and `+9V_LIM` rise while control gates are at $0\text{ V}$ (or $+3.3\text{ V}$), the low-side switches (`S5: 470 Ω -> AGND`, `S6: 2.49 kΩ -> AGND`, `S7: Direct AGND`) have $V_S = 0\text{ V}$ and $V_G \ge 0\text{ V}$, so they are **hard ON** ($R_{DS(\text{on})} \le 100\,\Omega$).
2. **High-Side `+9V_LIM` / `V_SRC_A` to `AGND` Shoot-Through & Gate-Diode Injection:**
   Even more dangerously, because an N-JFET gate is a **non-isolated P-N junction diode** directly into the channel (unlike an insulated-gate CMOS switch):
   - Whenever any gate is driven to $+3.3\text{ V}$ (`74HC595N` HIGH) while the DUT node or source sits near `AGND` ($0\text{ V}$), the gate-to-channel P-N diode becomes **heavily forward-biased**:
     $$V_{GS} = +3.30\text{ V} - 0.00\text{ V} = +3.30\text{ V} \gg V_{GS(F)} (+0.65\text{ V})$$
   - The gate drive pin directly injects uncontrolled parasitic gate current $I_G = \frac{3.30\text{ V} - 0.65\text{ V}}{R_{\text{driver}}}$ straight into the DUT terminal (`TP1_DUT..TP3_DUT`), corrupting every $\mu\text{A}$-level Germanium $I_{CBO}$, $I_{CEO}$, and $I_B$ measurement and forward-clamping the DUT node to $V_G - 0.65\text{ V}$!
3. **Destructive Shoot-Through Current into Vintage Germanium DUTs:**
   With all low-side and mid-rail `2N4393` paths simultaneously conducting ($R_{DS(\text{on})} \approx 75\text{–}100\,\Omega$) during boot or state transitions, `V_SRC_A`, `V_SRC_B`, `+9V_LIM`, and `AGND` are bridged directly together through the DUT socket before firmware can initialize:
   $$I_{\text{shoot-through}} = \frac{V_{\text{rail}}}{R_{DS(\text{on}),\text{hi}} + R_{DS(\text{on}),\text{lo}}} \approx \frac{9.0\text{ V}}{100\,\Omega + 100\,\Omega} = \mathbf{45.0\text{ mA}} \quad (\text{clamped only by the } 15.1\text{ mA rail limiter})$$
   Applying an unsequenced $9.0\text{ V}$, $15.1\text{ mA}$ fault across the emitter-base junction of a delicate alloy-junction or mesa RF Germanium transistor (`AF114`, `OC44`, $BV_{EBO} \approx 3\text{–}5\text{ V}$) causes **immediate, irreversible base-emitter avalanche degradation**.

---

## 3. Mathematical Proof of Failure #2: Single-Rail Cutoff & High-Side Pinch-Off Impossibility

To operate a `2N4393` as an analog switch across an analog signal swing of $V_{\text{sig}} \in [0.000\text{ V}, +9.173\text{ V}]$, two strict inequalities must be satisfied simultaneously across all process corners ($V_{GS(\text{off})} \in [-0.5\text{ V}, -3.0\text{ V}]$):

### 3.1 Condition to Turn a `2N4393` Switch **OFF** ($I_D < 100\text{ pA}$)
To guarantee pinch-off down to the minimum analog signal voltage $V_{S(\text{min})} = 0.000\text{ V}$ (`AGND`) for a worst-case device with $V_{GS(\text{off,max})} = -3.0\text{ V}$ (plus a $1.0\text{ V}$ guard band for sub-threshold leakage $<100\text{ pA}$):
$$V_{GS(\text{OFF})} \le V_{GS(\text{off,max})} - 1.0\text{ V} = -3.0\text{ V} - 1.0\text{ V} = -4.0\text{ V}$$
$$\implies V_{G(\text{OFF})} = V_{S(\text{min})} + V_{GS(\text{OFF})} = 0.000\text{ V} - 4.0\text{ V} = \mathbf{-4.00\text{ V}} \quad (\text{conventionally } \mathbf{-5.0\text{ V}})$$

- **Why Single-Rail Logic Fails to Turn OFF Low-Side Switches:**
  In our single-supply analog island (`AGND = 0.000 V`, `+3V3_ANA = +3.30 V`, `+12V_A = +12.00 V`), the lowest voltage available anywhere on the PCB is `AGND = 0.000 V`.
  With $V_{G(\text{min})} = 0.000\text{ V}$ and $V_S = 0.000\text{ V}$:
  $$V_{GS} = 0.000\text{ V} > -3.0\text{ V} \implies \textbf{IMPOSSIBLE TO TURN OFF without a dedicated }-5\text{ V negative rail!}$$
  Even if the analog node sits at $+2.0\text{ V}$, setting $V_G = 0\text{ V}$ only produces $V_{GS} = -2.0\text{ V}$, which fails to pinch off any `2N4393` having $V_{GS(\text{off})} \in [-2.0\text{ V}, -3.0\text{ V}]$!

### 3.2 Condition to Turn a High-Side `2N4393` Switch **ON** ($R_{DS} = R_{DS(\text{on})}$ without Gate Injection)
Conversely, to turn a `2N4393` fully **ON** ($V_{GS} = 0.000\text{ V}$) without forward-biasing its gate-channel P-N diode ($V_{GS} \le +0.2\text{ V}$), the gate driver must track the analog source voltage:
$$V_{G(\text{ON})} = V_S \in [0.000\text{ V}, +9.173\text{ V}]$$
- **Why $3.3\text{ V}$ Logic (`74HC595N`) Fails on a $+9.0\text{ V}$ High-Side Switch (`S8 = +9V_LIM` or `V_SRC_A = 9.173 V`):**
  - If the gate is driven directly by $3.3\text{ V}$ CMOS logic ($V_G = +3.30\text{ V}$) while switching a high-side node at $V_S = +9.00\text{ V}$:
    $$V_{GS} = V_G - V_S = +3.30\text{ V} - 9.00\text{ V} = \mathbf{-5.70\text{ V}} < V_{GS(\text{off,max})} (-3.0\text{ V})$$
    The switch is **permanently pinched OFF** even when logic HIGH ($+3.3\text{ V}$) is applied!
  - Conversely, if the gate were level-shifted to $+12.0\text{ V}$ to turn ON at $V_S = +9.0\text{ V}$, then as soon as $V_S$ drops below $+11.4\text{ V}$ (e.g., during a curve sweep from $0\text{ V}$ to $9\text{ V}$), $V_G - V_S = 12.0\text{ V} - V_S > +0.6\text{ V}$, **forward-biasing the JFET gate diode straight into the DUT**!
  - Realizing a bidirectional JFET switch across $0\text{–}9.173\text{ V}$ requires a discrete floating gate bootstrap circuit (1 diode + 1 pull-up resistor + 1 negative $-5\text{ V}$ level-shifter transistor) **per switch**—requiring $27 \times 3 = 81$ extra discrete parts plus a negative $-5\text{ V}$ charge-pump/inverter rail.

---

## 4. Direct Quantitative Comparison: `2N4393` vs. `DG408DJ` / `DG412LDJ` & `LMC6484IN`

| Engineering Requirement | Discrete `2N4393` N-JFET | `U1..U3` `DG408DJ` + `U8` `DG412LDJ` | `U4` `LMC6484IN` (Sense Buffer) |
| :--- | :--- | :--- | :--- |
| **Power-On / $V_{\text{logic}} = 0\text{ V}$ State** | **NORMALLY ON** ($R_{DS} \approx 75\text{–}100\,\Omega$) $\to$ **Failsafe Violation / Shoot-Through** | **NORMALLY OPEN (Hi-Z)** when `EN=0` / `IN=0` $\to$ **100% Boot-Safe** | Unity-gain high-Z input buffer ($>10^{13}\,\Omega$) |
| **Negative Power Supply Required?** | **YES** (Requires $-5.0\text{ V}$ rail to pinch off $V_S = 0\text{ V}$) | **NO** (Single $+12\text{ V}$ supply, $V^- = \text{AGND}$) | **NO** (Single $+12\text{ V}$ supply, True Rail-to-Rail I/O) |
| **Direct $3.3\text{ V}$ CMOS Logic Interface** | **FAILS** (Cannot cut off at $0\text{ V}$ or turn on at $+9\text{ V}$) | **PASS** (Guaranteed $V_{IH} = 2.4\text{ V}$, $V_{IL} = 0.8\text{ V}$) | N/A (Analog unity-gain buffer) |
| **Gate / Control-to-Channel Isolation** | **FAILS** (P-N junction forward-conducts if $V_G > V_S + 0.6\text{ V}$) | **INSULATED CMOS GATE** ($< 1\text{ nA}$ across full $0\text{–}12\text{ V}$ swing) | **INSULATED CMOS GATE** ($20\text{ fA}$ typical input bias) |
| **Off-State Channel Leakage ($25^\circ\text{C}$)** | $10\text{ pA}$ typ / $100\text{ pA}$ max (only if $V_{GS} \le -5\text{ V}$) | **$20\text{ pA}$ typ** (`DG408DJ` / `DG412LDJ`, $<0.5\text{–}1\text{ nA}$ max) | **$20\text{ fA}$ typ** ($0.02\text{ pA}$ — **$500\times$ lower than `2N4393`!**) |
| **On-State Resistance ($R_{DS(\text{on})}$)** | $75\text{–}100\,\Omega$ (varies if $V_G$ does not track $V_S$) | **$50\,\Omega$ typ** (`DG408DJ`) / **$20\,\Omega$ typ** (`DG412LDJ` hard clamp) | Closed-loop output impedance $R_{\text{out}} < 0.1\,\Omega$ |
| **Intrinsic Break-Before-Make (BBM)** | **NONE** (Requires custom multi-rail timing circuitry) | **GUARANTEED** ($t_{\text{OPEN}} \approx 25\text{ ns}$ built into silicon) | N/A |

---

## 5. Evaluation of Potential Reserve Roles for `2N4393`

We audited three classical analog roles to determine whether `2N4393` should be retained anywhere on the Rev 5.0 BOM:

1. **Reserve Role A — Ultra-High-Z Source-Follower Buffer (Superseded by `U4 LMC6484IN`):**
   - A `2N4393` source follower exhibits $I_{GSS} \approx 10\text{ pA}$ ($100\text{ pA}$ max), an unpredictable DC offset equal to $-V_{GS} \in [+0.4\text{ V}, +2.8\text{ V}]$ with a temperature drift of $\sim -2.2\text{ mV}/^\circ\text{C}$, and cannot swing its source down to `AGND` without a negative supply current sink.
   - By contrast, **`U4 LMC6484IN`** (DIP-14 quad CMOS rail-to-rail I/O op-amp) achieves **$I_{IB} = 20\text{ fA}$ ($0.020\text{ pA}$, $500\times$ lower bias current than `2N4393`)**, closed-loop unity gain with $< 0.75\text{ mV}$ offset, and true rail-to-rail input/output swing from `AGND` ($0.000\text{ V}$) to `+12V_A` ($+12.00\text{ V}$) on a single supply.
2. **Reserve Role B — Low-Leakage Picoamp Clamp Diode (Superseded by `6x 1N3595`):**
   - Connecting the Drain and Source of a `2N4393` together uses its gate-channel P-N junction as a low-leakage diode ($I_R \le 100\text{ pA}$), but its maximum forward current is limited to $I_{G(\text{max})} = 10\text{–}50\text{ mA}$ and its package is a 3-lead TO-18/TO-92.
   - By contrast, the **`1N3595`** (`D_CH1..3`, `D_CL1..3`, `D_HVH`, `D_HVL` in axial DO-35) is a purpose-built picoampere-leakage silicon diode rated for $I_R < 1.0\text{ nA}$ ($\sim 10\text{ pA}$ typ at $V_R = 12\text{ V}, 25^\circ\text{C}$), $BV_R = 150\text{ V}$, and $I_F = 150\text{–}225\text{ mA}$ continuous ($500\text{ mA}$ surge).
3. **Reserve Role C — Loose Golden-Sample Reference JFET DUT (VALID BENCH ACCESSORY):**
   - While excluded from the PCB schematic and BOM, a loose `2N4393` kept in the calibration kit serves as an ideal **Golden-Sample Depletion N-JFET Reference DUT** for verifying the instrument's self-biased JFET characterization algorithm (§5.4 of `p1_afe_matrix_topology.md`):
     - Expected measured $I_{DSS} \in [5.0\text{ mA}, 15.0\text{ mA}]$ (via `S1 = 470 Ω` at $V_{GS} = 0\text{ V}$).
     - Expected measured $V_{GS(\text{off})} \in [-0.50\text{ V}, -3.00\text{ V}]$ (via self-biased source sweep on `S3 = 100 kΩ`).
     - Expected measured $R_{DS(\text{on})} \le 100\,\Omega$ at $V_{GS} = 0\text{ V}$, and symmetric Drain/Source terminal interchangeability.
