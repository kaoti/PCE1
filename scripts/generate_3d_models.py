#!/usr/bin/env python3
"""
Generate detailed VRML 2.0 (.wrl) 3D models for the socketed modules and ZIF socket
in the Ge_Parametric_Engine KiCad 9 PCB project.

Coordinate convention in KiCad VRML:
- 1 VRML unit = 2.54 mm (1/10 inch) when KiCad loads standard .wrl files!
  WAIT: Let's verify KiCad 9 VRML scale factor! KiCad scales VRML by 2.54x (1 unit = 0.1 inch = 2.54mm),
  whereas STEP is 1 unit = 1 mm. So writing coordinates in inches/0.1 (i.e. mm / 2.54)
  matches KiCad's built-in .wrl models identically!
"""
import os
import math

OUT_DIR = "/Users/kaoti/Desktop/PCE1/3dmodels"
os.makedirs(OUT_DIR, exist_ok=True)

U = 1.0 / 2.54  # convert mm to KiCad VRML units (2.54 mm per unit)

HEADER = "#VRML V2.0 utf8\n# Generated for KAOTI Ge_Parametric_Engine\n\n"

def box(cx, cy, cz, sx, sy, sz, rgb, shininess=0.3, transparency=0.0):
    r, g, b = rgb
    return f"""Transform {{
  translation {cx*U:.4f} {cy*U:.4f} {cz*U:.4f}
  children [
    Shape {{
      appearance Appearance {{
        material Material {{
          diffuseColor {r:.3f} {g:.3f} {b:.3f}
          specularColor 0.25 0.25 0.25
          shininess {shininess:.2f}
          transparency {transparency:.2f}
        }}
      }}
      geometry Box {{ size {sx*U:.4f} {sy*U:.4f} {sz*U:.4f} }}
    }}
  ]
}}
"""

def oct_z(cx, cy, cz, radius, height, rgb, shininess=0.6):
    """Box-based octagonal stud aligned along Z (KiCad VRML raytracer drops Cylinder primitives)."""
    w = 1.8478 * radius
    t = 0.7654 * radius
    return (
        box(cx, cy, cz, w, t, height, rgb, shininess) +
        box(cx, cy, cz, t, w, height, rgb, shininess)
    )


def bar_x(cx, cy, cz, length, thickness, rgb, shininess=0.6):
    """Box-based rectangular bar aligned along X axis."""
    return box(cx, cy, cz, length, thickness, thickness, rgb, shininess)


def bar_y(cx, cy, cz, length, thickness, rgb, shininess=0.6):
    """Box-based rectangular bar aligned along Y axis."""
    return box(cx, cy, cz, thickness, length, thickness, rgb, shininess)

# Colors
PCB_BLACK  = (0.08, 0.08, 0.09)
PCB_PURPLE = (0.28, 0.10, 0.42)
PCB_BLUE   = (0.06, 0.22, 0.52)
ZIF_TEAL   = (0.06, 0.48, 0.52)
IC_DARK    = (0.14, 0.14, 0.15)
METAL_SILV = (0.78, 0.80, 0.82)
GOLD       = (0.85, 0.68, 0.20)
BRASS      = (0.82, 0.66, 0.18)
WHITE      = (0.92, 0.91, 0.88)
PLASTIC_BK = (0.12, 0.12, 0.13)
OLED_GLASS = (0.03, 0.04, 0.06)
OLED_CYAN  = (0.05, 0.72, 0.92)
OLED_YELL  = (0.95, 0.78, 0.10)
TRIM_BLUE  = (0.08, 0.34, 0.72)


