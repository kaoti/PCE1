#!/usr/bin/env python3
"""
Single-Source-of-Truth Schematic + Routed 2-Layer PCB Generator for:
  KAOTI AUDIO — Rev 5.0 Dual-MCU Germanium & Universal Parametric Characterization Engine
  100% Through-Hole · All-Socketed · Zero Moving Parts · Symmetrical Dual 1.9" ST7789 IPS Console.

Rev 5.0 Architecture Highlights:
  - Dual-MCU Optical Split:
      * U_MCU1 (WEMOS LOLIN32 Lite V1.0.0, Wi-Fi/BT OFF): Quiet Analog Core on +3V3_ANA / AGND
      * U_MCU2 (ESP32-C3 SuperMini): UI / Wi-Fi / BLE / Audio Core on +3V3_DIG / DGND
      * 2x H11L1M (U_ISO1, U_ISO2) 1MHz Schmitt-Trigger Optocouplers (0.000 uA Standby back-powering)
      * J_CYD_UART 4-Pin Expansion Tap with 2x 470R series contention guards
  - TI BQ25895 1.5MHz Synchronous Buck NVDC Fast Charger (MOD_CHG @ 0x6A, NO TP4056!):
      * USB-C Input (J_USB_C + 2x 5.1k CC1/CC2) -> F_USB (RXEF175) -> D_TVS_USB (P6KE15A)
      * Strict 3.7V Non-HV LiPo (4.192V VREG <= 4.20V max), SW_PWR DPDT 3A (0.000 uA OFF drain)
      * Q_REV_BAT (NDP6020P ideal diode: 1=G, 2=D, 3=S) & Q_ANA_SW (NDP6020P high-side analog cutoff)
      * 2x MCP1826S-3302E/AB 1.0A Ultra-LDOs (U_LDO_DIG, U_LDO_ANA: 1=VIN, 2=GND, 3=VOUT)
  - 2x Critically Damped LC Pi-Filters + Hardened OVP + 15.12 mA Active Limiter + Star Ground:
      * Pi-Filter #1: MOD_BOOST (MT3608 14.50V) -> L1 (100uH) + R_DAMP1 (2.2R 0.5W) -> U_REG12 (L7812CV)
        -> F1 (RXEF017) + D_OVP (1N4743A 13V) -> U_REG9 (L78L09ACZ) -> 2x 2N3906 (43R) + Q_OVC -> +9V_LIM
      * Pi-Filter #2: L2 (47uH) + R_DAMP2 (1.5R) -> U_LDO_ANA -> +3V3_ANA -> 10R/10.1uF -> +3V3_ADC
      * Partitioned AGND / DGND B.Cu planes (>=2.0mm moat) joined ONLY at R_STAR (0R_STAR_BRIDGE)
  - Symmetrical Rounded-Corner Handheld/Benchtop Multimeter PCB (260.0 x 155.0 mm, R = 10.0 mm corners):
      * Top 6-LED Status Bezel Bar, Symmetrical Dual 1.9" ST7789 IPS Screens, D-Pad Left / EC11 Center / A-B Right,
        Bottom Centered DUT Test Bay (J_ZIF, J_DUT3, J_KELVIN, J_TERM5, J_TEMP).
"""
import os
import re
import math
import uuid
import heapq
from collections import defaultdict

PROJ_DIR = "/Users/kaoti/Desktop/PCE1"
FP_DIR = os.path.join(PROJ_DIR, "Ge_Engine.pretty")
SYM_PATH = os.path.join(PROJ_DIR, "Ge_Engine.kicad_sym")

BOARD_W = 260.0
BOARD_H = 155.0
CORNER_R = 10.0
CENTER_X = 78.74  # Vertical symmetry axis of the Front-Panel Handheld/Console UI


def uid():
    return str(uuid.uuid4())


