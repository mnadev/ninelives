#include "sprites.h"

static GBitmap *s_slot_bitmaps[MAX_SPRITE_SLOTS];

static const uint8_t s_frame_counts[NUM_PET_STATES] = {
  [PET_STATE_ENERGETIC] = 3,
  [PET_STATE_LAZY] = 2,
  [PET_STATE_DISTRESSED] = 2,
  [PET_STATE_SLEEPING] = 2,
  [PET_STATE_DEAD] = 1,
  [PET_STATE_CHARGING] = 2,
  [PET_STATE_LOW_BATTERY] = 2,
};

static uint32_t prv_orange_resource(SpriteAnim anim, uint8_t frame, CatFacing facing) {
  static const uint32_t walk_right[3] = {
    RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_0,
    RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_1,
    RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_2,
  };
  static const uint32_t walk_left[3] = {
    RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_LEFT_1,
    RESOURCE_ID_IMAGE_CAT_ORANGE_WALK_LEFT_2,
  };
  static const uint32_t idle_right[2] = {
    RESOURCE_ID_IMAGE_CAT_ORANGE_IDLE_0,
    RESOURCE_ID_IMAGE_CAT_ORANGE_IDLE_1,
  };
  static const uint32_t idle_left[2] = {
    RESOURCE_ID_IMAGE_CAT_ORANGE_IDLE_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_ORANGE_IDLE_LEFT_1,
  };
  static const uint32_t distressed[2] = {
    RESOURCE_ID_IMAGE_CAT_ORANGE_DISTRESSED_0,
    RESOURCE_ID_IMAGE_CAT_ORANGE_DISTRESSED_1,
  };
  static const uint32_t sleep[2] = {
    RESOURCE_ID_IMAGE_CAT_ORANGE_SLEEP_0,
    RESOURCE_ID_IMAGE_CAT_ORANGE_SLEEP_1,
  };
  static const uint32_t charging[2] = {
    RESOURCE_ID_IMAGE_CAT_ORANGE_CHARGING_0,
    RESOURCE_ID_IMAGE_CAT_ORANGE_CHARGING_1,
  };

  switch (anim) {
    case SPRITE_ANIM_WALK:
      return facing == CAT_FACING_LEFT ? walk_left[frame % 3] : walk_right[frame % 3];
    case SPRITE_ANIM_IDLE:
      return facing == CAT_FACING_LEFT ? idle_left[frame % 2] : idle_right[frame % 2];
    case SPRITE_ANIM_DISTRESSED:
      return distressed[frame % 2];
    case SPRITE_ANIM_SLEEP:
      return sleep[frame % 2];
    case SPRITE_ANIM_CHARGING:
      return charging[frame % 2];
    default:
      return walk_right[0];
  }
}

static uint32_t prv_tuxedo_resource(SpriteAnim anim, uint8_t frame, CatFacing facing) {
  static const uint32_t walk_right[3] = {
    RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_0,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_1,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_2,
  };
  static const uint32_t walk_left[3] = {
    RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_LEFT_1,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_WALK_LEFT_2,
  };
  static const uint32_t idle_right[2] = {
    RESOURCE_ID_IMAGE_CAT_TUXEDO_IDLE_0,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_IDLE_1,
  };
  static const uint32_t idle_left[2] = {
    RESOURCE_ID_IMAGE_CAT_TUXEDO_IDLE_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_IDLE_LEFT_1,
  };
  static const uint32_t distressed[2] = {
    RESOURCE_ID_IMAGE_CAT_TUXEDO_DISTRESSED_0,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_DISTRESSED_1,
  };
  static const uint32_t sleep[2] = {
    RESOURCE_ID_IMAGE_CAT_TUXEDO_SLEEP_0,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_SLEEP_1,
  };
  static const uint32_t charging[2] = {
    RESOURCE_ID_IMAGE_CAT_TUXEDO_CHARGING_0,
    RESOURCE_ID_IMAGE_CAT_TUXEDO_CHARGING_1,
  };

  switch (anim) {
    case SPRITE_ANIM_WALK:
      return facing == CAT_FACING_LEFT ? walk_left[frame % 3] : walk_right[frame % 3];
    case SPRITE_ANIM_IDLE:
      return facing == CAT_FACING_LEFT ? idle_left[frame % 2] : idle_right[frame % 2];
    case SPRITE_ANIM_DISTRESSED:
      return distressed[frame % 2];
    case SPRITE_ANIM_SLEEP:
      return sleep[frame % 2];
    case SPRITE_ANIM_CHARGING:
      return charging[frame % 2];
    default:
      return walk_right[0];
  }
}

