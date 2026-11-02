#include "scene.h"
#include "layout.h"
#include "pixel.h"

#define ART_COLS (SCREEN_W / PX + 1)
#define SNAP(v) ((v) / PX * PX)

// The sun and moon rise from behind the hills and peak just under the clock.
#define HORIZON_Y (GROUND_Y - 2 * PX)
#define ZENITH_Y (TIME_Y + TIME_H + 3 * PX)
// On 1-bit screens the ground is a single white line, so it runs under the back
// row of cats instead of behind their middles.
#define HORIZON_LINE_Y PBL_IF_COLOR_ELSE(GROUND_Y, LANE_BACK_Y)
#define SKY_MID_Y SNAP(GROUND_Y * 42 / 100)
#define SKY_LOW_Y SNAP(GROUND_Y * 72 / 100)

#define SUN_RISE_MIN (6 * 60)
#define SUN_SET_MIN (20 * 60)

#define C(name) { .argb = GColor##name##ARGB8 }

#ifdef PBL_COLOR
static const ScenePalette PALETTES[NUM_SKY_PHASES] = {
  [SKY_NIGHT] = {
    .sky_top = C(OxfordBlue), .sky_mid = C(DukeBlue), .sky_low = C(Indigo),
    .hills = C(OxfordBlue),
    .grass = C(DarkGreen), .grass_light = C(JaegerGreen), .grass_dark = C(Black),
    .text_shadow = C(Black),
  },
  [SKY_DAWN] = {
    .sky_top = C(Liberty), .sky_mid = C(Purpureus), .sky_low = C(Melon),
    .hills = C(ImperialPurple),
    .grass = C(DarkGreen), .grass_light = C(KellyGreen), .grass_dark = C(Black),
    .text_shadow = C(ImperialPurple),
  },
  [SKY_DAY] = {
    .sky_top = C(VividCerulean), .sky_mid = C(PictonBlue), .sky_low = C(Celeste),
    .hills = C(MayGreen),
    .grass = C(IslamicGreen), .grass_light = C(ScreaminGreen), .grass_dark = C(DarkGreen),
    .text_shadow = C(CobaltBlue),
  },
  [SKY_DUSK] = {
    .sky_top = C(Indigo), .sky_mid = C(Purpureus), .sky_low = C(Rajah),
    .hills = C(ImperialPurple),
    .grass = C(DarkGreen), .grass_light = C(KellyGreen), .grass_dark = C(Black),
    .text_shadow = C(ImperialPurple),
  },
};
#else
// 1-bit screens get a black sky with white line art in every phase.
static const ScenePalette BW_PALETTE = {
  .sky_top = C(Black), .sky_mid = C(Black), .sky_low = C(Black),
  .hills = C(Black),
  .grass = C(Black), .grass_light = C(White), .grass_dark = C(Black),
  .text_shadow = C(Black),
};
#endif

static const char SUN[][8] = {
  "..ooo..",
  ".oyyyo.",
  "oyyyyyo",
  "oyyyyyo",
  "oyyyyyo",
  ".oyyyo.",
  "..ooo..",
};

static const char MOON[][8] = {
  "..mmm..",
  ".mmm...",
  "mmm....",
  "mmm....",
  "mmm....",
  ".mmm...",
  "..mmm..",
};

static const char CLOUD[][12] = {
  "....www....",
  "..wwwwwww..",
  ".wwwwwwwww.",
  "wwwwwwwwwww",
  ".sssssssss.",
};

typedef struct {
  uint8_t x_pct;
  uint8_t y_pct;
} Star;

static const Star STARS[] = {
  { 4, 8 }, { 14, 30 }, { 23, 12 }, { 31, 46 }, { 40, 4 }, { 47, 26 },
  { 58, 14 }, { 66, 42 }, { 74, 7 }, { 82, 27 }, { 91, 13 }, { 96, 40 },
  { 9, 56 }, { 37, 64 }, { 61, 58 }, { 87, 62 }, { 52, 76 }, { 19, 80 },
};

typedef struct {
  uint8_t center_pct;
  uint8_t radius_pct;
  uint8_t peak;
} Hill;

static const Hill HILLS[] = {
  { 10, 30, 6 },
  { 50, 44, 10 },
  { 92, 30, 7 },
};

static uint8_t s_hill_height[ART_COLS];

SkyPhase scene_phase(const struct tm *now) {
  const int hour = now->tm_hour;
  if (hour >= 5 && hour < 7) {
    return SKY_DAWN;
  }
  if (hour >= 7 && hour < 17) {
    return SKY_DAY;
  }
  if (hour >= 17 && hour < 20) {
    return SKY_DUSK;
  }
  return SKY_NIGHT;
}