def make_wemos_lolin32_lite():
    """
    WEMOS LOLIN32 Lite V1.0.0 seated atop two 1x13 female pin sockets.
    Footprint origin (0,0) = Pin 1 (VP, top-left header pin).
    In KiCad footprint coords: +X = right, +Y = down on PCB; in VRML: +X = right, +Y = UP (-Y_kicad)!
    Wait: KiCad maps VRML +X -> +X, VRML +Y -> -Y_kicad, VRML +Z -> +Z_up!
    Let's place Pin 1 (VP) at (0, 0) in footprint, so pins 1..13 go along Y_kicad = 0 .. +30.48 mm
    which in VRML is y = 0 .. -30.48 mm!
    Right header pins 14..26 are at x = +22.86 mm, y = 0 .. -30.48 mm!
    """
    parts = [HEADER]
    # Center of header array in VRML coords:
    cx = 11.43
    cy = -15.24  # mid between pin 1 (0) and pin 13 (-30.48)
    # Board sits above female sockets (socket height ~8.5mm, male header plastic 2.5mm -> PCB bottom at z=11.0mm)
    z_pcb = 11.0
    pcb_t = 1.6
    # Board extends ~8.5mm above pin 1 (antenna end: y=+8.5) and ~10.5mm below pin 13 (USB-C/JST end: y=-41.0)
    # Total length = 49.5mm, centered at y = -16.25mm, width = 25.4mm centered at x = 11.43mm
    by = -16.25
    parts.append(box(cx, by, z_pcb + pcb_t/2, 25.4, 49.5, pcb_t, PCB_BLACK, 0.25))

    # Gold meander PCB Wi-Fi antenna at top (+y end, y around +3 to +8)
    for i in range(4):
        ax = 7.5 + i * 3.0
        parts.append(box(ax, 5.2, z_pcb + pcb_t + 0.05, 0.7, 5.0, 0.1, GOLD, 0.7))
        if i < 3:
            ty = 7.5 if i % 2 == 0 else 2.9
            parts.append(box(ax + 1.5, ty, z_pcb + pcb_t + 0.05, 3.7, 0.7, 0.1, GOLD, 0.7))

    # ESP32 bare QFN-48 chip in upper center
    parts.append(box(cx, -8.5, z_pcb + pcb_t + 0.45, 6.0, 6.0, 0.9, IC_DARK, 0.4))
    # Flash SOIC-8 chip below ESP32
    parts.append(box(cx, -16.5, z_pcb + pcb_t + 0.85, 5.2, 4.0, 1.7, IC_DARK, 0.3))
    # CH340C USB-UART SOIC-16 on right lower side
    parts.append(box(17.2, -25.5, z_pcb + pcb_t + 0.85, 4.0, 9.8, 1.7, IC_DARK, 0.3))
    # 40MHz Crystal silver can
    parts.append(box(17.5, -11.5, z_pcb + pcb_t + 0.4, 2.5, 3.2, 0.8, METAL_SILV, 0.7))

    # USB-C connector at bottom-right (y around -37.5, overhanging slightly to -41.5)
    parts.append(box(17.2, -37.8, z_pcb + pcb_t + 1.6, 9.0, 7.6, 3.2, METAL_SILV, 0.8))
    # White JST-PH 2-pin battery connector at bottom-left
    parts.append(box(4.5, -37.2, z_pcb + pcb_t + 2.25, 7.5, 6.0, 4.5, WHITE, 0.2))
    # Reset tactile switch at bottom center
    parts.append(box(10.5, -37.5, z_pcb + pcb_t + 0.75, 3.0, 4.0, 1.5, METAL_SILV, 0.5))
    parts.append(oct_z(10.5, -37.5, z_pcb + pcb_t + 1.8, 0.95, 0.9, PLASTIC_BK, 0.3))

    # Male header shrouds + gold pins going down into the female sockets!
    for hx in (0.0, 22.86):
        parts.append(box(hx, cy, z_pcb - 1.25, 2.54, 13 * 2.54, 2.5, PLASTIC_BK, 0.2))
        for p in range(13):
            py = -p * 2.54
            parts.append(box(hx, py, z_pcb - 4.0, 0.64, 0.64, 6.0, GOLD, 0.8))
            # Top solder fillet & crisp pin tip
            parts.append(oct_z(hx, py, z_pcb + pcb_t + 0.3, 0.75, 0.6, METAL_SILV, 0.7))
            parts.append(box(hx, py, z_pcb + pcb_t + 0.55, 0.64, 0.64, 1.1, GOLD, 0.8))

    with open(os.path.join(OUT_DIR, "Wemos_Lolin32_Lite.wrl"), "w") as f:
        f.write("".join(parts))


def make_ads1115_breakout():
    """
    Standard 10-pin purple ADS1115 breakout module seated atop 1x10 female socket.
    Pin 1 (VDD) at (0,0), Pin 10 (A3) at (0, -22.86) in VRML (+Y_kicad = 22.86).
    Wait: let's orient 1x10 horizontally along X so pins 1..10 go from x=0 to x=22.86, y=0!
    Board body extends in +Y_vrml (i.e. -Y_kicad, upwards) by 17mm.
    """
    parts = [HEADER]
    cx = 4.5 * 2.54  # 11.43 mm
    z_pcb = 11.0
    pcb_t = 1.6
    # PCB 28mm x 17mm, pins along bottom edge y=0, body extends y=-2.0 to y=+15.0 (center y=+6.5)
    parts.append(box(cx, 6.5, z_pcb + pcb_t/2, 28.0, 17.0, pcb_t, PCB_PURPLE, 0.3))
    # ADS1115 MSOP-10 IC in center
    parts.append(box(cx, 7.5, z_pcb + pcb_t + 0.5, 3.0, 3.0, 1.0, IC_DARK, 0.4))
    # Small SMD passives
    for sx in (6.5, 8.5, 14.5, 16.5):
        parts.append(box(sx, 7.5, z_pcb + pcb_t + 0.3, 1.2, 2.0, 0.6, (0.55, 0.42, 0.25), 0.3))
    # Corner mounting hole rings (gold)
    for mx in (cx - 11.0, cx + 11.0):
        parts.append(oct_z(mx, 12.2, z_pcb + pcb_t + 0.05, 1.8, 0.1, GOLD, 0.8))
    # Male 1x10 header down into socket
    parts.append(box(cx, 0.0, z_pcb - 1.25, 10 * 2.54, 2.54, 2.5, PLASTIC_BK, 0.2))
    for p in range(10):
        px = p * 2.54
        parts.append(box(px, 0.0, z_pcb - 4.0, 0.64, 0.64, 6.0, GOLD, 0.8))
        parts.append(oct_z(px, 0.0, z_pcb + pcb_t + 0.3, 0.75, 0.6, METAL_SILV, 0.7))
        parts.append(box(px, 0.0, z_pcb + pcb_t + 0.55, 0.64, 0.64, 1.1, GOLD, 0.8))

    with open(os.path.join(OUT_DIR, "ADS1115_Breakout.wrl"), "w") as f:
        f.write("".join(parts))


