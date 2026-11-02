#pragma once

#include <pebble.h>

// Tiny pixel-art sprites written as strings, one character per art pixel.
// '.' is transparent; every other character is looked up in an ink table.

typedef struct {
  char key;
  GColor color;
} PixelInk;

// `data` holds `height` rows of `width + 1` chars each (NUL-terminated).
typedef struct {
  const char *data;
  uint8_t width;
  uint8_t height;
} PixelArt;

// Wraps a `static const char NAME[][W + 1]` grid.
#define PIXEL_ART(grid) \
  ((PixelArt){ &(grid)[0][0], sizeof((grid)[0]) - 1, ARRAY_LENGTH(grid) })

// Draws `art` with its top-left corner at `origin`, each art pixel `scale`
// screen pixels wide. Characters missing from `inks` are skipped.
void pixel_draw(GContext *ctx, PixelArt art, GPoint origin, int scale,
                const PixelInk *inks, int ink_count);

static inline GSize pixel_size(PixelArt art, int scale) {
  return GSize(art.width * scale, art.height * scale);
}
