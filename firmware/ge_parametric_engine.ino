/*
 * KAOTI AUDIO — Ge-LAB PRO Parametric Characterization Engine (Rev 4.0 Flagship)
 * Dual 1.9" ST7789 IPS Curve Tracer + Fluke-Style DMM + Wi-Fi/BLE Sync + 56k Modem Audio
 * Target MCU : WEMOS LOLIN32 Lite V1.0.0 (ESP32 REV1 USB-C)
 *
 * Rev 4.0 Flagship Upgrades (Built on 100% of Rev 3.0 Dual-TFT + Rev 2.0 Metrology Engine):
 *   1. Dual MCP23017-E/SP I2C Expanders (U11 @ 0x20 & U12 @ 0x21, MIRROR+ODR INT on IO32):
 *        U11 (0x20): Nintendo D-Pad (UP/DOWN/LEFT/RIGHT), BTN_A, BTN_B, SW_STBY (LOW=Standby),
 *                    TFT_BLK (GPA7), ENC_B (GPB0..2), ENC_C (GPB3..5), CAL_A0/A1 (GPB6..7)
 *        U12 (0x21): Top 6-LED Status Bar (PWR/CHG/WIFI/SCAN/PASS/WARN on GPA0..5),
 *                    TFT_RES (GPA6), ANA_EN_N (GPA7: LOW=Analog ON, HIGH=Standby Analog OFF)
 *   2. Passive Piezo Buzzer + 56k Dial-Up Modem Synthesizer on Hardware PWM (PIN_BZ_PWM = 13):
 *        Startup retro chime, non-blocking V.21/CED/V.34 handshake chirps during scan steps,
 *        solid 2700 Hz DMM continuity tone (<30 Ohm), plus 'm' / 'modem' CLI demo.
 *   3. Deterministic Transistor Characterization Hash (e.g. "GEP-098-L042-7A3F"):
 *        Generated for every DUT (type prefix + primary bucket + leakage bucket + 4-hex CRC16),
 *        displayed prominently on TFT2, and emitted as "hash" in Serial/HTTP/BLE JSON.
 *   4. Standby Mode (SW_STBY LOW) + Wi-Fi SoftAP/STA Web API + Optional BLE GATT + 32-Entry Ring Buffer:
 *        Radio stays OFF during analog measurements for zero RF interference.
 *        Standby powers down MT3608 boost (ANA_EN_N=HIGH), serves /api/latest, /api/history, /api/status
 *        on port 80, optional POST to companion server (:8080/api/tests) after allSafeHiZ().
 *   5. Verbose Hardware Debug ('V' / 'verbose') & 10-Step Benchtop Self-Test Suite ('T' / 'selftest').
 */

// Modular Radio Build Switches (ENABLE_WIFI_SYNC=1 + ENABLE_BLE_TELEMETRY=0 fits 1.3MB default partition;
// set ENABLE_BLE_TELEMETRY=1 with PartitionScheme=no_ota 2MB for simultaneous Wi-Fi + Bluedroid GATT NUS)
#ifndef ENABLE_WIFI_SYNC
#define ENABLE_WIFI_SYNC     1
#endif
#ifndef ENABLE_BLE_TELEMETRY
#define ENABLE_BLE_TELEMETRY 0
#endif

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>

#if ENABLE_WIFI_SYNC
#include <WiFi.h>
#include <WebServer.h>
#endif

#if ENABLE_BLE_TELEMETRY
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif

#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ======================= REV 4.0 EXACT HARDWARE PINOUT =======================
// I2C Bus (400 kHz, 9-clock recovery + 50 ms timeout)
static constexpr int PIN_I2C_SDA   = 19;
static constexpr int PIN_I2C_SCL   = 23;
static constexpr int PIN_ADS_ALRT  = 34; // Input-only, active-LOW RDY pulse (10k pull-up)
static constexpr int PIN_MCP_INT   = 32; // Active-LOW wired-OR open-drain INT from U11 & U12

// Shared Hardware SPI Bus (Mode 0)
static constexpr int PIN_SPI_SCK   = 14;
static constexpr int PIN_SPI_MOSI  = 27;
static constexpr int PIN_TFT1_CS   = 16; // Top 1.9" ST7789 Curve Tracer CS
static constexpr int PIN_TFT2_CS   = 33; // Bottom 1.9" ST7789 Multimeter/Telemetry CS
static constexpr int PIN_TFT_DC    = 25; // Shared D/C for TFT1 & TFT2
static constexpr int PIN_DAC_CS    = 26; // U9 MCP4822-E/P CS_N (10k pull-up)
static constexpr int PIN_SR_LATCH  = 18; // U6/U7 74HC595N RCLK (idle LOW)
static constexpr int PIN_SR_OE_N   = 17; // Non-strapping OE_N (10k pull-up to +3V3)

// Hardware PWM Piezo Synth, Thermal Probe & Fast ADC1 Cap Timing
static constexpr int PIN_BZ_PWM    = 13; // Passive piezo buzzer PWM (physical mute SW_BZ in series)
static constexpr int PIN_TEMP_DQ   = 4;  // DS18B20 1-Wire (4.7k pull-up)
static constexpr int PIN_FAST_TP1  = 36; // VP   (10k isolated fast RC timing)
static constexpr int PIN_FAST_TP2  = 39; // VN   (10k isolated fast RC timing)
static constexpr int PIN_FAST_TP3  = 35; // IO35 (10k isolated fast RC timing)
static constexpr int FAST_TP_PINS[3] = {PIN_FAST_TP1, PIN_FAST_TP2, PIN_FAST_TP3};

// Dual MCP23017-E/SP I2C Expanders
static constexpr uint8_t I2C_ADDR_U11 = 0x20; // Controls, Encoders, SW_STBY, TFT_BLK, CAL_MUX
static constexpr uint8_t I2C_ADDR_U12 = 0x21; // Top 6-LED Array, TFT_RES, ANA_EN_N

// U11 (0x20) Port A: IODIRA=0x7F, GPPUA=0x7F (bits 0..6 inputs w/ pull-ups, bit 7 output)
static constexpr uint8_t U11_GPA_BTN_UP    = 0; // Nintendo D-Pad UP (active LOW)
static constexpr uint8_t U11_GPA_BTN_DOWN  = 1; // Nintendo D-Pad DOWN (active LOW)
static constexpr uint8_t U11_GPA_BTN_LEFT  = 2; // Nintendo D-Pad LEFT (active LOW)
static constexpr uint8_t U11_GPA_BTN_RIGHT = 3; // Nintendo D-Pad RIGHT (active LOW)
static constexpr uint8_t U11_GPA_BTN_A     = 4; // A Button: Confirm / Scan / Hold (active LOW)
static constexpr uint8_t U11_GPA_BTN_B     = 5; // B Button: Back / Mode / Abort (active LOW)
static constexpr uint8_t U11_GPA_SW_STBY   = 6; // Standby slide switch (LOW = Standby Mode)
static constexpr uint8_t U11_GPA_TFT_BLK   = 7; // Output: HIGH = Backlight ON

// U11 (0x20) Port B: IODIRB=0x3F, GPPUB=0x3F (bits 0..5 inputs w/ pull-ups, bits 6..7 outputs)
static constexpr uint8_t U11_GPB_ENC_B_A   = 0; // Top Knob quadrature A
static constexpr uint8_t U11_GPB_ENC_B_B   = 1; // Top Knob quadrature B
static constexpr uint8_t U11_GPB_ENC_B_SW  = 2; // Top Knob push-switch (active LOW)
static constexpr uint8_t U11_GPB_ENC_C_A   = 3; // Bottom Knob quadrature A
static constexpr uint8_t U11_GPB_ENC_C_B   = 4; // Bottom Knob quadrature B
static constexpr uint8_t U11_GPB_ENC_C_SW  = 5; // Bottom Knob push-switch (active LOW)
static constexpr uint8_t U11_GPB_CAL_A0    = 6; // Output: U10 DG409DJ A0
static constexpr uint8_t U11_GPB_CAL_A1    = 7; // Output: U10 DG409DJ A1

// U12 (0x21) Port A: IODIRA=0x00 (all outputs)
static constexpr uint8_t U12_GPA_LED_PWR   = 0; // Green Power LED
static constexpr uint8_t U12_GPA_LED_CHG   = 1; // Amber Standby/Chg LED
static constexpr uint8_t U12_GPA_LED_WIFI  = 2; // Cyan Wi-Fi/BLE Radio LED
static constexpr uint8_t U12_GPA_LED_SCAN  = 3; // Blue Active Scan LED
static constexpr uint8_t U12_GPA_LED_PASS  = 4; // Green Pass LED
static constexpr uint8_t U12_GPA_LED_WARN  = 5; // Red Warn/Abort LED
static constexpr uint8_t U12_GPA_TFT_RES   = 6; // Active-LOW reset to BOTH ST7789 (idle HIGH)
static constexpr uint8_t U12_GPA_ANA_EN_N  = 7; // LOW = Analog +12V/+9V ON; HIGH = Analog OFF

// ======================= ANALOG, SPI & SAFETY CONSTANTS ======================
static constexpr uint8_t  I2C_ADDR_ADS1115 = 0x48;
static constexpr uint32_t I2C_FREQ_HZ      = 400000;
static constexpr uint16_t I2C_TIMEOUT_MS   = 50;

static constexpr uint32_t SPI_FREQ_TFT     = 27000000UL; // 27 MHz ST7789V2
static constexpr uint32_t SPI_FREQ_DAC     = 10000000UL; // 10 MHz MCP4822
static constexpr uint32_t SPI_FREQ_SR      = 4000000UL;  //  4 MHz 74HC595N

// 4:1 precision low-Z dividers: 1.50k / 499R (0.1%)
static constexpr float R_DIV_TOP     = 1500.0f;
static constexpr float R_DIV_BOT     = 499.0f;
static constexpr float DIV_GAIN      = (R_DIV_TOP + R_DIV_BOT) / R_DIV_BOT; // 4.006012f
static constexpr float R_TH_DIV      = 374.43f; // (1500 * 499) / (1500 + 499)

// 11:1 HV DMM divider on J_TERM5 Pin 5 (100.0k / 10.0k 0.1% -> U10 S4A)
static constexpr float R_HV_TOP      = 100000.0f;
static constexpr float R_HV_BOT      = 10000.0f;
static constexpr float HV_DIV_RATIO  = (R_HV_TOP + R_HV_BOT) / R_HV_BOT; // 11.000f
static constexpr float HV_GAIN       = HV_DIV_RATIO * DIV_GAIN;          // 44.066132f
static constexpr float DMM_HV_MAX_V  = 33.000f;
static constexpr float DMM_CONT_OHM  = 30.0f;
static constexpr uint32_t DMM_CONT_FREQ_HZ = 2700;

// MCP4822 1x gain (2.048V / 4096) + U5 op-amp (1 + 34.8k / 10.0k = 4.480x)
static constexpr float DAC_VREF_1X   = 2.048f;
static constexpr float DAC_GAIN      = 1.0f + (34800.0f / 10000.0f); // 4.480f
static constexpr float DAC_MAX_V     = (DAC_VREF_1X * (4095.0f / 4096.0f)) * DAC_GAIN; // 9.1728V
static constexpr uint16_t DAC_SETTLE_MS = 14; // 10k * 100nF RC filter

// Precision series path resistances (Ohms)
static constexpr float R_PROT        = 100.0f;     // Kelvin shunt on each DUT pin
static constexpr float R_LOW         = 470.0f;     // S1 (V_SRC_A), S5 (GND)
static constexpr float R_KEEN        = 2490.0f;    // S2 (V_SRC_A), S6 (GND)
static constexpr float R_MID         = 100000.0f;  // S3 (V_SRC_B)
static constexpr float R_KEEN_BASE   = 2200000.0f; // S4 (V_SRC_B)
static constexpr float R_DG408_NOM   = 50.0f;
static constexpr float R_DG412_NOM   = 20.0f;

// Safety & metrology limits
static constexpr float V_ABORT_HARD  = 9.50f;      // Stage 0 live-voltage hard abort
static constexpr float V_BLEED_START = 0.050f;     // Stage 0 soft discharge threshold
static constexpr float V_BLEED_DONE  = 0.020f;     // Stage 0 clean threshold
static constexpr float P_DUT_MAX_MW  = 35.0f;      // Germanium continuous dissipation ceiling
static constexpr uint16_t BBM_DEAD_US = 60;        // Mandatory 60 us break-before-make

static constexpr uint32_t CAL_MAGIC   = 0x4B474532UL; // "KGE2" (compatible NVS cal struct)
static constexpr uint16_t CAL_VERSION = 2;
static constexpr char NVS_NAMESPACE[] = "ge_engine_v2";
static constexpr char NVS_KEY_CAL[]   = "cal_v2";
static constexpr char NVS_NS_HIST[]   = "ge_hist_v4";
static constexpr char NVS_NS_WIFI[]   = "ge_wifi_v4";

// ======================= UI COLOR PALETTE (RGB565) ===========================
static constexpr uint16_t COL_BG        = 0x0063; // Deep navy (#040B1A)
static constexpr uint16_t COL_GRID      = 0x1947; // Graticule slate blue
static constexpr uint16_t COL_GRID_AXIS = 0x2A6B; // Axis center ticks
static constexpr uint16_t COL_BORDER    = 0x4C9F; // Steel cyan border
static constexpr uint16_t COL_SIDEBAR   = 0x098F; // CHY-BT2.1 blue sidebar
static constexpr uint16_t COL_SIDE_HDR  = 0x12D7; // Sidebar accent header
static constexpr uint16_t COL_CARD      = 0x08E7; // Dark card fill
static constexpr uint16_t COL_TEXT      = 0xFFFF; // Crisp white
static constexpr uint16_t COL_MUTED     = 0x9CD3; // Soft silver
static constexpr uint16_t COL_LABEL     = 0x67FF; // Cyan label
static constexpr uint16_t COL_RED       = 0xF800;
static constexpr uint16_t COL_CYAN      = 0x07FF;
static constexpr uint16_t COL_WHITE     = 0xFFFF;
static constexpr uint16_t COL_YELLOW    = 0xFFE0;
static constexpr uint16_t COL_GREEN     = 0x07E0;
static constexpr uint16_t COL_MAGENTA   = 0xF81F;
static constexpr uint16_t COL_ORANGE    = 0xFD20;
static constexpr uint16_t COL_GE_BADGE  = 0xC280; // Copper/amber Germanium
static constexpr uint16_t COL_SI_BADGE  = 0x1C9F; // Cobalt Silicon
static constexpr uint16_t COL_JFET_BDG  = 0x04D2; // Teal JFET
static constexpr uint16_t COL_MOS_BDG   = 0x7817; // Purple MOSFET
static constexpr uint16_t COL_PASS_BDG  = 0x0528; // Emerald passive
static constexpr uint16_t COL_DMM_HDR   = 0x036D; // Fluke-style dark teal header
static constexpr uint16_t COL_ERR_BDG   = 0xB800; // Crimson alarm

static const uint16_t CURVE_COLORS[6] = {
  COL_RED, COL_CYAN, COL_WHITE, COL_YELLOW, COL_GREEN, COL_MAGENTA
};

// ======================= ENUMS & DATA STRUCTURES =============================
enum PinModeSS : uint8_t {
  SS_HIZ        = 0xFF, // EN=0, DG412=OFF (<1 nA leakage)
  SS_SRCA_470R  = 0,    // S1: 470R to V_SRC_A
  SS_SRCA_2K49  = 1,    // S2: 2.49k (Keen) to V_SRC_A
  SS_SRCB_100K  = 2,    // S3: 100k to V_SRC_B
  SS_SRCB_2M2   = 3,    // S4: 2.20M (Keen) to V_SRC_B
  SS_GND_470R   = 4,    // S5: 470R to GND
  SS_GND_2K49   = 5,    // S6: 2.49k (Keen) to GND
  SS_GND_MUX    = 6,    // S7: direct mux to GND
  SS_9V_REF     = 7,    // S8: +9V_LIM rail
  SS_GND_HARD   = 0xFE  // EN=0, U8 DG412LDJ INx=ON (~20R to GND)
};

enum CalMuxCh : uint8_t {
  CAL_SRCA   = 0, // A1=0, A0=0 -> S1A = V_SRC_A
  CAL_SRCB   = 1, // A1=0, A0=1 -> S2A = V_SRC_B
  CAL_9VLIM  = 2, // A1=1, A0=0 -> S3A = +9V_LIM
  CAL_DMM30V = 3  // A1=1, A0=1 -> S4A = DMM_30V_DIV (11:1 from J_TERM5 Pin 5)
};

enum ConsoleMode : uint8_t {
  MODE_AUTO_TESTER = 0,
  MODE_CURVE_TRACER,
  MODE_MULTIMETER,
  MODE_THERMAL_DRIFT,
  MODE_COUNT
};

enum DmmSubMode : uint8_t {
  DMM_VOLT_12V = 0,
  DMM_VOLT_30V,
  DMM_OHM_CONT,
  DMM_DIODE,
  DMM_CAP,
  DMM_SUBMODE_COUNT
};

enum DutKind : uint8_t {
  DUT_NONE = 0,
  DUT_ABORT,
  DUT_BJT,
  DUT_JFET,
  DUT_MOSFET,
  DUT_DIODE,
  DUT_RESISTOR,
  DUT_CAPACITOR
};

struct CalData {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t  valid;
  uint8_t  reserved[3];
  float    adcOffsetV[4];
  float    chGainCorr[3];
  float    diffZeroAt0V[3];
  float    diffCmSlope[3];
  float    dacScaleA;
  float    dacScaleB;
  float    v9Lim_V;
  float    rDg408On[3];
  float    rDg412On[3];
  float    cStray_pF[3];
  float    fastAdcSlope[3];
  float    fastAdcOffsetV[3];
  uint32_t crc32;
};

struct PairProbe {
  int   hi;
  int   lo;
  int   third;
  float vHi;
  float vLo;
  float vDut;
  float i_uA;
  bool  conducts;
};

struct BjtResult {
  bool  valid        = false;
  bool  isPNP        = false;
  bool  isGermanium  = false;
  bool  leaky        = false;
  bool  powerClamped = false;
  int   pinE         = 0;
  int   pinB         = 1;
  int   pinC         = 2;
  float hfeTrue      = NAN;
  float hfeApparent  = NAN;
  float iceo_uA      = NAN;
  float icbo_uA      = NAN;
  float vbe_mV       = NAN;
  float vceSat_mV    = NAN;
  float vce_V        = NAN;
  float ic_mA        = NAN;
  float ib_uA        = NAN;
  float peakPower_mW = 0.0f;
  float tempC        = NAN;
};

struct JfetResult {
  bool  valid     = false;
  bool  isP       = false;
  int   pinD      = 0;
  int   pinG      = 1;
  int   pinS      = 2;
  float idss_mA   = NAN;
  float vgsoff_V  = NAN;
  float gm_mS     = NAN;
  float vfGate_mV = NAN;
  float tempC     = NAN;
};

struct MosfetResult {
  bool  valid      = false;
  bool  isP        = false;
  int   pinG       = 0;
  int   pinD       = 1;
  int   pinS       = 2;
  float vth_V      = NAN;
  float rds4v5_ohm = NAN;
  float rds8v5_ohm = NAN;
  float vfBody_mV  = NAN;
  float tempC      = NAN;
};

struct DiodeResult {
  bool  valid     = false;
  bool  isZener   = false;
  bool  isLED     = false;
  bool  isDual    = false;
  int   pinA      = 0;
  int   pinK      = 1;
  float vf0mA1_mV = NAN;
  float vf2mA_mV  = NAN;
  float irev_uA   = NAN;
  float vz_V      = NAN;
  float iz_mA     = NAN;
  float tempC     = NAN;
};

struct ResistorResult {
  bool  valid     = false;
  int   pinA      = 0;
  int   pinB      = 1;
  float r_ohm     = NAN;
  float range_ohm = NAN;
  float vDut_V    = NAN;
  float i_mA      = NAN;
  float tempC     = NAN;
};

struct CapResult {
  bool  valid     = false;
  int   pinA      = 0;
  int   pinB      = 1;
  float c_pF      = NAN;
  float ileak_uA  = NAN;
  float droop_mVs = NAN;
  float tempC     = NAN;
};

static constexpr int MAX_SWEEP_CURVES = 6;
static constexpr int MAX_SWEEP_PTS    = 10;

struct SweepPoint {
  bool  valid = false;
  float ib_uA = 0.0f;
  float vce_V = 0.0f;
  float ic_mA = 0.0f;
  float p_mW  = 0.0f;
};

struct SweepSummary {
  bool       valid        = false;
  bool       isPNP        = false;
  bool       isGermanium  = false;
  bool       powerClamped = false;
  int        pinE         = 0;
  int        pinB         = 1;
  int        pinC         = 2;
  int        curves       = 0;
  int        points       = 0;
  float      bStep_uA     = 4.0f;
  float      vMax_V       = 9.0f;
  float      peakHfe      = NAN;
  float      maxIc_mA     = 0.0f;
  float      maxVce_V     = 0.0f;
  float      maxPower_mW  = 0.0f;
  float      tempC        = NAN;
  int        curvePtCount[MAX_SWEEP_CURVES] = {0};
  SweepPoint pts[MAX_SWEEP_CURVES][MAX_SWEEP_PTS];
};

static constexpr int DRIFT_HIST_LEN = 96;
struct DriftState {
  bool     active       = false;
  bool     isPNP        = false;
  bool     isGermanium  = false;
  int      pinE         = 0;
  int      pinB         = 1;
  int      pinC         = 2;
  uint32_t startMs      = 0;
  uint32_t lastSampleMs = 0;
  uint32_t sampleCount  = 0;
  float    lastHfe      = NAN;
  float    lastIceo_uA  = NAN;
  float    lastIcbo_uA  = NAN;
  float    lastVbe_mV   = NAN;
  float    lastTempC    = NAN;
  float    histHfe[DRIFT_HIST_LEN];
  float    histIceo[DRIFT_HIST_LEN];
  int      histCount    = 0;
  int      histHead     = 0;
};

static constexpr int DMM_HIST_LEN = 120;
struct DmmState {
  DmmSubMode subMode              = DMM_VOLT_12V;
  bool       hold                 = false;
  bool       relActive            = false;
  float      relZero              = 0.0f;
  bool       valid                = false;
  bool       openLoop             = true;
  bool       continuityActive     = false;
  bool       liveVoltWarn         = false;
  bool       isZener              = false;
  bool       prevDiodeConducted   = false;
  float      primaryVal           = 0.0f;
  float      secondaryVal         = NAN;
  float      minVal               = NAN;
  float      maxVal               = NAN;
  double     sumVal               = 0.0;
  uint32_t   sampleCount          = 0;
  uint32_t   lastSampleMs         = 0;
  char       probeStr[16]         = "TP1 - COM";
  char       noteStr[24]          = "HI-Z >10G";
  float      hist[DMM_HIST_LEN]   = {0};
  int        histCount            = 0;
  int        histHead             = 0;
};

struct LastScanState {
  DutKind        kind = DUT_NONE;
  char           abortReason[32] = {0};
  float          abortV = 0.0f;
  char           dutHash[24] = {0};
  BjtResult      bjt;
  JfetResult     jfet;
  MosfetResult   mos;
  DiodeResult    diode;
  ResistorResult res;
  CapResult      cap;
};

static constexpr int HISTORY_CAP = 32;
struct HistoryEntry {
  uint32_t seq;
  uint32_t uptime_s;
  char     hash[24];
  char     type[16];
  char     summary[64];
  char     json[360];
};

struct HistoryNvsSlot {
  uint32_t seq;
  uint32_t uptime_s;
  char     hash[24];
  char     type[16];
  char     summary[64];
};

static constexpr uint32_t HIST_NVS_MAGIC = 0x48495334UL; // "HIS4"
struct HistoryNvsBlob {
  uint32_t       magic;
  int32_t        count;
  int32_t        head;
  uint32_t       seq;
  HistoryNvsSlot slots[HISTORY_CAP];
};

struct WifiSyncConfig {
  char     ssid[33];
  char     pass[65];
  char     serverHost[64];
  uint16_t serverPort;
  uint8_t  autoPost;
};

struct ToneStep {
  uint16_t freqHz;
  uint16_t durMs;
};

struct McpDebouncedBtn {
  bool     lastRaw;
  bool     stable;
  uint32_t lastEdgeMs;
  uint32_t pressedMs;
  bool     longFired;
};

// ======================= FORWARD SPI GUARD HELPER ============================
static inline void deselectAllSpiSlaves() {
  digitalWrite(PIN_TFT1_CS, HIGH);
  digitalWrite(PIN_TFT2_CS, HIGH);
  digitalWrite(PIN_DAC_CS,  HIGH);
  digitalWrite(PIN_SR_LATCH, LOW);
}

// ======================= SELF-CONTAINED ST7789V2 DRIVER ======================
class DualST7789 : public Adafruit_GFX {
public:
  DualST7789(int8_t csPin, int8_t dcPin)
      : Adafruit_GFX(170, 320), _cs(csPin), _dc(dcPin) {}

  void begin(uint8_t rot = 1) {
    digitalWrite(_cs, HIGH);
    pinMode(_cs, OUTPUT);
    digitalWrite(_dc, HIGH);
    pinMode(_dc, OUTPUT);

    writeCmd(0x01); // SWRESET
    delay(150);
    writeCmd(0x11); // SLPOUT
    delay(120);

    static const uint8_t colmodArg = 0x55; // 16-bit RGB565
    writeCmdData(0x3A, &colmodArg, 1);
    delay(10);

    static const uint8_t madArg = 0x00;
    writeCmdData(0x36, &madArg, 1);

    writeCmd(0x21); // INVON (IPS panels)
    delay(10);
    writeCmd(0x13); // NORON
    delay(10);
    writeCmd(0x29); // DISPON
    delay(40);

    setRotation(rot);
    setTextWrap(false);
  }

  void setRotation(uint8_t r) override {
    rotation = r & 3;
    uint8_t madctl = 0x00;
    switch (rotation) {
      case 0:
        madctl = 0x00; _width = 170; _height = 320; _xstart = 35; _ystart = 0; break;
      case 1:
        madctl = 0x60; _width = 320; _height = 170; _xstart = 0;  _ystart = 35; break;
      case 2:
        madctl = 0xC0; _width = 170; _height = 320; _xstart = 35; _ystart = 0; break;
      case 3:
        madctl = 0xA0; _width = 320; _height = 170; _xstart = 0;  _ystart = 35; break;
    }
    writeCmdData(0x36, &madctl, 1);
  }

  void invertDisplay(bool i) override {
    writeCmd(i ? 0x20 : 0x21);
  }

  void startWrite() override {
    if (_txDepth == 0) {
      deselectAllSpiSlaves();
      SPI.beginTransaction(SPISettings(SPI_FREQ_TFT, MSBFIRST, SPI_MODE0));
      digitalWrite(_cs, LOW);
    }
    _txDepth++;
  }

  void endWrite() override {
    if (_txDepth > 0) {
      _txDepth--;
      if (_txDepth == 0) {
        digitalWrite(_cs, HIGH);
        SPI.endTransaction();
      }
    }
  }

  void writePixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    setAddrWindow((uint16_t)x, (uint16_t)y, 1, 1);
    uint8_t px[2] = {(uint8_t)(color >> 8), (uint8_t)(color & 0xFF)};
    SPI.writeBytes(px, 2);
  }

  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= _width || y >= _height || w <= 0 || h <= 0) return;
    if (x + w > _width)  w = _width - x;
    if (y + h > _height) h = _height - y;
    setAddrWindow((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
    pushColorBlock(color, (uint32_t)w * (uint32_t)h);
  }

  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    writeFillRect(x, y, w, 1, color);
  }

  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    writeFillRect(x, y, 1, h, color);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    startWrite(); writePixel(x, y, color); endWrite();
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    startWrite(); writeFastHLine(x, y, w, color); endWrite();
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    startWrite(); writeFastVLine(x, y, h, color); endWrite();
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    startWrite(); writeFillRect(x, y, w, h, color); endWrite();
  }

  void fillScreen(uint16_t color) override {
    fillRect(0, 0, _width, _height, color);
  }

private:
  int8_t   _cs;
  int8_t   _dc;
  uint16_t _xstart  = 0;
  uint16_t _ystart  = 35;
  uint16_t _txDepth = 0;

  void writeCmd(uint8_t cmd) {
    startWrite();
    digitalWrite(_dc, LOW);
    SPI.write(cmd);
    digitalWrite(_dc, HIGH);
    endWrite();
  }

  void writeCmdData(uint8_t cmd, const uint8_t *data, size_t len) {
    startWrite();
    digitalWrite(_dc, LOW);
    SPI.write(cmd);
    digitalWrite(_dc, HIGH);
    if (len > 0 && data != nullptr) {
      SPI.writeBytes(data, (uint32_t)len);
    }
    endWrite();
  }

  void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x0 = x + _xstart;
    uint16_t x1 = x0 + w - 1;
    uint16_t y0 = y + _ystart;
    uint16_t y1 = y0 + h - 1;
    uint8_t buf[4];

    digitalWrite(_dc, LOW);
    SPI.write(0x2A);
    digitalWrite(_dc, HIGH);
    buf[0] = (uint8_t)(x0 >> 8); buf[1] = (uint8_t)(x0 & 0xFF);
    buf[2] = (uint8_t)(x1 >> 8); buf[3] = (uint8_t)(x1 & 0xFF);
    SPI.writeBytes(buf, 4);

    digitalWrite(_dc, LOW);
    SPI.write(0x2B);
    digitalWrite(_dc, HIGH);
    buf[0] = (uint8_t)(y0 >> 8); buf[1] = (uint8_t)(y0 & 0xFF);
    buf[2] = (uint8_t)(y1 >> 8); buf[3] = (uint8_t)(y1 & 0xFF);
    SPI.writeBytes(buf, 4);

    digitalWrite(_dc, LOW);
    SPI.write(0x2C);
    digitalWrite(_dc, HIGH);
  }

  static void pushColorBlock(uint16_t color, uint32_t count) {
    static uint8_t chunk[256];
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint32_t fillN = (count < 128U) ? count : 128U;
    for (uint32_t i = 0; i < fillN; i++) {
      chunk[i * 2]     = hi;
      chunk[i * 2 + 1] = lo;
    }
    while (count > 0) {
      uint32_t n = (count > 128U) ? 128U : count;
      SPI.writeBytes(chunk, n * 2U);
      count -= n;
    }
  }
};

// ======================= GLOBAL INSTANCES & STATE ============================
Adafruit_ADS1115  ads;
DualST7789        tft1(PIN_TFT1_CS, PIN_TFT_DC); // Top: Curve Tracer / Scope / History
DualST7789        tft2(PIN_TFT2_CS, PIN_TFT_DC); // Bot: DMM / Parametric / Standby Status
OneWire           oneWire(PIN_TEMP_DQ);
DallasTemperature tempSensor(&oneWire);
Preferences       prefs;

#if ENABLE_WIFI_SYNC
WebServer         httpServer(80);
bool              httpServerStarted = false;
#endif

#if ENABLE_BLE_TELEMETRY
static constexpr char BLE_SVC_UUID[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr char BLE_RX_UUID[]  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr char BLE_TX_UUID[]  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
BLEServer         *bleServer  = nullptr;
BLECharacteristic *bleTxChar  = nullptr;
bool              bleActive   = false;
bool              bleClientConnected = false;
#endif

CalData        cal;
LastScanState  lastScan;
SweepSummary   lastSweep;
DriftState     drift;
DmmState       dmm;
WifiSyncConfig wifiCfg;
ConsoleMode    consoleMode = MODE_AUTO_TESTER;

HistoryEntry   historyBuf[HISTORY_CAP];
int            historyCount = 0;
int            historyHead  = 0;
uint32_t       historySeq   = 0;

// Interactive Curve Tracer knob settings
static const float B_STEP_UA_LIST[6]    = {2.0f, 4.0f, 10.0f, 25.0f, 50.0f, 100.0f};
static const char *B_STEP_LABELS[6]     = {"2uA", "4uA[Keen]", "10uA", "25uA", "50uA", "100uA"};
static const float VCE_MAX_LIST[4]      = {2.0f, 4.5f, 6.0f, 9.0f};
static const char *VCE_MAX_LABELS[4]    = {"2.0V", "4.5V[RF]", "6.0V", "9.0V"};
static const char *DMM_MODE_LABELS[DMM_SUBMODE_COUNT] = {
  "12V DC", "30V HV", "OHM/CONT", "DIODE", "CAP"
};

uint8_t bRangeIdx   = 1; // Default: 4 uA [Keen]
uint8_t vceRangeIdx = 3; // Default: 9.0 V
int8_t  cursorCurve = -1;
int8_t  cursorPt    = -1;

bool     wireInitialized  = false;
bool     adsOK            = false;
bool     u11OK            = false;
bool     u12OK            = false;
bool     tftsOK           = false;
bool     tempPresent      = false;
bool     tempValid        = false;
float    latestTempC      = NAN;
uint32_t lastTempReqMs    = 0;

uint16_t currentMatrixWord = 0x0000;
PinModeSS currentChan[3]   = {SS_HIZ, SS_HIZ, SS_HIZ};
int      lastDacCode[2]    = {-1, -1};
float    currentDacV[2]    = {0.0f, 0.0f};
CalMuxCh currentCalMux     = CAL_SRCA;
uint32_t interlockFaults   = 0;
bool     powerClampedFlag  = false;

// U11 & U12 shadow latches
uint8_t  u11OlatA          = 0x00;
uint8_t  u11OlatB          = 0x00;
uint8_t  u12OlatA          = (1u << U12_GPA_TFT_RES) | (1u << U12_GPA_LED_PWR);
uint8_t  u12OlatB          = 0x00;

// Standby, Radio, Audio Sequencer & Debug flags
bool     inStandby         = false;
bool     softwareStandby   = false;
bool     wifiRadioActive   = false;
bool     analogRailOn      = true;
bool     verboseDebug      = false;
char     activeIpStr[24]   = "OFF";

static constexpr int TONE_QUEUE_MAX = 32;
ToneStep toneQueue[TONE_QUEUE_MAX];
int      toneHead          = 0;
int      toneTail          = 0;
bool     tonePlaying       = false;
uint32_t toneStepEndMs     = 0;
uint16_t currentContFreq   = 0;

volatile bool adsConvDone  = false;
volatile bool mcpIntFlag   = false;
volatile bool abortRequested = false;
bool     isMeasurementBusy = false;

// ======================= FORWARD DECLARATIONS ================================
bool setMatrix(PinModeSS ch1, PinModeSS ch2, PinModeSS ch3,
               bool activeLed = true, bool calibratedLoadMode = false);
bool setMatrixRaw16(uint16_t word, bool calibratedLoadMode = false);
void selectCalMux(CalMuxCh ch);
float readNodeV(uint8_t ch, int samples = 4);
float readLiveRefV(CalMuxCh muxCh, int samples = 4);
float readDiffSource(uint8_t dutCh, CalMuxCh srcMux, float vSrcActual, int samples = 4);
void renderScreens();
void renderTFT1();
void renderTFT2();
void runFullAutoDetect();
bool runOpenSocketCal();
void runPulsedGeSweep(bool runDetectFirst = true);
void startThermalDrift();
void stopThermalDrift();
void pollMcpInputs();
void setAnalogRailEnabled(bool en);
void setStatusLeds(bool chg, bool wifi, bool scan, bool pass, bool warn);
void updateLedStatusFromState();
void stopBuzzerAll();
void playBootChime();
void playModemScanStep(uint8_t step);
void playModemHandshakeDemo();
void updateToneSequencer();
void setStandbyMode(bool enter);
void ensureRadioOffForAnalog();
void runHardwareSelfTest();

// ======================= HARDWARE PWM PIEZO & 56K MODEM SYNTH ================
static void initBuzzerPwm() {
  ledcAttach(PIN_BZ_PWM, 2000, 8);
  ledcWriteTone(PIN_BZ_PWM, 0);
}

static void queueTone(uint16_t freqHz, uint16_t durMs) {
  int nextTail = (toneTail + 1) % TONE_QUEUE_MAX;
  if (nextTail == toneHead) return; // Queue full
  toneQueue[toneTail].freqHz = freqHz;
  toneQueue[toneTail].durMs  = durMs;
  toneTail = nextTail;
}

static void clearToneQueue() {
  toneHead = toneTail = 0;
  tonePlaying = false;
}

void updateToneSequencer() {
  if (consoleMode == MODE_MULTIMETER && dmm.subMode == DMM_OHM_CONT && dmm.continuityActive && !inStandby) {
    if (currentContFreq != DMM_CONT_FREQ_HZ) {
      clearToneQueue();
      ledcWriteTone(PIN_BZ_PWM, DMM_CONT_FREQ_HZ);
      currentContFreq = DMM_CONT_FREQ_HZ;
    }
    return;
  } else if (currentContFreq != 0) {
    ledcWriteTone(PIN_BZ_PWM, 0);
    currentContFreq = 0;
  }

  uint32_t now = millis();
  if (tonePlaying) {
    if ((int32_t)(now - toneStepEndMs) < 0) return;
    tonePlaying = false;
  }

  if (toneHead != toneTail) {
    ToneStep st = toneQueue[toneHead];
    toneHead = (toneHead + 1) % TONE_QUEUE_MAX;
    ledcWriteTone(PIN_BZ_PWM, st.freqHz);
    tonePlaying = true;
    toneStepEndMs = now + st.durMs;
  } else {
    ledcWriteTone(PIN_BZ_PWM, 0);
  }
}

void stopBuzzerAll() {
  clearToneQueue();
  currentContFreq = 0;
  ledcWriteTone(PIN_BZ_PWM, 0);
}

static void beepTone(uint16_t freqHz, uint16_t durMs) {
  clearToneQueue();
  queueTone(freqHz, durMs);
  updateToneSequencer();
}

void playBootChime() {
  clearToneQueue();
  queueTone(523, 55);  // C5
  queueTone(659, 55);  // E5
  queueTone(784, 55);  // G5
  queueTone(1047, 70); // C6
  queueTone(1319, 95); // E6
  updateToneSequencer();
}

void playModemScanStep(uint8_t step) {
  clearToneQueue();
  switch (step) {
    case 0: // Scan Start + V.21 FSK carrier preamble (1650/1850 Hz)
      queueTone(880,  30);
      queueTone(1320, 35);
      queueTone(1650, 28);
      queueTone(1850, 28);
      queueTone(1650, 28);
      queueTone(1850, 28);
      break;
    case 1: // 2100 Hz CED answer tone w/ phase-reversal notch + V.21 ch1
      queueTone(2100, 65);
      queueTone(0,     6); // Phase reversal audible click
      queueTone(2100, 65);
      queueTone(980,  28);
      queueTone(1180, 28);
      break;
    case 2: // V.32 / V.34 training warble & FSK data burst
      queueTone(1200, 22);
      queueTone(2400, 22);
      queueTone(1600, 22);
      queueTone(2700, 22);
      queueTone(1400, 22);
      queueTone(2100, 24);
      queueTone(1800, 24);
      break;
    case 3: // Pass completion chime
      queueTone(1568, 45);
      queueTone(2093, 85);
      break;
    default: // Abort / Fail dual low buzz
      queueTone(330, 80);
      queueTone(220, 120);
      break;
  }
  updateToneSequencer();
}

void playModemHandshakeDemo() {
  emitStatusJson("INFO", "Playing 56k V.90 Dial-Up Modem Handshake Synth");
  clearToneQueue();
  // 1. Dial tone + DTMF chirp
  queueTone(350,  110);
  queueTone(440,  110);
  queueTone(1209,  45);
  queueTone(1477,  45);
  queueTone(0,     40);
  // 2. V.21 300bps FSK preamble (1650 / 1850 Hz)
  for (int i = 0; i < 4; i++) {
    queueTone(1650, 26);
    queueTone(1850, 26);
  }
  // 3. 2100 Hz CED with phase reversals
  for (int i = 0; i < 3; i++) {
    queueTone(2100, 95);
    queueTone(0,     6);
  }
  // 4. V.8bis / V.34 probing & equalizer training warble
  static const uint16_t warble[14] = {
    980, 1180, 1650, 2400, 1350, 2700, 1800, 1200, 2600, 1500, 2250, 1950, 2850, 1750
  };
  for (int i = 0; i < 14; i++) {
    queueTone(warble[i], 28);
  }
  queueTone(2093, 75);
  updateToneSequencer();
}

static void drainAudioShort(uint32_t maxMs) {
  uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < maxMs && (tonePlaying || toneHead != toneTail)) {
    updateToneSequencer();
    delay(2);
  }
}