static uint32_t prv_siamese_resource(SpriteAnim anim, uint8_t frame, CatFacing facing) {
  static const uint32_t walk_right[3] = {
    RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_0,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_1,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_2,
  };
  static const uint32_t walk_left[3] = {
    RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_LEFT_1,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_WALK_LEFT_2,
  };
  static const uint32_t idle_right[2] = {
    RESOURCE_ID_IMAGE_CAT_SIAMESE_IDLE_0,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_IDLE_1,
  };
  static const uint32_t idle_left[2] = {
    RESOURCE_ID_IMAGE_CAT_SIAMESE_IDLE_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_IDLE_LEFT_1,
  };
  static const uint32_t distressed[2] = {
    RESOURCE_ID_IMAGE_CAT_SIAMESE_DISTRESSED_0,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_DISTRESSED_1,
  };
  static const uint32_t sleep[2] = {
    RESOURCE_ID_IMAGE_CAT_SIAMESE_SLEEP_0,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_SLEEP_1,
  };
  static const uint32_t charging[2] = {
    RESOURCE_ID_IMAGE_CAT_SIAMESE_CHARGING_0,
    RESOURCE_ID_IMAGE_CAT_SIAMESE_CHARGING_1,
  };

  switch (anim) {
    case SPRITE_ANIM_WALK:
      return facing == CAT_FACING_LEFT ? walk_left[frame % 3] : walk_right[frame % 3];
    case SPRITE_ANIM_IDLE:
      return facing == CAT_FACING_LEFT ? idle_left[frame % 2] : idle_right[frame % 2];
    case SPRITE_ANIM_DISTRESSED:
      return distressed[frame % 2];
    case SPRITE_ANIM_SLEEP:
      return sleep[frame % 2];
    case SPRITE_ANIM_CHARGING:
      return charging[frame % 2];
    default:
      return walk_right[0];
  }
}

