#!/usr/bin/env python3
"""Generate palettized pixel-art sprites for the Step or Ded watchface.

Sprites are defined as character grids mapped directly to palette indices,
rendered at 2x scale, and saved as 4-bit palettized PNGs with a tRNS
transparency chunk so the Pebble resource compiler preserves alpha.
"""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "resources" / "images"
PREVIEW_PATH = ROOT / "tools" / "preview.png"

# All colors are valid RGB222 Pebble colors (channels in {0, 85, 170, 255}).
PALETTE: list[tuple[int, int, int, int]] = [
    (0, 0, 0, 0),          # 0  transparent
    (0, 0, 0, 255),        # 1  black
    (255, 255, 255, 255),  # 2  white
    (255, 170, 0, 255),    # 3  orange
    (170, 85, 0, 255),     # 4  brown
    (255, 255, 170, 255),  # 5  cream
    (255, 170, 85, 255),   # 6  tan
    (85, 0, 0, 255),       # 7  dark brown
    (85, 85, 85, 255),     # 8  dark gray
    (170, 170, 170, 255),  # 9  light gray
    (255, 0, 0, 255),      # 10 red
    (255, 85, 85, 255),    # 11 pink
    (85, 170, 255, 255),   # 12 light blue
    (255, 255, 0, 255),    # 13 yellow
    (0, 170, 0, 255),      # 14 green
    (255, 255, 255, 170),  # 15 translucent white (ghost)
]

# Shared characters available in every template.
BASE_CHARS = {
    ".": 0,
    "k": 1,   # black
    "W": 2,   # white
    "z": 2,   # Zzz text
    "x": 6,   # cardboard
    "y": 4,   # cardboard shading
    "G": 9,   # tombstone slab
    "h": 8,   # tombstone shadow
    "d": 12,  # sweat drop
    "w": 15,  # ghost body
    "r": 10,  # red
    "f": 13,  # butterfly wing
    "u": 12,  # butterfly wing alt
}

# Per-breed characters: o outline, B body, b belly/chest, s stripe,
# A inner ear, T tail, e eye, n nose, p paw.
BREEDS = {
    "orange": {"o": 1, "B": 3, "b": 5, "s": 4, "A": 11, "T": 3, "e": 14, "n": 11, "p": 5},
    "tuxedo": {"o": 8, "B": 1, "b": 2, "s": 1, "A": 11, "T": 1, "e": 13, "n": 11, "p": 2},
    "siamese": {"o": 1, "B": 5, "b": 2, "s": 6, "A": 7, "T": 7, "e": 12, "n": 7, "p": 7},
    "void": {"o": 8, "B": 1, "b": 1, "s": 1, "A": 8, "T": 1, "e": 13, "n": 8, "p": 1},
}

GRID_W = 24

# ---------------------------------------------------------------------------
# Cat templates (24 wide). Cat faces right. Bottom row touches the ground.
# ---------------------------------------------------------------------------

# Standing body shared by walk/distressed poses (rows 0-15 of a 20-row grid).
WALK_TOP = [
    "..........oo....oo",
    ".........oAAo..oAAo",
    ".........oABBooBBAo",
    "........oBBBBBBBBBBo",
    "........oBBBBBBBBBBo",
    ".oo.....oBBeeBBeeBBo",
    "oTTo....oBBBBnnBBBBo",
    "oTo.ooooooBBBBBBBBo",
    ".oTBBBBBBBBBBBBBBBBo",
    "..oBBsBBsBBsBBsBBBBo",
    "..oBBsBBsBBsBBsBBBBo",
    "..oBBBBBBBBBBBBBBBBo",
    "..oBBBBBBBBBBBBBBBBo",
    "...oBBBBBBBBBBBBBBo",
    "....ooBBBBBBBBBBoo",
]

WALK_LEGS = [
    [  # frame 0: stride out
        "....oBBo.....oBBo",
        "...oBBo.......oBBo",
        "...oppo.......oppo",
        "....oo.........oo",
        "",
    ],
    [  # frame 1: neutral
        ".....oBBo...oBBo",
        ".....oBBo...oBBo",
        ".....oppo...oppo",
        "......oo.....oo",
        "",
    ],
    [  # frame 2: tuck
        "......oBBo..oBBo",
        "......oBBo..oBBo",
        "......oppo..oppo",
        ".......oo....oo",
        "",
    ],
]

