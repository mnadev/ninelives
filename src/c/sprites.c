#include "sprites.h"

static CatBreed s_breed = CAT_BREED_ORANGE;
static GBitmap *s_current_bitmap = NULL;

static const uint8_t s_frame_counts[NUM_PET_STATES] = {
  [PET_STATE_ENERGETIC] = 3,
  [PET_STATE_LAZY] = 2,
  [PET_STATE_DISTRESSED] = 2,
  [PET_STATE_SLEEPING] = 2,
  [PET_STATE_DEAD] = 1,
  [PET_STATE_CHARGING] = 2,
  [PET_STATE_LOW_BATTERY] = 2,
};

static uint32_t prv_breed_resource(CatBreed breed, SpriteAnim anim, uint8_t frame) {
  static const uint32_t orange[NUM_SPRITE_ANIMS][3] = {
    {RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_0, RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_1, RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_2},
    {RESOURCE_ID_IMAGE_CAT_ORANGE_IDLE_0, RESOURCE_ID_IMAGE_CAT_ORANGE_IDLE_1, 0},
    {RESOURCE_ID_IMAGE_CAT_ORANGE_DISTRESSED_0, RESOURCE_ID_IMAGE_CAT_ORANGE_DISTRESSED_1, 0},
    {RESOURCE_ID_IMAGE_CAT_ORANGE_SLEEP_0, RESOURCE_ID_IMAGE_CAT_ORANGE_SLEEP_1, 0},
    {RESOURCE_ID_IMAGE_CAT_ORANGE_CHARGING_0, RESOURCE_ID_IMAGE_CAT_ORANGE_CHARGING_1, 0},
  };
  static const uint32_t tuxedo[NUM_SPRITE_ANIMS][3] = {
    {RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_0, RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_1, RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_2},
    {RESOURCE_ID_IMAGE_CAT_TUXEDO_IDLE_0, RESOURCE_ID_IMAGE_CAT_TUXEDO_IDLE_1, 0},
    {RESOURCE_ID_IMAGE_CAT_TUXEDO_DISTRESSED_0, RESOURCE_ID_IMAGE_CAT_TUXEDO_DISTRESSED_1, 0},
    {RESOURCE_ID_IMAGE_CAT_TUXEDO_SLEEP_0, RESOURCE_ID_IMAGE_CAT_TUXEDO_SLEEP_1, 0},
    {RESOURCE_ID_IMAGE_CAT_TUXEDO_CHARGING_0, RESOURCE_ID_IMAGE_CAT_TUXEDO_CHARGING_1, 0},
  };
  static const uint32_t siamese[NUM_SPRITE_ANIMS][3] = {
    {RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_0, RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_1, RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_2},
    {RESOURCE_ID_IMAGE_CAT_SIAMESE_IDLE_0, RESOURCE_ID_IMAGE_CAT_SIAMESE_IDLE_1, 0},
    {RESOURCE_ID_IMAGE_CAT_SIAMESE_DISTRESSED_0, RESOURCE_ID_IMAGE_CAT_SIAMESE_DISTRESSED_1, 0},
    {RESOURCE_ID_IMAGE_CAT_SIAMESE_SLEEP_0, RESOURCE_ID_IMAGE_CAT_SIAMESE_SLEEP_1, 0},
    {RESOURCE_ID_IMAGE_CAT_SIAMESE_CHARGING_0, RESOURCE_ID_IMAGE_CAT_SIAMESE_CHARGING_1, 0},
  };
  static const uint32_t void_cat[NUM_SPRITE_ANIMS][3] = {
    {RESOURCE_ID_IMAGE_CAT_VOID_WALK_0, RESOURCE_ID_IMAGE_CAT_VOID_WALK_1, RESOURCE_ID_IMAGE_CAT_VOID_WALK_2},
    {RESOURCE_ID_IMAGE_CAT_VOID_IDLE_0, RESOURCE_ID_IMAGE_CAT_VOID_IDLE_1, 0},
    {RESOURCE_ID_IMAGE_CAT_VOID_DISTRESSED_0, RESOURCE_ID_IMAGE_CAT_VOID_DISTRESSED_1, 0},
    {RESOURCE_ID_IMAGE_CAT_VOID_SLEEP_0, RESOURCE_ID_IMAGE_CAT_VOID_SLEEP_1, 0},
    {RESOURCE_ID_IMAGE_CAT_VOID_CHARGING_0, RESOURCE_ID_IMAGE_CAT_VOID_CHARGING_1, 0},
  };

  const uint32_t (*tables[NUM_CAT_BREEDS])[3] = {orange, tuxedo, siamese, void_cat};
  if (breed >= NUM_CAT_BREEDS || anim >= NUM_SPRITE_ANIMS || frame >= 3) {
    return RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_0;
  }
  return tables[breed][anim][frame];
}

static SpriteAnim prv_anim_for_state(PetState state) {
  switch (state) {
    case PET_STATE_ENERGETIC:
      return SPRITE_ANIM_WALK;
    case PET_STATE_LAZY:
    case PET_STATE_LOW_BATTERY:
      return SPRITE_ANIM_IDLE;
    case PET_STATE_DISTRESSED:
      return SPRITE_ANIM_DISTRESSED;
    case PET_STATE_SLEEPING:
      return SPRITE_ANIM_SLEEP;
    case PET_STATE_CHARGING:
      return SPRITE_ANIM_CHARGING;
    default:
      return SPRITE_ANIM_IDLE;
  }
}

void sprites_init(void) {
  s_current_bitmap = NULL;
}

void sprites_deinit(void) {
  if (s_current_bitmap) {
    gbitmap_destroy(s_current_bitmap);
    s_current_bitmap = NULL;
  }
}

void sprites_set_breed(CatBreed breed) {
  if (breed >= NUM_CAT_BREEDS) {
    breed = CAT_BREED_ORANGE;
  }
  s_breed = breed;
}

uint32_t sprites_get_breed_resource(SpriteAnim anim, uint8_t frame_index) {
  return prv_breed_resource(s_breed, anim, frame_index);
}

uint32_t sprites_get_resource_for_state(PetState state, uint8_t frame_index) {
  if (state == PET_STATE_DEAD) {
    return RESOURCE_ID_IMAGE_TOMBSTONE;
  }
  SpriteAnim anim = prv_anim_for_state(state);
  uint8_t max_frames = s_frame_counts[state];
  if (max_frames == 0) {
    max_frames = 1;
  }
  uint8_t frame = frame_index % max_frames;
  return prv_breed_resource(s_breed, anim, frame);
}

uint8_t sprites_get_frame_count(PetState state) {
  if (state >= NUM_PET_STATES) {
    return 1;
  }
  uint8_t count = s_frame_counts[state];
  return count > 0 ? count : 1;
}

static void prv_destroy_current(void) {
  if (s_current_bitmap) {
    gbitmap_destroy(s_current_bitmap);
    s_current_bitmap = NULL;
  }
}

void sprites_load_frame(PetState state, uint8_t frame_index) {
  uint32_t resource_id = sprites_get_resource_for_state(state, frame_index);
  prv_destroy_current();
  s_current_bitmap = gbitmap_create_with_resource(resource_id);
}

void sprites_load_shared(uint32_t resource_id) {
  prv_destroy_current();
  s_current_bitmap = gbitmap_create_with_resource(resource_id);
}

GBitmap *sprites_get_current(void) {
  return s_current_bitmap;
}