const ScenePalette *scene_palette(SkyPhase phase) {
#ifdef PBL_COLOR
  return &PALETTES[phase < NUM_SKY_PHASES ? phase : SKY_DAY];
#else
  (void)phase;
  return &BW_PALETTE;
#endif
}

static int prv_isqrt(int value) {
  int root = 0;
  while ((root + 1) * (root + 1) <= value) {
    root++;
  }
  return root;
}

void scene_init(void) {
  for (int col = 0; col < ART_COLS; col++) {
    int height = 0;
    for (unsigned i = 0; i < ARRAY_LENGTH(HILLS); i++) {
      const int radius = HILLS[i].radius_pct * ART_COLS / 100;
      const int dx = col - HILLS[i].center_pct * ART_COLS / 100;
      if (dx * dx >= radius * radius) {
        continue;
      }
      const int cap = prv_isqrt(radius * radius - dx * dx) - (radius - HILLS[i].peak);
      if (cap > height) {
        height = cap;
      }
    }
    s_hill_height[col] = height;
  }
}

static void prv_fill(GContext *ctx, GRect rect, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, rect, 0, GCornerNone);
}

#ifdef PBL_COLOR
// One PX-tall row where `color` covers every other cell, to soften a band edge.
static void prv_dither_row(GContext *ctx, int y, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  for (int x = (y / PX) % 2 * PX; x < SCREEN_W; x += 2 * PX) {
    graphics_fill_rect(ctx, GRect(x, y, PX, PX), 0, GCornerNone);
  }
}
#endif

static void prv_draw_sky(GContext *ctx, const ScenePalette *palette) {
  prv_fill(ctx, GRect(0, 0, SCREEN_W, SKY_MID_Y), palette->sky_top);
  prv_fill(ctx, GRect(0, SKY_MID_Y, SCREEN_W, SKY_LOW_Y - SKY_MID_Y), palette->sky_mid);
  prv_fill(ctx, GRect(0, SKY_LOW_Y, SCREEN_W, GROUND_Y - SKY_LOW_Y), palette->sky_low);
#ifdef PBL_COLOR
  prv_dither_row(ctx, SKY_MID_Y - PX, palette->sky_mid);
  prv_dither_row(ctx, SKY_MID_Y, palette->sky_top);
  prv_dither_row(ctx, SKY_LOW_Y - PX, palette->sky_low);
  prv_dither_row(ctx, SKY_LOW_Y, palette->sky_mid);
#endif
}

static void prv_draw_stars(GContext *ctx, bool twinkle) {
  // Below the date line, above the hills.
  const int top = STATUS_Y + STATUS_H;
  const int sky_h = HORIZON_Y - 4 * PX - top;
  for (unsigned i = 0; i < ARRAY_LENGTH(STARS); i++) {
    const int x = SNAP(STARS[i].x_pct * SCREEN_W / 100);
    const int y = SNAP(top + STARS[i].y_pct * sky_h / 100);
    const GColor color = PBL_IF_COLOR_ELSE(i % 3 ? GColorLightGray : GColorWhite, GColorWhite);
    if (twinkle && i % 3 == 0) {
      prv_fill(ctx, GRect(x - PX, y, 3 * PX, PX), color);
      prv_fill(ctx, GRect(x, y - PX, PX, 3 * PX), color);
    } else {
      prv_fill(ctx, GRect(x, y, PX, PX), color);
    }
  }
}

// Draws `art` centred on a point along the sky arc, `t` from 0 (rising) to 1.
static void prv_draw_on_arc(GContext *ctx, PixelArt art, int t_permille,
                            const PixelInk *inks, int ink_count) {
  const GSize size = pixel_size(art, PX);
  const int lift = (HORIZON_Y - ZENITH_Y) * 4 * t_permille / 1000 * (1000 - t_permille) / 1000;
  const int cx = t_permille * SCREEN_W / 1000;
  const int cy = HORIZON_Y - lift;
  pixel_draw(ctx, art, GPoint(SNAP(cx - size.w / 2), SNAP(cy - size.h / 2)), PX, inks, ink_count);
}