# ==============================================================================
# 1. COMPLETE REV 5.0 8-BLOCK COMPONENT & NETLIST DEFINITION (242 COMPONENTS)
# ==============================================================================
def build_netlist():
    comps = []

    def add(ref, sym, val, fp, pins, block=1):
        comps.append({
            "ref": ref,
            "sym": f"Ge_Engine:{sym}",
            "val": val,
            "fp": f"Ge_Engine:{fp}",
            "pins": {str(k): v for k, v in pins.items()},
            "block": block,
        })

    # --- BLOCK 1: POWER, 3.7V NON-HV LIPO PROTECTION & TI BQ25895 NVDC FAST-CHARGER ---
    add("J_USB_C", "USBC_Breakout_6P", "USBC_5V_3A_HVDCP", "Socket_USBC_Breakout", {
        1: "V_USB_RAW", 2: "DGND", 3: "USB_CC1", 4: "USB_CC2", 5: None, 6: None
    }, block=1)
    add("R_CC1", "R_THT", "5.1k_1%", "R_Axial_P7.62mm", {1: "USB_CC1", 2: "DGND"}, block=1)
    add("R_CC2", "R_THT", "5.1k_1%", "R_Axial_P7.62mm", {1: "USB_CC2", 2: "DGND"}, block=1)
    add("F_USB", "F_PTC_THT", "RXEF175_1.75A", "Fuse_PTC_P5.00mm", {1: "V_USB_RAW", 2: "V_USB_PROT"}, block=1)
    add("D_TVS_USB", "DZ_THT", "P6KE15A", "D_DO-15_P10.16mm", {1: "V_USB_PROT", 2: "DGND"}, block=1)
    add("C_USB_IN", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "V_USB_PROT", 2: "DGND"}, block=1)

    add("J_BAT", "Conn_1x02", "LIPO_1S_3V7_NON_HV", "Header_1x02_P2.54mm", {1: "J_BAT+", 2: "DGND"}, block=1)
    add("J_NTC", "Conn_1x02", "BAT_NTC_10K", "Header_1x02_P2.54mm", {1: "BAT_NTC", 2: "DGND"}, block=1)
    add("SW_PWR", "SW_DPDT_THT", "MASTER_PWR_DPDT_3A", "SW_Slide_DPDT_THT", {
        1: None, 2: "J_BAT+", 3: "V_BAT_SW",
        4: "V_SYS_BLEED", 5: "V_SYS_RAW", 6: "V_BUS_SYS"
    }, block=1)
    add("R_BLEED_SYS", "R_THT", "10k", "R_Axial_P7.62mm", {1: "V_SYS_BLEED", 2: "DGND"}, block=1)
    add("Q_REV_BAT", "PMOS_NDP6020P", "NDP6020P", "TO-220-3_Vertical", {
        1: "Q_REV_G", 2: "V_BAT_SW", 3: "V_BAT_PROT"
    }, block=1)
    add("R_G_REV", "R_THT", "1.0k", "R_Axial_P7.62mm", {1: "Q_REV_G", 2: "DGND"}, block=1)

    add("MOD_CHG", "BQ25895_Module", "BQ25895_NVDC_3A", "Socket_BQ25895_Charger", {
        1: "V_USB_PROT", 2: "PMID_BYP", 3: "DGND", 4: "V_SYS_RAW", 5: "V_BAT_PROT", 6: "BAT_NTC",
        7: "UI_I2C_SCL", 8: "UI_I2C_SDA", 9: None, 10: "CHG_STAT_N", 11: "DGND", 12: "DGND"
    }, block=1)
    add("C_PMID", "CP_THT", "10uF_25V", "CP_Radial_D8_P3.5mm", {1: "PMID_BYP", 2: "DGND"}, block=1)
    add("C_BAT_BYP", "CP_THT", "100uF_16V", "CP_Radial_D8_P3.5mm", {1: "V_BAT_PROT", 2: "DGND"}, block=1)
    add("C_SYS1", "CP_THT", "100uF_16V", "CP_Radial_D8_P3.5mm", {1: "V_BUS_SYS", 2: "DGND"}, block=1)
    add("C_SYS2", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "V_BUS_SYS", 2: "DGND"}, block=1)

    add("U_LDO_DIG", "MCP1826S_TO220", "MCP1826S-3302E/AB", "TO-220-3_Vertical", {
        1: "V_BUS_SYS", 2: "DGND", 3: "+3V3_DIG"
    }, block=1)
    add("C_DIG1", "CP_THT", "100uF_16V", "CP_Radial_D8_P3.5mm", {1: "+3V3_DIG", 2: "DGND"}, block=1)
    add("C_DIG2", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_DIG", 2: "DGND"}, block=1)

    # --- BLOCK 2: SWITCHABLE ANALOG POWER, 2x CRITICALLY DAMPED LC PI-FILTERS, LDOs, 15mA LIMITER & STAR GROUND ---
    add("R_PD_ANA", "R_THT", "100k", "R_Axial_P7.62mm", {1: "ANA_PWR_EN", 2: "DGND"}, block=2)
    add("R_G_ANA", "R_THT", "100R", "R_Axial_P7.62mm", {1: "ANA_PWR_EN", 2: "Q_ANA_NG"}, block=2)
    add("Q_ANA_N", "NMOS_2N7000", "2N7000", "Socket_TO92_3Pin", {
        1: "DGND", 2: "Q_ANA_NG", 3: "Q_ANA_SW_G"
    }, block=2)
    add("R_PU_SW", "R_THT", "47k", "R_Axial_P7.62mm", {1: "V_BUS_SYS", 2: "Q_ANA_SW_G"}, block=2)
    add("Q_ANA_SW", "PMOS_NDP6020P", "NDP6020P", "TO-220-3_Vertical", {
        1: "Q_ANA_SW_G", 2: "V_ANA_IN", 3: "V_BUS_SYS"
    }, block=2)

    add("MOD_BOOST", "MT3608_Boost", "MT3608_14.5V", "Socket_MT3608_Boost", {
        1: "V_ANA_IN", 2: "AGND", 3: "V_BOOST_RAW", 4: "AGND"
    }, block=2)
    add("C_PI1A", "CP_THT", "100uF_35V", "CP_Radial_D8_P3.5mm", {1: "V_BOOST_RAW", 2: "AGND"}, block=2)
    add("C_PI1B", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "V_BOOST_RAW", 2: "AGND"}, block=2)
    add("L1", "L_RAD_THT", "100uH_1A", "L_Radial_P5.00mm", {1: "V_BOOST_RAW", 2: "V_LC1_MID"}, block=2)
    add("R_DAMP1", "R_THT", "2.2R_0.5W", "R_Axial_P7.62mm", {1: "V_LC1_MID", 2: "V_14V_FILT"}, block=2)
    add("C_PI2A", "CP_THT", "100uF_35V", "CP_Radial_D8_P3.5mm", {1: "V_14V_FILT", 2: "AGND"}, block=2)
    add("C_PI2B", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "V_14V_FILT", 2: "AGND"}, block=2)

    add("U_REG12", "L7812_TO220", "L7812CV", "TO-220-3_Vertical", {
        1: "V_14V_FILT", 2: "AGND", 3: "V_12V_PRE"
    }, block=2)
    add("F1", "F_PTC_THT", "RXEF017_170mA", "Fuse_PTC_P5.00mm", {1: "V_12V_PRE", 2: "+12V_A"}, block=2)
    add("D_OVP", "DZ_THT", "1N4743A_13V", "D_DO-41_P10.16mm", {1: "+12V_A", 2: "AGND"}, block=2)
    add("C17", "CP_THT", "10uF_25V", "CP_Radial_D8_P3.5mm", {1: "+12V_A", 2: "AGND"}, block=2)
    add("C18", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+12V_A", 2: "AGND"}, block=2)

    add("U_REG9", "L78L09_TO92", "L78L09ACZ", "Socket_TO92_3Pin", {
        1: "+9V_REF", 2: "AGND", 3: "+12V_A"
    }, block=2)
    add("C7", "CP_THT", "10uF_25V", "CP_Radial_D8_P3.5mm", {1: "+9V_REF", 2: "AGND"}, block=2)
    add("C8", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+9V_REF", 2: "AGND"}, block=2)
    add("R_ILIM", "R_THT", "43R_1%", "R_Axial_P7.62mm", {1: "+9V_REF", 2: "LIM_SENSE"}, block=2)
    add("Q_LIM1", "PNP_2N3906", "2N3906", "Socket_TO92_3Pin", {
        1: "LIM_SENSE", 2: "LIM_BASE", 3: "+9V_LIM"
    }, block=2)
    add("Q_LIM2", "PNP_2N3906", "2N3906", "Socket_TO92_3Pin", {
        1: "+9V_REF", 2: "LIM_SENSE", 3: "LIM_BASE"
    }, block=2)
    add("R_IBIAS", "R_THT", "10k", "R_Axial_P7.62mm", {1: "LIM_BASE", 2: "AGND"}, block=2)
    add("R_OVC_DIV1", "R_THT", "100k", "R_Axial_P7.62mm", {1: "+9V_REF", 2: "Q_OVC_B"}, block=2)
    add("R_OVC_DIV2", "R_THT", "22k", "R_Axial_P7.62mm", {1: "+9V_LIM", 2: "Q_OVC_B"}, block=2)
    add("Q_OVC", "NPN_2N3904", "2N3904", "Socket_TO92_3Pin", {
        1: "AGND", 2: "Q_OVC_B", 3: "FAULT_OVC_N"
    }, block=2)
    add("R_PU_OVC", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "FAULT_OVC_N"}, block=2)

    add("C_A1A", "CP_THT", "100uF_16V", "CP_Radial_D8_P3.5mm", {1: "V_ANA_IN", 2: "AGND"}, block=2)
    add("C_A1B", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "V_ANA_IN", 2: "AGND"}, block=2)
    add("L2", "L_RAD_THT", "47uH_1A", "L_Radial_P5.00mm", {1: "V_ANA_IN", 2: "V_LC2_MID"}, block=2)
    add("R_DAMP2", "R_THT", "1.5R_0.25W", "R_Axial_P7.62mm", {1: "V_LC2_MID", 2: "V_3V3_FILT"}, block=2)
    add("C_A2A", "CP_THT", "100uF_16V", "CP_Radial_D8_P3.5mm", {1: "V_3V3_FILT", 2: "AGND"}, block=2)
    add("C_A2B", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "V_3V3_FILT", 2: "AGND"}, block=2)
    add("U_LDO_ANA", "MCP1826S_TO220", "MCP1826S-3302E/AB", "TO-220-3_Vertical", {
        1: "V_3V3_FILT", 2: "AGND", 3: "+3V3_ANA"
    }, block=2)
    add("C1", "CP_THT", "100uF_16V", "CP_Radial_D8_P3.5mm", {1: "+3V3_ANA", 2: "AGND"}, block=2)
    add("C2", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_ANA", 2: "AGND"}, block=2)
    add("R_ADC_F", "R_THT", "10R_1%", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "+3V3_ADC"}, block=2)
    add("C_ADC_FA", "CP_THT", "10uF_16V", "CP_Radial_D8_P3.5mm", {1: "+3V3_ADC", 2: "AGND"}, block=2)
    add("R_STAR", "Star_Bridge_0R", "0R_STAR_BRIDGE", "Star_Bridge_0R", {1: "DGND", 2: "AGND"}, block=2)

    # --- BLOCK 3: MCU 1 QUIET ANALOG CORE (WEMOS LOLIN32 LITE, WI-FI/BT OFF) & 2x H11L1M OPTO-ISOLATED UART ---
    add("U_MCU1", "Wemos_Lolin32_Lite", "WEMOS_LOLIN32_Lite", "Socket_Wemos_Lolin32_Lite", {
        1: "FAST_TP1", 2: "FAST_TP2", 3: None, 4: "ADS_ALRT", 5: "FAST_TP3",
        6: "CAL_A0", 7: "CAL_A1", 8: "FAULT_OVC_N", 9: "DAC_CS",
        10: "AFE_SPI_MOSI", 11: "AFE_SPI_SCK", 12: None, 13: "AGND",
        14: "M1_3V3", 15: None, 16: "ANA_I2C_SDA", 17: "ANA_I2C_SCL",
        18: "SR_LATCH", 19: None, 20: "SR_OE_N", 21: "LINK_TX_M1",
        22: "TEMP_DQ", 23: None, 24: None, 25: None, 26: "LINK_RX_M1",
    }, block=3)
    add("JP_M1_3V3", "Conn_1x02", "JP_M1_3V3_ISO", "Header_1x02_P2.54mm", {1: "+3V3_ANA", 2: "M1_3V3"}, block=3)
    add("R_LATCH_PD", "R_THT", "10k", "R_Axial_P7.62mm", {1: "SR_LATCH", 2: "AGND"}, block=3)

    add("U_ISO1", "H11L1M_DIP6", "H11L1M", "Socket_DIP-6_W7.62mm", {
        1: "ISO1_A", 2: "LINK_TX_M1", 3: None, 4: "LINK_RX_M2", 5: "DGND", 6: "+3V3_DIG"
    }, block=3)
    add("R_LED_ISO1", "R_THT", "470R_1%", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "ISO1_A"}, block=3)
    add("R_PU_ISO1", "R_THT", "2.2k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "LINK_RX_M2"}, block=3)
    add("C_BYP_ISO1", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_DIG", 2: "DGND"}, block=3)

    add("U_ISO2", "H11L1M_DIP6", "H11L1M", "Socket_DIP-6_W7.62mm", {
        1: "ISO2_A", 2: "LINK_TX_M2", 3: None, 4: "LINK_RX_M1", 5: "AGND", 6: "+3V3_ANA"
    }, block=3)
    add("R_LED_ISO2", "R_THT", "470R_1%", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "ISO2_A"}, block=3)
    add("R_PU_ISO2", "R_THT", "2.2k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "LINK_RX_M1"}, block=3)
    add("C_BYP_ISO2", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_ANA", 2: "AGND"}, block=3)

    add("J_CYD_UART", "Conn_1x04", "CYD_P1_UART_TAP", "Header_1x04_P2.54mm", {
        1: "V_BUS_SYS", 2: "DGND", 3: "CYD_TX_TAP", 4: "CYD_RX_TAP"
    }, block=3)
    add("R_CYD_TX", "R_THT", "470R", "R_Axial_P7.62mm", {1: "LINK_TX_M2", 2: "CYD_TX_TAP"}, block=3)
    add("R_CYD_RX", "R_THT", "470R", "R_Axial_P7.62mm", {1: "LINK_RX_M2", 2: "CYD_RX_TAP"}, block=3)

    # --- BLOCK 4: MCU 2 COMMS/UI/AUDIO CORE (ESP32-C3 SUPERMINI) & U11 MCP23017-E/SP I2C EXPANDER ---
    add("U_MCU2", "ESP32_C3_SuperMini", "ESP32-C3_SuperMini", "Socket_ESP32_C3_SuperMini", {
        1: "TFT_DC", 2: "UI_SPI_MOSI", 3: "TFT1_CS", 4: None, 5: None, 6: "TFT2_CS",
        7: "LINK_RX_M2", 8: "LINK_TX_M2", 9: None, 10: "DGND", 11: "M2_3V3",
        12: "UI_SPI_SCK", 13: "BZ_PWM", 14: None, 15: "UI_I2C_SCL", 16: "UI_I2C_SDA",
    }, block=4)
    add("JP_M2_3V3", "Conn_1x02", "JP_M2_3V3_ISO", "Header_1x02_P2.54mm", {1: "+3V3_DIG", 2: "M2_3V3"}, block=4)
    add("R_UI_SDA", "R_THT", "4.7k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "UI_I2C_SDA"}, block=4)
    add("R_UI_SCL", "R_THT", "4.7k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "UI_I2C_SCL"}, block=4)

    add("U11", "MCP23017", "MCP23017-E/SP", "Socket_DIP-28_W7.62mm", {
        1: "ENC_A", 2: "ENC_B", 3: "LED_STBY_RF", 4: "LED_SCAN",
        5: "LED_DUT_OK", 6: "LED_FAULT", 7: "TFT_BLK_EN", 8: "ANA_PWR_EN",
        9: "+3V3_DIG", 10: "DGND", 11: None, 12: "UI_I2C_SCL",
        13: "UI_I2C_SDA", 14: None, 15: "DGND", 16: "DGND",
        17: "DGND", 18: "MCP_RST_N", 19: None, 20: None,
        21: "BTN_UP", 22: "BTN_DOWN", 23: "BTN_LEFT", 24: "BTN_RIGHT",
        25: "BTN_A", 26: "BTN_B", 27: "ENC_SW", 28: "SW_STBY",
    }, block=4)
    add("C22", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_DIG", 2: "DGND"}, block=4)
    add("R_RST_M11", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "MCP_RST_N"}, block=4)
    add("C_RST_M11", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "MCP_RST_N", 2: "DGND"}, block=4)

    # --- BLOCK 5: SOLID-STATE 3-TERMINAL AFE MATRIX, SHIFT REGISTERS & FAILSAFES ---
    add("U6", "74HC595N", "74HC595N", "Socket_DIP-16_W7.62mm", {
        1: "U1_A1", 2: "U1_A2", 3: "U1_EN", 4: "U2_A0", 5: "U2_A1", 6: "U2_A2", 7: "U2_EN",
        8: "AGND", 9: "SR_CHAIN", 10: "SR_CLR_N", 11: "AFE_SPI_SCK", 12: "SR_LATCH", 13: "SR_OE_N",
        14: "AFE_SPI_MOSI", 15: "U1_A0", 16: "+3V3_ANA"
    }, block=5)
    add("U7", "74HC595N", "74HC595N", "Socket_DIP-16_W7.62mm", {
        1: "U3_A1", 2: "U3_A2", 3: "U3_EN", 4: "SW_G1", 5: "SW_G2", 6: "SW_G3", 7: "LED2_DRV",
        8: "AGND", 9: None, 10: "SR_CLR_N", 11: "AFE_SPI_SCK", 12: "SR_LATCH", 13: "SR_OE_N",
        14: "SR_CHAIN", 15: "U3_A0", 16: "+3V3_ANA"
    }, block=5)
    add("C12", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_ANA", 2: "AGND"}, block=5)
    add("C13", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_ANA", 2: "AGND"}, block=5)
    add("R_OEN", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "SR_OE_N"}, block=5)
    add("R_POR", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "SR_CLR_N"}, block=5)
    add("C_POR", "C_THT", "1uF", "C_Disc_P5.00mm", {1: "SR_CLR_N", 2: "AGND"}, block=5)

    add("U8", "DG412LDJ", "DG412LDJ", "Socket_DIP-16_W7.62mm", {
        1: "SW_G1", 2: "TP1_MUX", 3: "AGND", 4: "AGND", 5: "AGND",
        6: "AGND", 7: "AGND", 8: "AGND",
        9: "SW_G3", 10: "TP3_MUX", 11: "AGND", 12: "+3V3_ANA", 13: "+12V_A",
        14: "AGND", 15: "TP2_MUX", 16: "SW_G2"
    }, block=5)
    add("C21", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+12V_A", 2: "AGND"}, block=5)

    for ch in (1, 2, 3):
        tp_dut = f"TP{ch}_DUT"
        tp_mux = f"TP{ch}_MUX"
        add(f"U{ch}", "DG408DJ", "DG408DJ", "Socket_DIP-16_W7.62mm", {
            1: f"U{ch}_A0", 2: f"U{ch}_EN", 3: "AGND",
            4: f"C{ch}_S1", 5: f"C{ch}_S2", 6: f"C{ch}_S3", 7: f"C{ch}_S4",
            8: tp_mux, 9: "+9V_LIM", 10: "AGND", 11: f"C{ch}_S6", 12: f"C{ch}_S5",
            13: "+12V_A", 14: "AGND", 15: f"U{ch}_A2", 16: f"U{ch}_A1"
        }, block=5)
        add(f"C{13+ch}", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+12V_A", 2: "AGND"}, block=5)
        add(f"R_EN{ch}", "R_THT", "100k", "R_Axial_P7.62mm", {1: f"U{ch}_EN", 2: "AGND"}, block=5)
        add(f"R_PD{ch}", "R_THT", "100k", "R_Axial_P7.62mm", {1: f"SW_G{ch}", 2: "AGND"}, block=5)
        add(f"R_PR{ch}", "R_THT", "100R_0.5W_0.1%", "R_Axial_P7.62mm", {1: tp_dut, 2: tp_mux}, block=5)
        add(f"D_CH{ch}", "D_THT", "1N3595", "D_DO-35_P7.62mm", {1: "+12V_A", 2: tp_mux}, block=5)
        add(f"D_CL{ch}", "D_THT", "1N3595", "D_DO-35_P7.62mm", {1: tp_mux, 2: "AGND"}, block=5)
        add(f"R_LA{ch}", "R_THT", "470R_0.1%", "R_Axial_P7.62mm", {1: f"C{ch}_S1", 2: "V_SRC_A"}, block=5)
        add(f"R_KA{ch}", "R_THT", "2.49k_0.1%", "R_Axial_P7.62mm", {1: f"C{ch}_S2", 2: "V_SRC_A"}, block=5)
        add(f"R_MB{ch}", "R_THT", "100k_0.1%", "R_Axial_P7.62mm", {1: f"C{ch}_S3", 2: "V_SRC_B"}, block=5)
        add(f"R_KB{ch}", "R_THT", "2.20M_0.1%", "R_Axial_P7.62mm", {1: f"C{ch}_S4", 2: "V_SRC_B"}, block=5)
        add(f"R_LG{ch}", "R_THT", "470R_0.1%", "R_Axial_P7.62mm", {1: f"C{ch}_S5", 2: "AGND"}, block=5)
        add(f"R_KG{ch}", "R_THT", "2.49k_0.1%", "R_Axial_P7.62mm", {1: f"C{ch}_S6", 2: "AGND"}, block=5)

    add("J_ZIF", "ZIF_14Pin", "ZIF-14_DUT", "Socket_ZIF_14Pin", {
        1: "TP1_DUT", 2: "TP1_DUT", 3: "TP1_DUT", 4: "TP2_DUT",
        5: "TP3_DUT", 6: "TP3_DUT", 7: "TP3_DUT",
        8: "TP3_DUT", 9: "TP3_DUT", 10: "TP3_DUT", 11: "TP2_DUT",
        12: "TP1_DUT", 13: "TP1_DUT", 14: "TP1_DUT",
    }, block=5)
    add("J_DUT3", "Conn_1x03", "DUT_3PIN_SKT", "Socket_1x03_DUT", {
        1: "TP1_DUT", 2: "TP2_DUT", 3: "TP3_DUT"
    }, block=5)
    add("J_KELVIN", "Conn_1x04", "KELVIN_LEADS", "Header_1x04_P2.54mm", {
        1: "TP1_DUT", 2: "TP2_DUT", 3: "TP3_DUT", 4: "AGND"
    }, block=5)
    add("J_TERM5", "TermBlock_5Pin", "PROBE_5P_508", "TerminalBlock_5Pin_508", {
        1: "TP1_DUT", 2: "TP2_DUT", 3: "TP3_DUT", 4: "AGND", 5: "DMM_30V"
    }, block=5)
    add("J_TEMP", "DS18B20_Socket", "DS18B20_TEMP", "Socket_TO92_3Pin", {
        1: "AGND", 2: "TEMP_DQ", 3: "+3V3_ANA"
    }, block=5)
    add("R_TEMP", "R_THT", "4.7k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "TEMP_DQ"}, block=5)

    # --- BLOCK 6: ADS1115 PRECISION SENSING, MCP4822 FORCE DAC, LMC6484IN BUFFER & DG409DJ CAL/HV MUX ---
    add("U9", "MCP4822", "MCP4822-E/P", "Socket_DIP-8_W7.62mm", {
        1: "+3V3_ANA", 2: "DAC_CS", 3: "AFE_SPI_SCK", 4: "AFE_SPI_MOSI",
        5: "AGND", 6: "DAC2_RAW", 7: "AGND", 8: "DAC1_RAW"
    }, block=6)
    add("C19", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_ANA", 2: "AGND"}, block=6)
    add("R_DACCS", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "DAC_CS"}, block=6)
    add("R_DAC1", "R_THT", "10k", "R_Axial_P7.62mm", {1: "DAC1_RAW", 2: "DAC1_FILT"}, block=6)
    add("C_DAC1", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "DAC1_FILT", 2: "AGND"}, block=6)
    add("R_DP1", "R_THT", "100k", "R_Axial_P7.62mm", {1: "DAC1_FILT", 2: "AGND"}, block=6)
    add("R_DAC2", "R_THT", "10k", "R_Axial_P7.62mm", {1: "DAC2_RAW", 2: "DAC2_FILT"}, block=6)
    add("C_DAC2", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "DAC2_FILT", 2: "AGND"}, block=6)
    add("R_DP2", "R_THT", "100k", "R_Axial_P7.62mm", {1: "DAC2_FILT", 2: "AGND"}, block=6)

    add("U5", "TLC2272ACP", "TLC2272ACP", "Socket_DIP-8_W7.62mm", {
        1: "V_SRC_A", 2: "FB_SRCA", 3: "DAC1_FILT", 4: "AGND",
        5: "DAC2_FILT", 6: "FB_SRCB", 7: "V_SRC_B", 8: "+12V_A"
    }, block=6)
    add("C10", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+12V_A", 2: "AGND"}, block=6)
    add("R_G1A", "R_THT", "34.8k_0.1%", "R_Axial_P7.62mm", {1: "V_SRC_A", 2: "FB_SRCA"}, block=6)
    add("R_G1B", "R_THT", "10.0k_0.1%", "R_Axial_P7.62mm", {1: "FB_SRCA", 2: "AGND"}, block=6)
    add("R_G2A", "R_THT", "34.8k_0.1%", "R_Axial_P7.62mm", {1: "V_SRC_B", 2: "FB_SRCB"}, block=6)
    add("R_G2B", "R_THT", "10.0k_0.1%", "R_Axial_P7.62mm", {1: "FB_SRCB", 2: "AGND"}, block=6)

    add("R_HV1", "R_THT", "100k_0.1%", "R_Axial_P7.62mm", {1: "DMM_30V", 2: "DMM_30V_DIV"}, block=6)
    add("R_HV2", "R_THT", "10.0k_0.1%", "R_Axial_P7.62mm", {1: "DMM_30V_DIV", 2: "AGND"}, block=6)
    add("D_HVH", "D_THT", "1N3595", "D_DO-35_P7.62mm", {1: "+12V_A", 2: "DMM_30V_DIV"}, block=6)
    add("D_HVL", "D_THT", "1N3595", "D_DO-35_P7.62mm", {1: "DMM_30V_DIV", 2: "AGND"}, block=6)

    add("U10", "DG409DJ", "DG409DJ", "Socket_DIP-16_W7.62mm", {
        1: "CAL_A0", 2: "+3V3_ANA", 3: "AGND",
        4: "V_SRC_A", 5: "V_SRC_B", 6: "+9V_LIM", 7: "DMM_30V_DIV", 8: "CAL_MUX_OUT",
        9: "AGND", 10: "AGND", 11: "AGND", 12: "AGND", 13: "AGND",
        14: "+12V_A", 15: "AGND", 16: "CAL_A1"
    }, block=6)
    add("C20", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+12V_A", 2: "AGND"}, block=6)

    add("U4", "LMC6484IN", "LMC6484IN", "Socket_DIP-14_W7.62mm", {
        1: "BUF_TP1", 2: "BUF_TP1", 3: "ESD_TP1", 4: "+12V_A",
        5: "ESD_TP2", 6: "BUF_TP2", 7: "BUF_TP2",
        8: "BUF_TP3", 9: "BUF_TP3", 10: "ESD_TP3", 11: "AGND",
        12: "CAL_MUX_OUT", 13: "BUF_CAL", 14: "BUF_CAL"
    }, block=6)
    add("C11", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+12V_A", 2: "AGND"}, block=6)

    for idx, (tp, esd, buf, adc, fast) in enumerate([
        ("TP1_DUT", "ESD_TP1", "BUF_TP1", "ADC_TP1", "FAST_TP1"),
        ("TP2_DUT", "ESD_TP2", "BUF_TP2", "ADC_TP2", "FAST_TP2"),
        ("TP3_DUT", "ESD_TP3", "BUF_TP3", "ADC_TP3", "FAST_TP3"),
    ], 1):
        add(f"R_ESD{idx}", "R_THT", "10k", "R_Axial_P7.62mm", {1: tp, 2: esd}, block=6)
        add(f"R_D{idx}A", "R_THT", "1.50k_0.1%", "R_Axial_P7.62mm", {1: buf, 2: adc}, block=6)
        add(f"R_D{idx}B", "R_THT", "499R_0.1%", "R_Axial_P7.62mm", {1: adc, 2: "AGND"}, block=6)
        add(f"C_A{idx-1}", "C_THT", "100nF_C0G", "C_Disc_P5.00mm", {1: adc, 2: "AGND"}, block=6)
        add(f"R_ISO{idx}", "R_THT", "10k", "R_Axial_P7.62mm", {1: adc, 2: fast}, block=6)

    add("R_D4A", "R_THT", "1.50k_0.1%", "R_Axial_P7.62mm", {1: "BUF_CAL", 2: "ADC_CAL"}, block=6)
    add("R_D4B", "R_THT", "499R_0.1%", "R_Axial_P7.62mm", {1: "ADC_CAL", 2: "AGND"}, block=6)
    add("C_A3", "C_THT", "100nF_C0G", "C_Disc_P5.00mm", {1: "ADC_CAL", 2: "AGND"}, block=6)

    add("MOD_ADS", "ADS1115_Module", "ADS1115_16Bit", "Socket_ADS1115_Module", {
        1: "+3V3_ADC", 2: "AGND", 3: "ANA_I2C_SCL", 4: "ANA_I2C_SDA",
        5: "AGND", 6: "ADS_ALRT", 7: "ADC_TP1", 8: "ADC_TP2", 9: "ADC_TP3", 10: "ADC_CAL"
    }, block=6)
    add("C9", "C_THT", "100nF", "C_Disc_P5.00mm", {1: "+3V3_ADC", 2: "AGND"}, block=6)
    add("R_SDA", "R_THT", "4.7k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "ANA_I2C_SDA"}, block=6)
    add("R_SCL", "R_THT", "4.7k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "ANA_I2C_SCL"}, block=6)
    add("R_ALRT", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_ANA", 2: "ADS_ALRT"}, block=6)

    # --- BLOCK 7: DUAL 1.9" ST7789 IPS DISPLAYS & RETRO CONTROLS (D-PAD, A/B, EC11, SW_STBY) ---
    add("MOD_TFT1", "TFT_1_9_ST7789", "TFT1_1.9_IPS_CURVE", "Socket_TFT_1_9_Curve", {
        1: "DGND", 2: "+3V3_DIG", 3: "UI_SPI_SCK", 4: "UI_SPI_MOSI",
        5: "TFT_RES", 6: "TFT_DC", 7: "TFT1_CS", 8: "TFT_BLK"
    }, block=7)
    add("MOD_TFT2", "TFT_1_9_ST7789", "TFT2_1.9_IPS_DMM", "Socket_TFT_1_9_DMM", {
        1: "DGND", 2: "+3V3_DIG", 3: "UI_SPI_SCK", 4: "UI_SPI_MOSI",
        5: "TFT_RES", 6: "TFT_DC", 7: "TFT2_CS", 8: "TFT_BLK"
    }, block=7)
    add("R_T1CS", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "TFT1_CS"}, block=7)
    add("R_T2CS", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "TFT2_CS"}, block=7)
    add("R_TRES", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "TFT_RES"}, block=7)
    add("C_TRES", "C_THT", "1uF", "C_Disc_P5.00mm", {1: "TFT_RES", 2: "DGND"}, block=7)
    add("R_PD_BLK", "R_THT", "100k", "R_Axial_P7.62mm", {1: "TFT_BLK_EN", 2: "DGND"}, block=7)
    add("R_G_BLK", "R_THT", "100R", "R_Axial_P7.62mm", {1: "TFT_BLK_EN", 2: "Q_BLK_G"}, block=7)
    add("Q_BLK", "NMOS_2N7000", "2N7000", "Socket_TO92_3Pin", {
        1: "DGND", 2: "Q_BLK_G", 3: "TFT_BLK"
    }, block=7)

    for sw_ref, sw_val, sw_fp, btn_net, r_ref, c_ref in [
        ("SW_UP",    "DPAD_UP",    "SW_Tact_12mm_Yellow", "BTN_UP",    "R_PU_UP", "C_DB_UP"),
        ("SW_DOWN",  "DPAD_DOWN",  "SW_Tact_12mm_Yellow", "BTN_DOWN",  "R_PU_DN", "C_DB_DN"),
        ("SW_LEFT",  "DPAD_LEFT",  "SW_Tact_12mm_Yellow", "BTN_LEFT",  "R_PU_LF", "C_DB_LF"),
        ("SW_RIGHT", "DPAD_RIGHT", "SW_Tact_12mm_Yellow", "BTN_RIGHT", "R_PU_RT", "C_DB_RT"),
        ("SW_A",     "BTN_A_SCAN", "SW_Tact_12mm_Red",    "BTN_A",     "R_PU_BA", "C_DB_BA"),
        ("SW_B",     "BTN_B_BACK", "SW_Tact_12mm_Blue",   "BTN_B",     "R_PU_BB", "C_DB_BB"),
    ]:
        add(sw_ref, "SW_THT", sw_val, sw_fp, {1: btn_net, 2: "DGND"}, block=7)
        add(r_ref,  "R_THT",  "10k",   "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: btn_net}, block=7)
        add(c_ref,  "C_THT",  "100nF", "C_Disc_P5.00mm",  {1: btn_net, 2: "DGND"}, block=7)

    add("ENC1", "EC11_Encoder", "EC11_NAV_KNOB", "EC11_Knob_Encoder", {
        1: "ENC_A", 2: "DGND", 3: "ENC_B", 4: "ENC_SW", 5: "DGND"
    }, block=7)
    add("R_PU_ENCA",  "R_THT", "10k",   "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "ENC_A"}, block=7)
    add("R_PU_ENCB",  "R_THT", "10k",   "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "ENC_B"}, block=7)
    add("R_PU_ENCSW", "R_THT", "10k",   "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "ENC_SW"}, block=7)
    add("C_DB_ENCA",  "C_THT", "10nF",  "C_Disc_P5.00mm",  {1: "ENC_A", 2: "DGND"}, block=7)
    add("C_DB_ENCB",  "C_THT", "10nF",  "C_Disc_P5.00mm",  {1: "ENC_B", 2: "DGND"}, block=7)
    add("C_DB_ENCSW", "C_THT", "100nF", "C_Disc_P5.00mm",  {1: "ENC_SW", 2: "DGND"}, block=7)

    add("SW_STBY", "SW_SPDT_THT", "STANDBY_RF_MODE", "SW_Slide_SPDT_THT", {
        1: "DGND", 2: "SW_STBY", 3: None
    }, block=7)
    add("R_PU_STBY", "R_THT", "10k", "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "SW_STBY"}, block=7)

    # --- BLOCK 8: TOP 6-LED STATUS BAR & 56K MODEM BUZZER STAGE (WITH HARDWARE MUTE & RC SNUBBER) ---
    add("D_LED1", "LED_THT", "1_PWR_GREEN",    "LED_D3mm_Green",  {1: "DGND", 2: "LED_PWR_A"}, block=8)
    add("R_LED_PWR", "R_THT", "1.0k",          "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "LED_PWR_A"}, block=8)
    add("D_LED2", "LED_THT", "2_CHG_AMBER",    "LED_D3mm_Yellow", {1: "CHG_STAT_N", 2: "LED_CHG_A"}, block=8)
    add("R_LED_CHG", "R_THT", "1.5k",          "R_Axial_P7.62mm", {1: "+3V3_DIG", 2: "LED_CHG_A"}, block=8)
    add("D_LED3", "LED_THT", "3_STBY_RF_CYAN", "LED_D3mm_Blue",   {1: "DGND", 2: "LED3_A"}, block=8)
    add("R_LED3", "R_THT",   "470R",           "R_Axial_P7.62mm", {1: "LED_STBY_RF", 2: "LED3_A"}, block=8)
    add("D_LED4", "LED_THT", "4_SCAN_BLUE",    "LED_D3mm_Blue",   {1: "DGND", 2: "LED4_A"}, block=8)
    add("R_LED4", "R_THT",   "470R",           "R_Axial_P7.62mm", {1: "LED_SCAN", 2: "LED4_A"}, block=8)
    add("D_LED5", "LED_THT", "5_DUT_OK_GREEN", "LED_D3mm_Green",  {1: "DGND", 2: "LED5_A"}, block=8)
    add("R_LED5", "R_THT",   "470R",           "R_Axial_P7.62mm", {1: "LED_DUT_OK", 2: "LED5_A"}, block=8)
    add("D_LED6", "LED_THT", "6_FAULT_RED",    "LED_D3mm_Red",    {1: "DGND", 2: "LED6_A"}, block=8)
    add("R_LED6", "R_THT",   "470R",           "R_Axial_P7.62mm", {1: "LED_FAULT", 2: "LED6_A"}, block=8)

    add("D1",     "LED_THT", "ANA_9V_ON",      "LED_D3mm_Green",  {1: "AGND", 2: "LED1_A"}, block=8)
    add("R_LED1", "R_THT",   "3.3k",           "R_Axial_P7.62mm", {1: "+9V_REF", 2: "LED1_A"}, block=8)
    add("D2",     "LED_THT", "AFE_ACT",        "LED_D3mm_Blue",   {1: "AGND", 2: "LED2_A"}, block=8)
    add("R_LED2", "R_THT",   "1.0k",           "R_Axial_P7.62mm", {1: "LED2_DRV", 2: "LED2_A"}, block=8)

    add("SW_BZ",   "SW_SPDT_THT", "MUTE_SWITCH", "SW_Slide_SPDT_THT", {1: None, 2: "BZ_POS", 3: "+3V3_DIG"}, block=8)
    add("BZ1",     "Buzzer_THT",  "BUZZER_12MM", "Buzzer_12mm",       {1: "BZ_POS", 2: "BZ_COL"}, block=8)
    add("D_BZ",    "D_THT",       "1N4148",      "D_DO-35_P7.62mm",   {1: "BZ_POS", 2: "BZ_COL"}, block=8)
    add("R_SNUB",  "R_THT",       "100R",        "R_Axial_P7.62mm",   {1: "BZ_POS", 2: "BZ_SNUB_MID"}, block=8)
    add("C_SNUB",  "C_THT",       "10nF",        "C_Disc_P5.00mm",    {1: "BZ_SNUB_MID", 2: "BZ_COL"}, block=8)
    add("Q_BZ",    "NPN_2N3904",  "2N3904",      "Socket_TO92_3Pin",  {1: "DGND", 2: "Q_BZ_B", 3: "BZ_COL"}, block=8)
    add("R_BZ_B",  "R_THT",       "1.0k",        "R_Axial_P7.62mm",   {1: "BZ_PWM", 2: "Q_BZ_B"}, block=8)
    add("R_BZ_PD", "R_THT",       "10k",         "R_Axial_P7.62mm",   {1: "Q_BZ_B", 2: "DGND"}, block=8)

    return comps


# ==============================================================================
# 2. 8-BLOCK BORDERED A2 SCHEMATIC GENERATOR (0 ERC ERRORS / 0 WARNINGS)
# ==============================================================================
def parse_sym_pin_offsets():
    txt = open(SYM_PATH).read()
    sym_blocks = re.split(r'\n  \(symbol "', txt)[1:]
    pin_map = {}
    for blk in sym_blocks:
        sname = blk.split('"', 1)[0]
        pins = {}
        for m in re.finditer(r'\(pin \w+ line \(at ([\d\.\-]+) ([\d\.\-]+) (\d+)\).*?\(number "(\w+)"', blk):
            px, py, ang, num = float(m.group(1)), float(m.group(2)), int(m.group(3)), m.group(4)
            pins[num] = (px, py, ang)
        pin_map[f"Ge_Engine:{sname}"] = pins
    return pin_map


def embed_lib_symbols():
    txt = open(SYM_PATH).read()
    inner = txt[txt.find('\n  (symbol'):txt.rfind(')')]
    inner = re.sub(r'\n  \(symbol "([^"]+)"', lambda m: f'\n    (symbol "Ge_Engine:{m.group(1)}"', inner)
    return f"  (lib_symbols\n{inner}\n  )\n"


def generate_schematic(comps):
    pin_offsets = parse_sym_pin_offsets()

    # Auto-layout each of the 8 functional blocks into a clean bordered box on an A1/A2 sheet
    # Paper A1 (841 x 594 mm) gives ultra-spacious, zero-overlap cells for all 242 components!
    block_defs = {
        1: ("1. POWER, 3.7V NON-HV LIPO PROTECTION & TI BQ25895 1.5MHz NVDC FAST-CHARGER",         15.24,  15.24, 266.70, 185.42, 4),
        2: ("2. SWITCHABLE ANALOG RAIL, 2x CRITICALLY DAMPED LC PI-FILTERS, LDOs & 15mA LIMITER", 274.32,  15.24, 558.80, 185.42, 5),
        3: ("3. MCU 1 QUIET ANALOG CORE (LOLIN32 LITE) & 2x H11L1M OPTO-ISOLATED UART",           566.42,  15.24, 825.50, 185.42, 3),
        4: ("4. MCU 2 COMMS/UI/AUDIO CORE (ESP32-C3 SUPERMINI) & U11 MCP23017 EXPANDER",           15.24, 193.04, 266.70, 381.00, 3),
        5: ("5. SOLID-STATE 3-TERMINAL AFE MATRIX (74HC595N, 3x DG408DJ, DG412LDJ) & FAILSAFES",  274.32, 193.04, 825.50, 381.00, 8),
        6: ("6. ADS1115 16-BIT ADC, MCP4822 DAC, TLC2272ACP, LMC6484IN 20fA BUFFER & DG409DJ",     15.24, 388.62, 396.24, 568.96, 6),
        7: ("7. DUAL 1.9in ST7789 IPS DISPLAYS, BACKLIGHT SWITCH & RETRO D-PAD / ENCODER",        403.86, 388.62, 629.92, 568.96, 4),
        8: ("8. TOP 6-LED STATUS BAR & 56K MODEM BUZZER STAGE (MUTE SWITCH & RC SNUBBER)",        637.54, 388.62, 825.50, 568.96, 4),
    }

    by_block = defaultdict(list)
    for c in comps:
        by_block[c["block"]].append(c)

    sch_pos = {}
    for bnum, bcomps in by_block.items():
        title, x1, y1, x2, y2, ncols = block_defs[bnum]
        col_w = (x2 - x1 - 45.72) / max(1, ncols - 1) if ncols > 1 else 0
        col_x = [round((x1 + 22.86 + i * col_w) / 2.54) * 2.54 for i in range(ncols)]
        col_y = [round((y1 + 22.86) / 2.54) * 2.54 for _ in range(ncols)]

        for c in bcomps:
            poffs = pin_offsets[c["sym"]]
            ys = [py for (_, py, _) in poffs.values()]
            half_h = max([abs(y) for y in ys] + [2.54]) + 7.62
            ci = min(range(ncols), key=lambda i: col_y[i])
            cy = round((col_y[ci] + half_h) / 2.54) * 2.54
            sch_pos[c["ref"]] = (col_x[ci], cy)
            col_y[ci] = round((cy + half_h + 7.62) / 2.54) * 2.54

    sheet_uuid = uid()
    out = [f"""(kicad_sch (version 20231120) (generator "ge_engine_gen")
  (uuid "{sheet_uuid}")
  (paper "A1")
  (title_block
    (title "KAOTI GERMANIUM PARAMETRIC CHARACTERIZATION ENGINE — REV 5.0 DUAL-MCU")
    (date "2026-11-21")
    (rev "REV 5.0")
    (company "KAOTI AUDIO")
    (comment 1 "Dual-MCU: WEMOS LOLIN32 Lite (Analog Core) + ESP32-C3 SuperMini (UI/RF) + 2x H11L1M Opto-UART")
    (comment 2 "TI BQ25895 NVDC 4.192V Fast Charger + 2x MCP1826S LDOs + 2x Damped LC Pi-Filters + R_STAR Ground")
  )
"""]
    out.append(embed_lib_symbols())

    for bnum, (title, x1, y1, x2, y2, _) in block_defs.items():
        out.append(f"""  (polyline
    (pts (xy {x1:.2f} {y1:.2f}) (xy {x2:.2f} {y1:.2f}) (xy {x2:.2f} {y2:.2f}) (xy {x1:.2f} {y2:.2f}) (xy {x1:.2f} {y1:.2f}))
    (stroke (width 0.35) (type dash)) (fill (type none)) (uuid "{uid()}")
  )
""")
        out.append(f'  (text "{title}" (exclude_from_sim no) (at {x1 + 5.08:.2f} {y1 + 6.35:.2f} 0) (effects (font (size 2.0 2.0) bold) (justify left)) (uuid "{uid()}"))\n')

    for c in comps:
        sx, sy = sch_pos[c["ref"]]
        suuid = uid()
        poffs = pin_offsets[c["sym"]]
        ys = [py for (_, py, _) in poffs.values()]
        box_half = max([abs(y) for y in ys] + [2.54]) + 3.81
        pins_inst = "\n".join(f'    (pin "{p}" (uuid "{uid()}"))' for p in c["pins"].keys())
        out.append(f"""  (symbol (lib_id "{c['sym']}") (at {sx:.2f} {sy:.2f} 0) (unit 1)
    (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no)
    (uuid "{suuid}")
    (property "Reference" "{c['ref']}" (at {sx:.2f} {sy - box_half:.2f} 0) (effects (font (size 1.27 1.27))))
    (property "Value" "{c['val']}" (at {sx:.2f} {sy + box_half:.2f} 0) (effects (font (size 1.15 1.15))))
    (property "Footprint" "{c['fp']}" (at {sx:.2f} {sy:.2f} 0) (effects (font (size 1.27 1.27)) hide))
    (property "Datasheet" "" (at {sx:.2f} {sy:.2f} 0) (effects (font (size 1.27 1.27)) hide))
{pins_inst}
    (instances
      (project "Ge_Parametric_Engine"
        (path "/{sheet_uuid}" (reference "{c['ref']}") (unit 1))
      )
    )
  )
""")
        for pnum, net in c["pins"].items():
            px, py, pang = poffs[pnum]
            wx = round(sx + px, 2)
            wy = round(sy - py, 2)
            if net is None:
                out.append(f'  (no_connect (at {wx:.2f} {wy:.2f}) (uuid "{uid()}"))\n')
            else:
                dx = -2.54 if pang == 0 else 2.54
                lx = round(wx + dx, 2)
                lang = 180 if pang == 0 else 0
                just = "right" if pang == 0 else "left"
                out.append(f'  (wire (pts (xy {wx:.2f} {wy:.2f}) (xy {lx:.2f} {wy:.2f})) (stroke (width 0) (type default)) (uuid "{uid()}"))\n')
                out.append(f'  (global_label "{net}" (shape passive) (at {lx:.2f} {wy:.2f} {lang}) (fields_autoplaced yes) (effects (font (size 1.016 1.016)) (justify {just})) (uuid "{uid()}"))\n')

    out.append("""  (sheet_instances
    (path "/" (page "1"))
  )
)
""")
    sch_path = os.path.join(PROJ_DIR, "Ge_Parametric_Engine.kicad_sch")
    with open(sch_path, "w") as f:
        f.write("".join(out))
    print("Generated Rev 5.0 8-Block schematic:", sch_path)


# ==============================================================================
# 3. REV 5.0 SYMMETRICAL ROUNDED-CORNER MULTIMETER PCB PLACEMENT (260 x 155 mm)
# ==============================================================================
def parse_fp_pads(fp_name):
    fname = fp_name.split(":")[-1] + ".kicad_mod"
    txt = open(os.path.join(FP_DIR, fname)).read()
    pads = []
    for m in re.finditer(
        r'\(pad "([^"]*)" (\w+) (\w+) \(at ([\d\.\-]+) ([\d\.\-]+)(?: [\d\.\-]+)?\) \(size ([\d\.]+) ([\d\.]+)\) \(drill ([\d\.]+)\)',
        txt
    ):
        pads.append((m.group(1), m.group(2), m.group(3), float(m.group(4)), float(m.group(5)),
                     float(m.group(6)), float(m.group(7)), float(m.group(8))))
    return pads, txt


def transform_pt(x, y, px, py, rot_deg):
    rad = math.radians(rot_deg)
    ca, sa = math.cos(rad), math.sin(rad)
    return px + x * ca + y * sa, py - x * sa + y * ca


def pcb_placement_rev5():
    """
    260.0 x 155.0 mm Symmetrical Rounded-Corner Handheld/Benchtop Multimeter Console (R = 10.0 mm):
      - LEFT SYMMETRICAL HANDHELD CONSOLE (X = 8 .. 148, Centerline X_sym = 78.74 mm, DGND Zone):
          * Top 6-LED Status Bezel Bar symmetric about X_sym = 78.74 mm (Y = 8.89 mm)
          * Dual 1.9" ST7789 IPS Screens symmetric about X_sym = 78.74 mm (Y = 20.32 and 52.07 mm)
          * Retro Control Console symmetric about X_sym = 78.74 mm:
              - Left D-Pad cross (SW_UP, SW_DOWN, SW_LEFT, SW_RIGHT) centered at X = 31.75 mm
              - Center Alps EC11 Rotary Encoder (ENC1) shaft centered on X_sym = 78.74 mm
              - Right Nintendo A & B Buttons (SW_A, SW_B) centered at X = 123.19 mm
          * Bottom Centered DUT Test Bay symmetric about X_sym = 78.74 mm (J_TERM5, J_ZIF, J_DUT3, J_KELVIN, J_TEMP)
          * Upper-mid Digital/Charger Strip (X = 114 .. 148): U_MCU2 (ESP32-C3 SuperMini at top edge),
            MOD_CHG (BQ25895), U_LDO_DIG, Q_REV_BAT, SW_PWR, SW_STBY, SW_BZ, BZ1, U11 (MCP23017).
      - GALVANIC BOUNDARY (X = 152.40 mm):
          * U_ISO1 & U_ISO2 (2x H11L1M DIP-6 straddling DGND/AGND moat) + R_STAR (0R_STAR_BRIDGE).
      - RIGHT QUIET ANALOG CORE (X = 156 .. 252, AGND Zone):
          * Top Analog Power & Dual LC Pi-Filters (MOD_BOOST, L1/R_DAMP1, U_REG12, F1, D_OVP, U_REG9,
            Q_LIM1/2, Q_OVC, L2/R_DAMP2, U_LDO_ANA, R_ADC_F)
          * Mid/Bottom Quiet Metrology Core: U_MCU1 (LOLIN32 Lite), U9 (MCP4822), U5 (TLC2272ACP),
            MOD_ADS (ADS1115), U4 (LMC6484IN), U10 (DG409DJ), U6/U7 (74HC595N), U8 (DG412LDJ),
            and 3x Symmetric DG408DJ Channels (U1, U2, U3) with 0.1% R.G. Keen resistor banks.
    """
    pos = {}

    # ==================== 1. SYMMETRICAL FRONT-PANEL CONSOLE (X_sym = 78.74 mm) ====================
    # Top 6-LED Status Bezel Bar at Y = 7.62 mm, centered symmetrically around X_sym = 78.74 mm
    # LED footprint center is at px + 1.27 mm -> place px at X_sym - 1.27 + offset
    led_offsets = [-38.10, -22.86, -7.62, +7.62, +22.86, +38.10]
    for idx, (lref, rref) in enumerate([
        ("D_LED1", "R_LED_PWR"), ("D_LED2", "R_LED_CHG"), ("D_LED3", "R_LED3"),
        ("D_LED4", "R_LED4"),    ("D_LED5", "R_LED5"),    ("D_LED6", "R_LED6"),
    ]):
        lx = round((CENTER_X - 1.27 + led_offsets[idx]) / 1.27) * 1.27
        pos[lref] = (lx, 7.62, 0)
        pos[rref] = (lx - 2.54, 12.70, 0)

    # Dual 1.9" ST7789 IPS Screens stacked symmetrically on X_sym = 78.74 mm
    # Footprint visual center is at px + 29.0 mm -> px = 78.74 - 29.0 = 49.53 mm
    pos["MOD_TFT1"] = (49.53, 21.59, 0)   # X = 47.5..109.5, Y = 16.0..45.0
    pos["MOD_TFT2"] = (49.53, 52.07, 0)   # X = 47.5..109.5, Y = 46.5..75.5

    # Ergonomic Retro Control Console (Y = 80..112 mm) symmetric about X_sym = 78.74 mm:
    # Left D-Pad Cross (SW_UP, SW_DOWN, SW_LEFT, SW_RIGHT; button center is px + 6.25, py + 2.50)
    pos["SW_UP"]    = (25.40, 81.28, 0)   # center X = 31.65, Y = 83.78
    pos["SW_DOWN"]  = (25.40, 99.06, 0)   # center X = 31.65, Y = 101.56
    pos["SW_LEFT"]  = (10.16, 90.17, 0)   # center X = 16.41, Y = 92.67
    pos["SW_RIGHT"] = (40.64, 90.17, 0)   # center X = 46.89, Y = 92.67

    # Center EC11 Rotary Encoder (shaft center is px + 7.50, py + 2.50 -> px = 78.74 - 7.50 = 71.12 mm!)
    pos["ENC1"]     = (71.12, 90.17, 0)   # shaft at X = 78.62 (~78.74 centerline!), Y = 92.67

    # Right Nintendo A & B Action Buttons (mirrored opposite D-Pad across X_sym = 78.74 mm!)
    pos["SW_B"]     = (106.68, 93.98, 0)  # center X = 112.93, Y = 96.48
    pos["SW_A"]     = (123.19, 86.36, 0)  # center X = 129.44, Y = 88.86

    # Accessible Top/Left Mode & Power Slide Switches + USB-C & Battery Input (Left Edge X = 10..42, Y = 16..74)
    pos["J_USB_C"]     = (11.43, 19.05, 0)
    pos["R_CC1"]       = (11.43, 24.13, 0)
    pos["R_CC2"]       = (22.86, 24.13, 0)
    pos["F_USB"]       = (34.29, 24.13, 0)
    pos["D_TVS_USB"]   = (11.43, 29.21, 0)
    pos["C_USB_IN"]    = (25.40, 29.21, 0)

    pos["J_BAT"]       = (11.43, 36.83, 0)
    pos["J_NTC"]       = (19.05, 36.83, 0)
    pos["SW_PWR"]      = (27.94, 36.83, 0)   # Master DPDT Power Switch
    pos["R_BLEED_SYS"] = (11.43, 44.45, 0)
    pos["Q_REV_BAT"]   = (24.13, 46.99, 0)   # NDP6020P Ideal Diode
    pos["R_G_REV"]     = (34.29, 46.99, 0)

    pos["MOD_CHG"]     = (12.70, 58.42, 0)   # TI BQ25895 2x6 Socket (X = 12.7..27.9, Y = 58.4..71.1)
    pos["C_PMID"]      = (34.29, 55.88, 0)
    pos["C_BAT_BYP"]   = (34.29, 66.04, 0)
    pos["C_SYS1"]      = (41.91, 55.88, 0)
    pos["C_SYS2"]      = (41.91, 63.50, 0)

    pos["U_LDO_DIG"]   = (40.64, 33.02, 0)   # MCP1826S-3302E/AB (+3V3_DIG)
    pos["C_DIG1"]      = (41.91, 41.91, 0)
    pos["C_DIG2"]      = (40.64, 49.53, 0)

    # Bottom Centered DUT Test Bay (Y = 120..146 mm, symmetric around X_sym = 78.74 mm)
    pos["J_TERM5"]     = (20.32, 138.43, 0)  # 5-Pin Screw Terminal on left of DUT bay
    pos["J_ZIF"]       = (74.93, 125.73, 0)  # ZIF-14 centered right on X_sym = 78.74 mm (74.93 + 3.81 = 78.74!)
    pos["J_DUT3"]      = (96.52, 124.46, 0)  # 3-Pin Machined Gold DUT Socket
    pos["J_KELVIN"]    = (96.52, 132.08, 0)  # 4-Pin Guarded Kelvin Header
    pos["J_TEMP"]      = (96.52, 140.97, 0)  # DS18B20 Thermal Probe Socket
    pos["R_TEMP"]      = (106.68, 140.97, 0)

    # D-Pad & Button Hardware RC Debounce Bank (Y = 111..132 mm, flanking ZIF socket)
    dpad_passives = [
        ("R_PU_UP", "C_DB_UP"), ("R_PU_DN", "C_DB_DN"), ("R_PU_LF", "C_DB_LF"),
        ("R_PU_RT", "C_DB_RT"), ("R_PU_BA", "C_DB_BA"), ("R_PU_BB", "C_DB_BB"),
    ]
    for i, (rref, cref) in enumerate(dpad_passives):
        px = 11.43 + (i % 3) * 16.51
        py = 113.03 + (i // 3) * 8.89
        pos[rref] = (px, py, 0)
        pos[cref] = (px, py + 4.06, 0)

    pos["R_PU_ENCA"]  = (57.15, 113.03, 0)
    pos["C_DB_ENCA"]  = (57.15, 117.09, 0)
    pos["R_PU_ENCB"]  = (57.15, 121.92, 0)
    pos["C_DB_ENCB"]  = (57.15, 125.98, 0)
    pos["R_PU_ENCSW"] = (57.15, 130.81, 0)
    pos["C_DB_ENCSW"] = (57.15, 134.87, 0)

    # ==================== 2. DIGITAL UI / MCU 2 / EXPANDER / AUDIO COLUMN (X = 114..148 mm, DGND) ====================
    # U_MCU2 (ESP32-C3 SuperMini): top-center-right so USB-C is at top and C3 antenna has a dedicated keepout!
    pos["U_MCU2"]     = (116.84, 19.05, 0)   # Pins Y = 19.05..36.83; C3 antenna at Y = 37..41
    pos["JP_M2_3V3"]  = (137.16, 19.05, 0)
    pos["R_UI_SDA"]   = (137.16, 24.13, 0)
    pos["R_UI_SCL"]   = (137.16, 28.19, 0)
    pos["J_CYD_UART"] = (137.16, 33.02, 0)
    pos["R_CYD_TX"]   = (137.16, 38.10, 0)
    pos["R_CYD_RX"]   = (137.16, 42.16, 0)

    # U11 (MCP23017-E/SP DIP-28) below U_MCU2 antenna keepout
    pos["U11"]        = (116.84, 48.26, 0)   # DIP-28: X = 116.8..124.5, Y = 48.3..81.3
    pos["C22"]        = (129.54, 48.26, 0)
    pos["R_RST_M11"]  = (129.54, 53.34, 0)
    pos["C_RST_M11"]  = (129.54, 58.42, 0)
    pos["R_T1CS"]     = (129.54, 63.50, 0)
    pos["R_T2CS"]     = (129.54, 67.56, 0)
    pos["R_TRES"]     = (129.54, 71.63, 0)
    pos["C_TRES"]     = (129.54, 75.69, 0)
    pos["Q_BLK"]      = (140.97, 48.26, 0)
    pos["R_PD_BLK"]   = (140.97, 54.61, 0)
    pos["R_G_BLK"]    = (140.97, 58.67, 0)
    pos["SW_STBY"]    = (140.97, 64.77, 0)
    pos["R_PU_STBY"]  = (140.97, 71.12, 0)

    # Audio Buzzer Stage (BZ1 + SW_BZ + Q_BZ + D_BZ + RC Snubber) at X = 114..144, Y = 110..144
    pos["SW_BZ"]      = (116.84, 111.76, 0)
    pos["BZ1"]        = (118.11, 124.46, 0)
    pos["D_BZ"]       = (116.84, 134.62, 0)
    pos["R_SNUB"]     = (116.84, 138.68, 0)
    pos["C_SNUB"]     = (116.84, 142.75, 0)
    pos["Q_BZ"]       = (132.08, 121.92, 0)
    pos["R_BZ_B"]     = (132.08, 128.27, 0)
    pos["R_BZ_PD"]    = (132.08, 132.33, 0)

    # ==================== 3. GALVANIC ISOLATION BOUNDARY (X = 152.40 mm: DGND <-> AGND) ====================
    # U_ISO1 & U_ISO2 straddle the X = 152.40 mm moat, and R_STAR bridges DGND (left) to AGND (right)!
    pos["R_STAR"]       = (148.59, 77.47, 0)   # Pin 1 (148.59, DGND) -> Pin 2 (156.21, AGND)
    pos["Q_ANA_N"]      = (138.43, 77.47, 0)   # In DGND zone driving Q_ANA_SW gate
    pos["R_PD_ANA"]     = (138.43, 83.82, 0)
    pos["R_G_ANA"]      = (138.43, 87.88, 0)

    pos["U_ISO1"]       = (156.21, 91.44, 180) # Rotated 180 so Pin 1..3 (AGND side) are at X=156.21, Pin 4..6 (DGND) at X=148.59!
    pos["R_LED_ISO1"]   = (148.59, 82.55, 0)
    pos["R_PU_ISO1"]    = (138.43, 92.71, 0)
    pos["C_BYP_ISO1"]   = (138.43, 96.77, 0)

    pos["U_ISO2"]       = (148.59, 100.33, 0)  # Pin 1..3 (DGND side) at X=148.59, Pin 4..6 (AGND) at X=156.21!
    pos["R_LED_ISO2"]   = (138.43, 101.60, 0)
    pos["R_PU_ISO2"]    = (148.59, 110.49, 0)
    pos["C_BYP_ISO2"]   = (148.59, 114.55, 0)

    # ==================== 4. RIGHT QUIET ANALOG CORE (X = 156..252 mm, AGND Zone) ====================
    # Top Analog Power & Dual Critically Damped LC Pi-Filters (Y = 12..44 mm)
    pos["Q_ANA_SW"]     = (157.48, 12.70, 0)
    pos["R_PU_SW"]      = (157.48, 19.05, 0)
    pos["MOD_BOOST"]    = (167.64, 12.70, 0)   # X = 167.6..200.7, Y = 12.7..25.4
    pos["C_PI1A"]       = (205.74, 12.70, 0)
    pos["C_PI1B"]       = (205.74, 21.59, 0)
    pos["L1"]           = (214.63, 12.70, 0)
    pos["R_DAMP1"]      = (213.36, 21.59, 0)
    pos["C_PI2A"]       = (223.52, 12.70, 0)
    pos["C_PI2B"]       = (223.52, 21.59, 0)
    pos["U_REG12"]      = (231.14, 12.70, 0)
    pos["F1"]           = (231.14, 19.05, 0)
    pos["D_OVP"]        = (231.14, 24.13, 0)
    pos["C17"]          = (243.84, 12.70, 0)
    pos["C18"]          = (243.84, 21.59, 0)

    # Second Row of Analog Power: L78L09ACZ + 15mA Limiter + Pi-Filter #2 + U_LDO_ANA (Y = 29..44 mm)
    pos["C_A1A"]        = (157.48, 29.21, 0)
    pos["C_A1B"]        = (157.48, 38.10, 0)
    pos["L2"]           = (166.37, 33.02, 0)
    pos["R_DAMP2"]      = (173.99, 31.75, 0)
    pos["C_A2A"]        = (184.15, 29.21, 0)
    pos["C_A2B"]        = (184.15, 38.10, 0)
    pos["U_LDO_ANA"]    = (191.77, 31.75, 0)
    pos["C1"]           = (200.66, 29.21, 0)
    pos["C2"]           = (200.66, 38.10, 0)
    pos["R_ADC_F"]      = (208.28, 31.75, 0)
    pos["C_ADC_FA"]     = (208.28, 38.10, 0)

    pos["U_REG9"]       = (218.44, 30.48, 0)
    pos["C7"]           = (218.44, 38.10, 0)
    pos["C8"]           = (226.06, 38.10, 0)
    pos["Q_LIM1"]       = (227.33, 30.48, 0)
    pos["Q_LIM2"]       = (236.22, 30.48, 0)
    pos["Q_OVC"]        = (245.11, 30.48, 0)
    pos["R_ILIM"]       = (233.68, 36.83, 0)
    pos["R_IBIAS"]      = (233.68, 40.89, 0)
    pos["R_OVC_DIV1"]   = (243.84, 36.83, 0)
    pos["R_OVC_DIV2"]   = (243.84, 40.89, 0)
    pos["R_PU_OVC"]     = (218.44, 43.18, 0)
    pos["D1"]           = (233.68, 44.96, 0)
    pos["R_LED1"]       = (240.03, 44.96, 0)

    # U_MCU1 (WEMOS LOLIN32 Lite, Wi-Fi/BT OFF) at Bottom-Left of Analog Zone (X = 162.56..185.42, Y = 104..134)
    pos["U_MCU1"]       = (162.56, 104.14, 0)  # Pins Y = 104.14..134.62; antenna at Y = 95..102
    pos["JP_M1_3V3"]    = (162.56, 86.36, 0)
    pos["R_LATCH_PD"]   = (170.18, 86.36, 0)

    # Above U_MCU1 (X = 160..198, Y = 48..84): U9 (MCP4822), U5 (TLC2272), MOD_ADS (ADS1115), U4 (LMC6484), U10 (DG409)
    pos["U9"]           = (160.02, 48.26, 0)
    pos["C19"]          = (160.02, 58.42, 0)
    pos["R_DACCS"]      = (160.02, 62.48, 0)
    pos["U5"]           = (171.45, 48.26, 0)
    pos["C10"]          = (171.45, 58.42, 0)
    pos["R_DAC1"]       = (160.02, 66.55, 0)
    pos["C_DAC1"]       = (170.18, 66.55, 0)
    pos["R_DP1"]        = (160.02, 70.61, 0)
    pos["R_G1A"]        = (170.18, 70.61, 0)
    pos["R_G1B"]        = (160.02, 74.68, 0)
    pos["R_DAC2"]       = (170.18, 74.68, 0)
    pos["C_DAC2"]       = (160.02, 78.74, 0)
    pos["R_DP2"]        = (170.18, 78.74, 0)
    pos["R_G2A"]        = (160.02, 82.80, 0)
    pos["R_G2B"]        = (170.18, 82.80, 0)

    pos["MOD_ADS"]      = (184.15, 62.23, 0)   # X = 181.6..209.6, Y = 47.2..64.2
    pos["C9"]           = (181.61, 67.31, 0)
    pos["R_SDA"]        = (189.23, 67.31, 0)
    pos["R_SCL"]        = (199.39, 67.31, 0)
    pos["R_ALRT"]       = (181.61, 71.37, 0)

    pos["U4"]           = (181.61, 76.20, 0)   # LMC6484IN DIP-14 (Y = 76.2..91.4)
    pos["C11"]          = (181.61, 93.98, 0)
    pos["U10"]          = (194.31, 76.20, 0)   # DG409DJ DIP-16 (Y = 76.2..94.0)
    pos["C20"]          = (194.31, 97.79, 0)

    # 4:1 Precision Dividers + 11:1 HV DMM Attenuator (X = 189.23..206.99, Y = 102..146)
    div_items = [
        "R_D1A", "R_D1B", "C_A0", "R_ESD1", "R_ISO1",
        "R_D2A", "R_D2B", "C_A1", "R_ESD2", "R_ISO2",
        "R_D3A", "R_D3B", "C_A2", "R_ESD3", "R_ISO3",
        "R_D4A", "R_D4B", "C_A3", "R_HV1",  "R_HV2", "D_HVH", "D_HVL"
    ]
    for idx, dref in enumerate(div_items):
        dx = 189.23 + (idx % 2) * 10.16
        dy = 102.87 + (idx // 2) * 4.064
        pos[dref] = (dx, dy, 0)

    # Shift Registers U6, U7 + U8 DG412LDJ (Column at X = 210.82 mm, Y = 49..142)
    pos["U6"]           = (210.82, 49.53, 0)
    pos["C12"]          = (212.09, 69.85, 0)
    pos["U7"]           = (210.82, 74.93, 0)
    pos["C13"]          = (212.09, 95.25, 0)
    pos["U8"]           = (210.82, 100.33, 0)
    pos["C21"]          = (212.09, 120.65, 0)
    pos["R_OEN"]        = (209.55, 125.73, 0)
    pos["R_POR"]        = (209.55, 129.79, 0)
    pos["C_POR"]        = (210.82, 133.86, 0)
    pos["R_LED2"]       = (209.55, 137.92, 0)
    pos["D2"]           = (212.09, 142.24, 0)

    # 3-Channel Solid-State Matrix: U1, U2, U3 (DG408DJ) stacked cleanly at X = 223.52 .. 248.92 mm
    for idx, ch in enumerate((1, 2, 3)):
        uy = 49.53 + idx * 33.02  # Y = 49.53, 82.55, 115.57
        pos[f"U{ch}"]    = (223.52, uy, 0)
        pos[f"C{13+ch}"] = (224.79, uy + 20.32, 0)
        pos[f"R_EN{ch}"] = (223.52, uy + 24.38, 0)
        pos[f"R_PD{ch}"] = (223.52, uy + 28.45, 0)

        # Right side of U{ch} (X = 234.95 and 245.11 mm): Kelvin R_PR, 1N3595 Clamps & 6x 0.1% R.G. Keen Bank
        bank = [
            f"R_PR{ch}", f"D_CH{ch}", f"D_CL{ch}",
            f"R_LA{ch}", f"R_KA{ch}", f"R_MB{ch}",
            f"R_KB{ch}", f"R_LG{ch}", f"R_KG{ch}",
        ]
        for bi, bref in enumerate(bank):
            bx = 234.95 + (bi % 2) * 10.16
            by = uy + (bi // 2) * 4.572
            pos[bref] = (bx, by, 0)

    return pos


# ==============================================================================
# 4. MULTI-TERMINAL 2-LAYER MANHATTAN A* AUTOROUTER (0.508 mm = 20 mil GRID)
# ==============================================================================
class BoardRouter:
    def __init__(self, w, h, grid=0.508, corner_r=10.0):
        self.w = w
        self.h = h
        self.g = grid
        self.corner_r = corner_r
        self.nx = int(round(w / grid)) + 1
        self.ny = int(round(h / grid)) + 1
        self.occ = [[[0] * self.ny for _ in range(self.nx)] for _ in range(2)]
        self.pad_cells = [[[0] * self.ny for _ in range(self.nx)] for _ in range(2)]
        self.net_nodes = defaultdict(set)
        self.segments = []
        self.vias = []
        self._mark_border()

    def to_grid(self, x, y):
        return max(0, min(self.nx - 1, int(round(x / self.g)))), max(0, min(self.ny - 1, int(round(y / self.g))))

    def to_mm(self, ix, iy):
        return ix * self.g, iy * self.g

    def _mark_border(self):
        margin = int(math.ceil(1.6 / self.g))
        for l in (0, 1):
            for ix in range(self.nx):
                for iy in range(margin):
                    self.occ[l][ix][iy] = -1
                    self.occ[l][ix][self.ny - 1 - iy] = -1
            for iy in range(self.ny):
                for ix in range(margin):
                    self.occ[l][ix][iy] = -1
                    self.occ[l][self.nx - 1 - ix][iy] = -1

        # Block the 4 rounded-corner exterior arcs (R = 10.0 mm + 1.8 mm clearance)
        rc = self.corner_r
        r_allow = max(1.0, rc - 1.8)
        corners = [
            (rc, rc, -1, -1),
            (self.w - rc, rc, 1, -1),
            (self.w - rc, self.h - rc, 1, 1),
            (rc, self.h - rc, -1, 1),
        ]
        for ix in range(self.nx):
            for iy in range(self.ny):
                x, y = self.to_mm(ix, iy)
                for cx, cy, sx, sy in corners:
                    if (x - cx) * sx > 0 and (y - cy) * sy > 0:
                        if math.hypot(x - cx, y - cy) >= r_allow:
                            for l in (0, 1):
                                self.occ[l][ix][iy] = -1
                                self.pad_cells[l][ix][iy] = -1

    def mark_keepout_rect(self, x1, y1, x2, y2):
        ix1, iy1 = self.to_grid(x1, y1)
        ix2, iy2 = self.to_grid(x2, y2)
        for l in (0, 1):
            for ix in range(min(ix1, ix2), max(ix1, ix2) + 1):
                for iy in range(min(iy1, iy2), max(iy1, iy2) + 1):
                    self.occ[l][ix][iy] = -1
                    self.pad_cells[l][ix][iy] = -1

    def mark_pad(self, x, y, sx, sy, shape, net_id):
        rad = max(sx, sy) * (0.72 if shape in ("rect", "roundrect") else 0.50)
        clr = rad + 0.44
        cx, cy = self.to_grid(x, y)
        r = int(math.ceil(clr / self.g))
        for ix in range(max(0, cx - r), min(self.nx, cx + r + 1)):
            for iy in range(max(0, cy - r), min(self.ny, cy + r + 1)):
                px, py = self.to_mm(ix, iy)
                if math.hypot(px - x, py - y) <= clr:
                    for l in (0, 1):
                        if self.pad_cells[l][ix][iy] == 0 or self.pad_cells[l][ix][iy] == net_id:
                            self.pad_cells[l][ix][iy] = net_id
                        else:
                            self.pad_cells[l][ix][iy] = -1
                        if self.occ[l][ix][iy] == 0 or self.occ[l][ix][iy] == net_id:
                            self.occ[l][ix][iy] = net_id
                        else:
                            self.occ[l][ix][iy] = -1

    def can_use(self, l, ix, iy, net_id, wide=False):
        o = self.occ[l][ix][iy]
        if o != 0 and o != net_id:
            return False
        if wide:
            if self.pad_cells[l][ix][iy] == net_id:
                return True
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = ix + dx, iy + dy
                if 0 <= nx < self.nx and 0 <= ny < self.ny:
                    no = self.occ[l][nx][ny]
                    if no != 0 and no != net_id:
                        return False
        return True

    def can_via(self, ix, iy, net_id):
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                nx, ny = ix + dx, iy + dy
                if not (0 <= nx < self.nx and 0 <= ny < self.ny):
                    return False
                for l in (0, 1):
                    o = self.occ[l][nx][ny]
                    if o != 0 and o != net_id:
                        return False
                    if self.pad_cells[l][nx][ny] != 0:
                        return False
        return True

    def mark_path(self, path, net_id, width_mm=0.25):
        wide = width_mm > 0.31
        for l, ix, iy in path:
            self.net_nodes[net_id].add((l, ix, iy))
            if self.occ[l][ix][iy] == 0:
                self.occ[l][ix][iy] = net_id
            elif self.occ[l][ix][iy] != net_id:
                self.occ[l][ix][iy] = -1
            if wide:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = ix + dx, iy + dy
                    if 0 <= nx < self.nx and 0 <= ny < self.ny:
                        if self.occ[l][nx][ny] == 0:
                            self.occ[l][nx][ny] = net_id
                        elif self.occ[l][nx][ny] != net_id:
                            self.occ[l][nx][ny] = -1

        i = 0
        while i < len(path) - 1:
            l1, x1, y1 = path[i]
            l2, x2, y2 = path[i + 1]
            if l1 != l2:
                vx, vy = self.to_mm(x1, y1)
                self.vias.append((vx, vy, net_id))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = x1 + dx, y1 + dy
                        if 0 <= nx < self.nx and 0 <= ny < self.ny:
                            for ll in (0, 1):
                                if self.occ[ll][nx][ny] == 0:
                                    self.occ[ll][nx][ny] = net_id
                i += 1
                continue
            dx, dy = x2 - x1, y2 - y1
            j = i + 1
            while j + 1 < len(path):
                nl, nx, ny = path[j + 1]
                pl, px, py = path[j]
                if nl == l1 and (nx - px) == dx and (ny - py) == dy:
                    j += 1
                else:
                    break
            sx, sy = self.to_mm(x1, y1)
            ex, ey = self.to_mm(path[j][1], path[j][2])
            lname = "F.Cu" if l1 == 0 else "B.Cu"
            self.segments.append((lname, sx, sy, ex, ey, width_mm, net_id))
            i = j

    def route_net(self, pad_list_mm, net_id, width_mm=0.25, max_exp=320000, relaxed=False):
        if len(pad_list_mm) <= 1:
            return []
        wide = width_mm > 0.31
        remaining = list(range(1, len(pad_list_mm)))
        connected = [0]
        p0x, p0y = self.to_grid(*pad_list_mm[0])
        self.net_nodes[net_id].add((0, p0x, p0y))
        self.net_nodes[net_id].add((1, p0x, p0y))
        failed = []

        while remaining:
            best = None
            for j in remaining:
                for i in connected:
                    d = math.hypot(pad_list_mm[i][0] - pad_list_mm[j][0], pad_list_mm[i][1] - pad_list_mm[j][1])
                    if best is None or d < best[0]:
                        best = (d, i, j)
            _, _, tgt_idx = best
            remaining.remove(tgt_idx)
            p_mm = pad_list_mm[tgt_idx]
            sx, sy = self.to_grid(*p_mm)

            targets = self.net_nodes[net_id]
            ref_pts = [self.to_grid(*pad_list_mm[i]) for i in connected]

            def heur(x, y):
                return min(abs(x - rx) + abs(y - ry) for rx, ry in ref_pts) * (10 if relaxed else 11)

            pq = []
            gscore = {}
            parent = {}
            for l in (0, 1):
                st = (l, sx, sy)
                gscore[st] = 0
                heapq.heappush(pq, (heur(sx, sy), 0, st))

            dirs = [(1, 0), (-1, 0), (0, 1), (0, -1)]
            exp = 0
            goal = None

            while pq and exp < max_exp:
                _, cost, cur = heapq.heappop(pq)
                if cost > gscore.get(cur, 1e18):
                    continue
                if cur in targets:
                    goal = cur
                    break
                l, x, y = cur
                exp += 1

                for dx, dy in dirs:
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < self.nx and 0 <= ny < self.ny and self.can_use(l, nx, ny, net_id, wide):
                        if relaxed:
                            step = 10
                        elif l == 0:
                            step = 10 if dy == 0 else 12
                        else:
                            step = 14 if dx == 0 else 17
                        p = parent.get(cur)
                        if p and p[0] == l and (x - p[1], y - p[2]) != (dx, dy):
                            step += 2 if relaxed else 3
                        nc = cost + step
                        nxt = (l, nx, ny)
                        if nc < gscore.get(nxt, 1e18):
                            gscore[nxt] = nc
                            parent[nxt] = cur
                            heapq.heappush(pq, (nc + heur(nx, ny), nc, nxt))

                ol = 1 - l
                if self.can_via(x, y, net_id):
                    nc = cost + (22 if relaxed else 48)
                    nxt = (ol, x, y)
                    if nc < gscore.get(nxt, 1e18):
                        gscore[nxt] = nc
                        parent[nxt] = cur
                        heapq.heappush(pq, (nc + heur(x, y), nc, nxt))

            if goal is None:
                failed.append((net_id, p_mm))
                connected.append(tgt_idx)
                self.net_nodes[net_id].add((0, sx, sy))
                self.net_nodes[net_id].add((1, sx, sy))
                continue

            path = []
            cur = goal
            while cur is not None:
                path.append(cur)
                cur = parent.get(cur)
            path.reverse()

            self.mark_path(path, net_id, width_mm)
            l_start = "F.Cu" if path[0][0] == 0 else "B.Cu"
            gx0, gy0 = self.to_mm(path[0][1], path[0][2])
            if math.hypot(p_mm[0] - gx0, p_mm[1] - gy0) > 0.01:
                self.segments.append((l_start, p_mm[0], p_mm[1], gx0, gy0, width_mm, net_id))
            if len(connected) == 1:
                l_end = "F.Cu" if path[-1][0] == 0 else "B.Cu"
                gxe, gye = self.to_mm(path[-1][1], path[-1][2])
                if math.hypot(pad_list_mm[0][0] - gxe, pad_list_mm[0][1] - gye) > 0.01:
                    self.segments.append((l_end, gxe, gye, pad_list_mm[0][0], pad_list_mm[0][1], width_mm, net_id))

            connected.append(tgt_idx)
            self.net_nodes[net_id].add((0, sx, sy))
            self.net_nodes[net_id].add((1, sx, sy))

        return failed


def generate_pcb(comps):
    pos = pcb_placement_rev5()

    sym_pin_names = {}
    sym_txt = open(SYM_PATH).read()
    for blk in re.split(r'\n  \(symbol "', sym_txt)[1:]:
        sname = "Ge_Engine:" + blk.split('"', 1)[0]
        sym_pin_names[sname] = {}
        for m in re.finditer(r'\(pin \w+ line [^\n]*?\(name "([^"]+)"[^\n]*?\(number "(\w+)"', blk):
            sym_pin_names[sname][m.group(2)] = m.group(1)

    net_names = set()
    nc_pad_nets = {}
    for c in comps:
        for pnum, net in c["pins"].items():
            if net:
                net_names.add(net)
            else:
                pname = sym_pin_names[c["sym"]].get(pnum, f"Pad{pnum}")
                nc_name = f"unconnected-({c['ref']}-{pname}-Pad{pnum})"
                nc_pad_nets[(c["ref"], pnum)] = nc_name
                net_names.add(nc_name)
    net_names.discard("DGND")
    net_names.discard("AGND")
    ordered_nets = [""] + ["DGND", "AGND"] + sorted(net_names)
    net_to_id = {n: i for i, n in enumerate(ordered_nets)}

    net_pads = defaultdict(list)
    all_pads_check = []
    fp_cache = {}

    # 4 M3 Mounting Holes inset safely inside the R = 10.0 mm rounded corners!
    mounting_holes = [
        ("H1", 8.0, 8.0), ("H2", BOARD_W - 8.0, 8.0),
        ("H3", 8.0, BOARD_H - 8.0), ("H4", BOARD_W - 8.0, BOARD_H - 8.0),
    ]

    for c in comps:
        px, py, rot = pos[c["ref"]]
        if c["fp"] not in fp_cache:
            fp_cache[c["fp"]] = parse_fp_pads(c["fp"])
        pads, _ = fp_cache[c["fp"]]
        for pnum, ptype, pshape, lx, ly, sx, sy, drill in pads:
            wx, wy = transform_pt(lx, ly, px, py, rot)
            net = c["pins"].get(pnum) if pnum else None
            all_pads_check.append((c["ref"], pnum, wx, wy, max(sx, sy) / 2.0))
            if net:
                net_pads[net].append((wx, wy))

    overlaps = 0
    for i in range(len(all_pads_check)):
        r1, p1, x1, y1, rad1 = all_pads_check[i]
        for href, hx, hy in mounting_holes:
            d = math.hypot(x1 - hx, y1 - hy)
            if d < rad1 + 3.2:
                print(f"  MOUNTING HOLE CLEARANCE: {r1}.{p1} ({x1:.2f},{y1:.2f}) vs {href} ({hx:.2f},{hy:.2f}) dist={d:.2f}")
                overlaps += 1
        for j in range(i + 1, len(all_pads_check)):
            r2, p2, x2, y2, rad2 = all_pads_check[j]
            if r1 == r2:
                continue
            d = math.hypot(x1 - x2, y1 - y2)
            if d < rad1 + rad2 + 0.30:
                print(f"  PAD OVERLAP: {r1}.{p1} ({x1:.2f},{y1:.2f}) vs {r2}.{p2} ({x2:.2f},{y2:.2f}) dist={d:.2f}")
                overlaps += 1
    print(f"Pad & Mounting-Hole clearance check: {overlaps} collisions")

    wide_power_nets = {
        "V_USB_RAW", "V_USB_PROT", "J_BAT+", "V_BAT_SW", "V_BAT_PROT",
        "PMID_BYP", "V_SYS_RAW", "V_BUS_SYS", "V_ANA_IN",
        "V_BOOST_RAW", "V_LC1_MID", "V_14V_FILT", "V_12V_PRE",
        "V_LC2_MID", "V_3V3_FILT",
    }
    med_power_nets = {
        "+3V3_DIG", "M2_3V3", "+3V3_ANA", "M1_3V3", "+3V3_ADC",
        "+12V_A", "+9V_REF", "LIM_SENSE", "+9V_LIM", "V_SRC_A", "V_SRC_B",
    }

    def net_span(pts):
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))

    nets_to_route = [n for n in net_pads.keys() if n not in ("DGND", "AGND") and len(net_pads[n]) > 1]

    def route_prio(n):
        if len(net_pads[n]) == 2 and net_span(net_pads[n]) < 28.0:
            return (0, net_span(net_pads[n]))
        if n.startswith(("TP", "C1_S", "C2_S", "C3_S", "U1_", "U2_", "U3_", "SW_G", "FAST_TP", "CAL_A", "LINK_")):
            return (1, net_span(net_pads[n]))
        if len(net_pads[n]) <= 4:
            return (2, net_span(net_pads[n]))
        if n in wide_power_nets:
            return (3, net_span(net_pads[n]))
        if n in med_power_nets:
            return (4, net_span(net_pads[n]))
        return (5, net_span(net_pads[n]))

    nets_to_route.sort(key=route_prio)
    total_conns = sum(len(net_pads[n]) - 1 for n in nets_to_route)

    # Strict RF Antenna Keepout Zones under BOTH U_MCU1 (WEMOS LOLIN32 Lite) and U_MCU2 (ESP32-C3 SuperMini)
    # U_MCU1 at (162.56, 104.14): meander antenna is at Y = 96.5..101.8, X = 165.0..182.0
    ANT_KEEP_MCU1 = (165.0, 96.5, 182.0, 101.8)
    # U_MCU2 at (116.84, 19.05): red ceramic C3 antenna is at Y = 38.2..41.2, X = 118.5..130.5
    ANT_KEEP_MCU2 = (118.5, 38.2, 130.5, 41.2)

    best_router = None
    best_failed = None
    order = list(nets_to_route)
    boosted = []
    for attempt in range(8):
        r_try = BoardRouter(BOARD_W, BOARD_H, grid=0.508, corner_r=CORNER_R)
        r_try.mark_keepout_rect(*ANT_KEEP_MCU1)
        r_try.mark_keepout_rect(*ANT_KEEP_MCU2)
        for _, hx, hy in mounting_holes:
            r_try.mark_pad(hx, hy, 5.8, 5.8, "circle", -1)
        for c in comps:
            px, py, rot = pos[c["ref"]]
            pads, _ = fp_cache[c["fp"]]
            for pnum, ptype, pshape, lx, ly, sx, sy, drill in pads:
                wx, wy = transform_pt(lx, ly, px, py, rot)
                net = c["pins"].get(pnum) if pnum else None
                nid = net_to_id[net] if net else -1
                r_try.mark_pad(wx, wy, sx, sy, pshape, nid)

        failed_list = []
        failed_nets = []
        for net in order:
            nid = net_to_id[net]
            if net in wide_power_nets:
                w = 0.45 if attempt < 2 else 0.30
            elif net in med_power_nets:
                w = 0.30
            else:
                w = 0.25
            pts = []
            for p in net_pads[net]:
                if all(math.hypot(p[0] - q[0], p[1] - q[1]) > 0.1 for q in pts):
                    pts.append(p)
            f = r_try.route_net(pts, nid, width_mm=w, max_exp=480000 if attempt >= 2 else 300000, relaxed=(attempt >= 2))
            if f:
                failed_nets.append(net)
                for _, pm in f:
                    failed_list.append((net, pm))

        print(f"  Attempt {attempt + 1}: Routed {total_conns - len(failed_list)}/{total_conns} (failed: {len(failed_list)})")
        if best_failed is None or len(failed_list) < len(best_failed):
            best_router = r_try
            best_failed = failed_list
        if not failed_list:
            break
        for fn in failed_nets:
            if fn not in boosted:
                boosted.insert(0, fn)
        order = [n for n in boosted if n in nets_to_route] + [n for n in nets_to_route if n not in boosted]

    router = best_router
    all_failed = best_failed
    print(f"Final Non-GND Routing: {total_conns - len(all_failed)}/{total_conns} (failed: {len(all_failed)})")
    for f in all_failed:
        print("  FAILED:", f)

    # Route DGND and AGND copper backbones independently within their respective domains!
    for gname in ("DGND", "AGND"):
        g_pts = []
        for p in net_pads[gname]:
            if all(math.hypot(p[0] - q[0], p[1] - q[1]) > 0.1 for q in g_pts):
                g_pts.append(p)
        g_failed = router.route_net(g_pts, net_to_id[gname], width_mm=0.25, max_exp=500000, relaxed=True)
        print(f"{gname} Backbone Routing: {len(g_pts) - 1 - len(g_failed)}/{len(g_pts) - 1} (failed: {len(g_failed)})")

    out = [f"""(kicad_pcb (version 20240108) (generator "ge_engine_gen")
  (general
    (thickness 1.6)
    (legacy_teardrops no)
  )
  (paper "A3")
  (title_block
    (title "KAOTI GERMANIUM PARAMETRIC CHARACTERIZATION ENGINE — REV 5.0 DUAL-MCU")
    (date "2026-11-21")
    (rev "REV 5.0")
    (company "KAOTI AUDIO")
  )
  (layers
    (0 "F.Cu" signal)
    (31 "B.Cu" signal)
    (36 "B.SilkS" user "B.Silkscreen")
    (37 "F.SilkS" user "F.Silkscreen")
    (38 "B.Mask" user)
    (39 "F.Mask" user)
    (44 "Edge.Cuts" user)
    (46 "B.CrtYd" user "B.Courtyard")
    (47 "F.CrtYd" user "F.Courtyard")
    (48 "B.Fab" user)
    (49 "F.Fab" user)
  )
  (setup
    (pad_to_mask_clearance 0.05)
    (pcbplotparams
      (layerselection 0x00010fc_ffffffff)
      (plot_on_all_layers_selection 0x0000000_00000000)
      (outputformat 1)
    )
  )
"""]
    for i, n in enumerate(ordered_nets):
        out.append(f'  (net {i} "{n}")\n')

    # Rounded-Corner Board Outline on Edge.Cuts (4x gr_line + 4x gr_arc with R = 10.0 mm)
    R = CORNER_R
    W = BOARD_W
    H = BOARD_H
    k = R * (1.0 - 1.0 / math.sqrt(2.0))  # 2.928932 mm arc midpoint inset
    edges = [
        f'  (gr_line (start {R:.3f} 0.000) (end {W - R:.3f} 0.000) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
        f'  (gr_line (start {W:.3f} {R:.3f}) (end {W:.3f} {H - R:.3f}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
        f'  (gr_line (start {W - R:.3f} {H:.3f}) (end {R:.3f} {H:.3f}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
        f'  (gr_line (start 0.000 {H - R:.3f}) (end 0.000 {R:.3f}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
        f'  (gr_arc (start 0.000 {R:.3f}) (mid {k:.4f} {k:.4f}) (end {R:.3f} 0.000) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
        f'  (gr_arc (start {W - R:.3f} 0.000) (mid {W - k:.4f} {k:.4f}) (end {W:.3f} {R:.3f}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
        f'  (gr_arc (start {W:.3f} {H - R:.3f}) (mid {W - k:.4f} {H - k:.4f}) (end {W - R:.3f} {H:.3f}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
        f'  (gr_arc (start {R:.3f} {H:.3f}) (mid {k:.4f} {H - k:.4f}) (end 0.000 {H - R:.3f}) (stroke (width 0.15) (type solid)) (layer "Edge.Cuts") (uuid "{uid()}"))\n',
    ]
    out.extend(edges)

    # Galvanic Isolation Moat Line & Front-Panel Instrument Silkscreen
    out.append(f'  (gr_line (start 152.40 6.00) (end 152.40 149.00) (stroke (width 0.25) (type dash)) (layer "F.SilkS") (uuid "{uid()}"))\n')
    silk_labels = [
        (78.74, 3.2,  "KAOTI AUDIO — REV 5.0 DUAL-MCU GERMANIUM PARAMETRIC ENGINE", 1.15),
        (204.0, 3.2,  "QUIET ANALOG CORE (AGND ISLAND + 2x DAMPED LC PI-FILTERS)", 1.05),
        (152.4, 151.8, "GALVANIC MOAT (>=2.0mm) | R_STAR SINGLE-POINT GROUND", 0.85),
        (78.74, 148.5, "SYMMETRICAL DUT TEST BAY: 5P PROBE | ZIF-14 | 3P GOLD | KELVIN | DS18B20", 0.85),
    ]
    for lx, ly, txt, sz in silk_labels:
        out.append(f'  (gr_text "{txt}" (at {lx:.2f} {ly:.2f}) (layer "F.SilkS") (uuid "{uid()}") (effects (font (size {sz:.2f} {sz:.2f}) (thickness 0.15))))\n')

    def embed_footprint(ref, val, fp_name, x, y, rot, pin_nets):
        _, raw = fp_cache[fp_name] if fp_name in fp_cache else parse_fp_pads(fp_name)
        body = raw.strip()
        first_nl = body.find("\n")
        inner = body[first_nl + 1:body.rfind(")")]
        inner = inner.replace('"REF**"', f'"{ref}"', 1)
        fp_short = fp_name.split(":")[-1]
        inner = inner.replace(f'(fp_text value "{fp_short}"', f'(fp_text value "{val}"', 1)

        def add_net_to_pad(m):
            full = m.group(0)
            pnum = m.group(1)
            if not pnum:
                return full
            net = pin_nets.get(pnum)
            if not net:
                net = nc_pad_nets.get((ref, pnum))
            if net:
                nid = net_to_id[net]
                return full[:-1] + f' (net {nid} "{net}"))'
            return full

        inner = re.sub(r'\(pad "([^"]*)" [^\n]+\)', add_net_to_pad, inner)
        return f'  (footprint "{fp_name}" (layer "F.Cu") (uuid "{uid()}")\n    (at {x:.2f} {y:.2f} {rot})\n{inner}\n  )\n'

    for c in comps:
        px, py, rot = pos[c["ref"]]
        out.append(embed_footprint(c["ref"], c["val"], c["fp"], px, py, rot, c["pins"]))

    for href, hx, hy in mounting_holes:
        out.append(embed_footprint(href, "M3", "Ge_Engine:MountingHole_M3", hx, hy, 0, {}))

    for lname, x1, y1, x2, y2, w, nid in router.segments:
        out.append(f'  (segment (start {x1:.3f} {y1:.3f}) (end {x2:.3f} {y2:.3f}) (width {w:.2f}) (layer "{lname}") (net {nid}) (uuid "{uid()}"))\n')

    for vx, vy, nid in router.vias:
        out.append(f'  (via (at {vx:.3f} {vy:.3f}) (size 0.7) (drill 0.35) (layers "F.Cu" "B.Cu") (net {nid}) (uuid "{uid()}"))\n')

    # Partitioned B.Cu Ground Planes with 2.40 mm Isolation Moat (DGND: X = 1.5..151.20; AGND: X = 153.60..258.50)
    dgnd_id = net_to_id["DGND"]
    agnd_id = net_to_id["AGND"]
    out.append(f"""  (zone (net {dgnd_id}) (net_name "DGND") (layer "B.Cu") (uuid "{uid()}") (hatch edge 0.5)
    (connect_pads (clearance 0.25))
    (min_thickness 0.20)
    (filled_areas_thickness no)
    (fill yes (thermal_gap 0.28) (thermal_bridge_width 0.30))
    (polygon
      (pts
        (xy 10.50 1.50) (xy 151.20 1.50) (xy 151.20 {H - 1.50:.2f})
        (xy 10.50 {H - 1.50:.2f}) (xy 1.50 {H - 10.50:.2f}) (xy 1.50 10.50)
      )
    )
  )
  (zone (net {agnd_id}) (net_name "AGND") (layer "B.Cu") (uuid "{uid()}") (hatch edge 0.5)
    (connect_pads (clearance 0.25))
    (min_thickness 0.20)
    (filled_areas_thickness no)
    (fill yes (thermal_gap 0.28) (thermal_bridge_width 0.30))
    (polygon
      (pts
        (xy 153.60 1.50) (xy {W - 10.50:.2f} 1.50) (xy {W - 1.50:.2f} 10.50)
        (xy {W - 1.50:.2f} {H - 10.50:.2f}) (xy {W - 10.50:.2f} {H - 1.50:.2f}) (xy 153.60 {H - 1.50:.2f})
      )
    )
  )
""")

    # Strict RF Antenna Keepouts for BOTH U_MCU1 and U_MCU2 (blocks tracks, vias, AND copper pours!)
    for ax1, ay1, ax2, ay2 in (ANT_KEEP_MCU1, ANT_KEEP_MCU2):
        out.append(f"""  (zone (net 0) (net_name "") (layers "F.Cu" "B.Cu") (uuid "{uid()}") (hatch edge 0.5)
    (keepout (tracks not_allowed) (vias not_allowed) (pads allowed) (copperpour not_allowed) (footprints allowed))
    (polygon
      (pts
        (xy {ax1:.2f} {ay1:.2f}) (xy {ax2:.2f} {ay1:.2f})
        (xy {ax2:.2f} {ay2:.2f}) (xy {ax1:.2f} {ay2:.2f})
      )
    )
  )
""")

    out.append(")\n")
    pcb_path = os.path.join(PROJ_DIR, "Ge_Parametric_Engine.kicad_pcb")
    with open(pcb_path, "w") as f:
        f.write("".join(out))
    print("Generated Rev 5.0 PCB:", pcb_path)
    return all_failed


if __name__ == "__main__":
    comps = build_netlist()
    print(f"Total Rev 5.0 components: {len(comps)}")
    generate_schematic(comps)
    generate_pcb(comps)
