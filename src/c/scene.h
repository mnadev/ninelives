#pragma once

#include <pebble.h>

typedef enum {
  SKY_NIGHT = 0,
  SKY_DAWN,
  SKY_DAY,
  SKY_DUSK,
  NUM_SKY_PHASES
} SkyPhase;

typedef struct {
  GColor sky_top;
  GColor sky_mid;
  GColor sky_low;
  GColor hills;
  GColor grass;
  GColor grass_light;
  GColor grass_dark;
  GColor text_shadow;
} ScenePalette;

SkyPhase scene_phase(const struct tm *now);
const ScenePalette *scene_palette(SkyPhase phase);

void scene_init(void);
// Sky with sun or moon, clouds or stars, then hills and grass. `twinkle`
// flips which stars sparkle.
void scene_draw(GContext *ctx, const struct tm *now, bool twinkle);