// ======================= STRICT RFC-8259 JSON HELPERS ========================
static void fmtJsonFloat(char *buf, size_t len, float v, int decimals) {
  if (isnan(v) || isinf(v)) {
    snprintf(buf, len, "null");
  } else {
    snprintf(buf, len, "%.*f", decimals, v);
  }
}

static void emitStatusJson(const char *level, const char *msg) {
  Serial.printf("{\"type\":\"STATUS\",\"level\":\"%s\",\"msg\":\"%s\"}\n", level, msg);
}

static void emitAbortJson(const char *reason, float vMax) {
  char vStr[20];
  fmtJsonFloat(vStr, sizeof(vStr), vMax, 3);
  Serial.printf("{\"type\":\"ABORT\",\"reason\":\"%s\",\"v_max_V\":%s}\n", reason, vStr);
}

// ======================= DETERMINISTIC DUT CHARACTERIZATION HASH =============
static uint16_t computeCrc16Ccitt(const char *s) {
  uint16_t crc = 0xFFFFU;
  while (*s != '\0') {
    crc ^= (uint16_t)(uint8_t)(*s++) << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static void computeDutCharHash(LastScanState &scan) {
  char prefix[4] = "UNK";
  int  b1 = 0;
  int  b2 = 0;
  char canon[128] = {0};

  if (scan.kind == DUT_BJT && scan.bjt.valid) {
    const BjtResult &b = scan.bjt;
    snprintf(prefix, sizeof(prefix), "%s%s", b.isGermanium ? "GE" : "SI", b.isPNP ? "P" : "N");
    b1 = (int)constrain(lroundf(b.hfeTrue), 0L, 999L);
    b2 = (int)constrain(lroundf(b.iceo_uA), 0L, 999L);
    char roles[4] = "???";
    roles[b.pinE] = 'E'; roles[b.pinB] = 'B'; roles[b.pinC] = 'C';
    long hq = lroundf(b.hfeTrue);
    long iq = lroundf(b.iceo_uA);
    long cq = lroundf(b.icbo_uA);
    long vq = lroundf(b.vbe_mV / 5.0f) * 5L;
    long sq = lroundf(b.vceSat_mV / 5.0f) * 5L;
    snprintf(canon, sizeof(canon), "BJT|%s|%s|%ld|%ld|%ld|%ld|%ld",
             prefix, roles, hq, iq, cq, vq, sq);
  } else if (scan.kind == DUT_JFET && scan.jfet.valid) {
    const JfetResult &j = scan.jfet;
    snprintf(prefix, sizeof(prefix), "%s", j.isP ? "PJF" : "NJF");
    b1 = (int)constrain(lroundf(j.idss_mA * 10.0f), 0L, 999L);
    b2 = (int)constrain(lroundf(fabsf(j.vgsoff_V) * 100.0f), 0L, 999L);
    char roles[4] = "???";
    roles[j.pinD] = 'D'; roles[j.pinG] = 'G'; roles[j.pinS] = 'S';
    snprintf(canon, sizeof(canon), "JFET|%s|%s|%d|%d|%ld",
             prefix, roles, b1, b2, lroundf(j.vfGate_mV / 10.0f));
  } else if (scan.kind == DUT_MOSFET && scan.mos.valid) {
    const MosfetResult &m = scan.mos;
    snprintf(prefix, sizeof(prefix), "%s", m.isP ? "PMO" : "NMO");
    b1 = (int)constrain(lroundf(fabsf(m.vth_V) * 100.0f), 0L, 999L);
    b2 = isnan(m.rds4v5_ohm) ? 999 : (int)constrain(lroundf(m.rds4v5_ohm * 10.0f), 0L, 999L);
    char roles[4] = "???";
    roles[m.pinG] = 'G'; roles[m.pinD] = 'D'; roles[m.pinS] = 'S';
    snprintf(canon, sizeof(canon), "MOS|%s|%s|%d|%d|%ld",
             prefix, roles, b1, b2, lroundf(m.vfBody_mV / 10.0f));
  } else if (scan.kind == DUT_DIODE && scan.diode.valid) {
    const DiodeResult &d = scan.diode;
    snprintf(prefix, sizeof(prefix), "%s", d.isZener ? "ZEN" : "DIO");
    b1 = d.isZener ? (int)constrain(lroundf(d.vz_V * 100.0f), 0L, 999L)
                   : (int)constrain(lroundf(d.vf2mA_mV), 0L, 999L);
    b2 = (int)constrain(lroundf(d.irev_uA), 0L, 999L);
    snprintf(canon, sizeof(canon), "DIO|%s|%d->%d|%d|%d|%d%d",
             prefix, d.pinA, d.pinK, b1, b2, d.isLED ? 1 : 0, d.isDual ? 1 : 0);
  } else if (scan.kind == DUT_RESISTOR && scan.res.valid) {
    const ResistorResult &r = scan.res;
    snprintf(prefix, sizeof(prefix), "RES");
    float rv = fmaxf(0.1f, r.r_ohm);
    int dec = (int)floorf(log10f(rv));
    if (dec < 0) dec = 0;
    float norm = rv / powf(10.0f, (float)fmax(0, dec - 2));
    b1 = (int)constrain(lroundf(norm), 0L, 999L);
    b2 = constrain(dec, 0, 999);
    snprintf(canon, sizeof(canon), "RES|%d-%d|%d|%d", r.pinA, r.pinB, b1, b2);
  } else if (scan.kind == DUT_CAPACITOR && scan.cap.valid) {
    const CapResult &c = scan.cap;
    snprintf(prefix, sizeof(prefix), "CAP");
    float cp = fmaxf(1.0f, c.c_pF);
    int dec = (int)floorf(log10f(cp));
    if (dec < 0) dec = 0;
    float norm = cp / powf(10.0f, (float)fmax(0, dec - 1));
    int sig2 = (int)constrain(lroundf(norm), 10L, 99L);
    b1 = sig2 * 10 + constrain(fmax(0, dec - 1), 0, 9);
    b2 = (int)constrain(lroundf(c.ileak_uA * 10.0f), 0L, 999L);
    snprintf(canon, sizeof(canon), "CAP|%d-%d|%d|%d", c.pinA, c.pinB, b1, b2);
  } else {
    scan.dutHash[0] = '\0';
    return;
  }

  uint16_t crc = computeCrc16Ccitt(canon);
  snprintf(scan.dutHash, sizeof(scan.dutHash), "%s-%03d-L%03d-%04X",
           prefix, b1, b2, (unsigned int)crc);
}

// ======================= CRC32 & NVS CALIBRATION + HISTORY + WIFI ==============
static uint32_t computeCalCrc32(const CalData &c) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&c);
  size_t len = offsetof(CalData, crc32);
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
  }
  return ~crc;
}

static void initCalDefaults(CalData &c) {
  memset(&c, 0, sizeof(c));
  c.magic   = CAL_MAGIC;
  c.version = CAL_VERSION;
  c.size    = sizeof(CalData);
  c.valid   = 0;
  for (int i = 0; i < 4; i++) c.adcOffsetV[i] = 0.0f;
  for (int i = 0; i < 3; i++) {
    c.chGainCorr[i]     = 1.0f;
    c.diffZeroAt0V[i]   = 0.0f;
    c.diffCmSlope[i]    = 0.0f;
    c.rDg408On[i]       = R_DG408_NOM;
    c.rDg412On[i]       = R_DG412_NOM;
    c.cStray_pF[i]      = 0.0f;
    c.fastAdcSlope[i]   = 1.0f;
    c.fastAdcOffsetV[i] = 0.0f;
  }
  c.dacScaleA = 1.0f;
  c.dacScaleB = 1.0f;
  c.v9Lim_V   = 9.000f;
  c.crc32     = computeCalCrc32(c);
}

static bool saveCalNVS(CalData &c) {
  c.magic   = CAL_MAGIC;
  c.version = CAL_VERSION;
  c.size    = sizeof(CalData);
  c.crc32   = computeCalCrc32(c);
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  size_t written = prefs.putBytes(NVS_KEY_CAL, &c, sizeof(CalData));
  prefs.end();
  return (written == sizeof(CalData));
}

static bool loadCalNVS(CalData &c) {
  initCalDefaults(c);
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  size_t len = prefs.getBytesLength(NVS_KEY_CAL);
  if (len != sizeof(CalData)) {
    prefs.end();
    return false;
  }
  CalData tmp;
  size_t got = prefs.getBytes(NVS_KEY_CAL, &tmp, sizeof(CalData));
  prefs.end();
  if (got != sizeof(CalData)) return false;
  if (tmp.magic != CAL_MAGIC || tmp.version != CAL_VERSION || tmp.size != sizeof(CalData)) {
    return false;
  }
  if (tmp.crc32 != computeCalCrc32(tmp)) return false;
  c = tmp;
  return (c.valid != 0);
}

static HistoryNvsBlob histBlobTmp;

static void saveHistoryNVS() {
  histBlobTmp.magic = HIST_NVS_MAGIC;
  histBlobTmp.count = historyCount;
  histBlobTmp.head  = historyHead;
  histBlobTmp.seq   = historySeq;
  for (int i = 0; i < HISTORY_CAP; i++) {
    histBlobTmp.slots[i].seq      = historyBuf[i].seq;
    histBlobTmp.slots[i].uptime_s = historyBuf[i].uptime_s;
    memcpy(histBlobTmp.slots[i].hash,    historyBuf[i].hash,    sizeof(histBlobTmp.slots[i].hash));
    memcpy(histBlobTmp.slots[i].type,    historyBuf[i].type,    sizeof(histBlobTmp.slots[i].type));
    memcpy(histBlobTmp.slots[i].summary, historyBuf[i].summary, sizeof(histBlobTmp.slots[i].summary));
  }
  if (!prefs.begin(NVS_NS_HIST, false)) return;
  prefs.putBytes("hist4", &histBlobTmp, sizeof(histBlobTmp));
  prefs.end();
}

static void loadHistoryNVS() {
  memset(historyBuf, 0, sizeof(historyBuf));
  historyCount = 0;
  historyHead  = 0;
  historySeq   = 0;
  if (!prefs.begin(NVS_NS_HIST, true)) return;
  if (prefs.getBytesLength("hist4") == sizeof(histBlobTmp)) {
    prefs.getBytes("hist4", &histBlobTmp, sizeof(histBlobTmp));
    if (histBlobTmp.magic == HIST_NVS_MAGIC) {
      historyCount = constrain((int)histBlobTmp.count, 0, HISTORY_CAP);
      historyHead  = constrain((int)histBlobTmp.head, 0, HISTORY_CAP - 1);
      historySeq   = histBlobTmp.seq;
      for (int i = 0; i < HISTORY_CAP; i++) {
        HistoryEntry &e = historyBuf[i];
        const HistoryNvsSlot &s = histBlobTmp.slots[i];
        e.seq      = s.seq;
        e.uptime_s = s.uptime_s;
        memcpy(e.hash,    s.hash,    sizeof(e.hash));
        memcpy(e.type,    s.type,    sizeof(e.type));
        memcpy(e.summary, s.summary, sizeof(e.summary));
        e.hash[sizeof(e.hash) - 1]       = '\0';
        e.type[sizeof(e.type) - 1]       = '\0';
        e.summary[sizeof(e.summary) - 1] = '\0';
        if (e.hash[0] != '\0') {
          snprintf(e.json, sizeof(e.json),
                   "{\"type\":\"%s\",\"hash\":\"%s\",\"summary\":\"%s\","
                   "\"seq\":%lu,\"uptime_s\":%lu,\"restored\":true}",
                   e.type, e.hash, e.summary,
                   (unsigned long)e.seq, (unsigned long)e.uptime_s);
        }
      }
    }
  }
  prefs.end();
}

static void loadWifiConfigNVS() {
  memset(&wifiCfg, 0, sizeof(wifiCfg));
  wifiCfg.serverPort = 8080;
  wifiCfg.autoPost   = 0;
  if (!prefs.begin(NVS_NS_WIFI, true)) return;
  if (prefs.getBytesLength("cfg") == sizeof(wifiCfg)) {
    prefs.getBytes("cfg", &wifiCfg, sizeof(wifiCfg));
  }
  prefs.end();
  wifiCfg.ssid[32] = '\0';
  wifiCfg.pass[64] = '\0';
  wifiCfg.serverHost[63] = '\0';
  if (wifiCfg.serverPort == 0) wifiCfg.serverPort = 8080;
}

static void saveWifiConfigNVS() {
  if (!prefs.begin(NVS_NS_WIFI, false)) return;
  prefs.putBytes("cfg", &wifiCfg, sizeof(wifiCfg));
  prefs.end();
}

static void pushHistoryEntry(const char *hash, const char *typeStr,
                             const char *summaryStr, const char *jsonStr) {
  HistoryEntry &e = historyBuf[historyHead];
  e.seq      = ++historySeq;
  e.uptime_s = millis() / 1000UL;
  snprintf(e.hash, sizeof(e.hash), "%s", hash);
  snprintf(e.type, sizeof(e.type), "%s", typeStr);
  snprintf(e.summary, sizeof(e.summary), "%s", summaryStr);
  snprintf(e.json, sizeof(e.json), "%s", jsonStr);

  historyHead = (historyHead + 1) % HISTORY_CAP;
  if (historyCount < HISTORY_CAP) historyCount++;
  saveHistoryNVS();

#if ENABLE_BLE_TELEMETRY
  if (bleActive && bleTxChar != nullptr && bleClientConnected) {
    // Send in <= 180-byte chunks if needed or notify JSON
    size_t len = strlen(e.json);
    size_t off = 0;
    while (off < len) {
      size_t n = (len - off > 180U) ? 180U : (len - off);
      bleTxChar->setValue((uint8_t *)(e.json + off), n);
      bleTxChar->notify();
      off += n;
      delay(8);
    }
    const char nl = '\n';
    bleTxChar->setValue((uint8_t *)&nl, 1);
    bleTxChar->notify();
  }
#endif
}

// ======================= I2C 9-CLOCK RECOVERY & DUAL MCP23017 ================
static constexpr uint8_t MCP_REG_IODIRA   = 0x00;
static constexpr uint8_t MCP_REG_IODIRB   = 0x01;
static constexpr uint8_t MCP_REG_GPINTENA = 0x04;
static constexpr uint8_t MCP_REG_GPINTENB = 0x05;
static constexpr uint8_t MCP_REG_INTCONA  = 0x08;
static constexpr uint8_t MCP_REG_INTCONB  = 0x09;
static constexpr uint8_t MCP_REG_IOCON    = 0x0A;
static constexpr uint8_t MCP_REG_GPPUA    = 0x0C;
static constexpr uint8_t MCP_REG_GPPUB    = 0x0D;
static constexpr uint8_t MCP_REG_GPIOA    = 0x12;
static constexpr uint8_t MCP_REG_GPIOB    = 0x13;
static constexpr uint8_t MCP_REG_OLATA    = 0x14;
static constexpr uint8_t MCP_REG_OLATB    = 0x15;

static bool probeI2CDevice(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission(true) == 0);
}

static void recoverI2CBus() {
  if (wireInitialized) {
    Wire.end();
    wireInitialized = false;
  }
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  pinMode(PIN_I2C_SCL, INPUT_PULLUP);
  delayMicroseconds(10);

  for (int i = 0; i < 9; i++) {
    if (digitalRead(PIN_I2C_SDA) == HIGH) break;
    pinMode(PIN_I2C_SCL, OUTPUT);
    digitalWrite(PIN_I2C_SCL, LOW);
    delayMicroseconds(5);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    delayMicroseconds(5);
  }

  pinMode(PIN_I2C_SDA, OUTPUT);
  digitalWrite(PIN_I2C_SDA, LOW);
  delayMicroseconds(5);
  pinMode(PIN_I2C_SCL, INPUT_PULLUP);
  delayMicroseconds(5);
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  delayMicroseconds(5);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  wireInitialized = true;
}

static bool mcpWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission(true) == 0);
}

static bool mcpReadReg(uint8_t addr, uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

static bool mcpReadPorts(uint8_t addr, uint8_t &gpioA, uint8_t &gpioB) {
  Wire.beginTransmission(addr);
  Wire.write(MCP_REG_GPIOA);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)2) != 2) return false;
  gpioA = Wire.read();
  gpioB = Wire.read();
  return true;
}

static void IRAM_ATTR onMcpIntISR() {
  mcpIntFlag = true;
}

static bool initU11Controls() {
  if (!probeI2CDevice(I2C_ADDR_U11)) {
    u11OK = false;
    return false;
  }
  u11OlatA = 0x00; // TFT_BLK OFF during TFT reset/init
  u11OlatB = 0x00; // CAL_A0/A1 = 0 (V_SRC_A)
  bool ok = true;
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_OLATA,    u11OlatA);
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_OLATB,    u11OlatB);
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_IODIRA,   0x7F); // GPA0..6 inputs, GPA7 output (TFT_BLK)
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_IODIRB,   0x3F); // GPB0..5 inputs, GPB6..7 outputs (CAL_A0/1)
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_GPPUA,    0x7F); // Pull-ups on D-Pad, A/B, SW_STBY
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_GPPUB,    0x3F); // Pull-ups on ENC_B & ENC_C
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_IOCON,    0x44); // MIRROR=1 | ODR=1 (open-drain INT!)
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_INTCONA,  0x00);
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_INTCONB,  0x00);
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_GPINTENA, 0x7F);
  ok &= mcpWriteReg(I2C_ADDR_U11, MCP_REG_GPINTENB, 0x3F);
  uint8_t a = 0xFF, b = 0xFF;
  mcpReadPorts(I2C_ADDR_U11, a, b);
  u11OK = ok;
  return ok;
}

static bool initU12StatusAndPower(bool bootInStandby) {
  if (!probeI2CDevice(I2C_ADDR_U12)) {
    u12OK = false;
    return false;
  }
  u12OlatA = (uint8_t)((1u << U12_GPA_TFT_RES) | (1u << U12_GPA_LED_PWR));
  if (bootInStandby) {
    u12OlatA |= (uint8_t)((1u << U12_GPA_ANA_EN_N) | (1u << U12_GPA_LED_CHG) | (1u << U12_GPA_LED_WIFI));
  }
  u12OlatB = 0x00;
  bool ok = true;
  ok &= mcpWriteReg(I2C_ADDR_U12, MCP_REG_IOCON,    0x44); // MIRROR=1 | ODR=1
  ok &= mcpWriteReg(I2C_ADDR_U12, MCP_REG_GPINTENA, 0x00);
  ok &= mcpWriteReg(I2C_ADDR_U12, MCP_REG_GPINTENB, 0x00);
  ok &= mcpWriteReg(I2C_ADDR_U12, MCP_REG_OLATA,    u12OlatA);
  ok &= mcpWriteReg(I2C_ADDR_U12, MCP_REG_OLATB,    u12OlatB);
  ok &= mcpWriteReg(I2C_ADDR_U12, MCP_REG_IODIRA,   0x00); // All outputs
  ok &= mcpWriteReg(I2C_ADDR_U12, MCP_REG_IODIRB,   0x00);
  u12OK = ok;
  analogRailOn = !bootInStandby;
  return ok;
}

static void setBacklight(bool on) {
  if (!u11OK) return;
  if (on) u11OlatA |=  (uint8_t)(1u << U11_GPA_TFT_BLK);
  else    u11OlatA &= (uint8_t)~(1u << U11_GPA_TFT_BLK);
  mcpWriteReg(I2C_ADDR_U11, MCP_REG_OLATA, u11OlatA);
}

static void pulseTftReset() {
  if (!u12OK) return;
  u12OlatA &= (uint8_t)~(1u << U12_GPA_TFT_RES);
  mcpWriteReg(I2C_ADDR_U12, MCP_REG_OLATA, u12OlatA);
  delay(20);
  u12OlatA |= (uint8_t)(1u << U12_GPA_TFT_RES);
  mcpWriteReg(I2C_ADDR_U12, MCP_REG_OLATA, u12OlatA);
  delay(120);
}

void setAnalogRailEnabled(bool en) {
  analogRailOn = en;
  if (!u12OK) return;
  if (en) u12OlatA &= (uint8_t)~(1u << U12_GPA_ANA_EN_N); // LOW = MT3608 +12V/+9V ON
  else    u12OlatA |=  (uint8_t)(1u << U12_GPA_ANA_EN_N);  // HIGH = Analog rails OFF
  mcpWriteReg(I2C_ADDR_U12, MCP_REG_OLATA, u12OlatA);
}

void setStatusLeds(bool chg, bool wifi, bool scan, bool pass, bool warn) {
  if (!u12OK) return;
  uint8_t next = (uint8_t)(u12OlatA & ((1u << U12_GPA_TFT_RES) | (1u << U12_GPA_ANA_EN_N)));
  next |= (1u << U12_GPA_LED_PWR);
  if (chg)  next |= (1u << U12_GPA_LED_CHG);
  if (wifi) next |= (1u << U12_GPA_LED_WIFI);
  if (scan) next |= (1u << U12_GPA_LED_SCAN);
  if (pass) next |= (1u << U12_GPA_LED_PASS);
  if (warn) next |= (1u << U12_GPA_LED_WARN);
  if (next != u12OlatA) {
    u12OlatA = next;
    mcpWriteReg(I2C_ADDR_U12, MCP_REG_OLATA, u12OlatA);
  }
}

void updateLedStatusFromState() {
  if (inStandby) {
    setStatusLeds(true, wifiRadioActive, false, false, false);
    return;
  }
  bool pass = false;
  bool warn = false;
  if (lastScan.kind == DUT_ABORT) {
    warn = true;
  } else if (lastScan.kind == DUT_BJT) {
    pass = lastScan.bjt.valid;
    warn = lastScan.bjt.leaky || lastScan.bjt.powerClamped;
  } else if (lastScan.kind != DUT_NONE) {
    pass = true;
  }
  if (consoleMode == MODE_MULTIMETER) {
    pass = dmm.continuityActive;
    warn = dmm.liveVoltWarn;
  }
  setStatusLeds(false, wifiRadioActive, isMeasurementBusy || drift.active, pass, warn);
}

// ======================= ADS1115 INIT ========================================
static bool initADS1115() {
  if (!probeI2CDevice(I2C_ADDR_ADS1115)) {
    adsOK = false;
    return false;
  }
  if (!ads.begin(I2C_ADDR_ADS1115, &Wire)) {
    adsOK = false;
    return false;
  }
  ads.setDataRate(RATE_ADS1115_128SPS);
  ads.setGain(GAIN_ONE);
  adsOK = true;
  return true;
}

