#!/usr/bin/env python3
"""Generate palettized pixel-art sprites for the NineLives watchface.

Cats are built from simple shapes (ellipses, capsules, triangles) on a
character grid and outlined automatically, so their curves come out round
instead of stair-stepped. Each character maps to a palette index; sprites
are rendered at 2x (3x for emery) and saved as 4-bit palettized PNGs with a
tRNS transparency chunk so the Pebble resource compiler preserves alpha.
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
    "k": 1,   # black (tombstone text, whiskers)
    "W": 2,   # white (eye shine)
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
    "i": 2,   # eye shine (alias)
    "c": 11,  # blush cheek
    "m": 11,  # omega mouth
    "v": 1,   # whisker
}

# Per-breed characters: o outline, B body, D body in shade (far legs),
# b chest, s stripe, S forehead marks, A inner ear, T tail, e eye, i eye shine,
# n nose, q muzzle, m mouth, c cheek, p paw.
BREEDS = {
    "orange": {"o": 1, "B": 3, "D": 4, "b": 5, "s": 4, "S": 4, "A": 11, "T": 3, "e": 14, "i": 2,
               "n": 11, "q": 5, "m": 1, "c": 11, "p": 5},
    "tuxedo": {"o": 9, "B": 1, "D": 8, "b": 2, "s": 1, "S": 1, "A": 11, "T": 1, "e": 13, "i": 2,
               "n": 11, "q": 2, "m": 8, "c": 11, "p": 2},
    "siamese": {"o": 1, "B": 5, "D": 6, "b": 2, "s": 5, "S": 5, "A": 7, "T": 7, "e": 12, "i": 2,
                "n": 7, "q": 6, "m": 7, "c": 11, "p": 7},
    "void": {"o": 9, "B": 1, "D": 8, "b": 1, "s": 1, "S": 1, "A": 8, "T": 1, "e": 13, "i": 2,
             "n": 8, "q": 1, "m": 8, "c": 8, "p": 1},
}

# 1-bit variants for B&W platforms (diorite/flint). Only black (1), white (2)
# and transparent (0) are used. Every breed is drawn as white line art on a
# black body, which reads far better on the black sky than white silhouettes
# do; breeds are told apart by their white markings.
BW_BASE_CHARS = {
    "x": 1,   # cardboard box -> black, outlined in white
    "y": 2,   # box shading -> white
    "G": 2,   # tombstone slab -> white
    "h": 1,   # tombstone shadow -> black
    "d": 2,   # sweat drop -> white
    "w": 2,   # ghost body -> white
    "r": 2,
    "f": 2,   # butterfly wings -> white
    "u": 2,
}

BW_BREEDS = {
    # Outlined cat with white ears and tabby forehead marks; body stripes
    # would just be noise at 1 bit.
    "orange": {"o": 2, "B": 1, "D": 1, "b": 1, "s": 1, "S": 2, "A": 2, "T": 1, "e": 2, "i": 1,
               "n": 2, "q": 1, "m": 2, "c": 1, "p": 1},
    # White chest, muzzle and socks.
    "tuxedo": {"o": 2, "B": 1, "D": 1, "b": 2, "s": 1, "S": 1, "A": 2, "T": 1, "e": 2, "i": 1,
               "n": 2, "q": 2, "m": 1, "c": 1, "p": 2},
    # White points: ears, tail and paws.
    "siamese": {"o": 2, "B": 1, "D": 1, "b": 1, "s": 1, "S": 1, "A": 2, "T": 2, "e": 2, "i": 1,
                "n": 2, "q": 1, "m": 2, "c": 1, "p": 2},
    # Plain outline with white eyes.
    "void": {"o": 2, "B": 1, "D": 1, "b": 1, "s": 1, "S": 1, "A": 1, "T": 1, "e": 2, "i": 1,
             "n": 2, "q": 1, "m": 2, "c": 1, "p": 1},
}

GRID_W = 24

# ---------------------------------------------------------------------------
# Shapes. Coordinates are in art pixels; a cell is inside a shape when its
# centre is.
# ---------------------------------------------------------------------------

def ellipse(cx: float, cy: float, rx: float, ry: float):
    return lambda x, y: ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0


def capsule(x0: float, y0: float, x1: float, y1: float, r: float):
    def inside(x: int, y: int) -> bool:
        px, py = x + 0.5, y + 0.5
        dx, dy = x1 - x0, y1 - y0
        t = ((px - x0) * dx + (py - y0) * dy) / ((dx * dx + dy * dy) or 1)
        t = max(0.0, min(1.0, t))
        return (px - x0 - t * dx) ** 2 + (py - y0 - t * dy) ** 2 <= r * r
    return inside


def path(points: list[tuple[float, float]], r: float):
    pieces = [capsule(*points[i], *points[i + 1], r) for i in range(len(points) - 1)]
    return lambda x, y: any(piece(x, y) for piece in pieces)


def triangle(a: tuple[float, float], b: tuple[float, float], c: tuple[float, float]):
    def side(p, q, r):
        return (p[0] - r[0]) * (q[1] - r[1]) - (q[0] - r[0]) * (p[1] - r[1])

    def inside(x: int, y: int) -> bool:
        p = (x + 0.5, y + 0.5)
        d = (side(p, a, b), side(p, b, c), side(p, c, a))
        return not (any(v < 0 for v in d) and any(v > 0 for v in d))
    return inside


def union(*shapes):
    return lambda x, y: any(shape(x, y) for shape in shapes)


class Sprite:
    """A character grid that shapes are painted onto, back to front."""

    def __init__(self, height: int, width: int = GRID_W):
        self.width = width
        self.height = height
        self.grid = [["."] * width for _ in range(height)]

    def cells(self, shape) -> set[tuple[int, int]]:
        return {(x, y) for y in range(self.height) for x in range(self.width) if shape(x, y)}

    def part(self, shape, fill: str = "B", outline: str = "o") -> set[tuple[int, int]]:
        """Paint a shape over what's there, with an outline along its edge."""
        cells = self.cells(shape)
        for (x, y) in cells:
            edge = any((x + dx, y + dy) not in cells
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            self.grid[y][x] = outline if edge else fill
        return cells

    def paint(self, shape, char: str, within: set[tuple[int, int]]) -> None:
        """Recolour the inside (not the outline) of `within` where `shape` covers it."""
        for (x, y) in self.cells(shape) & within:
            if self.grid[y][x] != "o":
                self.grid[y][x] = char

    def dots(self, cells, char: str) -> None:
        for (x, y) in cells:
            if 0 <= x < self.width and 0 <= y < self.height:
                self.grid[y][x] = char

    def rows(self) -> list[str]:
        return ["".join(row) for row in self.grid]


def draw_face(sprite: Sprite, eye_l: tuple[int, int], eye_r: tuple[int, int],
              nose: tuple[int, int], blink: bool = False, worried: bool = False) -> None:
    """2x2 eyes (top-left corners given), a 2-pixel nose and a small mouth."""
    for (ex, ey) in (eye_l, eye_r):
        if blink:
            sprite.dots([(ex, ey + 1), (ex + 1, ey + 1)], "o")
        elif worried:
            sprite.dots([(ex, ey), (ex + 1, ey), (ex, ey + 1), (ex + 1, ey + 1)], "e")
            sprite.dots([(ex + 1, ey + 1)], "i")
        else:
            sprite.dots([(ex, ey), (ex + 1, ey), (ex, ey + 1), (ex + 1, ey + 1)], "e")
            sprite.dots([(ex, ey)], "i")
    if worried:
        # Brows slanting up towards the middle.
        sprite.dots([(eye_l[0] + 1, eye_l[1] - 2), (eye_l[0], eye_l[1] - 1)], "o")
        sprite.dots([(eye_r[0], eye_r[1] - 2), (eye_r[0] + 1, eye_r[1] - 1)], "o")
    nx, ny = nose
    sprite.dots([(nx, ny), (nx + 1, ny)], "n")
    if worried:
        sprite.dots([(nx, ny + 1), (nx + 1, ny + 1), (nx - 1, ny + 2), (nx + 2, ny + 2)], "m")
    else:
        sprite.dots([(nx - 1, ny + 1), (nx + 2, ny + 1), (nx, ny + 2), (nx + 1, ny + 2)], "m")


# ---------------------------------------------------------------------------
# Cat poses, all facing right; left-facing copies are mirrored. The bottom
# row touches the ground.
# ---------------------------------------------------------------------------

WALK_FRAMES = 4
# How far the legs swing per walk frame; the passing frames bob up a pixel.
WALK_SWING = [1.6, 0.0, -1.6, 0.0]


def make_walk(frame: int) -> list[str]:
    s = Sprite(16)
    top = -1.0 if WALK_SWING[frame] == 0.0 else 0.0
    swing = WALK_SWING[frame]

    def leg(x: float, offset: float, shade: bool) -> None:
        cells = s.part(capsule(x, 9.0 + top, x + offset, 14.5, 1.45), "D" if shade else "B")
        s.paint(lambda xx, yy: yy >= 14, "p", within=cells)

    # Far legs first, in shade.
    leg(5.6, -swing * 0.7, True)
    leg(14.0, swing * 0.7, True)
    s.part(path([(5.4, 7.2 + top), (3.4, 5.8 + top), (2.4, 3.8 + top),
                 (2.6, 2.0 + top), (3.8, 1.2 + top)], 1.5), "T")
    body = s.part(ellipse(11.2, 8.6 + top, 7.6, 3.5), "B")
    s.paint(ellipse(11.4, 11.4 + top, 5.4, 1.6), "b", within=body)
    s.paint(lambda x, y: x in (7, 10, 13) and y <= 7 + top, "s", within=body)
    leg(7.6, swing, False)
    leg(15.6, -swing, False)
    head = s.part(union(ellipse(17.9, 5.2 + top, 4.9, 4.2),
                        triangle((14.2, 3.6 + top), (14.9, -0.6 + top), (17.5, 1.7 + top)),
                        triangle((18.4, 1.7 + top), (21.1, -0.6 + top), (21.8, 3.6 + top))))
    s.paint(union(triangle((15.2, 2.6 + top), (15.4, 0.6 + top), (16.9, 1.9 + top)),
                  triangle((19.0, 1.9 + top), (20.6, 0.6 + top), (20.8, 2.6 + top))),
            "A", within=head)
    s.paint(ellipse(18.2, 7.0 + top, 2.3, 1.5), "q", within=head)
    eye_y = int(round(3.0 + top))
    draw_face(s, (15, eye_y), (19, eye_y), (17, eye_y + 2))
    return s.rows()


def _sitting(frame: int, distressed: bool) -> Sprite:
    s = Sprite(20)
    if distressed:
        tail = path([(7.0, 18.0), (4.5, 18.4), (2.6, 17.8), (1.7, 16.4)], 1.5)
    else:
        tail = path([(6.8, 16.6), (4.4, 15.6), (3.0, 13.6), (2.9, 11.2), (4.0, 9.2)], 1.55)
    s.part(tail, "T")
    body = s.part(union(ellipse(12.6, 15.2, 6.0, 4.9), ellipse(9.2, 16.2, 5.1, 3.7)))
    # Tabby stripes down the back and chest fur.
    s.paint(lambda x, y: (x + y) % 4 == 0 and x < 11 and 11 <= y <= 18, "s", within=body)
    s.paint(ellipse(16.4, 14.6, 2.6, 3.6), "b", within=body)
    legs = s.part(union(capsule(14.8, 10.5, 14.8, 18.6, 1.5),
                        capsule(16.6, 10.5, 16.6, 18.6, 1.5)))
    s.paint(lambda x, y: y >= 17, "p", within=legs)
    s.paint(ellipse(16.0, 12.4, 2.2, 2.6), "b", within=legs)

    if distressed:
        # Ears pinned out to the sides.
        ears = union(triangle((10.4, 6.6), (9.6, 2.9), (13.4, 4.4)),
                     triangle((17.8, 4.4), (21.6, 2.9), (20.8, 6.6)))
        inner = union(triangle((11.0, 5.6), (10.6, 3.8), (12.8, 4.6)),
                      triangle((18.4, 4.6), (20.6, 3.8), (20.2, 5.6)))
        head_y, eye_y = 7.4, 7
    else:
        ears = union(triangle((11.0, 5.4), (11.7, 0.2), (15.0, 3.0)),
                     triangle((16.2, 3.0), (19.5, 0.2), (20.2, 5.4)))
        inner = union(triangle((12.1, 4.2), (12.3, 1.8), (14.1, 3.3)),
                      triangle((17.1, 3.3), (18.9, 1.8), (19.1, 4.2)))
        head_y, eye_y = 7.0, 6
    head = s.part(union(ellipse(15.6, head_y, 5.5, 4.6), ears))
    s.paint(inner, "A", within=head)
    s.dots([(14, eye_y - 3), (16, eye_y - 3)], "S")
    s.paint(ellipse(15.9, head_y + 2.8, 2.6, 1.7), "q", within=head)
    draw_face(s, (12, eye_y), (18, eye_y), (15, eye_y + 2),
              blink=frame == 1 and not distressed, worried=distressed)
    s.dots([(12, eye_y + 3), (19, eye_y + 3)], "c")
    return s


def make_sit(frame: int) -> list[str]:
    """Sitting upright; frame 1 blinks."""
    return _sitting(frame, distressed=False).rows()


# Sweat drop positions (col, row) per distressed frame.
DISTRESSED_SWEAT = [
    [(22, 3), (23, 5)],
    [(22, 2), (23, 4), (22, 6)],
]


def make_distressed(frame: int) -> list[str]:
    """Sitting with ears pinned, worried brows, a frown and sweat drops."""
    s = _sitting(frame, distressed=True)
    s.dots(DISTRESSED_SWEAT[frame], "d")
    return s.rows()


# Zzz positions per sleep frame.
SLEEP_Z = [
    [(21, 0), (22, 0), (22, 1), (21, 2), (22, 2), (19, 3), (19, 4)],
    [(22, 1), (23, 1), (23, 2), (22, 3), (23, 3), (20, 4), (20, 5)],
]


def make_sleep(frame: int) -> list[str]:
    """Curled up with the tail wrapped round the front."""
    s = Sprite(14)
    body = s.part(ellipse(11.4, 9.0, 7.6, 4.8))
    s.paint(lambda x, y: (x + y) % 4 == 0 and x < 13 and y <= 9, "s", within=body)
    s.part(path([(4.4, 11.6), (6.6, 13.0), (10.4, 13.4), (14.6, 13.2), (17.4, 12.4)], 1.1), "T")
    head = s.part(union(ellipse(16.6, 8.4, 4.6, 3.6),
                        triangle((12.6, 7.2), (12.9, 3.6), (15.4, 5.4)),
                        triangle((17.6, 5.2), (20.6, 3.8), (20.8, 7.4))))
    s.paint(union(triangle((13.5, 6.4), (13.6, 4.8), (14.8, 5.8)),
                  triangle((18.4, 5.6), (20.0, 4.8), (20.0, 6.4))), "A", within=head)
    s.paint(ellipse(16.9, 10.2, 2.4, 1.4), "q", within=head)
    s.dots([(14, 8), (15, 8), (18, 8), (19, 8)], "o")
    s.dots([(16, 9), (17, 9)], "n")
    s.dots(SLEEP_Z[frame], "z")
    return s.rows()


def make_charging(frame: int) -> list[str]:
    """Peeking over the edge of a cardboard box; frame 1 sinks a pixel."""
    s = Sprite(20)
    sink = frame
    head = s.part(union(ellipse(12.0, 6.6 + sink, 5.4, 4.5),
                        triangle((7.4, 5.0 + sink), (8.1, -0.2 + sink), (11.4, 2.6 + sink)),
                        triangle((12.6, 2.6 + sink), (15.9, -0.2 + sink), (16.6, 5.0 + sink))))
    s.paint(union(triangle((8.5, 3.8 + sink), (8.7, 1.4 + sink), (10.5, 2.9 + sink)),
                  triangle((13.5, 2.9 + sink), (15.3, 1.4 + sink), (15.5, 3.8 + sink))),
            "A", within=head)
    s.dots([(10, 3 + sink), (12, 3 + sink)], "S")
    s.paint(ellipse(12.3, 9.4 + sink, 2.6, 1.7), "q", within=head)
    draw_face(s, (8, 6 + sink), (14, 6 + sink), (11, 8 + sink))
    for paw_x in (7.5, 15.5):
        s.part(ellipse(paw_x, 10.6, 1.6, 1.3), "p")
    s.part(lambda x, y: 2 <= x <= 21 and 11 <= y <= 19, "x")
    s.dots([(x, 13) for x in range(4, 20)], "y")
    s.dots([(x, 11) for x in range(2, 22)], "o")
    return s.rows()


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


def make_ghost(frame: int) -> list[str]:
    """The cat's head with a wavy sheet below; frame 1 ripples the hem."""
    s = Sprite(14, GHOST_W)
    hem = [(4.6, 11.4), (9.9, 11.4), (15.2, 11.4)] if frame == 0 else \
          [(3.2, 11.4), (8.5, 11.4), (13.8, 11.4)]
    sheet = union(ellipse(10.0, 6.4, 6.6, 5.4),
                  lambda x, y: 4 <= x <= 15 and 6 <= y <= 11,
                  *[ellipse(cx, cy, 2.1, 1.6) for cx, cy in hem],
                  triangle((4.0, 4.6), (4.6, 0.2), (8.2, 2.4)),
                  triangle((11.8, 2.4), (15.4, 0.2), (16.0, 4.6)))
    s.part(sheet, "w")
    s.dots([(7, 6), (7, 7), (12, 6), (12, 7)], "k")
    s.dots([(9, 9), (10, 9)], "k")
    return s.rows()


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


def charmap_for(breed: str | None, bw: bool = False) -> dict[str, int]:
    cm = dict(BASE_CHARS)
    if bw:
        cm.update(BW_BASE_CHARS)
    if breed is not None:
        cm.update((BW_BREEDS if bw else BREEDS)[breed])
    else:
        # Shared sprites still reference outline chars.
        cm.update({ch: 1 for ch in "oBDbsSATeinqmcp"})
    return cm


def resource_name(filename: str) -> str:
    stem = Path(filename).stem
    if "~" in stem:
        stem = stem.split("~")[0]
    return f"IMAGE_{stem.upper()}"


def save_sprite(img: Image.Image, base_filename: str, scale: int,
                resources: list, generated: dict[str, Image.Image]) -> None:
    """Save a sprite at the given scale; emit ~emery tagged copy for 3x."""
    if scale == 2:
        save_png(img, OUT_DIR / base_filename)
        generated[base_filename] = img
        resources.append({
            "type": "bitmap",
            "name": resource_name(base_filename),
            "file": f"images/{base_filename}",
        })
    elif scale == 3:
        stem = Path(base_filename).stem
        emery_filename = f"{stem}~emery.png"
        save_png(img, OUT_DIR / emery_filename)


def save_sprite_mirrored(img: Image.Image, base_filename: str, scale: int,
                         resources: list, generated: dict[str, Image.Image]) -> None:
    """Save a horizontally mirrored variant (left-facing)."""
    mirrored = img.transpose(Image.FLIP_LEFT_RIGHT)
    save_sprite(mirrored, base_filename, scale, resources, generated)


def save_bw_variant(img: Image.Image, base_filename: str,
                    generated: dict[str, Image.Image]) -> None:
    """Save a ~bw tagged copy for 1-bit platforms (diorite/flint)."""
    bw_filename = f"{Path(base_filename).stem}~bw.png"
    save_png(img, OUT_DIR / bw_filename)
    generated[bw_filename] = img


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
        "walk": (WALK_FRAMES, make_walk),
        "idle": (2, make_sit),
        "distressed": (2, make_distressed),
        "sleep": (2, make_sleep),
        "charging": (2, make_charging),
    }
    mirror_anims = {"walk", "idle"}

    for scale in (2, 3):
        for breed in BREEDS:
            cm = charmap_for(breed)
            for anim, (count, factory) in cat_anims.items():
                for frame in range(count):
                    filename = f"cat_{breed}_{anim}_{frame}.png"
                    img = render(factory(frame), cm, GRID_W, scale=scale)
                    save_sprite(img, filename, scale, resources, generated)
                    if anim in mirror_anims:
                        left_filename = f"cat_{breed}_{anim}_left_{frame}.png"
                        save_sprite_mirrored(img, left_filename, scale, resources, generated)

        shared_cm = charmap_for(None)
        shared: dict[str, Image.Image] = {
            "tombstone.png": render(TOMBSTONE, shared_cm, GRID_W, scale=scale),
            "ghost_0.png": render(make_ghost(0), {**shared_cm, "o": 9}, GHOST_W, scale=scale),
            "ghost_1.png": render(make_ghost(1), {**shared_cm, "o": 9}, GHOST_W, scale=scale),
            "butterfly_0.png": render(BUTTERFLY[0], shared_cm, BUTTERFLY_W, scale=scale),
            "butterfly_1.png": render(BUTTERFLY[1], shared_cm, BUTTERFLY_W, scale=scale),
        }

        for filename, img in shared.items():
            save_sprite(img, filename, scale, resources, generated)

    # 1-bit variants for B&W platforms, rendered at base (2x) size.
    for breed in BREEDS:
        cm = charmap_for(breed, bw=True)
        for anim, (count, factory) in cat_anims.items():
            for frame in range(count):
                filename = f"cat_{breed}_{anim}_{frame}.png"
                img = render(factory(frame), cm, GRID_W, scale=2)
                save_bw_variant(img, filename, generated)
                if anim in mirror_anims:
                    save_bw_variant(img.transpose(Image.FLIP_LEFT_RIGHT),
                                    f"cat_{breed}_{anim}_left_{frame}.png", generated)

    shared_bw_cm = charmap_for(None, bw=True)
    shared_bw: dict[str, Image.Image] = {
        "tombstone.png": render(TOMBSTONE, shared_bw_cm, GRID_W, scale=2),
        "ghost_0.png": render(make_ghost(0), shared_bw_cm, GHOST_W, scale=2),
        "ghost_1.png": render(make_ghost(1), shared_bw_cm, GHOST_W, scale=2),
        "butterfly_0.png": render(BUTTERFLY[0], shared_bw_cm, BUTTERFLY_W, scale=2),
        "butterfly_1.png": render(BUTTERFLY[1], shared_bw_cm, BUTTERFLY_W, scale=2),
    }
    for filename, img in shared_bw.items():
        save_bw_variant(img, filename, generated)

    manifest_path = ROOT / "tools" / "generated_resources.json"
    manifest_path.write_text(json.dumps(resources, indent=2) + "\n")
    make_preview(generated)
    print(f"Generated {len(resources)} resource entries in {OUT_DIR}")
    print(f"Preview sheet: {PREVIEW_PATH}")


if __name__ == "__main__":
    main()
