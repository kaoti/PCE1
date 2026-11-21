#!/usr/bin/env python3
"""
Generate KiCad 9 project configuration, custom symbol library (Ge_Engine.kicad_sym),
and 100% Through-Hole + Socketed footprint library (Ge_Engine.pretty) with full 3D models.
"""
import os

PROJ_DIR = "/Users/kaoti/Desktop/PCE1"
FP_DIR = os.path.join(PROJ_DIR, "Ge_Engine.pretty")
os.makedirs(FP_DIR, exist_ok=True)


def write(path, text):
    with open(path, "w") as f:
        f.write(text)


def make_project_files():
    write(os.path.join(PROJ_DIR, "sym-lib-table"), """(sym_lib_table
  (version 7)
  (lib (name "Ge_Engine")(type "KiCad")(uri "${KIPRJMOD}/Ge_Engine.kicad_sym")(options "")(descr "Custom symbols for Ge Parametric Engine"))
)
""")
    write(os.path.join(PROJ_DIR, "fp-lib-table"), """(fp_lib_table
  (version 7)
  (lib (name "Ge_Engine")(type "KiCad")(uri "${KIPRJMOD}/Ge_Engine.pretty")(options "")(descr "100% THT Socketed footprints with 3D models"))
)
""")
    write(os.path.join(PROJ_DIR, "Ge_Parametric_Engine.kicad_pro"), """{
  "board": {
    "design_settings": {
      "defaults": {
        "board_outline_line_width": 0.15
      },
      "rules": {
        "min_clearance": 0.2,
        "min_track_width": 0.25,
        "min_via_annular_width": 0.15,
        "min_via_diameter": 0.6,
        "min_through_hole_diameter": 0.3
      }
    }
  },
  "libraries": {
    "pinned_footprint_libs": ["Ge_Engine"],
    "pinned_symbol_libs": ["Ge_Engine"]
  },
  "meta": {
    "filename": "Ge_Parametric_Engine.kicad_pro",
    "version": 1
  },
  "schematic": {
    "drawing": {
      "default_line_thickness": 6.0
    }
  }
}
""")


def fp_wrap(name, descr, ref_xy, val_xy, body_lines, pads_str, models_str, ref_size=0.80, val_layer="F.SilkS", val_size=0.80):
    rx, ry = ref_xy
    vx, vy = val_xy
    return f"""(footprint "{name}" (version 20240108) (generator "ge_engine_gen")
  (layer "F.Cu")
  (descr "{descr}")
  (attr through_hole)
  (fp_text reference "REF**" (at {rx:.2f} {ry:.2f}) (layer "F.SilkS")
    (effects (font (size {ref_size:.2f} {ref_size:.2f}) (thickness 0.15)))
  )
  (fp_text value "{name}" (at {vx:.2f} {vy:.2f}) (layer "{val_layer}")
    (effects (font (size {val_size:.2f} {val_size:.2f}) (thickness 0.15)))
  )
{body_lines}
{pads_str}
{models_str}
)
"""


def line_silk(x1, y1, x2, y2, layer="F.SilkS", w=0.15):
    return f'  (fp_line (start {x1:.2f} {y1:.2f}) (end {x2:.2f} {y2:.2f}) (stroke (width {w}) (type solid)) (layer "{layer}"))\n'


def rect_lines(x1, y1, x2, y2, layer="F.SilkS", w=0.15):
    return (
        f'  (fp_line (start {x1:.2f} {y1:.2f}) (end {x2:.2f} {y1:.2f}) (stroke (width {w}) (type solid)) (layer "{layer}"))\n'
        f'  (fp_line (start {x2:.2f} {y1:.2f}) (end {x2:.2f} {y2:.2f}) (stroke (width {w}) (type solid)) (layer "{layer}"))\n'
        f'  (fp_line (start {x2:.2f} {y2:.2f}) (end {x1:.2f} {y2:.2f}) (stroke (width {w}) (type solid)) (layer "{layer}"))\n'
        f'  (fp_line (start {x1:.2f} {y2:.2f}) (end {x1:.2f} {y1:.2f}) (stroke (width {w}) (type solid)) (layer "{layer}"))\n'
    )


def pad_th(num, x, y, shape="circle", sx=1.7, sy=1.7, drill=1.0):
    return f'  (pad "{num}" thru_hole {shape} (at {x:.2f} {y:.2f}) (size {sx:.2f} {sy:.2f}) (drill {drill:.2f}) (layers "*.Cu" "*.Mask"))\n'


def model_entry(path, ox=0, oy=0, oz=0, rx=0, ry=0, rz=0, sx=1, sy=1, sz=1):
    return f"""  (model "{path}"
    (offset (xyz {ox:.3f} {oy:.3f} {oz:.3f}))
    (scale (xyz {sx:.3f} {sy:.3f} {sz:.3f}))
    (rotate (xyz {rx:.3f} {ry:.3f} {rz:.3f}))
  )
"""