// ======================= 12-BIT HARDWARE SPI DAC DRIVER (U9 MCP4822) =========
static void writeMcp4822Raw(uint8_t channel, uint16_t code12) {
  code12 &= 0x0FFF;
  uint16_t cmd = ((uint16_t)(channel & 0x01) << 15) |
                 (0u << 14) |
                 (1u << 13) |
                 (1u << 12) |
                 code12;

  if (verboseDebug) {
    Serial.printf("[DBG][DAC] ch=%u code=0x%03X\n", (unsigned)channel, (unsigned)code12);
  }

  deselectAllSpiSlaves();
  SPI.beginTransaction(SPISettings(SPI_FREQ_DAC, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_DAC_CS, LOW);
  SPI.write((uint8_t)((cmd >> 8) & 0xFF));
  SPI.write((uint8_t)(cmd & 0xFF));
  digitalWrite(PIN_DAC_CS, HIGH);
  SPI.endTransaction();
}

static int voltageToDacCode(float volts, float scaleCal) {
  float safeScale = constrain(scaleCal, 0.90f, 1.10f);
  float vAdj = constrain(volts / safeScale, 0.0f, DAC_MAX_V);
  float lsbV = (DAC_VREF_1X * DAC_GAIN) / 4096.0f;
  int code = (int)lroundf(vAdj / lsbV);
  return constrain(code, 0, 4095);
}

static void setDacChannel(uint8_t ch, float volts, bool waitSettle) {
  float clamped = constrain(volts, 0.0f, DAC_MAX_V);
  float scale = (ch == 0) ? cal.dacScaleA : cal.dacScaleB;
  int code = voltageToDacCode(clamped, scale);
  if (code != lastDacCode[ch]) {
    writeMcp4822Raw(ch, (uint16_t)code);
    lastDacCode[ch] = code;
    currentDacV[ch] = ((float)code / 4096.0f) * DAC_VREF_1X * DAC_GAIN * scale;
    if (waitSettle) delay(DAC_SETTLE_MS);
  }
}

static void setSourceA(float volts, bool waitSettle = true) {
  setDacChannel(0, volts, waitSettle);
}

static void setSourceB(float volts, bool waitSettle = true) {
  setDacChannel(1, volts, waitSettle);
}

static void setSourcesAB(float vA, float vB, bool waitSettle = true) {
  int prevA = lastDacCode[0];
  int prevB = lastDacCode[1];
  setDacChannel(0, vA, false);
  setDacChannel(1, vB, false);
  if (waitSettle && (lastDacCode[0] != prevA || lastDacCode[1] != prevB)) {
    delay(DAC_SETTLE_MS);
  }
}

// ======================= 16-BIT HARDWARE SPI SHIFT REGISTER MATRIX ===========
static void shiftOut16Raw(uint16_t word) {
  if (verboseDebug && word != currentMatrixWord) {
    Serial.printf("[DBG][SR] word=0x%04X\n", (unsigned)word);
  }
  deselectAllSpiSlaves();
  SPI.beginTransaction(SPISettings(SPI_FREQ_SR, MSBFIRST, SPI_MODE0));
  SPI.write((uint8_t)((word >> 8) & 0xFF));
  SPI.write((uint8_t)(word & 0xFF));
  SPI.endTransaction();
  digitalWrite(PIN_SR_LATCH, HIGH);
  __asm__ __volatile__("nop;nop;nop;nop;");
  digitalWrite(PIN_SR_LATCH, LOW);
}

static bool buildMatrixWord(PinModeSS ch1, PinModeSS ch2, PinModeSS ch3,
                            bool activeLed, uint16_t &outWord) {
  PinModeSS ch[3] = {ch1, ch2, ch3};
  uint16_t word = 0;

  for (int i = 0; i < 3; i++) {
    int baseBit = i * 4;
    if (ch[i] == SS_GND_HARD) {
      word |= (uint16_t)(1u << (12 + i));
    } else if (ch[i] != SS_HIZ) {
      uint8_t modeVal = static_cast<uint8_t>(ch[i]);
      if (modeVal > 7) return false;
      word |= (uint16_t)(modeVal << baseBit);
      word |= (uint16_t)(1u << (baseBit + 3));
    }
  }
  if (activeLed) word |= (uint16_t)(1u << 15);
  outWord = word;
  return true;
}

static bool violates9VInterlock(uint16_t word, bool calibratedLoadMode) {
  if (calibratedLoadMode) return false;
  bool has9VRef = false;
  bool hasHardOrMuxGnd = false;

  for (int i = 0; i < 3; i++) {
    bool en408 = ((word >> (i * 4 + 3)) & 0x01) != 0;
    uint8_t addr = (word >> (i * 4)) & 0x07;
    bool en412 = ((word >> (12 + i)) & 0x01) != 0;

    if (en408 && addr == static_cast<uint8_t>(SS_9V_REF)) {
      has9VRef = true;
    }
    if (en412 || (en408 && addr == static_cast<uint8_t>(SS_GND_MUX))) {
      hasHardOrMuxGnd = true;
    }
  }
  return (has9VRef && hasHardOrMuxGnd);
}

bool setMatrixRaw16(uint16_t word, bool calibratedLoadMode) {
  if (violates9VInterlock(word, calibratedLoadMode)) {
    interlockFaults++;
    shiftOut16Raw(0x0000);
    currentMatrixWord = 0x0000;
    currentChan[0] = currentChan[1] = currentChan[2] = SS_HIZ;
    return false;
  }

  if (word == currentMatrixWord) return true;

  if (word != 0x0000 && currentMatrixWord != 0x0000) {
    shiftOut16Raw(0x0000);
    delayMicroseconds(BBM_DEAD_US);
  }

  shiftOut16Raw(word);
  currentMatrixWord = word;
  return true;
}

bool setMatrix(PinModeSS ch1, PinModeSS ch2, PinModeSS ch3,
               bool activeLed, bool calibratedLoadMode) {
  uint16_t word = 0;
  if (!buildMatrixWord(ch1, ch2, ch3, activeLed, word)) {
    interlockFaults++;
    setMatrixRaw16(0x0000, true);
    return false;
  }
  if (!setMatrixRaw16(word, calibratedLoadMode)) {
    return false;
  }
  currentChan[0] = ch1;
  currentChan[1] = ch2;
  currentChan[2] = ch3;
  return true;
}

static void allSafeHiZ() {
  setMatrix(SS_HIZ, SS_HIZ, SS_HIZ, false, true);
  setSourcesAB(0.0f, 0.0f, false);
}

static bool applyRoleMatrix(int p0, PinModeSS m0, int p1, PinModeSS m1,
                            int p2, PinModeSS m2, uint16_t settleMs = 15,
                            bool calibratedLoadMode = false) {
  PinModeSS arr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
  arr[p0] = m0;
  arr[p1] = m1;
  arr[p2] = m2;
  bool ok = setMatrix(arr[0], arr[1], arr[2], true, calibratedLoadMode);
  if (ok && settleMs > 0) delay(settleMs);
  return ok;
}

// ======================= U10 LIVE CAL & HV DMM MUX (VIA U11 GPB6/7) =========
void selectCalMux(CalMuxCh ch) {
  if (!u11OK) return;
  if (ch == currentCalMux) return;
  uint8_t v = static_cast<uint8_t>(ch) & 0x03;
  u11OlatB = (uint8_t)((u11OlatB & 0x3Fu) | (v << U11_GPB_CAL_A0));
  if (!mcpWriteReg(I2C_ADDR_U11, MCP_REG_OLATB, u11OlatB)) {
    recoverI2CBus();
    initU11Controls();
    u11OlatB = (uint8_t)((u11OlatB & 0x3Fu) | (v << U11_GPB_CAL_A0));
    mcpWriteReg(I2C_ADDR_U11, MCP_REG_OLATB, u11OlatB);
  }
  currentCalMux = ch;
  delayMicroseconds(200);
}

// ======================= ADS1115 INTERRUPT-DRIVEN ENGINE =====================
static void IRAM_ATTR onAdsAlertISR() {
  adsConvDone = true;
}

static float zDiffForGain(adsGain_t pga) {
  switch (pga) {
    case GAIN_SIXTEEN:   return 710000.0f;
    case GAIN_EIGHT:     return 1200000.0f;
    case GAIN_FOUR:      return 2400000.0f;
    case GAIN_TWO:       return 4900000.0f;
    case GAIN_ONE:       return 15000000.0f;
    case GAIN_TWOTHIRDS: return 10000000.0f;
    default:             return 15000000.0f;
  }
}

static adsGain_t stepDownGain(adsGain_t pga) {
  switch (pga) {
    case GAIN_SIXTEEN:   return GAIN_EIGHT;
    case GAIN_EIGHT:     return GAIN_FOUR;
    case GAIN_FOUR:      return GAIN_TWO;
    case GAIN_TWO:       return GAIN_ONE;
    case GAIN_ONE:       return GAIN_TWOTHIRDS;
    default:             return GAIN_TWOTHIRDS;
  }
}

static adsGain_t selectOptimalGain(float absPinV) {
  if (absPinV < 0.215f)      return GAIN_SIXTEEN;
  else if (absPinV < 0.440f) return GAIN_EIGHT;
  else if (absPinV < 0.890f) return GAIN_FOUR;
  else if (absPinV < 1.800f) return GAIN_TWO;
  else                       return GAIN_ONE;
}

static uint32_t expectedConvTimeoutMs() {
  switch (ads.getDataRate()) {
    case RATE_ADS1115_860SPS: return 12;
    case RATE_ADS1115_475SPS: return 15;
    case RATE_ADS1115_250SPS: return 18;
    case RATE_ADS1115_128SPS: return 28;
    case RATE_ADS1115_64SPS:  return 40;
    case RATE_ADS1115_32SPS:  return 65;
    case RATE_ADS1115_16SPS:  return 110;
    case RATE_ADS1115_8SPS:   return 180;
    default:                  return 35;
  }
}

static bool adsConvertSingleShot(uint16_t muxConfig, adsGain_t gain, int16_t &outRaw) {
  if (!adsOK && !initADS1115()) {
    outRaw = 0;
    return false;
  }

  for (int attempt = 0; attempt < 2; attempt++) {
    ads.setGain(gain);
    adsConvDone = false;
    ads.startADCReading(muxConfig, false);

    uint32_t t0 = millis();
    uint32_t timeoutMs = expectedConvTimeoutMs();
    while (!adsConvDone && (uint32_t)(millis() - t0) < timeoutMs) {
      updateToneSequencer();
      delayMicroseconds(100);
    }

    if (adsConvDone || ads.conversionComplete()) {
      outRaw = ads.getLastConversionResults();
      return true;
    }

    recoverI2CBus();
    initU11Controls();
    initU12StatusAndPower(inStandby);
    initADS1115();
  }

  outRaw = 0;
  return false;
}

static float rawToScaledVolts(int16_t raw, adsGain_t pga, bool isDiff) {
  ads.setGain(pga);
  float vPin = ads.computeVolts(raw);
  float rTh = isDiff ? (2.0f * R_TH_DIV) : R_TH_DIV;
  vPin *= (1.0f + rTh / zDiffForGain(pga));
  float vOut = vPin * DIV_GAIN;
  if (verboseDebug) {
    Serial.printf("[DBG][ADS] pga=0x%04X raw=%6d v_div=%.4fV\n",
                  (unsigned)pga, (int)raw, vOut);
  }
  return vOut;
}

static float readMuxAutoRangeRaw(uint16_t muxConfig, bool isDiff, int samples) {
  int16_t probeRaw = 0;
  if (!adsConvertSingleShot(muxConfig, GAIN_ONE, probeRaw)) return 0.0f;

  float probePinV = fabsf(ads.computeVolts(probeRaw));
  adsGain_t gain = selectOptimalGain(probePinV);

  for (int retry = 0; retry < 5; retry++) {
    double acc = 0.0;
    bool clipped = false;
    for (int i = 0; i < samples; i++) {
      int16_t raw = 0;
      if (!adsConvertSingleShot(muxConfig, gain, raw)) return 0.0f;
      if (abs((int)raw) >= 32000 && gain != GAIN_TWOTHIRDS) {
        gain = stepDownGain(gain);
        clipped = true;
        break;
      }
      acc += (double)rawToScaledVolts(raw, gain, isDiff);
    }
    if (!clipped) {
      return (float)(acc / (double)samples);
    }
  }
  return 0.0f;
}

static float readNodeVRaw(uint8_t ch, int samples = 4) {
  if (ch > 3) return 0.0f;
  return readMuxAutoRangeRaw(MUX_BY_CHANNEL[ch], false, samples);
}

float readNodeV(uint8_t ch, int samples) {
  if (ch > 3) return 0.0f;
  float v = readNodeVRaw(ch, samples) - cal.adcOffsetV[ch];
  if (ch < 3) v *= cal.chGainCorr[ch];
  return v;
}

static void readAllNodes(float v[4], int samples = 4) {
  for (uint8_t ch = 0; ch < 4; ch++) {
    v[ch] = readNodeV(ch, samples);
  }
}

float readLiveRefV(CalMuxCh muxCh, int samples) {
  selectCalMux(muxCh);
  return readNodeV(3, samples);
}

static float readDiffRaw(uint8_t dutCh, int samples = 4) {
  uint16_t mux = ADS1X15_REG_CONFIG_MUX_DIFF_0_3;
  if (dutCh == 1)      mux = ADS1X15_REG_CONFIG_MUX_DIFF_1_3;
  else if (dutCh == 2) mux = ADS1X15_REG_CONFIG_MUX_DIFF_2_3;
  return readMuxAutoRangeRaw(mux, true, samples);
}

float readDiffSource(uint8_t dutCh, CalMuxCh srcMux, float vSrcActual, int samples) {
  if (dutCh > 2) return 0.0f;
  selectCalMux(srcMux);
  float vDiffRaw = readDiffRaw(dutCh, samples);
  float vDiffCorr = vDiffRaw - (cal.diffZeroAt0V[dutCh] + cal.diffCmSlope[dutCh] * vSrcActual);
  return -vDiffCorr;
}

// ======================= EXACT RESISTANCE & POWER GUARD ======================
static float pathResToSource(int ch, PinModeSS mode) {
  float rSwitch = (ch >= 0 && ch < 3) ? cal.rDg408On[ch] : R_DG408_NOM;
  switch (mode) {
    case SS_SRCA_470R: return R_PROT + R_LOW       + rSwitch;
    case SS_SRCA_2K49: return R_PROT + R_KEEN      + rSwitch;
    case SS_SRCB_100K: return R_PROT + R_MID       + rSwitch;
    case SS_SRCB_2M2:  return R_PROT + R_KEEN_BASE + rSwitch;
    case SS_9V_REF:    return R_PROT + rSwitch;
    default:           return INFINITY;
  }
}

static float pathResToGnd(int ch, PinModeSS mode) {
  float r408 = (ch >= 0 && ch < 3) ? cal.rDg408On[ch] : R_DG408_NOM;
  float r412 = (ch >= 0 && ch < 3) ? cal.rDg412On[ch] : R_DG412_NOM;
  switch (mode) {
    case SS_GND_470R: return R_PROT + R_LOW  + r408;
    case SS_GND_2K49: return R_PROT + R_KEEN + r408;
    case SS_GND_MUX:  return R_PROT + r408;
    case SS_GND_HARD: return R_PROT + r412;
    default:          return INFINITY;
  }
}

static bool checkPowerSafety(float vDut, float iDut_A, float &peakMwOut) {
  float pMw = fabsf(vDut) * fabsf(iDut_A) * 1000.0f;
  if (pMw > peakMwOut) peakMwOut = pMw;
  if (pMw > P_DUT_MAX_MW) {
    powerClampedFlag = true;
    return false;
  }
  return true;
}

// ======================= ASYNC NON-BLOCKING DS18B20 ==========================
static void initTempSensor() {
  tempSensor.begin();
  tempPresent = (tempSensor.getDeviceCount() > 0);
  tempSensor.setResolution(12);
  tempSensor.setWaitForConversion(false);
  if (tempPresent) {
    tempSensor.requestTemperatures();
    lastTempReqMs = millis();
  }
}

static void updateTempAsync() {
  if (!tempPresent) return;
  uint32_t now = millis();
  if ((uint32_t)(now - lastTempReqMs) >= 800) {
    if (tempSensor.isConversionComplete() || (uint32_t)(now - lastTempReqMs) >= 900) {
      float t = tempSensor.getTempCByIndex(0);
      if (t != DEVICE_DISCONNECTED_C && t > -50.0f && t < 125.0f) {
        latestTempC = t;
        tempValid = true;
      } else {
        latestTempC = NAN;
        tempValid = false;
      }
      tempSensor.requestTemperatures();
      lastTempReqMs = now;
    }
  }
}

static float getTempC() {
  updateTempAsync();
  return tempValid ? latestTempC : NAN;
}

// ======================= FAST ESP32 ADC1 HELPERS =============================
static float readFastAdcCalV(int ch) {
  int pin = FAST_TP_PINS[ch];
  float vRaw = (float)analogReadMilliVolts(pin) * 0.001f;
  return vRaw * cal.fastAdcSlope[ch] + cal.fastAdcOffsetV[ch];
}

static float readFastAdcAvgMV(int ch, int n) {
  uint32_t acc = 0;
  int pin = FAST_TP_PINS[ch];
  for (int i = 0; i < n; i++) {
    acc += analogReadMilliVolts(pin);
    delayMicroseconds(20);
  }
  return (float)acc / (float)n;
}

static float measureFastCapPf_2M2(int pHi, int pLo, int pThird, bool gndIsCom = false) {
  const float vStep = 2.200f;
  const float vLowThresh  = 0.25f * vStep;
  const float vHighThresh = 0.70f * vStep;
  const float lnRatio = 0.91629073f;

  PinModeSS disch[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
  disch[pHi] = SS_GND_HARD;
  if (!gndIsCom) disch[pLo] = SS_GND_HARD;

  PinModeSS chg[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
  chg[pHi] = SS_SRCB_2M2;
  if (!gndIsCom) chg[pLo] = SS_GND_HARD;

  uint16_t dischWord = 0, chgWord = 0;
  buildMatrixWord(disch[0], disch[1], disch[2], true, dischWord);
  buildMatrixWord(chg[0], chg[1], chg[2], true, chgWord);
  (void)pThird;

  setSourcesAB(0.0f, vStep, true);
  float vSrcBActual = readLiveRefV(CAL_SRCB, 4);
  if (vSrcBActual < 1.5f) vSrcBActual = vStep;

  auto probeAtUs = [&](uint32_t targetUs) -> float {
    setMatrixRaw16(dischWord, true);
    delay(2);
    shiftOut16Raw(0x0000);
    delayMicroseconds(BBM_DEAD_US);

    deselectAllSpiSlaves();
    SPI.beginTransaction(SPISettings(SPI_FREQ_SR, MSBFIRST, SPI_MODE0));
    SPI.write((uint8_t)((chgWord >> 8) & 0xFF));
    SPI.write((uint8_t)(chgWord & 0xFF));
    SPI.endTransaction();
    int64_t tStart = esp_timer_get_time();
    digitalWrite(PIN_SR_LATCH, HIGH);
    __asm__ __volatile__("nop;nop;nop;nop;");
    digitalWrite(PIN_SR_LATCH, LOW);
    currentMatrixWord = chgWord;

    while ((uint32_t)(esp_timer_get_time() - tStart) < targetUs) {}
    float v = readFastAdcCalV(pHi);
    setMatrixRaw16(dischWord, true);
    return v;
  };

  auto binarySearchThreshold = [&](float threshV, uint32_t maxUs) -> int32_t {
    if (probeAtUs(maxUs) < threshV) return -1;
    uint32_t lo = 2, hi = maxUs;
    for (int iter = 0; iter < 13; iter++) {
      uint32_t mid = (lo + hi) / 2;
      if (probeAtUs(mid) >= threshV) hi = mid;
      else                           lo = mid + 1;
      if (lo >= hi) break;
    }
    return (int32_t)hi;
  };

  float thresh1 = (vLowThresh / vStep) * vSrcBActual;
  float thresh2 = (vHighThresh / vStep) * vSrcBActual;

  int32_t t1 = binarySearchThreshold(thresh1, 8000);
  int32_t t2 = binarySearchThreshold(thresh2, 16000);
  allSafeHiZ();

  if (t1 < 0 || t2 <= t1) return NAN;

  float rTot = pathResToSource(pHi, SS_SRCB_2M2) +
               (gndIsCom ? 0.0f : pathResToGnd(pLo, SS_GND_HARD));
  float tau_us = (float)(t2 - t1) / lnRatio;
  return (tau_us * 1e-6f / rTot) * 1e12f;
}

// ======================= WI-FI WEB API, SYNC POST & BLE GATT =================
void ensureRadioOffForAnalog() {
#if ENABLE_BLE_TELEMETRY
  if (bleActive) {
    BLEDevice::deinit(false);
    bleActive = false;
    bleClientConnected = false;
    bleServer = nullptr;
    bleTxChar = nullptr;
  }
#endif
#if ENABLE_WIFI_SYNC
  if (httpServerStarted) {
    httpServer.stop();
    httpServerStarted = false;
  }
  if (wifiRadioActive || WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiRadioActive = false;
  }
#endif
  snprintf(activeIpStr, sizeof(activeIpStr), "RF-OFF");
  updateLedStatusFromState();
}

#if ENABLE_BLE_TELEMETRY
class GeBleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    (void)pServer;
    bleClientConnected = true;
  }
  void onDisconnect(BLEServer *pServer) override {
    (void)pServer;
    bleClientConnected = false;
    if (bleActive) BLEDevice::startAdvertising();
  }
};
#endif

#if ENABLE_WIFI_SYNC
static void sendJsonWithCors(int code, const String &body) {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.send(code, "application/json", body);
}

static void handleApiLatest() {
  if (historyCount == 0) {
    sendJsonWithCors(200, "{\"type\":\"NONE\",\"msg\":\"No DUT tests recorded\"}");
    return;
  }
  int latestIdx = (historyHead - 1 + HISTORY_CAP) % HISTORY_CAP;
  sendJsonWithCors(200, String(historyBuf[latestIdx].json));
}

static void handleApiHistory() {
  String out;
  out.reserve((size_t)historyCount * 320U + 64U);
  out += "{\"count\":";
  out += String(historyCount);
  out += ",\"items\":[";
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyHead - 1 - i + HISTORY_CAP) % HISTORY_CAP;
    if (i > 0) out += ",";
    out += historyBuf[idx].json;
  }
  out += "]}";
  sendJsonWithCors(200, out);
}

static void handleApiStatus() {
  char tStr[20];
  fmtJsonFloat(tStr, sizeof(tStr), getTempC(), 2);
  const char *lHash = (historyCount > 0)
                          ? historyBuf[(historyHead - 1 + HISTORY_CAP) % HISTORY_CAP].hash
                          : "";
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"device\":\"KAOTI-Ge-LAB-PRO\",\"rev\":\"4.0\",\"standby\":%s,"
           "\"ana_powered\":%s,\"ip\":\"%s\",\"cal_valid\":%s,"
           "\"temp_C\":%s,\"history_count\":%d,\"latest_hash\":\"%s\"}",
           inStandby ? "true" : "false",
           analogRailOn ? "true" : "false",
           activeIpStr,
           cal.valid ? "true" : "false",
           tStr, historyCount, lHash);
  sendJsonWithCors(200, String(buf));
}

static void handleRootPage() {
  const char *lHash = (historyCount > 0)
                          ? historyBuf[(historyHead - 1 + HISTORY_CAP) % HISTORY_CAP].hash
                          : "NONE";
  String html = "<!doctype html><html><head><meta charset='utf-8'>"
                "<title>KAOTI Ge-LAB PRO</title>"
                "<style>body{font-family:monospace;background:#071224;color:#eef;padding:18px}"
                "a{color:#4ef}</style></head><body>"
                "<h2>KAOTI Ge-LAB PRO Rev 4.0</h2>"
                "<p>Latest Hash: <b>" + String(lHash) + "</b> | History: " + String(historyCount) + "/32</p>"
                "<p><a href='/api/latest'>/api/latest</a> | "
                "<a href='/api/history'>/api/history</a> | "
                "<a href='/api/status'>/api/status</a></p></body></html>";
  httpServer.send(200, "text/html", html);
}
#endif

static void startStandbyRadioServices() {
#if ENABLE_WIFI_SYNC
  bool hasSta = (strlen(wifiCfg.ssid) > 0);
  WiFi.mode(hasSta ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP("KAOTI-Ge-PRO", "kaotige123");

  if (hasSta) {
    WiFi.begin(wifiCfg.ssid, wifiCfg.pass);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (uint32_t)(millis() - t0) < 3500) {
      delay(50);
    }
  }

  if (hasSta && WiFi.status() == WL_CONNECTED) {
    snprintf(activeIpStr, sizeof(activeIpStr), "%s", WiFi.localIP().toString().c_str());
  } else {
    snprintf(activeIpStr, sizeof(activeIpStr), "%s", WiFi.softAPIP().toString().c_str());
  }

  if (!httpServerStarted) {
    httpServer.on("/",            HTTP_GET, handleRootPage);
    httpServer.on("/api/latest",  HTTP_GET, handleApiLatest);
    httpServer.on("/api/history", HTTP_GET, handleApiHistory);
    httpServer.on("/api/status",  HTTP_GET, handleApiStatus);
    httpServer.begin();
    httpServerStarted = true;
  }
  wifiRadioActive = true;
#else
  snprintf(activeIpStr, sizeof(activeIpStr), "BLE-ONLY");
#endif

#if ENABLE_BLE_TELEMETRY
  if (!bleActive) {
    BLEDevice::init("KAOTI-Ge-PRO");
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new GeBleServerCallbacks());
    BLEService *svc = bleServer->createService(BLE_SVC_UUID);
    bleTxChar = svc->createCharacteristic(
        BLE_TX_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    bleTxChar->addDescriptor(new BLE2902());
    BLECharacteristic *rxChar = svc->createCharacteristic(
        BLE_RX_UUID, BLECharacteristic::PROPERTY_WRITE);
    (void)rxChar;
    if (historyCount > 0) {
      int latestIdx = (historyHead - 1 + HISTORY_CAP) % HISTORY_CAP;
      bleTxChar->setValue(historyBuf[latestIdx].hash);
    } else {
      bleTxChar->setValue("KAOTI-Ge-PRO READY");
    }
    svc->start();
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SVC_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
    bleActive = true;
  }
#endif
  updateLedStatusFromState();
}

#if ENABLE_WIFI_SYNC
static void wifiPostAfterSafeHiZ(const char *jsonPayload) {
  if (!wifiCfg.autoPost || strlen(wifiCfg.ssid) == 0 || strlen(wifiCfg.serverHost) == 0) {
    return;
  }
  allSafeHiZ();
  setStatusLeds(inStandby, true, false, false, false);

  bool wasOff = (WiFi.getMode() == WIFI_OFF);
  if (wasOff) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiCfg.ssid, wifiCfg.pass);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (uint32_t)(millis() - t0) < 4000) {
      delay(40);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    if (client.connect(wifiCfg.serverHost, wifiCfg.serverPort)) {
      size_t bodyLen = strlen(jsonPayload);
      client.printf("POST /api/tests HTTP/1.1\r\n"
                    "Host: %s:%u\r\n"
                    "Content-Type: application/json\r\n"
                    "Connection: close\r\n"
                    "Content-Length: %u\r\n\r\n%s",
                    wifiCfg.serverHost, (unsigned)wifiCfg.serverPort,
                    (unsigned)bodyLen, jsonPayload);
      uint32_t tWait = millis();
      while (client.connected() && !client.available() && (uint32_t)(millis() - tWait) < 1200) {
        delay(10);
      }
      client.stop();
      emitStatusJson("INFO", "HTTP POST /api/tests delivered");
    }
  }

  if (wasOff && !inStandby) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiRadioActive = false;
  }
  updateLedStatusFromState();
}
#else
static void wifiPostAfterSafeHiZ(const char *jsonPayload) { (void)jsonPayload; }
#endif

// ======================= STAGE 0: LIVE-VOLTAGE GUARD & SOFT DISCHARGE ========
static bool runStage0GuardAndDischarge() {
  allSafeHiZ();
  delay(5);

  float v[4];
  readAllNodes(v, 2);
  float vMax = fmaxf(fabsf(v[0]), fmaxf(fabsf(v[1]), fabsf(v[2])));

  if (vMax > V_ABORT_HARD) {
    lastScan.kind = DUT_ABORT;
    snprintf(lastScan.abortReason, sizeof(lastScan.abortReason), "LIVE_VOLTAGE_GT_9V5");
    lastScan.abortV = vMax;
    emitAbortJson(lastScan.abortReason, vMax);
    allSafeHiZ();
    return false;
  }

  if (vMax > V_BLEED_START) {
    uint32_t tStart = millis();
    setMatrix(SS_GND_2K49, SS_GND_2K49, SS_GND_2K49, true);
    while ((uint32_t)(millis() - tStart) < 1500) {
      delay(25);
      readAllNodes(v, 1);
      vMax = fmaxf(fabsf(v[0]), fmaxf(fabsf(v[1]), fabsf(v[2])));
      if (vMax < 1.50f) break;
    }

    setMatrix(SS_GND_470R, SS_GND_470R, SS_GND_470R, true);
    while ((uint32_t)(millis() - tStart) < 3000) {
      delay(25);
      readAllNodes(v, 1);
      vMax = fmaxf(fabsf(v[0]), fmaxf(fabsf(v[1]), fabsf(v[2])));
      if (vMax < V_BLEED_DONE) break;
    }

    if (vMax >= V_BLEED_DONE) {
      lastScan.kind = DUT_ABORT;
      snprintf(lastScan.abortReason, sizeof(lastScan.abortReason), "EXT_LIVE_SOURCE");
      lastScan.abortV = vMax;
      emitAbortJson(lastScan.abortReason, vMax);
      allSafeHiZ();
      return false;
    }
  }

  setMatrix(SS_GND_2K49, SS_GND_2K49, SS_GND_2K49, true);
  delay(5);
  setMatrix(SS_GND_HARD, SS_GND_HARD, SS_GND_HARD, true);
  delay(2);
  allSafeHiZ();
  delay(2);
  return true;
}

// ======================= STAGE 1: ULTRA-SAFE PIN-TOPOLOGY SCAN ===============
static const int PAIR_TABLE[6][3] = {
  {0, 1, 2}, {0, 2, 1}, {1, 0, 2},
  {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
};

static void scanStage1Pairs(float vDrive, PairProbe out[6], int &conductCount) {
  conductCount = 0;
  setSourcesAB(0.0f, vDrive, true);
  float vSrcBActual = readLiveRefV(CAL_SRCB, 4);

  for (int k = 0; k < 6; k++) {
    int hi = PAIR_TABLE[k][0];
    int lo = PAIR_TABLE[k][1];
    int th = PAIR_TABLE[k][2];

    applyRoleMatrix(hi, SS_SRCB_100K, lo, SS_GND_HARD, th, SS_HIZ, 12);
    float vHi = readNodeV(hi, 3);
    float vLo = readNodeV(lo, 3);
    float vDrop = readDiffSource(hi, CAL_SRCB, vSrcBActual, 3);
    if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vSrcBActual - vHi);

    float rPath = pathResToSource(hi, SS_SRCB_100K);
    float i_uA = (vDrop / rPath) * 1e6f;

    out[k].hi       = hi;
    out[k].lo       = lo;
    out[k].third    = th;
    out[k].vHi      = vHi;
    out[k].vLo      = vLo;
    out[k].vDut     = vHi - vLo;
    out[k].i_uA     = i_uA;
    out[k].conducts = (i_uA > 0.35f);
    if (out[k].conducts) conductCount++;
  }
  allSafeHiZ();
}

static bool testCapChargeRetention(int pA, int pB) {
  int pTh = 3 - pA - pB;
  setSourcesAB(2.50f, 0.0f, true);
  applyRoleMatrix(pA, SS_SRCA_470R, pB, SS_GND_HARD, pTh, SS_HIZ, 60);
  applyRoleMatrix(pA, SS_HIZ, pB, SS_GND_HARD, pTh, SS_HIZ, 2);
  float vRet = readNodeV(pA, 2) - readNodeV(pB, 2);

  setSourcesAB(0.0f, 0.0f, false);
  applyRoleMatrix(pA, SS_GND_2K49, pB, SS_GND_HARD, pTh, SS_HIZ, 15);
  applyRoleMatrix(pA, SS_GND_HARD, pB, SS_GND_HARD, pTh, SS_HIZ, 10);
  allSafeHiZ();

  return (vRet > 0.015f);
}

// ======================= OPEN-SOCKET AUTO-CALIBRATION ========================
static void showCalBannerTft(const char *line1, const char *line2, uint16_t hdrCol = COL_SIDE_HDR) {
  if (!tftsOK) return;
  tft2.fillScreen(COL_BG);
  tft2.fillRect(0, 0, 320, 32, hdrCol);
  tft2.setTextColor(COL_WHITE);
  tft2.setTextSize(2);
  tft2.setCursor(10, 8);
  tft2.print("OPEN-SOCKET AUTO-CAL");
  tft2.setTextColor(COL_CYAN);
  tft2.setTextSize(2);
  tft2.setCursor(12, 54);
  tft2.print(line1);
  tft2.setTextColor(COL_MUTED);
  tft2.setTextSize(1);
  tft2.setCursor(12, 92);
  tft2.print(line2);
}

bool runOpenSocketCal() {
  if (inStandby) {
    emitStatusJson("ERROR", "Exit Standby before Auto-Cal");
    return false;
  }
  ensureRadioOffForAnalog();
  setStatusLeds(false, false, true, false, false);
  emitStatusJson("INFO", "Starting Open-Socket Auto-Cal...");
  showCalBannerTft("Keep ZIF socket empty", "Checking open-circuit & zero offsets...");

  if (!runStage0GuardAndDischarge()) {
    updateLedStatusFromState();
    renderScreens();
    return false;
  }

  PairProbe p[6];
  int cnt = 0;
  scanStage1Pairs(3.60f, p, cnt);
  if (cnt > 0) {
    emitStatusJson("ERROR", "Auto-Cal aborted: DUT detected in socket!");
    showCalBannerTft("AUTO-CAL ABORTED!", "Remove DUT from ZIF socket & retry", COL_ERR_BDG);
    setStatusLeds(false, false, false, false, true);
    beepTone(280, 140);
    drainAudioShort(200);
    delay(1000);
    updateLedStatusFromState();
    renderScreens();
    return false;
  }

  CalData nextCal;
  initCalDefaults(nextCal);

  // 1. Zero-offset calibration (U10 S4A if open < 25mV, else V_SRC_A = 0.000V)
  allSafeHiZ();
  setSourcesAB(0.0f, 0.0f, true);
  setMatrix(SS_GND_HARD, SS_GND_HARD, SS_GND_HARD, true);
  delay(10);

  selectCalMux(CAL_DMM30V);
  delay(5);
  float vS4Check = readNodeVRaw(3, 8);
  CalMuxCh zeroRefMux = (fabsf(vS4Check) < 0.025f) ? CAL_DMM30V : CAL_SRCA;
  selectCalMux(zeroRefMux);
  delay(5);

  nextCal.adcOffsetV[3] = constrain(readNodeVRaw(3, 16), -0.050f, 0.050f);
  for (int i = 0; i < 3; i++) {
    nextCal.adcOffsetV[i] = constrain(readNodeVRaw(i, 16), -0.050f, 0.050f);
    nextCal.diffZeroAt0V[i] = constrain(readDiffRaw(i, 16), -0.050f, 0.050f);
  }
  allSafeHiZ();

  // 2. Live +9V_LIM rail measurement
  selectCalMux(CAL_9VLIM);
  delay(5);
  float v9 = readNodeVRaw(3, 16) - nextCal.adcOffsetV[3];
  nextCal.v9Lim_V = (v9 > 7.0f && v9 < 10.8f) ? v9 : 9.000f;

  cal = nextCal;

  // 3. MCP4822 + U5 V_SRC_A and V_SRC_B scale calibration
  showCalBannerTft("Calibrating DAC & Gain", "Sweeping 4.50V / 8.50V reference points...");
  setSourcesAB(4.500f, 4.500f, true);
  selectCalMux(CAL_SRCA);
  float vA45 = readNodeVRaw(3, 12) - nextCal.adcOffsetV[3];
  selectCalMux(CAL_SRCB);
  float vB45 = readNodeVRaw(3, 12) - nextCal.adcOffsetV[3];

  setSourcesAB(8.500f, 8.500f, true);
  selectCalMux(CAL_SRCA);
  float vA85 = readNodeVRaw(3, 12) - nextCal.adcOffsetV[3];
  selectCalMux(CAL_SRCB);
  float vB85 = readNodeVRaw(3, 12) - nextCal.adcOffsetV[3];

  float sA = (vA85 - vA45) / 4.000f;
  float sB = (vB85 - vB45) / 4.000f;
  nextCal.dacScaleA = (sA > 0.90f && sA < 1.10f) ? sA : 1.0f;
  nextCal.dacScaleB = (sB > 0.90f && sB < 1.10f) ? sB : 1.0f;

  // 4. Channel gain normalization & differential common-mode mismatch across TP1..TP3
  setSourcesAB(7.500f, 0.0f, true);
  selectCalMux(CAL_SRCA);
  float vRefA = readNodeVRaw(3, 16) - nextCal.adcOffsetV[3];
  for (int i = 0; i < 3; i++) {
    PinModeSS arr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
    arr[i] = SS_SRCA_470R;
    setMatrix(arr[0], arr[1], arr[2], true);
    delay(10);
    float vCh = readNodeVRaw(i, 12) - nextCal.adcOffsetV[i];
    if (vCh > 5.0f && vRefA > 5.0f) {
      nextCal.chGainCorr[i] = constrain(vRefA / vCh, 0.95f, 1.05f);
    }
    float vDiff = readDiffRaw(i, 12);
    nextCal.diffCmSlope[i] = constrain((vDiff - nextCal.diffZeroAt0V[i]) / vRefA, -0.02f, 0.02f);
  }
  allSafeHiZ();

  cal = nextCal;

  // 5. Measure R_DG408_ON & R_DG412_ON per channel
  showCalBannerTft("Measuring Mux Ron", "Characterizing DG408DJ & DG412LDJ...");
  setSourcesAB(7.500f, 0.0f, true);
  float vSrcMeas = readLiveRefV(CAL_SRCA, 8);
  for (int i = 0; i < 3; i++) {
    uint16_t bit412 = (uint16_t)(1u << (12 + i));
    uint16_t ledBit = (uint16_t)(1u << 15);
    uint16_t w470  = ledBit | bit412 | (uint16_t)((1u << (i * 4 + 3)) | (0u << (i * 4)));
    uint16_t w2k49 = ledBit | bit412 | (uint16_t)((1u << (i * 4 + 3)) | (1u << (i * 4)));

    setMatrixRaw16(w470, true);
    delay(6);
    float v1 = readNodeV(i, 8);

    setMatrixRaw16(w2k49, true);
    delay(6);
    float v2 = readNodeV(i, 8);

    allSafeHiZ();
    if (v1 > 0.03f && v2 > 0.005f && v1 < vSrcMeas && v2 < v1) {
      float r1 = (vSrcMeas / v1) - 1.0f;
      float r2 = (vSrcMeas / v2) - 1.0f;
      if ((r2 - r1) > 10.0f) {
        float r412 = (R_KEEN - R_LOW) / (r2 - r1);
        float r408 = r1 * r412 - R_LOW;
        if (r412 >= 4.0f && r412 <= 80.0f && r408 >= 10.0f && r408 <= 150.0f) {
          nextCal.rDg412On[i] = r412;
          nextCal.rDg408On[i] = r408;
        }
      }
    }
  }

  // 6. Calibrate ESP32 ADC1 fast RC timing pins
  for (int i = 0; i < 3; i++) {
    PinModeSS arr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
    arr[i] = SS_SRCA_470R;
    setMatrix(arr[0], arr[1], arr[2], true);

    setSourceA(0.550f, true);
    float ads1 = readNodeV(i, 4);
    float fst1 = readFastAdcAvgMV(i, 16) * 0.001f;

    setSourceA(1.900f, true);
    float ads2 = readNodeV(i, 4);
    float fst2 = readFastAdcAvgMV(i, 16) * 0.001f;

    if ((fst2 - fst1) > 0.5f) {
      float slope = (ads2 - ads1) / (fst2 - fst1);
      if (slope > 0.85f && slope < 1.15f) {
        nextCal.fastAdcSlope[i]   = slope;
        nextCal.fastAdcOffsetV[i] = ads1 - slope * fst1;
      }
    }
    allSafeHiZ();
  }

  cal = nextCal;

  // 7. Measure baseline open-socket stray capacitance
  showCalBannerTft("Measuring Stray Cap", "2.20M binary search open-socket baseline...");
  for (int i = 0; i < 3; i++) {
    int pLo = (i + 1) % 3;
    int pTh = (i + 2) % 3;
    double acc = 0.0;
    int good = 0;
    for (int r = 0; r < 4; r++) {
      float c = measureFastCapPf_2M2(i, pLo, pTh);
      if (!isnan(c) && c >= 0.0f && c < 600.0f) {
        acc += c;
        good++;
      }
    }
    if (good > 0) nextCal.cStray_pF[i] = (float)(acc / good);
  }

  nextCal.valid = 1;
  cal = nextCal;
  bool nvsOk = saveCalNVS(cal);

  Serial.printf(
      "{\"type\":\"CAL_DONE\",\"nvs_saved\":%s,\"v9_lim_V\":%.4f,"
      "\"dac_scale\":[%.5f,%.5f],\"r_dg408\":[%.2f,%.2f,%.2f],"
      "\"r_dg412\":[%.2f,%.2f,%.2f],\"c_stray_pF\":[%.1f,%.1f,%.1f]}\n",
      nvsOk ? "true" : "false", cal.v9Lim_V,
      cal.dacScaleA, cal.dacScaleB,
      cal.rDg408On[0], cal.rDg408On[1], cal.rDg408On[2],
      cal.rDg412On[0], cal.rDg412On[1], cal.rDg412On[2],
      cal.cStray_pF[0], cal.cStray_pF[1], cal.cStray_pF[2]);

  playModemScanStep(3);
  updateLedStatusFromState();
  renderScreens();
  return true;
}

static void emitCalInfoJson() {
  char tStr[20];
  fmtJsonFloat(tStr, sizeof(tStr), getTempC(), 2);
  float v9Live = analogRailOn ? readLiveRefV(CAL_9VLIM, 4) : 0.0f;
  Serial.printf(
      "{\"type\":\"CAL_INFO\",\"valid\":%s,\"v9_lim_cal_V\":%.4f,\"v9_lim_live_V\":%.4f,"
      "\"dac_scale\":[%.5f,%.5f],\"ch_gain\":[%.5f,%.5f,%.5f],"
      "\"r_dg408\":[%.2f,%.2f,%.2f],\"r_dg412\":[%.2f,%.2f,%.2f],"
      "\"c_stray_pF\":[%.1f,%.1f,%.1f],\"interlock_faults\":%lu,\"temp_C\":%s}\n",
      cal.valid ? "true" : "false", cal.v9Lim_V, v9Live,
      cal.dacScaleA, cal.dacScaleB,
      cal.chGainCorr[0], cal.chGainCorr[1], cal.chGainCorr[2],
      cal.rDg408On[0], cal.rDg408On[1], cal.rDg408On[2],
      cal.rDg412On[0], cal.rDg412On[1], cal.rDg412On[2],
      cal.cStray_pF[0], cal.cStray_pF[1], cal.cStray_pF[2],
      (unsigned long)interlockFaults, tStr);
}

// ======================= STAGE 2: BJT CHARACTERIZER ==========================
static float measureBjtLeakageAuto(int pOpen, int pRef, int pSense,
                                   bool isPNP, float vTest, float &vceOut, float &peakMw) {
  auto runRange = [&](PinModeSS senseMode, float vSrc, float &i_uA_out) {
    if (!isPNP) {
      if (senseMode == SS_SRCB_100K) {
        setSourcesAB(0.0f, vSrc, true);
        float vLiveB = readLiveRefV(CAL_SRCB, 4);
        applyRoleMatrix(pOpen, SS_HIZ, pRef, SS_GND_HARD, pSense, SS_SRCB_100K, 15);
        float vDrop = readDiffSource(pSense, CAL_SRCB, vLiveB, 6);
        float vS = readNodeV(pSense, 4);
        float vR = readNodeV(pRef, 4);
        if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveB - vS);
        i_uA_out = (vDrop / pathResToSource(pSense, SS_SRCB_100K)) * 1e6f;
        vceOut = vS - vR;
      } else {
        setSourcesAB(vSrc, 0.0f, true);
        float vLiveA = readLiveRefV(CAL_SRCA, 4);
        applyRoleMatrix(pOpen, SS_HIZ, pRef, SS_GND_HARD, pSense, senseMode, 15);
        float vDrop = readDiffSource(pSense, CAL_SRCA, vLiveA, 6);
        float vS = readNodeV(pSense, 4);
        float vR = readNodeV(pRef, 4);
        if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveA - vS);
        i_uA_out = (vDrop / pathResToSource(pSense, senseMode)) * 1e6f;
        vceOut = vS - vR;
      }
    } else {
      PinModeSS refMode = (vSrc >= 8.70f) ? SS_9V_REF : SS_SRCA_470R;
      if (senseMode == SS_SRCB_100K) {
        setSourcesAB((refMode == SS_SRCA_470R) ? vSrc : 0.0f, 0.0f, true);
        float vLiveB = readLiveRefV(CAL_SRCB, 4);
        applyRoleMatrix(pOpen, SS_HIZ, pRef, refMode, pSense, SS_SRCB_100K, 15);
        float vDrop = -readDiffSource(pSense, CAL_SRCB, vLiveB, 6);
        float vS = readNodeV(pSense, 4);
        float vR = readNodeV(pRef, 4);
        if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vS - vLiveB);
        i_uA_out = (vDrop / pathResToSource(pSense, SS_SRCB_100K)) * 1e6f;
        vceOut = vR - vS;
      } else {
        setSourcesAB((refMode == SS_SRCA_470R) ? vSrc : 0.0f, 0.0f, true);
        applyRoleMatrix(pOpen, SS_HIZ, pRef, refMode, pSense, senseMode, 15);
        float vS = readNodeV(pSense, 6);
        float vR = readNodeV(pRef, 4);
        i_uA_out = (fmaxf(0.0f, vS) / pathResToGnd(pSense, senseMode)) * 1e6f;
        vceOut = vR - vS;
      }
    }
  };

  float i_uA = 0.0f;
  PinModeSS midMode = isPNP ? SS_GND_2K49 : SS_SRCA_2K49;
  runRange(midMode, vTest, i_uA);

  if (!checkPowerSafety(vceOut, i_uA * 1e-6f, peakMw)) {
    runRange(midMode, 4.50f, i_uA);
    checkPowerSafety(vceOut, i_uA * 1e-6f, peakMw);
    allSafeHiZ();
    return i_uA;
  }

  if (i_uA < 2.0f) {
    runRange(SS_SRCB_100K, vTest, i_uA);
    checkPowerSafety(vceOut, i_uA * 1e-6f, peakMw);
  } else if (i_uA > 1800.0f) {
    PinModeSS lowMode = isPNP ? SS_GND_470R : SS_SRCA_470R;
    float iLow = i_uA;
    runRange(lowMode, vTest, iLow);
    if (!checkPowerSafety(vceOut, iLow * 1e-6f, peakMw)) {
      runRange(lowMode, 4.50f, iLow);
      checkPowerSafety(vceOut, iLow * 1e-6f, peakMw);
    }
    i_uA = iLow;
  }

  allSafeHiZ();
  return i_uA;
}

