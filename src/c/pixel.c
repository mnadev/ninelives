#include "pixel.h"

static const PixelInk *prv_find_ink(char key, const PixelInk *inks, int ink_count) {
  for (int i = 0; i < ink_count; i++) {
    if (inks[i].key == key) {
      return &inks[i];
    }
  }
  return NULL;
}

void pixel_draw(GContext *ctx, PixelArt art, GPoint origin, int scale,
                const PixelInk *inks, int ink_count) {
  for (int y = 0; y < art.height; y++) {
    const char *row = art.data + y * (art.width + 1);
    int x = 0;
    while (x < art.width && row[x]) {
      // Fill each run of identical characters with one rectangle.
      const char key = row[x];
      int run = 1;
      while (x + run < art.width && row[x + run] == key) {
        run++;
      }
      const PixelInk *ink = key == '.' ? NULL : prv_find_ink(key, inks, ink_count);
      if (ink) {
        graphics_context_set_fill_color(ctx, ink->color);
        graphics_fill_rect(ctx, GRect(origin.x + x * scale, origin.y + y * scale,
                                      run * scale, scale), 0, GCornerNone);
      }
      x += run;
    }
  }
}