def make_footprints():
    # 1. WEMOS LOLIN32 Lite V1.0.0 Socketed (2x 1x13 female sockets at 22.86mm spacing + module 3D model)
    pads = []
    for i in range(13):
        sh = "rect" if i == 0 else "circle"
        pads.append(pad_th(i + 1, 0.0, i * 2.54, sh, 1.7, 1.7, 1.0))
    for i in range(13):
        pads.append(pad_th(14 + i, 22.86, i * 2.54, "circle", 1.7, 1.7, 1.0))
    body = rect_lines(-1.5, -8.5, 24.36, 41.0, "F.SilkS", 0.18)
    body += rect_lines(-1.8, -8.8, 24.66, 41.3, "F.CrtYd", 0.05)
    body += '  (fp_text user "ANTENNA" (at 11.43 -5.5) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))\n'
    body += '  (fp_text user "USB-C" (at 17.2 38.0) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))\n'
    body += '  (fp_text user "2x 1x13 SOCKET" (at 11.43 15.24) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))\n'
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x13_P2.54mm_Vertical.step", 0, 0, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x13_P2.54mm_Vertical.step", 22.86, 0, 0) +
        model_entry("${KIPRJMOD}/3dmodels/Wemos_Lolin32_Lite.wrl", 0, 0, 0)
    )
    write(os.path.join(FP_DIR, "Socket_Wemos_Lolin32_Lite.kicad_mod"),
          fp_wrap("Socket_Wemos_Lolin32_Lite", "WEMOS LOLIN32 Lite V1.0.0 on 2x13 female sockets",
                  (11.43, -10.2), (11.43, 43.0), body, "".join(pads), models))

    # 2. ADS1115 10-pin Breakout Socketed (1x10 horizontal along +X)
    pads = []
    for i in range(10):
        sh = "rect" if i == 0 else "circle"
        pads.append(pad_th(i + 1, i * 2.54, 0.0, sh, 1.7, 1.7, 1.0))
    body = rect_lines(-2.57, -15.0, 25.43, 2.0, "F.SilkS", 0.18)
    body += rect_lines(-2.9, -15.3, 25.8, 2.3, "F.CrtYd", 0.05)
    body += '  (fp_text user "ADS1115 16-BIT (1x10 SKT)" (at 11.43 -7.5) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))\n'
    # PinSocket_1x10 goes along -Y_3d (+Y_kicad). Rotating -90 around Z maps -Y_3d -> +X_3d (+X_kicad)!
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x10_P2.54mm_Vertical.step", 0, 0, 0, 0, 0, -90) +
        model_entry("${KIPRJMOD}/3dmodels/ADS1115_Breakout.wrl", 0, 0, 0)
    )
    write(os.path.join(FP_DIR, "Socket_ADS1115_Module.kicad_mod"),
          fp_wrap("Socket_ADS1115_Module", "ADS1115 10-pin breakout in 1x10 female socket",
                  (11.43, -16.8), (11.43, 3.8), body, "".join(pads), models))

    # 3. MT3608 Boost Converter Socketed (4 corner machined sockets)
    pads = [
        pad_th(1, 0.0, 0.0, "rect", 2.0, 2.0, 1.1),      # IN+
        pad_th(2, 0.0, 12.70, "circle", 2.0, 2.0, 1.1),   # IN-
        pad_th(3, 33.02, 0.0, "circle", 2.0, 2.0, 1.1),   # OUT+
        pad_th(4, 33.02, 12.70, "circle", 2.0, 2.0, 1.1), # OUT-
    ]
    body = rect_lines(-1.74, -2.15, 34.76, 14.85, "F.SilkS", 0.18)
    body += rect_lines(-2.1, -2.5, 35.1, 15.2, "F.CrtYd", 0.05)
    body += '  (fp_text user "MT3608 BOOST -> 12.0V" (at 16.51 6.35) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))\n'
    body += '  (fp_text user "IN+" (at 3.2 0.0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    body += '  (fp_text user "IN-" (at 3.2 12.7) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    body += '  (fp_text user "OUT+" (at 29.0 0.0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    body += '  (fp_text user "OUT-" (at 29.0 12.7) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 0.0, 0.0, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 0.0, -12.70, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 33.02, 0.0, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 33.02, -12.70, 0) +
        model_entry("${KIPRJMOD}/3dmodels/MT3608_Boost.wrl", 0, 0, 0)
    )
    write(os.path.join(FP_DIR, "Socket_MT3608_Boost.kicad_mod"),
          fp_wrap("Socket_MT3608_Boost", "MT3608 Boost Converter on 4x machined sockets",
                  (16.51, -3.6), (16.51, 16.5), body, "".join(pads), models))

    # 4. OLED 0.96" SSD1306 Socketed (1x04 along +X: GND, VCC, SCL, SDA)
    pads = []
    for i in range(4):
        sh = "rect" if i == 0 else "circle"
        pads.append(pad_th(i + 1, i * 2.54, 0.0, sh, 1.7, 1.7, 1.0))
    body = rect_lines(-9.84, -2.15, 17.46, 25.15, "F.SilkS", 0.18)
    body += rect_lines(-10.2, -2.5, 17.8, 25.5, "F.CrtYd", 0.05)
    body += '  (fp_text user "OLED 0.96 I2C (1x04 SKT)" (at 3.81 12.0) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))\n'
    body += '  (fp_text user "G V C D" (at 3.81 2.4) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.15))))\n'
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x04_P2.54mm_Vertical.step", 0, 0, 0, 0, 0, -90) +
        model_entry("${KIPRJMOD}/3dmodels/OLED_096_SSD1306.wrl", 0, 0, 0)
    )
    write(os.path.join(FP_DIR, "Socket_OLED_096.kicad_mod"),
          fp_wrap("Socket_OLED_096", "0.96in I2C SSD1306 OLED on 1x04 female socket",
                  (3.81, -3.8), (3.81, 27.0), body, "".join(pads), models))

    # 5. 14-Pin ZIF Socket (7.62mm row spacing)
    pads = []
    for i in range(7):
        sh = "rect" if i == 0 else "circle"
        pads.append(pad_th(i + 1, 0.0, i * 2.54, sh, 1.8, 1.8, 1.0))
    for i in range(7):
        pads.append(pad_th(8 + i, 7.62, (6 - i) * 2.54, "circle", 1.8, 1.8, 1.0))
    body = rect_lines(-3.7, -8.5, 11.3, 23.5, "F.SilkS", 0.20)
    body += rect_lines(-6.0, -8.8, 18.0, 23.8, "F.CrtYd", 0.05)
    body += '  (fp_text user "ZIF-14 DUT" (at 3.81 7.62 90) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))\n'
    for y_lbl, role_txt in [
        (0.00, "1"), (2.54, "1"), (5.08, "1"), (7.62, "2"),
        (10.16, "3"), (12.70, "3"), (15.24, "3"),
    ]:
        body += f'  (fp_text user "{role_txt}" (at -5.00 {y_lbl:.2f} 0) (layer "F.SilkS") (effects (font (size 0.9 0.9) (thickness 0.15))))\n'
    models = model_entry("${KIPRJMOD}/3dmodels/ZIF_Socket_14Pin.wrl", 0, 0, 0)
    write(os.path.join(FP_DIR, "Socket_ZIF_14Pin.kicad_mod"),
          fp_wrap("Socket_ZIF_14Pin", "14-pin Zero Insertion Force DUT Test Socket",
                  (3.81, -10.5), (3.81, 25.0), body, "".join(pads), models))

    # 6. DIP-6, DIP-8, DIP-14, DIP-16, DIP-28 Machined IC Sockets (with built-in KiCad DIP+Socket 3D models!)
    for n in (6, 8, 14, 16, 28):
        half = n // 2
        pads = []
        for i in range(half):
            sh = "rect" if i == 0 else "circle"
            pads.append(pad_th(i + 1, 0.0, i * 2.54, sh, 1.65, 1.65, 0.85))
        for i in range(half):
            pads.append(pad_th(half + 1 + i, 7.62, (half - 1 - i) * 2.54, "circle", 1.65, 1.65, 0.85))
        ymax = (half - 1) * 2.54
        body = rect_lines(1.30, -1.25, 6.32, ymax + 1.25, "F.SilkS", 0.15)
        body += rect_lines(-1.7, -1.8, 9.32, ymax + 1.8, "F.CrtYd", 0.05)
        body += f'  (fp_arc (start 2.81 -1.25) (mid 3.81 -0.25) (end 4.81 -1.25) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))\n'
        models = (
            model_entry(f"${{KICAD9_3DMODEL_DIR}}/Package_DIP.3dshapes/DIP-{n}_W7.62mm_Socket.step", 0, 0, 0) +
            model_entry(f"${{KICAD9_3DMODEL_DIR}}/Package_DIP.3dshapes/DIP-{n}_W7.62mm.step", 0, 0, 3.2)
        )
        write(os.path.join(FP_DIR, f"Socket_DIP-{n}_W7.62mm.kicad_mod"),
              fp_wrap(f"Socket_DIP-{n}_W7.62mm", f"DIP-{n} machined IC socket with seated DIP-{n} IC",
                      (3.81, -2.6), (3.81, ymax + 2.6), body, "".join(pads), models))

    # 7. TO-92 Seated in 1x03 Machined Socket (pins along +X: 0, 2.54, 5.08)
    # Matches TO-92_Inline_Wide.step: flat face on +Y side (y = +1.75), curved D-cap on -Y side.
    pads = [
        pad_th(1, 0.0, 0.0, "rect", 1.7, 1.7, 1.0),
        pad_th(2, 2.54, 0.0, "circle", 1.7, 1.7, 1.0),
        pad_th(3, 5.08, 0.0, "circle", 1.7, 1.7, 1.0),
    ]
    body = line_silk(-0.80, 1.75, 5.88, 1.75, "F.SilkS", 0.20)  # flat face along +Y
    body += '  (fp_arc (start -0.80 -1.75) (mid 2.54 -2.55) (end 5.88 -1.75) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))\n'
    body += line_silk(-0.80, 1.75, -1.35, 1.20, "F.SilkS", 0.15)
    body += line_silk(-1.35, -1.20, -0.80, -1.75, "F.SilkS", 0.15)
    body += line_silk(5.88, 1.75, 6.43, 1.20, "F.SilkS", 0.15)
    body += line_silk(6.43, -1.20, 5.88, -1.75, "F.SilkS", 0.15)
    body += '  (fp_circle (center -1.45 0.0) (end -1.25 0.0) (stroke (width 0.25) (type solid)) (layer "F.SilkS"))\n'
    body += '  (fp_text user "FLAT" (at 2.54 2.75) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.15))))\n'
    body += rect_lines(-1.9, -2.8, 6.9, 3.2, "F.CrtYd", 0.05)
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x03_P2.54mm_Vertical.step", 0, 0, 0, 0, 0, -90) +
        model_entry("${KICAD9_3DMODEL_DIR}/Package_TO_SOT_THT.3dshapes/TO-92_Inline_Wide.step", 0, 0, 8.2)
    )
    write(os.path.join(FP_DIR, "Socket_TO92_3Pin.kicad_mod"),
          fp_wrap("Socket_TO92_3Pin", "3-pin machined socket with seated TO-92 semiconductor",
                  (2.54, -3.5), (2.54, 3.8), body, "".join(pads), models))

    # 8. Standalone 1x03 Machined DUT Socket (empty socket for DUT insertion!)
    body_dut = rect_lines(-1.35, -1.45, 6.43, 1.45, "F.SilkS", 0.18)
    body_dut += rect_lines(-1.6, -1.6, 6.7, 1.6, "F.CrtYd", 0.05)
    body_dut += '  (fp_text user "1  2  3" (at 2.54 2.5) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.14))))\n'
    models_dut = model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x03_P2.54mm_Vertical.step", 0, 0, 0, 0, 0, -90)
    write(os.path.join(FP_DIR, "Socket_1x03_DUT.kicad_mod"),
          fp_wrap("Socket_1x03_DUT", "3-pin machined female DUT socket (TP1, TP2, TP3)",
                  (2.54, -2.6), (2.54, 3.8), body_dut, "".join(pads), models_dut))

    # 9. Pin Headers (1x02, 1x03, 1x04, 1x06 horizontal along +X)
    for n in (2, 3, 4, 6):
        pads = [pad_th(i + 1, i * 2.54, 0.0, "rect" if i == 0 else "circle", 1.7, 1.7, 1.0) for i in range(n)]
        xmax = (n - 1) * 2.54
        body = rect_lines(-1.35, -1.45, xmax + 1.35, 1.45, "F.SilkS", 0.15)
        body += rect_lines(-1.6, -1.6, xmax + 1.6, 1.6, "F.CrtYd", 0.05)
        models = model_entry(f"${{KICAD9_3DMODEL_DIR}}/Connector_PinHeader_2.54mm.3dshapes/PinHeader_1x0{n}_P2.54mm_Vertical.step", 0, 0, 0, 0, 0, -90)
        write(os.path.join(FP_DIR, f"Header_1x0{n}_P2.54mm.kicad_mod"),
              fp_wrap(f"Header_1x0{n}_P2.54mm", f"1x0{n} 2.54mm vertical pin header",
                      (xmax / 2, -2.5), (xmax / 2, 2.5), body, "".join(pads), models))
    write(os.path.join(FP_DIR, "Socket_USBC_Breakout.kicad_mod"),
          open(os.path.join(FP_DIR, "Header_1x06_P2.54mm.kicad_mod")).read().replace("Header_1x06_P2.54mm", "Socket_USBC_Breakout"))

    # 10. Axial Resistor DIN0207 7.62mm pitch (Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "circle", 1.6, 1.6, 0.8), pad_th(2, 7.62, 0.0, "circle", 1.6, 1.6, 0.8)]
    body = rect_lines(1.30, -1.25, 6.32, 1.25, "F.SilkS", 0.15)
    body += rect_lines(-1.0, -1.5, 8.62, 1.5, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Resistor_THT.3dshapes/R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "R_Axial_P7.62mm.kicad_mod"),
          fp_wrap("R_Axial_P7.62mm", "THT Axial 1/4W Metal Film Resistor 7.62mm pitch",
                  (3.81, -1.95), (3.81, 1.95), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 11. Ceramic Disc Cap 5.00mm pitch (Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "circle", 1.6, 1.6, 0.8), pad_th(2, 5.0, 0.0, "circle", 1.6, 1.6, 0.8)]
    body = rect_lines(1.15, -1.25, 3.85, 1.25, "F.SilkS", 0.15)
    body += rect_lines(-1.0, -1.7, 6.0, 1.7, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Capacitor_THT.3dshapes/C_Disc_D5.0mm_W2.5mm_P5.00mm.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "C_Disc_P5.00mm.kicad_mod"),
          fp_wrap("C_Disc_P5.00mm", "THT Ceramic/Film Capacitor 5.00mm pitch",
                  (2.5, -2.05), (2.5, 2.05), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 12. Electrolytic Cap D8.0mm P3.50mm (Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "rect", 1.8, 1.8, 0.9), pad_th(2, 3.5, 0.0, "circle", 1.8, 1.8, 0.9)]
    body = '  (fp_circle (center 1.75 0) (end 5.95 0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))\n'
    body += '  (fp_text user "+" (at -1.5 -1.8) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))\n'
    body += rect_lines(-2.6, -4.3, 6.1, 4.3, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Capacitor_THT.3dshapes/CP_Radial_D8.0mm_P3.50mm.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "CP_Radial_D8_P3.5mm.kicad_mod"),
          fp_wrap("CP_Radial_D8_P3.5mm", "THT Electrolytic Capacitor D8.0mm P3.50mm",
                  (1.75, -5.05), (1.75, 5.05), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 13. Axial Inductor 100uH 12.70mm pitch
    pads = [pad_th(1, 0.0, 0.0, "circle", 1.8, 1.8, 0.9), pad_th(2, 12.70, 0.0, "circle", 1.8, 1.8, 0.9)]
    body = rect_lines(1.5, -2.2, 11.2, 2.2, "F.SilkS", 0.15)
    body += rect_lines(-1.1, -2.5, 13.8, 2.5, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Inductor_THT.3dshapes/L_Axial_L9.5mm_D4.0mm_P12.70mm_Horizontal_Fastron_SMCC.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "L_Axial_P12.7mm.kicad_mod"),
          fp_wrap("L_Axial_P12.7mm", "THT Axial 100uH RF/Power Choke 12.70mm pitch",
                  (6.35, -3.2), (6.35, 3.2), body, "".join(pads), models))

    # 14. Tactile Pushbutton 6x6mm (broken silkscreen lines clear of all 4 pads)
    pads = [
        pad_th(1, 0.0, 0.0, "circle", 2.0, 2.0, 1.1),
        pad_th(1, 6.5, 0.0, "circle", 2.0, 2.0, 1.1),
        pad_th(2, 0.0, 4.5, "circle", 2.0, 2.0, 1.1),
        pad_th(2, 6.5, 4.5, "circle", 2.0, 2.0, 1.1),
    ]
    body = line_silk(1.40, -0.85, 5.10, -0.85, "F.SilkS", 0.15)
    body += line_silk(1.40, 5.35, 5.10, 5.35, "F.SilkS", 0.15)
    body += line_silk(-0.35, 1.40, -0.35, 3.10, "F.SilkS", 0.15)
    body += line_silk(6.85, 1.40, 6.85, 3.10, "F.SilkS", 0.15)
    body += '  (fp_circle (center 3.25 2.25) (end 4.75 2.25) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))\n'
    body += rect_lines(-1.2, -1.2, 7.7, 5.7, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Button_Switch_THT.3dshapes/SW_PUSH_6mm.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "SW_Tact_6mm.kicad_mod"),
          fp_wrap("SW_Tact_6mm", "THT 6x6mm Tactile Pushbutton Switch",
                  (3.25, -2.2), (3.25, 6.6), body, "".join(pads), models))

    # 15. LED 3mm Green, Blue, Red, Yellow & Amber (silkscreen bars/arc & K mark clear of pads)
    for color, step_file in [
        ("Green", "LED_D3.0mm_Green.step"),
        ("Blue", "LED_D3.0mm_Blue.step"),
        ("Red", "LED_D3.0mm.step"),
        ("Yellow", "LED_D3.0mm_Yellow.step"),
        ("Amber", "LED_D3.0mm_Yellow.step"),
    ]:
        pads = [pad_th(1, 0.0, 0.0, "rect", 1.7, 1.7, 0.9), pad_th(2, 2.54, 0.0, "circle", 1.7, 1.7, 0.9)]
        body = line_silk(-0.40, -1.65, 3.00, -1.65, "F.SilkS", 0.15)
        body += line_silk(-0.40, 1.65, 3.00, 1.65, "F.SilkS", 0.15)
        body += '  (fp_arc (start 3.00 -1.65) (mid 3.85 0.00) (end 3.00 1.65) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))\n'
        body += line_silk(-1.15, -1.20, -1.15, 1.20, "F.SilkS", 0.22)  # cathode flat bar
        body += '  (fp_text user "K" (at -2.05 0.00) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.15))))\n'
        body += rect_lines(-2.5, -2.1, 4.1, 2.1, "F.CrtYd", 0.05)
        models = model_entry(f"${{KICAD9_3DMODEL_DIR}}/LED_THT.3dshapes/{step_file}", 0, 0, 0)
        write(os.path.join(FP_DIR, f"LED_D3mm_{color}.kicad_mod"),
              fp_wrap(f"LED_D3mm_{color}", f"THT 3mm {color} LED (Pin 1=K, Pin 2=A)",
                      (1.27, -2.8), (1.27, 2.8), body, "".join(pads), models))

    # 16. M3 Mounting Hole (NPTH)
    m3 = """(footprint "MountingHole_M3" (version 20240108) (generator "ge_engine_gen")
  (layer "F.Cu")
  (descr "M3 Mounting Hole 3.2mm NPTH")
  (attr board_only exclude_from_pos_files exclude_from_bom)
  (fp_text reference "REF**" (at 0 -3.4) (layer "F.SilkS") (effects (font (size 0.80 0.80) (thickness 0.15))))
  (fp_text value "M3" (at 0 3.4) (layer "F.SilkS") (effects (font (size 0.80 0.80) (thickness 0.15))))
  (fp_circle (center 0 0) (end 2.8 0) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))
  (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers "*.Cu" "*.Mask"))
)
"""
    write(os.path.join(FP_DIR, "MountingHole_M3.kicad_mod"), m3)

    # 17. TO-220-3 Vertical (Rev 2.0 L7812CV Linear Regulator)
    pads = [
        pad_th(1, 0.00, 0.00, "rect", 1.8, 1.8, 1.05),
        pad_th(2, 2.54, 0.00, "circle", 1.8, 1.8, 1.05),
        pad_th(3, 5.08, 0.00, "circle", 1.8, 1.8, 1.05),
    ]
    body = rect_lines(-2.57, -3.26, 7.65, 1.36, "F.SilkS", 0.15)
    body += line_silk(-2.57, -2.20, 7.65, -2.20, "F.SilkS", 0.25)  # metal tab bar on -Y side
    body += rect_lines(-2.75, -3.45, 7.85, 1.55, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Package_TO_SOT_THT.3dshapes/TO-220-3_Vertical.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "TO-220-3_Vertical.kicad_mod"),
          fp_wrap("TO-220-3_Vertical", "THT TO-220-3 Vertical Package (L7812CV)",
                  (2.54, -4.3), (2.54, 2.5), body, "".join(pads), models))

    # 18. Radial Inductor 5.00mm pitch (Rev 2.0 22uH Boost Filter Choke, Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "circle", 1.8, 1.8, 1.0), pad_th(2, 5.0, 0.0, "circle", 1.8, 1.8, 1.0)]
    body = '  (fp_circle (center 2.50 0.00) (end 6.50 0.00) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))\n'
    body += rect_lines(-1.8, -4.3, 6.8, 4.3, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Inductor_THT.3dshapes/L_Radial_D7.8mm_P5.00mm_Fastron_07HCP.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "L_Radial_P5.00mm.kicad_mod"),
          fp_wrap("L_Radial_P5.00mm", "THT Radial Inductor D7.8mm P5.00mm",
                  (2.5, -4.9), (2.5, 4.9), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 19. Signal Diode DO-35 7.62mm pitch (Rev 2.0 1N3595 Low-Leakage Clamp, Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "rect", 1.6, 1.6, 0.8), pad_th(2, 7.62, 0.0, "circle", 1.6, 1.6, 0.8)]
    body = rect_lines(1.40, -1.10, 6.22, 1.10, "F.SilkS", 0.15)
    body += line_silk(2.20, -1.10, 2.20, 1.10, "F.SilkS", 0.30)  # cathode band
    body += rect_lines(-1.0, -1.5, 8.62, 1.5, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Diode_THT.3dshapes/D_DO-35_SOD27_P7.62mm_Horizontal.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "D_DO-35_P7.62mm.kicad_mod"),
          fp_wrap("D_DO-35_P7.62mm", "THT DO-35 Axial Signal Diode P7.62mm (Pin 1=K, Pin 2=A)",
                  (3.81, -1.95), (3.81, 1.95), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 20. Zener / Power Diode DO-41 10.16mm pitch (Rev 2.0 1N4743A 13V Clamp, Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "rect", 1.8, 1.8, 0.95), pad_th(2, 10.16, 0.0, "circle", 1.8, 1.8, 0.95)]
    body = rect_lines(1.80, -1.40, 8.36, 1.40, "F.SilkS", 0.15)
    body += line_silk(2.80, -1.40, 2.80, 1.40, "F.SilkS", 0.32)  # cathode band
    body += rect_lines(-1.1, -1.7, 11.26, 1.7, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Diode_THT.3dshapes/D_DO-41_SOD81_P10.16mm_Horizontal.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "D_DO-41_P10.16mm.kicad_mod"),
          fp_wrap("D_DO-41_P10.16mm", "THT DO-41 Axial Diode P10.16mm (Pin 1=K, Pin 2=A)",
                  (5.08, -2.20), (5.08, 2.20), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 21. Resettable PTC Fuse 5.00mm pitch (Rev 2.0 RXEF017, Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "circle", 1.7, 1.7, 0.9), pad_th(2, 5.0, 0.0, "circle", 1.7, 1.7, 0.9)]
    body = rect_lines(1.20, -1.30, 3.80, 1.30, "F.SilkS", 0.15)
    body += rect_lines(-1.05, -1.7, 6.05, 1.7, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Capacitor_THT.3dshapes/C_Disc_D5.0mm_W2.5mm_P5.00mm.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "Fuse_PTC_P5.00mm.kicad_mod"),
          fp_wrap("Fuse_PTC_P5.00mm", "THT Radial PTC Resettable Fuse P5.00mm",
                  (2.5, -2.10), (2.5, 2.10), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 22. Rev 3.0 Dual 1.9" ST7789 IPS Displays (Curve Tracer & DMM) on 1x08 Female Sockets
    for fp_name, wrl_name, descr in [
        ("Socket_TFT_1_9_Curve", "TFT_1_9_ST7789_Curve.wrl", "1.9in ST7789 IPS Curve Tracer Display on 1x08 female socket"),
        ("Socket_TFT_1_9_DMM", "TFT_1_9_ST7789_DMM.wrl", "1.9in ST7789 IPS Multimeter Display on 1x08 female socket"),
    ]:
        pads = [pad_th(i + 1, 0.0, i * 2.54, "rect" if i == 0 else "circle", 1.7, 1.7, 1.0) for i in range(8)]
        body = rect_lines(-2.0, -5.6, 60.0, 23.4, "F.SilkS", 0.18)
        body += rect_lines(-2.3, -5.9, 60.3, 23.7, "F.CrtYd", 0.05)
        body += '  (fp_text user "1.9in ST7789 (1x08 SKT)" (at 30.0 8.89) (layer "F.SilkS") (effects (font (size 1.0 1.0) (thickness 0.15))))\n'
        models = (
            model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x08_P2.54mm_Vertical.step", 0, 0, 0) +
            model_entry(f"${{KIPRJMOD}}/3dmodels/{wrl_name}", 0, 0, 0)
        )
        write(os.path.join(FP_DIR, f"{fp_name}.kicad_mod"),
              fp_wrap(fp_name, descr, (29.0, -7.2), (29.0, 25.0), body, "".join(pads), models))

    # 23. Rev 3.0 & Rev 4.0 12x12mm Tactile Pushbuttons (Yellow, Red, Blue, Dark)
    pads = [
        pad_th(1, 0.0, 0.0, "circle", 2.1, 2.1, 1.1),
        pad_th(1, 12.5, 0.0, "circle", 2.1, 2.1, 1.1),
        pad_th(2, 0.0, 5.0, "circle", 2.1, 2.1, 1.1),
        pad_th(2, 12.5, 5.0, "circle", 2.1, 2.1, 1.1),
    ]
    body = rect_lines(1.5, -3.5, 11.0, 8.5, "F.SilkS", 0.15)
    body += rect_lines(-1.4, -3.8, 13.9, 8.8, "F.CrtYd", 0.05)
    for cap_color in ("Yellow", "Red", "Blue", "Dark"):
        models = model_entry(f"${{KIPRJMOD}}/3dmodels/SW_12mm_{cap_color}Cap.wrl", 0, 0, 0)
        write(os.path.join(FP_DIR, f"SW_Tact_12mm_{cap_color}.kicad_mod"),
              fp_wrap(f"SW_Tact_12mm_{cap_color}", f"THT 12x12mm Tactile Pushbutton with {cap_color} Cap",
                      (6.25, -4.8), (6.25, 9.8), body, "".join(pads), models))

    # 24. Rev 3.0 Alps EC11 Rotary Encoder with Fluted Instrument Knob
    pads = [
        pad_th(1, 0.0, 0.0, "rect", 1.8, 1.8, 1.0),
        pad_th(2, 0.0, 2.5, "circle", 1.8, 1.8, 1.0),
        pad_th(3, 0.0, 5.0, "circle", 1.8, 1.8, 1.0),
        pad_th(4, 14.5, 5.0, "circle", 1.8, 1.8, 1.0),
        pad_th(5, 14.5, 0.0, "circle", 1.8, 1.8, 1.0),
        '  (pad "" np_thru_hole circle (at 7.50 -3.10) (size 2.20 2.20) (drill 2.20) (layers "*.Cu" "*.Mask"))\n',
        '  (pad "" np_thru_hole circle (at 7.50 8.10) (size 2.20 2.20) (drill 2.20) (layers "*.Cu" "*.Mask"))\n',
    ]
    body = '  (fp_circle (center 7.50 2.50) (end 20.00 2.50) (stroke (width 0.18) (type solid)) (layer "F.SilkS"))\n'
    body += rect_lines(-5.5, -10.5, 20.5, 15.5, "F.CrtYd", 0.05)
    models = model_entry("${KIPRJMOD}/3dmodels/EC11_Fluted_Knob.wrl", 0, 0, 0)
    write(os.path.join(FP_DIR, "EC11_Knob_Encoder.kicad_mod"),
          fp_wrap("EC11_Knob_Encoder", "Alps EC11 Rotary Encoder with Fluted Instrument Knob",
                  (7.5, -11.8), (7.5, 16.8), body, "".join(pads), models))

    # 25. Rev 3.0 5-Pin 5.08mm Phoenix Screw Terminal Block
    pads = [pad_th(i + 1, i * 5.08, 0.0, "rect" if i == 0 else "circle", 2.6, 2.6, 1.3) for i in range(5)]
    body = rect_lines(-2.6, -4.2, 22.92, 4.2, "F.SilkS", 0.15)
    body += '  (fp_text user "1   2   3  COM 30V" (at 10.16 5.4) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.15))))\n'
    body += rect_lines(-3.0, -4.6, 23.3, 4.6, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/TerminalBlock_Phoenix.3dshapes/TerminalBlock_Phoenix_MKDS-1,5-5-5.08_1x05_P5.08mm_Horizontal.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "TerminalBlock_5Pin_508.kicad_mod"),
          fp_wrap("TerminalBlock_5Pin_508", "THT 5-Pin 5.08mm Screw Terminal Block (TP1..3, COM, 30V)",
                  (10.16, -5.6), (10.16, 6.8), body, "".join(pads), models))

    # 26. Rev 3.0 12mm Piezo/Magnetic Buzzer
    pads = [
        pad_th(1, 0.0, 0.0, "rect", 1.9, 1.9, 1.0),
        pad_th(2, 7.6, 0.0, "circle", 1.9, 1.9, 1.0),
    ]
    body = '  (fp_circle (center 3.80 0.00) (end 10.00 0.00) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))\n'
    body += '  (fp_text user "+" (at -2.0 0.0) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.15))))\n'
    body += rect_lines(-2.6, -6.5, 10.2, 6.5, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Buzzer_Beeper.3dshapes/Buzzer_12x9.5RM7.6.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "Buzzer_12mm.kicad_mod"),
          fp_wrap("Buzzer_12mm", "THT 12mm Buzzer RM7.6mm (Pin 1=+, Pin 2=-)",
                  (3.8, -7.4), (3.8, 7.4), body, "".join(pads), models))

    # 27. Rev 4.0 TP4056 LiPo Charger Breakout on 6 Machined Sockets
    pads = [
        pad_th(1, 0.0, 0.0, "rect", 1.9, 1.9, 1.05),       # IN+
        pad_th(2, 0.0, 12.70, "circle", 1.9, 1.9, 1.05),   # IN-
        pad_th(3, 25.40, 0.0, "circle", 1.9, 1.9, 1.05),   # OUT+
        pad_th(4, 25.40, 5.08, "circle", 1.9, 1.9, 1.05),  # B+
        pad_th(5, 25.40, 7.62, "circle", 1.9, 1.9, 1.05),  # B-
        pad_th(6, 25.40, 12.70, "circle", 1.9, 1.9, 1.05), # OUT-
    ]
    body = rect_lines(-1.60, -2.15, 27.00, 14.85, "F.SilkS", 0.18)
    body += rect_lines(-1.95, -2.50, 27.35, 15.20, "F.CrtYd", 0.05)
    body += '  (fp_text user "TP4056 3.7V LiPo (NO HV!)" (at 12.70 6.35) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.14))))\n'
    body += '  (fp_text user "IN+" (at 2.80 0.00) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))\n'
    body += '  (fp_text user "IN-" (at 2.80 12.70) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))\n'
    body += '  (fp_text user "OUT+" (at 22.10 0.00) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))\n'
    body += '  (fp_text user "B+" (at 22.80 5.08) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))\n'
    body += '  (fp_text user "B-" (at 22.80 7.62) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))\n'
    body += '  (fp_text user "OUT-" (at 22.10 12.70) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))\n'
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 0.0, 0.0, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 0.0, -12.70, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 25.40, 0.0, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 25.40, -5.08, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 25.40, -7.62, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x01_P2.54mm_Vertical.step", 25.40, -12.70, 0) +
        model_entry("${KIPRJMOD}/3dmodels/TP4056_Charger.wrl", 0, 0, 0)
    )
    write(os.path.join(FP_DIR, "Socket_TP4056_Charger.kicad_mod"),
          fp_wrap("Socket_TP4056_Charger", "TP4056 3.7V LiPo Charger on 6x machined sockets",
                  (12.70, -3.50), (12.70, 16.20), body, "".join(pads), models))

    # 28. Rev 4.0 THT SPDT Slide/Toggle Power Switch
    pads = [
        pad_th(1, 0.0, 0.0, "rect", 1.8, 1.8, 1.0),
        pad_th(2, 2.54, 0.0, "circle", 1.8, 1.8, 1.0),
        pad_th(3, 5.08, 0.0, "circle", 1.8, 1.8, 1.0),
    ]
    body = rect_lines(-3.21, -2.75, 8.29, 2.75, "F.SilkS", 0.15)
    body += rect_lines(-3.55, -3.10, 8.65, 3.10, "F.CrtYd", 0.05)
    models = model_entry("${KIPRJMOD}/3dmodels/SW_Slide_SPDT.wrl", 0, 0, 0)
    write(os.path.join(FP_DIR, "SW_Slide_SPDT_THT.kicad_mod"),
          fp_wrap("SW_Slide_SPDT_THT", "THT SPDT Slide Power Switch (1=A, 2=COM, 3=B)",
                  (2.54, -3.80), (2.54, 3.80), body, "".join(pads), models))

    # 29. Rev 4.0 3A Schottky Power Diode DO-201AD 12.70mm pitch (1N5822, Value on F.SilkS!)
    pads = [pad_th(1, 0.0, 0.0, "rect", 2.2, 2.2, 1.2), pad_th(2, 12.70, 0.0, "circle", 2.2, 2.2, 1.2)]
    body = rect_lines(2.00, -2.40, 10.70, 2.40, "F.SilkS", 0.15)
    body += line_silk(3.20, -2.40, 3.20, 2.40, "F.SilkS", 0.35)  # cathode bar
    body += rect_lines(-1.50, -2.75, 14.20, 2.75, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Diode_THT.3dshapes/D_DO-201AD_P12.70mm_Horizontal.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "D_DO-201AD_P12.7mm.kicad_mod"),
          fp_wrap("D_DO-201AD_P12.7mm", "THT DO-201AD 3A Schottky Diode P12.70mm (Pin 1=K, Pin 2=A)",
                  (6.35, -3.25), (6.35, 3.25), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 30. Rev 5.0 DO-15 Axial TVS Diode 10.16mm pitch (P6KE15A)
    pads = [pad_th(1, 0.0, 0.0, "rect", 1.8, 1.8, 0.95), pad_th(2, 10.16, 0.0, "circle", 1.8, 1.8, 0.95)]
    body = rect_lines(1.80, -1.65, 8.36, 1.65, "F.SilkS", 0.15)
    body += line_silk(2.80, -1.65, 2.80, 1.65, "F.SilkS", 0.32)
    body += rect_lines(-1.1, -1.9, 11.26, 1.9, "F.CrtYd", 0.05)
    models = model_entry("${KICAD9_3DMODEL_DIR}/Diode_THT.3dshapes/D_DO-15_P10.16mm_Horizontal.step", 0, 0, 0)
    write(os.path.join(FP_DIR, "D_DO-15_P10.16mm.kicad_mod"),
          fp_wrap("D_DO-15_P10.16mm", "THT DO-15 Axial TVS Diode P10.16mm (Pin 1=K, Pin 2=A)",
                  (5.08, -2.40), (5.08, 2.40), body, "".join(pads), models,
                  ref_size=0.80, val_layer="F.SilkS", val_size=0.80))

    # 31. Rev 5.0 ESP32-C3 SuperMini Socketed (2x 1x08 female sockets at 15.24mm spacing + module 3D model)
    pads = []
    for i in range(8):
        sh = "rect" if i == 0 else "circle"
        pads.append(pad_th(i + 1, 0.0, i * 2.54, sh, 1.7, 1.7, 1.0))
    for i in range(8):
        pads.append(pad_th(9 + i, 15.24, i * 2.54, "circle", 1.7, 1.7, 1.0))
    body = rect_lines(-1.40, -3.50, 16.64, 20.50, "F.SilkS", 0.18)
    body += rect_lines(-1.70, -3.80, 16.94, 20.80, "F.CrtYd", 0.05)
    body += '  (fp_text user "USB-C" (at 7.62 -1.8) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.14))))\n'
    body += '  (fp_text user "C3 ANT" (at 7.62 18.8) (layer "F.SilkS") (effects (font (size 0.85 0.85) (thickness 0.14))))\n'
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x08_P2.54mm_Vertical.step", 0, 0, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x08_P2.54mm_Vertical.step", 15.24, 0, 0) +
        model_entry("${KIPRJMOD}/3dmodels/ESP32_C3_SuperMini.wrl", 0, 0, 0)
    )
    write(os.path.join(FP_DIR, "Socket_ESP32_C3_SuperMini.kicad_mod"),
          fp_wrap("Socket_ESP32_C3_SuperMini", "ESP32-C3 SuperMini on 2x 1x08 female sockets (15.24mm row spacing)",
                  (7.62, -4.90), (7.62, 22.00), body, "".join(pads), models))

    # 32. Rev 5.0 TI BQ25895 1.5MHz 3A NVDC Switching Charger Module on 2x 1x06 Female Sockets (15.24mm spacing)
    pads = []
    for i in range(6):
        sh = "rect" if i == 0 else "circle"
        pads.append(pad_th(i + 1, 0.0, i * 2.54, sh, 1.85, 1.85, 1.05))
    for i in range(6):
        pads.append(pad_th(7 + i, 15.24, i * 2.54, "circle", 1.75, 1.75, 1.00))
    body = rect_lines(-2.38, -4.50, 17.62, 19.50, "F.SilkS", 0.18)
    body += rect_lines(-2.70, -4.85, 17.95, 19.85, "F.CrtYd", 0.05)
    body += '  (fp_text user "BQ25895 NVDC 4.192V" (at 7.62 6.35 90) (layer "F.SilkS") (effects (font (size 0.80 0.80) (thickness 0.14))))\n'
    models = (
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x06_P2.54mm_Vertical.step", 0, 0, 0) +
        model_entry("${KICAD9_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x06_P2.54mm_Vertical.step", 15.24, 0, 0) +
        model_entry("${KIPRJMOD}/3dmodels/BQ25895_Charger.wrl", 0, 0, 0)
    )
    write(os.path.join(FP_DIR, "Socket_BQ25895_Charger.kicad_mod"),
          fp_wrap("Socket_BQ25895_Charger", "TI BQ25895 1.5MHz NVDC Fast Charger on 2x 1x06 female sockets",
                  (7.62, -5.80), (7.62, 20.80), body, "".join(pads), models))

    # 33. Rev 5.0 THT DPDT Slide/Toggle Master Power Switch (6 pins: 1..3 at y=0, 4..6 at y=2.54)
    pads = [
        pad_th(1, 0.00, 0.00, "rect", 1.8, 1.8, 1.0),
        pad_th(2, 2.54, 0.00, "circle", 1.8, 1.8, 1.0),
        pad_th(3, 5.08, 0.00, "circle", 1.8, 1.8, 1.0),
        pad_th(4, 0.00, 2.54, "circle", 1.8, 1.8, 1.0),
        pad_th(5, 2.54, 2.54, "circle", 1.8, 1.8, 1.0),
        pad_th(6, 5.08, 2.54, "circle", 1.8, 1.8, 1.0),
    ]
    body = rect_lines(-3.46, -2.50, 8.54, 5.04, "F.SilkS", 0.15)
    body += rect_lines(-3.80, -2.85, 8.90, 5.40, "F.CrtYd", 0.05)
    models = model_entry("${KIPRJMOD}/3dmodels/SW_Slide_DPDT.wrl", 0, 0, 0)
    write(os.path.join(FP_DIR, "SW_Slide_DPDT_THT.kicad_mod"),
          fp_wrap("SW_Slide_DPDT_THT", "THT DPDT Slide Master Power Switch >=2A (Pole1: 1-2-3, Pole2: 4-5-6)",
                  (2.54, -3.55), (2.54, 6.10), body, "".join(pads), models))

    # 34. Rev 5.0 Star-Ground Bridge 0-Ohm Axial Jumper
    write(os.path.join(FP_DIR, "Star_Bridge_0R.kicad_mod"),
          open(os.path.join(FP_DIR, "R_Axial_P7.62mm.kicad_mod")).read().replace("R_Axial_P7.62mm", "Star_Bridge_0R"))

    print("Generated all footprints in", FP_DIR)