static float evalBjtOrientation1V5(int pE, int pB, int pC, bool isPNP) {
  float vce = 0.0f, peakMw = 0.0f;
  float iceo = measureBjtLeakageAuto(pB, pE, pC, isPNP, 1.50f, vce, peakMw);

  float ic_uA = 0.0f, ib_uA = 0.0f;
  if (!isPNP) {
    setSourcesAB(1.65f, 4.50f, true);
    float vLiveA = readLiveRefV(CAL_SRCA, 3);
    float vLiveB = readLiveRefV(CAL_SRCB, 3);
    applyRoleMatrix(pE, SS_GND_HARD, pB, SS_SRCB_2M2, pC, SS_SRCA_470R, 12);
    float vB = readNodeV(pB, 4);
    float vC = readNodeV(pC, 4);
    float vE = readNodeV(pE, 4);
    float vDropC = readDiffSource(pC, CAL_SRCA, vLiveA, 4);
    if (vDropC < 0.0f) vDropC = fmaxf(0.0f, vLiveA - vC);
    ib_uA = ((vLiveB - vB) / pathResToSource(pB, SS_SRCB_2M2)) * 1e6f;
    ic_uA = (vDropC / pathResToSource(pC, SS_SRCA_470R)) * 1e6f;
    if ((ic_uA - iceo) < 20.0f || (vC - vE) < 0.25f) {
      setSourcesAB(1.65f, 1.50f, true);
      vLiveB = readLiveRefV(CAL_SRCB, 3);
      applyRoleMatrix(pE, SS_GND_HARD, pB, SS_SRCB_100K, pC, SS_SRCA_470R, 12);
      vB = readNodeV(pB, 4);
      vC = readNodeV(pC, 4);
      vDropC = readDiffSource(pC, CAL_SRCA, vLiveA, 4);
      if (vDropC < 0.0f) vDropC = fmaxf(0.0f, vLiveA - vC);
      ib_uA = ((vLiveB - vB) / pathResToSource(pB, SS_SRCB_100K)) * 1e6f;
      ic_uA = (vDropC / pathResToSource(pC, SS_SRCA_470R)) * 1e6f;
    }
  } else {
    setSourcesAB(1.65f, 0.00f, true);
    float vLiveB = readLiveRefV(CAL_SRCB, 3);
    applyRoleMatrix(pE, SS_SRCA_470R, pB, SS_SRCB_2M2, pC, SS_GND_470R, 12);
    float vB = readNodeV(pB, 4);
    float vC = readNodeV(pC, 4);
    float vE = readNodeV(pE, 4);
    ib_uA = ((vB - vLiveB) / pathResToSource(pB, SS_SRCB_2M2)) * 1e6f;
    ic_uA = (fmaxf(0.0f, vC) / pathResToGnd(pC, SS_GND_470R)) * 1e6f;
    if ((ic_uA - iceo) < 20.0f || (vE - vC) < 0.25f) {
      setSourcesAB(1.65f, 0.50f, true);
      vLiveB = readLiveRefV(CAL_SRCB, 3);
      applyRoleMatrix(pE, SS_SRCA_470R, pB, SS_SRCB_100K, pC, SS_GND_470R, 12);
      vB = readNodeV(pB, 4);
      vC = readNodeV(pC, 4);
      ib_uA = ((vB - vLiveB) / pathResToSource(pB, SS_SRCB_100K)) * 1e6f;
      ic_uA = (fmaxf(0.0f, vC) / pathResToGnd(pC, SS_GND_470R)) * 1e6f;
    }
  }

  allSafeHiZ();
  if (ib_uA < 0.1f || ic_uA <= iceo + 0.5f) return -1.0f;
  return (ic_uA - iceo) / ib_uA;
}

static BjtResult characterizeBJT(int p1, int pB, int p2, bool isPNP) {
  BjtResult res;
  res.isPNP = isPNP;
  res.pinB  = pB;
  res.tempC = getTempC();

  float score1 = evalBjtOrientation1V5(p1, pB, p2, isPNP);
  float score2 = evalBjtOrientation1V5(p2, pB, p1, isPNP);
  if (score1 < 0.0f && score2 < 0.0f) return res;

  if (score1 >= score2) { res.pinE = p1; res.pinC = p2; }
  else                  { res.pinE = p2; res.pinC = p1; }

  const int pE = res.pinE;
  const int pC = res.pinC;
  float vTestCC = 9.00f, vDummy = 0.0f;

  res.iceo_uA = measureBjtLeakageAuto(pB, pE, pC, isPNP, vTestCC, vDummy, res.peakPower_mW);
  res.icbo_uA = measureBjtLeakageAuto(pE, pB, pC, isPNP, vTestCC, vDummy, res.peakPower_mW);

  struct HfePoint { PinModeSS bMode; PinModeSS cMode; float vCC; float vBB; };
  const HfePoint npnPlan[4] = {
    {SS_SRCB_2M2,  SS_SRCA_2K49, 9.00f, 9.00f},
    {SS_SRCB_2M2,  SS_SRCA_470R, 9.00f, 9.00f},
    {SS_SRCB_100K, SS_SRCA_470R, 9.00f, 3.80f},
    {SS_SRCB_2M2,  SS_SRCA_470R, 5.00f, 5.00f}
  };
  const HfePoint pnpPlan[4] = {
    {SS_SRCB_2M2,  SS_GND_2K49, 9.00f, 0.00f},
    {SS_SRCB_2M2,  SS_GND_470R, 9.00f, 0.00f},
    {SS_SRCB_100K, SS_GND_470R, 9.00f, 5.20f},
    {SS_SRCB_2M2,  SS_GND_470R, 5.00f, 0.00f}
  };

  float vbeLowI_mV = NAN;
  for (int idx = 0; idx < 4; idx++) {
    HfePoint pt = isPNP ? pnpPlan[idx] : npnPlan[idx];
    float iceoMatched = 0.0f;
    if (!isPNP) {
      setSourcesAB(pt.vCC, 0.0f, true);
      float vLiveA = readLiveRefV(CAL_SRCA, 4);
      applyRoleMatrix(pE, SS_GND_HARD, pB, SS_HIZ, pC, pt.cMode, 15);
      float vDrop0 = readDiffSource(pC, CAL_SRCA, vLiveA, 6);
      if (vDrop0 < 0.0f) vDrop0 = fmaxf(0.0f, vLiveA - readNodeV(pC, 4));
      iceoMatched = (vDrop0 / pathResToSource(pC, pt.cMode)) * 1e6f;
    } else {
      PinModeSS eMode = (pt.vCC >= 8.70f) ? SS_9V_REF : SS_SRCA_470R;
      setSourcesAB((eMode == SS_SRCA_470R) ? pt.vCC : 0.0f, 0.0f, true);
      applyRoleMatrix(pE, eMode, pB, SS_HIZ, pC, pt.cMode, 15);
      iceoMatched = (fmaxf(0.0f, readNodeV(pC, 6)) / pathResToGnd(pC, pt.cMode)) * 1e6f;
    }

    float ic_uA = 0.0f, ib_uA = 0.0f, vce = 0.0f, vbe_mV = 0.0f;
    if (!isPNP) {
      setSourcesAB(pt.vCC, pt.vBB, true);
      float vLiveA = readLiveRefV(CAL_SRCA, 4);
      float vLiveB = readLiveRefV(CAL_SRCB, 4);
      applyRoleMatrix(pE, SS_GND_HARD, pB, pt.bMode, pC, pt.cMode, 15);
      float vB = readNodeV(pB, 6);
      float vC = readNodeV(pC, 6);
      float vE = readNodeV(pE, 6);
      float vDropC = readDiffSource(pC, CAL_SRCA, vLiveA, 6);
      if (vDropC < 0.0f) vDropC = fmaxf(0.0f, vLiveA - vC);
      ib_uA  = ((vLiveB - vB) / pathResToSource(pB, pt.bMode)) * 1e6f;
      ic_uA  = (vDropC / pathResToSource(pC, pt.cMode)) * 1e6f;
      vce    = vC - vE;
      vbe_mV = (vB - vE) * 1000.0f;
    } else {
      PinModeSS eMode = (pt.vCC >= 8.70f) ? SS_9V_REF : SS_SRCA_470R;
      setSourcesAB((eMode == SS_SRCA_470R) ? pt.vCC : 0.0f, pt.vBB, true);
      float vLiveB = readLiveRefV(CAL_SRCB, 4);
      applyRoleMatrix(pE, eMode, pB, pt.bMode, pC, pt.cMode, 15);
      float vB = readNodeV(pB, 6);
      float vC = readNodeV(pC, 6);
      float vE = readNodeV(pE, 6);
      ib_uA  = ((vB - vLiveB) / pathResToSource(pB, pt.bMode)) * 1e6f;
      ic_uA  = (fmaxf(0.0f, vC) / pathResToGnd(pC, pt.cMode)) * 1e6f;
      vce    = vE - vC;
      vbe_mV = (vE - vB) * 1000.0f;
    }
    allSafeHiZ();

    if (pt.bMode == SS_SRCB_2M2 && isnan(vbeLowI_mV) && vbe_mV > 40.0f) {
      vbeLowI_mV = vbe_mV;
    }

    if (!checkPowerSafety(vce, ic_uA * 1e-6f, res.peakPower_mW)) {
      res.powerClamped = true;
      continue;
    }

    float deltaIc = ic_uA - iceoMatched;
    if (ib_uA > 0.2f && deltaIc > 2.0f) {
      float hfeApp = ic_uA / ib_uA;
      float hfeT   = deltaIc / ib_uA;
      if (hfeT > 1.5f && hfeT < 5000.0f) {
        res.hfeApparent = hfeApp;
        res.hfeTrue     = hfeT;
        res.vbe_mV      = vbe_mV;
        res.vce_V       = vce;
        res.ic_mA       = ic_uA * 0.001f;
        res.ib_uA       = ib_uA;
        res.valid       = true;
        if (vce > 2.50f && (deltaIc >= 40.0f || idx >= 2)) break;
      }
    }
  }

  if (res.valid) {
    if (!isPNP) {
      float vA = 2.20f, vB = 8.40f;
      for (int it = 0; it < 2; it++) {
        setSourcesAB(vA, vB, true);
        float vLiveA = readLiveRefV(CAL_SRCA, 3);
        float vLiveB = readLiveRefV(CAL_SRCB, 3);
        applyRoleMatrix(pE, SS_GND_HARD, pB, SS_SRCB_100K, pC, SS_SRCA_2K49, 12);
        float vB_m = readNodeV(pB, 4);
        float vC_m = readNodeV(pC, 4);
        float vE_m = readNodeV(pE, 4);
        float ic = ((vLiveA - vC_m) / pathResToSource(pC, SS_SRCA_2K49));
        float ib = ((vLiveB - vB_m) / pathResToSource(pB, SS_SRCB_100K));
        res.vceSat_mV = (vC_m - vE_m) * 1000.0f;
        if (ic > 1e-4f) {
          float targetIb = ic / 10.0f;
          vB = constrain(vB_m + targetIb * pathResToSource(pB, SS_SRCB_100K), 0.2f, 9.10f);
        }
        (void)ib;
      }
    } else {
      float vA = 7.40f;
      for (int it = 0; it < 2; it++) {
        setSourcesAB(vA, 0.0f, true);
        float vLiveA = readLiveRefV(CAL_SRCA, 3);
        applyRoleMatrix(pE, SS_9V_REF, pB, SS_SRCA_2K49, pC, SS_GND_2K49, 12);
        float vB_m = readNodeV(pB, 4);
        float vC_m = readNodeV(pC, 4);
        float vE_m = readNodeV(pE, 4);
        float ic = fmaxf(0.0f, vC_m) / pathResToGnd(pC, SS_GND_2K49);
        res.vceSat_mV = (vE_m - vC_m) * 1000.0f;
        if (ic > 1e-4f) {
          float targetIb = ic / 10.0f;
          vA = constrain(vB_m - targetIb * pathResToSource(pB, SS_SRCA_2K49), 0.5f, 8.80f);
          (void)vLiveA;
        }
      }
    }
    allSafeHiZ();
  }

  if (res.valid) {
    float vbeRef = !isnan(vbeLowI_mV) ? vbeLowI_mV : res.vbe_mV;
    float dT = isnan(res.tempC) ? 0.0f : (res.tempC - 25.0f);
    float vbe25_mV = vbeRef + 2.1f * dT;
    float icbo25_uA = res.icbo_uA * powf(2.0f, -dT / 10.0f);

    if (vbe25_mV > 40.0f && vbe25_mV < 430.0f) {
      res.isGermanium = true;
    } else if (vbe25_mV >= 430.0f && vbe25_mV < 465.0f && icbo25_uA > 0.8f) {
      res.isGermanium = true;
    } else {
      res.isGermanium = false;
    }
    res.leaky = (!res.isGermanium && (icbo25_uA > 5.0f || res.iceo_uA > 10.0f));
  }

  return res;
}

// ======================= STAGE 2: JFET CHARACTERIZER =========================
static JfetResult characterizeJFET(int p1, int pG, int p2, bool isP) {
  JfetResult res;
  res.isP = isP;
  res.pinG = pG;
  res.tempC = getTempC();

  auto measureIdssAndGm = [&](int pD, int pS, float &idssOut_mA, float &gmOut_mS) {
    idssOut_mA = 0.0f;
    gmOut_mS   = 0.0f;
    float peakMw = 0.0f;

    if (!isP) {
      PinModeSS dMode = SS_SRCA_470R;
      float vA = 5.50f, vB = 0.10f;
      for (int it = 0; it < 3; it++) {
        setSourcesAB(vA, vB, true);
        applyRoleMatrix(pD, dMode, pG, SS_SRCB_100K, pS, SS_GND_HARD, 12);
        vB = constrain(readNodeV(pS, 4), 0.0f, 8.5f);
      }
      float vLiveA = readLiveRefV(CAL_SRCA, 4);
      float vD = readNodeV(pD, 6);
      float vS = readNodeV(pS, 6);
      float vDrop = readDiffSource(pD, CAL_SRCA, vLiveA, 6);
      if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveA - vD);
      float i0_mA = (vDrop / pathResToSource(pD, dMode)) * 1e3f;
      if (!checkPowerSafety(vD - vS, i0_mA * 1e-3f, peakMw)) {
        dMode = SS_SRCA_2K49;
        setSourcesAB(5.00f, 0.05f, true);
        for (int it = 0; it < 2; it++) {
          applyRoleMatrix(pD, dMode, pG, SS_SRCB_100K, pS, SS_GND_HARD, 10);
          vB = constrain(readNodeV(pS, 3), 0.0f, 8.5f);
          setSourceB(vB, true);
        }
        vLiveA = readLiveRefV(CAL_SRCA, 4);
        vD = readNodeV(pD, 6);
        vS = readNodeV(pS, 6);
        vDrop = readDiffSource(pD, CAL_SRCA, vLiveA, 6);
        i0_mA = (vDrop / pathResToSource(pD, dMode)) * 1e3f;
      }
      idssOut_mA = i0_mA;

      float dVgs = (vS >= 0.25f) ? 0.25f : 0.15f;
      float vG2 = fmaxf(0.0f, vS - dVgs);
      setSourceB(vG2, true);
      applyRoleMatrix(pD, dMode, pG, SS_SRCB_100K, pS, SS_GND_HARD, 10);
      float vD2 = readNodeV(pD, 4);
      float vS2 = readNodeV(pS, 4);
      float vDrop2 = readDiffSource(pD, CAL_SRCA, vLiveA, 4);
      if (vDrop2 < 0.0f) vDrop2 = fmaxf(0.0f, vLiveA - vD2);
      float i2_mA = (vDrop2 / pathResToSource(pD, dMode)) * 1e3f;
      float actualDvgs = (vS2 - vG2);
      if (actualDvgs > 0.05f && i0_mA > i2_mA) {
        gmOut_mS = (i0_mA - i2_mA) / actualDvgs;
      }
    } else {
      float vA = 6.00f, vB = 5.50f;
      for (int it = 0; it < 3; it++) {
        setSourcesAB(vA, vB, true);
        applyRoleMatrix(pS, SS_SRCA_470R, pG, SS_SRCB_100K, pD, SS_GND_2K49, 12);
        vB = constrain(readNodeV(pS, 4), 0.0f, 9.0f);
      }
      float vS = readNodeV(pS, 6);
      float vD = readNodeV(pD, 6);
      float i0_mA = (fmaxf(0.0f, vD) / pathResToGnd(pD, SS_GND_2K49)) * 1e3f;
      idssOut_mA = i0_mA;

      setSourceB(constrain(vS + 0.25f, 0.0f, 9.1f), true);
      applyRoleMatrix(pS, SS_SRCA_470R, pG, SS_SRCB_100K, pD, SS_GND_2K49, 10);
      float vS2 = readNodeV(pS, 4);
      float vD2 = readNodeV(pD, 4);
      float i2_mA = (fmaxf(0.0f, vD2) / pathResToGnd(pD, SS_GND_2K49)) * 1e3f;
      float actualDvgs = ((vS + 0.25f) - vS2);
      if (actualDvgs > 0.05f && i0_mA > i2_mA) {
        gmOut_mS = (i0_mA - i2_mA) / actualDvgs;
      }
    }
    allSafeHiZ();
  };

  float id1 = 0, gm1 = 0, id2 = 0, gm2 = 0;
  measureIdssAndGm(p1, p2, id1, gm1);
  measureIdssAndGm(p2, p1, id2, gm2);

  if (id1 >= id2) { res.pinD = p1; res.pinS = p2; res.idss_mA = id1; res.gm_mS = gm1; }
  else            { res.pinD = p2; res.pinS = p1; res.idss_mA = id2; res.gm_mS = gm2; }

  if (!isP) {
    setSourceA(8.50f, true);
    float lo = 0.0f, hi = 8.00f;
    for (int iter = 0; iter < 12; iter++) {
      float mid = 0.5f * (lo + hi);
      setSourceB(mid, true);
      float vLiveB = readLiveRefV(CAL_SRCB, 3);
      applyRoleMatrix(res.pinD, SS_SRCA_2K49, res.pinG, SS_GND_470R, res.pinS, SS_SRCB_100K, 10);
      float vS = readNodeV(res.pinS, 4);
      float iS_uA = ((vS - vLiveB) / pathResToSource(res.pinS, SS_SRCB_100K)) * 1e6f;
      if (iS_uA > 10.0f) lo = mid;
      else               hi = mid;
    }
    setSourceB(0.5f * (lo + hi), true);
    applyRoleMatrix(res.pinD, SS_SRCA_2K49, res.pinG, SS_GND_470R, res.pinS, SS_SRCB_100K, 10);
    res.vgsoff_V = readNodeV(res.pinG, 6) - readNodeV(res.pinS, 6);
  } else {
    setSourceA(8.50f, true);
    float lo = 0.0f, hi = 8.50f;
    for (int iter = 0; iter < 12; iter++) {
      float mid = 0.5f * (lo + hi);
      setSourceB(mid, true);
      float vLiveB = readLiveRefV(CAL_SRCB, 3);
      applyRoleMatrix(res.pinG, SS_SRCA_470R, res.pinD, SS_GND_2K49, res.pinS, SS_SRCB_100K, 10);
      float vS = readNodeV(res.pinS, 4);
      float iS_uA = ((vLiveB - vS) / pathResToSource(res.pinS, SS_SRCB_100K)) * 1e6f;
      if (iS_uA > 10.0f) hi = mid;
      else               lo = mid;
    }
    setSourceB(0.5f * (lo + hi), true);
    applyRoleMatrix(res.pinG, SS_SRCA_470R, res.pinD, SS_GND_2K49, res.pinS, SS_SRCB_100K, 10);
    res.vgsoff_V = readNodeV(res.pinG, 6) - readNodeV(res.pinS, 6);
  }

  setSourcesAB(3.00f, 0.0f, true);
  if (!isP) {
    applyRoleMatrix(res.pinG, SS_SRCA_2K49, res.pinS, SS_GND_HARD, res.pinD, SS_HIZ, 10);
    res.vfGate_mV = (readNodeV(res.pinG, 4) - readNodeV(res.pinS, 4)) * 1000.0f;
  } else {
    applyRoleMatrix(res.pinS, SS_SRCA_2K49, res.pinG, SS_GND_HARD, res.pinD, SS_HIZ, 10);
    res.vfGate_mV = (readNodeV(res.pinS, 4) - readNodeV(res.pinG, 4)) * 1000.0f;
  }

  allSafeHiZ();
  res.valid = (res.idss_mA > 0.02f);
  return res;
}

// ======================= STAGE 2: MOSFET CHARACTERIZER =======================
static MosfetResult characterizeMOSFET(int pG, int pD, int pS, bool isP) {
  MosfetResult res;
  res.isP = isP;
  res.pinG = pG; res.pinD = pD; res.pinS = pS;
  res.tempC = getTempC();

  setSourcesAB(3.00f, 0.0f, true);
  if (!isP) {
    applyRoleMatrix(pS, SS_SRCA_2K49, pD, SS_GND_HARD, pG, SS_GND_HARD, 10);
    res.vfBody_mV = (readNodeV(pS, 4) - readNodeV(pD, 4)) * 1000.0f;
  } else {
    applyRoleMatrix(pD, SS_SRCA_2K49, pS, SS_GND_HARD, pG, SS_SRCA_2K49, 10);
    res.vfBody_mV = (readNodeV(pD, 4) - readNodeV(pS, 4)) * 1000.0f;
  }

  if (!isP) {
    setSourceA(4.00f, true);
    float vLiveA = readLiveRefV(CAL_SRCA, 3);
    float lo = 0.0f, hi = 8.50f;
    for (int iter = 0; iter < 13; iter++) {
      float mid = 0.5f * (lo + hi);
      setSourceB(mid, true);
      applyRoleMatrix(pD, SS_SRCA_2K49, pS, SS_GND_HARD, pG, SS_SRCB_100K, 8);
      float vDrop = readDiffSource(pD, CAL_SRCA, vLiveA, 3);
      if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveA - readNodeV(pD, 3));
      float id_uA = (vDrop / pathResToSource(pD, SS_SRCA_2K49)) * 1e6f;
      if (id_uA >= 250.0f) hi = mid;
      else                 lo = mid;
    }
    setSourceB(hi, true);
    applyRoleMatrix(pD, SS_SRCA_2K49, pS, SS_GND_HARD, pG, SS_SRCB_100K, 10);
    res.vth_V = readNodeV(pG, 6) - readNodeV(pS, 6);
  } else {
    setSourceA(8.80f, true);
    float vLiveA = readLiveRefV(CAL_SRCA, 3);
    float lo = 0.0f, hi = 8.80f;
    for (int iter = 0; iter < 13; iter++) {
      float mid = 0.5f * (lo + hi);
      setSourceB(mid, true);
      applyRoleMatrix(pS, SS_SRCA_2K49, pD, SS_GND_HARD, pG, SS_SRCB_100K, 8);
      float vDrop = readDiffSource(pS, CAL_SRCA, vLiveA, 3);
      if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveA - readNodeV(pS, 3));
      float is_uA = (vDrop / pathResToSource(pS, SS_SRCA_2K49)) * 1e6f;
      if (is_uA >= 250.0f) lo = mid;
      else                 hi = mid;
    }
    setSourceB(lo, true);
    applyRoleMatrix(pS, SS_SRCA_2K49, pD, SS_GND_HARD, pG, SS_SRCB_100K, 10);
    res.vth_V = readNodeV(pG, 6) - readNodeV(pS, 6);
  }

  auto measureRdsAtVgs = [&](float targetAbsVgs) -> float {
    if (!isP) {
      float vA = 2.00f;
      float vB = targetAbsVgs + 0.35f;
      for (int it = 0; it < 2; it++) {
        setSourcesAB(vA, constrain(vB, 0.0f, DAC_MAX_V), true);
        applyRoleMatrix(pD, SS_SRCA_470R, pS, SS_GND_HARD, pG, SS_SRCB_100K, 10);
        vB = readNodeV(pS, 4) + targetAbsVgs;
      }
      setSourceB(constrain(vB, 0.0f, DAC_MAX_V), true);
      float vLiveA = readLiveRefV(CAL_SRCA, 4);
      applyRoleMatrix(pD, SS_SRCA_470R, pS, SS_GND_HARD, pG, SS_SRCB_100K, 10);
      float vD = readNodeV(pD, 12);
      float vS = readNodeV(pS, 12);
      float vDrop = readDiffSource(pD, CAL_SRCA, vLiveA, 8);
      if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveA - vD);
      float iD = vDrop / pathResToSource(pD, SS_SRCA_470R);
      if (iD < 50e-6f || (vD - vS) > 1.8f) return NAN;
      return fmaxf(0.0f, (vD - vS) / iD);
    } else {
      float vS_est = cal.v9Lim_V - 0.45f;
      float vB = constrain(vS_est - targetAbsVgs, 0.0f, DAC_MAX_V);
      setSourcesAB(0.0f, vB, true);
      applyRoleMatrix(pS, SS_9V_REF, pD, SS_GND_2K49, pG, SS_SRCB_100K, 10);
      float vS = readNodeV(pS, 12);
      vB = constrain(vS - targetAbsVgs, 0.0f, DAC_MAX_V);
      setSourceB(vB, true);
      applyRoleMatrix(pS, SS_9V_REF, pD, SS_GND_2K49, pG, SS_SRCB_100K, 10);
      vS = readNodeV(pS, 12);
      float vD = readNodeV(pD, 12);
      float iD = fmaxf(0.0f, vD) / pathResToGnd(pD, SS_GND_2K49);
      if (iD < 50e-6f || (vS - vD) > 2.5f) return NAN;
      return fmaxf(0.0f, (vS - vD) / iD);
    }
  };

  res.rds4v5_ohm = measureRdsAtVgs(4.50f);
  res.rds8v5_ohm = measureRdsAtVgs(8.50f);

  setSourcesAB(0.0f, 0.0f, false);
  setMatrix(SS_GND_HARD, SS_GND_HARD, SS_GND_HARD, true);
  delay(12);
  allSafeHiZ();

  res.valid = !isnan(res.vth_V);
  return res;
}

// ======================= STAGE 2: DIODE / LED / ZENER ========================
static DiodeResult characterizeDiode(int pA, int pK, bool kIsCom = false) {
  DiodeResult res;
  res.pinA = pA; res.pinK = kIsCom ? -1 : pK;
  res.tempC = getTempC();
  int pTh = kIsCom ? -1 : (3 - pA - pK);

  auto setDiodeDrive = [&](PinModeSS aMode, uint16_t settleMs) {
    if (kIsCom) {
      PinModeSS arr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
      arr[pA] = aMode;
      setMatrix(arr[0], arr[1], arr[2], true);
      if (settleMs > 0) delay(settleMs);
    } else {
      applyRoleMatrix(pA, aMode, pK, SS_GND_HARD, pTh, SS_HIZ, settleMs);
    }
  };

  auto measureVfAtCurrent = [&](PinModeSS aMode, float targetI_mA) -> float {
    float rPath = pathResToSource(pA, aMode) + (kIsCom ? 0.0f : pathResToGnd(pK, SS_GND_HARD));
    float vA = 0.6f + (targetI_mA * 1e-3f) * rPath;
    for (int it = 0; it < 3; it++) {
      setSourceA(constrain(vA, 0.05f, DAC_MAX_V), true);
      setDiodeDrive(aMode, 10);
      float vP = readNodeV(pA, 4);
      float vN = kIsCom ? 0.0f : readNodeV(pK, 4);
      float vF = vP - vN;
      vA = vF + (targetI_mA * 1e-3f) * rPath;
    }
    setSourceA(constrain(vA, 0.05f, DAC_MAX_V), true);
    setDiodeDrive(aMode, 10);
    float vN = kIsCom ? 0.0f : readNodeV(pK, 8);
    return (readNodeV(pA, 8) - vN) * 1000.0f;
  };

  res.vf0mA1_mV = measureVfAtCurrent(SS_SRCA_2K49, 0.10f);
  res.vf2mA_mV  = measureVfAtCurrent(SS_SRCA_470R, 2.00f);
  res.isLED     = (res.vf2mA_mV > 1100.0f);

  if (!kIsCom) {
    float vRevTest = res.isLED ? 3.00f : 5.00f;
    setSourcesAB(0.0f, vRevTest, true);
    float vLiveB = readLiveRefV(CAL_SRCB, 4);
    applyRoleMatrix(pK, SS_SRCB_100K, pA, SS_GND_HARD, pTh, SS_HIZ, 12);
    float vDropRev = readDiffSource(pK, CAL_SRCB, vLiveB, 6);
    if (vDropRev < 0.0f) vDropRev = fmaxf(0.0f, vLiveB - readNodeV(pK, 4));
    res.irev_uA = (vDropRev / pathResToSource(pK, SS_SRCB_100K)) * 1e6f;

    float vRevDut = readNodeV(pK, 4) - readNodeV(pA, 4);
    if (res.irev_uA > 20.0f && vRevDut < 1.15f) {
      res.isDual = true;
    }

    if (!res.isLED && !res.isDual) {
      float peakMw = 0.0f;
      for (float vStep = 2.0f; vStep <= 8.55f; vStep += 0.50f) {
        setSourcesAB(vStep, 0.0f, true);
        float vLiveA = readLiveRefV(CAL_SRCA, 3);
        applyRoleMatrix(pK, SS_SRCA_2K49, pA, SS_GND_HARD, pTh, SS_HIZ, 8);
        float vK_m = readNodeV(pK, 4);
        float vA_m = readNodeV(pA, 4);
        float vDrop = readDiffSource(pK, CAL_SRCA, vLiveA, 4);
        if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveA - vK_m);
        float i_mA = (vDrop / pathResToSource(pK, SS_SRCA_2K49)) * 1e3f;
        float vZ = vK_m - vA_m;
        if (!checkPowerSafety(vZ, i_mA * 1e-3f, peakMw)) break;

        if (i_mA >= 0.85f && vZ > 1.50f) {
          float r2k = pathResToSource(pK, SS_SRCA_2K49) + pathResToGnd(pA, SS_GND_HARD);
          float v1mA = constrain(vZ + 0.001f * r2k, 2.0f, 8.8f);
          setSourceA(v1mA, true);
          vLiveA = readLiveRefV(CAL_SRCA, 4);
          applyRoleMatrix(pK, SS_SRCA_2K49, pA, SS_GND_HARD, pTh, SS_HIZ, 10);
          vK_m = readNodeV(pK, 8);
          vA_m = readNodeV(pA, 8);
          vDrop = readDiffSource(pK, CAL_SRCA, vLiveA, 6);
          if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLiveA - vK_m);
          res.vz_V    = vK_m - vA_m;
          res.iz_mA   = (vDrop / pathResToSource(pK, SS_SRCA_2K49)) * 1e3f;
          res.isZener = true;
          break;
        }
      }
    }
  } else {
    res.irev_uA = 0.0f;
  }

  allSafeHiZ();
  res.valid = (res.vf2mA_mV > 40.0f && res.vf2mA_mV < 5000.0f);
  return res;
}

// ======================= STAGE 2: RESISTOR CHARACTERIZER =====================
static ResistorResult characterizeResistor(int pA, int pB, bool bIsCom = false) {
  ResistorResult res;
  res.pinA = pA; res.pinB = bIsCom ? -1 : pB;
  res.tempC = getTempC();
  int pTh = bIsCom ? -1 : (3 - pA - pB);

  auto measureWithMode = [&](PinModeSS hiMode, float vSrc, float &rOut,
                             float &vDutOut, float &i_mA_out) -> bool {
    bool isSrcB = (hiMode == SS_SRCB_100K || hiMode == SS_SRCB_2M2);
    setSourcesAB(isSrcB ? 0.0f : vSrc, isSrcB ? vSrc : 0.0f, true);
    CalMuxCh calCh = isSrcB ? CAL_SRCB : CAL_SRCA;
    float vLive = readLiveRefV(calCh, 4);

    if (bIsCom) {
      PinModeSS arr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
      arr[pA] = hiMode;
      setMatrix(arr[0], arr[1], arr[2], true);
      delay(12);
    } else {
      applyRoleMatrix(pA, hiMode, pB, SS_GND_HARD, pTh, SS_HIZ, 12);
    }

    float vHi = readNodeV(pA, 6);
    float vLo = bIsCom ? 0.0f : readNodeV(pB, 6);
    float vDrop = readDiffSource(pA, calCh, vLive, 8);
    if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLive - vHi);

    float i_A = vDrop / pathResToSource(pA, hiMode);
    if (i_A < 1e-8f) return false;
    vDutOut  = vHi - vLo;
    i_mA_out = i_A * 1e3f;
    rOut     = fmaxf(0.0f, vDutOut / i_A);
    return true;
  };

  float rEst = 0.0f, vDut = 0.0f, i_mA = 0.0f;
  if (!measureWithMode(SS_SRCB_100K, 3.60f, rEst, vDut, i_mA)) {
    if (!measureWithMode(SS_SRCB_2M2, 8.00f, rEst, vDut, i_mA)) {
      allSafeHiZ();
      return res;
    }
    res.range_ohm = R_KEEN_BASE;
  } else if (rEst < 1200.0f) {
    measureWithMode(SS_SRCA_470R, 2.00f, rEst, vDut, i_mA);
    res.range_ohm = R_LOW;
  } else if (rEst < 22000.0f) {
    measureWithMode(SS_SRCA_2K49, 3.00f, rEst, vDut, i_mA);
    res.range_ohm = R_KEEN;
  } else if (rEst < 450000.0f) {
    measureWithMode(SS_SRCB_100K, 5.00f, rEst, vDut, i_mA);
    res.range_ohm = R_MID;
  } else {
    measureWithMode(SS_SRCB_2M2, 8.00f, rEst, vDut, i_mA);
    res.range_ohm = R_KEEN_BASE;
  }

  allSafeHiZ();
  res.r_ohm  = rEst;
  res.vDut_V = vDut;
  res.i_mA   = i_mA;
  res.valid  = (rEst < 50.0e6f && i_mA > 0.0001f);
  return res;
}