def make_mt3608_boost():
    """
    MT3608 DC-DC Boost Converter module (36 x 17 mm) seated on 4 corner machined sockets.
    Pin 1 (IN+) at (0, 0), Pin 2 (IN-) at (0, -12.70 VRML / +12.70 KiCad),
    Pin 3 (OUT+) at (33.02, 0), Pin 4 (OUT-) at (33.02, -12.70 VRML).
    """
    parts = [HEADER]
    cx = 16.51
    cy = -6.35
    z_pcb = 11.0
    pcb_t = 1.6
    parts.append(box(cx, cy, z_pcb + pcb_t/2, 36.5, 17.0, pcb_t, PCB_BLUE, 0.3))
    # 22uH SMD shielded power inductor on left
    parts.append(box(10.5, cy, z_pcb + pcb_t + 2.2, 7.2, 7.2, 4.4, (0.20, 0.20, 0.22), 0.35))
    # Blue multi-turn Bourns 3296W potentiometer on right-top
    parts.append(box(23.5, cy + 2.2, z_pcb + pcb_t + 4.8, 9.5, 4.8, 9.6, TRIM_BLUE, 0.25))
    # Brass adjustment screw on top of trimpot (box-based octagonal stud + screw slot)
    parts.append(oct_z(27.0, cy + 2.2, z_pcb + pcb_t + 10.1, 1.25, 1.0, BRASS, 0.85))
    parts.append(box(27.0, cy + 2.2, z_pcb + pcb_t + 10.62, 2.0, 0.32, 0.12, (0.30, 0.22, 0.06), 0.2))
    # SS34 Schottky diode
    parts.append(box(21.0, cy - 4.2, z_pcb + pcb_t + 1.1, 4.5, 2.6, 2.2, IC_DARK, 0.3))
    # MT3608 SOT-23-6
    parts.append(box(15.8, cy - 4.2, z_pcb + pcb_t + 0.55, 2.9, 1.6, 1.1, IC_DARK, 0.3))
    # 4 corner pins plugging down into sockets
    for px in (0.0, 33.02):
        for py in (0.0, -12.70):
            parts.append(box(px, py, z_pcb - 3.5, 0.85, 0.85, 7.5, GOLD, 0.8))
            parts.append(oct_z(px, py, z_pcb + pcb_t + 0.3, 0.9, 0.6, METAL_SILV, 0.7))

    with open(os.path.join(OUT_DIR, "MT3608_Boost.wrl"), "w") as f:
        f.write("".join(parts))


def make_oled_096():
    """
    0.96" SSD1306 I2C OLED display module (27.3 x 27.3 mm) seated atop 1x04 female socket.
    Pins 1..4 (GND, VCC, SCL, SDA) along top edge: x = 0, 2.54, 5.08, 7.62, y = 0.
    Display body extends downwards in KiCad (+Y_kicad -> -Y_vrml).
    """
    parts = [HEADER]
    cx = 3.81  # midpoint of 4 pins (0..7.62)
    z_pcb = 11.0
    pcb_t = 1.6
    # Carrier PCB 27.3 x 27.3 mm centered at (3.81, -11.5)
    by = -11.5
    parts.append(box(cx, by, z_pcb + pcb_t/2, 27.3, 27.3, pcb_t, PCB_BLUE, 0.3))
    # Black glass OLED panel on top
    parts.append(box(cx, by - 1.2, z_pcb + pcb_t + 0.9, 26.5, 19.0, 1.8, OLED_GLASS, 0.85))
    # Active screen area (dual-color yellow top bar + cyan main region!)
    parts.append(box(cx, by + 3.6, z_pcb + pcb_t + 1.83, 22.0, 3.2, 0.08, OLED_YELL, 0.4))
    parts.append(box(cx, by - 2.4, z_pcb + pcb_t + 1.83, 22.0, 8.2, 0.08, OLED_CYAN, 0.4))
    # 4 mounting hole gold pads
    for mx in (cx - 11.2, cx + 11.2):
        for my in (by - 11.2, by + 11.2):
            parts.append(oct_z(mx, my, z_pcb + pcb_t + 0.05, 1.4, 0.1, GOLD, 0.8))
    # Male 1x04 header down into socket
    parts.append(box(cx, 0.0, z_pcb - 1.25, 4 * 2.54, 2.54, 2.5, PLASTIC_BK, 0.2))
    for p in range(4):
        px = p * 2.54
        parts.append(box(px, 0.0, z_pcb - 4.0, 0.64, 0.64, 6.0, GOLD, 0.8))
        parts.append(oct_z(px, 0.0, z_pcb + pcb_t + 0.3, 0.75, 0.6, METAL_SILV, 0.7))
        parts.append(box(px, 0.0, z_pcb + pcb_t + 0.55, 0.64, 0.64, 1.1, GOLD, 0.8))

    with open(os.path.join(OUT_DIR, "OLED_096_SSD1306.wrl"), "w") as f:
        f.write("".join(parts))