def sym_pin(ptype, x, y, angle, length, name, num):
    return f'        (pin {ptype} line (at {x:.2f} {y:.2f} {angle}) (length {length:.2f}) (name "{name}" (effects (font (size 1.016 1.016)))) (number "{num}" (effects (font (size 1.016 1.016)))))\n'


def make_box_symbol(name, ref, val, fp, left_pins, right_pins, w=15.24):
    """Build a clean rectangular KiCad 9 schematic symbol."""
    n = max(len(left_pins), len(right_pins))
    h = max(5.08, (n + 1) * 2.54)
    y_top = (n - 1) * 1.27
    lines = [
        f'  (symbol "{name}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)',
        f'    (property "Reference" "{ref}" (at 0 {h/2 + 2.54:.2f} 0) (effects (font (size 1.27 1.27))))',
        f'    (property "Value" "{val}" (at 0 {-h/2 - 2.54:.2f} 0) (effects (font (size 1.27 1.27))))',
        f'    (property "Footprint" "{fp}" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))',
        f'    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))',
        f'    (symbol "{name}_0_1"',
        f'      (rectangle (start {-w/2:.2f} {h/2:.2f}) (end {w/2:.2f} {-h/2:.2f}) (stroke (width 0.254) (type default)) (fill (type background)))',
        f'    )',
        f'    (symbol "{name}_1_1"',
    ]
    for i, (num, pname, ptype) in enumerate(left_pins):
        py = y_top - i * 2.54
        lines.append(sym_pin(ptype, -w/2 - 2.54, py, 0, 2.54, pname, str(num)))
    for i, (num, pname, ptype) in enumerate(right_pins):
        py = y_top - i * 2.54
        lines.append(sym_pin(ptype, w/2 + 2.54, py, 180, 2.54, pname, str(num)))
    lines.append('    )')
    lines.append('  )')
    return "\n".join(lines) + "\n"