// ======================= STAGE 2: CAPACITOR CHARACTERIZER ====================
static CapResult characterizeCapacitor(int pA, int pB, bool bIsCom = false) {
  CapResult res;
  res.pinA = pA; res.pinB = bIsCom ? -1 : pB;
  res.tempC = getTempC();
  int pTh = bIsCom ? -1 : (3 - pA - pB);

  const float vStep = 2.200f;
  auto dischargeCap = [&]() {
    setSourcesAB(0.0f, 0.0f, false);
    PinModeSS arr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
    arr[pA] = SS_GND_2K49;
    if (!bIsCom) arr[pB] = SS_GND_HARD;
    setMatrix(arr[0], arr[1], arr[2], true);
    delay(25);
    arr[pA] = SS_GND_470R;
    setMatrix(arr[0], arr[1], arr[2], true);
    delay(25);
    arr[pA] = SS_GND_HARD;
    setMatrix(arr[0], arr[1], arr[2], true);
    delay(15);
    uint32_t t0 = millis();
    while ((uint32_t)(millis() - t0) < 1500) {
      float vB = bIsCom ? 0.0f : readNodeV(pB, 1);
      if (fabsf(readNodeV(pA, 1) - vB) < 0.008f) break;
      delay(20);
    }
  };

  dischargeCap();

  auto measureWithRange = [&](PinModeSS hiMode, bool useFastAdc1,
                              uint32_t timeoutMs, float &cOut_F) -> bool {
    bool isSrcB = (hiMode == SS_SRCB_100K || hiMode == SS_SRCB_2M2);
    setSourcesAB(isSrcB ? 0.0f : vStep, isSrcB ? vStep : 0.0f, true);
    float vSrcActual = readLiveRefV(isSrcB ? CAL_SRCB : CAL_SRCA, 4);
    if (vSrcActual < 1.5f) vSrcActual = vStep;

    float v1Target = 0.25f * vSrcActual;
    float v2Target = 0.70f * vSrcActual;
    float rTot = pathResToSource(pA, hiMode) +
                 (bIsCom ? 0.0f : pathResToGnd(pB, SS_GND_HARD));

    PinModeSS chg[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
    chg[pA] = hiMode;
    if (!bIsCom) chg[pB] = SS_GND_HARD;
    uint16_t chgWord = 0;
    buildMatrixWord(chg[0], chg[1], chg[2], true, chgWord);

    if (useFastAdc1) {
      setMatrixRaw16(chgWord, true);
      int64_t tStart = esp_timer_get_time();
      int64_t t1 = -1, t2 = -1;
      float v1 = 0.0f, v2 = 0.0f;

      while ((uint32_t)((esp_timer_get_time() - tStart) / 1000LL) < timeoutMs) {
        int64_t tNow = esp_timer_get_time();
        float vNow = readFastAdcCalV(pA);
        if (t1 < 0 && vNow >= v1Target && vNow < v2Target) { t1 = tNow; v1 = vNow; }
        if (t1 >= 0 && vNow >= v2Target) { t2 = tNow; v2 = vNow; break; }
        delayMicroseconds(25);
      }
      if (t1 < 0 || t2 <= t1) return false;
      float denom = logf((vSrcActual - v1) / fmaxf(0.05f, vSrcActual - v2));
      if (denom <= 0.01f) return false;
      float tau_s = ((float)(t2 - t1) * 1e-6f) / denom;
      cOut_F = tau_s / rTot;
      return true;
    } else {
      ads.setDataRate(RATE_ADS1115_250SPS);
      setMatrixRaw16(chgWord, true);
      int64_t tStart = esp_timer_get_time();
      int64_t t1 = -1, t2 = -1;
      float v1 = 0.0f, v2 = 0.0f;

      while ((uint32_t)((esp_timer_get_time() - tStart) / 1000LL) < timeoutMs) {
        float vB = bIsCom ? 0.0f : readNodeV(pB, 1);
        float vNow = readNodeV(pA, 1) - vB;
        int64_t tNow = esp_timer_get_time();
        if (t1 < 0 && vNow >= 0.08f * vSrcActual) {
          t1 = tNow; v1 = vNow;
        }
        if (t1 >= 0) {
          t2 = tNow; v2 = vNow;
          if (vNow >= 0.65f * vSrcActual) break;
        }
      }
      ads.setDataRate(RATE_ADS1115_128SPS);
      if (t1 < 0 || t2 <= t1 || (v2 - v1) < 0.08f) return false;
      float denom = logf((vSrcActual - v1) / fmaxf(0.05f, vSrcActual - v2));
      if (denom <= 0.02f) return false;
      float tau_s = ((float)(t2 - t1) * 1e-6f) / denom;
      cOut_F = tau_s / rTot;
      return true;
    }
  };

  float c_F = NAN;
  bool found = false;

  float cPfFast = measureFastCapPf_2M2(pA, pB, pTh, bIsCom);
  if (!isnan(cPfFast) && cPfFast < 6000.0f) {
    float netPf = cPfFast - cal.cStray_pF[pA];
    if (netPf >= 4.0f) {
      c_F = netPf * 1e-12f;
      found = true;
    }
  }

  if (!found) {
    dischargeCap();
    if (measureWithRange(SS_SRCB_100K, true, 55, c_F)) {
      c_F = fmaxf(0.0f, c_F - cal.cStray_pF[pA] * 1e-12f);
      found = true;
    }
  }

  if (!found) {
    dischargeCap();
    if (measureWithRange(SS_SRCA_2K49, true, 55, c_F)) found = true;
  }

  if (!found) {
    dischargeCap();
    if (measureWithRange(SS_SRCA_470R, false, 2600, c_F)) found = true;
  }

  if (found && c_F > 1e-9f) {
    setSourcesAB(2.00f, 0.0f, true);
    PinModeSS chgArr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
    chgArr[pA] = SS_SRCA_470R;
    if (!bIsCom) chgArr[pB] = SS_GND_HARD;
    setMatrix(chgArr[0], chgArr[1], chgArr[2], true);
    delay(40);
    chgArr[pA] = SS_HIZ;
    setMatrix(chgArr[0], chgArr[1], chgArr[2], true);
    delay(10);
    float v0 = readNodeV(pA, 2) - (bIsCom ? 0.0f : readNodeV(pB, 2));
    delay(250);
    float v1 = readNodeV(pA, 2) - (bIsCom ? 0.0f : readNodeV(pB, 2));
    float dV = fmaxf(0.0f, v0 - v1);
    res.droop_mVs = (dV / 0.250f) * 1000.0f;
    res.ileak_uA  = c_F * (dV / 0.250f) * 1e6f;
  } else {
    res.droop_mVs = 0.0f;
    res.ileak_uA  = 0.0f;
  }

  dischargeCap();
  allSafeHiZ();

  if (found && c_F > 4e-12f) {
    res.c_pF  = c_F * 1e12f;
    res.valid = true;
  }
  return res;
}

// ======================= JSON RESULT EMITTERS (WITH HASH + SYNC) =============
static void finalizeAndPublishScanJson(const char *typeStr, const char *summaryStr,
                                       const char *jsonStr) {
  Serial.println(jsonStr);
  pushHistoryEntry(lastScan.dutHash, typeStr, summaryStr, jsonStr);
  wifiPostAfterSafeHiZ(jsonStr);
}

static void emitBjtJson(const BjtResult &r) {
  char roles[3] = {'?', '?', '?'};
  roles[r.pinE] = 'E'; roles[r.pinB] = 'B'; roles[r.pinC] = 'C';
  char tStr[20], jsonBuf[360], sumBuf[64];
  fmtJsonFloat(tStr, sizeof(tStr), r.tempC, 2);
  snprintf(jsonBuf, sizeof(jsonBuf),
      "{\"type\":\"BJT\",\"hash\":\"%s\",\"mat\":\"%s\",\"pol\":\"%s\",\"pins\":\"1:%c 2:%c 3:%c\","
      "\"hfe_true\":%.1f,\"hfe_raw\":%.1f,\"iceo_uA\":%.2f,\"icbo_uA\":%.2f,"
      "\"vbe_mV\":%.1f,\"vcesat_mV\":%.1f,\"vce_V\":%.2f,\"ic_mA\":%.3f,"
      "\"ib_uA\":%.2f,\"p_mW\":%.2f,\"leaky\":%s,\"power_clamped\":%s,\"temp_C\":%s}",
      lastScan.dutHash, r.isGermanium ? "Ge" : "Si", r.isPNP ? "PNP" : "NPN",
      roles[0], roles[1], roles[2],
      r.hfeTrue, r.hfeApparent, r.iceo_uA, r.icbo_uA,
      r.vbe_mV, r.vceSat_mV, r.vce_V, r.ic_mA, r.ib_uA, r.peakPower_mW,
      r.leaky ? "true" : "false", r.powerClamped ? "true" : "false", tStr);
  snprintf(sumBuf, sizeof(sumBuf), "%s %s hFE=%.0f Iceo=%.0fuA",
           r.isGermanium ? "Ge" : "Si", r.isPNP ? "PNP" : "NPN", r.hfeTrue, r.iceo_uA);
  finalizeAndPublishScanJson("BJT", sumBuf, jsonBuf);
}

static void emitJfetJson(const JfetResult &r) {
  char roles[3] = {'?', '?', '?'};
  roles[r.pinD] = 'D'; roles[r.pinG] = 'G'; roles[r.pinS] = 'S';
  char tStr[20], gmStr[20], jsonBuf[360], sumBuf[64];
  fmtJsonFloat(tStr, sizeof(tStr), r.tempC, 2);
  fmtJsonFloat(gmStr, sizeof(gmStr), r.gm_mS, 2);
  snprintf(jsonBuf, sizeof(jsonBuf),
      "{\"type\":\"JFET\",\"hash\":\"%s\",\"pol\":\"%s\",\"pins\":\"1:%c 2:%c 3:%c\","
      "\"idss_mA\":%.3f,\"vgsoff_V\":%.3f,\"gm_mS\":%s,\"vf_gate_mV\":%.1f,\"temp_C\":%s}",
      lastScan.dutHash, r.isP ? "P-JFET" : "N-JFET", roles[0], roles[1], roles[2],
      r.idss_mA, r.vgsoff_V, gmStr, r.vfGate_mV, tStr);
  snprintf(sumBuf, sizeof(sumBuf), "%s Idss=%.2fmA Vgs=%.2fV",
           r.isP ? "P-JFET" : "N-JFET", r.idss_mA, r.vgsoff_V);
  finalizeAndPublishScanJson("JFET", sumBuf, jsonBuf);
}

static void emitMosfetJson(const MosfetResult &r) {
  char roles[3] = {'?', '?', '?'};
  roles[r.pinG] = 'G'; roles[r.pinD] = 'D'; roles[r.pinS] = 'S';
  char tStr[20], r45Str[20], r85Str[20], jsonBuf[360], sumBuf[64];
  fmtJsonFloat(tStr, sizeof(tStr), r.tempC, 2);
  fmtJsonFloat(r45Str, sizeof(r45Str), r.rds4v5_ohm, 3);
  fmtJsonFloat(r85Str, sizeof(r85Str), r.rds8v5_ohm, 3);
  snprintf(jsonBuf, sizeof(jsonBuf),
      "{\"type\":\"MOSFET\",\"hash\":\"%s\",\"pol\":\"%s\",\"pins\":\"1:%c 2:%c 3:%c\","
      "\"vth_V\":%.3f,\"rds_4v5_ohm\":%s,\"rds_8v5_ohm\":%s,\"vf_body_mV\":%.1f,\"temp_C\":%s}",
      lastScan.dutHash, r.isP ? "P-MOS" : "N-MOS", roles[0], roles[1], roles[2],
      r.vth_V, r45Str, r85Str, r.vfBody_mV, tStr);
  snprintf(sumBuf, sizeof(sumBuf), "%s Vth=%.2fV", r.isP ? "P-MOS" : "N-MOS", r.vth_V);
  finalizeAndPublishScanJson("MOSFET", sumBuf, jsonBuf);
}

static void emitDiodeJson(const DiodeResult &r) {
  char roles[3] = {'-', '-', '-'};
  if (r.pinA >= 0 && r.pinA < 3) roles[r.pinA] = 'A';
  if (r.pinK >= 0 && r.pinK < 3) roles[r.pinK] = 'K';
  const char *sub = r.isDual ? "DUAL" :
                    (r.isZener ? "ZENER" :
                    (r.isLED ? "LED" :
                    (r.vf2mA_mV < 420.0f ? "Ge_Schottky" : "Si")));
  char tStr[20], vzStr[20], izStr[20], jsonBuf[360], sumBuf[64];
  fmtJsonFloat(tStr, sizeof(tStr), r.tempC, 2);
  fmtJsonFloat(vzStr, sizeof(vzStr), r.vz_V, 3);
  fmtJsonFloat(izStr, sizeof(izStr), r.iz_mA, 2);
  snprintf(jsonBuf, sizeof(jsonBuf),
      "{\"type\":\"DIODE\",\"hash\":\"%s\",\"subtype\":\"%s\",\"pins\":\"1:%c 2:%c 3:%c\","
      "\"vf_0mA1_mV\":%.1f,\"vf_2mA_mV\":%.1f,\"irev_uA\":%.2f,\"vz_V\":%s,\"iz_mA\":%s,\"temp_C\":%s}",
      lastScan.dutHash, sub, roles[0], roles[1], roles[2],
      r.vf0mA1_mV, r.vf2mA_mV, r.irev_uA, vzStr, izStr, tStr);
  if (r.isZener) snprintf(sumBuf, sizeof(sumBuf), "ZENER Vz=%.2fV", r.vz_V);
  else           snprintf(sumBuf, sizeof(sumBuf), "%s Vf=%.0fmV", sub, r.vf2mA_mV);
  finalizeAndPublishScanJson("DIODE", sumBuf, jsonBuf);
}

static void emitResistorJson(const ResistorResult &r) {
  char tStr[20], jsonBuf[360], sumBuf[64];
  fmtJsonFloat(tStr, sizeof(tStr), r.tempC, 2);
  snprintf(jsonBuf, sizeof(jsonBuf),
      "{\"type\":\"RESISTOR\",\"hash\":\"%s\",\"pins\":\"TP%d-TP%d\",\"r_ohm\":%.2f,"
      "\"range_ohm\":%.0f,\"v_dut_V\":%.4f,\"i_mA\":%.4f,\"temp_C\":%s}",
      lastScan.dutHash, constrain(r.pinA, 0, 2) + 1, constrain(r.pinB, 0, 2) + 1,
      r.r_ohm, r.range_ohm, r.vDut_V, r.i_mA, tStr);
  if (r.r_ohm >= 1e6f)      snprintf(sumBuf, sizeof(sumBuf), "RES %.2f MOhm", r.r_ohm * 1e-6f);
  else if (r.r_ohm >= 1e3f) snprintf(sumBuf, sizeof(sumBuf), "RES %.2f kOhm", r.r_ohm * 1e-3f);
  else                      snprintf(sumBuf, sizeof(sumBuf), "RES %.2f Ohm", r.r_ohm);
  finalizeAndPublishScanJson("RESISTOR", sumBuf, jsonBuf);
}

static void emitCapJson(const CapResult &r) {
  char tStr[20], jsonBuf[360], sumBuf[64];
  fmtJsonFloat(tStr, sizeof(tStr), r.tempC, 2);
  snprintf(jsonBuf, sizeof(jsonBuf),
      "{\"type\":\"CAPACITOR\",\"hash\":\"%s\",\"pins\":\"TP%d-TP%d\",\"c_pF\":%.1f,"
      "\"c_nF\":%.3f,\"c_uF\":%.5f,\"ileak_uA\":%.2f,\"droop_mVs\":%.1f,\"temp_C\":%s}",
      lastScan.dutHash, constrain(r.pinA, 0, 2) + 1, constrain(r.pinB, 0, 2) + 1,
      r.c_pF, r.c_pF * 1e-3f, r.c_pF * 1e-6f, r.ileak_uA, r.droop_mVs, tStr);
  if (r.c_pF >= 1e6f)      snprintf(sumBuf, sizeof(sumBuf), "CAP %.2f uF", r.c_pF * 1e-6f);
  else if (r.c_pF >= 1e3f) snprintf(sumBuf, sizeof(sumBuf), "CAP %.2f nF", r.c_pF * 1e-3f);
  else                     snprintf(sumBuf, sizeof(sumBuf), "CAP %.1f pF", r.c_pF);
  finalizeAndPublishScanJson("CAPACITOR", sumBuf, jsonBuf);
}

// ======================= STAGE 1 -> STAGE 2 DISPATCH ENGINE ==================
static bool tryMosfetGateToggle(int pBodyA, int pBodyK, int pG, MosfetResult &outMos) {
  setSourcesAB(4.50f, 0.0f, true);
  applyRoleMatrix(pBodyK, SS_SRCA_2K49, pBodyA, SS_GND_HARD, pG, SS_SRCB_100K, 10);
  float vD_off = readNodeV(pBodyK, 3);
  setSourceB(5.50f, true);
  float vLiveB = readLiveRefV(CAL_SRCB, 3);
  applyRoleMatrix(pBodyK, SS_SRCA_2K49, pBodyA, SS_GND_HARD, pG, SS_SRCB_100K, 10);
  float vD_on = readNodeV(pBodyK, 3);
  float vG_on = readNodeV(pG, 3);
  float ig_uA = fabsf((vLiveB - vG_on) / pathResToSource(pG, SS_SRCB_100K)) * 1e6f;
  allSafeHiZ();

  if (vD_off > 3.5f && vD_on < 1.5f && ig_uA < 1.0f) {
    outMos = characterizeMOSFET(pG, pBodyK, pBodyA, false);
    return outMos.valid;
  }

  setSourcesAB(5.50f, 5.50f, true);
  applyRoleMatrix(pBodyK, SS_SRCA_2K49, pBodyA, SS_GND_HARD, pG, SS_SRCB_100K, 10);
  float vS_off = readNodeV(pBodyK, 3);
  setSourceB(0.00f, true);
  vLiveB = readLiveRefV(CAL_SRCB, 3);
  applyRoleMatrix(pBodyK, SS_SRCA_2K49, pBodyA, SS_GND_HARD, pG, SS_SRCB_100K, 10);
  float vS_on = readNodeV(pBodyK, 3);
  vG_on = readNodeV(pG, 3);
  ig_uA = fabsf((vG_on - vLiveB) / pathResToSource(pG, SS_SRCB_100K)) * 1e6f;
  setSourcesAB(0.0f, 0.0f, false);
  setMatrix(SS_GND_HARD, SS_GND_HARD, SS_GND_HARD, true);
  delay(8);
  allSafeHiZ();

  if (vS_off > 3.5f && vS_on < 2.2f && ig_uA < 1.0f) {
    outMos = characterizeMOSFET(pG, pBodyA, pBodyK, true);
    return outMos.valid;
  }
  return false;
}

static bool tryJfetPinchOff(int p1, int p2, int pG, bool isP, JfetResult &outJfet) {
  if (testCapChargeRetention(p1, p2)) return false;

  float iOn = 0.0f, iOff = 0.0f, igCheck = 100.0f;
  if (!isP) {
    setSourcesAB(8.50f, 0.0f, true);
    float vLiveA = readLiveRefV(CAL_SRCA, 3);
    applyRoleMatrix(p1, SS_SRCA_2K49, p2, SS_GND_470R, pG, SS_GND_470R, 10);
    iOn = ((vLiveA - readNodeV(p1, 4)) / pathResToSource(p1, SS_SRCA_2K49)) * 1e3f;

    setSourceB(4.50f, true);
    applyRoleMatrix(p1, SS_SRCA_2K49, p2, SS_SRCB_100K, pG, SS_GND_470R, 10);
    float vD = readNodeV(p1, 4);
    float vS = readNodeV(p2, 4);
    float vG = readNodeV(pG, 4);
    iOff = ((vLiveA - vD) / pathResToSource(p1, SS_SRCA_2K49)) * 1e3f;
    igCheck = (fabsf(vG) / pathResToGnd(pG, SS_GND_470R)) * 1e6f;
    if ((vD - vS) < 1.5f) iOff = iOn;
  } else {
    setSourcesAB(8.50f, 4.00f, true);
    float vLiveA = readLiveRefV(CAL_SRCA, 3);
    applyRoleMatrix(p2, SS_9V_REF, p1, SS_GND_2K49, pG, SS_SRCA_470R, 10);
    iOn = (readNodeV(p1, 4) / pathResToGnd(p1, SS_GND_2K49)) * 1e3f;

    applyRoleMatrix(p2, SS_SRCB_100K, p1, SS_GND_2K49, pG, SS_SRCA_470R, 10);
    float vD = readNodeV(p1, 4);
    float vS = readNodeV(p2, 4);
    float vG = readNodeV(pG, 4);
    iOff = (vD / pathResToGnd(p1, SS_GND_2K49)) * 1e3f;
    igCheck = (fabsf(vLiveA - vG) / pathResToSource(pG, SS_SRCA_470R)) * 1e6f;
    if ((vS - vD) < 1.5f) iOff = iOn;
  }
  allSafeHiZ();

  if (iOn > 0.05f && iOff < 0.35f * iOn && igCheck < 5.0f) {
    outJfet = characterizeJFET(p1, pG, p2, isP);
    return outJfet.valid;
  }
  return false;
}

static void commitScanOutcome(bool found) {
  if (found) {
    computeDutCharHash(lastScan);
    playModemScanStep(3);
  } else if (lastScan.kind == DUT_ABORT) {
    playModemScanStep(4);
  } else {
    beepTone(420, 45);
  }
  updateLedStatusFromState();
  renderScreens();
}

static void detectAndDispatchDUT() {
  if (inStandby) {
    emitStatusJson("ERROR", "Exit Standby mode before scanning DUT");
    return;
  }
  ensureRadioOffForAnalog();

  lastScan = LastScanState();
  lastSweep = SweepSummary();
  cursorCurve = -1;
  cursorPt = -1;
  powerClampedFlag = false;
  setStatusLeds(false, false, true, false, false);

  // Stage 0 + Modem Handshake Step 0
  playModemScanStep(0);
  if (!runStage0GuardAndDischarge()) {
    commitScanOutcome(false);
    return;
  }

  // Stage 1A + Modem Handshake Step 1
  playModemScanStep(1);
  PairProbe p1A[6];
  int cnt1A = 0;
  scanStage1Pairs(0.85f, p1A, cnt1A);

  auto findPair = [](const PairProbe arr[6], int a, int b) -> const PairProbe & {
    for (int k = 0; k < 6; k++) {
      if (arr[k].hi == a && arr[k].lo == b) return arr[k];
    }
    return arr[0];
  };

  // Stage 1B / Stage 2 + Modem Handshake Step 2
  playModemScanStep(2);

  if (cnt1A == 0) {
    PairProbe p1B[6];
    int cnt1B = 0;
    scanStage1Pairs(3.60f, p1B, cnt1B);

    if (cnt1B == 2) {
      for (int b = 0; b < 3; b++) {
        int o1 = (b + 1) % 3, o2 = (b + 2) % 3;
        const PairProbe &f1 = findPair(p1B, b, o1);
        const PairProbe &f2 = findPair(p1B, b, o2);
        const PairProbe &r1 = findPair(p1B, o1, b);
        const PairProbe &r2 = findPair(p1B, o2, b);

        if (f1.conducts && f2.conducts && !r1.conducts && !r2.conducts) {
          BjtResult bjt = characterizeBJT(o1, b, o2, false);
          if (bjt.valid) {
            lastScan.kind = DUT_BJT; lastScan.bjt = bjt;
            commitScanOutcome(true); emitBjtJson(bjt); return;
          }
        }
        if (r1.conducts && r2.conducts && !f1.conducts && !f2.conducts) {
          BjtResult bjt = characterizeBJT(o1, b, o2, true);
          if (bjt.valid) {
            lastScan.kind = DUT_BJT; lastScan.bjt = bjt;
            commitScanOutcome(true); emitBjtJson(bjt); return;
          }
        }
      }

      for (int k = 0; k < 6; k++) {
        if (p1B[k].conducts) {
          const PairProbe &rev = findPair(p1B, p1B[k].lo, p1B[k].hi);
          if (rev.conducts) {
            if (fabsf(p1B[k].vDut - rev.vDut) < 0.25f) {
              ResistorResult rr = characterizeResistor(p1B[k].hi, p1B[k].lo);
              if (rr.valid) {
                lastScan.kind = DUT_RESISTOR; lastScan.res = rr;
                commitScanOutcome(true); emitResistorJson(rr); return;
              }
            } else {
              DiodeResult dr = characterizeDiode(p1B[k].hi, p1B[k].lo);
              if (dr.valid) {
                lastScan.kind = DUT_DIODE; lastScan.diode = dr;
                commitScanOutcome(true); emitDiodeJson(dr); return;
              }
            }
          }
          break;
        }
      }
    } else if (cnt1B == 1) {
      for (int k = 0; k < 6; k++) {
        if (p1B[k].conducts) {
          MosfetResult mr;
          if (tryMosfetGateToggle(p1B[k].hi, p1B[k].lo, p1B[k].third, mr)) {
            lastScan.kind = DUT_MOSFET; lastScan.mos = mr;
            commitScanOutcome(true); emitMosfetJson(mr); return;
          }
          DiodeResult dr = characterizeDiode(p1B[k].hi, p1B[k].lo);
          if (dr.valid) {
            lastScan.kind = DUT_DIODE; lastScan.diode = dr;
            commitScanOutcome(true); emitDiodeJson(dr); return;
          }
          break;
        }
      }
    }

    const int capPairs[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    int bestA = -1, bestB = -1;
    float bestCpF = 0.0f;
    for (int c = 0; c < 3; c++) {
      int a = capPairs[c][0], b = capPairs[c][1];
      float cMeas = measureFastCapPf_2M2(a, b, 3 - a - b);
      float cNet = isnan(cMeas) ? 0.0f : (cMeas - cal.cStray_pF[a]);
      if (isnan(cMeas)) {
        bestA = a; bestB = b; bestCpF = 10000.0f;
        break;
      }
      if (cNet > bestCpF) {
        bestCpF = cNet; bestA = a; bestB = b;
      }
    }
    if (bestA >= 0 && bestCpF >= (cal.valid ? 5.0f : 35.0f)) {
      CapResult cr = characterizeCapacitor(bestA, bestB);
      if (cr.valid) {
        lastScan.kind = DUT_CAPACITOR; lastScan.cap = cr;
        commitScanOutcome(true); emitCapJson(cr); return;
      }
    }

    for (int c = 0; c < 3; c++) {
      ResistorResult rr = characterizeResistor(capPairs[c][0], capPairs[c][1]);
      if (rr.valid && rr.r_ohm < 40.0e6f) {
        lastScan.kind = DUT_RESISTOR; lastScan.res = rr;
        commitScanOutcome(true); emitResistorJson(rr); return;
      }
    }

    lastScan.kind = DUT_NONE;
    Serial.println("{\"type\":\"NONE\",\"msg\":\"No DUT detected\"}");
    commitScanOutcome(false);
    return;
  }

  for (int c = 0; c < 3; c++) {
    int a = (c == 0) ? 0 : (c == 1 ? 0 : 1);
    int b = (c == 0) ? 1 : (c == 1 ? 2 : 2);
    const PairProbe &fab = findPair(p1A, a, b);
    const PairProbe &fba = findPair(p1A, b, a);
    if (fab.conducts && fba.conducts && fab.vDut < 0.35f && fba.vDut < 0.35f) {
      if (testCapChargeRetention(a, b)) {
        CapResult cr = characterizeCapacitor(a, b);
        if (cr.valid) {
          lastScan.kind = DUT_CAPACITOR; lastScan.cap = cr;
          commitScanOutcome(true); emitCapJson(cr); return;
        }
      }
    }
  }

  for (int g = 0; g < 3; g++) {
    int p1 = (g + 1) % 3, p2 = (g + 2) % 3;
    const PairProbe &d12 = findPair(p1A, p1, p2);
    const PairProbe &d21 = findPair(p1A, p2, p1);
    if (d12.conducts && d21.conducts && d12.vDut < 0.12f && d21.vDut < 0.12f &&
        fabsf(d12.vDut - d21.vDut) < 0.04f) {
      const PairProbe &gp1 = findPair(p1A, g, p1);
      const PairProbe &gp2 = findPair(p1A, g, p2);
      const PairProbe &p1g = findPair(p1A, p1, g);
      const PairProbe &p2g = findPair(p1A, p2, g);

      JfetResult jr;
      if (gp1.conducts && gp2.conducts && !p1g.conducts && !p2g.conducts) {
        if (tryJfetPinchOff(p1, p2, g, false, jr)) {
          lastScan.kind = DUT_JFET; lastScan.jfet = jr;
          commitScanOutcome(true); emitJfetJson(jr); return;
        }
      }
      if (p1g.conducts && p2g.conducts && !gp1.conducts && !gp2.conducts) {
        if (tryJfetPinchOff(p1, p2, g, true, jr)) {
          lastScan.kind = DUT_JFET; lastScan.jfet = jr;
          commitScanOutcome(true); emitJfetJson(jr); return;
        }
      }
    }
  }

  int bestBase = -1;
  bool bestIsPNP = false;
  float bestBjtScore = 0.0f;
  for (int b = 0; b < 3; b++) {
    int p1 = (b + 1) % 3, p2 = (b + 2) % 3;
    const PairProbe &f1 = findPair(p1A, b, p1);
    const PairProbe &f2 = findPair(p1A, b, p2);
    const PairProbe &r1 = findPair(p1A, p1, b);
    const PairProbe &r2 = findPair(p1A, p2, b);

    if (f1.conducts && f2.conducts && (f1.i_uA > r1.i_uA + 0.5f) && (f2.i_uA > r2.i_uA + 0.5f)) {
      float sc = (f1.i_uA - r1.i_uA) + (f2.i_uA - r2.i_uA);
      if (sc > bestBjtScore) { bestBjtScore = sc; bestBase = b; bestIsPNP = false; }
    }
    if (r1.conducts && r2.conducts && (r1.i_uA > f1.i_uA + 0.5f) && (r2.i_uA > f2.i_uA + 0.5f)) {
      float sc = (r1.i_uA - f1.i_uA) + (r2.i_uA - f2.i_uA);
      if (sc > bestBjtScore) { bestBjtScore = sc; bestBase = b; bestIsPNP = true; }
    }
  }
  if (bestBase >= 0) {
    int p1 = (bestBase + 1) % 3, p2 = (bestBase + 2) % 3;
    BjtResult bjt = characterizeBJT(p1, bestBase, p2, bestIsPNP);
    if (bjt.valid) {
      lastScan.kind = DUT_BJT; lastScan.bjt = bjt;
      commitScanOutcome(true); emitBjtJson(bjt); return;
    }
  }

  if (cnt1A == 1) {
    for (int k = 0; k < 6; k++) {
      if (p1A[k].conducts) {
        MosfetResult mr;
        if (tryMosfetGateToggle(p1A[k].hi, p1A[k].lo, p1A[k].third, mr)) {
          lastScan.kind = DUT_MOSFET; lastScan.mos = mr;
          commitScanOutcome(true); emitMosfetJson(mr); return;
        }
        DiodeResult dr = characterizeDiode(p1A[k].hi, p1A[k].lo);
        if (dr.valid) {
          lastScan.kind = DUT_DIODE; lastScan.diode = dr;
          commitScanOutcome(true); emitDiodeJson(dr); return;
        }
      }
    }
  }

  if (cnt1A == 6) {
    bool allSym = true;
    const int potPairs[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    for (int c = 0; c < 3; c++) {
      const PairProbe &f = findPair(p1A, potPairs[c][0], potPairs[c][1]);
      const PairProbe &r = findPair(p1A, potPairs[c][1], potPairs[c][0]);
      if (!f.conducts || !r.conducts || fabsf(f.vDut - r.vDut) >= 0.08f) allSym = false;
    }
    if (allSym) {
      ResistorResult bestR;
      for (int c = 0; c < 3; c++) {
        ResistorResult rr = characterizeResistor(potPairs[c][0], potPairs[c][1]);
        if (rr.valid && (!bestR.valid || rr.r_ohm > bestR.r_ohm)) bestR = rr;
      }
      if (bestR.valid) {
        lastScan.kind = DUT_RESISTOR; lastScan.res = bestR;
        commitScanOutcome(true); emitResistorJson(bestR); return;
      }
    }
  }

  for (int k = 0; k < 6; k++) {
    if (p1A[k].conducts) {
      const PairProbe &rev = findPair(p1A, p1A[k].lo, p1A[k].hi);
      if (rev.conducts) {
        if (fabsf(p1A[k].vDut - rev.vDut) < 0.08f) {
          ResistorResult rr = characterizeResistor(p1A[k].hi, p1A[k].lo);
          if (rr.valid) {
            lastScan.kind = DUT_RESISTOR; lastScan.res = rr;
            commitScanOutcome(true); emitResistorJson(rr); return;
          }
        } else {
          int pA = (p1A[k].vDut <= rev.vDut) ? p1A[k].hi : p1A[k].lo;
          int pK = (p1A[k].vDut <= rev.vDut) ? p1A[k].lo : p1A[k].hi;
          DiodeResult dr = characterizeDiode(pA, pK);
          if (dr.valid) {
            lastScan.kind = DUT_DIODE; lastScan.diode = dr;
            commitScanOutcome(true); emitDiodeJson(dr); return;
          }
        }
      } else {
        MosfetResult mr;
        if (tryMosfetGateToggle(p1A[k].hi, p1A[k].lo, p1A[k].third, mr)) {
          lastScan.kind = DUT_MOSFET; lastScan.mos = mr;
          commitScanOutcome(true); emitMosfetJson(mr); return;
        }
        DiodeResult dr = characterizeDiode(p1A[k].hi, p1A[k].lo);
        if (dr.valid) {
          lastScan.kind = DUT_DIODE; lastScan.diode = dr;
          commitScanOutcome(true); emitDiodeJson(dr); return;
        }
      }
      break;
    }
  }

  lastScan.kind = DUT_NONE;
  Serial.println("{\"type\":\"NONE\",\"msg\":\"Unrecognized DUT\"}");
  commitScanOutcome(false);
}

void runFullAutoDetect() {
  if (drift.active) stopThermalDrift();
  isMeasurementBusy = true;
  abortRequested = false;
  updateLedStatusFromState();
  detectAndDispatchDUT();
  isMeasurementBusy = false;
  updateLedStatusFromState();
}

// ======================= TFT1 CURVE TRACER GRATICULE & PLOTTER ===============
static constexpr int PLOT_GX = 34;
static constexpr int PLOT_GY = 18;
static constexpr int PLOT_GW = 192; // 8 columns x 24 px
static constexpr int PLOT_GH = 130; // 5 rows    x 26 px
static constexpr int SIDE_X  = 230;
static constexpr int SIDE_W  = 90;

static void drawCurveTracerFrame(float vMax, float icScaleMax_mA, bool useMicroAmps,
                                 const char *hdrTitle, bool clamped) {
  if (!tftsOK) return;
  tft1.fillScreen(COL_BG);

  tft1.setTextSize(1);
  tft1.setTextColor(COL_LABEL);
  tft1.setCursor(4, 5);
  tft1.print(hdrTitle);

  if (clamped) {
    tft1.fillRoundRect(160, 2, 66, 13, 2, COL_ERR_BDG);
    tft1.setTextColor(COL_WHITE);
    tft1.setCursor(164, 5);
    tft1.print("P>35mW");
  } else {
    tft1.setTextColor(COL_GREEN);
    tft1.setCursor(172, 5);
    tft1.print("<35mW OK");
  }

  for (int c = 0; c <= 8; c++) {
    int x = PLOT_GX + c * 24;
    tft1.drawFastVLine(x, PLOT_GY, PLOT_GH, (c == 0 || c == 8) ? COL_BORDER : COL_GRID);
  }
  for (int r = 0; r <= 5; r++) {
    int y = PLOT_GY + r * 26;
    tft1.drawFastHLine(PLOT_GX, y, PLOT_GW, (r == 0 || r == 5) ? COL_BORDER : COL_GRID);
  }
  for (int x = PLOT_GX + 6; x < PLOT_GX + PLOT_GW; x += 6) {
    tft1.drawPixel(x, PLOT_GY + PLOT_GH / 2, COL_GRID_AXIS);
  }
  for (int y = PLOT_GY + 6; y < PLOT_GY + PLOT_GH; y += 6) {
    tft1.drawPixel(PLOT_GX + PLOT_GW / 2, y, COL_GRID_AXIS);
  }

  tft1.setTextColor(COL_MUTED);
  tft1.setTextSize(1);
  char yBuf[10];
  float topVal = useMicroAmps ? (icScaleMax_mA * 1000.0f) : icScaleMax_mA;
  if (topVal >= 100.0f) snprintf(yBuf, sizeof(yBuf), "%.0f", topVal);
  else if (topVal >= 10.0f) snprintf(yBuf, sizeof(yBuf), "%.1f", topVal);
  else snprintf(yBuf, sizeof(yBuf), "%.2f", topVal);
  tft1.setCursor(2, PLOT_GY - 2);
  tft1.print(yBuf);

  float midVal = 0.5f * topVal;
  if (midVal >= 100.0f) snprintf(yBuf, sizeof(yBuf), "%.0f", midVal);
  else if (midVal >= 10.0f) snprintf(yBuf, sizeof(yBuf), "%.1f", midVal);
  else snprintf(yBuf, sizeof(yBuf), "%.2f", midVal);
  tft1.setCursor(2, PLOT_GY + PLOT_GH / 2 - 4);
  tft1.print(yBuf);

  tft1.setCursor(2, PLOT_GY + PLOT_GH - 7);
  tft1.print(useMicroAmps ? "uA" : "mA");

  tft1.setCursor(PLOT_GX - 2, PLOT_GY + PLOT_GH + 6);
  tft1.print("0V");
  char xBuf[12];
  snprintf(xBuf, sizeof(xBuf), "%.1fV", 0.5f * vMax);
  tft1.setCursor(PLOT_GX + PLOT_GW / 2 - 14, PLOT_GY + PLOT_GH + 6);
  tft1.print(xBuf);
  snprintf(xBuf, sizeof(xBuf), "%.1fV", vMax);
  tft1.setCursor(PLOT_GX + PLOT_GW - 24, PLOT_GY + PLOT_GH + 6);
  tft1.print(xBuf);

  tft1.fillRect(SIDE_X, 0, SIDE_W, 170, COL_SIDEBAR);
  tft1.drawFastVLine(SIDE_X, 0, 170, COL_BORDER);
  tft1.fillRect(SIDE_X + 1, 0, SIDE_W - 1, 20, COL_SIDE_HDR);

  tft1.setTextColor(COL_WHITE);
  tft1.setTextSize(1);
  tft1.setCursor(SIDE_X + 6, 6);
  if (lastScan.kind == DUT_BJT && lastScan.bjt.valid) {
    tft1.printf("%s %s", lastScan.bjt.isGermanium ? "Ge" : "Si",
                lastScan.bjt.isPNP ? "PNP" : "NPN");
  } else if (lastScan.kind == DUT_JFET && lastScan.jfet.valid) {
    tft1.print(lastScan.jfet.isP ? "P-JFET" : "N-JFET");
  } else if (lastScan.kind == DUT_DIODE && lastScan.diode.valid) {
    tft1.print(lastScan.diode.isZener ? "ZENER" : "DIODE");
  } else {
    tft1.print("Ge-LAB PRO");
  }

  tft1.setTextColor(COL_LABEL);
  tft1.setCursor(SIDE_X + 5, 24);
  tft1.print("Model/Pins");
  tft1.setTextColor(COL_WHITE);
  tft1.setCursor(SIDE_X + 5, 34);
  if (lastScan.kind == DUT_BJT && lastScan.bjt.valid) {
    char r[3] = {'?', '?', '?'};
    r[lastScan.bjt.pinE] = 'E'; r[lastScan.bjt.pinB] = 'B'; r[lastScan.bjt.pinC] = 'C';
    tft1.printf("1%c 2%c 3%c", r[0], r[1], r[2]);
  } else if (lastScan.kind == DUT_JFET && lastScan.jfet.valid) {
    char r[3] = {'?', '?', '?'};
    r[lastScan.jfet.pinD] = 'D'; r[lastScan.jfet.pinG] = 'G'; r[lastScan.jfet.pinS] = 'S';
    tft1.printf("1%c 2%c 3%c", r[0], r[1], r[2]);
  } else {
    tft1.print("ZIF 1-2-3");
  }

  tft1.setTextColor(COL_LABEL);
  tft1.setCursor(SIDE_X + 5, 48);
  tft1.print("B RANGE:");
  tft1.setTextColor(COL_YELLOW);
  tft1.setCursor(SIDE_X + 5, 58);
  tft1.print(B_STEP_LABELS[bRangeIdx]);

  tft1.setTextColor(COL_LABEL);
  tft1.setCursor(SIDE_X + 5, 72);
  tft1.print("C/V RNG:");
  tft1.setTextColor(COL_CYAN);
  tft1.setCursor(SIDE_X + 5, 82);
  tft1.print(VCE_MAX_LABELS[vceRangeIdx]);

  tft1.setTextColor(COL_LABEL);
  tft1.setCursor(SIDE_X + 5, 96);
  tft1.print("hFE Peak:");
  tft1.setTextColor(COL_GREEN);
  tft1.setTextSize(2);
  tft1.setCursor(SIDE_X + 5, 106);
  float hShow = (lastSweep.valid && !isnan(lastSweep.peakHfe))
                    ? lastSweep.peakHfe
                    : ((lastScan.kind == DUT_BJT) ? lastScan.bjt.hfeTrue : NAN);
  if (!isnan(hShow)) tft1.printf("%.0f", hShow);
  else               tft1.print("--");

  tft1.drawFastHLine(SIDE_X + 3, 125, SIDE_W - 6, COL_BORDER);
  tft1.setTextSize(1);
  if (lastSweep.valid && cursorCurve >= 0 && cursorCurve < lastSweep.curves &&
      cursorPt >= 0 && cursorPt < lastSweep.curvePtCount[cursorCurve] &&
      lastSweep.pts[cursorCurve][cursorPt].valid) {
    const SweepPoint &sp = lastSweep.pts[cursorCurve][cursorPt];
    tft1.setTextColor(CURVE_COLORS[cursorCurve % 6]);
    tft1.setCursor(SIDE_X + 5, 129);
    tft1.printf("Ib:%.1fuA", sp.ib_uA);
    tft1.setTextColor(COL_WHITE);
    tft1.setCursor(SIDE_X + 5, 140);
    tft1.printf("Vce:%.2fV", sp.vce_V);
    tft1.setCursor(SIDE_X + 5, 151);
    if (fabsf(sp.ic_mA) < 1.0f) tft1.printf("Ic:%.0fuA", sp.ic_mA * 1000.0f);
    else                        tft1.printf("Ic:%.2fmA", sp.ic_mA);
  } else {
    tft1.setTextColor(COL_MUTED);
    tft1.setCursor(SIDE_X + 5, 130);
    tft1.print("BTN A:Sweep");
    tft1.setCursor(SIDE_X + 5, 142);
    tft1.print("D-PAD:Cur");
    tft1.setCursor(SIDE_X + 5, 154);
    tft1.print("BTN B:Mode");
  }
}

static void mapPlotXY(float vce, float ic_mA, float vMax, float icMax_mA, int &px, int &py) {
  float fx = constrain(vce / fmaxf(0.1f, vMax), 0.0f, 1.0f);
  float fy = constrain(ic_mA / fmaxf(0.001f, icMax_mA), 0.0f, 1.0f);
  px = PLOT_GX + (int)lroundf(fx * (float)(PLOT_GW - 1));
  py = (PLOT_GY + PLOT_GH - 1) - (int)lroundf(fy * (float)(PLOT_GH - 1));
}

static void drawStoredCurvesOnTFT1() {
  float vMax = lastSweep.valid ? lastSweep.vMax_V : VCE_MAX_LIST[vceRangeIdx];
  float icMax = 1.0f;
  if (lastSweep.valid && lastSweep.maxIc_mA > 0.02f) {
    icMax = lastSweep.maxIc_mA * 1.12f;
  }
  if (icMax <= 0.1f)       icMax = 0.1f;
  else if (icMax <= 0.25f) icMax = 0.25f;
  else if (icMax <= 0.5f)  icMax = 0.5f;
  else if (icMax <= 1.0f)  icMax = 1.0f;
  else if (icMax <= 2.5f)  icMax = 2.5f;
  else if (icMax <= 5.0f)  icMax = 5.0f;
  else if (icMax <= 10.0f) icMax = 10.0f;
  else if (icMax <= 20.0f) icMax = 20.0f;
  else                     icMax = ceilf(icMax / 5.0f) * 5.0f;

  bool useMicroAmps = (icMax < 1.0f);
  drawCurveTracerFrame(vMax, icMax, useMicroAmps, "CHY-Ge CURVE TRACER",
                       lastSweep.valid && lastSweep.powerClamped);

  if (!lastSweep.valid || lastSweep.points == 0) {
    tft1.setTextColor(COL_MUTED);
    tft1.setTextSize(1);
    tft1.setCursor(PLOT_GX + 24, PLOT_GY + 52);
    tft1.print("Insert BJT & Press BTN A");
    tft1.setCursor(PLOT_GX + 28, PLOT_GY + 66);
    tft1.print("to sweep 6 pulsed curves");
    return;
  }

  for (int c = 0; c < lastSweep.curves && c < MAX_SWEEP_CURVES; c++) {
    uint16_t col = CURVE_COLORS[c % 6];
    int n = lastSweep.curvePtCount[c];
    int prevX = PLOT_GX, prevY = PLOT_GY + PLOT_GH - 1;
    bool hasPrev = false;
    for (int i = 0; i < n; i++) {
      if (!lastSweep.pts[c][i].valid) continue;
      int px, py;
      mapPlotXY(lastSweep.pts[c][i].vce_V, lastSweep.pts[c][i].ic_mA, vMax, icMax, px, py);
      if (hasPrev) {
        tft1.drawLine(prevX, prevY, px, py, col);
        tft1.drawLine(prevX, max(PLOT_GY, prevY - 1), px, max(PLOT_GY, py - 1), col);
      }
      tft1.fillCircle(px, py, 2, col);
      prevX = px; prevY = py; hasPrev = true;
    }
  }

  if (cursorCurve >= 0 && cursorCurve < lastSweep.curves &&
      cursorPt >= 0 && cursorPt < lastSweep.curvePtCount[cursorCurve] &&
      lastSweep.pts[cursorCurve][cursorPt].valid) {
    const SweepPoint &sp = lastSweep.pts[cursorCurve][cursorPt];
    int cx, cy;
    mapPlotXY(sp.vce_V, sp.ic_mA, vMax, icMax, cx, cy);
    tft1.drawCircle(cx, cy, 5, COL_WHITE);
    tft1.drawFastHLine(max(PLOT_GX, cx - 8), cy, 17, COL_WHITE);
    tft1.drawFastVLine(cx, max(PLOT_GY, cy - 8), 17, COL_WHITE);
  }
}

// ======================= STAGE 3: PULSED Ge 6-CURVE SWEEP ('s') ==============
void runPulsedGeSweep(bool runDetectFirst) {
  if (inStandby) {
    emitStatusJson("ERROR", "Exit Standby before Curve Sweep");
    return;
  }
  ensureRadioOffForAnalog();
  if (drift.active) stopThermalDrift();

  isMeasurementBusy = true;
  abortRequested = false;
  setStatusLeds(false, false, true, false, false);
  emitStatusJson("INFO", "Starting Pulsed Ge Curve Sweep...");

  if (runDetectFirst || lastScan.kind != DUT_BJT || !lastScan.bjt.valid) {
    detectAndDispatchDUT();
  }
  if (lastScan.kind != DUT_BJT || !lastScan.bjt.valid) {
    emitStatusJson("ERROR", "Curve sweep requires a valid BJT in socket");
    isMeasurementBusy = false;
    updateLedStatusFromState();
    renderScreens();
    return;
  }

  const BjtResult &bjt = lastScan.bjt;
  const int pE = bjt.pinE, pB = bjt.pinB, pC = bjt.pinC;
  const bool isPNP = bjt.isPNP;
  const float ibStep = B_STEP_UA_LIST[bRangeIdx];
  const float vMax   = VCE_MAX_LIST[vceRangeIdx];

  SweepSummary sw;
  sw.valid       = true;
  sw.isPNP       = isPNP;
  sw.isGermanium = bjt.isGermanium;
  sw.pinE        = pE;
  sw.pinB        = pB;
  sw.pinC        = pC;
  sw.bStep_uA    = ibStep;
  sw.vMax_V      = vMax;
  sw.tempC       = getTempC();

  static constexpr int NUM_VPTS = 9;
  float vccSteps[NUM_VPTS];
  for (int i = 0; i < NUM_VPTS; i++) {
    float frac = (float)(i + 1) / (float)NUM_VPTS;
    vccSteps[i] = 0.25f + (vMax - 0.25f) * (0.45f * frac + 0.55f * frac * frac);
  }

  float hfeEst = (!isnan(bjt.hfeTrue) && bjt.hfeTrue > 2.0f) ? bjt.hfeTrue : 80.0f;
  float yLiveMax_mA = constrain((5.0f * ibStep * 0.001f * hfeEst) * 1.25f, 1.0f, 25.0f);
  bool yLiveMicro = (yLiveMax_mA < 1.0f);
  lastSweep = sw;
  cursorCurve = -1;
  cursorPt = -1;
  if (tftsOK) {
    drawCurveTracerFrame(vMax, yLiveMax_mA, yLiveMicro, "SWEEPING LIVE...", false);
  }

  const PinModeSS cDriveMode = isPNP ? SS_GND_470R : SS_SRCA_470R;
  const PinModeSS eMode      = isPNP ? SS_SRCA_470R : SS_GND_HARD;
  const float vbeEst = constrain(bjt.vbe_mV * 0.001f, 0.12f, 0.75f);

  ads.setDataRate(RATE_ADS1115_250SPS);

  for (int cIdx = 0; cIdx < MAX_SWEEP_CURVES; cIdx++) {
    pollMcpInputs();
    if (abortRequested) break;

    float ibGoal = (float)cIdx * ibStep;
    PinModeSS bMode = (ibGoal < 0.1f) ? SS_HIZ :
                      (ibGoal <= 3.8f ? SS_SRCB_2M2 : SS_SRCB_100K);
    sw.curves++;
    int ptIdx = 0;
    int prevX = PLOT_GX, prevY = PLOT_GY + PLOT_GH - 1;
    bool hasPrev = false;
    uint16_t curveCol = CURVE_COLORS[cIdx % 6];

    for (int vIdx = 0; vIdx < NUM_VPTS; vIdx++) {
      pollMcpInputs();
      if (abortRequested) break;

      float vSupply = vccSteps[vIdx];
      float vB = 0.0f;
      if (bMode != SS_HIZ) {
        float rB = pathResToSource(pB, bMode);
        if (!isPNP) {
          vB = constrain(vbeEst + (ibGoal * 1e-6f) * rB, 0.15f, DAC_MAX_V);
        } else {
          float vE_est = fmaxf(0.4f, vSupply - 0.35f);
          vB = constrain(vE_est - vbeEst - (ibGoal * 1e-6f) * rB, 0.0f, DAC_MAX_V);
          if (vE_est - vbeEst - vB < 0.05f && bMode == SS_SRCB_2M2) {
            bMode = SS_SRCB_100K;
            rB = pathResToSource(pB, bMode);
            vB = constrain(vE_est - vbeEst - (ibGoal * 1e-6f) * rB, 0.0f, DAC_MAX_V);
          }
        }
      }

      allSafeHiZ();
      setSourcesAB(vSupply, vB, true);
      float vLiveA = readLiveRefV(CAL_SRCA, 2);
      float vLiveB = (bMode != SS_HIZ) ? readLiveRefV(CAL_SRCB, 2) : 0.0f;
      delay(10);

      applyRoleMatrix(pE, eMode, pB, bMode, pC, cDriveMode, 7);
      float vC = readNodeV(pC, 2);
      float vE = readNodeV(pE, 2);
      float vB_m = readNodeV(pB, 2);
      float ic_mA = 0.0f, ib_uA = 0.0f, vce = 0.0f;

      if (!isPNP) {
        float vDropC = readDiffSource(pC, CAL_SRCA, vLiveA, 2);
        if (vDropC < 0.0f) vDropC = fmaxf(0.0f, vLiveA - vC);
        ic_mA = (vDropC / pathResToSource(pC, cDriveMode)) * 1e3f;
        ib_uA = (bMode == SS_HIZ) ? 0.0f
                : ((vLiveB - vB_m) / pathResToSource(pB, bMode)) * 1e6f;
        vce = vC - vE;
      } else {
        ic_mA = (fmaxf(0.0f, vC) / pathResToGnd(pC, cDriveMode)) * 1e3f;
        ib_uA = (bMode == SS_HIZ) ? 0.0f
                : ((vB_m - vLiveB) / pathResToSource(pB, bMode)) * 1e6f;
        vce = vE - vC;
      }
      allSafeHiZ();

      float pMw = fabsf(vce) * fabsf(ic_mA);
      if (pMw > sw.maxPower_mW) sw.maxPower_mW = pMw;
      if (ic_mA > sw.maxIc_mA)  sw.maxIc_mA = ic_mA;
      if (vce > sw.maxVce_V)    sw.maxVce_V = vce;

      if (ib_uA > 0.3f && vce > 1.0f) {
        float h = (ic_mA * 1000.0f) / ib_uA;
        if (isnan(sw.peakHfe) || h > sw.peakHfe) sw.peakHfe = h;
      }

      if (ptIdx < MAX_SWEEP_PTS) {
        sw.pts[cIdx][ptIdx].valid = true;
        sw.pts[cIdx][ptIdx].ib_uA = ib_uA;
        sw.pts[cIdx][ptIdx].vce_V = vce;
        sw.pts[cIdx][ptIdx].ic_mA = ic_mA;
        sw.pts[cIdx][ptIdx].p_mW  = pMw;
        ptIdx++;
        sw.curvePtCount[cIdx] = ptIdx;
      }
      sw.points++;

      if (tftsOK) {
        int px, py;
        mapPlotXY(vce, ic_mA, vMax, yLiveMax_mA, px, py);
        if (hasPrev) tft1.drawLine(prevX, prevY, px, py, curveCol);
        tft1.fillCircle(px, py, 2, curveCol);
        prevX = px; prevY = py; hasPrev = true;
      }

      Serial.printf(
          "{\"type\":\"SWEEP_PT\",\"curve\":%d,\"ib_uA\":%.2f,\"vce_V\":%.3f,"
          "\"ic_mA\":%.3f,\"p_mW\":%.2f}\n",
          cIdx, ib_uA, vce, ic_mA, pMw);

      if (pMw > P_DUT_MAX_MW) {
        sw.powerClamped = true;
        break;
      }
    }
    if (abortRequested) break;
  }

  ads.setDataRate(RATE_ADS1115_128SPS);
  allSafeHiZ();

  if (abortRequested) {
    emitStatusJson("INFO", "Sweep aborted by user");
    isMeasurementBusy = false;
    updateLedStatusFromState();
    renderScreens();
    return;
  }

  lastSweep = sw;
  if (sw.curves > 0 && sw.curvePtCount[sw.curves - 1] > 0) {
    cursorCurve = sw.curves - 1;
    cursorPt    = sw.curvePtCount[cursorCurve] - 1;
  }

  char tStr[20], hfeStr[20], jsonBuf[360];
  fmtJsonFloat(tStr, sizeof(tStr), sw.tempC, 2);
  fmtJsonFloat(hfeStr, sizeof(hfeStr), sw.peakHfe, 1);
  snprintf(jsonBuf, sizeof(jsonBuf),
      "{\"type\":\"SWEEP_DONE\",\"hash\":\"%s\",\"mat\":\"%s\",\"pol\":\"%s\",\"curves\":%d,"
      "\"points\":%d,\"peak_hfe\":%s,\"max_ic_mA\":%.3f,\"max_vce_V\":%.3f,"
      "\"max_p_mW\":%.2f,\"power_clamped\":%s,\"temp_C\":%s}",
      lastScan.dutHash, sw.isGermanium ? "Ge" : "Si", sw.isPNP ? "PNP" : "NPN",
      sw.curves, sw.points, hfeStr, sw.maxIc_mA, sw.maxVce_V,
      sw.maxPower_mW, sw.powerClamped ? "true" : "false", tStr);
  Serial.println(jsonBuf);
  wifiPostAfterSafeHiZ(jsonBuf);

  playModemScanStep(3);
  isMeasurementBusy = false;
  updateLedStatusFromState();
  renderScreens();
}

// ======================= STAGE 3: THERMAL DRIFT LOGGER ('d') =================
void stopThermalDrift() {
  if (!drift.active) return;
  drift.active = false;
  allSafeHiZ();
  Serial.printf("{\"type\":\"DRIFT_STOP\",\"samples\":%lu}\n",
                (unsigned long)drift.sampleCount);
  updateLedStatusFromState();
  renderScreens();
}

static void sampleThermalDriftOnce() {
  if (!drift.active || inStandby) return;
  const int pE = drift.pinE, pB = drift.pinB, pC = drift.pinC;
  const bool isPNP = drift.isPNP;

  float vDummy = 0.0f, peakMw = 0.0f;
  float iceo = measureBjtLeakageAuto(pB, pE, pC, isPNP, 9.00f, vDummy, peakMw);
  float icbo = measureBjtLeakageAuto(pE, pB, pC, isPNP, 9.00f, vDummy, peakMw);

  float ic_uA = 0.0f, ib_uA = 0.0f, vbe_mV = 0.0f, hfeTrue = NAN;
  PinModeSS cMode = (iceo > 1500.0f) ? (isPNP ? SS_GND_470R : SS_SRCA_470R)
                                     : (isPNP ? SS_GND_2K49 : SS_SRCA_2K49);
  if (!isPNP) {
    setSourcesAB(9.00f, 9.00f, true);
    float vLiveA = readLiveRefV(CAL_SRCA, 2);
    float vLiveB = readLiveRefV(CAL_SRCB, 2);
    applyRoleMatrix(pE, SS_GND_HARD, pB, SS_SRCB_2M2, pC, cMode, 10);
    float vB = readNodeV(pB, 3);
    float vE = readNodeV(pE, 3);
    float vDropC = readDiffSource(pC, CAL_SRCA, vLiveA, 3);
    if (vDropC < 0.0f) vDropC = fmaxf(0.0f, vLiveA - readNodeV(pC, 2));
    ib_uA  = ((vLiveB - vB) / pathResToSource(pB, SS_SRCB_2M2)) * 1e6f;
    ic_uA  = (vDropC / pathResToSource(pC, cMode)) * 1e6f;
    vbe_mV = (vB - vE) * 1000.0f;
  } else {
    setSourcesAB(0.0f, 0.00f, true);
    float vLiveB = readLiveRefV(CAL_SRCB, 2);
    applyRoleMatrix(pE, SS_9V_REF, pB, SS_SRCB_2M2, pC, cMode, 10);
    float vB = readNodeV(pB, 3);
    float vC = readNodeV(pC, 3);
    float vE = readNodeV(pE, 3);
    ib_uA  = ((vB - vLiveB) / pathResToSource(pB, SS_SRCB_2M2)) * 1e6f;
    ic_uA  = (fmaxf(0.0f, vC) / pathResToGnd(pC, cMode)) * 1e6f;
    vbe_mV = (vE - vB) * 1000.0f;
  }
  allSafeHiZ();

  if (ib_uA > 0.2f && ic_uA > iceo + 1.0f) {
    hfeTrue = (ic_uA - iceo) / ib_uA;
  }

  drift.sampleCount++;
  drift.lastHfe      = hfeTrue;
  drift.lastIceo_uA  = iceo;
  drift.lastIcbo_uA  = icbo;
  drift.lastVbe_mV   = vbe_mV;
  drift.lastTempC    = getTempC();
  drift.lastSampleMs = millis();

  drift.histHfe[drift.histHead]  = isnan(hfeTrue) ? 0.0f : hfeTrue;
  drift.histIceo[drift.histHead] = iceo;
  drift.histHead = (drift.histHead + 1) % DRIFT_HIST_LEN;
  if (drift.histCount < DRIFT_HIST_LEN) drift.histCount++;

  float elapsed_s = (float)(drift.lastSampleMs - drift.startMs) * 0.001f;
  char hfeStr[20], tStr[20];
  fmtJsonFloat(hfeStr, sizeof(hfeStr), hfeTrue, 1);
  fmtJsonFloat(tStr, sizeof(tStr), drift.lastTempC, 2);

  Serial.printf(
      "{\"type\":\"THERMAL_DRIFT\",\"hash\":\"%s\",\"sample\":%lu,\"elapsed_s\":%.2f,"
      "\"mat\":\"%s\",\"pol\":\"%s\",\"hfe_true\":%s,\"iceo_uA\":%.2f,"
      "\"icbo_uA\":%.2f,\"vbe_mV\":%.1f,\"temp_C\":%s}\n",
      lastScan.dutHash, (unsigned long)drift.sampleCount, elapsed_s,
      drift.isGermanium ? "Ge" : "Si", drift.isPNP ? "PNP" : "NPN",
      hfeStr, iceo, icbo, vbe_mV, tStr);

  renderScreens();
}

void startThermalDrift() {
  if (inStandby) {
    emitStatusJson("ERROR", "Exit Standby before Thermal Drift");
    return;
  }
  if (drift.active) {
    stopThermalDrift();
    return;
  }
  ensureRadioOffForAnalog();
  detectAndDispatchDUT();
  if (lastScan.kind != DUT_BJT || !lastScan.bjt.valid) {
    emitStatusJson("ERROR", "Thermal drift logger requires a valid BJT in socket");
    renderScreens();
    return;
  }

  drift = DriftState();
  drift.active       = true;
  drift.isPNP        = lastScan.bjt.isPNP;
  drift.isGermanium  = lastScan.bjt.isGermanium;
  drift.pinE         = lastScan.bjt.pinE;
  drift.pinB         = lastScan.bjt.pinB;
  drift.pinC         = lastScan.bjt.pinC;
  drift.startMs      = millis();
  drift.lastSampleMs = 0;
  consoleMode        = MODE_THERMAL_DRIFT;
  updateLedStatusFromState();
  emitStatusJson("INFO", "Thermal drift logger active (1 Hz). Send 'd' or press BTN A to stop.");
}

// ======================= FLUKE-STYLE 5-IN-1 MULTIMETER ENGINE ================
static void dmmResetStats() {
  dmm.minVal       = NAN;
  dmm.maxVal       = NAN;
  dmm.sumVal       = 0.0;
  dmm.sampleCount  = 0;
  dmm.histCount    = 0;
  dmm.histHead     = 0;
  dmm.prevDiodeConducted = false;
}

static void dmmPushSample(float val) {
  dmm.primaryVal = val;
  if (isnan(dmm.minVal) || val < dmm.minVal) dmm.minVal = val;
  if (isnan(dmm.maxVal) || val > dmm.maxVal) dmm.maxVal = val;
  dmm.sumVal += (double)val;
  dmm.sampleCount++;

  dmm.hist[dmm.histHead] = val;
  dmm.histHead = (dmm.histHead + 1) % DMM_HIST_LEN;
  if (dmm.histCount < DMM_HIST_LEN) dmm.histCount++;
}

static void dmmSelectSubMode(DmmSubMode nextMode) {
  dmm.subMode          = nextMode;
  dmm.hold             = false;
  dmm.relActive        = false;
  dmm.relZero          = 0.0f;
  dmm.continuityActive = false;
  dmm.liveVoltWarn     = false;
  dmm.isZener          = false;
  dmm.openLoop         = true;
  dmm.valid            = false;
  dmm.secondaryVal     = NAN;
  dmm.lastSampleMs     = 0;
  stopBuzzerAll();
  allSafeHiZ();
  dmmResetStats();
  updateLedStatusFromState();
  renderScreens();
}

static void dmmSampleOnce() {
  if (inStandby || consoleMode != MODE_MULTIMETER || dmm.hold) return;
  uint32_t now = millis();
  uint32_t periodMs = 120;
  if (dmm.subMode == DMM_DIODE) periodMs = 220;
  else if (dmm.subMode == DMM_CAP) periodMs = 600;
  if (dmm.lastSampleMs != 0 && (uint32_t)(now - dmm.lastSampleMs) < periodMs) return;
  dmm.lastSampleMs = now;

  dmm.liveVoltWarn = false;

  if (dmm.subMode == DMM_VOLT_12V) {
    allSafeHiZ();
    float v = readNodeV(0, 4);
    dmm.valid        = true;
    dmm.openLoop     = false;
    dmm.continuityActive = false;
    dmm.secondaryVal = NAN;
    snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1 - COM");
    if (fabsf(v) > 11.80f) {
      dmm.liveVoltWarn = true;
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "OVER 12V! USE 30V");
    } else {
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "HI-Z >10G  0..12V");
    }
    float vNet = dmm.relActive ? (v - dmm.relZero) : v;
    dmmPushSample(vNet);
  }
  else if (dmm.subMode == DMM_VOLT_30V) {
    allSafeHiZ();
    selectCalMux(CAL_DMM30V);
    float v = readNodeV(3, 4) * HV_DIV_RATIO;
    dmm.valid        = true;
    dmm.openLoop     = false;
    dmm.continuityActive = false;
    dmm.secondaryVal = v / HV_DIV_RATIO;
    snprintf(dmm.probeStr, sizeof(dmm.probeStr), "V30V - COM");
    if (v > DMM_HV_MAX_V) {
      dmm.liveVoltWarn = true;
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "HV OVER 33.0V!");
    } else {
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "11:1 HV RAIL 0..33V");
    }
    float vNet = dmm.relActive ? (v - dmm.relZero) : v;
    dmmPushSample(vNet);
  }
  else if (dmm.subMode == DMM_OHM_CONT) {
    allSafeHiZ();
    float vIdle = readNodeV(0, 1);
    if (fabsf(vIdle) > 0.35f) {
      dmm.liveVoltWarn     = true;
      dmm.valid            = false;
      dmm.openLoop         = true;
      dmm.continuityActive = false;
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1 - COM");
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "LIVE VOLT: %.2fV", vIdle);
      updateToneSequencer();
      updateLedStatusFromState();
      renderScreens();
      return;
    }

    auto measureOhmFast = [&](bool comMode, float &rOut, float &vDutOut, float &iOut_mA) -> bool {
      auto runMode = [&](PinModeSS hiMode, float vSrc, float &rM, float &vM, float &iM) -> bool {
        bool isSrcB = (hiMode == SS_SRCB_100K || hiMode == SS_SRCB_2M2);
        setSourcesAB(isSrcB ? 0.0f : vSrc, isSrcB ? vSrc : 0.0f, true);
        CalMuxCh calCh = isSrcB ? CAL_SRCB : CAL_SRCA;
        float vLive = readLiveRefV(calCh, 2);

        if (comMode) setMatrix(hiMode, SS_HIZ, SS_HIZ, true);
        else         setMatrix(hiMode, SS_HIZ, SS_GND_HARD, true);
        delay(8);

        float vHi = readNodeV(0, 3);
        float vLo = comMode ? 0.0f : readNodeV(2, 3);
        float vDrop = readDiffSource(0, calCh, vLive, 3);
        if (vDrop < 0.0f) vDrop = fmaxf(0.0f, vLive - vHi);

        float i_A = vDrop / pathResToSource(0, hiMode);
        if (i_A < 1e-8f) return false;
        vM = vHi - vLo;
        iM = i_A * 1e3f;
        rM = fmaxf(0.0f, vM / i_A);
        return true;
      };

      if (runMode(SS_SRCA_470R, 1.50f, rOut, vDutOut, iOut_mA) && rOut < 1200.0f) return true;
      if (runMode(SS_SRCA_2K49, 3.00f, rOut, vDutOut, iOut_mA) && rOut < 22000.0f) return true;
      if (runMode(SS_SRCB_100K, 5.00f, rOut, vDutOut, iOut_mA) && rOut < 450000.0f) return true;
      if (runMode(SS_SRCB_2M2,  8.00f, rOut, vDutOut, iOut_mA) && rOut < 25.0e6f) return true;
      return false;
    };

    float rMeas = 0.0f, vDut = 0.0f, i_mA = 0.0f;
    bool okCom = measureOhmFast(true, rMeas, vDut, i_mA);
    if (okCom) {
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1 - COM");
    } else {
      okCom = measureOhmFast(false, rMeas, vDut, i_mA);
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), okCom ? "TP1 - TP3" : "TP1 - COM");
    }
    allSafeHiZ();

    if (okCom && rMeas <= 22.0e6f) {
      float rNet = dmm.relActive ? fmaxf(0.0f, rMeas - dmm.relZero) : rMeas;
      dmm.valid        = true;
      dmm.openLoop     = false;
      dmm.secondaryVal = i_mA;
      dmm.continuityActive = (rNet < DMM_CONT_OHM);
      if (dmm.continuityActive) snprintf(dmm.noteStr, sizeof(dmm.noteStr), "CONT 2.7k (<30R)");
      else                      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "Vdut:%.3fV", vDut);
      dmmPushSample(rNet);
    } else {
      dmm.valid            = false;
      dmm.openLoop         = true;
      dmm.continuityActive = false;
      dmm.secondaryVal     = NAN;
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "OPEN (>20 MOhm)");
    }
    updateToneSequencer();
  }
  else if (dmm.subMode == DMM_DIODE) {
    allSafeHiZ();
    float vIdle = readNodeV(0, 1);
    if (fabsf(vIdle) > 0.35f) {
      dmm.liveVoltWarn = true;
      dmm.valid        = false;
      dmm.openLoop     = true;
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1 - COM");
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "LIVE VOLT: %.2fV", vIdle);
      updateLedStatusFromState();
      renderScreens();
      return;
    }

    auto probeDiodePair = [&](int pA, int pK, bool kCom, float &vf_V) -> bool {
      setSourcesAB(3.80f, 0.0f, true);
      float vLive = readLiveRefV(CAL_SRCA, 2);
      if (kCom) setMatrix(SS_SRCA_2K49, SS_HIZ, SS_HIZ, true);
      else      applyRoleMatrix(pA, SS_SRCA_2K49, pK, SS_GND_HARD, 3 - pA - pK, SS_HIZ, 0);
      delay(8);
      float vA = readNodeV(pA, 3);
      float vK = kCom ? 0.0f : readNodeV(pK, 3);
      float i_mA = ((vLive - vA) / pathResToSource(pA, SS_SRCA_2K49)) * 1e3f;
      allSafeHiZ();
      if (i_mA > 0.05f && (vA - vK) < 3.45f) {
        DiodeResult dr = characterizeDiode(pA, pK, kCom);
        if (dr.valid) {
          vf_V = dr.vf2mA_mV * 0.001f;
          dmm.isZener = dr.isZener;
          if (dr.isZener) {
            dmm.secondaryVal = dr.vz_V;
            snprintf(dmm.noteStr, sizeof(dmm.noteStr), "ZENER Vz=%.2fV", dr.vz_V);
          } else {
            dmm.secondaryVal = dr.vf0mA1_mV;
            snprintf(dmm.noteStr, sizeof(dmm.noteStr), "Vf.1=%.0fmV %s",
                     dr.vf0mA1_mV, dr.isLED ? "LED" : (dr.vf2mA_mV < 420.0f ? "Ge/Sch" : "Si"));
          }
          return true;
        }
      }
      return false;
    };

    float vfV = 0.0f;
    bool got = probeDiodePair(0, -1, true, vfV);
    if (got) {
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1+ COM-");
    } else if (probeDiodePair(0, 2, false, vfV)) {
      got = true;
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1+ TP3-");
    } else if (probeDiodePair(2, 0, false, vfV)) {
      got = true;
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP3+ TP1-");
    }

    if (got) {
      dmm.valid    = true;
      dmm.openLoop = false;
      if (!dmm.prevDiodeConducted && vfV >= 0.06f && vfV <= 3.50f) {
        beepTone(2100, 45);
      }
      dmm.prevDiodeConducted = true;
      dmmPushSample(vfV);
    } else {
      dmm.valid              = false;
      dmm.openLoop           = true;
      dmm.isZener            = false;
      dmm.prevDiodeConducted = false;
      dmm.secondaryVal       = NAN;
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1 - COM");
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "OPEN / OL");
    }
  }
  else if (dmm.subMode == DMM_CAP) {
    if (!runStage0GuardAndDischarge()) {
      dmm.liveVoltWarn = true;
      dmm.valid        = false;
      dmm.openLoop     = true;
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "DISCHARGE FAIL!");
      updateLedStatusFromState();
      renderScreens();
      return;
    }

    CapResult cr = characterizeCapacitor(0, -1, true);
    if (cr.valid && cr.c_pF > 6.0f) {
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), "TP1 - COM");
    } else {
      cr = characterizeCapacitor(0, 2, false);
      snprintf(dmm.probeStr, sizeof(dmm.probeStr), cr.valid ? "TP1 - TP3" : "TP1 - COM");
    }

    if (cr.valid && cr.c_pF > 4.0f) {
      float cNet_pF = dmm.relActive ? fmaxf(0.0f, cr.c_pF - dmm.relZero) : cr.c_pF;
      dmm.valid        = true;
      dmm.openLoop     = false;
      dmm.secondaryVal = cr.ileak_uA;
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "Ileak:%.2fuA", cr.ileak_uA);
      dmmPushSample(cNet_pF);
    } else {
      dmm.valid        = false;
      dmm.openLoop     = true;
      dmm.secondaryVal = NAN;
      snprintf(dmm.noteStr, sizeof(dmm.noteStr), "NO CAP (<5 pF)");
    }
  }

  updateLedStatusFromState();
  renderScreens();
}