def make_zif_14():
    """
    14-Pin 3M/Aries-style Zero-Insertion-Force (ZIF) DIP Test Socket (7.62mm row spacing).
    Pin 1 at (0,0), Pins 1..7 at x=0, y_kicad=0..15.24 (y_vrml=0..-15.24).
    Pins 8..14 at x=7.62, y_kicad=15.24..0 (y_vrml=-15.24..0).
    """
    parts = [HEADER]
    cx = 3.81
    cy = -7.62
    # Main teal ZIF housing body (15.0mm wide x 33.0mm long x 11.5mm tall, overhanging slightly on lever end)
    parts.append(box(cx, cy + 1.5, 5.75, 15.0, 32.0, 11.5, ZIF_TEAL, 0.3))
    # Top recess plate
    parts.append(box(cx, cy, 11.6, 12.2, 22.0, 0.4, (0.04, 0.38, 0.42), 0.2))
    # 14 dark contact slots + gold wipers
    for p in range(7):
        py = -p * 2.54
        for sx in (0.0, 7.62):
            parts.append(box(sx, py, 11.85, 1.5, 1.2, 0.3, PLASTIC_BK, 0.1))
            parts.append(box(sx, py, -1.5, 0.55, 0.4, 3.2, METAL_SILV, 0.7))
    # Chrome locking lever pivot at +Y end (y = +7.5) extending clearly to the right (+X)
    parts.append(bar_x(cx + 8.5, 7.5, 8.5, 12.0, 2.4, METAL_SILV, 0.85))
    parts.append(box(cx + 13.3, 7.5, 8.5, 2.8, 2.8, 2.8, METAL_SILV, 0.85))
    # Lever arm pointing back along -Y on the right side
    parts.append(bar_y(cx + 13.0, -3.5, 8.5, 22.0, 2.2, METAL_SILV, 0.85))
    # Black ergonomic tip grip on lever handle
    parts.append(box(cx + 13.0, -14.5, 8.5, 3.8, 7.5, 3.8, PLASTIC_BK, 0.3))

    with open(os.path.join(OUT_DIR, "ZIF_Socket_14Pin.wrl"), "w") as f:
        f.write("".join(parts))