# Sitting cat (idle). Frame 1 blinks.
SIT = [
    "..........oo....oo",
    ".........oAAo..oAAo",
    ".........oABBooBBAo",
    "........oBBBBBBBBBBo",
    "........oBBBBBBBBBBo",
    "........oBBeeBBeeBBo",
    "........oBBBBnnBBBBo",
    ".........oBBBBBBBBo",
    "........oBBBBBBBBBBo",
    ".......oBBBBBbbBBBBo",
    ".......oBBBBbbbbBBBo",
    "......oBBsBBbbbbBBBo",
    "......oBBsBBbbbbBBBo",
    ".....oBBBsBBbbbbBBBo",
    ".....oBBBBBBbbbbBBBo",
    "....oBBBBBBBBBBBBBBo",
    "....oBBBBBBBBBBBBBBo",
    "....oBBBBBoBBBBoBBBo",
    ".oooBBBBBBoBppoBppBo",
    "oTTTooooooooooooooo",
]

# Distressed: standing, ears flat, sad eyes, frown, drooping tail.
DISTRESSED = [
    "",
    "",
    "........oo......oo",
    ".......oAAooooooAAo",
    ".......oBBBBBBBBBBo",
    ".......oBeeBBBBeeBo",
    ".......oBBBBnnBBBBo",
    ".......oBBBooooBBBo",
    "........oBBBBBBBBo",
    ".......ooBBBBBBBBoo",
    "......oBBsBBsBBsBBBo",
    ".oo...oBBBBBBBBBBBBo",
    ".oTo..oBsBBsBBsBBBBo",
    "..oTo.oBBBBBBBBBBBo",
    "..oToBBBBBBBBBBBBBo",
    "...oBBBBBBBBBBBBBo",
    "....ooBBBBBBBBBBoo",
    ".....oBBo....oBBo",
    ".....oBBo....oBBo",
    ".....oppo....oppo",
]

# Sweat drop positions (col, row) per distressed frame.
DISTRESSED_SWEAT = [
    [(20, 4), (21, 6)],
    [(20, 3), (21, 5), (20, 7)],
]

# Sleeping loaf with Zzz. Two frames move the Zzz.
SLEEP = [
    "...................z",
    ".................zz",
    "......oo....oo....z",
    ".....oAAo..oAAo",
    ".....oABBooBBAo",
    "....oBBBBBBBBBBo",
    "....oBooBBBBooBo",
    "....oBBBBnnBBBBo",
    "..ooBBBBBBBBBBBoo",
    ".oBBBBBBBBBBBBBBBo",
    ".oBssBBssBBssBBBBo",
    ".oBBBBBBBBBBBBBBBo",
    "..oBBBBBBBBBBBBBoo",
    "...ooooooooooooo",
]

SLEEP_Z_FRAME1 = {
    (19, 0): ".",
    (17, 1): ".",
    (18, 1): ".",
    (18, 2): ".",
    (20, 1): "z",
    (18, 3): "z",
    (19, 3): "z",
    (19, 4): "z",
}

# Charging: cat head peeking out of a cardboard box. Frame 1 sinks 1 row.
CHARGE_HEAD = [
    "..........oo....oo",
    ".........oAAo..oAAo",
    ".........oABBooBBAo",
    "........oBBBBBBBBBBo",
    "........oBooBBBBooBo",
    "........oBBBBnnBBBBo",
    ".........oBBBBBBBBo",
]

CHARGE_BOX = [
    "..oooooooooooooooooo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxyyyyyyyyyyyyyyxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oxxxxxxxxxxxxxxxxo",
    "..oooooooooooooooooo",
]

TOMBSTONE = [
    "......oooooooo",
    "....ooGGGGGGGGoo",
    "...oGGGGGGGGGGGGGo",
    "...oGGGGGGGGGGGGGo",
    "...oGkkGGkkkGkkkGo",
    "...oGkGkGGkGGkGkGo",
    "...oGkkGGGkGGkkkGo",
    "...oGkGkGGkGGkGGGo",
    "...oGkGkGkkkGkGGGo",
    "...oGGGGGGGGGGGhho",
    "...oGGGGGGGGGGGhho",
    "...oGGGGGGGGGGGhho",
    "...oGGGGGGGGGGGhho",
    "...oGGGGGGGGGGGhho",
    "...oGGGGGGGGGGGhho",
    "...oGGGGGGGGGGGhho",
    "..oGGGGGGGGGGGGGhho",
    "..oGGGGGGGGGGGGGhho",
    ".ohhhhhhhhhhhhhhhhho",
    "..ooooooooooooooooo",
]

