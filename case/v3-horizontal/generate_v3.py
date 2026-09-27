"""
Case v3 — estacao meteo. Port fiel da fonte OpenSCAD v2 (placas montam na
tampa, BMP581 ao lado, divisoria de calor) com 2 mudancas:
  1. Layout girado 90 graus: tela em PAISAGEM (janela 41.97 x 27.09),
     BMP581 fica acima da tela. Spacers M2 girados junto.
  2. Talho gigante do USB (22 x 18 mm no canto) substituido por ARCO de
     13 x 8.4 mm na parede +X, alinhado com o conector USB-C da placa
     (que aponta +X no layout paisagem, centro y=-17, z~4.2).
Screws: 4x M2x20 (Waveshare), 2x M2x6 (BMP581), 4x M2x8 (tampa).
"""
import numpy as np
import trimesh
from trimesh.creation import box as tbox, cylinder as tcyl
from trimesh.boolean import union, difference

OUT = "/home/pantojinho/estacao_case"

# ------- params (mm) — v2 -------
CASE = 70.0
DEPTH = 22.0
WALL = 2.0
FRONT = 2.2
LID_T = 2.4
SPACER_H = 15.5
R_OUT, R_IN = 6.0, 4.0
DISPLAY_C = (0.0, -17.0)   # v2: (-17, 0) girado 90 CCW
SENSOR_C = (0.0, 18.0)     # v2: (18, 0) girado 90 CCW
WIN_W, WIN_H = 41.97, 27.09     # paisagem (era 26.74+gap x 41.62+gap)
WIN_R = 2.6
SENS_REC_W, SENS_REC_H, SENS_REC_R = 27.0, 30.0, 3.4
VENT_OFFS = (-9.0, -3.0, 3.0, 9.0)
VENT_W, VENT_L = 2.2, 12.0
POST_XY, POST_D = 31.5, 7.0      # postes M3 no corpo
PILOT_D, PILOT_TOP = 2.8, 10.0   # piloto M3 auto-roscante
# USB-C: centro do conector (ponta do PCB em x=19.25, conector ~2mm afora,
# centro y = -17 = centro da tela, z = PCB 2.5..4.1 + conector ~3.2)
USB_C_Y = -17.0
USB_ARC_Z = 2.4               # centro do arco (boca do conector ~Z 2.8..6.5)
USB_W = 14.5                  # largura do arco (plug shell 12.35 + folga)
BAFF = dict(x=(-14.0, 14.0), y=(2.0, 3.6), z=(2.2, 11.7))
# spacers na tampa (posicoes giradas dos furos M2)
WS_SP = [(x, -17.0 + y) for x in (-19.25, 19.25) for y in (-11.43, 11.43)]
BMP_SP = [(x, 24.35) for x in (-10.16, 10.16)]  # furos 2.54 abaixo da borda +Y
TIE_XY = [(-18.0, 3.0), (18.0, 3.0)]


def rprism(w, h, d, r, cx=0.0, cy=0.0, z0=0.0):
    """rounded_prism do scad: hull de 4 cilindros = caixa com cantos redondos."""
    parts = []
    for ox in (-w/2 + r, w/2 - r):
        for oy in (-h/2 + r, h/2 - r):
            c = tcyl(r, d, sections=48)
            c.apply_translation([cx + ox, cy + oy, z0 + d/2])
            parts.append(c)
    return trimesh.util.concatenate(parts).convex_hull


def bx(x0, x1, y0, y1, z0, z1):
    b = tbox((x1-x0, y1-y0, z1-z0))
    b.apply_translation([(x0+x1)/2, (y0+y1)/2, (z0+z1)/2])
    return b


def cylz(cx, cy, d, z0, z1, sec=48):
    c = tcyl(d/2, z1-z0, sections=sec)
    c.apply_translation([cx, cy, (z0+z1)/2])
    return c


# ================= CORPO =================
corpo = rprism(CASE, CASE, DEPTH, R_OUT)
cav = rprism(CASE-2*WALL, CASE-2*WALL, DEPTH-FRONT+0.2, R_IN, z0=FRONT)
corpo = difference([corpo, cav])

# janela paisagem
corpo = difference([corpo, rprism(WIN_W, WIN_H, FRONT+0.3, WIN_R,
                                  cx=DISPLAY_C[0], cy=DISPLAY_C[1], z0=-0.1)])
# recess do BMP (0.7 de profundidade, deixa 1.5 de parede)
corpo = difference([corpo, rprism(SENS_REC_W, SENS_REC_H, 0.8, SENS_REC_R,
                                  cx=SENSOR_C[0], cy=SENSOR_C[1], z0=-0.1)])
# 4 respiros do sensor
for dx in VENT_OFFS:
    corpo = difference([corpo, bx(SENSOR_C[0]+dx-VENT_W/2, SENSOR_C[0]+dx+VENT_W/2,
                                  SENSOR_C[1]-VENT_L/2, SENSOR_C[1]+VENT_L/2,
                                  -0.1, FRONT+0.3)])