// ======================= STANDBY MODE TRANSITION =============================
void setStandbyMode(bool enter) {
  if (enter == inStandby) return;
  inStandby = enter;

  if (inStandby) {
    stopBuzzerAll();
    if (drift.active) stopThermalDrift();
    abortRequested = true;
    allSafeHiZ();
    setAnalogRailEnabled(false); // ANA_EN_N = HIGH -> cut MT3608 +12V/+9V to save LiPo!
    startStandbyRadioServices();
    emitStatusJson("INFO", "Entered STANDBY (ANA_EN_N=HIGH, Wi-Fi Web API active)");
  } else {
    ensureRadioOffForAnalog();
    setAnalogRailEnabled(true); // ANA_EN_N = LOW -> power up +12V/+9V analog rails
    delay(70);
    initADS1115();
    allSafeHiZ();
    beepTone(1319, 40);
    emitStatusJson("INFO", "Exited STANDBY (RF OFF, Analog rails ON)");
  }

  updateLedStatusFromState();
  renderScreens();
}

// ======================= TFT1 & TFT2 UI RENDERERS ============================
static void formatDmmValue(DmmSubMode m, float val, bool openLoop,
                           char *valBuf, size_t valLen,
                           char *unitBuf, size_t unitLen) {
  if (openLoop || isnan(val)) {
    snprintf(valBuf, valLen, "  O.L.  ");
    if (m == DMM_OHM_CONT) snprintf(unitBuf, unitLen, "Ohm");
    else if (m == DMM_CAP) snprintf(unitBuf, unitLen, "pF");
    else                   snprintf(unitBuf, unitLen, "V");
    return;
  }

  if (m == DMM_VOLT_12V || m == DMM_VOLT_30V || m == DMM_DIODE) {
    if (m == DMM_VOLT_12V && fabsf(val) < 0.9995f) {
      snprintf(valBuf, valLen, "%+6.1f", val * 1000.0f);
      snprintf(unitBuf, unitLen, "mV");
    } else if (m == DMM_VOLT_30V) {
      snprintf(valBuf, valLen, "%6.3f", val);
      snprintf(unitBuf, unitLen, "V");
    } else {
      snprintf(valBuf, valLen, "%+6.3f", val);
      snprintf(unitBuf, unitLen, "V");
    }
  } else if (m == DMM_OHM_CONT) {
    if (val >= 1e6f)      { snprintf(valBuf, valLen, "%6.3f", val * 1e-6f); snprintf(unitBuf, unitLen, "MOhm"); }
    else if (val >= 1e3f) { snprintf(valBuf, valLen, "%6.2f", val * 1e-3f); snprintf(unitBuf, unitLen, "kOhm"); }
    else                  { snprintf(valBuf, valLen, "%6.2f", val);         snprintf(unitBuf, unitLen, "Ohm");  }
  } else if (m == DMM_CAP) {
    if (val >= 1e6f)      { snprintf(valBuf, valLen, "%6.2f", val * 1e-6f); snprintf(unitBuf, unitLen, "uF"); }
    else if (val >= 1e3f) { snprintf(valBuf, valLen, "%6.2f", val * 1e-3f); snprintf(unitBuf, unitLen, "nF"); }
    else                  { snprintf(valBuf, valLen, "%6.1f", val);         snprintf(unitBuf, unitLen, "pF"); }
  }
}

