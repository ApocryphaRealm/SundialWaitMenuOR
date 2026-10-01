"""Sundial Wait Menu - the dial's art, drawn here (no other author's art is used).

Writes four 1024 x 1024 PNGs into dist/.../Plugins/SundialWaitMenu/, all on the same square so they stack exactly:
  Face.png   the rim's faint track and the parchment face: an astrolabe engraving (hour ticks, a dotted ring, a compass
             rose), quiet in the middle where the game's own text sits
  Hour.png   one hour of the rim's bright arc, from the top clockwise (15 degrees); the plugin turns 24 copies
  Marks.png  the day's four marks on the rim in paper medallions: noon (top), sunset (right), midnight (bottom),
             sunrise (left)
  Sun.png    the gilt sun marker at the top of the rim; the plugin turns it to the current hour

Palette: the Oblivion Remastered paper-map white #e6dfd1, brown ink #4a3222, brown hairline #6b4e36, gilt.
Drawn at 4x and reduced, so every edge is smooth. Run: python tools/gen-art.py [--preview out.png]
"""

import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 1024
SS = 4                      # supersampling
S = SIZE * SS
C = S / 2

# geometry, as fractions of the square (the plugin's layout reads none of this - it only stacks the images)
R_RING = 0.440              # the rim the hours run along
W_TRACK = 0.011
W_HOUR = 0.021
R_DISC = 0.385              # the parchment face
W_BORDER = 0.017
R_MEDAL = 0.037
R_SUN = 0.022

PAPER = (230, 223, 209, 255)
PAPER_EDGE = (214, 202, 178, 255)
INK = (74, 50, 34, 255)
HAIR = (107, 78, 54, 255)
TRACK = (128, 121, 112, 150)
ARC = (236, 226, 204, 255)
ARC_EDGE = (107, 78, 54, 200)
GILT = (212, 169, 64, 255)
GILT_DARK = (138, 106, 42, 255)

OUT = Path(__file__).resolve().parent.parent / "dist" / "OblivionRemastered" / "Binaries" / "Win64" / "OBSE" / "Plugins" / "SundialWaitMenu"


def px(f):
    return f * S


def canvas():
    return Image.new("RGBA", (S, S), (0, 0, 0, 0))


def finish(img):
    return img.resize((SIZE, SIZE), Image.LANCZOS)


def circle(d, r, **kw):
    d.ellipse((C - r, C - r, C + r, C + r), **kw)


def polar(r, deg):
    """deg clockwise from the top"""
    a = math.radians(deg)
    return C + r * math.sin(a), C - r * math.cos(a)


def ring_arc(d, r, w, deg0, deg1, fill):
    # PIL measures from 3 o'clock, clockwise
    d.arc((C - r, C - r, C + r, C + r), deg0 - 90, deg1 - 90, fill=fill, width=int(round(w)))