static uint32_t prv_void_resource(SpriteAnim anim, uint8_t frame, CatFacing facing) {
  static const uint32_t walk_right[3] = {
    RESOURCE_ID_IMAGE_CAT_VOID_WALK_0,
    RESOURCE_ID_IMAGE_CAT_VOID_WALK_1,
    RESOURCE_ID_IMAGE_CAT_VOID_WALK_2,
  };
  static const uint32_t walk_left[3] = {
    RESOURCE_ID_IMAGE_CAT_VOID_WALK_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_VOID_WALK_LEFT_1,
    RESOURCE_ID_IMAGE_CAT_VOID_WALK_LEFT_2,
  };
  static const uint32_t idle_right[2] = {
    RESOURCE_ID_IMAGE_CAT_VOID_IDLE_0,
    RESOURCE_ID_IMAGE_CAT_VOID_IDLE_1,
  };
  static const uint32_t idle_left[2] = {
    RESOURCE_ID_IMAGE_CAT_VOID_IDLE_LEFT_0,
    RESOURCE_ID_IMAGE_CAT_VOID_IDLE_LEFT_1,
  };
  static const uint32_t distressed[2] = {
    RESOURCE_ID_IMAGE_CAT_VOID_DISTRESSED_0,
    RESOURCE_ID_IMAGE_CAT_VOID_DISTRESSED_1,
  };
  static const uint32_t sleep[2] = {
    RESOURCE_ID_IMAGE_CAT_VOID_SLEEP_0,
    RESOURCE_ID_IMAGE_CAT_VOID_SLEEP_1,
  };
  static const uint32_t charging[2] = {
    RESOURCE_ID_IMAGE_CAT_VOID_CHARGING_0,
    RESOURCE_ID_IMAGE_CAT_VOID_CHARGING_1,
  };

  switch (anim) {
    case SPRITE_ANIM_WALK:
      return facing == CAT_FACING_LEFT ? walk_left[frame % 3] : walk_right[frame % 3];
    case SPRITE_ANIM_IDLE:
      return facing == CAT_FACING_LEFT ? idle_left[frame % 2] : idle_right[frame % 2];
    case SPRITE_ANIM_DISTRESSED:
      return distressed[frame % 2];
    case SPRITE_ANIM_SLEEP:
      return sleep[frame % 2];
    case SPRITE_ANIM_CHARGING:
      return charging[frame % 2];
    default:
      return walk_right[0];
  }
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

static uint32_t prv_breed_resource(CatBreed breed, SpriteAnim anim, uint8_t frame, CatFacing facing) {
  switch (breed) {
    case CAT_BREED_TUXEDO:
      return prv_tuxedo_resource(anim, frame, facing);
    case CAT_BREED_SIAMESE:
      return prv_siamese_resource(anim, frame, facing);
    case CAT_BREED_VOID:
      return prv_void_resource(anim, frame, facing);
    case CAT_BREED_ORANGE:
    default:
      return prv_orange_resource(anim, frame, facing);
  }
}

void sprites_init(void) {
  for (int i = 0; i < MAX_SPRITE_SLOTS; i++) {
    s_slot_bitmaps[i] = NULL;
  }
}

void sprites_deinit(void) {
  for (int i = 0; i < MAX_SPRITE_SLOTS; i++) {
    if (s_slot_bitmaps[i]) {
      gbitmap_destroy(s_slot_bitmaps[i]);
      s_slot_bitmaps[i] = NULL;
    }
  }
}

uint32_t sprites_get_resource_for_breed(CatBreed breed, PetState state, uint8_t frame_index, CatFacing facing) {
  if (state == PET_STATE_DEAD) {
    return RESOURCE_ID_IMAGE_TOMBSTONE;
  }
  SpriteAnim anim = prv_anim_for_state(state);
  uint8_t max_frames = s_frame_counts[state];
  if (max_frames == 0) {
    max_frames = 1;
  }
  uint8_t frame = frame_index % max_frames;
  if (breed >= NUM_CAT_BREEDS) {
    breed = CAT_BREED_ORANGE;
  }
  return prv_breed_resource(breed, anim, frame, facing);
}

uint8_t sprites_get_frame_count(PetState state) {
  if (state >= NUM_PET_STATES) {
    return 1;
  }
  uint8_t count = s_frame_counts[state];
  return count > 0 ? count : 1;
}

static void prv_destroy_slot(uint8_t slot) {
  if (slot >= MAX_SPRITE_SLOTS) {
    return;
  }
  if (s_slot_bitmaps[slot]) {
    gbitmap_destroy(s_slot_bitmaps[slot]);
    s_slot_bitmaps[slot] = NULL;
  }
}

void sprites_load_slot(uint8_t slot, CatBreed breed, PetState state, uint8_t frame_index, CatFacing facing) {
  if (slot >= MAX_SPRITE_SLOTS) {
    return;
  }
  uint32_t resource_id = sprites_get_resource_for_breed(breed, state, frame_index, facing);
  prv_destroy_slot(slot);
  s_slot_bitmaps[slot] = gbitmap_create_with_resource(resource_id);
}

void sprites_load_shared_slot(uint8_t slot, uint32_t resource_id) {
  if (slot >= MAX_SPRITE_SLOTS) {
    return;
  }
  prv_destroy_slot(slot);
  s_slot_bitmaps[slot] = gbitmap_create_with_resource(resource_id);
}

GBitmap *sprites_get_slot(uint8_t slot) {
  if (slot >= MAX_SPRITE_SLOTS) {
    return NULL;
  }
  return s_slot_bitmaps[slot];
}
