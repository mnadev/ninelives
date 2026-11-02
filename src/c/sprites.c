#include "sprites.h"

// Each breed's images in a fixed order: walk right, walk left, sit right,
// sit left, distressed, sleep, then the cardboard box.
#define CAT_ROW(BREED) {                                \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_0,               \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_1,               \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_2,               \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_3,               \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_LEFT_0,          \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_LEFT_1,          \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_LEFT_2,          \
  RESOURCE_ID_IMAGE_CAT_##BREED##_WALK_LEFT_3,          \
  RESOURCE_ID_IMAGE_CAT_##BREED##_IDLE_0,               \
  RESOURCE_ID_IMAGE_CAT_##BREED##_IDLE_1,               \
  RESOURCE_ID_IMAGE_CAT_##BREED##_IDLE_LEFT_0,          \
  RESOURCE_ID_IMAGE_CAT_##BREED##_IDLE_LEFT_1,          \
  RESOURCE_ID_IMAGE_CAT_##BREED##_DISTRESSED_0,         \
  RESOURCE_ID_IMAGE_CAT_##BREED##_DISTRESSED_1,         \
  RESOURCE_ID_IMAGE_CAT_##BREED##_SLEEP_0,              \
  RESOURCE_ID_IMAGE_CAT_##BREED##_SLEEP_1,              \
  RESOURCE_ID_IMAGE_CAT_##BREED##_CHARGING_0,           \
  RESOURCE_ID_IMAGE_CAT_##BREED##_CHARGING_1,           \
}

#define CAT_IMAGES_PER_BREED 18

static const uint32_t CAT_RESOURCES[NUM_CAT_BREEDS][CAT_IMAGES_PER_BREED] = {
  [CAT_BREED_ORANGE] = CAT_ROW(ORANGE),
  [CAT_BREED_TUXEDO] = CAT_ROW(TUXEDO),
  [CAT_BREED_SIAMESE] = CAT_ROW(SIAMESE),
  [CAT_BREED_VOID] = CAT_ROW(VOID),
};

typedef struct {
  uint8_t frames;
  // Index of frame 0 in a breed's row, facing right and facing left.
  uint8_t first[2];
} PoseInfo;

static const PoseInfo POSES[NUM_CAT_POSES] = {
  [CAT_POSE_WALK] = { 4, { 0, 4 } },
  [CAT_POSE_SIT] = { 2, { 8, 10 } },
  [CAT_POSE_DISTRESSED] = { 2, { 12, 12 } },
  [CAT_POSE_SLEEP] = { 2, { 14, 14 } },
  [CAT_POSE_BOX] = { 2, { 16, 16 } },
};

typedef struct {
  uint32_t resource_id;
  GBitmap *bitmap;
} Slot;

static Slot s_slots[NUM_SPRITE_SLOTS];

static GBitmap *prv_load(SpriteSlot slot, uint32_t resource_id) {
  Slot *entry = &s_slots[slot];
  if (entry->bitmap && entry->resource_id == resource_id) {
    return entry->bitmap;
  }
  if (entry->bitmap) {
    gbitmap_destroy(entry->bitmap);
  }
  entry->bitmap = gbitmap_create_with_resource(resource_id);
  entry->resource_id = resource_id;
  return entry->bitmap;
}

uint8_t sprites_pose_frames(CatPose pose) {
  return pose < NUM_CAT_POSES ? POSES[pose].frames : 1;
}

GBitmap *sprites_cat(int index, CatBreed breed, CatPose pose, uint8_t frame, CatFacing facing) {
  if (index < 0 || index >= MAX_CATS || (unsigned)breed >= NUM_CAT_BREEDS
      || (unsigned)pose >= NUM_CAT_POSES) {
    return NULL;
  }
  const PoseInfo *info = &POSES[pose];
  const uint8_t image = info->first[facing == CAT_FACING_LEFT] + frame % info->frames;
  return prv_load(SPRITE_SLOT_CAT_0 + index, CAT_RESOURCES[breed][image]);
}

GBitmap *sprites_tombstone(void) {
  return prv_load(SPRITE_SLOT_TOMBSTONE, RESOURCE_ID_IMAGE_TOMBSTONE);
}

GBitmap *sprites_ghost(uint8_t frame) {
  return prv_load(SPRITE_SLOT_GHOST,
                  frame % 2 ? RESOURCE_ID_IMAGE_GHOST_1 : RESOURCE_ID_IMAGE_GHOST_0);
}

GBitmap *sprites_butterfly(uint8_t frame) {
  return prv_load(SPRITE_SLOT_BUTTERFLY,
                  frame % 2 ? RESOURCE_ID_IMAGE_BUTTERFLY_1 : RESOURCE_ID_IMAGE_BUTTERFLY_0);
}

void sprites_deinit(void) {
  for (int i = 0; i < NUM_SPRITE_SLOTS; i++) {
    if (s_slots[i].bitmap) {
      gbitmap_destroy(s_slots[i].bitmap);
    }
    s_slots[i] = (Slot){ 0 };
  }
}