def face():
    img = canvas()
    d = ImageDraw.Draw(img)
    # the rim's faint track
    circle(d, px(R_RING + W_TRACK / 2), outline=TRACK, width=int(px(W_TRACK)))   # PIL draws a width inward: centred on R_RING

    # the parchment: a radial fall-off to the edge, and a fixed paper grain
    disc = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    dd = ImageDraw.Draw(disc)
    steps = 48
    for i in range(steps):
        t = i / (steps - 1)
        r = px(R_DISC) * (1 - t)
        col = tuple(int(PAPER_EDGE[k] + (PAPER[k] - PAPER_EDGE[k]) * min(1.0, t * 1.6)) for k in range(3)) + (255,)
        circle(dd, r, fill=col)
    disc = disc.filter(ImageFilter.GaussianBlur(1.2 * SS))
    img.alpha_composite(disc)
    # the grain on a layer of its own: ImageDraw REPLACES pixels, so flecks drawn straight onto the paper punched
    # see-through holes in it
    grain = canvas()
    dd = ImageDraw.Draw(grain)
    rnd = random.Random(4331)
    for _ in range(9000):
        a = rnd.random() * 2 * math.pi
        rr = math.sqrt(rnd.random()) * px(R_DISC) * 0.985
        x, y = C + rr * math.cos(a), C + rr * math.sin(a)
        s = rnd.choice((2, 3, 4, 6))
        col = (120, 92, 64, rnd.randint(8, 22))   # a warm fleck of the ink, barely there
        dd.ellipse((x - s, y - s, x + s, y + s), fill=col)
    img.alpha_composite(grain.filter(ImageFilter.GaussianBlur(1.0 * SS)))
    d = ImageDraw.Draw(img)

    # the engraving
    eng = canvas()
    e = ImageDraw.Draw(eng)
    line = (74, 50, 34, 120)
    faint = (74, 50, 34, 70)
    circle(e, px(0.356), outline=(107, 78, 54, 200), width=int(px(0.0035)))
    circle(e, px(0.300), outline=faint, width=int(px(0.0022)))
    for h in range(24):
        deg = h * 15
        if h % 6 == 0:
            r0, w = 0.318, 0.0055
        elif h % 3 == 0:
            r0, w = 0.330, 0.0042
        else:
            r0, w = 0.340, 0.0030
        e.line((polar(px(r0), deg), polar(px(0.356), deg)), fill=(74, 50, 34, 210), width=int(px(w)))
        if h % 6 != 0:   # quarter-hour dots between
            pass
    for q in range(96):   # the dotted ring
        if q % 4 == 0:
            continue
        x, y = polar(px(0.272), q * 3.75)
        r = px(0.0026)
        e.ellipse((x - r, y - r, x + r, y + r), fill=faint)

    # the compass rose: long points on the four quarters, short ones between; each point half filled, as engraved
    for k in range(8):
        deg = k * 45
        long_ = k % 2 == 0
        tip = px(0.290 if long_ else 0.190)
        half = 6.5 if long_ else 9.0
        base_r = px(0.045)
        p_tip = polar(tip, deg)
        p_l = polar(base_r, deg - half * 3)
        p_r = polar(base_r, deg + half * 3)
        p_c = (C, C)
        e.polygon([p_c, p_l, p_tip], fill=(74, 50, 34, 46 if long_ else 30))
        e.line([p_c, p_l, p_tip, p_r, p_c], fill=line, width=int(px(0.0024)), joint="curve")
        e.line([p_c, p_tip], fill=line, width=int(px(0.0016)))
    circle(e, px(0.110), outline=faint, width=int(px(0.0022)))
    circle(e, px(0.050), outline=line, width=int(px(0.0022)))
    img.alpha_composite(eng)

    # quiet in the middle: the game's text sits there, so the engraving fades under a wash of paper
    wash = canvas()
    w = ImageDraw.Draw(wash)
    steps = 40
    for i in range(steps):
        t = i / (steps - 1)                       # 0 outside .. 1 centre
        r = px(0.255) * (1 - t) + px(0.02) * t
        a = int(215 * min(1.0, (t / 0.55)) ** 1.4)
        circle(w, r, fill=PAPER[:3] + (a,))
    wash = wash.filter(ImageFilter.GaussianBlur(6 * SS))
    img.alpha_composite(wash)

    # the face's border last, over everything at the edge
    d = ImageDraw.Draw(img)
    circle(d, px(R_DISC), outline=INK, width=int(px(W_BORDER)))
    circle(d, px(R_DISC - W_BORDER - 0.004), outline=HAIR, width=int(px(0.0025)))
    return finish(img)


def hour():
    img = canvas()
    d = ImageDraw.Draw(img)
    # the bright arc centred on the track, a sliver short of each end so the hours read as ticks (PIL draws a width
    # inward from the box, so the box's radius is the band's outer edge)
    ring_arc(d, px(R_RING + W_HOUR / 2 + 0.0025), px(W_HOUR + 0.005), 0.55, 14.45, ARC_EDGE)
    ring_arc(d, px(R_RING + W_HOUR / 2), px(W_HOUR), 0.7, 14.3, ARC)
    return finish(img)


def medallion(d, deg):
    x, y = polar(px(R_RING), deg)
    r = px(R_MEDAL)
    d.ellipse((x - r, y - r, x + r, y + r), fill=PAPER, outline=INK, width=int(px(0.0055)))
    r2 = r - px(0.0085)
    d.ellipse((x - r2, y - r2, x + r2, y + r2), outline=(107, 78, 54, 150), width=int(px(0.0018)))
    return x, y


def rays(d, x, y, r0, r1, n, start, span, width):
    for k in range(n):
        a = math.radians(start + span * k / max(1, n - 1)) if span < 360 else math.radians(start + 360 * k / n)
        d.line((x + r0 * math.sin(a), y - r0 * math.cos(a), x + r1 * math.sin(a), y - r1 * math.cos(a)), fill=INK, width=width)


