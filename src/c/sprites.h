#pragma once

#include <pebble.h>
#include "game.h"
#include "layout.h"

// Cat sprites are 24 art pixels wide and at most 20 tall.
#define CAT_W (24 * PX)
#define CAT_H (20 * PX)

typedef enum {
  CAT_FACING_RIGHT = 0,
  CAT_FACING_LEFT = 1
} CatFacing;

typedef enum {
  CAT_POSE_WALK = 0,
  CAT_POSE_SIT,
  CAT_POSE_DISTRESSED,
  CAT_POSE_SLEEP,
  CAT_POSE_BOX,
  NUM_CAT_POSES
} CatPose;

// One bitmap is kept per slot and only reloaded when its image changes.
typedef enum {
  SPRITE_SLOT_CAT_0 = 0,
  SPRITE_SLOT_TOMBSTONE = MAX_CATS,
  SPRITE_SLOT_GHOST,
  SPRITE_SLOT_BUTTERFLY,
  NUM_SPRITE_SLOTS
} SpriteSlot;

uint8_t sprites_pose_frames(CatPose pose);
GBitmap *sprites_cat(int index, CatBreed breed, CatPose pose, uint8_t frame, CatFacing facing);
GBitmap *sprites_tombstone(void);
GBitmap *sprites_ghost(uint8_t frame);
GBitmap *sprites_butterfly(uint8_t frame);
void sprites_deinit(void);
