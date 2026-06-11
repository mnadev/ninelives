#!/usr/bin/env python3
"""Generate palettized pixel-art sprites for the Step or Ded watchface."""

from __future__ import annotations

import json
import os
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "resources" / "images"

# Pebble-friendly palette (subset of 64-color e-paper colors).
PALETTE = [
    (0, 0, 0, 0),
    (0, 0, 0, 255),
    (255, 255, 255, 255),
    (170, 85, 0, 255),
    (255, 170, 0, 255),
    (255, 204, 102, 255),
    (85, 51, 17, 255),
    (204, 136, 68, 255),
    (255, 221, 170, 255),
    (51, 51, 51, 255),
    (102, 102, 102, 255),
    (170, 170, 170, 255),
    (255, 85, 85, 255),
    (0, 170, 255, 255),
    (0, 85, 170, 255),
    (255, 255, 0, 255),
]

BREEDS = {
    "orange": {
        "body": 3,
        "belly": 7,
        "stripe": 5,
        "ear": 5,
        "eye": 1,
        "nose": 11,
        "paw": 7,
    },
    "tuxedo": {
        "body": 1,
        "belly": 2,
        "stripe": 1,
        "ear": 1,
        "eye": 2,
        "nose": 11,
        "paw": 2,
    },
    "siamese": {
        "body": 7,
        "belly": 8,
        "stripe": 5,
        "ear": 5,
        "eye": 1,
        "nose": 11,
        "paw": 8,
    },
    "void": {
        "body": 1,
        "belly": 1,
        "stripe": 1,
        "ear": 1,
        "eye": 15,
        "nose": 11,
        "paw": 1,
    },
}


def nearest_palette_index(rgba: tuple[int, int, int, int]) -> int:
    best = 0
    best_dist = 1 << 30
    for i, color in enumerate(PALETTE):
        dr = rgba[0] - color[0]
        dg = rgba[1] - color[1]
        db = rgba[2] - color[2]
        dist = dr * dr + dg * dg + db * db
        if dist < best_dist:
            best_dist = dist
            best = i
    return best


def save_palettized(img: Image.Image, path: Path) -> None:
    rgba = img.convert("RGBA")
    indexed = Image.new("P", rgba.size)
    palette_bytes = []
    for color in PALETTE:
        palette_bytes.extend(color[:3])
    while len(palette_bytes) < 256 * 3:
        palette_bytes.append(0)
    indexed.putpalette(palette_bytes)

    src = rgba.load()
    dst = indexed.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            dst[x, y] = nearest_palette_index(src[x, y])

    indexed.save(path, optimize=True)


def blank(w: int = 48, h: int = 44) -> Image.Image:
    return Image.new("RGBA", (w, h), PALETTE[0])


def draw_cat(
    draw: ImageDraw.ImageDraw,
    colors: dict[str, int],
    pose: str,
    frame: int,
    distressed: bool = False,
) -> None:
    body = colors["body"]
    belly = colors["belly"]
    stripe = colors["stripe"]
    ear = colors["ear"]
    eye = colors["eye"]
    nose = colors["nose"]
    paw = colors["paw"]

    if distressed:
        body = 10
        belly = 11
        stripe = 10

    offset_x = 0
    if pose == "walk":
        offset_x = [-2, 0, 2][frame % 3]
    elif pose == "idle":
        offset_x = [0, 1][frame % 2]

    ox = 8 + offset_x
    oy = 10

    # Tail
    draw.line([(ox - 2, oy + 18), (ox - 8, oy + 10), (ox - 10, oy + 4)], fill=body, width=2)
    # Body
    draw.ellipse([ox, oy + 10, ox + 26, oy + 30], fill=body)
    draw.ellipse([ox + 6, oy + 14, ox + 20, oy + 26], fill=belly)
    # Stripes
    draw.line([(ox + 8, oy + 12), (ox + 8, oy + 20)], fill=stripe, width=1)
    draw.line([(ox + 16, oy + 12), (ox + 16, oy + 20)], fill=stripe, width=1)
    # Head
    draw.ellipse([ox + 14, oy, ox + 34, oy + 18], fill=body)
    draw.polygon([(ox + 16, oy + 2), (ox + 14, oy - 4), (ox + 20, oy + 2)], fill=ear)
    draw.polygon([(ox + 30, oy + 2), (ox + 36, oy - 4), (ox + 28, oy + 2)], fill=ear)
    # Face
    draw.ellipse([ox + 22, oy + 8, ox + 24, oy + 10], fill=eye)
    draw.ellipse([ox + 28, oy + 8, ox + 30, oy + 10], fill=eye)
    draw.point((ox + 26, oy + 12), fill=nose)

    if pose == "walk":
        leg_shift = [2, -2, 2][frame % 3]
        draw.rectangle([ox + 8, oy + 26, ox + 12, oy + 32 + leg_shift], fill=paw)
        draw.rectangle([ox + 18, oy + 26, ox + 22, oy + 32 - leg_shift], fill=paw)
    elif pose == "idle" and frame % 2 == 1:
        draw.arc([ox + 20, oy + 10, ox + 32, oy + 18], 200, 340, fill=belly, width=2)
        draw.rectangle([ox + 10, oy + 26, ox + 14, oy + 32], fill=paw)
        draw.rectangle([ox + 18, oy + 26, ox + 22, oy + 32], fill=paw)
    elif pose == "sleep":
        draw.ellipse([ox + 4, oy + 16, ox + 30, oy + 30], fill=body)
        draw.ellipse([ox + 20, oy + 10, ox + 32, oy + 20], fill=body)
        draw.text((ox + 30, oy - 6), "Z", fill=2)
        draw.text((ox + 34, oy - 10), "z", fill=2)
    elif pose == "charging":
        draw.rectangle([ox - 2, oy + 20, ox + 34, oy + 34], fill=8)
        draw.rectangle([ox, oy + 18, ox + 32, oy + 22], fill=8)
        draw.ellipse([ox + 10, oy + 10, ox + 28, oy + 24], fill=body)
        draw.ellipse([ox + 18, oy + 14, ox + 22, oy + 18], fill=eye)
    else:
        draw.rectangle([ox + 10, oy + 26, ox + 14, oy + 32], fill=paw)
        draw.rectangle([ox + 18, oy + 26, ox + 22, oy + 32], fill=paw)

    if distressed:
        draw.ellipse([ox + 34, oy + 4, ox + 38, oy + 8], fill=13)
        draw.ellipse([ox + 36, oy + 10, ox + 40, oy + 14], fill=13)
        draw.line([(ox + 12, oy + 30), (ox + 16, oy + 34), (ox + 20, oy + 30)], fill=9, width=2)