def marks():
    img = canvas()
    d = ImageDraw.Draw(img)
    lw = int(px(0.0042))
    # noon - a full sun
    x, y = medallion(d, 0)
    r = px(0.011)
    d.ellipse((x - r, y - r, x + r, y + r), fill=INK)
    rays(d, x, y, px(0.016), px(0.023), 8, 0, 360, lw)
    # sunset (right) - a sun half under the horizon, going down
    x, y = medallion(d, 90)
    hy = y + px(0.006)
    r = px(0.013)
    d.pieslice((x - r, hy - r, x + r, hy + r), 180, 360, fill=INK)
    d.line((x - px(0.024), hy, x + px(0.024), hy), fill=INK, width=lw)
    rays(d, x, hy, px(0.017), px(0.022), 5, -60, 120, lw)
    ch = px(0.006)
    d.line((x - ch, hy + px(0.008), x, hy + px(0.008) + ch, x + ch, hy + px(0.008)), fill=INK, width=lw, joint="curve")
    # midnight (bottom) - a crescent and a star
    x, y = medallion(d, 180)
    r = px(0.015)
    moon = Image.new("L", (S, S), 0)
    m = ImageDraw.Draw(moon)
    m.ellipse((x - r, y - r, x + r, y + r), fill=255)
    off = px(0.008)
    m.ellipse((x - r + off, y - r - off * 0.5, x + r + off, y + r - off * 0.5), fill=0)
    ink = Image.new("RGBA", (S, S), INK)
    img.paste(ink, (0, 0), moon)
    d = ImageDraw.Draw(img)
    sx, sy, sr = x + px(0.010), y - px(0.010), px(0.0045)
    d.polygon([(sx, sy - sr), (sx + sr * 0.3, sy - sr * 0.3), (sx + sr, sy), (sx + sr * 0.3, sy + sr * 0.3),
               (sx, sy + sr), (sx - sr * 0.3, sy + sr * 0.3), (sx - sr, sy), (sx - sr * 0.3, sy - sr * 0.3)], fill=INK)
    # sunrise (left) - a sun half over the horizon, coming up
    x, y = medallion(d, 270)
    hy = y + px(0.006)
    r = px(0.013)
    d.pieslice((x - r, hy - r, x + r, hy + r), 180, 360, fill=INK)
    d.line((x - px(0.024), hy, x + px(0.024), hy), fill=INK, width=lw)
    rays(d, x, hy, px(0.017), px(0.022), 5, -60, 120, lw)
    d.line((x - ch, hy + px(0.008) + ch, x, hy + px(0.008), x + ch, hy + px(0.008) + ch), fill=INK, width=lw, joint="curve")
    return finish(img)


def sun():
    img = canvas()
    d = ImageDraw.Draw(img)
    x, y = polar(px(R_RING), 0)
    for k in range(12):
        a = math.radians(k * 30)
        r0, r1 = px(R_SUN * 0.9), px(R_SUN * (1.55 if k % 2 == 0 else 1.3))
        w = px(0.006)
        p = [(x + r1 * math.sin(a), y - r1 * math.cos(a)),
             (x + r0 * math.sin(a + 0.22), y - r0 * math.cos(a + 0.22)),
             (x + r0 * math.sin(a - 0.22), y - r0 * math.cos(a - 0.22))]
        d.polygon(p, fill=GILT, outline=GILT_DARK)
    r = px(R_SUN)
    d.ellipse((x - r, y - r, x + r, y + r), fill=GILT, outline=INK, width=int(px(0.004)))
    r2 = px(R_SUN * 0.55)
    d.ellipse((x - r2, y - r2, x + r2, y + r2), outline=GILT_DARK, width=int(px(0.0025)))
    return finish(img)


def preview(path, start_hour=10.9, hours=3):
    """the dial as the plugin stacks it, over a dark backdrop with stand-in text - for checking the look only"""
    bg = Image.new("RGBA", (SIZE, SIZE), (28, 26, 30, 255))
    bg.alpha_composite(Image.open(OUT / "Face.png"))
    h = Image.open(OUT / "Hour.png")
    a0 = (start_hour - 12) * 15
    for k in range(hours):
        bg.alpha_composite(h.rotate(-(a0 + 15 * k), resample=Image.BICUBIC, center=(SIZE / 2, SIZE / 2)))
    bg.alpha_composite(Image.open(OUT / "Marks.png"))
    bg.alpha_composite(Image.open(OUT / "Sun.png").rotate(-a0, resample=Image.BICUBIC, center=(SIZE / 2, SIZE / 2)))
    bg.save(path)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, fn in (("Face", face), ("Hour", hour), ("Marks", marks), ("Sun", sun)):
        fn().save(OUT / f"{name}.png", optimize=True)
        print("wrote", OUT / f"{name}.png")
    if "--preview" in sys.argv:
        out = sys.argv[sys.argv.index("--preview") + 1]
        preview(out)
        print("preview", out)


if __name__ == "__main__":
    main()
