#pragma once

#include <pebble.h>
#include "game.h"

#define MAX_SPRITE_SLOTS MAX_CATS

typedef enum {
  SPRITE_ANIM_WALK = 0,
  SPRITE_ANIM_IDLE,
  SPRITE_ANIM_DISTRESSED,
  SPRITE_ANIM_SLEEP,
  SPRITE_ANIM_CHARGING,
  NUM_SPRITE_ANIMS
} SpriteAnim;

typedef enum {
  CAT_FACING_RIGHT = 0,
  CAT_FACING_LEFT = 1
} CatFacing;

// Nominal cat sprite size; actual bitmaps vary by pose, use gbitmap_get_bounds.
#define SPRITE_W 48
#define SPRITE_H 40

void sprites_init(void);
void sprites_deinit(void);

void sprites_load_slot(uint8_t slot, CatBreed breed, PetState state, uint8_t frame_index, CatFacing facing);
void sprites_load_shared_slot(uint8_t slot, uint32_t resource_id);

GBitmap *sprites_get_slot(uint8_t slot);
uint8_t sprites_get_frame_count(PetState state);
uint32_t sprites_get_resource_for_breed(CatBreed breed, PetState state, uint8_t frame_index, CatFacing facing);