def make_cat_frame(breed: str, anim: str, frame: int) -> Image.Image:
    img = blank()
    draw = ImageDraw.Draw(img)
    colors = BREEDS[breed]
    distressed = anim == "distressed"
    pose = {
        "walk": "walk",
        "idle": "idle",
        "distressed": "walk",
        "sleep": "sleep",
        "charging": "charging",
    }[anim]
    draw_cat(draw, colors, pose, frame, distressed=distressed)
    return img


def make_tombstone() -> Image.Image:
    img = blank()
    draw = ImageDraw.Draw(img)
    draw.polygon([(18, 8), (30, 8), (34, 38), (14, 38)], fill=10)
    draw.rectangle([20, 14, 28, 22], fill=2)
    draw.text((21, 15), "RIP", fill=1)
    return img


def make_ghost(frame: int) -> Image.Image:
    img = blank()
    draw = ImageDraw.Draw(img)
    y = 6 + (frame % 2)
    draw.ellipse([14, y, 34, y + 18], fill=11)
    draw.rectangle([14, y + 10, 34, y + 24], fill=11)
    for x in range(14, 35, 4):
        draw.ellipse([x, y + 20, x + 3, y + 26], fill=11)
    draw.ellipse([20, y + 8, 22, y + 10], fill=1)
    draw.ellipse([26, y + 8, 28, y + 10], fill=1)
    return img


def make_butterfly(frame: int) -> Image.Image:
    img = Image.new("RGBA", (12, 10), PALETTE[0])
    draw = ImageDraw.Draw(img)
    wing = 13 if frame % 2 == 0 else 3
    draw.ellipse([0, 1, 5, 8], fill=wing)
    draw.ellipse([7, 1, 12, 8], fill=wing)
    draw.rectangle([5, 4, 7, 9], fill=1)
    return img


def make_fish_skeleton() -> Image.Image:
    img = Image.new("RGBA", (64, 16), (0, 0, 0, 255))
    draw = ImageDraw.Draw(img)
    draw.ellipse([4, 4, 14, 12], fill=11)
    draw.line([(14, 8), (52, 8)], fill=11, width=2)
    for x in range(18, 54, 6):
        draw.line([(x, 8), (x + 2, 4)], fill=11, width=1)
        draw.line([(x, 8), (x + 2, 12)], fill=11, width=1)
    draw.polygon([(54, 8), (62, 4), (62, 12)], fill=11)
    return img


def make_fish_meat() -> Image.Image:
    img = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.ellipse([4, 4, 14, 12], fill=12)
    draw.rectangle([10, 5, 60, 11], fill=3)
    draw.polygon([(58, 8), (63, 5), (63, 11)], fill=3)
    return img


def resource_name(filename: str) -> str:
    stem = Path(filename).stem.upper()
    return f"IMAGE_{stem}"


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    resources = []

    for breed in BREEDS:
        for anim, count in [
            ("walk", 3),
            ("idle", 2),
            ("distressed", 2),
            ("sleep", 2),
            ("charging", 2),
        ]:
            for frame in range(count):
                filename = f"cat_{breed}_{anim}_{frame}.png"
                path = OUT_DIR / filename
                img = make_cat_frame(breed, anim, frame)
                save_palettized(img, path)
                resources.append(
                    {
                        "type": "bitmap",
                        "name": resource_name(filename),
                        "file": f"images/{filename}",
                    }
                )

    shared = {
        "tombstone.png": make_tombstone,
        "ghost_0.png": lambda: make_ghost(0),
        "ghost_1.png": lambda: make_ghost(1),
        "butterfly_0.png": lambda: make_butterfly(0),
        "butterfly_1.png": lambda: make_butterfly(1),
        "fish_skeleton.png": make_fish_skeleton,
        "fish_meat.png": make_fish_meat,
    }

    for filename, factory in shared.items():
        path = OUT_DIR / filename
        save_palettized(factory(), path)
        resources.append(
            {
                "type": "bitmap",
                "name": resource_name(filename),
                "file": f"images/{filename}",
            }
        )

    manifest_path = ROOT / "tools" / "generated_resources.json"
    manifest_path.write_text(json.dumps(resources, indent=2))
    print(f"Generated {len(resources)} sprites in {OUT_DIR}")


if __name__ == "__main__":
    main()