def make_symbols():
    syms = ['(kicad_symbol_lib (version 20231120) (generator "ge_engine_gen")\n']

    # WEMOS LOLIN32 Lite V1.0.0 (exact verified 26-pin silkscreen!)
    esp_left = [
        (1, "VP/36", "passive"), (2, "VN/39", "passive"), (3, "EN", "passive"),
        (4, "IO34", "passive"), (5, "IO35", "passive"), (6, "IO32", "passive"),
        (7, "IO33", "passive"), (8, "DAC1/25", "passive"), (9, "DAC2/26", "passive"),
        (10, "IO27", "passive"), (11, "IO14", "passive"), (12, "IO12", "passive"),
        (13, "GND", "passive"),
    ]
    esp_right = [
        (14, "3V3", "passive"), (15, "IO22", "passive"), (16, "IO19/SDA", "passive"),
        (17, "IO23/SCL", "passive"), (18, "IO18", "passive"), (19, "IO5", "passive"),
        (20, "IO17", "passive"), (21, "IO16", "passive"), (22, "IO4", "passive"),
        (23, "IO0", "passive"), (24, "IO2", "passive"), (25, "IO15", "passive"),
        (26, "IO13", "passive"),
    ]
    syms.append(make_box_symbol("Wemos_Lolin32_Lite", "U", "WEMOS_LOLIN32_Lite",
                                "Ge_Engine:Socket_Wemos_Lolin32_Lite", esp_left, esp_right, w=20.32))

    # ADS1115 Breakout (exact 10-pin order)
    ads_left = [
        (1, "VDD", "passive"), (2, "GND", "passive"), (3, "SCL", "passive"),
        (4, "SDA", "passive"), (5, "ADDR", "passive"), (6, "ALRT", "passive"),
    ]
    ads_right = [
        (7, "A0", "passive"), (8, "A1", "passive"), (9, "A2", "passive"), (10, "A3", "passive"),
    ]
    syms.append(make_box_symbol("ADS1115_Module", "MOD", "ADS1115_Breakout",
                                "Ge_Engine:Socket_ADS1115_Module", ads_left, ads_right, w=17.78))

    # MT3608 Boost Module
    syms.append(make_box_symbol("MT3608_Boost", "MOD", "MT3608_Boost_12V",
                                "Ge_Engine:Socket_MT3608_Boost",
                                [(1, "IN+", "passive"), (2, "IN-", "passive")],
                                [(3, "OUT+", "passive"), (4, "OUT-", "passive")], w=15.24))

    # OLED 0.96 I2C
    syms.append(make_box_symbol("OLED_096_I2C", "MOD", "OLED_0.96_SSD1306",
                                "Ge_Engine:Socket_OLED_096",
                                [(1, "GND", "passive"), (2, "VCC", "passive"),
                                 (3, "SCL", "passive"), (4, "SDA", "passive")],
                                [], w=15.24))

    # DG408DJ 8:1 High-Voltage CMOS Analog Mux (DIP-16 exact datasheet pinout!)
    dg_left = [
        (1, "A0", "passive"), (16, "A1", "passive"), (15, "A2", "passive"),
        (2, "EN", "passive"), (13, "V+", "passive"), (3, "V-", "passive"),
        (14, "GND", "passive"), (8, "D_COM", "passive"),
    ]
    dg_right = [
        (4, "S1", "passive"), (5, "S2", "passive"), (6, "S3", "passive"),
        (7, "S4", "passive"), (12, "S5", "passive"), (11, "S6", "passive"),
        (10, "S7", "passive"), (9, "S8", "passive"),
    ]
    syms.append(make_box_symbol("DG408DJ", "U", "DG408DJ",
                                "Ge_Engine:Socket_DIP-16_W7.62mm", dg_left, dg_right, w=17.78))

    # DG412LDJ Quad SPST High-Voltage CMOS Analog Switch (DIP-16)
    dg412_left = [
        (1, "IN1", "passive"), (16, "IN2", "passive"), (9, "IN3", "passive"),
        (8, "IN4", "passive"), (12, "VL", "passive"), (13, "V+", "passive"),
        (4, "V-", "passive"), (5, "GND", "passive"),
    ]
    dg412_right = [
        (2, "D1", "passive"), (3, "S1", "passive"), (15, "D2", "passive"),
        (14, "S2", "passive"), (10, "D3", "passive"), (11, "S3", "passive"),
        (7, "D4", "passive"), (6, "S4", "passive"),
    ]
    syms.append(make_box_symbol("DG412LDJ", "U", "DG412LDJ",
                                "Ge_Engine:Socket_DIP-16_W7.62mm", dg412_left, dg412_right, w=17.78))

    # DG409DJ Dual 4:1 High-Voltage CMOS Analog Mux (DIP-16)
    dg409_left = [
        (1, "A0", "passive"), (16, "A1", "passive"), (2, "EN", "passive"),
        (14, "V+", "passive"), (3, "V-", "passive"), (15, "GND", "passive"),
        (8, "DA", "passive"), (9, "DB", "passive"),
    ]
    dg409_right = [
        (4, "S1A", "passive"), (5, "S2A", "passive"), (6, "S3A", "passive"),
        (7, "S4A", "passive"), (13, "S1B", "passive"), (12, "S2B", "passive"),
        (11, "S3B", "passive"), (10, "S4B", "passive"),
    ]
    syms.append(make_box_symbol("DG409DJ", "U", "DG409DJ",
                                "Ge_Engine:Socket_DIP-16_W7.62mm", dg409_left, dg409_right, w=17.78))

    # MCP4822-E/P Dual 12-Bit SPI DAC with Internal VREF (DIP-8)
    dac_left = [
        (1, "VDD", "passive"), (2, "CS", "passive"), (3, "SCK", "passive"),
        (4, "SDI", "passive"),
    ]
    dac_right = [
        (8, "VOUTA", "passive"), (6, "VOUTB", "passive"), (5, "LDAC", "passive"),
        (7, "VSS", "passive"),
    ]
    syms.append(make_box_symbol("MCP4822", "U", "MCP4822-E/P",
                                "Ge_Engine:Socket_DIP-8_W7.62mm", dac_left, dac_right, w=17.78))

    # 74HC595N Shift Register (DIP-16)
    sr_left = [
        (14, "SER", "passive"), (11, "SRCLK", "passive"), (12, "RCLK", "passive"),
        (13, "OE_N", "passive"), (10, "SRCLR", "passive"), (16, "VCC", "passive"),
        (8, "GND", "passive"), (9, "QH'", "passive"),
    ]
    sr_right = [
        (15, "QA", "passive"), (1, "QB", "passive"), (2, "QC", "passive"),
        (3, "QD", "passive"), (4, "QE", "passive"), (5, "QF", "passive"),
        (6, "QG", "passive"), (7, "QH", "passive"),
    ]
    syms.append(make_box_symbol("74HC595N", "U", "74HC595N",
                                "Ge_Engine:Socket_DIP-16_W7.62mm", sr_left, sr_right, w=17.78))

    # TLC274CN Quad CMOS Op-Amp (DIP-14)
    tlc_left = [
        (3, "1IN+", "passive"), (2, "1IN-", "passive"), (5, "2IN+", "passive"),
        (6, "2IN-", "passive"), (10, "3IN+", "passive"), (9, "3IN-", "passive"),
        (12, "4IN+", "passive"), (13, "4IN-", "passive"),
    ]
    tlc_right = [
        (1, "1OUT", "passive"), (7, "2OUT", "passive"), (8, "3OUT", "passive"),
        (14, "4OUT", "passive"), (4, "VDD", "passive"), (11, "GND", "passive"),
    ]
    syms.append(make_box_symbol("TLC274CN", "U", "TLC274CN",
                                "Ge_Engine:Socket_DIP-14_W7.62mm", tlc_left, tlc_right, w=17.78))
    syms.append(make_box_symbol("LMC6484IN", "U", "LMC6484IN",
                                "Ge_Engine:Socket_DIP-14_W7.62mm", tlc_left, tlc_right, w=17.78))

    # LM358N Dual Op-Amp (DIP-8) & TLC2272ACP Rail-to-Rail Dual CMOS Op-Amp (DIP-8)
    lm_left = [
        (3, "1IN+", "passive"), (2, "1IN-", "passive"), (5, "2IN+", "passive"),
        (6, "2IN-", "passive"),
    ]
    lm_right = [
        (1, "1OUT", "passive"), (7, "2OUT", "passive"), (8, "VCC", "passive"),
        (4, "GND", "passive"),
    ]
    syms.append(make_box_symbol("LM358N", "U", "LM358N",
                                "Ge_Engine:Socket_DIP-8_W7.62mm", lm_left, lm_right, w=15.24))
    syms.append(make_box_symbol("TLC2272ACP", "U", "TLC2272ACP",
                                "Ge_Engine:Socket_DIP-8_W7.62mm", lm_left, lm_right, w=15.24))

    # ZIF-14 DUT Socket
    zif_l = [(i + 1, f"P{i+1}", "passive") for i in range(7)]
    zif_r = [(14 - i, f"P{14-i}", "passive") for i in range(7)]
    syms.append(make_box_symbol("ZIF_14Pin", "J", "ZIF-14_DUT",
                                "Ge_Engine:Socket_ZIF_14Pin", zif_l, zif_r, w=12.70))

    # TO-92 N-MOSFET TN0702N3-G (Pin 1=S, 2=G, 3=D)
    syms.append(make_box_symbol("TN0702_NMOS", "Q", "TN0702N3-G",
                                "Ge_Engine:Socket_TO92_3Pin",
                                [(2, "G", "passive")],
                                [(3, "D", "passive"), (1, "S", "passive")], w=10.16))

    # TO-92 PNP BJT 2N3906 (Pin 1=E, 2=B, 3=C)
    syms.append(make_box_symbol("PNP_2N3906", "Q", "2N3906",
                                "Ge_Engine:Socket_TO92_3Pin",
                                [(2, "B", "passive")],
                                [(1, "E", "passive"), (3, "C", "passive")], w=10.16))

    # L78L09ACZ TO-92 (+9V Reg: Pin 1=OUT, 2=GND, 3=IN)
    syms.append(make_box_symbol("L78L09_TO92", "U", "L78L09ACZ",
                                "Ge_Engine:Socket_TO92_3Pin",
                                [(3, "IN", "passive")],
                                [(1, "OUT", "passive"), (2, "GND", "passive")], w=12.70))

    # L7812CV TO-220 (+12V Reg: Pin 1=IN, 2=GND, 3=OUT)
    syms.append(make_box_symbol("L7812_TO220", "U", "L7812CV",
                                "Ge_Engine:TO-220-3_Vertical",
                                [(1, "IN", "passive")],
                                [(3, "OUT", "passive"), (2, "GND", "passive")], w=12.70))

    # DS18B20 Temp Probe Socket (Pin 1=GND, 2=DQ, 3=VDD)
    syms.append(make_box_symbol("DS18B20_Socket", "J", "DS18B20",
                                "Ge_Engine:Socket_TO92_3Pin",
                                [(1, "GND", "passive"), (2, "DQ", "passive"), (3, "VDD", "passive")],
                                [], w=12.70))

    # Generic Connectors 1x02, 1x03, 1x04
    for n in (2, 3, 4):
        syms.append(make_box_symbol(f"Conn_1x0{n}", "J", f"Conn_1x0{n}",
                                    f"Ge_Engine:Header_1x0{n}_P2.54mm",
                                    [(i + 1, f"P{i+1}", "passive") for i in range(n)],
                                    [], w=10.16))

    # 2-pin passives: R, C, CP, L, L_RAD, SW, LED, D, DZ, F_PTC
    for sname, ref, val, fp, p1, p2 in [
        ("R_THT", "R", "R", "Ge_Engine:R_Axial_P7.62mm", "1", "2"),
        ("C_THT", "C", "100nF", "Ge_Engine:C_Disc_P5.00mm", "1", "2"),
        ("CP_THT", "C", "100uF", "Ge_Engine:CP_Radial_D8_P3.5mm", "+", "-"),
        ("L_THT", "L", "100uH", "Ge_Engine:L_Axial_P12.7mm", "1", "2"),
        ("L_RAD_THT", "L", "22uH", "Ge_Engine:L_Radial_P5.00mm", "1", "2"),
        ("SW_THT", "SW", "SW_PUSH", "Ge_Engine:SW_Tact_6mm", "1", "2"),
        ("LED_THT", "D", "LED", "Ge_Engine:LED_D3mm_Green", "K", "A"),
        ("D_THT", "D", "1N3595", "Ge_Engine:D_DO-35_P7.62mm", "K", "A"),
        ("DZ_THT", "D", "1N4743A", "Ge_Engine:D_DO-41_P10.16mm", "K", "A"),
        ("F_PTC_THT", "F", "RXEF017", "Ge_Engine:Fuse_PTC_P5.00mm", "1", "2"),
    ]:
        syms.append(make_box_symbol(sname, ref, val, fp,
                                    [(1, p1, "passive")], [(2, p2, "passive")], w=7.62))

    # --- Rev 3.0 Front-Panel Console Symbols ---
    # 1.9" ST7789 IPS Display Module (1x08)
    tft_left = [
        (1, "GND", "passive"), (2, "VCC", "passive"), (3, "SCL", "passive"),
        (4, "SDA", "passive"), (5, "RES", "passive"), (6, "DC", "passive"),
        (7, "CS", "passive"), (8, "BLK", "passive"),
    ]
    syms.append(make_box_symbol("TFT_1_9_ST7789", "MOD", "TFT_1.9_ST7789",
                                "Ge_Engine:Socket_TFT_1_9_Curve", tft_left, [], w=15.24))

    # MCP23017-E/SP 16-bit I2C I/O Expander (DIP-28)
    mcp_left = [
        (9, "VDD", "passive"), (10, "VSS", "passive"), (12, "SCL", "passive"),
        (13, "SDA", "passive"), (15, "A0", "passive"), (16, "A1", "passive"),
        (17, "A2", "passive"), (18, "RESET", "passive"), (20, "INTA", "passive"),
        (19, "INTB", "passive"), (11, "NC1", "passive"), (14, "NC2", "passive"),
    ]
    mcp_right = [
        (21, "GPA0", "passive"), (22, "GPA1", "passive"), (23, "GPA2", "passive"),
        (24, "GPA3", "passive"), (25, "GPA4", "passive"), (26, "GPA5", "passive"),
        (27, "GPA6", "passive"), (28, "GPA7", "passive"), (1, "GPB0", "passive"),
        (2, "GPB1", "passive"), (3, "GPB2", "passive"), (4, "GPB3", "passive"),
        (5, "GPB4", "passive"), (6, "GPB5", "passive"), (7, "GPB6", "passive"),
        (8, "GPB7", "passive"),
    ]
    syms.append(make_box_symbol("MCP23017", "U", "MCP23017-E/SP",
                                "Ge_Engine:Socket_DIP-28_W7.62mm", mcp_left, mcp_right, w=20.32))

    # Alps EC11 Rotary Encoder with Push Switch
    syms.append(make_box_symbol("EC11_Encoder", "ENC", "EC11_KNOB",
                                "Ge_Engine:EC11_Knob_Encoder",
                                [(1, "A", "passive"), (2, "C", "passive"), (3, "B", "passive")],
                                [(4, "S1", "passive"), (5, "S2", "passive")], w=12.70))

    # 5-Pin 5.08mm Screw Terminal Block
    tb_left = [
        (1, "TP1", "passive"), (2, "TP2", "passive"), (3, "TP3", "passive"),
        (4, "COM", "passive"), (5, "V30V", "passive"),
    ]
    syms.append(make_box_symbol("TermBlock_5Pin", "J", "TERM_5P_508",
                                "Ge_Engine:TerminalBlock_5Pin_508", tb_left, [], w=12.70))

    # 12mm THT Buzzer
    syms.append(make_box_symbol("Buzzer_THT", "BZ", "BUZZER_12MM",
                                "Ge_Engine:Buzzer_12mm",
                                [(1, "+", "passive")], [(2, "-", "passive")], w=10.16))

    # TO-92 NPN BJT 2N3904 (Pin 1=E, 2=B, 3=C)
    syms.append(make_box_symbol("NPN_2N3904", "Q", "2N3904",
                                "Ge_Engine:Socket_TO92_3Pin",
                                [(2, "B", "passive")],
                                [(3, "C", "passive"), (1, "E", "passive")], w=10.16))

    # --- Rev 4.0 Flagship Power, Protection & UI Symbols ---
    # TP4056 + DW01A 3.7V LiPo Charger Module (6-pin socketed)
    syms.append(make_box_symbol("TP4056_Module", "MOD", "TP4056_3V7_LiPo",
                                "Ge_Engine:Socket_TP4056_Charger",
                                [(1, "IN+", "passive"), (2, "IN-", "passive")],
                                [(3, "OUT+", "passive"), (4, "B+", "passive"),
                                 (5, "B-", "passive"), (6, "OUT-", "passive")], w=17.78))

    # LM1117T-3.3 TO-220-3 LDO Regulator (TI datasheet: 1=GND, 2=OUT, 3=IN)
    syms.append(make_box_symbol("LM1117_TO220", "U", "LM1117T-3.3",
                                "Ge_Engine:TO-220-3_Vertical",
                                [(3, "IN", "passive")],
                                [(2, "OUT", "passive"), (1, "GND", "passive")], w=12.70))

    # NDP6020P Logic-Level P-Channel MOSFET (ON Semi datasheet: 1=G, 2=D, 3=S)
    syms.append(make_box_symbol("PMOS_NDP6020P", "Q", "NDP6020P",
                                "Ge_Engine:TO-220-3_Vertical",
                                [(1, "G", "passive")],
                                [(3, "S", "passive"), (2, "D", "passive")], w=12.70))

    # THT SPDT Slide/Toggle Power Switch (1=A, 2=COM, 3=B)
    syms.append(make_box_symbol("SW_SPDT_THT", "SW", "SW_SPDT",
                                "Ge_Engine:SW_Slide_SPDT_THT",
                                [(2, "COM", "passive")],
                                [(1, "A", "passive"), (3, "B", "passive")], w=12.70))

    # 1N5822 3A Schottky Power Diode DO-201AD (1=K, 2=A)
    syms.append(make_box_symbol("D_SCHOTTKY_3A", "D", "1N5822",
                                "Ge_Engine:D_DO-201AD_P12.7mm",
                                [(1, "K", "passive")],
                                [(2, "A", "passive")], w=7.62))

    # --- Rev 5.0 Dual-MCU, BQ25895 NVDC, MCP1826S, H11L1M & Star-Ground Symbols ---
    # ESP32-C3 SuperMini (16-pin 2x8 @ 15.24mm)
    c3_left = [
        (1, "GPIO5/DC", "passive"), (2, "GPIO6/MOSI", "passive"), (3, "GPIO7/CS1", "passive"),
        (4, "GPIO8/STRP", "passive"), (5, "GPIO9/BOOT", "passive"), (6, "GPIO10/CS2", "passive"),
        (7, "GPIO20/RX", "passive"), (8, "GPIO21/TX", "passive"),
    ]
    c3_right = [
        (9, "5V", "passive"), (10, "GND", "passive"), (11, "3V3", "passive"),
        (12, "GPIO4/SCK", "passive"), (13, "GPIO3/BZ", "passive"), (14, "GPIO2/STRP", "passive"),
        (15, "GPIO1/SCL", "passive"), (16, "GPIO0/SDA", "passive"),
    ]
    syms.append(make_box_symbol("ESP32_C3_SuperMini", "U", "ESP32-C3_SuperMini",
                                "Ge_Engine:Socket_ESP32_C3_SuperMini", c3_left, c3_right, w=20.32))

    # TI BQ25895 1.5MHz 3A NVDC Switching Charger Module (12-pin 2x6 @ 15.24mm)
    bq_left = [
        (1, "VBUS", "passive"), (2, "PMID", "passive"), (3, "GND", "passive"),
        (4, "SYS", "passive"), (5, "BAT", "passive"), (6, "TS", "passive"),
    ]
    bq_right = [
        (7, "SCL", "passive"), (8, "SDA", "passive"), (9, "INT", "passive"),
        (10, "STAT", "passive"), (11, "CE_N", "passive"), (12, "OTG", "passive"),
    ]
    syms.append(make_box_symbol("BQ25895_Module", "MOD", "BQ25895_NVDC_3A",
                                "Ge_Engine:Socket_BQ25895_Charger", bq_left, bq_right, w=17.78))

    # USB-C 6-Pin Breakout (1=VBUS, 2=GND, 3=CC1, 4=CC2, 5=DP, 6=DM)
    syms.append(make_box_symbol("USBC_Breakout_6P", "J", "USBC_5V_3A",
                                "Ge_Engine:Socket_USBC_Breakout",
                                [(1, "VBUS", "passive"), (2, "GND", "passive"), (3, "CC1", "passive"),
                                 (4, "CC2", "passive"), (5, "DP", "passive"), (6, "DM", "passive")],
                                [], w=12.70))

    # MCP1826S-3302E/AB TO-220-3 Ultra-Low-Dropout 3.3V 1A LDO (1=VIN, 2=GND, 3=VOUT)
    syms.append(make_box_symbol("MCP1826S_TO220", "U", "MCP1826S-3302E/AB",
                                "Ge_Engine:TO-220-3_Vertical",
                                [(1, "VIN", "passive")],
                                [(3, "VOUT", "passive"), (2, "GND", "passive")], w=15.24))

    # 2N7000 TO-92 N-Channel MOSFET (1=S, 2=G, 3=D)
    syms.append(make_box_symbol("NMOS_2N7000", "Q", "2N7000",
                                "Ge_Engine:Socket_TO92_3Pin",
                                [(2, "G", "passive")],
                                [(3, "D", "passive"), (1, "S", "passive")], w=10.16))

    # H11L1M DIP-6 High-Speed Schmitt-Trigger Optocoupler (1=A, 2=K, 3=NC, 4=VO, 5=GND, 6=VCC)
    syms.append(make_box_symbol("H11L1M_DIP6", "U", "H11L1M",
                                "Ge_Engine:Socket_DIP-6_W7.62mm",
                                [(1, "A", "passive"), (2, "K", "passive"), (3, "NC", "passive")],
                                [(6, "VCC", "passive"), (4, "VO", "passive"), (5, "GND", "passive")], w=15.24))

    # THT DPDT Slide/Toggle Master Power Switch (Pole 1: 1=OFF1, 2=COM1, 3=ON1; Pole 2: 4=OFF2, 5=COM2, 6=ON2)
    syms.append(make_box_symbol("SW_DPDT_THT", "SW", "SW_DPDT_3A",
                                "Ge_Engine:SW_Slide_DPDT_THT",
                                [(2, "COM1", "passive"), (5, "COM2", "passive")],
                                [(1, "OFF1", "passive"), (3, "ON1", "passive"),
                                 (4, "OFF2", "passive"), (6, "ON2", "passive")], w=15.24))

    # Single-Point Star Ground Bridge 0-Ohm (1=DGND, 2=AGND)
    syms.append(make_box_symbol("Star_Bridge_0R", "R", "0R_STAR_BRIDGE",
                                "Ge_Engine:Star_Bridge_0R",
                                [(1, "DGND", "passive")],
                                [(2, "AGND", "passive")], w=10.16))

    syms.append(")\n")
    write(os.path.join(PROJ_DIR, "Ge_Engine.kicad_sym"), "".join(syms))
    print("Generated Ge_Engine.kicad_sym")


if __name__ == "__main__":
    make_project_files()
    make_footprints()
    make_symbols()
