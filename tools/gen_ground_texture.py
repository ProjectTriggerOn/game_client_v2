#!/usr/bin/env python3
"""Generate the port-concrete texture shipment.map's ground slab uses.

Everything else in the map is an FBX (see tools/blender_build_assets.py); the
ground is the one surface still drawn as a box brush, because a 33 m slab does
not want to be a mesh.

Cube_Draw runs in CUBE_UV_PER_FACE mode (Game/game.cpp), so a box face stretches
the whole texture across itself rather than tiling it. The slab is about 34 m
across, so 1024 px works out at roughly 3.3 cm/px and joints, stains and scuffs
are drawn at world scale. (The previous attempt authored this for a 240 m pad —
23 cm/px — which is why the ground read as a grid rather than as concrete.)

Run from the client repo root:  python tools/gen_ground_texture.py
"""

import os
import random

from PIL import Image, ImageDraw

OUT_DIR = os.path.join("resource", "texture")
SIZE = 1024
METRES = 34.0


def draw_ground(seed):
    ppm = SIZE / METRES
    rng = random.Random(seed)
    base = (108, 107, 104)
    img = Image.new("RGB", (SIZE, SIZE), base)
    d = ImageDraw.Draw(img)

    # Per-pixel jitter, so the flat fill does not band. One offset for all three
    # channels keeps the noise neutral.
    px = img.load()
    for _ in range(90000):
        x, y = rng.randrange(SIZE), rng.randrange(SIZE)
        n = rng.randint(-11, 11)
        r, g, b = px[x, y]
        px[x, y] = (max(0, min(255, r + n)),
                    max(0, min(255, g + n)),
                    max(0, min(255, b + n)))

    # Slab expansion joints every 4 m.
    joint = tuple(int(c * 0.62) for c in base)
    step = int(4.0 * ppm)
    width = max(1, int(0.05 * ppm))
    for k in range(0, SIZE, step):
        d.rectangle([k, 0, k + width, SIZE - 1], fill=joint)
        d.rectangle([0, k, SIZE - 1, k + width], fill=joint)

    # Oil stains, 0.6-2.0 m across. The darkening factor is drawn ONCE per stain:
    # putting rng.random() inside the generator expression re-rolls it per
    # channel, which tinted every stain a different colour.
    for _ in range(30):
        cx, cy = rng.randrange(SIZE), rng.randrange(SIZE)
        rad = int(rng.uniform(0.3, 1.0) * ppm)
        shade = 0.52 + rng.random() * 0.2
        col = tuple(int(c * shade) for c in base)
        d.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], fill=col)

    # Tyre scuffs.
    for _ in range(70):
        x, y = rng.randrange(SIZE), rng.randrange(SIZE)
        ln = int(rng.uniform(1.0, 4.0) * ppm)
        col = tuple(int(c * 0.80) for c in base)
        if rng.random() < 0.5:
            d.rectangle([x, y, x + ln, y + max(1, int(0.10 * ppm))], fill=col)
        else:
            d.rectangle([x, y, x + max(1, int(0.10 * ppm)), y + ln], fill=col)
    return img


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, "ground_concrete.png")
    draw_ground(seed=7001).save(path)
    print(f"wrote {path}  {SIZE}x{SIZE}  ({METRES:.0f} m slab, {100 * METRES / SIZE:.1f} cm/px)")


if __name__ == "__main__":
    main()