def make_tft_1_9(filename, mode):
    """
    Estardyn 1.9" IPS 170x320 ST7789 Display Module (62.0 x 29.0 mm landscape)
    seated atop a 1x08 female socket (z_pcb = 11.0 mm, pcb_t = 1.6 mm).
    Origin (0,0) = Pin 1 (GND) of left-edge 1x08 vertical header.
    Pins 1..8 run along +Y_kicad -> y_vrml = 0 .. -17.78 mm at x = 0.
    """
    parts = [HEADER]
    z_pcb = 11.0
    pcb_t = 1.6
    cx = 29.0     # x from -2.0 to +60.0 -> width 62.0 mm
    cy = -8.89    # midpoint of pins 0..-17.78 -> 29.0 mm tall (-23.39 to +5.61)

    # Blue carrier PCB (62.0 x 29.0 x 1.6 mm)
    parts.append(box(cx, cy, z_pcb + pcb_t / 2, 62.0, 29.0, pcb_t, PCB_BLUE, 0.30))

    # 4 corner mounting hole rings (gold, clear of bezel x=5..57)
    for mx in (1.5, 57.5):
        for my in (cy - 12.2, cy + 12.2):
            parts.append(oct_z(mx, my, z_pcb + pcb_t + 0.05, 1.3, 0.1, GOLD, 0.8))

    # Male 1x08 header plastic shroud + gold pins dropping into female socket
    parts.append(box(0.0, cy, z_pcb - 1.25, 2.54, 8 * 2.54, 2.5, PLASTIC_BK, 0.2))
    for p in range(8):
        py = -p * 2.54
        parts.append(box(0.0, py, z_pcb - 4.0, 0.64, 0.64, 6.0, GOLD, 0.8))
        parts.append(oct_z(0.0, py, z_pcb + pcb_t + 0.3, 0.75, 0.6, METAL_SILV, 0.7))
        parts.append(box(0.0, py, z_pcb + pcb_t + 0.55, 0.64, 0.64, 1.1, GOLD, 0.8))

    # Glossy black glass display bezel (52.0 x 25.5 x 1.8 mm) centered at (31.0, -8.89)
    bx = 31.0
    by = -8.89
    z_glass_top = z_pcb + pcb_t + 1.8
    parts.append(box(bx, by, z_pcb + pcb_t + 0.9, 52.0, 25.5, 1.8, OLED_GLASS, 0.85))

    z_scr = z_glass_top + 0.04
    if mode == "curve":
        # Dark navy oscilloscope screen (47.0 x 22.5 mm)
        parts.append(box(bx, by, z_scr, 47.0, 22.5, 0.08, (0.04, 0.07, 0.18), 0.35))
        # Blue right-hand parameter sidebar
        parts.append(box(bx + 18.0, by, z_scr + 0.03, 9.5, 21.5, 0.05, (0.07, 0.22, 0.48), 0.3))
        for sy_off, col in [(7.5, OLED_CYAN), (3.5, OLED_YELL), (-0.5, WHITE), (-4.5, (0.20, 0.90, 0.35)), (-8.5, OLED_CYAN)]:
            parts.append(box(bx + 18.0, by + sy_off, z_scr + 0.06, 7.0, 1.4, 0.04, col, 0.4))

        # Faint blue oscilloscope grid bars in main plot area (x = bx - 22.5 .. bx + 12.5)
        grid_col = (0.10, 0.24, 0.52)
        for gx in (-19.0, -12.0, -5.0, 2.0, 9.0):
            parts.append(box(bx + gx, by, z_scr + 0.03, 0.22, 21.0, 0.04, grid_col, 0.2))
        for gy in (-8.0, -4.0, 0.0, 4.0, 8.0):
            parts.append(box(bx - 5.0, by + gy, z_scr + 0.03, 34.0, 0.22, 0.04, grid_col, 0.2))

        # 5 stepped colorful transistor curve traces (red, cyan, white, green, yellow)
        traces = [
            ((0.92, 0.20, 0.20), -8.5, -5.2),
            ((0.05, 0.78, 0.95), -8.5, -2.0),
            ((0.95, 0.95, 0.95), -8.5,  1.2),
            ((0.18, 0.88, 0.32), -8.5,  4.4),
            ((0.96, 0.82, 0.12), -8.5,  7.6),
        ]
        for idx, (col, y0, y_plat) in enumerate(traces):
            x_knee = bx - 18.5 + idx * 1.4
            x_end = bx + 11.5
            # Rising elbow segment from origin (-21.0, y0)
            parts.append(box(bx - 19.5 + idx * 0.6, (y0 + y_plat) / 2 + by, z_scr + 0.07,
                             2.2, max(1.2, abs(y_plat - y0)), 0.05, col, 0.5))
            # Horizontal saturation plateau
            w_plat = x_end - x_knee
            parts.append(box(x_knee + w_plat / 2, by + y_plat, z_scr + 0.07,
                             w_plat, 0.65, 0.05, col, 0.5))
    else:
        # Dark slate multimeter screen (47.0 x 22.5 mm)
        parts.append(box(bx, by, z_scr, 47.0, 22.5, 0.08, (0.05, 0.08, 0.10), 0.35))
        # Crisp cyan top status banner
        parts.append(box(bx, by + 8.6, z_scr + 0.04, 45.0, 3.4, 0.05, OLED_CYAN, 0.45))
        parts.append(box(bx - 12.0, by + 8.6, z_scr + 0.07, 14.0, 1.2, 0.04, (0.02, 0.08, 0.12), 0.2))
        parts.append(box(bx + 13.0, by + 8.6, z_scr + 0.07, 10.0, 1.2, 0.04, WHITE, 0.4))
        # Big bold yellow & green digital readout bars
        for dx, col in [(-14.5, OLED_YELL), (-8.5, OLED_YELL), (-2.5, OLED_YELL), (4.5, (0.20, 0.92, 0.38)), (11.5, (0.20, 0.92, 0.38))]:
            parts.append(box(bx + dx, by + 2.2, z_scr + 0.06, 4.2, 6.2, 0.05, col, 0.5))
        parts.append(box(bx + 18.5, by + 1.0, z_scr + 0.06, 5.0, 2.8, 0.05, WHITE, 0.4))
        # 3 bottom telemetry boxes
        for tx, col in [(-14.5, (0.12, 0.34, 0.52)), (0.0, (0.12, 0.45, 0.32)), (14.5, (0.45, 0.28, 0.10))]:
            parts.append(box(bx + tx, by - 6.6, z_scr + 0.04, 13.2, 5.8, 0.05, col, 0.3))
            parts.append(box(bx + tx, by - 6.6, z_scr + 0.07, 9.0, 1.6, 0.04, WHITE, 0.4))

    with open(os.path.join(OUT_DIR, filename), "w") as f:
        f.write("".join(parts))


def make_ec11_fluted_knob():
    """
    Alps EC11 Rotary Encoder with large matte-black fluted instrument knob.
    Matches KiCad footprint: A at (0,0), shaft center at (x=7.5, y_vrml=-2.5).
    """
    parts = [HEADER]
    sx, sy = 7.5, -2.5

    # Metallic/blue EC11 housing (12.0 x 12.0 x 6.5 mm at z=0..6.5)
    parts.append(box(sx, sy, 3.25, 12.0, 12.0, 6.5, METAL_SILV, 0.65))
    parts.append(box(sx, sy, 1.2, 12.4, 12.4, 2.4, (0.12, 0.32, 0.62), 0.35))

    # Electrical pins (A, C, B on left x=0; S1, S2 on right x=14.5) + mounting tabs
    for px, py in [(0.0, 0.0), (0.0, -2.5), (0.0, -5.0), (14.5, -5.0), (14.5, 0.0)]:
        parts.append(box(px, py, -0.75, 0.8, 0.4, 2.5, METAL_SILV, 0.7))
    for my in (3.1, -8.1):
        parts.append(box(sx, my, -0.5, 2.0, 0.8, 2.0, METAL_SILV, 0.7))

    # Threaded bushing (oct_z at z=6.5..11.5)
    parts.append(oct_z(sx, sy, 9.0, 3.5, 5.0, BRASS, 0.75))
    parts.append(oct_z(sx, sy, 7.6, 4.8, 1.8, METAL_SILV, 0.75))

    # Large matte-black fluted instrument knob (diameter 18.0 mm -> r=9.0, z=11.5..26.5)
    parts.append(oct_z(sx, sy, 19.0, 9.0, 15.0, PLASTIC_BK, 0.25))
    parts.append(oct_z(sx, sy, 12.2, 9.6, 1.4, (0.18, 0.18, 0.20), 0.25))
    # Subtle matte top inlay
    parts.append(oct_z(sx, sy, 26.55, 6.5, 0.2, (0.18, 0.18, 0.20), 0.20))

    # Crisp white pointer line on top and front face (-Y_vrml = +Y_kicad front)
    # Outer flat of oct_z(r=9.0) sits at radius * 0.9239 = 8.315 mm
    parts.append(box(sx, sy - 4.15, 26.72, 1.3, 8.3, 0.25, WHITE, 0.4))
    parts.append(box(sx, sy - 8.38, 19.6, 1.3, 0.25, 13.8, WHITE, 0.4))

    with open(os.path.join(OUT_DIR, "EC11_Fluted_Knob.wrl"), "w") as f:
        f.write("".join(parts))