# arco USB na parede +X: box z 0..USB_ARC_Z + cilindro horizontal (eixo X)
arch = bx(32.0, 36.0, USB_C_Y-USB_W/2, USB_C_Y+USB_W/2, -0.1, USB_ARC_Z)
arc = tcyl(USB_W/2, 4.2, sections=48)
arc.apply_transform(trimesh.transformations.rotation_matrix(np.pi/2, [0, 1, 0]))
arc.apply_translation([34.0, USB_C_Y, USB_ARC_Z])
arch = union([arch, arc])
corpo = difference([corpo, arch])

# postes M2x8 da tampa + pilotos
for px in (-POST_XY, POST_XY):
    for py in (-POST_XY, POST_XY):
        corpo = union([corpo, cylz(px, py, POST_D, FRONT, DEPTH)])
for px in (-POST_XY, POST_XY):
    for py in (-POST_XY, POST_XY):
        corpo = difference([corpo, cylz(px, py, PILOT_D, DEPTH-PILOT_TOP, DEPTH+0.2)])
# divisoria de calor
corpo = union([corpo, bx(BAFF['x'][0], BAFF['x'][1], BAFF['y'][0], BAFF['y'][1],
                         BAFF['z'][0], BAFF['z'][1])])

# ================= TAMPA (coords locais, montada espelhada) =================
tampa = rprism(CASE, CASE, LID_T, R_OUT)
for sx, sy in WS_SP + BMP_SP:
    tampa = union([tampa, cylz(sx, sy, POST_D, LID_T, LID_T+SPACER_H)])
for sx, sy in WS_SP:
    tampa = difference([tampa, cylz(sx, sy, 2.35, -0.1, LID_T+SPACER_H+0.3)])
    tampa = difference([tampa, cylz(sx, sy, 4.3, -0.1, 1.4)])
for sx, sy in BMP_SP:
    tampa = difference([tampa, cylz(sx, sy, 1.65, LID_T+SPACER_H-6.5, LID_T+SPACER_H+0.2)])
for px in (-POST_XY, POST_XY):
    for py in (-POST_XY, POST_XY):
        tampa = difference([tampa, cylz(px, py, 3.4, -0.1, LID_T+0.3)])
        tampa = difference([tampa, cylz(px, py, 6.0, -0.1, 1.6)])
for tx, ty in TIE_XY:
    tampa = difference([tampa, cylz(tx, ty, 2.2, -0.1, LID_T+0.3)])

# ================= VERIFICACOES =================
for name, m in [("corpo", corpo), ("tampa", tampa)]:
    assert m.is_watertight, f"{name} nao watertight"
    e = m.extents
    print(f"{name}: {e[0]:.2f} x {e[1]:.2f} x {e[2]:.2f} mm | vol {m.volume/1000:.1f} cm3 | tris {len(m.faces)}")

# janela x vidro paisagem (44 x 29.12): bezel >= 0.8/lado
assert (44.0-WIN_W)/2 >= 0.8 and (29.12-WIN_H)/2 >= 0.8
gx0, gx1 = DISPLAY_C[0]-22, DISPLAY_C[0]+22
gy0, gy1 = DISPLAY_C[1]-14.56, DISPLAY_C[1]+14.56
print(f"vidro: x {gx0:.2f}..{gx1:.2f}  y {gy0:.2f}..{gy1:.2f}")
for sx, sy in WS_SP:
    assert -19.25 <= sx <= 19.25 and -28.43 <= sy <= -5.57, f"spacer fora ({sx},{sy})"
for sx, sy in BMP_SP:
    assert -12.7 <= sx <= 12.7 and 9.11 <= sy <= 26.89, f"bmp fora ({sx},{sy})"
print(f"USB: arco y {USB_C_Y-USB_W/2}..{USB_C_Y+USB_W/2} ({USB_W}mm) | z 0..{USB_ARC_Z+USB_W/2:.2f}")
for px in (-POST_XY, POST_XY):
    for py in (-POST_XY, POST_XY):
        assert not (gx0-2 < px < gx1+2 and gy0-2 < py < gy1+2), f"poste no vidro ({px},{py})"
        for sx, sy in WS_SP + BMP_SP:
            assert (px-sx)**2 + (py-sy)**2 > 25, f"poste perto do spacer ({px},{sy})"
print("asserts OK")

# ================= EXPORT =================
corpo.export(f"{OUT}/caixa_integrada_corpo-3.stl")
tampa.export(f"{OUT}/caixa_integrada_tampa-3.stl")

# cena montada (tampa em cima espelhada) + proxies de placa pra render
lid_m = tampa.copy()
lid_m.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0]))
lid_m.apply_translation([0, 0, DEPTH + LID_T])
glass = rprism(44.0, 29.12, 1.0, 3.7, cx=DISPLAY_C[0], cy=DISPLAY_C[1], z0=1.5)
bmp = bx(-12.7, 12.7, 9.11, 26.89, DEPTH - LID_T - SPACER_H - 1.6,
         DEPTH - LID_T - SPACER_H)
scene = trimesh.Scene()
for n, g in [("corpo", corpo), ("tampa", lid_m), ("glass", glass), ("bmp", bmp)]:
    scene.add_geometry(g, node_name=n)
scene.export(f"{OUT}/v3_montada.glb")
print("export OK")