static void drawStandbyHistoryOnTFT1() {
  if (!tftsOK) return;
  tft1.fillScreen(COL_BG);
  tft1.fillRect(0, 0, 320, 22, COL_DMM_HDR);
  tft1.setTextColor(COL_WHITE);
  tft1.setTextSize(1);
  tft1.setCursor(8, 7);
  tft1.printf("STANDBY LOG - HASH RING BUFFER (%d/%d)", historyCount, HISTORY_CAP);

  if (historyCount == 0) {
    tft1.setTextColor(COL_MUTED);
    tft1.setTextSize(1);
    tft1.setCursor(52, 76);
    tft1.print("No DUT characterization hashes yet.");
    return;
  }

  int rows = min(historyCount, 7);
  for (int i = 0; i < rows; i++) {
    int idx = (historyHead - 1 - i + HISTORY_CAP) % HISTORY_CAP;
    const HistoryEntry &e = historyBuf[idx];
    int y = 28 + i * 20;
    tft1.fillRoundRect(4, y, 312, 17, 2, COL_CARD);
    tft1.setTextSize(1);
    tft1.setTextColor(i == 0 ? COL_YELLOW : COL_CYAN);
    tft1.setCursor(10, y + 5);
    tft1.print(e.hash[0] ? e.hash : e.type);
    tft1.setTextColor(COL_WHITE);
    tft1.setCursor(128, y + 5);
    tft1.printf("%.30s", e.summary);
  }
}

static void drawDmmTrendScopeOnTFT1() {
  if (!tftsOK) return;
  tft1.fillScreen(COL_BG);
  tft1.fillRect(0, 0, 320, 18, COL_DMM_HDR);
  tft1.setTextColor(COL_WHITE);
  tft1.setTextSize(1);
  tft1.setCursor(6, 5);
  tft1.printf("DMM TREND SCOPE  [%s]", DMM_MODE_LABELS[dmm.subMode]);

  if (dmm.hold) {
    tft1.fillRoundRect(240, 2, 36, 14, 2, COL_ORANGE);
    tft1.setTextColor(COL_BG);
    tft1.setCursor(246, 5);
    tft1.print("HOLD");
  }
  if (dmm.relActive) {
    tft1.fillRoundRect(280, 2, 34, 14, 2, COL_CYAN);
    tft1.setTextColor(COL_BG);
    tft1.setCursor(288, 5);
    tft1.print("REL");
  }

  const int sx = 48, sy = 22, sw = 264, sh = 104;
  for (int c = 0; c <= 8; c++) {
    int x = sx + (c * sw) / 8;
    tft1.drawFastVLine(x, sy, sh, (c == 0 || c == 8) ? COL_BORDER : COL_GRID);
  }
  for (int r = 0; r <= 4; r++) {
    int y = sy + (r * sh) / 4;
    tft1.drawFastHLine(sx, y, sw, (r == 0 || r == 4) ? COL_BORDER : COL_GRID);
  }

  float yMin = 0.0f, yMax = 1.0f;
  if (dmm.histCount > 0) {
    yMin = yMax = dmm.hist[(dmm.histHead - dmm.histCount + DMM_HIST_LEN) % DMM_HIST_LEN];
    for (int i = 0; i < dmm.histCount; i++) {
      int idx = (dmm.histHead - dmm.histCount + i + DMM_HIST_LEN) % DMM_HIST_LEN;
      float v = dmm.hist[idx];
      if (v < yMin) yMin = v;
      if (v > yMax) yMax = v;
    }
    float span = yMax - yMin;
    if (span < 1e-4f) span = fmaxf(0.05f, fabsf(yMax) * 0.10f);
    yMin -= 0.10f * span;
    yMax += 0.10f * span;
  }

  char lTop[12], lBot[12], uBuf[12];
  formatDmmValue(dmm.subMode, yMax, false, lTop, sizeof(lTop), uBuf, sizeof(uBuf));
  formatDmmValue(dmm.subMode, yMin, false, lBot, sizeof(lBot), uBuf, sizeof(uBuf));
  tft1.setTextColor(COL_MUTED);
  tft1.setCursor(2, sy);
  tft1.print(lTop);
  tft1.setCursor(2, sy + sh - 8);
  tft1.print(lBot);

  uint16_t traceCol = COL_CYAN;
  if (dmm.subMode == DMM_VOLT_30V) traceCol = COL_YELLOW;
  else if (dmm.subMode == DMM_OHM_CONT) traceCol = dmm.continuityActive ? COL_GREEN : COL_WHITE;
  else if (dmm.subMode == DMM_DIODE) traceCol = COL_ORANGE;
  else if (dmm.subMode == DMM_CAP) traceCol = COL_MAGENTA;

  if (dmm.histCount >= 2) {
    int prevX = sx, prevY = sy + sh / 2;
    for (int i = 0; i < dmm.histCount; i++) {
      int idx = (dmm.histHead - dmm.histCount + i + DMM_HIST_LEN) % DMM_HIST_LEN;
      float v = dmm.hist[idx];
      float fn = constrain((v - yMin) / fmaxf(1e-6f, yMax - yMin), 0.0f, 1.0f);
      int px = sx + (i * (sw - 1)) / (DMM_HIST_LEN - 1);
      int py = (sy + sh - 1) - (int)lroundf(fn * (float)(sh - 1));
      if (i > 0) {
        tft1.drawLine(prevX, prevY, px, py, traceCol);
        tft1.drawLine(prevX, max(sy, prevY - 1), px, max(sy, py - 1), traceCol);
      }
      prevX = px; prevY = py;
    }
  } else {
    tft1.setTextColor(COL_MUTED);
    tft1.setCursor(sx + 60, sy + 48);
    tft1.print("Acquiring trend samples...");
  }

  float fullScale = 12.0f;
  if (dmm.subMode == DMM_VOLT_30V) fullScale = 33.0f;
  else if (dmm.subMode == DMM_OHM_CONT) fullScale = (dmm.primaryVal < 100.0f) ? 100.0f : (dmm.primaryVal < 10000.0f ? 10000.0f : 1e6f);
  else if (dmm.subMode == DMM_DIODE) fullScale = 3.3f;
  else if (dmm.subMode == DMM_CAP) fullScale = (dmm.primaryVal < 1000.0f) ? 1000.0f : 1e6f;

  float barFrac = (dmm.valid && !dmm.openLoop)
                      ? constrain(fabsf(dmm.primaryVal) / fullScale, 0.0f, 1.0f)
                      : 0.0f;
  const int bx = 12, by = 138, bw = 296, bh = 16;
  tft1.drawRect(bx, by, bw, bh, COL_BORDER);
  int segs = 36;
  int litSegs = (int)lroundf(barFrac * (float)segs);
  int segW = (bw - 4) / segs;
  for (int s = 0; s < segs; s++) {
    int sxPos = bx + 2 + s * segW;
    uint16_t scol = (s < litSegs) ? ((s > 30) ? COL_YELLOW : traceCol) : COL_GRID;
    tft1.fillRect(sxPos, by + 2, max(2, segW - 2), bh - 4, scol);
  }
  tft1.setTextColor(COL_MUTED);
  tft1.setCursor(12, 158);
  tft1.print("0%");
  tft1.setCursor(150, 158);
  tft1.print("50%");
  tft1.setCursor(286, 158);
  tft1.print("100%");
}

static void drawThermalDriftOnTFT1() {
  if (!tftsOK) return;
  tft1.fillScreen(COL_BG);
  tft1.fillRect(0, 0, 320, 18, COL_SIDE_HDR);
  tft1.setTextColor(COL_WHITE);
  tft1.setTextSize(1);
  tft1.setCursor(6, 5);
  tft1.printf("THERMAL DRIFT LOGGER  [%s]", drift.active ? "RUNNING 1Hz" : "IDLE");

  const int sx = 40, sy = 22, sw = 272, sh = 126;
  for (int c = 0; c <= 8; c++) {
    int x = sx + (c * sw) / 8;
    tft1.drawFastVLine(x, sy, sh, (c == 0 || c == 8) ? COL_BORDER : COL_GRID);
  }
  for (int r = 0; r <= 5; r++) {
    int y = sy + (r * sh) / 5;
    tft1.drawFastHLine(sx, y, sw, (r == 0 || r == 5) ? COL_BORDER : COL_GRID);
  }

  if (drift.histCount >= 2) {
    float hMin = 1e9f, hMax = -1e9f, iMin = 1e9f, iMax = -1e9f;
    for (int i = 0; i < drift.histCount; i++) {
      int idx = (drift.histHead - drift.histCount + i + DRIFT_HIST_LEN) % DRIFT_HIST_LEN;
      if (drift.histHfe[idx] < hMin)  hMin = drift.histHfe[idx];
      if (drift.histHfe[idx] > hMax)  hMax = drift.histHfe[idx];
      if (drift.histIceo[idx] < iMin) iMin = drift.histIceo[idx];
      if (drift.histIceo[idx] > iMax) iMax = drift.histIceo[idx];
    }
    if ((hMax - hMin) < 5.0f) { hMin -= 2.5f; hMax += 2.5f; }
    if ((iMax - iMin) < 5.0f) { iMin -= 2.5f; iMax += 2.5f; }

    int pxH = sx, pyH = sy + sh / 2, pxI = sx, pyI = sy + sh / 2;
    for (int i = 0; i < drift.histCount; i++) {
      int idx = (drift.histHead - drift.histCount + i + DRIFT_HIST_LEN) % DRIFT_HIST_LEN;
      int px = sx + (i * (sw - 1)) / (DRIFT_HIST_LEN - 1);
      float fh = constrain((drift.histHfe[idx] - hMin) / (hMax - hMin), 0.0f, 1.0f);
      float fi = constrain((drift.histIceo[idx] - iMin) / (iMax - iMin), 0.0f, 1.0f);
      int yH = (sy + sh - 1) - (int)lroundf(fh * (float)(sh - 1));
      int yI = (sy + sh - 1) - (int)lroundf(fi * (float)(sh - 1));
      if (i > 0) {
        tft1.drawLine(pxH, pyH, px, yH, COL_CYAN);
        tft1.drawLine(pxI, pyI, px, yI, COL_ORANGE);
      }
      pxH = px; pyH = yH; pxI = px; pyI = yI;
    }
    tft1.setTextColor(COL_CYAN);   tft1.setCursor(4, sy + 4);  tft1.print("hFE");
    tft1.setTextColor(COL_ORANGE); tft1.setCursor(4, sy + 18); tft1.print("Iceo");
  } else {
    tft1.setTextColor(COL_MUTED);
    tft1.setCursor(72, 80);
    tft1.print("Press BTN A to start 1 Hz drift trace");
  }
  tft1.setTextColor(COL_MUTED);
  tft1.setCursor(42, 155);
  tft1.print("CYAN: hFE(true)   AMBER: Iceo(uA)");
}

void renderTFT1() {
  if (!tftsOK) return;
  if (inStandby) {
    drawStandbyHistoryOnTFT1();
  } else if (consoleMode == MODE_MULTIMETER) {
    drawDmmTrendScopeOnTFT1();
  } else if (consoleMode == MODE_THERMAL_DRIFT) {
    drawThermalDriftOnTFT1();
  } else {
    drawStoredCurvesOnTFT1();
  }
}

static void drawHashBadgeTFT2(const char *hashStr) {
  if (hashStr == nullptr || hashStr[0] == '\0') return;
  tft2.fillRoundRect(190, 36, 124, 21, 3, COL_CARD);
  tft2.drawRoundRect(190, 36, 124, 21, 3, COL_CYAN);
  tft2.setTextSize(1);
  tft2.setTextColor(COL_YELLOW);
  int xPos = 252 - ((int)strlen(hashStr) * 6) / 2;
  if (xPos < 194) xPos = 194;
  tft2.setCursor(xPos, 43);
  tft2.print(hashStr);
}

void renderTFT2() {
  if (!tftsOK) return;
  tft2.fillScreen(COL_BG);
  float tC = getTempC();

  if (inStandby) {
    tft2.fillRect(0, 0, 320, 30, COL_DMM_HDR);
    tft2.setTextColor(COL_WHITE);
    tft2.setTextSize(2);
    tft2.setCursor(8, 7);
    tft2.print("STANDBY / SYNC");
    tft2.fillRoundRect(204, 5, 110, 20, 3, COL_ORANGE);
    tft2.setTextColor(COL_BG);
    tft2.setTextSize(1);
    tft2.setCursor(214, 11);
    tft2.print("ANA RAIL: OFF");

    tft2.fillRoundRect(8, 36, 304, 54, 4, COL_CARD);
    tft2.drawRoundRect(8, 36, 304, 54, 4, COL_CYAN);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_LABEL);
    tft2.setCursor(16, 42);
    tft2.print("WI-FI SOFTAP / WEB API (:80)");
    tft2.setTextColor(COL_WHITE);
    tft2.setTextSize(2);
    tft2.setCursor(16, 56);
    tft2.printf("IP: %s", activeIpStr);

    tft2.fillRoundRect(8, 96, 304, 48, 4, COL_CARD);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_CYAN);
    tft2.setCursor(16, 103);
    tft2.print("AP: KAOTI-Ge-PRO   GET /api/latest /history");
    tft2.setTextColor(COL_YELLOW);
    tft2.setCursor(16, 122);
    if (historyCount > 0) {
      int latestIdx = (historyHead - 1 + HISTORY_CAP) % HISTORY_CAP;
      tft2.printf("LAST HASH: %s", historyBuf[latestIdx].hash);
    } else {
      tft2.print("LAST HASH: NONE");
    }

    tft2.drawFastHLine(0, 151, 320, COL_GRID);
    tft2.setTextColor(COL_MUTED);
    tft2.setTextSize(1);
    tft2.setCursor(8, 157);
    tft2.printf("BLE:%s  SW_STBY LOW -> Slide HIGH for Test",
                ENABLE_BLE_TELEMETRY ? "ON" : "MOD");
    return;
  }

  if (consoleMode == MODE_MULTIMETER) {
    tft2.fillRect(0, 0, 320, 30, COL_DMM_HDR);
    tft2.setTextColor(COL_WHITE);
    tft2.setTextSize(2);
    tft2.setCursor(8, 7);
    tft2.print("DMM ");
    tft2.setTextColor(COL_YELLOW);
    tft2.print(DMM_MODE_LABELS[dmm.subMode]);

    if (dmm.hold) {
      tft2.fillRoundRect(202, 5, 52, 20, 3, COL_ORANGE);
      tft2.setTextColor(COL_BG);
      tft2.setTextSize(1);
      tft2.setCursor(215, 11);
      tft2.print("HOLD");
    }
    if (dmm.continuityActive) {
      tft2.fillRoundRect(260, 5, 54, 20, 3, COL_GREEN);
      tft2.setTextColor(COL_BG);
      tft2.setTextSize(1);
      tft2.setCursor(272, 11);
      tft2.print("BEEP!");
    } else if (dmm.relActive) {
      tft2.fillRoundRect(260, 5, 54, 20, 3, COL_CYAN);
      tft2.setTextColor(COL_BG);
      tft2.setTextSize(1);
      tft2.setCursor(275, 11);
      tft2.print("REL");
    }

    tft2.fillRoundRect(6, 36, 308, 62, 5, COL_CARD);
    tft2.drawRoundRect(6, 36, 308, 62, 5, dmm.liveVoltWarn ? COL_RED : COL_BORDER);

    if (dmm.liveVoltWarn) {
      tft2.setTextColor(COL_RED);
      tft2.setTextSize(2);
      tft2.setCursor(20, 56);
      tft2.print(dmm.noteStr);
    } else {
      char valStr[16], unitStr[12];
      formatDmmValue(dmm.subMode, dmm.primaryVal, dmm.openLoop,
                     valStr, sizeof(valStr), unitStr, sizeof(unitStr));
      tft2.setTextColor(dmm.continuityActive ? COL_GREEN : COL_WHITE);
      tft2.setTextSize(4);
      tft2.setCursor(16, 51);
      tft2.print(valStr);
      tft2.setTextColor(COL_CYAN);
      tft2.setTextSize(3);
      tft2.setCursor(228, 56);
      tft2.print(unitStr);
    }

    tft2.fillRoundRect(6, 104, 150, 42, 4, COL_CARD);
    tft2.fillRoundRect(164, 104, 150, 42, 4, COL_CARD);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_LABEL);
    tft2.setCursor(14, 110);
    tft2.print("MIN / MAX");
    tft2.setTextColor(COL_WHITE);
    if (dmm.sampleCount > 0) {
      char sMin[14], sMax[14], uTmp[10];
      formatDmmValue(dmm.subMode, dmm.minVal, false, sMin, sizeof(sMin), uTmp, sizeof(uTmp));
      formatDmmValue(dmm.subMode, dmm.maxVal, false, sMax, sizeof(sMax), uTmp, sizeof(uTmp));
      tft2.setCursor(14, 124); tft2.printf("Lo:%s", sMin);
      tft2.setCursor(14, 135); tft2.printf("Hi:%s", sMax);
    } else {
      tft2.setCursor(14, 128); tft2.print("-- / --");
    }

    tft2.setTextColor(COL_LABEL);
    tft2.setCursor(172, 110);
    tft2.printf("PROBE: %s", dmm.probeStr);
    tft2.setTextColor(COL_YELLOW);
    tft2.setCursor(172, 126);
    tft2.print(dmm.noteStr);

    tft2.drawFastHLine(0, 151, 320, COL_GRID);
    tft2.setTextColor(COL_MUTED);
    tft2.setTextSize(1);
    tft2.setCursor(8, 157);
    if (!isnan(tC)) tft2.printf("T:%.1fC", tC);
    else            tft2.print("T:--C");
    tft2.setCursor(76, 157);
    tft2.print("L/R:Range  A:Hold  B:Mode");
    return;
  }

  if (consoleMode == MODE_THERMAL_DRIFT) {
    tft2.fillRect(0, 0, 320, 30, drift.active ? COL_GE_BADGE : COL_SIDE_HDR);
    tft2.setTextColor(COL_WHITE);
    tft2.setTextSize(2);
    tft2.setCursor(10, 7);
    tft2.printf("DRIFT [%s]", drift.active ? "RUN 1Hz" : "IDLE");
    tft2.setTextSize(1);
    tft2.setCursor(220, 11);
    tft2.printf("N=%lu", (unsigned long)drift.sampleCount);

    if (drift.sampleCount > 0) {
      tft2.setTextColor(COL_LABEL); tft2.setCursor(12, 40); tft2.print("hFE(true)");
      tft2.setTextColor(COL_WHITE); tft2.setTextSize(3); tft2.setCursor(12, 52);
      if (!isnan(drift.lastHfe)) tft2.printf("%.1f", drift.lastHfe);
      else                       tft2.print("--");

      tft2.fillRoundRect(160, 38, 152, 46, 4, COL_CARD);
      tft2.setTextSize(1);
      tft2.setTextColor(COL_LABEL); tft2.setCursor(168, 44); tft2.print("TEMP");
      tft2.setTextColor(COL_YELLOW); tft2.setTextSize(2); tft2.setCursor(168, 58);
      if (!isnan(drift.lastTempC)) tft2.printf("%.1fC", drift.lastTempC);
      else                         tft2.print("--.-C");

      tft2.fillRoundRect(8, 92, 304, 52, 4, COL_CARD);
      tft2.setTextSize(1);
      tft2.setTextColor(COL_WHITE);
      tft2.setCursor(16, 100);
      tft2.printf("Iceo: %7.1f uA      Icbo: %7.1f uA", drift.lastIceo_uA, drift.lastIcbo_uA);
      tft2.setCursor(16, 122);
      tft2.printf("Vbe : %7.0f mV      Elapsed: %5.1fs",
                  drift.lastVbe_mV, (float)(drift.lastSampleMs - drift.startMs) * 0.001f);
    } else {
      tft2.setTextColor(COL_CYAN); tft2.setTextSize(2); tft2.setCursor(16, 62);
      tft2.print("1 Hz Ge Thermal Tracker");
      tft2.setTextColor(COL_MUTED); tft2.setTextSize(1); tft2.setCursor(16, 98);
      tft2.print("Press BTN A (or 'd') to start/stop logging");
    }
    tft2.drawFastHLine(0, 151, 320, COL_GRID);
    tft2.setTextColor(COL_MUTED); tft2.setTextSize(1); tft2.setCursor(8, 157);
    tft2.print("BTN A:Start/Stop  BTN B:Switch Mode");
    return;
  }

  // --- AUTO_TESTER & CURVE_TRACER PARAMETRIC READOUT SCREEN ---
  uint16_t hdrCol = COL_SIDE_HDR;
  char badgeTxt[24] = "Ge-LAB PRO v4";
  char pinTxt[36]   = "READY";

  if (lastScan.kind == DUT_ABORT) {
    hdrCol = COL_ERR_BDG;
    snprintf(badgeTxt, sizeof(badgeTxt), "SAFETY ABORT!");
    snprintf(pinTxt, sizeof(pinTxt), "V=%.2fV", lastScan.abortV);
  } else if (lastScan.kind == DUT_BJT) {
    const BjtResult &b = lastScan.bjt;
    hdrCol = b.isGermanium ? COL_GE_BADGE : COL_SI_BADGE;
    snprintf(badgeTxt, sizeof(badgeTxt), "%s %s", b.isGermanium ? "Ge" : "Si", b.isPNP ? "PNP" : "NPN");
    char r[3] = {'?', '?', '?'};
    r[b.pinE] = 'E'; r[b.pinB] = 'B'; r[b.pinC] = 'C';
    snprintf(pinTxt, sizeof(pinTxt), "1:%c 2:%c 3:%c", r[0], r[1], r[2]);
  } else if (lastScan.kind == DUT_JFET) {
    const JfetResult &j = lastScan.jfet;
    hdrCol = COL_JFET_BDG;
    snprintf(badgeTxt, sizeof(badgeTxt), "%s", j.isP ? "P-JFET" : "N-JFET");
    char r[3] = {'?', '?', '?'};
    r[j.pinD] = 'D'; r[j.pinG] = 'G'; r[j.pinS] = 'S';
    snprintf(pinTxt, sizeof(pinTxt), "1:%c 2:%c 3:%c", r[0], r[1], r[2]);
  } else if (lastScan.kind == DUT_MOSFET) {
    const MosfetResult &m = lastScan.mos;
    hdrCol = COL_MOS_BDG;
    snprintf(badgeTxt, sizeof(badgeTxt), "%s", m.isP ? "P-MOSFET" : "N-MOSFET");
    char r[3] = {'?', '?', '?'};
    r[m.pinG] = 'G'; r[m.pinD] = 'D'; r[m.pinS] = 'S';
    snprintf(pinTxt, sizeof(pinTxt), "1:%c 2:%c 3:%c", r[0], r[1], r[2]);
  } else if (lastScan.kind == DUT_DIODE) {
    const DiodeResult &d = lastScan.diode;
    hdrCol = COL_PASS_BDG;
    snprintf(badgeTxt, sizeof(badgeTxt), "%s",
             d.isDual ? "DUAL DIODE" : (d.isZener ? "ZENER" : (d.isLED ? "LED" : "DIODE")));
    snprintf(pinTxt, sizeof(pinTxt), "TP%d->TP%d",
             constrain(d.pinA, 0, 2) + 1, constrain(d.pinK, 0, 2) + 1);
  } else if (lastScan.kind == DUT_RESISTOR) {
    hdrCol = COL_PASS_BDG;
    snprintf(badgeTxt, sizeof(badgeTxt), "RESISTOR");
    snprintf(pinTxt, sizeof(pinTxt), "TP%d-TP%d",
             constrain(lastScan.res.pinA, 0, 2) + 1, constrain(lastScan.res.pinB, 0, 2) + 1);
  } else if (lastScan.kind == DUT_CAPACITOR) {
    hdrCol = COL_PASS_BDG;
    snprintf(badgeTxt, sizeof(badgeTxt), "CAPACITOR");
    snprintf(pinTxt, sizeof(pinTxt), "TP%d-TP%d",
             constrain(lastScan.cap.pinA, 0, 2) + 1, constrain(lastScan.cap.pinB, 0, 2) + 1);
  }

  tft2.fillRect(0, 0, 320, 32, hdrCol);
  tft2.setTextColor(COL_WHITE);
  tft2.setTextSize(2);
  tft2.setCursor(8, 8);
  tft2.print(badgeTxt);
  tft2.setTextColor(COL_YELLOW);
  int pinX = 314 - (int)strlen(pinTxt) * 12;
  if (pinX < 176) pinX = 176;
  tft2.setCursor(pinX, 8);
  tft2.print(pinTxt);

  if (lastScan.kind == DUT_ABORT) {
    tft2.setTextColor(COL_RED); tft2.setTextSize(2); tft2.setCursor(12, 54);
    tft2.print(lastScan.abortReason);
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(1); tft2.setCursor(12, 92);
    tft2.print("Disconnect live voltage from ZIF / terminals!");
  } else if (lastScan.kind == DUT_BJT) {
    const BjtResult &b = lastScan.bjt;
    tft2.setTextColor(COL_LABEL); tft2.setTextSize(1); tft2.setCursor(10, 38);
    tft2.printf("hFE(true)   (raw %.1f)", b.hfeApparent);
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(3); tft2.setCursor(10, 50);
    tft2.printf("%.1f", b.hfeTrue);

    drawHashBadgeTFT2(lastScan.dutHash);

    if (b.leaky) {
      tft2.fillRoundRect(244, 60, 70, 17, 3, COL_ERR_BDG);
      tft2.setTextColor(COL_WHITE); tft2.setTextSize(1); tft2.setCursor(259, 65);
      tft2.print("LEAKY!");
    } else if (b.powerClamped) {
      tft2.fillRoundRect(244, 60, 70, 17, 3, COL_ORANGE);
      tft2.setTextColor(COL_BG); tft2.setTextSize(1); tft2.setCursor(256, 65);
      tft2.print("CLAMPED");
    }

    tft2.fillRoundRect(8, 82, 148, 30, 3, COL_CARD);
    tft2.fillRoundRect(164, 82, 148, 30, 3, COL_CARD);
    tft2.fillRoundRect(8, 116, 148, 30, 3, COL_CARD);
    tft2.fillRoundRect(164, 116, 148, 30, 3, COL_CARD);

    tft2.setTextSize(1);
    tft2.setTextColor(COL_LABEL); tft2.setCursor(14, 86);   tft2.print("Iceo (leak):");
    tft2.setTextColor(COL_WHITE); tft2.setCursor(14, 98);   tft2.printf("%.1f uA", b.iceo_uA);
    tft2.setTextColor(COL_LABEL); tft2.setCursor(170, 86);  tft2.print("Icbo (C-B):");
    tft2.setTextColor(COL_WHITE); tft2.setCursor(170, 98);  tft2.printf("%.1f uA", b.icbo_uA);
    tft2.setTextColor(COL_LABEL); tft2.setCursor(14, 120);  tft2.print("Vbe (fwd):");
    tft2.setTextColor(COL_CYAN);  tft2.setCursor(14, 132);  tft2.printf("%.0f mV", b.vbe_mV);
    tft2.setTextColor(COL_LABEL); tft2.setCursor(170, 120); tft2.print("Vce(sat) B=10:");
    tft2.setTextColor(COL_GREEN); tft2.setCursor(170, 132); tft2.printf("%.0f mV", b.vceSat_mV);
  } else if (lastScan.kind == DUT_JFET) {
    const JfetResult &j = lastScan.jfet;
    tft2.setTextColor(COL_LABEL); tft2.setTextSize(1); tft2.setCursor(10, 40); tft2.print("Idss:");
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(3); tft2.setCursor(10, 52); tft2.printf("%.2fmA", j.idss_mA);
    drawHashBadgeTFT2(lastScan.dutHash);
    tft2.fillRoundRect(8, 92, 148, 48, 4, COL_CARD);
    tft2.fillRoundRect(164, 92, 148, 48, 4, COL_CARD);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_LABEL); tft2.setCursor(14, 98);  tft2.print("Vgs(off):");
    tft2.setTextColor(COL_CYAN);  tft2.setCursor(14, 114); tft2.printf("%.2f V", j.vgsoff_V);
    tft2.setTextColor(COL_LABEL); tft2.setCursor(170, 98); tft2.print("gm / Vf(G):");
    tft2.setTextColor(COL_GREEN); tft2.setCursor(170, 114);
    tft2.printf("%.2fmS / %.0fmV", isnan(j.gm_mS) ? 0.0f : j.gm_mS, j.vfGate_mV);
  } else if (lastScan.kind == DUT_MOSFET) {
    const MosfetResult &m = lastScan.mos;
    tft2.setTextColor(COL_LABEL); tft2.setTextSize(1); tft2.setCursor(10, 40); tft2.print("Vgs(th):");
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(3); tft2.setCursor(10, 52); tft2.printf("%.2f V", m.vth_V);
    drawHashBadgeTFT2(lastScan.dutHash);
    tft2.fillRoundRect(8, 92, 148, 48, 4, COL_CARD);
    tft2.fillRoundRect(164, 92, 148, 48, 4, COL_CARD);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_LABEL); tft2.setCursor(14, 98);  tft2.print("Rds(on) @4.5 / 8.5V:");
    tft2.setTextColor(COL_CYAN);  tft2.setCursor(14, 114);
    if (!isnan(m.rds4v5_ohm)) tft2.printf("%.2fR / %.2fR", m.rds4v5_ohm, isnan(m.rds8v5_ohm) ? 0.0f : m.rds8v5_ohm);
    else                      tft2.print(">HiZ @4.5V");
    tft2.setTextColor(COL_LABEL); tft2.setCursor(170, 98); tft2.print("Body Diode Vf:");
    tft2.setTextColor(COL_GREEN); tft2.setCursor(170, 114); tft2.printf("%.0f mV", m.vfBody_mV);
  } else if (lastScan.kind == DUT_DIODE) {
    const DiodeResult &d = lastScan.diode;
    tft2.setTextColor(COL_LABEL); tft2.setTextSize(1);
    tft2.setCursor(10, 40); tft2.print(d.isZener ? "Zener Vz @1mA:" : "Forward Vf @2mA:");
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(3); tft2.setCursor(10, 52);
    if (d.isZener) tft2.printf("%.2f V", d.vz_V);
    else           tft2.printf("%.0fmV", d.vf2mA_mV);
    drawHashBadgeTFT2(lastScan.dutHash);
    tft2.fillRoundRect(8, 92, 304, 48, 4, COL_CARD);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_CYAN); tft2.setCursor(14, 102);
    tft2.printf("Vf@0.1mA: %.0fmV   Vf@2mA: %.0fmV", d.vf0mA1_mV, d.vf2mA_mV);
    tft2.setTextColor(COL_YELLOW); tft2.setCursor(14, 120);
    tft2.printf("Irev: %.2f uA   Type: %s", d.irev_uA,
                d.isZener ? "ZENER" : (d.isLED ? "LED" : (d.vf2mA_mV < 420.0f ? "Ge/Schottky" : "Silicon")));
  } else if (lastScan.kind == DUT_RESISTOR) {
    const ResistorResult &r = lastScan.res;
    tft2.setTextColor(COL_LABEL); tft2.setTextSize(1); tft2.setCursor(10, 40); tft2.print("Resistance:");
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(3); tft2.setCursor(10, 54);
    if (r.r_ohm >= 1e6f)      tft2.printf("%.2fMO", r.r_ohm * 1e-6f);
    else if (r.r_ohm >= 1e3f) tft2.printf("%.2fkO", r.r_ohm * 1e-3f);
    else                      tft2.printf("%.1f O", r.r_ohm);
    drawHashBadgeTFT2(lastScan.dutHash);
    tft2.fillRoundRect(8, 96, 304, 44, 4, COL_CARD);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_CYAN); tft2.setCursor(14, 112);
    tft2.printf("Vdut: %.3f V   I: %.3f mA   Rng: %.0fR", r.vDut_V, r.i_mA, r.range_ohm);
  } else if (lastScan.kind == DUT_CAPACITOR) {
    const CapResult &c = lastScan.cap;
    tft2.setTextColor(COL_LABEL); tft2.setTextSize(1); tft2.setCursor(10, 40); tft2.print("Capacitance:");
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(3); tft2.setCursor(10, 54);
    if (c.c_pF >= 1e6f)      tft2.printf("%.2fuF", c.c_pF * 1e-6f);
    else if (c.c_pF >= 1e3f) tft2.printf("%.2fnF", c.c_pF * 1e-3f);
    else                     tft2.printf("%.0fpF", c.c_pF);
    drawHashBadgeTFT2(lastScan.dutHash);
    tft2.fillRoundRect(8, 96, 304, 44, 4, COL_CARD);
    tft2.setTextSize(1);
    tft2.setTextColor(COL_CYAN); tft2.setCursor(14, 112);
    tft2.printf("Ileak: %.2f uA   Droop: %.1f mV/s", c.ileak_uA, c.droop_mVs);
  } else {
    tft2.setTextColor(COL_CYAN); tft2.setTextSize(2); tft2.setCursor(14, 48);
    tft2.print("AUTO SCAN READY");
    tft2.setTextColor(COL_WHITE); tft2.setTextSize(1);
    tft2.setCursor(14, 78);  tft2.print("BTN A: Auto-Scan (Hold >=1.2s: Auto-Cal)");
    tft2.setCursor(14, 96);  tft2.print("BTN B: Cycle [AUTO / CURVE / DMM / DRIFT]");
    tft2.setCursor(14, 114); tft2.print("D-PAD / KNOBS: B Range, Vce Max, Cursor");
  }

  tft2.drawFastHLine(0, 151, 320, COL_GRID);
  tft2.setTextColor(COL_MUTED);
  tft2.setTextSize(1);
  tft2.setCursor(8, 157);
  if (!isnan(tC)) tft2.printf("T:%.1fC", tC);
  else            tft2.print("T:--C");
  tft2.setCursor(68, 157);
  tft2.print(cal.valid ? "CAL:NVS" : "CAL:DEF");
  tft2.setCursor(124, 157);
  tft2.printf("MODE:%s", (consoleMode == MODE_CURVE_TRACER) ? "CURVE" : "AUTO");
  if (lastScan.kind == DUT_BJT) {
    tft2.setCursor(214, 157);
    tft2.printf("Pmax:%.1fmW", lastScan.bjt.peakPower_mW);
  }
}