def make_sw_12mm_cap(filename, cap_rgb):
    """
    12x12mm Tactile Pushbutton Switch with colored square push-cap.
    Matches pads (0,0), (12.5,0), (0,5.0), (12.5,5.0) -> center (6.25, -2.5).
    """
    parts = [HEADER]
    cx, cy = 6.25, -2.5

    # 4 metal leads going through board
    for px in (0.0, 12.5):
        for py in (0.0, -5.0):
            parts.append(box(px, py, -0.75, 0.9, 0.5, 2.5, METAL_SILV, 0.7))

    # Dark black 12.0 x 12.0 x 5.0 mm housing (z = 0..5.0)
    parts.append(box(cx, cy, 2.5, 12.0, 12.0, 5.0, PLASTIC_BK, 0.25))
    # Subtle bezel collar
    parts.append(oct_z(cx, cy, 5.2, 4.0, 0.6, (0.20, 0.20, 0.22), 0.3))

    # Colored square push-cap (9.5 x 9.5 x 5.5 mm at z = 5.0..10.5)
    parts.append(box(cx, cy, 7.75, 9.5, 9.5, 5.5, cap_rgb, 0.45))

    with open(os.path.join(OUT_DIR, filename), "w") as f:
        f.write("".join(parts))


def make_tp4056_charger():
    """
    TP4056 + DW01A 3.7V LiPo Charger breakout (28.0 x 17.0 mm) seated on 6 machined sockets (z_pcb=11.0mm).
    Left edge (x=0): Pin 1 IN+ at (0,0), Pin 2 IN- at (0,-12.70 VRML / +12.70 KiCad).
    Right edge (x=25.40): Pin 3 OUT+ at (25.40,0), Pin 4 B+ at (25.40,-5.08),
    Pin 5 B- at (25.40,-7.62), Pin 6 OUT- at (25.40,-12.70).
    """
    parts = [HEADER]
    cx = 12.70
    cy = -6.35
    z_pcb = 11.0
    pcb_t = 1.6

    # Blue PCB (28.0 x 17.0 x 1.6 mm)
    parts.append(box(cx, cy, z_pcb + pcb_t / 2, 28.0, 17.0, pcb_t, PCB_BLUE, 0.30))

    # Silver USB-C shell on left edge (x = 1.5)
    parts.append(box(1.5, cy, z_pcb + pcb_t + 1.6, 7.6, 9.0, 3.2, METAL_SILV, 0.80))

    # TP4056 SOIC-8 IC in center
    parts.append(box(12.2, cy, z_pcb + pcb_t + 0.85, 5.0, 4.0, 1.7, IC_DARK, 0.35))
    # DW01A protection IC & 8205A dual MOSFET near right edge
    parts.append(box(19.0, cy + 3.2, z_pcb + pcb_t + 0.55, 3.0, 2.0, 1.1, IC_DARK, 0.35))
    parts.append(box(19.2, cy - 3.2, z_pcb + pcb_t + 0.60, 3.2, 4.4, 1.2, IC_DARK, 0.35))

    # Red (charging) & Blue (charged) SMD LED indicators near left-top/bottom
    parts.append(box(8.0, cy + 4.8, z_pcb + pcb_t + 0.40, 2.0, 1.2, 0.8, (0.92, 0.14, 0.14), 0.50))
    parts.append(box(8.0, cy - 4.8, z_pcb + pcb_t + 0.40, 2.0, 1.2, 0.8, (0.10, 0.45, 0.95), 0.50))

    # PROG & sense SMD passives
    for sx, sy in [(12.2, cy + 4.5), (12.2, cy - 4.5), (16.2, cy)]:
        parts.append(box(sx, sy, z_pcb + pcb_t + 0.30, 1.6, 1.0, 0.6, (0.55, 0.42, 0.25), 0.30))

    # 6 gold pins going down into machined sockets
    pins = [
        (0.0, 0.0), (0.0, -12.70),
        (25.40, 0.0), (25.40, -5.08), (25.40, -7.62), (25.40, -12.70),
    ]
    for px, py in pins:
        parts.append(box(px, py, z_pcb - 3.5, 0.85, 0.85, 7.5, GOLD, 0.80))
        parts.append(oct_z(px, py, z_pcb + pcb_t + 0.30, 0.90, 0.60, METAL_SILV, 0.70))

    with open(os.path.join(OUT_DIR, "TP4056_Charger.wrl"), "w") as f:
        f.write("".join(parts))