GHOST_W = 20
GHOST = [
    [
        "....oo......oo",
        "...owwo....owwo",
        "...owwwoooowwwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owkkwwwwwwkkwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwkkwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owwowwowwowwowwo",
        "...o..o..o..o..o",
        "",
    ],
    [
        "....oo......oo",
        "...owwo....owwo",
        "...owwwoooowwwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owkkwwwwwwkkwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwkkwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owwwwwwwwwwwwo",
        "..owowwowwowwoww",
        "..o..o..o..o..o",
        "",
    ],
]

BUTTERFLY_W = 6
BUTTERFLY = [
    [
        "ff.ff",
        "ffkff",
        ".fkf",
        "ffkff",
        "ff.ff",
    ],
    [
        ".f.f",
        ".fkf",
        ".fkf",
        ".fkf",
        ".f.f",
    ],
]


def build_grid(rows: list[str], width: int, height: int) -> list[str]:
    grid = [row.ljust(width, ".") for row in rows]
    for i, row in enumerate(grid):
        if len(row) > width:
            raise ValueError(f"row {i} is {len(row)} chars, max {width}: {row!r}")
    while len(grid) < height:
        grid.append("." * width)
    if len(grid) > height:
        raise ValueError(f"grid has {len(grid)} rows, max {height}")
    return grid


def render(rows: list[str], charmap: dict[str, int], width: int, scale: int = 2) -> Image.Image:
    grid = build_grid(rows, width, len(rows))
    h = len(grid)
    img = Image.new("P", (width * scale, h * scale), 0)
    palette_bytes: list[int] = []
    for color in PALETTE:
        palette_bytes.extend(color[:3])
    palette_bytes.extend([0] * (768 - len(palette_bytes)))
    img.putpalette(palette_bytes)

    px = img.load()
    for y, row in enumerate(grid):
        for x, ch in enumerate(row):
            idx = charmap.get(ch)
            if idx is None:
                raise ValueError(f"unknown char {ch!r} at ({x},{y})")
            if idx == 0:
                continue
            for dy in range(scale):
                for dx in range(scale):
                    px[x * scale + dx, y * scale + dy] = idx
    return img


def save_png(img: Image.Image, path: Path) -> None:
    alpha = bytes(c[3] for c in PALETTE)
    img.save(path, optimize=True, transparency=alpha)


def charmap_for(breed: str | None) -> dict[str, int]:
    cm = dict(BASE_CHARS)
    if breed is not None:
        cm.update(BREEDS[breed])
    else:
        # Shared sprites still reference outline chars.
        cm.update({"o": 1, "B": 1, "b": 1, "s": 1, "A": 1, "T": 1, "e": 1, "n": 1, "p": 1})
    return cm


def apply_cells(rows: list[str], cells: dict[tuple[int, int], str] | list[tuple[int, int]],
                char: str | None = None) -> list[str]:
    grid = [list(row.ljust(GRID_W, ".")) for row in rows]
    if isinstance(cells, dict):
        for (x, y), ch in cells.items():
            grid[y][x] = ch
    else:
        for x, y in cells:
            grid[y][x] = char
    return ["".join(row).rstrip(".") for row in grid]


def make_walk(frame: int) -> list[str]:
    return WALK_TOP + WALK_LEGS[frame]


def make_sit(frame: int) -> list[str]:
    rows = list(SIT)
    if frame == 1:
        rows[5] = rows[5].replace("ee", "oo")  # blink
    return rows


def make_distressed(frame: int) -> list[str]:
    return apply_cells(DISTRESSED, DISTRESSED_SWEAT[frame], "d")


def make_sleep(frame: int) -> list[str]:
    if frame == 0:
        return list(SLEEP)
    return apply_cells(SLEEP, SLEEP_Z_FRAME1)


def make_charging(frame: int) -> list[str]:
    head = list(CHARGE_HEAD)
    pad = 0 if frame == 0 else 1
    rows = [""] * pad + head
    # Box overlaps the bottom of the head so the cat sits inside it.
    box_top = len(rows) - 1
    grid = [list(r.ljust(GRID_W, ".")) for r in rows[:box_top]]
    for box_row in CHARGE_BOX:
        grid.append(list(box_row.ljust(GRID_W, ".")))
    return ["".join(r).rstrip(".") for r in grid]


def resource_name(filename: str) -> str:
    return f"IMAGE_{Path(filename).stem.upper()}"