void renderScreens() {
  renderTFT1();
  renderTFT2();
}

// ======================= 10-STEP BENCHTOP SELF-TEST SUITE ====================
void runHardwareSelfTest() {
  if (inStandby) {
    emitStatusJson("ERROR", "Exit Standby (SW_STBY HIGH) before running Self-Test");
    return;
  }
  ensureRadioOffForAnalog();
  isMeasurementBusy = true;
  setStatusLeds(false, false, true, false, false);

  Serial.println("\n================ KAOTI Ge-LAB PRO REV 4.0 SELF-TEST ================");
  int passCnt = 0, failCnt = 0;

  auto reportStep = [&](int num, const char *name, bool ok, const char *detail) {
    if (ok) passCnt++;
    else    failCnt++;
    Serial.printf("  [%02d] %-32s : %-4s (%s)\n",
                  num, name, ok ? "PASS" : "FAIL", detail);
    if (tftsOK) {
      showCalBannerTft(name, detail, ok ? COL_PASS_BDG : COL_ERR_BDG);
    }
  };

  // Step 1: I2C Bus Scan (0x20 U11, 0x21 U12, 0x48 ADS1115)
  bool ack20 = probeI2CDevice(I2C_ADDR_U11);
  bool ack21 = probeI2CDevice(I2C_ADDR_U12);
  bool ack48 = probeI2CDevice(I2C_ADDR_ADS1115);
  char d1[64];
  snprintf(d1, sizeof(d1), "0x20:%d 0x21:%d 0x48:%d", ack20, ack21, ack48);
  reportStep(1, "I2C Bus (U11/U12/ADS1115)", ack20 && ack21 && ack48, d1);

  // Step 2: Dual MCP23017 Registers & SW_STBY / ANA_EN_N state
  uint8_t u11A = 0, u11B = 0, u12IoA = 0xFF, u12OlA = 0;
  bool rOk = mcpReadPorts(I2C_ADDR_U11, u11A, u11B) &&
             mcpReadReg(I2C_ADDR_U12, MCP_REG_IODIRA, u12IoA) &&
             mcpReadReg(I2C_ADDR_U12, MCP_REG_OLATA, u12OlA);
  bool anaEn = ((u12OlA >> U12_GPA_ANA_EN_N) & 0x01) == 0;
  char d2[64];
  snprintf(d2, sizeof(d2), "U11=0x%02X/%02X U12_ANA=%s", u11A, u11B, anaEn ? "ON" : "OFF");
  reportStep(2, "MCP23017 U11/U12 & ANA_EN_N", rOk && (u12IoA == 0x00) && anaEn, d2);

  // Step 3: +9V_LIM Active Limiter Rail Check (8.20 - 9.40 V)
  allSafeHiZ();
  float v9 = readLiveRefV(CAL_9VLIM, 6);
  bool ok9 = (v9 >= 8.20f && v9 <= 9.40f);
  char d3[48];
  snprintf(d3, sizeof(d3), "+9V_LIM=%.3fV (spec 8.20-9.40V)", v9);
  reportStep(3, "+9V_LIM Active Rail", ok9, d3);

  // Step 4: MCP4822 DAC V_SRC_A Linearity Sweep (0V / 3V / 6V / 9V)
  const float pts[4] = {0.00f, 3.00f, 6.00f, 9.00f};
  float maxErrA = 0.0f;
  for (int i = 0; i < 4; i++) {
    setSourceA(pts[i], true);
    float m = readLiveRefV(CAL_SRCA, 4);
    float e = fabsf(m - pts[i]);
    if (e > maxErrA) maxErrA = e;
  }
  allSafeHiZ();
  char d4[48];
  snprintf(d4, sizeof(d4), "MaxErr=%.3fV (0/3/6/9V)", maxErrA);
  reportStep(4, "MCP4822 DAC A Linearity", maxErrA < 0.30f, d4);

  // Step 5: MCP4822 DAC V_SRC_B Linearity Sweep (0V / 3V / 6V / 9V)
  float maxErrB = 0.0f;
  for (int i = 0; i < 4; i++) {
    setSourceB(pts[i], true);
    float m = readLiveRefV(CAL_SRCB, 4);
    float e = fabsf(m - pts[i]);
    if (e > maxErrB) maxErrB = e;
  }
  allSafeHiZ();
  char d5[48];
  snprintf(d5, sizeof(d5), "MaxErr=%.3fV (0/3/6/9V)", maxErrB);
  reportStep(5, "MCP4822 DAC B Linearity", maxErrB < 0.30f, d5);

  // Step 6: U10 DG409DJ 4-Channel Cal & 30V Mux Check
  setSourcesAB(4.00f, 7.00f, true);
  float m0 = readLiveRefV(CAL_SRCA, 4);
  float m1 = readLiveRefV(CAL_SRCB, 4);
  float m2 = readLiveRefV(CAL_9VLIM, 4);
  float m3 = readLiveRefV(CAL_DMM30V, 4);
  allSafeHiZ();
  bool okMux = (fabsf(m0 - 4.00f) < 0.30f) && (fabsf(m1 - 7.00f) < 0.30f) &&
               (m2 >= 8.20f) && (m3 >= -0.05f && m3 <= 9.0f);
  char d6[64];
  snprintf(d6, sizeof(d6), "S1=%.2f S2=%.2f S3=%.2f S4=%.2fV", m0, m1, m2, m3);
  reportStep(6, "U10 DG409DJ 4-Ch Mux", okMux, d6);

  // Step 7: U1..U3 DG408DJ + U8 DG412LDJ Matrix & Ron Check
  allSafeHiZ();
  delay(4);
  float vIdle[4];
  readAllNodes(vIdle, 2);
  bool sockFree = (fabsf(vIdle[0]) < 0.25f && fabsf(vIdle[1]) < 0.25f && fabsf(vIdle[2]) < 0.25f);
  bool ronOk = sockFree;
  float avg408 = 0.0f, avg412 = 0.0f;
  int goodCh = 0;
  if (sockFree) {
    setSourcesAB(7.500f, 0.0f, true);
    float vSrcM = readLiveRefV(CAL_SRCA, 4);
    for (int i = 0; i < 3; i++) {
      uint16_t bit412 = (uint16_t)(1u << (12 + i));
      uint16_t ledBit = (uint16_t)(1u << 15);
      uint16_t w470  = ledBit | bit412 | (uint16_t)((1u << (i * 4 + 3)) | (0u << (i * 4)));
      uint16_t w2k49 = ledBit | bit412 | (uint16_t)((1u << (i * 4 + 3)) | (1u << (i * 4)));
      setMatrixRaw16(w470, true); delay(5); float v1 = readNodeV(i, 4);
      setMatrixRaw16(w2k49, true); delay(5); float v2 = readNodeV(i, 4);
      allSafeHiZ();
      if (v1 > 0.03f && v2 > 0.005f && v1 < vSrcM && v2 < v1) {
        float r1 = (vSrcM / v1) - 1.0f;
        float r2 = (vSrcM / v2) - 1.0f;
        if ((r2 - r1) > 10.0f) {
          float r412 = (R_KEEN - R_LOW) / (r2 - r1);
          float r408 = r1 * r412 - R_LOW;
          if (r412 >= 2.0f && r412 <= 100.0f && r408 >= 5.0f && r408 <= 180.0f) {
            avg412 += r412; avg408 += r408; goodCh++;
          }
        }
      }
    }
    ronOk = (goodCh == 3);
  }
  char d7[64];
  if (!sockFree) snprintf(d7, sizeof(d7), "Socket not empty/idle");
  else if (goodCh > 0) snprintf(d7, sizeof(d7), "ch=%d/3 Ron408=%.1fR Ron412=%.1fR", goodCh, avg408 / goodCh, avg412 / goodCh);
  else snprintf(d7, sizeof(d7), "Matrix path fault");
  reportStep(7, "DG408DJ + DG412LDJ Matrix", ronOk, d7);

  // Step 8: Fast ESP32 ADC1 Isolation Path (IO36/39/35)
  float maxFastErr = 0.0f;
  setSourceA(1.500f, true);
  for (int i = 0; i < 3; i++) {
    PinModeSS arr[3] = {SS_HIZ, SS_HIZ, SS_HIZ};
    arr[i] = SS_SRCA_470R;
    setMatrix(arr[0], arr[1], arr[2], true);
    delay(6);
    float va = readNodeV(i, 2);
    float vf = readFastAdcCalV(i);
    float err = fabsf(va - vf);
    if (err > maxFastErr) maxFastErr = err;
  }
  allSafeHiZ();
  char d8[48];
  snprintf(d8, sizeof(d8), "MaxADC1Err=%.3fV @1.5V", maxFastErr);
  reportStep(8, "ESP32 ADC1 Cap Paths", maxFastErr < 0.25f, d8);

  // Step 9: Top 6-LED Lightbar (U12) & 56k Modem PWM Buzzer (IO13)
  static const uint16_t chirpFreqs[6] = {1650, 1850, 2100, 1200, 2400, 2700};
  for (int i = 0; i < 6; i++) {
    uint8_t ledBit = (uint8_t)(1u << i);
    u12OlatA = (uint8_t)((u12OlatA & ((1u << U12_GPA_TFT_RES) | (1u << U12_GPA_ANA_EN_N))) | ledBit);
    mcpWriteReg(I2C_ADDR_U12, MCP_REG_OLATA, u12OlatA);
    ledcWriteTone(PIN_BZ_PWM, chirpFreqs[i]);
    delay(65);
  }
  ledcWriteTone(PIN_BZ_PWM, 0);
  reportStep(9, "U12 6-LED Bar & PWM Modem", u12OK, "6-LED sweep + 1650-2700Hz PWM");

  // Step 10: DS18B20 Probe & NVS Storage Check
  initTempSensor();
  delay(20);
  float tCheck = getTempC();
  bool nvsOk = prefs.begin(NVS_NAMESPACE, true);
  if (nvsOk) prefs.end();
  char d10[64];
  if (tempPresent && !isnan(tCheck)) snprintf(d10, sizeof(d10), "DS18B20=%.2fC NVS=%s", tCheck, nvsOk ? "OK" : "ERR");
  else if (tempPresent)              snprintf(d10, sizeof(d10), "DS18B20=FOUND NVS=%s", nvsOk ? "OK" : "ERR");
  else                               snprintf(d10, sizeof(d10), "DS18B20=NOT_FITTED NVS=%s", nvsOk ? "OK" : "ERR");
  reportStep(10, "DS18B20 & NVS Storage", tempPresent && nvsOk, d10);

  Serial.println("--------------------------------------------------------------------");
  Serial.printf("  SELF-TEST SUMMARY: %d PASS, %d FAIL\n", passCnt, failCnt);
  Serial.println("====================================================================\n");
  Serial.printf("{\"type\":\"SELFTEST_DONE\",\"pass\":%d,\"fail\":%d}\n", passCnt, failCnt);

  isMeasurementBusy = false;
  updateLedStatusFromState();
  renderScreens();
}

// ======================= U11 D-PAD, A/B, KNOBS & SW_STBY =====================
static McpDebouncedBtn btnUp    = {true, true, 0, 0, false};
static McpDebouncedBtn btnDown  = {true, true, 0, 0, false};
static McpDebouncedBtn btnLeft  = {true, true, 0, 0, false};
static McpDebouncedBtn btnRight = {true, true, 0, 0, false};
static McpDebouncedBtn btnA     = {true, true, 0, 0, false};
static McpDebouncedBtn btnB     = {true, true, 0, 0, false};
static McpDebouncedBtn btnEncB  = {true, true, 0, 0, false};
static McpDebouncedBtn btnEncC  = {true, true, 0, 0, false};

static uint8_t prevEncBState = 0x03;
static uint8_t prevEncCState = 0x03;
static int8_t  encBAccum     = 0;
static int8_t  encCAccum     = 0;
static bool    lastHwStbyLow = false;

static const int8_t ENC_GRAY_LUT[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

static void onEncoderBStep(int dir) {
  if (inStandby) return;
  int next = constrain((int)bRangeIdx + dir, 0, 5);
  if (next != (int)bRangeIdx) {
    bRangeIdx = (uint8_t)next;
    if (consoleMode == MODE_AUTO_TESTER || consoleMode == MODE_CURVE_TRACER) {
      renderTFT1();
    }
  }
}

static void onEncoderCStep(int dir) {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    int next = ((int)dmm.subMode + dir + (int)DMM_SUBMODE_COUNT) % (int)DMM_SUBMODE_COUNT;
    dmmSelectSubMode(static_cast<DmmSubMode>(next));
  } else {
    int next = constrain((int)vceRangeIdx + dir, 0, 3);
    if (next != (int)vceRangeIdx) {
      vceRangeIdx = (uint8_t)next;
      if (consoleMode == MODE_AUTO_TESTER || consoleMode == MODE_CURVE_TRACER) {
        renderTFT1();
      }
    }
  }
}

static void stepCurveCursorPt(int dir) {
  if (!lastSweep.valid || lastSweep.points == 0) return;
  if (cursorCurve < 0 || cursorCurve >= lastSweep.curves) {
    cursorCurve = 0; cursorPt = 0;
    renderTFT1();
    return;
  }
  int c = cursorCurve;
  int p = cursorPt + dir;
  if (p >= lastSweep.curvePtCount[c]) {
    c = (c + 1) % lastSweep.curves;
    p = 0;
  } else if (p < 0) {
    c = (c - 1 + lastSweep.curves) % lastSweep.curves;
    p = max(0, lastSweep.curvePtCount[c] - 1);
  }
  cursorCurve = (int8_t)c;
  cursorPt    = (int8_t)p;
  renderTFT1();
}

static void stepCurveCursorLine(int dir) {
  if (!lastSweep.valid || lastSweep.curves == 0) return;
  if (cursorCurve < 0) {
    cursorCurve = 0; cursorPt = 0;
  } else {
    cursorCurve = (int8_t)((cursorCurve + dir + lastSweep.curves) % lastSweep.curves);
    if (cursorPt >= lastSweep.curvePtCount[cursorCurve]) {
      cursorPt = max(0, lastSweep.curvePtCount[cursorCurve] - 1);
    }
  }
  renderTFT1();
}

static void onBtnAShort() {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    dmm.hold = !dmm.hold;
    beepTone(1800, 25);
    renderScreens();
    return;
  }
  if (drift.active) { stopThermalDrift(); return; }
  if (consoleMode == MODE_THERMAL_DRIFT) {
    startThermalDrift();
  } else if (consoleMode == MODE_CURVE_TRACER) {
    runPulsedGeSweep(true);
  } else {
    runFullAutoDetect();
    if (!abortRequested && lastScan.kind == DUT_BJT && lastScan.bjt.valid) {
      runPulsedGeSweep(false);
    }
  }
}

static void onBtnALong() {
  if (inStandby) return;
  if (drift.active) stopThermalDrift();
  stopBuzzerAll();
  runOpenSocketCal();
}

static void onBtnBShort() {
  stopBuzzerAll();
  allSafeHiZ();
  if (inStandby) return;
  if (isMeasurementBusy) { abortRequested = true; return; }
  if (drift.active) { stopThermalDrift(); return; }
  consoleMode = static_cast<ConsoleMode>((static_cast<uint8_t>(consoleMode) + 1) % MODE_COUNT);
  if (consoleMode == MODE_MULTIMETER) dmmSelectSubMode(dmm.subMode);
  else                                renderScreens();
}

static void onBtnBLong() {
  stopBuzzerAll();
  allSafeHiZ();
  abortRequested = true;
  if (drift.active) stopThermalDrift();
  if (inStandby) return;
  consoleMode = MODE_AUTO_TESTER;
  beepTone(1200, 35);
  renderScreens();
}

static void onDpadUp() {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    if (dmm.relActive) { dmm.relActive = false; dmm.relZero = 0.0f; }
    else if (dmm.valid && !dmm.openLoop) { dmm.relZero = dmm.primaryVal; dmm.relActive = true; }
    beepTone(1600, 25);
    renderScreens();
  } else if (lastSweep.valid && lastSweep.points > 0) {
    stepCurveCursorLine(1);
  } else {
    onEncoderBStep(1);
  }
}

static void onDpadDown() {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    dmmResetStats();
    beepTone(1200, 25);
    renderScreens();
  } else if (lastSweep.valid && lastSweep.points > 0) {
    stepCurveCursorLine(-1);
  } else {
    onEncoderBStep(-1);
  }
}

static void onDpadLeft() {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    onEncoderCStep(-1);
  } else if (lastSweep.valid && lastSweep.points > 0) {
    stepCurveCursorPt(-1);
  } else {
    onEncoderCStep(-1);
  }
}

static void onDpadRight() {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    onEncoderCStep(1);
  } else if (lastSweep.valid && lastSweep.points > 0) {
    stepCurveCursorPt(1);
  } else {
    onEncoderCStep(1);
  }
}

static void onEncBSwPress() {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    if (dmm.relActive) { dmm.relActive = false; dmm.relZero = 0.0f; }
    else if (dmm.valid && !dmm.openLoop) { dmm.relZero = dmm.primaryVal; dmm.relActive = true; }
    beepTone(1600, 25);
    renderScreens();
  } else {
    runPulsedGeSweep(lastScan.kind != DUT_BJT || !lastScan.bjt.valid);
  }
}

static void onEncCSwPress() {
  if (inStandby) return;
  if (consoleMode == MODE_MULTIMETER) {
    dmmResetStats();
    beepTone(1200, 25);
    renderScreens();
  } else {
    stepCurveCursorPt(1);
  }
}

static void updateDebouncedMcpBtn(McpDebouncedBtn &b, bool rawHigh, bool allowLong,
                                  void (*onShort)(), void (*onLong)()) {
  uint32_t now = millis();
  if (rawHigh != b.lastRaw) {
    b.lastEdgeMs = now;
    b.lastRaw = rawHigh;
  }
  if ((uint32_t)(now - b.lastEdgeMs) >= 30) {
    if (rawHigh != b.stable) {
      b.stable = rawHigh;
      if (!b.stable) {
        b.pressedMs = now;
        b.longFired = false;
      } else {
        if (!b.longFired && onShort != nullptr) onShort();
      }
    } else if (!b.stable && allowLong && !b.longFired &&
               (uint32_t)(now - b.pressedMs) >= 1200) {
      b.longFired = true;
      if (onLong != nullptr) onLong();
    }
  }
}

void pollMcpInputs() {
  if (!u11OK) return;
  static uint32_t lastPollMs = 0;
  uint32_t now = millis();
  if (!mcpIntFlag && digitalRead(PIN_MCP_INT) == HIGH && (uint32_t)(now - lastPollMs) < 15) {
    return;
  }
  mcpIntFlag = false;
  lastPollMs = now;

  uint8_t gpioA = 0xFF, gpioB = 0xFF;
  if (!mcpReadPorts(I2C_ADDR_U11, gpioA, gpioB)) return;

  // Hardware SW_STBY (U11 GPA6, LOW = Standby)
  bool hwStbyLow = ((gpioA >> U11_GPA_SW_STBY) & 0x01) == 0;
  if (hwStbyLow != lastHwStbyLow) {
    lastHwStbyLow = hwStbyLow;
    softwareStandby = false;
    setStandbyMode(hwStbyLow);
  }

  // Dual Quadrature Encoders on U11 Port B (ENC_B = GPB0/1, ENC_C = GPB3/4)
  uint8_t curB = ((gpioB >> U11_GPB_ENC_B_A) & 0x01) |
                 (((gpioB >> U11_GPB_ENC_B_B) & 0x01) << 1);
  if (curB != prevEncBState) {
    encBAccum += ENC_GRAY_LUT[(prevEncBState << 2) | curB];
    prevEncBState = curB;
    if (encBAccum >= 4 || (curB == 0x03 && encBAccum >= 2)) {
      encBAccum = 0;
      if (!isMeasurementBusy) onEncoderBStep(1);
    } else if (encBAccum <= -4 || (curB == 0x03 && encBAccum <= -2)) {
      encBAccum = 0;
      if (!isMeasurementBusy) onEncoderBStep(-1);
    }
  }

  uint8_t curC = ((gpioB >> U11_GPB_ENC_C_A) & 0x01) |
                 (((gpioB >> U11_GPB_ENC_C_B) & 0x01) << 1);
  if (curC != prevEncCState) {
    encCAccum += ENC_GRAY_LUT[(prevEncCState << 2) | curC];
    prevEncCState = curC;
    if (encCAccum >= 4 || (curC == 0x03 && encCAccum >= 2)) {
      encCAccum = 0;
      if (!isMeasurementBusy) onEncoderCStep(1);
    } else if (encCAccum <= -4 || (curC == 0x03 && encCAccum <= -2)) {
      encCAccum = 0;
      if (!isMeasurementBusy) onEncoderCStep(-1);
    }
  }

  bool upRaw    = ((gpioA >> U11_GPA_BTN_UP)    & 0x01) != 0;
  bool downRaw  = ((gpioA >> U11_GPA_BTN_DOWN)  & 0x01) != 0;
  bool leftRaw  = ((gpioA >> U11_GPA_BTN_LEFT)  & 0x01) != 0;
  bool rightRaw = ((gpioA >> U11_GPA_BTN_RIGHT) & 0x01) != 0;
  bool aRaw     = ((gpioA >> U11_GPA_BTN_A)     & 0x01) != 0;
  bool bRaw     = ((gpioA >> U11_GPA_BTN_B)     & 0x01) != 0;
  bool swBRaw   = ((gpioB >> U11_GPB_ENC_B_SW)  & 0x01) != 0;
  bool swCRaw   = ((gpioB >> U11_GPB_ENC_C_SW)  & 0x01) != 0;

  updateDebouncedMcpBtn(btnB, bRaw, true, onBtnBShort, onBtnBLong);

  if (!isMeasurementBusy) {
    updateDebouncedMcpBtn(btnA,     aRaw,     true,  onBtnAShort,   onBtnALong);
    updateDebouncedMcpBtn(btnUp,    upRaw,    false, onDpadUp,      nullptr);
    updateDebouncedMcpBtn(btnDown,  downRaw,  false, onDpadDown,    nullptr);
    updateDebouncedMcpBtn(btnLeft,  leftRaw,  false, onDpadLeft,    nullptr);
    updateDebouncedMcpBtn(btnRight, rightRaw, false, onDpadRight,   nullptr);
    updateDebouncedMcpBtn(btnEncB,  swBRaw,   false, onEncBSwPress, nullptr);
    updateDebouncedMcpBtn(btnEncC,  swCRaw,   false, onEncCSwPress, nullptr);
  }
}

// ======================= SERIAL CLI + VERBOSE DEBUG ==========================
static char     serialLineBuf[160];
static size_t   serialLineLen    = 0;
static uint32_t serialLastRxMs   = 0;
static uint32_t lastVerboseDumpMs = 0;

static void dumpVerboseLiveState() {
  uint8_t u11A = 0, u11B = 0, u12A = 0, u12B = 0;
  if (u11OK) mcpReadPorts(I2C_ADDR_U11, u11A, u11B);
  if (u12OK) mcpReadPorts(I2C_ADDR_U12, u12A, u12B);
  float v[4] = {0, 0, 0, 0};
  if (adsOK && analogRailOn) readAllNodes(v, 1);
  Serial.printf(
      "[DBG][LIVE] SR=0x%04X DAC=[%d,%d] U11=(%02X,%02X) U12=(%02X,%02X) "
      "V=[%.3f,%.3f,%.3f,A3=%.3f] STBY=%d\n",
      (unsigned)currentMatrixWord, lastDacCode[0], lastDacCode[1],
      (unsigned)u11A, (unsigned)u11B, (unsigned)u12A, (unsigned)u12B,
      v[0], v[1], v[2], v[3], inStandby ? 1 : 0);
}

static void executeSerialCommand(const char *cmd) {
  if (cmd[0] == '\0') return;

  if (strcmp(cmd, "verbose") == 0 || strcmp(cmd, "V") == 0) {
    verboseDebug = !verboseDebug;
    Serial.printf("{\"type\":\"VERBOSE\",\"enabled\":%s}\n", verboseDebug ? "true" : "false");
    if (verboseDebug) dumpVerboseLiveState();
    return;
  }
  if (strcmp(cmd, "selftest") == 0 || strcmp(cmd, "T") == 0) {
    runHardwareSelfTest();
    return;
  }
  if (strcmp(cmd, "modem") == 0 || strcmp(cmd, "m") == 0 || strcmp(cmd, "M") == 0) {
    playModemHandshakeDemo();
    return;
  }
  if (strcmp(cmd, "stby") == 0 || strcmp(cmd, "Z") == 0) {
    softwareStandby = !inStandby;
    setStandbyMode(softwareStandby);
    return;
  }
  if (strncmp(cmd, "wifi ", 5) == 0) {
    char ssid[33] = {0}, pass[65] = {0}, host[64] = {0};
    unsigned port = 8080;
    int n = sscanf(cmd + 5, "%32s %64s %63s %u", ssid, pass, host, &port);
    if (n >= 2) {
      snprintf(wifiCfg.ssid, sizeof(wifiCfg.ssid), "%s", ssid);
      snprintf(wifiCfg.pass, sizeof(wifiCfg.pass), "%s", (strcmp(pass, "-") == 0) ? "" : pass);
      if (n >= 3) snprintf(wifiCfg.serverHost, sizeof(wifiCfg.serverHost), "%s", host);
      if (n >= 4 && port > 0 && port < 65535) wifiCfg.serverPort = (uint16_t)port;
      wifiCfg.autoPost = (strlen(wifiCfg.serverHost) > 0) ? 1 : 0;
      saveWifiConfigNVS();
      Serial.printf("{\"type\":\"WIFI_CFG\",\"ssid\":\"%s\",\"host\":\"%s\",\"port\":%u,\"auto_post\":%s}\n",
                    wifiCfg.ssid, wifiCfg.serverHost, (unsigned)wifiCfg.serverPort,
                    wifiCfg.autoPost ? "true" : "false");
      if (inStandby) {
        startStandbyRadioServices();
        renderScreens();
      }
    } else {
      emitStatusJson("ERROR", "Usage: wifi <ssid> <pass|-> [server_ip] [port]");
    }
    return;
  }
  if (strcmp(cmd, "t") == 0 || strcmp(cmd, "scan") == 0) {
    consoleMode = MODE_AUTO_TESTER;
    runFullAutoDetect();
    return;
  }
  if (strcmp(cmd, "s") == 0 || strcmp(cmd, "S") == 0 || strcmp(cmd, "sweep") == 0) {
    consoleMode = MODE_CURVE_TRACER;
    runPulsedGeSweep(true);
    return;
  }
  if (strcmp(cmd, "d") == 0 || strcmp(cmd, "D") == 0 || strcmp(cmd, "drift") == 0) {
    startThermalDrift();
    return;
  }
  if (strcmp(cmd, "dmm") == 0 || strcmp(cmd, "u") == 0 || strcmp(cmd, "U") == 0) {
    if (drift.active) stopThermalDrift();
    consoleMode = MODE_MULTIMETER;
    dmmSelectSubMode(static_cast<DmmSubMode>((static_cast<uint8_t>(dmm.subMode) + 1) % DMM_SUBMODE_COUNT));
    Serial.printf("{\"type\":\"DMM_MODE\",\"mode\":\"%s\"}\n", DMM_MODE_LABELS[dmm.subMode]);
    return;
  }
  if (strcmp(cmd, "b") == 0 || strcmp(cmd, "B") == 0) { onEncoderBStep(1); return; }
  if (strcmp(cmd, "v") == 0)                           { onEncoderCStep(1); return; }
  if (strcmp(cmd, "c") == 0 || strcmp(cmd, "C") == 0 || strcmp(cmd, "cal") == 0) {
    if (drift.active) stopThermalDrift();
    runOpenSocketCal();
    return;
  }
  if (strcmp(cmd, "p") == 0 || strcmp(cmd, "P") == 0 || strcmp(cmd, "calinfo") == 0) {
    emitCalInfoJson();
    return;
  }
  if (strcmp(cmd, "x") == 0 || strcmp(cmd, "X") == 0 || strcmp(cmd, "q") == 0 || strcmp(cmd, "Q") == 0) {
    stopBuzzerAll();
    if (drift.active) stopThermalDrift();
    allSafeHiZ();
    emitStatusJson("INFO", "Idle Hi-Z");
    return;
  }
  if (strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0) {
    Serial.println(
        "{\"type\":\"HELP\",\"cmds\":{\"t\":\"auto_scan\",\"s\":\"pulsed_ge_sweep\","
        "\"d\":\"thermal_drift\",\"u/dmm\":\"cycle_dmm\",\"m/modem\":\"56k_modem_demo\","
        "\"T/selftest\":\"10step_hw_selftest\",\"V/verbose\":\"toggle_verbose_debug\","
        "\"Z/stby\":\"toggle_standby\",\"c\":\"open_socket_cal\",\"p\":\"cal_info\","
        "\"b\":\"step_b_range\",\"v\":\"step_vce_range\",\"x\":\"stop_hiz\","
        "\"wifi <ssid> <pass> [host] [port]\":\"configure_wifi_sync\"}}");
  }
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    serialLastRxMs = millis();
    if (c == '\r' || c == '\n') {
      serialLineBuf[serialLineLen] = '\0';
      if (serialLineLen > 0) {
        executeSerialCommand(serialLineBuf);
        serialLineLen = 0;
      }
    } else if (serialLineLen < sizeof(serialLineBuf) - 1) {
      serialLineBuf[serialLineLen++] = c;
    }
  }
  // Single-keypress terminal support without requiring Enter
  if (serialLineLen == 1 && (uint32_t)(millis() - serialLastRxMs) >= 130) {
    serialLineBuf[1] = '\0';
    serialLineLen = 0;
    executeSerialCommand(serialLineBuf);
  }
}

// ======================= SETUP & MAIN LOOP ===================================
void setup() {
  // 1. CRITICAL BOOT ORDER: Keep 74HC595 OE_N HIGH (Hi-Z) before anything else!
  digitalWrite(PIN_SR_OE_N, HIGH);
  pinMode(PIN_SR_OE_N, OUTPUT);
  digitalWrite(PIN_SR_OE_N, HIGH);

  digitalWrite(PIN_SR_LATCH, LOW);
  pinMode(PIN_SR_LATCH, OUTPUT);
  digitalWrite(PIN_DAC_CS, HIGH);
  pinMode(PIN_DAC_CS, OUTPUT);
  digitalWrite(PIN_TFT1_CS, HIGH);
  pinMode(PIN_TFT1_CS, OUTPUT);
  digitalWrite(PIN_TFT2_CS, HIGH);
  pinMode(PIN_TFT2_CS, OUTPUT);
  digitalWrite(PIN_TFT_DC, HIGH);
  pinMode(PIN_TFT_DC, OUTPUT);

  SPI.begin(PIN_SPI_SCK, -1, PIN_SPI_MOSI, -1);

  delay(5);
  shiftOut16Raw(0x0000);
  shiftOut16Raw(0x0000);
  currentMatrixWord = 0x0000;

  writeMcp4822Raw(0, 0);
  writeMcp4822Raw(1, 0);
  lastDacCode[0] = 0;
  lastDacCode[1] = 0;

  digitalWrite(PIN_SR_OE_N, LOW);

  pinMode(PIN_ADS_ALRT, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ADS_ALRT), onAdsAlertISR, FALLING);

  analogReadResolution(12);
  for (int i = 0; i < 3; i++) {
    pinMode(FAST_TP_PINS[i], INPUT);
    analogSetPinAttenuation(FAST_TP_PINS[i], ADC_11db);
  }

  initBuzzerPwm();

  Serial.begin(115200);
  initCalDefaults(cal);
  bool calLoaded = loadCalNVS(cal);
  loadHistoryNVS();
  loadWifiConfigNVS();

  // 2. I2C 9-clock recovery + 400 kHz + 50 ms timeout
  recoverI2CBus();

  // 3. Init U11 (0x20) first so we can read SW_STBY before powering analog rails on U12 (0x21)
  bool bootStandby = false;
  if (initU11Controls()) {
    pinMode(PIN_MCP_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_MCP_INT), onMcpIntISR, FALLING);

    uint8_t a = 0xFF, b = 0xFF;
    mcpReadPorts(I2C_ADDR_U11, a, b);
    bootStandby   = ((a >> U11_GPA_SW_STBY) & 0x01) == 0;
    lastHwStbyLow = bootStandby;
    prevEncBState = ((b >> U11_GPB_ENC_B_A) & 0x01) | (((b >> U11_GPB_ENC_B_B) & 0x01) << 1);
    prevEncCState = ((b >> U11_GPB_ENC_C_A) & 0x01) | (((b >> U11_GPB_ENC_C_B) & 0x01) << 1);

    currentCalMux = CAL_DMM30V;
    selectCalMux(CAL_SRCA);
  }

  // 4. Init U12 (0x21) status LEDs + TFT_RES + ANA_EN_N
  initU12StatusAndPower(bootStandby);
  if (!bootStandby) {
    delay(65); // Allow MT3608 + L7812 + L78L09 to settle
  }

  // 5. Reset & init dual 1.9" ST7789 IPS screens (rotation=1 -> 320x170)
  pulseTftReset();
  tft1.begin(1);
  tft2.begin(1);
  tftsOK = true;

  // 6. Init ADS1115 & DS18B20
  if (!initADS1115()) {
    emitStatusJson("ERROR", "ADS1115 not found at 0x48");
  } else {
    emitStatusJson("READY", calLoaded ? "Rev 4.0 Ge-LAB PRO ready (NVS cal loaded)"
                                      : "Rev 4.0 Ge-LAB PRO ready (uncalibrated defaults)");
  }
  initTempSensor();

  // 7. Apply boot Standby or Active RF-quiet state & render screens
  inStandby = bootStandby;
  if (inStandby) {
    startStandbyRadioServices();
  } else {
    ensureRadioOffForAnalog();
    playBootChime();
  }

  updateLedStatusFromState();
  renderScreens();
  setBacklight(true);
}

void loop() {
  updateTempAsync();
  updateToneSequencer();
  pollMcpInputs();
  handleSerial();

#if ENABLE_WIFI_SYNC
  if (inStandby && httpServerStarted) {
    httpServer.handleClient();
  }
#endif

  if (verboseDebug && (uint32_t)(millis() - lastVerboseDumpMs) >= 1000) {
    lastVerboseDumpMs = millis();
    dumpVerboseLiveState();
  }

  if (!inStandby) {
    if (drift.active) {
      uint32_t now = millis();
      if (drift.lastSampleMs == 0 || (uint32_t)(now - drift.lastSampleMs) >= 1000) {
        sampleThermalDriftOnce();
      }
    } else if (consoleMode == MODE_MULTIMETER) {
      dmmSampleOnce();
    }
  }

  delay(4);
}