def make_sw_slide_spdt():
    """
    THT SPDT slide/toggle power switch (3 pins at (0,0), (2.54,0), (5.08,0)).
    Silver metal housing (11.5 x 5.5 x 6.0 mm) centered at (2.54, 0) + black slider knob (3.2 x 3.2 x 6.0 mm).
    """
    parts = [HEADER]
    cx, cy = 2.54, 0.0

    # 3 flat silver THT pins going down through board
    for px in (0.0, 2.54, 5.08):
        parts.append(box(px, 0.0, -1.25, 0.7, 0.4, 2.5, METAL_SILV, 0.75))

    # Dark phenolic base rim + silver metal housing (11.5 x 5.5 x 6.0 mm)
    parts.append(box(cx, cy, 3.0, 11.5, 5.5, 6.0, METAL_SILV, 0.65))
    parts.append(box(cx, cy, 6.02, 7.6, 3.4, 0.12, PLASTIC_BK, 0.20))

    # Tall black rectangular slider knob (3.2 x 3.2 x 6.0 mm on top)
    parts.append(box(cx - 1.4, cy, 9.0, 3.2, 3.2, 6.0, PLASTIC_BK, 0.30))

    with open(os.path.join(OUT_DIR, "SW_Slide_SPDT.wrl"), "w") as f:
        f.write("".join(parts))


def make_sw_slide_dpdt():
    """
    THT DPDT Master Power Switch (6 pins: Row 1 at y=0, x=0..5.08; Row 2 at y=-2.54 VRML / +2.54 KiCad, x=0..5.08).
    Silver metal housing (12.0 x 7.5 x 6.5 mm) centered at (2.54, -1.27) + black slider actuator.
    """
    parts = [HEADER]
    cx, cy = 2.54, -1.27

    for py in (0.0, -2.54):
        for px in (0.0, 2.54, 5.08):
            parts.append(box(px, py, -1.25, 0.75, 0.45, 2.5, METAL_SILV, 0.75))

    parts.append(box(cx, cy, 3.25, 12.0, 7.5, 6.5, METAL_SILV, 0.65))
    parts.append(box(cx, cy, 6.52, 8.0, 4.4, 0.12, PLASTIC_BK, 0.20))
    parts.append(box(cx + 1.4, cy, 9.5, 3.6, 4.0, 6.0, (0.85, 0.14, 0.14), 0.35))

    with open(os.path.join(OUT_DIR, "SW_Slide_DPDT.wrl"), "w") as f:
        f.write("".join(parts))


def make_esp32_c3_supermini():
    """
    ESP32-C3 SuperMini (HZWDONE, 18.0 x 22.52 mm) seated atop two 1x08 female pin sockets (15.24mm row spacing).
    Pin 1 at (0, 0), Pins 1..8 at x=0, y_vrml=0..-17.78 (y_kicad=0..17.78).
    Pins 9..16 at x=15.24, y_vrml=0..-17.78.
    USB-C at top (+Y_vrml = -Y_kicad), Red Ceramic C3 Antenna at bottom (-Y_vrml = +Y_kicad).
    """
    parts = [HEADER]
    cx = 7.62
    cy = -8.89
    z_pcb = 11.0
    pcb_t = 1.6

    # Matte black PCB (18.0 x 22.52 x 1.6 mm)
    parts.append(box(cx, cy, z_pcb + pcb_t / 2, 18.0, 22.52, pcb_t, PCB_BLACK, 0.25))

    # Silver USB-C receptacle at top edge (y_vrml = +1.0, overhanging slightly)
    parts.append(box(cx, 1.0, z_pcb + pcb_t + 1.6, 8.9, 7.4, 3.2, METAL_SILV, 0.80))

    # ESP32-C3FN4 QFN-32 silicon package in center
    parts.append(box(cx, cy - 0.5, z_pcb + pcb_t + 0.45, 5.0, 5.0, 0.9, IC_DARK, 0.40))
    # 40MHz crystal & LDO
    parts.append(box(cx - 3.6, cy + 3.2, z_pcb + pcb_t + 0.40, 2.0, 2.5, 0.8, METAL_SILV, 0.70))
    parts.append(box(cx + 3.6, cy + 3.2, z_pcb + pcb_t + 0.50, 2.8, 1.6, 1.0, IC_DARK, 0.35))

    # Tactile BOOT and RST micro-buttons flanking USB-C
    for bx in (cx - 4.8, cx + 4.8):
        parts.append(box(bx, -2.5, z_pcb + pcb_t + 0.55, 2.6, 3.4, 1.1, METAL_SILV, 0.60))
        parts.append(oct_z(bx, -2.5, z_pcb + pcb_t + 1.30, 0.75, 0.5, PLASTIC_BK, 0.30))

    # Red 3D ceramic chip antenna at bottom edge (y_vrml = -18.5)
    parts.append(box(cx + 1.5, -18.5, z_pcb + pcb_t + 0.90, 5.2, 2.2, 1.8, (0.86, 0.14, 0.16), 0.45))
    parts.append(box(cx - 2.2, -18.5, z_pcb + pcb_t + 0.05, 1.2, 2.2, 0.1, GOLD, 0.75))

    # Male 2x 1x08 header shrouds + gold pins going down into female sockets
    for hx in (0.0, 15.24):
        parts.append(box(hx, cy, z_pcb - 1.25, 2.54, 8 * 2.54, 2.5, PLASTIC_BK, 0.20))
        for p in range(8):
            py = -p * 2.54
            parts.append(box(hx, py, z_pcb - 4.0, 0.64, 0.64, 6.0, GOLD, 0.80))
            parts.append(oct_z(hx, py, z_pcb + pcb_t + 0.30, 0.75, 0.6, METAL_SILV, 0.70))
            parts.append(box(hx, py, z_pcb + pcb_t + 0.55, 0.64, 0.64, 1.1, GOLD, 0.80))

    with open(os.path.join(OUT_DIR, "ESP32_C3_SuperMini.wrl"), "w") as f:
        f.write("".join(parts))