static void prv_draw_celestial(GContext *ctx, const struct tm *now, SkyPhase phase) {
  const int minute = now->tm_hour * 60 + now->tm_min;
  if (minute >= SUN_RISE_MIN && minute < SUN_SET_MIN) {
#ifdef PBL_COLOR
    // A low sun glows orange.
    const bool low = phase != SKY_DAY;
    const PixelInk inks[] = {
      { 'o', low ? GColorOrange : GColorChromeYellow },
      { 'y', low ? GColorRajah : GColorYellow },
    };
#else
    const PixelInk inks[] = { { 'o', GColorWhite }, { 'y', GColorBlack } };
#endif
    const int t = (minute - SUN_RISE_MIN) * 1000 / (SUN_SET_MIN - SUN_RISE_MIN);
    prv_draw_on_arc(ctx, PIXEL_ART(SUN), t, inks, ARRAY_LENGTH(inks));
  } else {
    const int night_len = MINUTES_PER_DAY - SUN_SET_MIN + SUN_RISE_MIN;
    const int up = (minute - SUN_SET_MIN + MINUTES_PER_DAY) % MINUTES_PER_DAY;
    const PixelInk inks[] = { { 'm', PBL_IF_COLOR_ELSE(GColorPastelYellow, GColorWhite) } };
    prv_draw_on_arc(ctx, PIXEL_ART(MOON), up * 1000 / night_len, inks, ARRAY_LENGTH(inks));
  }
}

#ifdef PBL_COLOR
static void prv_draw_clouds(GContext *ctx, const struct tm *now) {
  const PixelArt cloud = PIXEL_ART(CLOUD);
  const PixelInk inks[] = { { 'w', GColorWhite }, { 's', GColorBabyBlueEyes } };
  const int wrap = ART_COLS + cloud.width;
  const int drift = (now->tm_hour * 60 + now->tm_min) / 2;
  const int rows[] = { SNAP(ZENITH_Y - 2 * PX), SNAP((ZENITH_Y + HORIZON_Y) / 2) };
  const int offsets[] = { 0, wrap / 2 + 7 };
  for (unsigned i = 0; i < ARRAY_LENGTH(rows); i++) {
    const int col = (drift + offsets[i]) % wrap - cloud.width;
    pixel_draw(ctx, cloud, GPoint(col * PX, rows[i]), PX, inks, ARRAY_LENGTH(inks));
  }
}
#endif

static void prv_draw_hills(GContext *ctx, const ScenePalette *palette) {
  for (int col = 0; col < ART_COLS; col++) {
    const int height = s_hill_height[col];
    if (height <= 0) {
      continue;
    }
    const int top = HORIZON_LINE_Y - height * PX;
    prv_fill(ctx, GRect(col * PX, top, PX, height * PX), palette->hills);
#ifndef PBL_COLOR
    // Outline only: run down far enough to meet the lower neighbour. Hills
    // carry on past the screen edges rather than dropping to the ground there.
    const int left = col > 0 ? s_hill_height[col - 1] : height;
    const int right = col + 1 < ART_COLS ? s_hill_height[col + 1] : height;
    const int lowest = left < right ? left : right;
    const int span = lowest < height ? height - lowest : 1;
    prv_fill(ctx, GRect(col * PX, top, PX, span * PX), GColorWhite);
#endif
  }
}

static void prv_draw_ground(GContext *ctx, const ScenePalette *palette) {
  const int ground_h = PANEL_Y - HORIZON_LINE_Y;
  prv_fill(ctx, GRect(0, HORIZON_LINE_Y, SCREEN_W, ground_h), palette->grass);
  prv_fill(ctx, GRect(0, HORIZON_LINE_Y, SCREEN_W, PX), palette->grass_light);

  // Scattered blades of grass, the same every frame.
  const int rows = ground_h / PX;
  for (int col = 0; col < ART_COLS; col++) {
    const int hash = (col * 37 + 11) % 17;
    if (hash < 3 && rows > 3) {
      const int row = 2 + (col * 7) % (rows - 3);
      prv_fill(ctx, GRect(col * PX, HORIZON_LINE_Y + row * PX, PX, PX),
               hash == 0 ? palette->grass_light : palette->grass_dark);
    }
  }
  prv_fill(ctx, GRect(0, PANEL_Y - PX, SCREEN_W, PX), palette->grass_dark);
}

void scene_draw(GContext *ctx, const struct tm *now, bool twinkle) {
  const SkyPhase phase = scene_phase(now);
  const ScenePalette *palette = scene_palette(phase);

  prv_draw_sky(ctx, palette);
  if (phase == SKY_NIGHT) {
    prv_draw_stars(ctx, twinkle);
  }
  prv_draw_celestial(ctx, now, phase);
#ifdef PBL_COLOR
  if (phase == SKY_DAY) {
    prv_draw_clouds(ctx, now);
  }
#endif
  prv_draw_hills(ctx, palette);
  prv_draw_ground(ctx, palette);
}