def make_fish(meat: bool) -> Image.Image:
    w, h = 32, 8
    img = Image.new("P", (w * 2, h * 2), 0)
    palette_bytes = []
    for color in PALETTE:
        palette_bytes.extend(color[:3])
    palette_bytes.extend([0] * (768 - len(palette_bytes)))
    img.putpalette(palette_bytes)

    if meat:
        rows = [
            "........................",
            "....ooooooooooooooo.....",
            "..ooBBBBBBBBBBBBBBBoo.o.",
            ".oBBeBBBBBBBBBBBBBBBooo.",
            ".oBBBBBBBBBBBBBBBBBBooo.",
            "..ooBBBBBBBBBBBBBBBoo.o.",
            "....ooooooooooooooo.....",
            "........................",
        ]
        cm = {".": 0, "o": 1, "B": 3, "e": 1}
    else:
        rows = [
            "........................",
            "....G..................." ,
            "..GGGGG.G.G.G.G.G.G..G..",
            ".G..GGGGGGGGGGGGGGGGGG..",
            "..GGGGG.G.G.G.G.G.G..G..",
            "....G...................",
            "........................",
            "........................",
        ]
        cm = {".": 0, "G": 9}
    rows = [r.ljust(w, ".")[:w] for r in rows]
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            idx = cm[ch]
            if idx == 0:
                continue
            for dy in range(2):
                for dx in range(2):
                    px[x * 2 + dx, y * 2 + dy] = idx
    return img


def make_preview(images: dict[str, Image.Image]) -> None:
    """Contact sheet on a checkerboard to spot transparency problems."""
    scale = 3
    cols = 8
    cell = 56
    names = sorted(images)
    rows_n = (len(names) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cell * scale, rows_n * cell * scale), (40, 120, 60))
    for i, name in enumerate(names):
        # Re-open the saved file so the tRNS chunk is honored.
        img = Image.open(OUT_DIR / name).convert("RGBA")
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        cx = (i % cols) * cell * scale
        cy = (i // cols) * cell * scale
        # Checker background per cell
        for ty in range(0, cell * scale, 12):
            for tx in range(0, cell * scale, 12):
                if (tx // 12 + ty // 12) % 2 == 0:
                    for yy in range(ty, min(ty + 12, cell * scale)):
                        for xx in range(tx, min(tx + 12, cell * scale)):
                            sheet.putpixel((cx + xx, cy + yy), (60, 140, 80))
        ox = cx + (cell * scale - img.width) // 2
        oy = cy + (cell * scale - img.height) // 2
        sheet.paste(img, (ox, oy), img)
    sheet.save(PREVIEW_PATH)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    resources = []
    generated: dict[str, Image.Image] = {}

    cat_anims = {
        "walk": (3, make_walk),
        "idle": (2, make_sit),
        "distressed": (2, make_distressed),
        "sleep": (2, make_sleep),
        "charging": (2, make_charging),
    }

    for breed in BREEDS:
        cm = charmap_for(breed)
        for anim, (count, factory) in cat_anims.items():
            for frame in range(count):
                filename = f"cat_{breed}_{anim}_{frame}.png"
                img = render(factory(frame), cm, GRID_W)
                save_png(img, OUT_DIR / filename)
                generated[filename] = img
                resources.append({
                    "type": "bitmap",
                    "name": resource_name(filename),
                    "file": f"images/{filename}",
                })

    shared_cm = charmap_for(None)
    shared: dict[str, Image.Image] = {
        "tombstone.png": render(TOMBSTONE, shared_cm, GRID_W),
        "ghost_0.png": render(GHOST[0], {**shared_cm, "o": 9}, GHOST_W),
        "ghost_1.png": render(GHOST[1], {**shared_cm, "o": 9}, GHOST_W),
        "butterfly_0.png": render(BUTTERFLY[0], shared_cm, BUTTERFLY_W),
        "butterfly_1.png": render(BUTTERFLY[1], shared_cm, BUTTERFLY_W),
        "fish_skeleton.png": make_fish(meat=False),
        "fish_meat.png": make_fish(meat=True),
    }

    for filename, img in shared.items():
        save_png(img, OUT_DIR / filename)
        generated[filename] = img
        resources.append({
            "type": "bitmap",
            "name": resource_name(filename),
            "file": f"images/{filename}",
        })

    manifest_path = ROOT / "tools" / "generated_resources.json"
    manifest_path.write_text(json.dumps(resources, indent=2) + "\n")
    make_preview(generated)
    print(f"Generated {len(resources)} sprites in {OUT_DIR}")
    print(f"Preview sheet: {PREVIEW_PATH}")


if __name__ == "__main__":
    main()