def make_bq25895_charger():
    """
    TI BQ25895 1.5MHz 3A NVDC Switching Charger Module (20.0 x 24.0 mm) seated on 2x 1x06 female sockets (15.24mm row spacing).
    Left row (Pins 1..6: VBUS, PMID, GND, SYS, BAT, TS) at x=0, y_vrml=0..-12.70 (y_kicad=0..12.70).
    Right row (Pins 7..12: SCL, SDA, INT, STAT, CE_N, OTG) at x=15.24, y_vrml=0..-12.70.
    """
    parts = [HEADER]
    cx = 7.62
    cy = -6.35
    z_pcb = 11.0
    pcb_t = 1.6

    # Purple/Dark-Blue High-Power Switching Charger PCB (20.0 x 24.0 x 1.6 mm)
    parts.append(box(cx, cy, z_pcb + pcb_t / 2, 20.0, 24.0, pcb_t, PCB_PURPLE, 0.30))

    # TI BQ25895RTWR WQFN-24 (4.0 x 4.0 mm) in center-right
    parts.append(box(cx + 2.0, cy + 1.0, z_pcb + pcb_t + 0.45, 4.0, 4.0, 0.9, IC_DARK, 0.40))

    # Shielded 1.0uH / 5A Molded Alloy Buck Inductor (7.0 x 6.6 x 3.0 mm) on center-left
    parts.append(box(cx - 2.6, cy - 2.2, z_pcb + pcb_t + 1.50, 6.8, 6.8, 3.0, (0.24, 0.25, 0.27), 0.35))

    # High-capacity 1206/0805 MLCC bank (VBUS, PMID, VSYS, VBAT, REGN, BTST)
    for sx, sy in [(cx - 3.2, cy + 4.8), (cx - 0.8, cy + 4.8), (cx + 1.8, cy + 5.2),
                   (cx + 2.6, cy - 4.5), (cx + 2.6, cy - 6.5)]:
        parts.append(box(sx, sy, z_pcb + pcb_t + 0.55, 2.0, 3.2, 1.1, (0.58, 0.44, 0.24), 0.35))

    # Male 2x 1x06 header shrouds + gold pins going down into female sockets
    for hx in (0.0, 15.24):
        parts.append(box(hx, cy, z_pcb - 1.25, 2.54, 6 * 2.54, 2.5, PLASTIC_BK, 0.20))
        for p in range(6):
            py = -p * 2.54
            parts.append(box(hx, py, z_pcb - 4.0, 0.64, 0.64, 6.0, GOLD, 0.80))
            parts.append(oct_z(hx, py, z_pcb + pcb_t + 0.30, 0.75, 0.6, METAL_SILV, 0.70))
            parts.append(box(hx, py, z_pcb + pcb_t + 0.55, 0.64, 0.64, 1.1, GOLD, 0.80))

    with open(os.path.join(OUT_DIR, "BQ25895_Charger.wrl"), "w") as f:
        f.write("".join(parts))


if __name__ == "__main__":
    make_wemos_lolin32_lite()
    make_ads1115_breakout()
    make_mt3608_boost()
    make_oled_096()
    make_zif_14()
    make_tft_1_9("TFT_1_9_ST7789_Curve.wrl", "curve")
    make_tft_1_9("TFT_1_9_ST7789_DMM.wrl", "dmm")
    make_ec11_fluted_knob()
    for fname, cap_rgb in [
        ("SW_12mm_YellowCap.wrl", (0.95, 0.80, 0.08)),
        ("SW_12mm_RedCap.wrl", (0.85, 0.12, 0.14)),
        ("SW_12mm_BlueCap.wrl", (0.12, 0.42, 0.88)),
        ("SW_12mm_DarkCap.wrl", (0.22, 0.23, 0.26)),
    ]:
        make_sw_12mm_cap(fname, cap_rgb)
    make_tp4056_charger()
    make_sw_slide_spdt()
    make_sw_slide_dpdt()
    make_esp32_c3_supermini()
    make_bq25895_charger()
    print("Generated 17 custom VRML 3D models in", OUT_DIR)
