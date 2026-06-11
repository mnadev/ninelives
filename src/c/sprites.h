#pragma once

#include <pebble.h>
#include "game.h"

typedef enum {
  SPRITE_ANIM_WALK = 0,
  SPRITE_ANIM_IDLE,
  SPRITE_ANIM_DISTRESSED,
  SPRITE_ANIM_SLEEP,
  SPRITE_ANIM_CHARGING,
  NUM_SPRITE_ANIMS
} SpriteAnim;

#define SPRITE_W 48
#define SPRITE_H 44

void sprites_init(void);
void sprites_deinit(void);
void sprites_set_breed(CatBreed breed);

void sprites_load_frame(PetState state, uint8_t frame_index);
void sprites_load_shared(uint32_t resource_id);

GBitmap *sprites_get_current(void);
uint8_t sprites_get_frame_count(PetState state);
uint32_t sprites_get_resource_for_state(PetState state, uint8_t frame_index);
uint32_t sprites_get_breed_resource(SpriteAnim anim, uint8_t frame_index);
