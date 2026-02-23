#include "spatial_audio.h"
#include "a11y_strings.h"
#include "variables.h"
#include "features.h"
#include "tile_detect.h"
#include "zelda_rtl.h"
#include "assets.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#if defined(__APPLE__)
#include "platform/macos/speechsynthesis.h"
#elif defined(_WIN32)
#include "platform/win32/speechsynthesis.h"
#endif

typedef struct SpatialCue {
  int16 dx, dy;
  bool active;
} SpatialCue;

// Snapshot: one cue per category
static SpatialCue g_cue_snapshot[kSpatialCue_Count];

// Oscillator state: persistent phase accumulators (never reset)
static uint32 g_phase[kSpatialCue_Count];

// Precomputed base phase increments (16.16 fixed point)
static uint32 g_phase_inc[kSpatialCue_Count];

// 256-entry sine table
static int16 g_sine_table[256];

// Pitch multiplier table: 9 entries for -4..+4 semitones (1.15 fixed point)
static uint16 g_pitch_mult[9];

static bool g_enabled;
static int g_sample_rate;

// NPC ding envelope state
static uint32 g_ding_envelope;
static int g_ding_timer;
static uint32 g_ding_decay;

// Enemy ding envelope state
static uint32 g_enemy_envelope;
static int g_enemy_timer;
static uint32 g_enemy_decay;

// Door ding envelope state
static uint32 g_door_envelope;
static int g_door_timer;
static uint32 g_door_decay;

// Stair ding envelope state (shared by both up/down)
static uint32 g_stair_envelope;
static int g_stair_timer;
static uint32 g_stair_decay;

// Stair sweep counters (continuous pitch sweep per sample)
static uint32 g_stair_sweep;

// Blocked ding
static uint32 g_blocked_envelope;
static int g_blocked_timer;
static uint32 g_blocked_decay;
static uint32 g_blocked_phase;
static uint32 g_blocked_phase_inc;
static bool g_blocked_active;
static uint16 g_prev_link_x, g_prev_link_y;
static int g_blocked_frame_count;

// Room/screen change chime
static uint32 g_room_chime_envelope;
static uint32 g_room_chime_phase;
static uint32 g_room_chime_phase_inc;
static uint32 g_room_chime_decay;
static uint16 g_prev_ow_screen;
static uint16 g_prev_dung_room;

// Menu accessibility
static uint8 g_prev_module;
static uint8 g_prev_submodule;
static uint8 g_prev_cursor;

// Inventory tracking
static uint8 g_prev_hud_item;
static uint8 g_prev_hud_item_x;
static bool g_prev_menu_open;

// Dungeon name tracking
static uint16 g_prev_palace_index;
static bool g_prev_indoors;

// Overworld map / flute / dungeon map tracking
static bool g_prev_ow_map_active;
static uint8 g_prev_flute_sel;
static bool g_prev_flute_active;
static bool g_prev_dung_map_active;
static uint16 g_prev_dungmap_floor;

// Item collection tracking
static uint16 g_prev_rupees;
static uint8 g_prev_health;
static uint8 g_prev_arrows;
static uint8 g_prev_bombs;
static uint8 g_prev_keys;
static bool g_tracking_initialized;

// Name registration tracking
static uint8 g_prev_name_col;
static uint8 g_prev_name_row;

// Floor tracking
static uint8 g_prev_floor;
static bool g_floor_tracking_init;

// Overworld area name tracking
static uint16 g_prev_ow_area;

// Movement direction tracking
static uint8 g_prev_facing;

// Nearest overworld entrance tracking (for TTS)
static int g_nearest_entrance_idx;
static int g_nearest_entrance_dist2;   // squared distance to nearest entrance
static int g_prev_nearest_entrance_idx;
static bool g_was_indoors;             // track indoor→outdoor transitions
static int g_outdoor_suppress_frames;  // suppress entrance TTS after exiting

// Passage chime state (wall distance jump indicator)
static int g_prev_beam_dist[4];      // previous frame narrow-beam wall distance per direction
static int g_passage_pos[4];         // chime sample position, -1 = inactive
static uint32 g_passage_phase[4];    // oscillator phase
static uint32 g_passage_inc1[4];     // note 1 phase increment
static uint32 g_passage_inc2[4];     // note 2 phase increment
static int g_passage_pan_L[4];       // left gain
static int g_passage_pan_R[4];       // right gain
static int g_passage_chime_len;      // total chime duration in samples
static uint32 g_d5_inc, g_d6_inc, g_d7_inc;  // D5/D6/D7 phase increments

// Hole ding envelope (for continuous tone)
static uint32 g_hole_envelope;
static int g_hole_timer;
static uint32 g_hole_decay;
static uint32 g_hole_sweep;  // descending sweep counter

// Liftable "swipe" (ascending chirp) state
static uint32 g_lift_envelope;
static int g_lift_timer;
static uint32 g_lift_decay;
static uint32 g_lift_sweep;  // ascending sweep counter

// Chest "sparkle" (vibrato shimmer) state
static uint32 g_chest_envelope;
static int g_chest_timer;
static uint32 g_chest_decay;

// NPC double-chime state
static int g_npc_beep_pos;
static uint32 g_npc_phase_inc2;  // E4 (330Hz) second note

// Hole proximity warning ding (plays when moving near a hole)
static uint32 g_hole_warn_envelope;
static int g_hole_warn_timer;
static uint32 g_hole_warn_decay;
static uint32 g_hole_warn_phase;
static uint32 g_hole_warn_phase_inc_base;   // 1200Hz base
static uint32 g_hole_warn_phase_inc_hi;     // 1400Hz (hole above)
static uint32 g_hole_warn_phase_inc_lo;     // 1000Hz (hole below)
static bool g_hole_warn_active;
static int16 g_hole_warn_dx, g_hole_warn_dy; // direction of nearest danger hole

// Enemy combat dings
static uint32 g_sword_range_envelope, g_sword_range_phase;
static int g_sword_range_timer;
static uint32 g_sword_range_decay, g_sword_range_phase_inc;
static bool g_sword_range_active;

static uint32 g_danger_phase;
static uint32 g_danger_phase_inc;
static bool g_danger_active;
static bool g_danger_prev;          // previous frame state for edge detection
static int g_danger_drill_pos;      // sample position within drill sound
static int g_danger_drill_len;      // total drill length in samples
static int g_danger_exit_pos;       // sample position within exit chime
static int g_danger_exit_len;       // total exit chime length in samples
static uint32 g_danger_exit_inc1;   // E4 phase increment
static uint32 g_danger_exit_inc2;   // B3 phase increment

// Sword range double-beep: second tone (ascending)
static uint32 g_sword_range_phase_inc2;
static int g_sword_range_beep_pos;

// Alignment sonar ping (variable rate based on precision)
static uint32 g_align_envelope;
static int g_align_timer;
static int g_align_interval;  // samples between pings (varies with precision)
static uint32 g_align_decay;
static uint32 g_align_phase;
static uint32 g_align_phase_inc;
static int g_align_offset;    // how many px off-axis (0 = perfect)

// Sprite hitbox size tables (duplicated from sprite.c for danger zone calculation)
static const uint8 kHitboxXSize[32] = {
  12, 1, 16, 20, 20, 8, 4, 32, 48, 24, 32, 32, 32, 48, 12, 12,
  60, 124, 12, 32, 4, 12, 48, 32, 40, 8, 24, 24, 5, 80, 4, 8,
};
static const uint8 kHitboxYSize[32] = {
  14, 1, 16, 21, 24, 4, 8, 40, 20, 24, 40, 29, 36, 48, 60, 124,
  12, 12, 17, 28, 4, 2, 28, 20, 10, 4, 24, 16, 5, 48, 8, 12,
};

// Wall click envelope state (4 directions: N, S, E, W)
static uint32 g_wall_envelope[4];
static int g_wall_timer[4];
static uint32 g_wall_decay;
static int g_wall_interval[4];  // repeat interval per direction (set by scan)

// Ledge envelope state
static uint32 g_ledge_envelope;
static int g_ledge_timer;
static uint32 g_ledge_decay;
static uint32 g_ledge_sweep;       // sweep counter

// Deep water envelope state
static uint32 g_water_envelope;
static int g_water_timer;
static uint32 g_water_decay;

// Hazard/spikes envelope state
static uint32 g_hazard_envelope;
static int g_hazard_timer;
static uint32 g_hazard_decay;

// Conveyor envelope state
static int g_conveyor_timer;
static int g_conveyor_cycle_len;   // total cycle length in samples

// Terrain underfoot tracking
enum {
  kTerrain_Normal,
  kTerrain_Grass,
  kTerrain_ShallowWater,
  kTerrain_Ice,
  kTerrain_Spike,
};
static int g_terrain_type;
static int g_prev_terrain_type;
static uint32 g_terrain_phase;
static uint32 g_terrain_phase_inc;
static int g_terrain_pos;          // sample position, -1 = inactive
static int g_terrain_len;          // total tone length in samples
static uint32 g_terrain_decay;
static uint32 g_terrain_envelope;
static bool g_terrain_am;          // amplitude modulation flag for shallow water

// Sound legend state
static bool g_legend_active;
static int g_legend_index;
static int g_legend_demo_pos;      // sample position, -1 = inactive
static int g_legend_demo_type;     // which cue to demo
static uint32 g_legend_demo_phase;
static uint32 g_legend_demo_sweep; // sweep counter for ledge/stair demos

// Base frequencies per category
static const uint16 kBaseFreq[kSpatialCue_Count] = {
  250,  // WallN — forward wall, higher pitch
  120,  // WallS — behind wall, lower pitch
  170,  // WallE — right wall
  170,  // WallW — left wall
  300,  // HoleL — descending sweep 300→150Hz ("falling")
  300,  // HoleR — same sound, split by side
  440,  // Enemy (A4) — square wave buzz
  262,  // NPC (C4) — first note of ascending double-chime
  523,  // Chest (C5) — vibrato shimmer
  200,  // Liftable — ascending chirp 200→600Hz ("swipe")
  392,  // Door (G4)
  500,  // StairUp — rising sweep from 500Hz
  500,  // StairDown — falling sweep from 500Hz
  600,  // Ledge — descending sweep 600→300Hz
  180,  // DeepWater — wavering low tone
  800,  // Hazard — harsh rapid buzz
  300,  // Conveyor — double-pulse pattern
};

// Per-group volume control (0-100, default 100)
static int g_cue_group_volume[kCueGroup_Count];
static int g_scan_range = 96;  // was SCAN_RANGE macro

// Options menu state
static bool g_options_active;
static int g_options_index;
static int g_options_menu;   // 0=top, 1=voice submenu, 2=sound setup

static int CueToGroup(int cue) {
  switch (cue) {
  case kSpatialCue_WallN: case kSpatialCue_WallS:
  case kSpatialCue_WallE: case kSpatialCue_WallW: return kCueGroup_Walls;
  case kSpatialCue_HoleL: case kSpatialCue_HoleR: return kCueGroup_Holes;
  case kSpatialCue_Enemy: return kCueGroup_Enemy;
  case kSpatialCue_NPC: return kCueGroup_NPC;
  case kSpatialCue_Chest: return kCueGroup_Chest;
  case kSpatialCue_Liftable: return kCueGroup_Liftable;
  case kSpatialCue_Door: return kCueGroup_Door;
  case kSpatialCue_StairUp: case kSpatialCue_StairDown: return kCueGroup_Stairs;
  case kSpatialCue_Ledge: return kCueGroup_Ledge;
  case kSpatialCue_DeepWater: return kCueGroup_DeepWater;
  case kSpatialCue_Hazard: return kCueGroup_Hazard;
  case kSpatialCue_Conveyor: return kCueGroup_Conveyor;
  default: return -1;
  }
}

#define SCAN_RANGE g_scan_range

static uint32 isqrt32(uint32 n) {
  if (n == 0) return 0;
  uint32 x = n;
  uint32 y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}

#if defined(__APPLE__) || defined(_WIN32)
static void Speak(const char *fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  SpeechSynthesis_Speak(buf);
}

// Direction lookup by link_direction_facing value
static const char *GetDirectionName(uint8 dir, bool lowercase) {
  if (lowercase) {
    switch (dir) {
    case 0: return A11y(kA11y_north);
    case 2: return A11y(kA11y_south);
    case 4: return A11y(kA11y_left);
    case 6: return A11y(kA11y_right);
    default: return NULL;
    }
  }
  switch (dir) {
  case 0: return A11y(kA11y_North);
  case 2: return A11y(kA11y_South);
  case 4: return A11y(kA11y_Left);
  case 6: return A11y(kA11y_Right);
  default: return NULL;
  }
}
#endif  // __APPLE__

void SpatialAudio_Init(int sample_rate) {
  g_sample_rate = sample_rate;
  g_enabled = false;
  memset(g_phase, 0, sizeof(g_phase));
  memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));

  // Initialize cue group volumes to 100%
  for (int i = 0; i < kCueGroup_Count; i++)
    g_cue_group_volume[i] = 100;
  g_scan_range = 96;
  g_options_active = false;
  g_options_index = 0;
  g_options_menu = 0;

  for (int i = 0; i < 256; i++)
    g_sine_table[i] = (int16)(sinf(2.0f * 3.14159265f * i / 256.0f) * 16383.0f);

  for (int i = 0; i < kSpatialCue_Count; i++)
    g_phase_inc[i] = (uint32)((uint64)kBaseFreq[i] * 256 * 65536 / sample_rate);

  for (int i = 0; i < 9; i++) {
    float semitones = (float)(i - 4);
    g_pitch_mult[i] = (uint16)(powf(2.0f, semitones / 12.0f) * 32768.0f);
  }

  // Wall click: 120ms decay, variable repeat (set per frame by scan)
  {
    int ds = sample_rate * 120 / 1000;
    g_wall_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
    memset(g_wall_envelope, 0, sizeof(g_wall_envelope));
    memset(g_wall_timer, 0, sizeof(g_wall_timer));
    memset(g_wall_interval, 0, sizeof(g_wall_interval));
  }

  // NPC double-chime: C4 (262Hz) then E4 (330Hz), 60ms each, 1.2s repeat
  {
    int ds = sample_rate * 60 / 1000;
    g_ding_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
    g_ding_envelope = 0;
    g_ding_timer = 0;
    g_npc_beep_pos = 0;
    g_npc_phase_inc2 = (uint32)((uint64)330 * 256 * 65536 / sample_rate);
  }

  // Enemy: 100ms decay, 200ms repeat (now square wave — waveform change in MixAudio)
  int ds = sample_rate * 100 / 1000;
  g_enemy_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_enemy_envelope = 0;
  g_enemy_timer = 0;

  // Door ding: 200ms decay, 400ms repeat
  ds = sample_rate * 200 / 1000;
  g_door_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_door_envelope = 0;
  g_door_timer = 0;

  // Stair ding: 150ms decay, 300ms repeat
  ds = sample_rate * 150 / 1000;
  g_stair_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_stair_envelope = 0;
  g_stair_timer = 0;

  // Hole "falling tone": 200ms decay, 500ms repeat, descending sweep 300→150Hz
  ds = sample_rate * 200 / 1000;
  g_hole_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_hole_envelope = 0;
  g_hole_timer = 0;
  g_hole_sweep = 0;

  // Hole proximity warning: 1200Hz base, +200 for up holes, -200 for down holes
  g_hole_warn_phase_inc_base = (uint32)((uint64)1200 * 256 * 65536 / sample_rate);
  g_hole_warn_phase_inc_hi   = (uint32)((uint64)1400 * 256 * 65536 / sample_rate);
  g_hole_warn_phase_inc_lo   = (uint32)((uint64)1000 * 256 * 65536 / sample_rate);
  ds = sample_rate * 80 / 1000;
  g_hole_warn_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_hole_warn_envelope = 0;
  g_hole_warn_timer = 0;
  g_hole_warn_phase = 0;
  g_hole_warn_active = false;
  g_hole_warn_dx = g_hole_warn_dy = 0;

  // Sword range double-beep: 880Hz then 1100Hz, 40ms each, repeats every 500ms
  g_sword_range_phase_inc = (uint32)((uint64)880 * 256 * 65536 / sample_rate);
  g_sword_range_phase_inc2 = (uint32)((uint64)1100 * 256 * 65536 / sample_rate);
  ds = sample_rate * 40 / 1000;
  g_sword_range_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_sword_range_envelope = 0; g_sword_range_timer = 0; g_sword_range_phase = 0;
  g_sword_range_beep_pos = 0;
  g_sword_range_active = false;

  // Danger drill: 300Hz tone amplitude-modulated at 25Hz for drill effect
  g_danger_phase_inc = (uint32)((uint64)300 * 256 * 65536 / sample_rate);
  g_danger_phase = 0;
  g_danger_active = false;
  g_danger_prev = false;
  g_danger_drill_pos = -1;
  g_danger_drill_len = sample_rate * 300 / 1000;  // 300ms drill
  // Exit chime: E4 (330Hz) then B3 (247Hz), 40ms each with 40ms gap = 120ms total
  g_danger_exit_inc1 = (uint32)((uint64)330 * 256 * 65536 / sample_rate);
  g_danger_exit_inc2 = (uint32)((uint64)247 * 256 * 65536 / sample_rate);
  g_danger_exit_pos = -1;
  g_danger_exit_len = sample_rate * 120 / 1000;

  // Alignment sonar: 1500Hz ping, 30ms decay
  g_align_phase_inc = (uint32)((uint64)1500 * 256 * 65536 / sample_rate);
  ds = sample_rate * 30 / 1000;
  g_align_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_align_envelope = 0; g_align_timer = 0; g_align_phase = 0;
  g_align_interval = 0; g_align_offset = SCAN_RANGE;

  // Blocked ding: 110Hz, 200ms decay
  g_blocked_phase_inc = (uint32)((uint64)110 * 256 * 65536 / sample_rate);
  ds = sample_rate * 200 / 1000;
  g_blocked_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_blocked_envelope = 0;
  g_blocked_timer = 0;
  g_blocked_phase = 0;
  g_blocked_active = false;
  g_prev_link_x = g_prev_link_y = 0;
  g_blocked_frame_count = 0;

  // Room change chime: 587 Hz (D5), 300ms decay
  g_room_chime_phase_inc = (uint32)((uint64)587 * 256 * 65536 / sample_rate);
  ds = sample_rate * 300 / 1000;
  g_room_chime_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_room_chime_envelope = 0;
  g_room_chime_phase = 0;
  g_prev_ow_screen = 0xFFFF;
  g_prev_dung_room = 0xFFFF;

  // Menu/choice/inventory state
  g_prev_module = 0xFF;
  g_prev_submodule = 0xFF;
  g_prev_cursor = 0xFF;
  g_prev_hud_item = 0xFF;
  g_prev_hud_item_x = 0xFF;
  g_prev_menu_open = false;
  g_prev_ow_map_active = false;
  g_prev_flute_active = false;
  g_prev_flute_sel = 0xFF;
  g_prev_dung_map_active = false;
  g_prev_dungmap_floor = 0xFFFF;
  g_prev_palace_index = 0xFFFF;
  g_prev_indoors = false;
  g_tracking_initialized = false;
  g_prev_name_col = 0xFF;
  g_prev_name_row = 0xFF;
  g_prev_floor = 0;
  g_floor_tracking_init = false;
  g_prev_ow_area = 0xFFFF;
  g_prev_facing = 0xFF;
  g_nearest_entrance_idx = -1;
  g_nearest_entrance_dist2 = 0;
  g_prev_nearest_entrance_idx = -1;
  g_was_indoors = false;
  g_outdoor_suppress_frames = 0;

  // Ledge: 600Hz, 80ms decay, 350ms repeat
  ds = sample_rate * 80 / 1000;
  g_ledge_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_ledge_envelope = 0; g_ledge_timer = 0; g_ledge_sweep = 0;

  // Liftable "swipe": ascending chirp 200→600Hz over 120ms, 700ms repeat
  ds = sample_rate * 120 / 1000;
  g_lift_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_lift_envelope = 0; g_lift_timer = 0; g_lift_sweep = 0;

  // Chest "sparkle": 523Hz + 8Hz vibrato, 200ms decay, 800ms repeat
  ds = sample_rate * 200 / 1000;
  g_chest_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_chest_envelope = 0; g_chest_timer = 0;

  // Deep water: 180Hz, 250ms decay, 600ms repeat
  ds = sample_rate * 250 / 1000;
  g_water_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_water_envelope = 0; g_water_timer = 0;

  // Hazard: 800Hz, 40ms decay, 150ms repeat
  ds = sample_rate * 40 / 1000;
  g_hazard_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_hazard_envelope = 0; g_hazard_timer = 0;

  // Conveyor: 300Hz, 400ms cycle (double-pulse)
  g_conveyor_timer = 0;
  g_conveyor_cycle_len = sample_rate * 400 / 1000;

  // Terrain underfoot
  g_terrain_type = kTerrain_Normal;
  g_prev_terrain_type = kTerrain_Normal;
  g_terrain_phase = 0; g_terrain_phase_inc = 0;
  g_terrain_pos = -1; g_terrain_len = 0;
  g_terrain_decay = 0; g_terrain_envelope = 0;
  g_terrain_am = false;

  // Sound legend
  g_legend_active = false;
  g_legend_index = 0;
  g_legend_demo_pos = -1;
  g_legend_demo_type = 0;
  g_legend_demo_phase = 0;
  g_legend_demo_sweep = 0;

  // Passage chime: D5=587Hz, D6=1175Hz, D7=2349Hz
  g_d5_inc = (uint32)((uint64)587 * 256 * 65536 / sample_rate);
  g_d6_inc = (uint32)((uint64)1175 * 256 * 65536 / sample_rate);
  g_d7_inc = (uint32)((uint64)2349 * 256 * 65536 / sample_rate);
  g_passage_chime_len = sample_rate * 120 / 1000;  // 120ms total
  for (int i = 0; i < 4; i++) { g_prev_beam_dist[i] = SCAN_RANGE; g_passage_pos[i] = -1; }

  // Load saved settings (overrides defaults above)
  SpatialAudio_LoadSettings();
}

void SpatialAudio_Shutdown(void) {
  g_enabled = false;
}

void SpatialAudio_Toggle(void) {
  g_enabled = !g_enabled;
  if (!g_enabled) {
    memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));
    if (g_legend_active) {
      g_legend_active = false;
      g_legend_demo_pos = -1;
    }
    if (g_options_active) {
      g_options_active = false;
    }
  }
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_Speak(g_enabled ? A11y(kA11y_On) : A11y(kA11y_Off));
#endif
}

void SpatialAudio_Enable(void) {
  if (!g_enabled)
    SpatialAudio_Toggle();
}

bool SpatialAudio_IsEnabled(void) {
  return g_enabled;
}

void SpatialAudio_SpeakHealth(void) {
  if (!g_enabled) return;
#if defined(__APPLE__) || defined(_WIN32)
  uint8 cur = link_health_current;
  uint8 cap = link_health_capacity;
  int hearts = cur / 8;
  int half = (cur % 8) >= 4 ? 1 : 0;
  int max_hearts = cap / 8;
  if (half)
    Speak(A11y(kA11y_FmtHeartsHalf), hearts, max_hearts);
  else
    Speak(A11y(kA11y_FmtHearts), hearts, max_hearts);
#endif
}

void SpatialAudio_SpeakLocation(void) {
  if (!g_enabled) return;
#if defined(__APPLE__) || defined(_WIN32)
  uint8 mod = main_module_index;
  if (mod != 7 && mod != 9 && mod != 14) {
    SpeechSynthesis_Speak(A11y(kA11y_InMenu));
    return;
  }

  // Area name
  const char *area = NULL;
  if (player_is_indoors) {
    uint8 idx = BYTE(cur_palace_index_x2) / 2;
    area = A11yDungeonName(idx);
  } else {
    area = A11yAreaName(overworld_area_index);
  }

  // Direction
  const char *dir = GetDirectionName(link_direction_facing, false);

  // Nearest entrance info (only overworld)
  const char *entrance = NULL;
  const char *entrance_dir = NULL;
  if (!player_is_indoors && g_nearest_entrance_idx >= 0) {
    uint8 eid = kOverworld_Entrance_Id[g_nearest_entrance_idx];
    entrance = A11yEntranceName(eid);
    if (g_cue_snapshot[kSpatialCue_Door].active) {
      int edx = g_cue_snapshot[kSpatialCue_Door].dx;
      int edy = g_cue_snapshot[kSpatialCue_Door].dy;
      int ax = edx < 0 ? -edx : edx;
      int ay = edy < 0 ? -edy : edy;
      if (ax > ay)
        entrance_dir = (edx > 0) ? A11y(kA11y_right) : A11y(kA11y_left);
      else
        entrance_dir = (edy > 0) ? A11y(kA11y_south) : A11y(kA11y_north);
    }
  }

  // Nearest NPC
  bool has_npc = g_cue_snapshot[kSpatialCue_NPC].active;
  const char *npc_dir = NULL;
  if (has_npc) {
    int ndx = g_cue_snapshot[kSpatialCue_NPC].dx;
    int ndy = g_cue_snapshot[kSpatialCue_NPC].dy;
    int ax = ndx < 0 ? -ndx : ndx;
    int ay = ndy < 0 ? -ndy : ndy;
    if (ax > ay)
      npc_dir = (ndx > 0) ? A11y(kA11y_right) : A11y(kA11y_left);
    else
      npc_dir = (ndy > 0) ? A11y(kA11y_south) : A11y(kA11y_north);
  }

  // Build and speak summary
  char buf[256];
  int pos = 0;
  if (area)
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s. ", area);
  if (dir)
    pos += snprintf(buf + pos, sizeof(buf) - pos, A11y(kA11y_FmtFacing), dir);
  if (entrance && entrance_dir)
    pos += snprintf(buf + pos, sizeof(buf) - pos, A11y(kA11y_FmtEntranceDir), entrance, entrance_dir);
  else if (entrance)
    pos += snprintf(buf + pos, sizeof(buf) - pos, A11y(kA11y_FmtEntranceNearby), entrance);
  if (has_npc && npc_dir)
    pos += snprintf(buf + pos, sizeof(buf) - pos, A11y(kA11y_FmtNPCDir), npc_dir);
  if (pos == 0)
    snprintf(buf, sizeof(buf), "%s", A11y(kA11y_FmtNothingNearby));

  SpeechSynthesis_Speak(buf);
#endif
}

#define TILE_CLASS_WALL -2  // intermediate: needs directional binning

static int ClassifyTile(uint8 tile) {
  if (tile >= 0x01 && tile <= 0x03) return TILE_CLASS_WALL;
  if (tile == 0x26 || tile == 0x43)  return TILE_CLASS_WALL;
  if (tile == 0x27)                  return TILE_CLASS_WALL;  // hookshottable walls
  if (tile == 0x42)                  return TILE_CLASS_WALL;  // gravestones (outdoor)
  if (tile == 0x57)                  return TILE_CLASS_WALL;  // bonk rocks
  if (tile == 0x20)                  return kSpatialCue_HoleL;  // placeholder, split by dx at scan time
  if (tile >= 0xB0 && tile <= 0xBD)  return kSpatialCue_HoleL;
  if (tile >= 0x50 && tile <= 0x56)  return kSpatialCue_Liftable;
  if ((tile >= 0x58 && tile <= 0x5D) || tile == 0x63) return kSpatialCue_Chest;
  if (tile >= 0x80 && tile <= 0x8D)  return kSpatialCue_Door;  // closed doors (not 0x8E/0x8F)
  if (tile >= 0x90 && tile <= 0xAF)  return kSpatialCue_Door;  // shutter doors
  if (tile >= 0xF0)                  return kSpatialCue_Door;  // flaggable doors
  // Entrances: 0x8E/0x8F
  if (tile == 0x8E || tile == 0x8F)  return kSpatialCue_Door;
  // Stairs going UP: 0x1D-0x1F (north layer stairs), 0x22, 0x30-0x33 (bit2=0), 0x5E (spiral up)
  if (tile >= 0x1D && tile <= 0x1F)  return kSpatialCue_StairUp;
  if (tile == 0x22)                  return kSpatialCue_StairUp;
  if (tile >= 0x30 && tile <= 0x33)  return kSpatialCue_StairUp;
  if (tile == 0x5E)                  return kSpatialCue_StairUp;
  // Stairs going DOWN: 0x34-0x37 (bit2=1), 0x38-0x3F (south/straight), 0x5F (spiral down)
  if (tile >= 0x34 && tile <= 0x3F)  return kSpatialCue_StairDown;
  if (tile == 0x5F)                  return kSpatialCue_StairDown;
  // Ledges (one-way drops): 0x28-0x2F
  if (tile >= 0x28 && tile <= 0x2F)  return kSpatialCue_Ledge;
  // Deep water: 0x08 (always), 0x0B handled at scan site (needs !indoors check)
  if (tile == 0x08)                  return kSpatialCue_DeepWater;
  // Hazard/spikes: 0x0D, 0x44
  if (tile == 0x0D || tile == 0x44)  return kSpatialCue_Hazard;
  // Conveyor belts: 0x68-0x6B
  if (tile >= 0x68 && tile <= 0x6B)  return kSpatialCue_Conveyor;
  return -1;
}

// Tiles that block movement and form corridor boundaries but already have
// their own audio cues (water tremolo, ledge sweep, liftable tone).
// These feed the passage detection beam without becoming wall cues.
static bool IsBeamOpaque(int cat, uint8 tile, bool indoors) {
  if (cat == kSpatialCue_DeepWater) return true;
  if (!indoors && tile == 0x0B) return true;  // outdoor deep water variant
  if (cat == kSpatialCue_Ledge) return true;
  if (cat == kSpatialCue_Liftable) return true;
  return false;
}

#if defined(__APPLE__) || defined(_WIN32)
// --- Menu TTS ---

static void DecodeSramName(int slot, char *out, int outlen) {
  static const char kSramToAscii[256] = {
    [0x00]='A', [0x01]='B', [0x02]='C', [0x03]='D', [0x04]='E', [0x05]='F',
    [0x06]='G', [0x07]='H', [0x09]='J', [0x0a]='K', [0x0b]='L', [0x0c]='M',
    [0x0d]='N', [0x0e]='O', [0x0f]='P',
    [0x20]='Q', [0x21]='R', [0x22]='S', [0x23]='T', [0x24]='U', [0x25]='V',
    [0x26]='W', [0x27]='X', [0x28]='Y', [0x29]='Z',
    [0x2a]='a', [0x2b]='b', [0x2c]='c', [0x2d]='d', [0x2e]='e', [0x2f]='f',
    [0x40]='g', [0x41]='h', [0x43]='j', [0x44]='k', [0x46]='m', [0x47]='n',
    [0x48]='o', [0x49]='p', [0x4a]='q', [0x4b]='r', [0x4c]='s', [0x4d]='t',
    [0x4e]='u', [0x4f]='v', [0x60]='w', [0x61]='x', [0x62]='y', [0x63]='z',
    [0x80]='0', [0x81]='1', [0x82]='2', [0x85]='5', [0x86]='6',
  };
  const uint8 *sram = g_zenv.sram + slot * 0x500;
  const uint16 *name = (const uint16 *)(sram + 0x3d9);
  int pos = 0;
  for (int i = 0; i < 6 && pos < outlen - 1; i++) {
    uint16 tile = name[i];
    if (tile == 0xa9) continue;
    char ch = kSramToAscii[tile & 0xFF];
    if (!ch) continue;
    out[pos++] = ch;
  }
  out[pos] = '\0';
}

static void AnnounceMenuState(void) {
  uint8 mod = main_module_index;
  uint8 sub = submodule_index;
  uint8 cursor = g_ram[0xc8];

  // Detect module or relevant submodule changes
  bool mod_changed = (mod != g_prev_module);
  bool sub_changed = (sub != g_prev_submodule);

  if (mod_changed || (mod <= 1 && sub_changed)) {
    // Handle title/intro sequence (module 0)
    if (mod == 0 && mod_changed) {
      SpeechSynthesis_Speak(A11y(kA11y_Nintendo));
    } else if (mod == 0 && sub >= 5 && g_prev_submodule < 5) {
      SpeechSynthesis_Speak(A11y(kA11y_LegendOfZelda));
    }

    if (mod_changed) {
      g_prev_module = mod;
      g_prev_cursor = 0xFF;
    }
    g_prev_submodule = sub;

    if (mod == 1 && sub == 5)
      SpeechSynthesis_Speak(A11y(kA11y_FileSelect));
    else if (mod == 2)
      SpeechSynthesis_Speak(A11y(kA11y_CopyPlayer));
    else if (mod == 3)
      SpeechSynthesis_Speak(A11y(kA11y_ErasePlayer));
    else if (mod == 4)
      SpeechSynthesis_Speak(A11y(kA11y_RegisterName));
  }

  if (mod == 1 && sub == 5 && cursor != g_prev_cursor) {
    g_prev_cursor = cursor;
    char buf[64];
    if (cursor < 3) {
      uint16 valid = *(uint16 *)(g_zenv.sram + cursor * 0x500 + 0x3E5);
      if (valid == 0x55AA) {
        char name[16];
        DecodeSramName(cursor, name, sizeof(name));
        snprintf(buf, sizeof(buf), A11y(kA11y_FmtFile), cursor + 1, name[0] ? name : A11y(kA11y_Unnamed));
      } else {
        snprintf(buf, sizeof(buf), A11y(kA11y_FmtFileEmpty), cursor + 1);
      }
    } else if (cursor == 3) {
      snprintf(buf, sizeof(buf), "%s", A11y(kA11y_CopyPlayer));
    } else {
      snprintf(buf, sizeof(buf), "%s", A11y(kA11y_ErasePlayer));
    }
    SpeechSynthesis_Speak(buf);
  }

  if ((mod == 2 || mod == 3) && cursor != g_prev_cursor) {
    g_prev_cursor = cursor;
    char buf[64];
    if (cursor < 3) {
      uint16 valid = *(uint16 *)(g_zenv.sram + cursor * 0x500 + 0x3E5);
      if (valid == 0x55AA) {
        char name[16];
        DecodeSramName(cursor, name, sizeof(name));
        snprintf(buf, sizeof(buf), A11y(kA11y_FmtFile), cursor + 1, name[0] ? name : A11y(kA11y_Unnamed));
      } else {
        snprintf(buf, sizeof(buf), A11y(kA11y_FmtFileEmpty), cursor + 1);
      }
    } else {
      snprintf(buf, sizeof(buf), "%s", A11y(kA11y_Cancel));
    }
    SpeechSynthesis_Speak(buf);
  }
}

// --- Name registration letter tracking ---

// Grid value to ASCII character (best-effort mapping)
static const char kGridToChar[128] = {
  'A','B','C','D','E','F','G','H', 0, 'J',  // 0x00-0x09
  'K','L','M','N','O','P',                    // 0x0A-0x0F
  'Q','R','S','T','U','V','W','X','Y','Z',    // 0x10-0x19
  'a','b','c','d','e','f',                     // 0x1A-0x1F
  'g','h', 0, 'j','k', 0, 'm','n','o','p',    // 0x20-0x29
  'q','r','s','t','u','v',                     // 0x2A-0x2F
  'w','x','y','z', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x30-0x3F
  '0','1','2', 0, 0, '5','6', 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x40-0x4F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'I',      // 0x50-0x5F
  'i','l', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     // 0x60-0x6F
  0, 0, 0, 0, 0, 0, '3','4','7','8','9', 0, 0, 0, 0, 0,   // 0x70-0x7F
};

static const int8 kNameGrid[128] = {
     6,    7, 0x5f,    9, 0x59, 0x59, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x60, 0x23,
  0x59, 0x59, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x59, 0x59, 0x59,    0,    1,    2,    3,    4,    5,
  0x10, 0x11, 0x12, 0x13, 0x59, 0x59, 0x24, 0x5f, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d,
  0x59, 0x59, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x59, 0x59, 0x59,  0xa,  0xb,  0xc,  0xd,  0xe,  0xf,
  0x40, 0x41, 0x42, 0x59, 0x59, 0x59, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x40, 0x41, 0x42, 0x59,
  0x59, 0x59, 0x61, 0x3f, 0x45, 0x46, 0x59, 0x59, 0x59, 0x59, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
  0x44, 0x59, 0x6f, 0x6f, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x5a, 0x44, 0x59, 0x6f, 0x6f,
  0x59, 0x59, 0x5a, 0x44, 0x59, 0x6f, 0x6f, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x5a,
};

static void AnnounceNameRegistration(void) {
  uint8 mod = main_module_index;
  if (mod != 4) {
    g_prev_name_col = 0xFF;
    g_prev_name_row = 0xFF;
    return;
  }

  uint8 col = selectfile_var3;
  uint8 row = selectfile_var5;
  if (col == g_prev_name_col && row == g_prev_name_row) return;
  g_prev_name_col = col;
  g_prev_name_row = row;

  if (col >= 32 || row >= 4) return;
  int8 grid_val = kNameGrid[col + row * 32];

  if (grid_val == 0x5a)
    SpeechSynthesis_Speak(A11y(kA11y_Back));
  else if (grid_val == 0x44)
    SpeechSynthesis_Speak(A11y(kA11y_Next));
  else if (grid_val == 0x59 || grid_val == 0x6f)
    return;  // blank, don't announce
  else {
    uint8 idx = (uint8)grid_val;
    char ch = (idx < 128) ? kGridToChar[idx] : 0;
    if (ch) {
      char buf[16];
      if (ch >= 'A' && ch <= 'Z')
        Speak(A11y(kA11y_FmtCapital), ch);
      else
        Speak("%c", ch);
    }
  }
}

// --- Floor change tracking ---

static void AnnounceFloorChange(void) {
  uint8 mod = main_module_index;
  // Only track during dungeon gameplay
  if (mod != 9 && mod != 14) {
    g_floor_tracking_init = false;
    return;
  }
  if (!player_is_indoors) {
    g_floor_tracking_init = false;
    return;
  }

  uint8 floor = dung_cur_floor;
  if (!g_floor_tracking_init) {
    g_prev_floor = floor;
    g_floor_tracking_init = true;
    return;
  }

  if (floor != g_prev_floor) {
    g_prev_floor = floor;
    if (floor == 0) {
      SpeechSynthesis_Speak(A11y(kA11y_GroundFloor));
    } else if (!sign8(floor)) {
      Speak(A11y(kA11y_FmtFloor), floor);
    } else {
      Speak(A11y(kA11y_FmtBasement), (uint8)(~floor) + 1);
    }
  }
}

// --- Inventory tracking ---

// Map HUD item slot (1-20) to Y-item index for name lookup
// hud_cur_item stores 1-based item type codes (1=Bow .. 20=Mirror).
// Y-item index (for link_item_bow[]) is simply slot - 1.
static const char *GetItemName(uint8 slot) {
  if (slot == 0 || slot > 20) return NULL;
  // Type 13 = Shovel/Flute slot; if player has flute, announce "Flute"
  if (slot == 13 && link_item_flute >= 2)
    return A11y(kA11y_Flute);
  return A11yItemName(slot - 1);
}

static void AnnounceInventory(void) {
  uint8 mod = main_module_index;
  bool menu_open = (mod == 14 && submodule_index == 1);
  bool gameplay = (mod == 7 || mod == 9);

  if (menu_open && !g_prev_menu_open) {
    g_prev_hud_item = 0xFF;
    g_prev_hud_item_x = 0xFF;
  }
  g_prev_menu_open = menu_open;

  if (!menu_open && !gameplay) return;

  uint8 item = hud_cur_item;
  if (item != g_prev_hud_item && item > 0 && item <= 20) {
    g_prev_hud_item = item;
    const char *name = GetItemName(item);
    if (name)
      SpeechSynthesis_Speak(name);
  }

  uint8 item_x = hud_cur_item_x;
  if (item_x != g_prev_hud_item_x && item_x > 0 && item_x <= 20) {
    g_prev_hud_item_x = item_x;
    const char *name = GetItemName(item_x);
    if (name)
      SpeechSynthesis_Speak(name);
  }
}

// --- Dungeon name announcement ---

static void AnnounceDungeon(void) {
  bool indoors = player_is_indoors;
  uint8 palace = BYTE(cur_palace_index_x2);

  if (indoors && !g_prev_indoors) {
    const char *name = A11yDungeonName(palace / 2);
    if (name) SpeechSynthesis_Speak(name);
  }
  else if (indoors && palace != (uint8)g_prev_palace_index && g_prev_palace_index != 0xFFFF) {
    const char *name = A11yDungeonName(palace / 2);
    if (name) SpeechSynthesis_Speak(name);
  }

  g_prev_indoors = indoors;
  g_prev_palace_index = palace;
}

// --- Item collection announcements ---

static void AnnounceCollections(void) {
  uint8 mod = main_module_index;
  // Only during gameplay
  if (mod != 7 && mod != 9 && mod != 14) return;

  uint16 rupees = link_rupees_goal;
  uint8 health = link_health_current;
  uint8 arrows = link_num_arrows;
  uint8 bombs = link_item_bombs;
  uint8 keys = link_num_keys;

  // 0xFF is a sentinel meaning "no item" — treat as 0 for comparison
  if (keys == 0xFF) keys = 0;
  if (arrows == 0xFF) arrows = 0;
  if (bombs == 0xFF) bombs = 0;

  if (!g_tracking_initialized) {
    g_prev_rupees = rupees;
    g_prev_health = health;
    g_prev_arrows = arrows;
    g_prev_bombs = bombs;
    g_prev_keys = keys;
    g_tracking_initialized = true;
    return;
  }

  // Rupees gained
  if (rupees > g_prev_rupees) {
    int gained = rupees - g_prev_rupees;
    Speak(A11y(kA11y_FmtPlusRupees), gained, rupees);
  }

  // Health gained
  if (health > g_prev_health) {
    int hearts = health / 8;
    int half = (health % 8) >= 4;
    int max = link_health_capacity / 8;
    if (half)
      Speak(A11y(kA11y_FmtHeartsHalf), hearts, max);
    else
      Speak(A11y(kA11y_FmtHearts), hearts, max);
  }

  // Arrows gained
  if (arrows > g_prev_arrows)
    Speak(A11y(kA11y_FmtArrows), arrows);

  // Bombs gained
  if (bombs > g_prev_bombs)
    Speak(A11y(kA11y_FmtBombs), bombs);

  // Keys gained
  if (keys > g_prev_keys)
    Speak(A11y(kA11y_FmtGotKey), keys);

  g_prev_rupees = rupees;
  g_prev_health = health;
  g_prev_arrows = arrows;
  g_prev_bombs = bombs;
  g_prev_keys = keys;
}

// --- Overworld area name announcement ---

static void AnnounceAreaChange(void) {
  uint8 mod = main_module_index;
  if (mod != 7 && mod != 9 && mod != 14) return;
  if (player_is_indoors) return;

  uint16 area = overworld_area_index;
  if (area != g_prev_ow_area) {
    if (g_prev_ow_area != 0xFFFF) {
      const char *name = A11yAreaName(area);
      if (name) SpeechSynthesis_Speak(name);
    }
    g_prev_ow_area = area;
  }
}

// --- Movement direction announcement ---

static void AnnounceDirectionChange(void) {
  uint8 mod = main_module_index;
  if (mod != 7 && mod != 9) return;

  uint8 dir = link_direction_facing;
  if (dir != g_prev_facing) {
    g_prev_facing = dir;
    const char *name = GetDirectionName(dir, false);
    if (name) SpeechSynthesis_Speak(name);
  }
}

// --- Nearby entrance announcement ---

static void AnnounceNearbyEntrance(void) {
  // Suppress entrance TTS right after exiting a building
  if (g_outdoor_suppress_frames > 0) {
    g_outdoor_suppress_frames--;
    g_prev_nearest_entrance_idx = g_nearest_entrance_idx;
    return;
  }
  // Only announce entrances within ~32px (4 tiles) — 32^2 = 1024
  int announce_idx = -1;
  if (g_nearest_entrance_idx >= 0 && g_nearest_entrance_dist2 <= 1024)
    announce_idx = g_nearest_entrance_idx;
  if (announce_idx == g_prev_nearest_entrance_idx) return;
  g_prev_nearest_entrance_idx = announce_idx;
  if (announce_idx >= 0) {
    uint8 eid = kOverworld_Entrance_Id[announce_idx];
    const char *name = A11yEntranceName(eid);
    if (name) SpeechSynthesis_Speak(name);
  }
}

// --- Overworld map markers ---

// Pendant marker slot → dungeon index (palace_index / 2)
static const int kPendantSlotToDungeon[3] = {3, 4, 13};  // Eastern, Desert, Hera
static const uint8 kPendantBitMask_a11y[3] = {4, 1, 2};

// Crystal marker slot → dungeon index (palace_index / 2)
static const int kCrystalSlotToDungeon[7] = {8, 7, 10, 14, 12, 9, 11};
static const uint8 kCrystalBitMask_a11y[7] = {2, 0x40, 8, 0x20, 1, 4, 0x10};

static void AnnounceOverworldMap(void) {
  uint8 mod = main_module_index;
  bool map_active = (mod == 14 && submodule_index == 7 && overworld_map_state >= 5);

  if (map_active && !g_prev_ow_map_active) {
    // Map just opened — announce summary
    char buf[256];
    int pos = 0;
    bool dark = is_in_dark_world != 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s ",
                    dark ? A11y(kA11y_DarkWorldMap) : A11y(kA11y_LightWorldMap));

    uint8 k = savegame_map_icons_indicator;

    if (k == 3 && !dark) {
      // Pendant mode — list uncollected pendant dungeons
      for (int i = 0; i < 3; i++) {
        if (!(link_which_pendants & kPendantBitMask_a11y[i])) {
          const char *name = A11yDungeonName(kPendantSlotToDungeon[i]);
          if (name && pos < (int)sizeof(buf) - 2)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s. ", name);
        }
      }
    } else if (k == 7 && dark) {
      // Crystal mode — list uncollected crystal dungeons
      for (int i = 0; i < 7; i++) {
        if (!(link_has_crystals & kCrystalBitMask_a11y[i])) {
          const char *name = A11yDungeonName(kCrystalSlotToDungeon[i]);
          if (name && pos < (int)sizeof(buf) - 2)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s. ", name);
        }
      }
    }

    SpeechSynthesis_Speak(buf);
  }
  g_prev_ow_map_active = map_active;
}

// --- Flute destination ---

static void AnnounceFluteDestination(void) {
  uint8 mod = main_module_index;
  bool flute_active = (mod == 14 && submodule_index == 10);

  if (flute_active) {
    uint8 sel = birdtravel_var1[0] & 7;
    if (!g_prev_flute_active || sel != g_prev_flute_sel) {
      g_prev_flute_sel = sel;
      const char *name = A11yFluteName(sel);
      if (name) SpeechSynthesis_Speak(name);
    }
  }
  g_prev_flute_active = flute_active;
}

// --- Dungeon map floor ---

static void AnnounceDungeonMap(void) {
  uint8 mod = main_module_index;
  bool dmap_active = (mod == 14 && submodule_index == 3);

  if (dmap_active && !g_prev_dung_map_active) {
    // Dungeon map just opened — announce dungeon name + current floor
    char buf[128];
    const char *dname = A11yDungeonName(BYTE(cur_palace_index_x2) / 2);
    int8 floor = (int8)dungmap_cur_floor;
    if (dname) {
      if (floor == 0)
        snprintf(buf, sizeof(buf), "%s %s. %s", A11y(kA11y_DungeonMap), dname, A11y(kA11y_GroundFloor));
      else if (floor > 0)
        snprintf(buf, sizeof(buf), "%s %s. " , A11y(kA11y_DungeonMap), dname);
      else
        snprintf(buf, sizeof(buf), "%s %s. ", A11y(kA11y_DungeonMap), dname);
      SpeechSynthesis_Speak(buf);
    }
  } else if (dmap_active) {
    // Track floor changes while dungeon map is open
    uint16 floor = dungmap_cur_floor;
    if (floor != g_prev_dungmap_floor) {
      char buf[64];
      int8 f = (int8)(uint8)floor;
      if (f == 0)
        SpeechSynthesis_Speak(A11y(kA11y_GroundFloor));
      else if (f > 0)
        snprintf(buf, sizeof(buf), A11y(kA11y_FmtFloor), f), SpeechSynthesis_Speak(buf);
      else
        snprintf(buf, sizeof(buf), A11y(kA11y_FmtBasement), (uint8)(~f) + 1), SpeechSynthesis_Speak(buf);
    }
  }
  if (dmap_active)
    g_prev_dungmap_floor = dungmap_cur_floor;
  g_prev_dung_map_active = dmap_active;
}
#endif  // __APPLE__

void SpatialAudio_ScanFrame(void) {
  SpatialCue cues[kSpatialCue_Count];
  uint32 best_dist2[kSpatialCue_Count];

  if (!g_enabled) return;

#if defined(__APPLE__) || defined(_WIN32)
  AnnounceMenuState();
  AnnounceInventory();
  AnnounceDungeon();
  AnnounceFloorChange();
  AnnounceCollections();
  AnnounceNameRegistration();
  AnnounceAreaChange();
  AnnounceOverworldMap();
  AnnounceFluteDestination();
  AnnounceDungeonMap();
  // Direction available via 'i' key but not auto-announced (too noisy)
#endif

  // Suppress normal scanning during legend mode
  if (g_legend_active) return;

  memset(cues, 0, sizeof(cues));
  for (int i = 0; i < kSpatialCue_Count; i++)
    best_dist2[i] = 0xFFFFFFFF;

  uint8 mod = main_module_index;
  if (mod != 7 && mod != 9 && mod != 14) {
    memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));
    // Kill all in-progress one-shot sounds (death, menu transitions, etc.)
    g_sword_range_active = false;
    g_sword_range_envelope = 0;
    g_danger_active = false;
    g_danger_prev = false;
    g_danger_drill_pos = -1;
    g_danger_exit_pos = -1;
    g_align_interval = 0;
    g_align_envelope = 0;
    g_hole_warn_active = false;
    g_hole_warn_envelope = 0;
    g_blocked_active = false;
    g_blocked_envelope = 0;
    g_room_chime_envelope = 0;
    g_ledge_envelope = 0; g_ledge_timer = 0;
    g_water_envelope = 0; g_water_timer = 0;
    g_hazard_envelope = 0; g_hazard_timer = 0;
    g_conveyor_timer = 0;
    g_terrain_type = kTerrain_Normal;
    g_prev_terrain_type = kTerrain_Normal;
    g_terrain_pos = -1;
    return;
  }

  uint16 lx = link_x_coord;
  uint16 ly = link_y_coord;
  bool indoors = player_is_indoors;

  // Track indoor→outdoor transition: suppress entrance TTS for 60 frames (~1s)
  if (g_was_indoors && !indoors)
    g_outdoor_suppress_frames = 60;
  g_was_indoors = indoors;

  // Pre-filter overworld entrances for the current area
  uint16 ow_entrance_pos[32];
  int ow_entrance_orig_idx[32];
  int num_ow_entrances = 0;
  g_nearest_entrance_idx = -1;
  g_nearest_entrance_dist2 = 0;
  if (!indoors) {
    int total = kOverworld_Entrance_Pos_SIZE / 2;
    for (int i = 0; i < total && num_ow_entrances < 32; i++) {
      if (kOverworld_Entrance_Area[i] == overworld_area_index) {
        ow_entrance_pos[num_ow_entrances] = kOverworld_Entrance_Pos[i];
        ow_entrance_orig_idx[num_ow_entrances] = i;
        num_ow_entrances++;
      }
    }
  }

  // Narrow-beam wall distances for passage detection
  // Only considers wall tiles within ±16px perpendicular band
  int beam_dist[4];  // N, S, E, W
  for (int i = 0; i < 4; i++) beam_dist[i] = SCAN_RANGE;

  // Tile scan: 96px radius, 8px step for full diagonal coverage
  int scan_steps = SCAN_RANGE / 8;
  for (int tdy = -scan_steps; tdy <= scan_steps; tdy++) {
    for (int tdx = -scan_steps; tdx <= scan_steps; tdx++) {
      int px = (int)lx + 8 + tdx * 8;
      int py = (int)ly + 12 + tdy * 8;
      if (px < 0 || py < 0) continue;

      // Check if this tile is an overworld entrance (use raw coords, not sprite center)
      if (!indoors && num_ow_entrances > 0) {
        uint16 raw_x = (uint16)((int)lx + tdx * 8);
        uint16 raw_y = (uint16)((int)ly + tdy * 8);
        uint16 xc = raw_x >> 3;
        uint16 yc = raw_y + 7;
        uint16 pos = ((yc - overworld_offset_base_y) & overworld_offset_mask_y) * 8 +
                     ((xc - overworld_offset_base_x) & overworld_offset_mask_x);
        for (int e = 0; e < num_ow_entrances; e++) {
          if (pos == ow_entrance_pos[e]) {
            int dx = tdx * 8;
            int dy = tdy * 8;
            uint32 d2 = (uint32)(dx * dx + dy * dy);
            if (d2 < best_dist2[kSpatialCue_Door]) {
              best_dist2[kSpatialCue_Door] = d2;
              cues[kSpatialCue_Door].dx = (int16)dx;
              cues[kSpatialCue_Door].dy = (int16)dy;
              cues[kSpatialCue_Door].active = true;
              g_nearest_entrance_idx = ow_entrance_orig_idx[e];
              g_nearest_entrance_dist2 = (int)d2;
            }
            break;
          }
        }
      }

      uint8 tile;
      if (indoors) {
        int tx = (px >> 3) & 63;
        int ty = (py >> 3) & 63;
        int base_offs = ty * 64 + tx;
        // Read current layer first
        int offs = base_offs + (link_is_on_lower_level ? 0x1000 : 0);
        tile = dung_bg2_attr_table[offs];
        // If current layer is just floor, check other layer for stairs/doors
        if (ClassifyTile(tile) < 0) {
          int offs2 = base_offs + (link_is_on_lower_level ? 0 : 0x1000);
          uint8 tile2 = dung_bg2_attr_table[offs2];
          if (ClassifyTile(tile2) >= 0)
            tile = tile2;
        }
      } else {
        tile = Overworld_GetTileAttributeAtLocation((uint16)(px >> 3), (uint16)py);
      }

      int cat = ClassifyTile(tile);
      // Tile 0x0B is deep water only outdoors
      if (cat < 0 && !indoors && tile == 0x0B) cat = kSpatialCue_DeepWater;
      if (cat < 0 && cat != TILE_CLASS_WALL) continue;

      int dx = tdx * 8;
      int dy = tdy * 8;

      // Bin wall tiles into directional quadrants
      if (cat == TILE_CLASS_WALL) {
        int abs_dx = dx < 0 ? -dx : dx;
        int abs_dy = dy < 0 ? -dy : dy;
        if (abs_dx == 0 && abs_dy == 0) continue;  // tile right on Link, skip
        if (abs_dx >= abs_dy)
          cat = (dx >= 0) ? kSpatialCue_WallE : kSpatialCue_WallW;
        else
          cat = (dy >= 0) ? kSpatialCue_WallS : kSpatialCue_WallN;

        // Narrow-beam wall tracking for passage detection
        // Only count tiles within ±16px perpendicular to the cardinal direction
        if (dy < 0 && abs_dx <= 16 && -dy < beam_dist[0]) beam_dist[0] = -dy;  // North
        if (dy > 0 && abs_dx <= 16 &&  dy < beam_dist[1]) beam_dist[1] = dy;   // South
        if (dx > 0 && abs_dy <= 16 &&  dx < beam_dist[2]) beam_dist[2] = dx;   // East
        if (dx < 0 && abs_dy <= 16 && -dx < beam_dist[3]) beam_dist[3] = -dx;  // West
      }

      // Non-wall impassable tiles also feed the passage detection beam
      if (IsBeamOpaque(cat, tile, indoors)) {
        int abs_dx = dx < 0 ? -dx : dx;
        int abs_dy = dy < 0 ? -dy : dy;
        if (abs_dx > 0 || abs_dy > 0) {
          if (dy < 0 && abs_dx <= 16 && -dy < beam_dist[0]) beam_dist[0] = -dy;
          if (dy > 0 && abs_dx <= 16 &&  dy < beam_dist[1]) beam_dist[1] = dy;
          if (dx > 0 && abs_dy <= 16 &&  dx < beam_dist[2]) beam_dist[2] = dx;
          if (dx < 0 && abs_dy <= 16 && -dx < beam_dist[3]) beam_dist[3] = -dx;
        }
      }

      // Split holes into left/right by dx relative to Link
      if (cat == kSpatialCue_HoleL)
        cat = (dx < 0) ? kSpatialCue_HoleL : kSpatialCue_HoleR;

      uint32 d2 = (uint32)(dx * dx + dy * dy);
      if (d2 < best_dist2[cat]) {
        best_dist2[cat] = d2;
        cues[cat].dx = (int16)dx;
        cues[cat].dy = (int16)dy;
        cues[cat].active = true;
      }
    }
  }

  // Sprite scan: 16 slots, extended range
  int nearest_enemy_dist = SCAN_RANGE;
  int nearest_enemy_dx = 0, nearest_enemy_dy = 0;
  bool any_in_danger = false;
  for (int k = 0; k < 16; k++) {
    if (sprite_state[k] == 0) continue;

    int sx = (sprite_x_hi[k] << 8) | sprite_x_lo[k];
    int sy = (sprite_y_hi[k] << 8) | sprite_y_lo[k];
    int dx = sx - (int)lx;
    int dy = sy - (int)ly;

    if (dx < -SCAN_RANGE || dx > SCAN_RANGE || dy < -SCAN_RANGE || dy > SCAN_RANGE) continue;

    int cat = sprite_bump_damage[k] > 0 ? kSpatialCue_Enemy : kSpatialCue_NPC;
    uint32 d2 = (uint32)(dx * dx + dy * dy);
    if (d2 < best_dist2[cat]) {
      best_dist2[cat] = d2;
      cues[cat].dx = (int16)dx;
      cues[cat].dy = (int16)dy;
      cues[cat].active = true;
    }

    // Track nearest enemy + dynamic danger zone
    if (cat == kSpatialCue_Enemy) {
      int ed = (int)isqrt32(d2);
      if (ed < nearest_enemy_dist) {
        nearest_enemy_dist = ed;
        nearest_enemy_dx = dx;
        nearest_enemy_dy = dy;
      }

      // Dynamic danger zone: hitbox size + approach speed * reaction frames
      int hi = sprite_flags4[k] & 0x1F;
      int hitbox_r = kHitboxXSize[hi];
      if (kHitboxYSize[hi] > hitbox_r) hitbox_r = kHitboxYSize[hi];

      // Approach speed: dot product of velocity toward Link
      int8 vx = (int8)sprite_x_vel[k];
      int8 vy = (int8)sprite_y_vel[k];
      int approach = 0;
      if (ed > 0)
        approach = (-(int)vx * dx - (int)vy * dy) / ed;  // positive = approaching
      if (approach < 0) approach = 0;

      // danger_dist = hitbox_radius + approach_speed * 6 frames reaction time
      int danger_dist = hitbox_r + approach * 6;
      if (danger_dist < 16) danger_dist = 16;  // minimum 16px

      if (ed <= danger_dist)
        any_in_danger = true;
    }
  }

  // Combat indicators based on nearest enemy
  bool has_enemy = cues[kSpatialCue_Enemy].active;
  // Sword range: within 28px — you can hit them
  g_sword_range_active = has_enemy && (nearest_enemy_dist <= 28);
  // Danger: dynamic per-sprite based on hitbox + approach speed
  g_danger_active = any_in_danger;
  // Detect transitions: entering → drill, leaving → exit chime
  if (g_danger_active && !g_danger_prev) {
    g_danger_drill_pos = 0;   // start drill
    g_danger_exit_pos = -1;   // cancel any exit chime
  } else if (!g_danger_active && g_danger_prev) {
    g_danger_exit_pos = 0;    // start exit chime
    g_danger_drill_pos = -1;  // cancel drill
  }
  g_danger_prev = g_danger_active;

  // Alignment sonar: check if nearly lined up on X or Y axis with nearest enemy
  // Pick whichever axis is closer to aligned
  if (has_enemy && nearest_enemy_dist > 28) {
    int abs_dx = nearest_enemy_dx < 0 ? -nearest_enemy_dx : nearest_enemy_dx;
    int abs_dy = nearest_enemy_dy < 0 ? -nearest_enemy_dy : nearest_enemy_dy;
    int off = abs_dx < abs_dy ? abs_dx : abs_dy;  // best axis alignment
    if (off < 16) {
      g_align_offset = off;
      // Variable ping rate: closer to aligned = faster pings
      if (off <= 4)
        g_align_interval = g_sample_rate / 10;   // every 100ms — locked on
      else if (off <= 8)
        g_align_interval = g_sample_rate / 5;    // every 200ms
      else
        g_align_interval = g_sample_rate * 2 / 5; // every 400ms — getting close
    } else {
      g_align_offset = SCAN_RANGE;
      g_align_interval = 0;
    }
  } else {
    g_align_offset = SCAN_RANGE;
    g_align_interval = 0;
  }

  memcpy(g_cue_snapshot, cues, sizeof(g_cue_snapshot));

  // Terrain underfoot tracking: read tile at Link's sprite center
  {
    int foot_x = (int)lx + 8;
    int foot_y = (int)ly + 16;
    uint8 foot_tile;
    if (indoors) {
      int tx = (foot_x >> 3) & 63;
      int ty = (foot_y >> 3) & 63;
      int offs = ty * 64 + tx + (link_is_on_lower_level ? 0x1000 : 0);
      foot_tile = dung_bg2_attr_table[offs];
    } else {
      foot_tile = Overworld_GetTileAttributeAtLocation((uint16)(foot_x >> 3), (uint16)foot_y);
    }
    int terrain = kTerrain_Normal;
    if (foot_tile == 0x09)
      terrain = kTerrain_ShallowWater;
    else if (!indoors && foot_tile == 0x04)
      terrain = kTerrain_Grass;
    else if (foot_tile == 0x40)
      terrain = kTerrain_Grass;
    else if (foot_tile == 0x0E || foot_tile == 0x0F)
      terrain = kTerrain_Ice;
    else if (foot_tile == 0x0D)
      terrain = kTerrain_Spike;

    g_terrain_type = terrain;
    if (terrain != g_prev_terrain_type) {
      g_prev_terrain_type = terrain;
      // Trigger one-shot terrain tone (not for Normal or Spike return)
      if (terrain == kTerrain_Grass) {
        g_terrain_phase_inc = (uint32)((uint64)200 * 256 * 65536 / g_sample_rate);
        int ds = g_sample_rate * 100 / 1000;
        g_terrain_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
        g_terrain_len = ds;
        g_terrain_pos = 0; g_terrain_phase = 0;
        g_terrain_envelope = 65536; g_terrain_am = false;
      } else if (terrain == kTerrain_ShallowWater) {
        g_terrain_phase_inc = (uint32)((uint64)280 * 256 * 65536 / g_sample_rate);
        int ds = g_sample_rate * 120 / 1000;
        g_terrain_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
        g_terrain_len = ds;
        g_terrain_pos = 0; g_terrain_phase = 0;
        g_terrain_envelope = 65536; g_terrain_am = true;
      } else if (terrain == kTerrain_Ice) {
        g_terrain_phase_inc = (uint32)((uint64)1000 * 256 * 65536 / g_sample_rate);
        int ds = g_sample_rate * 60 / 1000;
        g_terrain_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
        g_terrain_len = ds;
        g_terrain_pos = 0; g_terrain_phase = 0;
        g_terrain_envelope = 65536; g_terrain_am = false;
      }
      // kTerrain_Normal and kTerrain_Spike: no tone
    }
  }

#if defined(__APPLE__) || defined(_WIN32)
  AnnounceNearbyEntrance();
#endif

  // Only play wall sonar in the direction Link is facing
  int facing_wall = -1;
  switch (link_direction_facing) {
  case 0: facing_wall = 0; break;  // North
  case 2: facing_wall = 1; break;  // South
  case 4: facing_wall = 3; break;  // West
  case 6: facing_wall = 2; break;  // East
  }

  // Compute wall click intervals (only for facing direction)
  for (int w = 0; w < 4; w++) {
    int c = kSpatialCue_WallN + w;
    if (w == facing_wall && g_cue_snapshot[c].active) {
      int wdx = g_cue_snapshot[c].dx;
      int wdy = g_cue_snapshot[c].dy;
      int dist = (int)isqrt32((uint32)(wdx * wdx + wdy * wdy));
      int ms = 60 + (dist * 190 / SCAN_RANGE);
      g_wall_interval[w] = g_sample_rate * ms / 1000;
    } else {
      g_wall_interval[w] = 0;
    }
  }

  // Passage chimes: trigger when a nearby wall in the narrow beam disappears
  // Narrow beam (±16px perpendicular) ensures only walls directly beside Link count
  {
    uint8 facing = link_direction_facing;
    for (int w = 0; w < 4; w++) {
      int cur_dist = beam_dist[w];
      int prev_dist = g_prev_beam_dist[w];
      // Wall was present and nearby, now gone from the narrow beam
      bool passage_opened = (prev_dist < SCAN_RANGE) && (cur_dist == SCAN_RANGE);
      g_prev_beam_dist[w] = cur_dist;

      if (passage_opened) {
        bool trigger = false;
        uint32 inc1 = 0, inc2 = 0;
        int panL = 128, panR = 128;

        if (facing == 0 || facing == 2) {
          // Facing N/S: lateral walls are E(2) and W(3)
          if (w == 2) { trigger = true; inc1 = g_d5_inc; inc2 = g_d6_inc; panL = 32; panR = 224; }
          if (w == 3) { trigger = true; inc1 = g_d5_inc; inc2 = g_d6_inc; panL = 224; panR = 32; }
        } else if (facing == 4 || facing == 6) {
          // Facing E/W: lateral walls are N(0) and S(1)
          if (w == 0) { trigger = true; inc1 = g_d5_inc; inc2 = g_d6_inc; panL = 128; panR = 128; }
          if (w == 1) { trigger = true; inc1 = g_d7_inc; inc2 = g_d6_inc; panL = 128; panR = 128; }
        }

        if (trigger) {
          g_passage_pos[w] = 0;
          g_passage_phase[w] = 0;
          g_passage_inc1[w] = inc1;
          g_passage_inc2[w] = inc2;
          g_passage_pan_L[w] = panL;
          g_passage_pan_R[w] = panR;
        }
      }
    }
  }

  // Room/screen change detection
  uint16 cur_ow_screen = overworld_screen_index;
  uint16 cur_dung_room = dungeon_room_index;
  if (!indoors) {
    if (cur_ow_screen != g_prev_ow_screen && g_prev_ow_screen != 0xFFFF)
      g_room_chime_envelope = 65536;
    g_prev_ow_screen = cur_ow_screen;
  } else {
    if (cur_dung_room != g_prev_dung_room && g_prev_dung_room != 0xFFFF)
      g_room_chime_envelope = 65536;
    g_prev_dung_room = cur_dung_room;
  }

  // Hole proximity warning: active when moving and a hole is within 16px
  {
    bool link_moving = (lx != g_prev_link_x || ly != g_prev_link_y);
    int hole_dist = SCAN_RANGE;
    int nearest_dx = 0, nearest_dy = 0;
    for (int h = kSpatialCue_HoleL; h <= kSpatialCue_HoleR; h++) {
      if (cues[h].active) {
        int hd2 = cues[h].dx * cues[h].dx + cues[h].dy * cues[h].dy;
        int hd = (int)isqrt32((uint32)hd2);
        if (hd < hole_dist) {
          hole_dist = hd;
          nearest_dx = cues[h].dx;
          nearest_dy = cues[h].dy;
        }
      }
    }
    g_hole_warn_active = link_moving && (hole_dist <= 16);
    g_hole_warn_dx = (int16)nearest_dx;
    g_hole_warn_dy = (int16)nearest_dy;
  }

  // Blocked detection
  bool pressing_dir = (joypad1H_last & kJoypadH_AnyDir) != 0;
  bool pos_stuck = (lx == g_prev_link_x && ly == g_prev_link_y);
  if (pressing_dir && pos_stuck && (mod == 7 || mod == 9)) {
    g_blocked_frame_count++;
    g_blocked_active = (g_blocked_frame_count >= 3);
  } else {
    g_blocked_frame_count = 0;
    g_blocked_active = false;
  }
  g_prev_link_x = lx;
  g_prev_link_y = ly;
}

void SpatialAudio_MixAudio(int16 *buf, int samples, int channels) {
  if (!g_enabled || channels != 2) return;

  for (int s = 0; s < samples; s++) {
    int32 mix_L = 0, mix_R = 0;

    // NPC double-chime: position-based pattern (beep1 60ms, gap 40ms, beep2 60ms, silence)
    // Total pattern: 160ms, repeat every 1.2s
    if (g_ding_timer <= 0) { g_ding_timer = g_sample_rate * 12 / 10; g_npc_beep_pos = 0; }
    g_ding_timer--;
    g_npc_beep_pos++;

    if (g_enemy_timer <= 0) { g_enemy_envelope = 65536; g_enemy_timer = g_sample_rate / 5; }
    g_enemy_timer--;
    g_enemy_envelope = (uint32)((uint64)g_enemy_envelope * g_enemy_decay >> 16);

    if (g_door_timer <= 0) { g_door_envelope = 65536; g_door_timer = g_sample_rate * 2 / 5; }
    g_door_timer--;
    g_door_envelope = (uint32)((uint64)g_door_envelope * g_door_decay >> 16);

    if (g_stair_timer <= 0) { g_stair_envelope = 65536; g_stair_timer = g_sample_rate * 3 / 10; }
    g_stair_timer--;
    g_stair_envelope = (uint32)((uint64)g_stair_envelope * g_stair_decay >> 16);
    // Sweep counter: wraps every 0.75 seconds
    g_stair_sweep++;
    if (g_stair_sweep >= (uint32)(g_sample_rate * 3 / 4))
      g_stair_sweep = 0;

    if (g_hole_timer <= 0) { g_hole_envelope = 65536; g_hole_timer = g_sample_rate / 2; g_hole_sweep = 0; }
    g_hole_timer--;
    g_hole_envelope = (uint32)((uint64)g_hole_envelope * g_hole_decay >> 16);
    g_hole_sweep++;

    // Liftable "swipe": 120ms decay, 700ms repeat
    if (g_lift_timer <= 0) { g_lift_envelope = 65536; g_lift_timer = g_sample_rate * 700 / 1000; g_lift_sweep = 0; }
    g_lift_timer--;
    g_lift_envelope = (uint32)((uint64)g_lift_envelope * g_lift_decay >> 16);
    g_lift_sweep++;

    // Chest "sparkle": 200ms decay, 800ms repeat
    if (g_chest_timer <= 0) { g_chest_envelope = 65536; g_chest_timer = g_sample_rate * 800 / 1000; }
    g_chest_timer--;
    g_chest_envelope = (uint32)((uint64)g_chest_envelope * g_chest_decay >> 16);

    // Wall click envelopes (4 directions, variable repeat rate)
    for (int w = 0; w < 4; w++) {
      if (g_wall_interval[w] > 0) {
        if (g_wall_timer[w] <= 0) {
          g_wall_envelope[w] = 65536;
          g_wall_timer[w] = g_wall_interval[w];
        }
      }
      if (g_wall_timer[w] > 0) g_wall_timer[w]--;
      g_wall_envelope[w] = (uint32)((uint64)g_wall_envelope[w] * g_wall_decay >> 16);
    }

    // Ledge envelope: 80ms decay, 350ms repeat
    if (g_ledge_timer <= 0) { g_ledge_envelope = 65536; g_ledge_timer = g_sample_rate * 350 / 1000; }
    g_ledge_timer--;
    g_ledge_envelope = (uint32)((uint64)g_ledge_envelope * g_ledge_decay >> 16);
    g_ledge_sweep++;
    if (g_ledge_sweep >= (uint32)(g_sample_rate * 350 / 1000))
      g_ledge_sweep = 0;

    // Deep water envelope: 250ms decay, 600ms repeat
    if (g_water_timer <= 0) { g_water_envelope = 65536; g_water_timer = g_sample_rate * 600 / 1000; }
    g_water_timer--;
    g_water_envelope = (uint32)((uint64)g_water_envelope * g_water_decay >> 16);

    // Hazard envelope: 40ms decay, 150ms repeat
    if (g_hazard_timer <= 0) { g_hazard_envelope = 65536; g_hazard_timer = g_sample_rate * 150 / 1000; }
    g_hazard_timer--;
    g_hazard_envelope = (uint32)((uint64)g_hazard_envelope * g_hazard_decay >> 16);

    // Conveyor timer
    if (g_conveyor_timer <= 0) g_conveyor_timer = g_conveyor_cycle_len;
    g_conveyor_timer--;

    for (int c = 0; c < kSpatialCue_Count; c++) {
      if (!g_cue_snapshot[c].active) continue;

      int dx = g_cue_snapshot[c].dx;
      int dy = g_cue_snapshot[c].dy;

      uint32 d2 = (uint32)(dx * dx + dy * dy);
      int dist = (int)isqrt32(d2);
      if (dist >= SCAN_RANGE) continue;
      int volume = (SCAN_RANGE - dist) * 256 / SCAN_RANGE;

      // Per-category envelope
      if (c >= kSpatialCue_WallN && c <= kSpatialCue_WallW) {
        int w = c - kSpatialCue_WallN;
        volume = (int)((uint32)volume * g_wall_envelope[w] >> 16);
      } else if (c == kSpatialCue_NPC) {
        // Double-chime: beep1 60ms, gap 40ms, beep2 60ms, silence until repeat
        int beep1_len = g_sample_rate * 60 / 1000;
        int gap_end = beep1_len + g_sample_rate * 40 / 1000;
        int beep2_end = gap_end + beep1_len;
        if (g_npc_beep_pos < beep1_len) {
          int env = (beep1_len - g_npc_beep_pos) * 65536 / beep1_len;
          volume = (int)((uint32)volume * env >> 16);
        } else if (g_npc_beep_pos >= gap_end && g_npc_beep_pos < beep2_end) {
          int p = g_npc_beep_pos - gap_end;
          int env = (beep1_len - p) * 65536 / beep1_len;
          volume = (int)((uint32)volume * env >> 16);
        } else {
          volume = 0;
        }
      } else if (c == kSpatialCue_Enemy) {
        int base = volume / 3;
        int ding = (int)((uint32)(volume * 2 / 3) * g_enemy_envelope >> 16);
        volume = (base + ding) * 2;
        if (volume > 512) volume = 512;
      } else if (c == kSpatialCue_Door) {
        int base = volume / 3;
        int ding = (int)((uint32)(volume * 2 / 3) * g_door_envelope >> 16);
        volume = (base + ding) * 2;
        if (volume > 512) volume = 512;
      } else if (c == kSpatialCue_StairUp || c == kSpatialCue_StairDown) {
        int base = volume / 3;
        int ding = (int)((uint32)(volume * 2 / 3) * g_stair_envelope >> 16);
        volume = (base + ding) * 2;
        if (volume > 512) volume = 512;
      } else if (c == kSpatialCue_HoleL || c == kSpatialCue_HoleR) {
        int base = volume / 3;
        int ding = (int)((uint32)(volume * 2 / 3) * g_hole_envelope >> 16);
        volume = (base + ding) * 2;
        if (volume > 512) volume = 512;
      } else if (c == kSpatialCue_Liftable) {
        volume = (int)((uint32)volume * g_lift_envelope >> 16);
      } else if (c == kSpatialCue_Chest) {
        volume = (int)((uint32)volume * g_chest_envelope >> 16);
      } else if (c == kSpatialCue_Ledge) {
        volume = (int)((uint32)volume * g_ledge_envelope >> 16);
      } else if (c == kSpatialCue_DeepWater) {
        // 4Hz tremolo (amplitude modulation)
        int am_period = g_sample_rate / 4;
        int am_pos = s % am_period;
        int am_frac = am_pos * 256 / am_period;
        int am_sin = g_sine_table[am_frac & 0xFF];
        int am_mult = 128 + (am_sin >> 8);  // 0-256 range
        volume = (int)((uint32)volume * g_water_envelope >> 16);
        volume = volume * am_mult >> 8;
      } else if (c == kSpatialCue_Hazard) {
        volume = (int)((uint32)volume * g_hazard_envelope >> 16);
      } else if (c == kSpatialCue_Conveyor) {
        // Double-pulse pattern: two 30ms pulses separated by 30ms gap, then silence
        int pulse_len = g_sample_rate * 30 / 1000;
        int pos_in_cycle = g_conveyor_cycle_len - g_conveyor_timer;
        bool pulse_on = (pos_in_cycle < pulse_len) ||
                        (pos_in_cycle >= pulse_len * 2 && pos_in_cycle < pulse_len * 3);
        if (!pulse_on) volume = 0;
      }

      // Apply per-group volume
      int grp = CueToGroup(c);
      if (grp >= 0)
        volume = volume * g_cue_group_volume[grp] / 100;

      int gain_L, gain_R;
      if (c >= kSpatialCue_WallN && c <= kSpatialCue_WallW) {
        // Hard-pan wall cues by direction for clear spatial distinction
        switch (c) {
        case kSpatialCue_WallE: gain_L = 32;  gain_R = 224; break;  // right
        case kSpatialCue_WallW: gain_L = 224; gain_R = 32;  break;  // left
        default:                gain_L = 128; gain_R = 128; break;  // N/S center
        }
      } else {
        int clamped_dx = dx < -64 ? -64 : (dx > 64 ? 64 : dx);
        gain_L = (64 - clamped_dx) * 2;
        gain_R = (64 + clamped_dx) * 2;
      }

      int semi = dy / 16;
      if (semi < -4) semi = -4;
      if (semi > 4) semi = 4;
      uint32 adj_inc = (uint32)((uint64)g_phase_inc[c] * g_pitch_mult[semi + 4] >> 15);

      // Per-cue waveform synthesis
      int16 raw;
      if (c == kSpatialCue_StairUp || c == kSpatialCue_StairDown) {
        // Stairs: continuous pitch sweep (up=rising, down=falling)
        uint32 sweep_period = (uint32)(g_sample_rate * 3 / 4);
        uint32 frac;
        if (c == kSpatialCue_StairUp)
          frac = (g_stair_sweep * 65536) / sweep_period;
        else
          frac = ((sweep_period - g_stair_sweep) * 65536) / sweep_period;
        uint32 inc = adj_inc + (uint32)((uint64)adj_inc * frac >> 16);
        raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
        g_phase[c] += inc;
      } else if (c == kSpatialCue_Ledge) {
        // Descending sweep: 600→300Hz per ding (sweep over 350ms cycle)
        uint32 sweep_period = (uint32)(g_sample_rate * 350 / 1000);
        uint32 frac = ((sweep_period - g_ledge_sweep) * 65536) / sweep_period;
        uint32 inc = adj_inc / 2 + (uint32)((uint64)(adj_inc / 2) * frac >> 16);
        raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
        g_phase[c] += inc;
      } else if (c == kSpatialCue_Hazard) {
        // Full-wave rectified sine: abs(sin) creates harsh buzz
        raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
        g_phase[c] += adj_inc;
        if (raw < 0) raw = -raw;
      } else if (c == kSpatialCue_Enemy) {
        // Square wave at 440Hz — harsh, menacing character
        int phase_idx = (g_phase[c] >> 16) & 0xFF;
        raw = (phase_idx < 128) ? 8000 : -8000;
        g_phase[c] += adj_inc;
      } else if (c == kSpatialCue_NPC) {
        // Double-chime: C4 then E4, position selects which note
        int beep1_len = g_sample_rate * 60 / 1000;
        int gap_end = beep1_len + g_sample_rate * 40 / 1000;
        if (g_npc_beep_pos >= gap_end) {
          // Second note: E4 (330Hz)
          raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
          g_phase[c] += g_npc_phase_inc2;
        } else {
          // First note: C4 (262Hz) — uses base phase_inc
          raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
          g_phase[c] += adj_inc;
        }
      } else if (c == kSpatialCue_Liftable) {
        // Ascending chirp: 200→600Hz over 120ms sweep period
        uint32 sweep_len = (uint32)(g_sample_rate * 120 / 1000);
        uint32 sw = g_lift_sweep < sweep_len ? g_lift_sweep : sweep_len;
        // frac: 0→65536 over sweep_len => freq goes 1x→3x base (200→600Hz)
        uint32 frac = (sw * 65536) / sweep_len;
        uint32 inc = adj_inc + (uint32)((uint64)(adj_inc * 2) * frac >> 16);
        raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
        g_phase[c] += inc;
      } else if (c == kSpatialCue_Chest) {
        // Vibrato shimmer: 523Hz with 8Hz ±30Hz wobble
        uint32 vib_period = (uint32)(g_sample_rate / 8);
        uint32 vib_pos = g_chest_timer > 0 ? (uint32)(g_sample_rate * 800 / 1000 - g_chest_timer) % vib_period : 0;
        int vib_frac = (int)(vib_pos * 256 / vib_period);
        int vib_mod = g_sine_table[vib_frac & 0xFF];  // -16383..16383
        // ±30Hz wobble as fraction of base: 30/523 ≈ 0.057 → ~3735 in 16.16
        uint32 inc = (uint32)((int64)adj_inc + ((int64)adj_inc * vib_mod / 16383 * 30 / 523));
        raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
        g_phase[c] += inc;
      } else if (c == kSpatialCue_HoleL || c == kSpatialCue_HoleR) {
        // Descending sweep: 300→150Hz over 200ms
        uint32 sweep_len = (uint32)(g_sample_rate * 200 / 1000);
        uint32 sw = g_hole_sweep < sweep_len ? g_hole_sweep : sweep_len;
        // frac: 65536→0 (descending) => freq goes from adj_inc to adj_inc/2
        uint32 frac = ((sweep_len - sw) * 65536) / sweep_len;
        uint32 inc = adj_inc / 2 + (uint32)((uint64)(adj_inc / 2) * frac >> 16);
        raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
        g_phase[c] += inc;
      } else {
        raw = g_sine_table[(g_phase[c] >> 16) & 0xFF];
        g_phase[c] += adj_inc;
      }

      int32 scaled = (int32)raw * volume >> 8;
      mix_L += scaled * gain_L >> 8;
      mix_R += scaled * gain_R >> 8;
    }

    // Blocked ding
    if (g_blocked_active) {
      if (g_blocked_timer <= 0) {
        g_blocked_envelope = 65536;
        g_blocked_timer = g_sample_rate / 2;
      }
    }
    if (g_blocked_timer > 0) g_blocked_timer--;
    g_blocked_envelope = (uint32)((uint64)g_blocked_envelope * g_blocked_decay >> 16);
    if (g_blocked_envelope > 0) {
      int16 raw = g_sine_table[(g_blocked_phase >> 16) & 0xFF];
      g_blocked_phase += g_blocked_phase_inc;
      int32 scaled = (int32)raw * (int)(g_blocked_envelope >> 8) >> 8;
      scaled = scaled * g_cue_group_volume[kCueGroup_Combat] / 100;
      mix_L += scaled;
      mix_R += scaled;
    }

    // Hole proximity warning ding — pitch encodes up/down, panning encodes left/right
    if (g_hole_warn_active) {
      if (g_hole_warn_timer <= 0) {
        g_hole_warn_envelope = 65536;
        g_hole_warn_timer = g_sample_rate / 4;  // repeat every 250ms
      }
    }
    if (g_hole_warn_timer > 0) g_hole_warn_timer--;
    g_hole_warn_envelope = (uint32)((uint64)g_hole_warn_envelope * g_hole_warn_decay >> 16);
    if (g_hole_warn_envelope > 100) {
      // Pick phase increment based on hole direction: up=high, down=low, level=base
      uint32 warn_inc;
      if (g_hole_warn_dy < -4)
        warn_inc = g_hole_warn_phase_inc_hi;   // hole above → high pitch
      else if (g_hole_warn_dy > 4)
        warn_inc = g_hole_warn_phase_inc_lo;   // hole below → low pitch
      else
        warn_inc = g_hole_warn_phase_inc_base;  // hole at same level
      int16 raw = g_sine_table[(g_hole_warn_phase >> 16) & 0xFF];
      g_hole_warn_phase += warn_inc;
      int32 scaled = (int32)raw * (int)(g_hole_warn_envelope >> 8) >> 8;
      scaled = scaled * g_cue_group_volume[kCueGroup_Holes] / 100;
      // Pan based on hole's horizontal direction
      int cdx = g_hole_warn_dx < -16 ? -16 : (g_hole_warn_dx > 16 ? 16 : g_hole_warn_dx);
      int wL = (16 - cdx) * 8;  // 0-256 range
      int wR = (16 + cdx) * 8;
      mix_L += scaled * wL >> 8;
      mix_R += scaled * wR >> 8;
    }

    // Combat indicators

    // Sword range: ascending double-beep (880Hz then 1100Hz)
    // Pattern: [beep1 40ms] [gap 20ms] [beep2 40ms] [silence until 500ms]
    if (g_sword_range_active) {
      if (g_sword_range_timer <= 0) {
        g_sword_range_timer = g_sample_rate / 2;  // repeat every 500ms
        g_sword_range_beep_pos = 0;
      }
      int beep1_end = g_sample_rate * 40 / 1000;
      int gap_end = g_sample_rate * 60 / 1000;
      int beep2_end = g_sample_rate * 100 / 1000;
      if (g_sword_range_beep_pos < beep1_end) {
        // First beep: 880Hz
        int16 raw = g_sine_table[(g_sword_range_phase >> 16) & 0xFF];
        g_sword_range_phase += g_sword_range_phase_inc;
        int vol = 65536 - (g_sword_range_beep_pos * 65536 / beep1_end);
        int32 scaled = (int32)raw * (vol >> 8) >> 8;
        scaled = scaled * g_cue_group_volume[kCueGroup_Combat] / 100;
        mix_L += scaled;
        mix_R += scaled;
      } else if (g_sword_range_beep_pos >= gap_end && g_sword_range_beep_pos < beep2_end) {
        // Second beep: 1100Hz (higher = ascending)
        int pos_in_beep = g_sword_range_beep_pos - gap_end;
        int beep_len = beep2_end - gap_end;
        int16 raw = g_sine_table[(g_sword_range_phase >> 16) & 0xFF];
        g_sword_range_phase += g_sword_range_phase_inc2;
        int vol = 65536 - (pos_in_beep * 65536 / beep_len);
        int32 scaled = (int32)raw * (vol >> 8) >> 8;
        scaled = scaled * g_cue_group_volume[kCueGroup_Combat] / 100;
        mix_L += scaled;
        mix_R += scaled;
      }
      g_sword_range_beep_pos++;
    }
    if (g_sword_range_timer > 0) g_sword_range_timer--;

    // Danger drill: rapid amplitude-modulated buzz on entering danger zone
    if (g_danger_drill_pos >= 0 && g_danger_drill_pos < g_danger_drill_len) {
      // 300Hz tone modulated on/off at 25Hz (toggle every 20ms)
      int mod_period = g_sample_rate / 25;
      bool mod_on = ((g_danger_drill_pos / (mod_period / 2)) & 1) == 0;
      if (mod_on) {
        int phase_idx = (g_danger_phase >> 16) & 0xFF;
        int16 raw = (phase_idx < 128) ? 10000 : -10000;  // square wave
        g_danger_phase += g_danger_phase_inc;
        // Fade out over duration
        int vol = (g_danger_drill_len - g_danger_drill_pos) * 256 / g_danger_drill_len;
        int32 scaled = (int32)raw * vol >> 8;
        scaled = scaled * g_cue_group_volume[kCueGroup_Combat] / 100;
        mix_L += scaled;
        mix_R += scaled;
      }
      g_danger_drill_pos++;
    }

    // Danger exit chime: E4 then B3 (descending) when leaving danger zone
    if (g_danger_exit_pos >= 0 && g_danger_exit_pos < g_danger_exit_len) {
      int note_len = g_sample_rate * 40 / 1000;   // 40ms per note
      int gap_end = note_len + g_sample_rate * 40 / 1000;  // 40ms gap
      int16 raw = 0;
      if (g_danger_exit_pos < note_len) {
        // First note: E4 (330Hz)
        raw = g_sine_table[(g_danger_phase >> 16) & 0xFF];
        g_danger_phase += g_danger_exit_inc1;
        int fade = (note_len - g_danger_exit_pos) * 256 / note_len;
        raw = (int16)((int32)raw * fade >> 8);
      } else if (g_danger_exit_pos >= gap_end) {
        // Second note: B3 (247Hz)
        raw = g_sine_table[(g_danger_phase >> 16) & 0xFF];
        g_danger_phase += g_danger_exit_inc2;
        int pos_in_note = g_danger_exit_pos - gap_end;
        int fade = (note_len - pos_in_note) * 256 / note_len;
        if (fade < 0) fade = 0;
        raw = (int16)((int32)raw * fade >> 8);
      }
      if (raw) {
        int32 dr = (int32)raw * g_cue_group_volume[kCueGroup_Combat] / 100;
        mix_L += dr;
        mix_R += dr;
      }
      g_danger_exit_pos++;
    }

    // Alignment sonar ping — variable rate based on precision
    if (g_align_interval > 0) {
      if (g_align_timer <= 0) {
        g_align_envelope = 65536;
        g_align_timer = g_align_interval;
      }
    }
    if (g_align_timer > 0) g_align_timer--;
    g_align_envelope = (uint32)((uint64)g_align_envelope * g_align_decay >> 16);
    if (g_align_envelope > 100) {
      int16 raw = g_sine_table[(g_align_phase >> 16) & 0xFF];
      g_align_phase += g_align_phase_inc;
      int32 scaled = (int32)raw * (int)(g_align_envelope >> 8) >> 8;
      scaled = scaled * g_cue_group_volume[kCueGroup_Combat] / 100;
      mix_L += scaled;
      mix_R += scaled;
    }

    // Room change chime
    if (g_room_chime_envelope > 100) {
      int16 raw = g_sine_table[(g_room_chime_phase >> 16) & 0xFF];
      g_room_chime_phase += g_room_chime_phase_inc;
      int32 scaled = (int32)raw * (int)(g_room_chime_envelope >> 8) >> 8;
      mix_L += scaled;
      mix_R += scaled;
    }
    g_room_chime_envelope = (uint32)((uint64)g_room_chime_envelope * g_room_chime_decay >> 16);

    // Passage chimes: double-note when a lateral wall disappears
    for (int pw = 0; pw < 4; pw++) {
      if (g_passage_pos[pw] < 0 || g_passage_pos[pw] >= g_passage_chime_len) {
        if (g_passage_pos[pw] >= g_passage_chime_len) g_passage_pos[pw] = -1;
        continue;
      }
      int note1_end = g_sample_rate * 50 / 1000;
      int gap_end = g_sample_rate * 70 / 1000;
      int note2_end = g_passage_chime_len;
      int16 raw = 0;
      if (g_passage_pos[pw] < note1_end) {
        raw = g_sine_table[(g_passage_phase[pw] >> 16) & 0xFF];
        g_passage_phase[pw] += g_passage_inc1[pw];
        int fade = (note1_end - g_passage_pos[pw]) * 256 / note1_end;
        raw = (int16)((int32)raw * fade >> 8);
      } else if (g_passage_pos[pw] >= gap_end && g_passage_pos[pw] < note2_end) {
        raw = g_sine_table[(g_passage_phase[pw] >> 16) & 0xFF];
        g_passage_phase[pw] += g_passage_inc2[pw];
        int pos_in = g_passage_pos[pw] - gap_end;
        int note2_len = note2_end - gap_end;
        int fade = (note2_len - pos_in) * 256 / note2_len;
        raw = (int16)((int32)raw * fade >> 8);
      }
      if (raw) {
        mix_L += (int32)raw * g_passage_pan_L[pw] >> 8;
        mix_R += (int32)raw * g_passage_pan_R[pw] >> 8;
      }
      g_passage_pos[pw]++;
    }

    // Terrain transition tone (center-panned, decaying sine)
    if (g_terrain_pos >= 0 && g_terrain_pos < g_terrain_len) {
      int16 raw = g_sine_table[(g_terrain_phase >> 16) & 0xFF];
      g_terrain_phase += g_terrain_phase_inc;
      int vol = (int)(g_terrain_envelope >> 8);
      if (g_terrain_am) {
        // 6Hz AM for shallow water
        int am_period = g_sample_rate / 6;
        int am_pos = g_terrain_pos % am_period;
        int am_frac = am_pos * 256 / am_period;
        int am_sin = g_sine_table[am_frac & 0xFF];
        vol = vol * (128 + (am_sin >> 8)) >> 8;
      }
      int32 scaled = (int32)raw * vol >> 8;
      scaled = scaled * g_cue_group_volume[kCueGroup_Terrain] / 100;
      mix_L += scaled;
      mix_R += scaled;
      g_terrain_envelope = (uint32)((uint64)g_terrain_envelope * g_terrain_decay >> 16);
      g_terrain_pos++;
    }

    // Legend demo sound synthesis
    if (g_legend_active && g_legend_demo_pos >= 0) {
      int demo_len = g_sample_rate;  // 1 second max demo
      if (g_legend_demo_pos < demo_len) {
        int16 demo_raw = 0;
        int demo_vol = 200;
        switch (g_legend_demo_type) {
        case 0: case 1: case 2: case 3: {
          // Wall clicks — use wall frequency, 60ms decay click
          int click_period = g_sample_rate * 200 / 1000;
          int pos_in_click = g_legend_demo_pos % click_period;
          if (pos_in_click == 0) g_legend_demo_phase = 0;
          int ds = g_sample_rate * 60 / 1000;
          int env = (pos_in_click < ds) ? ((ds - pos_in_click) * 256 / ds) : 0;
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += g_phase_inc[g_legend_demo_type];
          demo_vol = env;
          break;
        }
        case 4: {
          // Passage chime: D5-D6
          int note_len = g_sample_rate * 50 / 1000;
          int gap = g_sample_rate * 20 / 1000;
          if (g_legend_demo_pos < note_len) {
            demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
            g_legend_demo_phase += g_d5_inc;
            demo_vol = (note_len - g_legend_demo_pos) * 256 / note_len;
          } else if (g_legend_demo_pos >= note_len + gap && g_legend_demo_pos < note_len * 2 + gap) {
            demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
            g_legend_demo_phase += g_d6_inc;
            int p = g_legend_demo_pos - note_len - gap;
            demo_vol = (note_len - p) * 256 / note_len;
          }
          break;
        }
        case 5: {
          // Hole "falling tone": descending sweep 300→150Hz, 200ms decay, 500ms repeat
          int ding_period = g_sample_rate / 2;
          int pos_in = g_legend_demo_pos % ding_period;
          int ds = g_sample_rate * 200 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          uint32 sweep_len = (uint32)(g_sample_rate * 200 / 1000);
          uint32 sw = (uint32)pos_in < sweep_len ? (uint32)pos_in : sweep_len;
          uint32 frac = ((sweep_len - sw) * 65536) / sweep_len;
          uint32 base_inc = g_phase_inc[kSpatialCue_HoleL];
          uint32 inc = base_inc / 2 + (uint32)((uint64)(base_inc / 2) * frac >> 16);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 6: {
          // Ledge: descending sweep 600→300Hz
          int sweep_period = g_sample_rate * 350 / 1000;
          int pos_in = g_legend_demo_pos % sweep_period;
          int ds = g_sample_rate * 80 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          uint32 frac = ((sweep_period - pos_in) * 65536) / sweep_period;
          uint32 base_inc = g_phase_inc[kSpatialCue_Ledge];
          uint32 inc = base_inc / 2 + (uint32)((uint64)(base_inc / 2) * frac >> 16);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 7: {
          // Deep water: 180Hz with 4Hz tremolo
          int ding_period = g_sample_rate * 600 / 1000;
          int pos_in = g_legend_demo_pos % ding_period;
          int ds = g_sample_rate * 250 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          int am_period = g_sample_rate / 4;
          int am_frac = (pos_in % am_period) * 256 / am_period;
          int am_sin = g_sine_table[am_frac & 0xFF];
          int am_mult = 128 + (am_sin >> 8);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += g_phase_inc[kSpatialCue_DeepWater];
          demo_vol = env * am_mult >> 8;
          break;
        }
        case 8: {
          // Hazard: 800Hz rectified sine, rapid repeat
          int ding_period = g_sample_rate * 150 / 1000;
          int pos_in = g_legend_demo_pos % ding_period;
          int ds = g_sample_rate * 40 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += g_phase_inc[kSpatialCue_Hazard];
          if (demo_raw < 0) demo_raw = -demo_raw;
          demo_vol = env;
          break;
        }
        case 9: {
          // Conveyor: double-pulse 300Hz
          int cycle = g_sample_rate * 400 / 1000;
          int pulse = g_sample_rate * 30 / 1000;
          int pos_in = g_legend_demo_pos % cycle;
          bool pulse_on = (pos_in < pulse) || (pos_in >= pulse * 2 && pos_in < pulse * 3);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += g_phase_inc[kSpatialCue_Conveyor];
          if (!pulse_on) demo_raw = 0;
          break;
        }
        case 10: {
          // Enemy: 440Hz square wave buzz, 100ms decay, 200ms repeat
          int ding_period = g_sample_rate / 5;
          int pos_in = g_legend_demo_pos % ding_period;
          int ds = g_sample_rate * 100 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          int phase_idx = (g_legend_demo_phase >> 16) & 0xFF;
          demo_raw = (phase_idx < 128) ? 8000 : -8000;
          g_legend_demo_phase += g_phase_inc[kSpatialCue_Enemy];
          demo_vol = env;
          break;
        }
        case 11: {
          // NPC double-chime: C4 (262Hz) then E4 (330Hz), 60ms each, 40ms gap
          int beep1_len = g_sample_rate * 60 / 1000;
          int gap_end = beep1_len + g_sample_rate * 40 / 1000;
          int beep2_end = gap_end + beep1_len;
          int period = g_sample_rate * 12 / 10;
          int pos_in = g_legend_demo_pos % period;
          if (pos_in < beep1_len) {
            int env = (beep1_len - pos_in) * 256 / beep1_len;
            demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
            g_legend_demo_phase += g_phase_inc[kSpatialCue_NPC];
            demo_vol = env;
          } else if (pos_in >= gap_end && pos_in < beep2_end) {
            int p = pos_in - gap_end;
            int env = (beep1_len - p) * 256 / beep1_len;
            demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
            g_legend_demo_phase += g_npc_phase_inc2;
            demo_vol = env;
          }
          break;
        }
        case 12: {
          // Chest "sparkle": 523Hz with 8Hz vibrato, 200ms decay, 800ms repeat
          int ding_period = g_sample_rate * 800 / 1000;
          int pos_in = g_legend_demo_pos % ding_period;
          int ds = g_sample_rate * 200 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          uint32 vib_period = (uint32)(g_sample_rate / 8);
          int vib_frac = (int)((pos_in % vib_period) * 256 / vib_period);
          int vib_mod = g_sine_table[vib_frac & 0xFF];
          uint32 base_inc = g_phase_inc[kSpatialCue_Chest];
          uint32 inc = (uint32)((int64)base_inc + ((int64)base_inc * vib_mod / 16383 * 30 / 523));
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 13: {
          // Liftable "swipe": ascending chirp 200→600Hz, 120ms decay, 700ms repeat
          int ding_period = g_sample_rate * 700 / 1000;
          int pos_in = g_legend_demo_pos % ding_period;
          int ds = g_sample_rate * 120 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          uint32 sweep_len = (uint32)(g_sample_rate * 120 / 1000);
          uint32 sw = (uint32)pos_in < sweep_len ? (uint32)pos_in : sweep_len;
          uint32 frac = (sw * 65536) / sweep_len;
          uint32 base_inc = g_phase_inc[kSpatialCue_Liftable];
          uint32 inc = base_inc + (uint32)((uint64)(base_inc * 2) * frac >> 16);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 14: {
          // Door ding: 392Hz
          int ding_period = g_sample_rate * 2 / 5;
          int pos_in = g_legend_demo_pos % ding_period;
          int ds = g_sample_rate * 200 / 1000;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += g_phase_inc[kSpatialCue_Door];
          demo_vol = env;
          break;
        }
        case 15: {
          // Stairs up: rising sweep from 500Hz
          uint32 sweep_period = (uint32)(g_sample_rate * 3 / 4);
          uint32 pos_in = g_legend_demo_pos % sweep_period;
          uint32 frac = (pos_in * 65536) / sweep_period;
          uint32 base_inc = g_phase_inc[kSpatialCue_StairUp];
          uint32 inc = base_inc + (uint32)((uint64)base_inc * frac >> 16);
          int ds = g_sample_rate * 150 / 1000;
          int env_pos = g_legend_demo_pos % (g_sample_rate * 3 / 10);
          int env = (env_pos < ds) ? ((ds - env_pos) * 256 / ds) : 0;
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 16: {
          // Stairs down: falling sweep from 500Hz
          uint32 sweep_period = (uint32)(g_sample_rate * 3 / 4);
          uint32 pos_in = g_legend_demo_pos % sweep_period;
          uint32 frac = ((sweep_period - pos_in) * 65536) / sweep_period;
          uint32 base_inc = g_phase_inc[kSpatialCue_StairDown];
          uint32 inc = base_inc + (uint32)((uint64)base_inc * frac >> 16);
          int ds = g_sample_rate * 150 / 1000;
          int env_pos = g_legend_demo_pos % (g_sample_rate * 3 / 10);
          int env = (env_pos < ds) ? ((ds - env_pos) * 256 / ds) : 0;
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 17: {
          // Grass terrain: 200Hz, 100ms
          int ds = g_sample_rate * 100 / 1000;
          int env = (g_legend_demo_pos < ds) ? ((ds - g_legend_demo_pos) * 256 / ds) : 0;
          uint32 inc = (uint32)((uint64)200 * 256 * 65536 / g_sample_rate);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 18: {
          // Shallow water: 280Hz with 6Hz AM, 120ms
          int ds = g_sample_rate * 120 / 1000;
          int env = (g_legend_demo_pos < ds) ? ((ds - g_legend_demo_pos) * 256 / ds) : 0;
          int am_period = g_sample_rate / 6;
          int am_frac = (g_legend_demo_pos % am_period) * 256 / am_period;
          int am_sin = g_sine_table[am_frac & 0xFF];
          int am_mult = 128 + (am_sin >> 8);
          uint32 inc = (uint32)((uint64)280 * 256 * 65536 / g_sample_rate);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env * am_mult >> 8;
          break;
        }
        case 19: {
          // Ice floor: 1000Hz, 60ms
          int ds = g_sample_rate * 60 / 1000;
          int env = (g_legend_demo_pos < ds) ? ((ds - g_legend_demo_pos) * 256 / ds) : 0;
          uint32 inc = (uint32)((uint64)1000 * 256 * 65536 / g_sample_rate);
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += inc;
          demo_vol = env;
          break;
        }
        case 20: {
          // Sword range: ascending double-beep (880Hz then 1100Hz)
          int beep1_end = g_sample_rate * 40 / 1000;
          int gap_end = g_sample_rate * 60 / 1000;
          int beep2_end = g_sample_rate * 100 / 1000;
          int period = g_sample_rate / 2;
          int pos_in = g_legend_demo_pos % period;
          if (pos_in < beep1_end) {
            demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
            g_legend_demo_phase += g_sword_range_phase_inc;
            demo_vol = (beep1_end - pos_in) * 256 / beep1_end;
          } else if (pos_in >= gap_end && pos_in < beep2_end) {
            demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
            g_legend_demo_phase += g_sword_range_phase_inc2;
            int p = pos_in - gap_end;
            demo_vol = (beep2_end - gap_end - p) * 256 / (beep2_end - gap_end);
          }
          break;
        }
        case 21: {
          // Danger zone: rapid buzzing drill (300Hz AM at 25Hz)
          int mod_period = g_sample_rate / 25;
          bool mod_on = ((g_legend_demo_pos / (mod_period / 2)) & 1) == 0;
          if (mod_on) {
            int phase_idx = (g_legend_demo_phase >> 16) & 0xFF;
            demo_raw = (phase_idx < 128) ? 10000 : -10000;
            g_legend_demo_phase += g_danger_phase_inc;
            int fade = (demo_len - g_legend_demo_pos) * 256 / demo_len;
            demo_vol = fade;
          }
          break;
        }
        case 22: {
          // Alignment sonar: 1500Hz ping
          int ds = g_sample_rate * 30 / 1000;
          int period = g_sample_rate / 5;
          int pos_in = g_legend_demo_pos % period;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += g_align_phase_inc;
          demo_vol = env;
          break;
        }
        case 23: {
          // Blocked: 110Hz low thud
          int ds = g_sample_rate * 200 / 1000;
          int period = g_sample_rate / 2;
          int pos_in = g_legend_demo_pos % period;
          int env = (pos_in < ds) ? ((ds - pos_in) * 256 / ds) : 0;
          demo_raw = g_sine_table[(g_legend_demo_phase >> 16) & 0xFF];
          g_legend_demo_phase += g_blocked_phase_inc;
          demo_vol = env;
          break;
        }
        }
        int32 demo_scaled = (int32)demo_raw * demo_vol >> 8;
        mix_L += demo_scaled;
        mix_R += demo_scaled;
        g_legend_demo_pos++;
      }
    }

    buf[s * 2 + 0] += (int16)(mix_L >> 1);
    buf[s * 2 + 1] += (int16)(mix_R >> 1);
  }
}

// --- Sound Legend ---

#define LEGEND_COUNT 24

bool SpatialAudio_IsLegendActive(void) {
  return g_legend_active;
}

static void LegendStartDemo(int index) {
  g_legend_demo_type = index;
  g_legend_demo_pos = 0;
  g_legend_demo_phase = 0;
  g_legend_demo_sweep = 0;
}

void SpatialAudio_ToggleLegend(void) {
  if (!g_enabled) return;
  if (g_options_active) return;
  g_legend_active = !g_legend_active;
  if (g_legend_active) {
    g_legend_index = 0;
    g_legend_demo_pos = -1;
    // Clear spatial cues so they don't play during legend
    memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));
#if defined(__APPLE__) || defined(_WIN32)
    SpeechSynthesis_Speak(A11y(kA11y_LegendOpen));
#endif
  } else {
    g_legend_demo_pos = -1;
#if defined(__APPLE__) || defined(_WIN32)
    SpeechSynthesis_Speak(A11y(kA11y_LegendClosed));
#endif
  }
}

void SpatialAudio_LegendNavigate(int dir) {
  if (!g_legend_active) return;
  g_legend_index += dir;
  if (g_legend_index < 0) g_legend_index = LEGEND_COUNT - 1;
  if (g_legend_index >= LEGEND_COUNT) g_legend_index = 0;
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_Speak(A11yLegendName(g_legend_index));
#endif
  LegendStartDemo(g_legend_index);
}

// --- Accessibility Options Menu ---

// Top-level menu items
enum {
  kTopMenu_SpeechRate,
  kTopMenu_SpeechVoice,
  kTopMenu_SpeechVolume,
  kTopMenu_SoundSetup,
  kTopMenu_SaveOptions,
  kTopMenu_Count,
};


// Sound setup submenu items
enum {
  kSoundSetup_Walls,
  kSoundSetup_Holes,
  kSoundSetup_Enemy,
  kSoundSetup_NPC,
  kSoundSetup_Chest,
  kSoundSetup_Liftable,
  kSoundSetup_Door,
  kSoundSetup_Stairs,
  kSoundSetup_Ledge,
  kSoundSetup_DeepWater,
  kSoundSetup_Hazard,
  kSoundSetup_Conveyor,
  kSoundSetup_Terrain,
  kSoundSetup_Combat,
  kSoundSetup_DetectionRange,
  kSoundSetup_Back,
  kSoundSetup_Count,
};


bool SpatialAudio_IsOptionsActive(void) {
  return g_options_active;
}

#if defined(__APPLE__) || defined(_WIN32)
static void OptionsAnnounceCurrentItem(void) {
  char buf[128];
  if (g_options_menu == 0) {
    switch (g_options_index) {
    case kTopMenu_SpeechRate: {
      int pct = (int)(SpeechSynthesis_GetRate() * 200);
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtSpeechRate), pct);
      SpeechSynthesis_Speak(buf);
      break;
    }
    case kTopMenu_SpeechVoice: {
      int vi = SpeechSynthesis_GetCurrentVoiceIndex();
      const char *name = SpeechSynthesis_GetVoiceName(vi);
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtSpeechVoice), name ? name : A11y(kA11y_FmtDefault));
      SpeechSynthesis_Speak(buf);
      break;
    }
    case kTopMenu_SpeechVolume: {
      int pct = (int)(SpeechSynthesis_GetVolume() * 100);
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtSpeechVolume), pct);
      SpeechSynthesis_Speak(buf);
      break;
    }
    default: {
      const char *name = A11yTopMenuName(g_options_index);
      if (name) SpeechSynthesis_Speak(name);
      break;
    }
    }
  } else if (g_options_menu == 1) {
    const char *name = SpeechSynthesis_GetVoiceName(g_options_index);
    int cur = SpeechSynthesis_GetCurrentVoiceIndex();
    if (name) {
      if (g_options_index == cur)
        snprintf(buf, sizeof(buf), A11y(kA11y_FmtSelected), name);
      else
        snprintf(buf, sizeof(buf), "%s", name);
      SpeechSynthesis_Speak(buf);
    }
  } else if (g_options_menu == 2) {
    if (g_options_index < kCueGroup_Count) {
      const char *name = A11ySoundSetupName(g_options_index);
      char pct[32];
      snprintf(pct, sizeof(pct), A11y(kA11y_FmtPercent), g_cue_group_volume[g_options_index]);
      snprintf(buf, sizeof(buf), "%s, %s", name ? name : "", pct);
      SpeechSynthesis_Speak(buf);
    } else if (g_options_index == kSoundSetup_DetectionRange) {
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtDetectionRange), g_scan_range);
      SpeechSynthesis_Speak(buf);
    } else if (g_options_index == kSoundSetup_Back) {
      SpeechSynthesis_Speak(A11y(kA11y_Back));
    }
  }
}
#endif

void SpatialAudio_ToggleOptions(void) {
  if (!g_enabled) return;
  if (g_legend_active) return;
  g_options_active = !g_options_active;
  if (g_options_active) {
    g_options_index = 0;
    g_options_menu = 0;
    memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));
#if defined(__APPLE__) || defined(_WIN32)
    SpeechSynthesis_Speak(A11y(kA11y_OptionsOpen));
#endif
  } else {
#if defined(__APPLE__) || defined(_WIN32)
    SpeechSynthesis_Speak(A11y(kA11y_OptionsClosed));
#endif
  }
}

void SpatialAudio_OptionsNavigate(int dir) {
  if (!g_options_active) return;
  int count;
  if (g_options_menu == 0)
    count = kTopMenu_Count;
  else if (g_options_menu == 1)
    count = SpeechSynthesis_GetVoiceCount();
  else
    count = kSoundSetup_Count;

  if (count <= 0) return;
  g_options_index += dir;
  if (g_options_index < 0) g_options_index = count - 1;
  if (g_options_index >= count) g_options_index = 0;
#if defined(__APPLE__) || defined(_WIN32)
  OptionsAnnounceCurrentItem();
#endif
}

void SpatialAudio_OptionsAdjust(int dir) {
  if (!g_options_active) return;
#if defined(__APPLE__) || defined(_WIN32)
  char buf[64];
#endif

  if (g_options_menu == 0) {
    // Top-level sliders
    switch (g_options_index) {
    case kTopMenu_SpeechRate: {
      float rate = SpeechSynthesis_GetRate() + dir * 0.05f;
      SpeechSynthesis_SetRate(rate);
      int pct = (int)(SpeechSynthesis_GetRate() * 200);
#if defined(__APPLE__) || defined(_WIN32)
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtPercent), pct);
      SpeechSynthesis_Speak(buf);
#endif
      break;
    }
    case kTopMenu_SpeechVolume: {
      float vol = SpeechSynthesis_GetVolume() + dir * 0.1f;
      SpeechSynthesis_SetVolume(vol);
      int pct = (int)(SpeechSynthesis_GetVolume() * 100);
#if defined(__APPLE__) || defined(_WIN32)
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtPercent), pct);
      SpeechSynthesis_Speak(buf);
#endif
      break;
    }
    default:
      break;
    }
  } else if (g_options_menu == 2) {
    // Sound setup sliders
    if (g_options_index < kCueGroup_Count) {
      int vol = g_cue_group_volume[g_options_index] + dir * 10;
      if (vol < 0) vol = 0;
      if (vol > 100) vol = 100;
      g_cue_group_volume[g_options_index] = vol;
#if defined(__APPLE__) || defined(_WIN32)
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtPercent), vol);
      SpeechSynthesis_Speak(buf);
#endif
    } else if (g_options_index == kSoundSetup_DetectionRange) {
      int range = g_scan_range + dir * 16;
      if (range < 32) range = 32;
      if (range > 192) range = 192;
      g_scan_range = range;
#if defined(__APPLE__) || defined(_WIN32)
      snprintf(buf, sizeof(buf), "%d", range);
      SpeechSynthesis_Speak(buf);
#endif
    }
  }
}

void SpatialAudio_OptionsSelect(void) {
  if (!g_options_active) return;

  if (g_options_menu == 0) {
    switch (g_options_index) {
    case kTopMenu_SpeechVoice: {
      int vc = SpeechSynthesis_GetVoiceCount();
      if (vc > 0) {
        g_options_menu = 1;
        g_options_index = SpeechSynthesis_GetCurrentVoiceIndex();
#if defined(__APPLE__) || defined(_WIN32)
        SpeechSynthesis_Speak(A11y(kA11y_VoiceSubmenuOpen));
#endif
      }
      break;
    }
    case kTopMenu_SoundSetup:
      g_options_menu = 2;
      g_options_index = 0;
#if defined(__APPLE__) || defined(_WIN32)
      SpeechSynthesis_Speak(A11y(kA11y_SoundSetupOpen));
#endif
      break;
    case kTopMenu_SaveOptions:
      SpatialAudio_SaveSettings();
#if defined(__APPLE__) || defined(_WIN32)
      SpeechSynthesis_Speak(A11y(kA11y_OptionsSaved));
#endif
      break;
    default:
      break;
    }
  } else if (g_options_menu == 1) {
    // Voice submenu: select voice
    SpeechSynthesis_SetVoice(g_options_index);
#if defined(__APPLE__) || defined(_WIN32)
    {
      char buf[128];
      const char *name = SpeechSynthesis_GetVoiceName(g_options_index);
      snprintf(buf, sizeof(buf), A11y(kA11y_FmtSelected), name ? name : A11y(kA11y_FmtDefault));
      SpeechSynthesis_Speak(buf);
    }
#endif
    // Return to top menu
    g_options_menu = 0;
    g_options_index = kTopMenu_SpeechVoice;
  } else if (g_options_menu == 2) {
    if (g_options_index == kSoundSetup_Back) {
      g_options_menu = 0;
      g_options_index = kTopMenu_SoundSetup;
#if defined(__APPLE__) || defined(_WIN32)
      OptionsAnnounceCurrentItem();
#endif
    }
  }
}

// Escape handling for submenus: returns true if handled (went back), false if should close
bool OptionsHandleEscape(void) {
  if (g_options_menu == 1) {
    g_options_index = kTopMenu_SpeechVoice;
    g_options_menu = 0;
#if defined(__APPLE__) || defined(_WIN32)
    OptionsAnnounceCurrentItem();
#endif
    return true;
  }
  if (g_options_menu == 2) {
    g_options_index = kTopMenu_SoundSetup;
    g_options_menu = 0;
#if defined(__APPLE__) || defined(_WIN32)
    OptionsAnnounceCurrentItem();
#endif
    return true;
  }
  return false;
}

// Settings getters/setters
int SpatialAudio_GetCueGroupVolume(int group) {
  if (group < 0 || group >= kCueGroup_Count) return 100;
  return g_cue_group_volume[group];
}

void SpatialAudio_SetCueGroupVolume(int group, int vol) {
  if (group < 0 || group >= kCueGroup_Count) return;
  if (vol < 0) vol = 0;
  if (vol > 100) vol = 100;
  g_cue_group_volume[group] = vol;
}

int SpatialAudio_GetScanRange(void) {
  return g_scan_range;
}

void SpatialAudio_SetScanRange(int range) {
  if (range < 32) range = 32;
  if (range > 192) range = 192;
  g_scan_range = range;
}

// --- Settings Persistence ---

static const char * const kCueGroupKeys[kCueGroup_Count] = {
  "walls_volume", "holes_volume", "enemy_volume", "npc_volume",
  "chest_volume", "liftable_volume", "door_volume", "stairs_volume",
  "ledge_volume", "deepwater_volume", "hazard_volume", "conveyor_volume",
  "terrain_volume", "combat_volume",
};

void SpatialAudio_SaveSettings(void) {
  FILE *f = fopen("zelda3_a11y.ini", "w");
  if (!f) return;

  fprintf(f, "[Accessibility]\n");

#if defined(__APPLE__)
  fprintf(f, "speech_rate=%.3f\n", SpeechSynthesis_GetRate());
  fprintf(f, "speech_volume=%.2f\n", SpeechSynthesis_GetVolume());
  const char *vid = SpeechSynthesis_GetVoiceId(SpeechSynthesis_GetCurrentVoiceIndex());
  if (vid)
    fprintf(f, "speech_voice_id=%s\n", vid);
#endif

  for (int i = 0; i < kCueGroup_Count; i++)
    fprintf(f, "%s=%d\n", kCueGroupKeys[i], g_cue_group_volume[i]);
  fprintf(f, "scan_range=%d\n", g_scan_range);

  fclose(f);
}

void SpatialAudio_LoadSettings(void) {
  FILE *f = fopen("zelda3_a11y.ini", "r");
  if (!f) return;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    // Strip newline
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    nl = strchr(line, '\r');
    if (nl) *nl = '\0';

    // Skip comments and section headers
    if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == '\0')
      continue;

    char *eq = strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';
    const char *key = line;
    const char *val = eq + 1;

#if defined(__APPLE__)
    if (strcmp(key, "speech_rate") == 0) {
      SpeechSynthesis_SetRate((float)atof(val));
      continue;
    }
    if (strcmp(key, "speech_volume") == 0) {
      SpeechSynthesis_SetVolume((float)atof(val));
      continue;
    }
    if (strcmp(key, "speech_voice_id") == 0) {
      SpeechSynthesis_SetVoiceById(val);
      continue;
    }
#endif

    if (strcmp(key, "scan_range") == 0) {
      int r = atoi(val);
      if (r >= 32 && r <= 192) g_scan_range = r;
      continue;
    }

    for (int i = 0; i < kCueGroup_Count; i++) {
      if (strcmp(key, kCueGroupKeys[i]) == 0) {
        int v = atoi(val);
        if (v >= 0 && v <= 100) g_cue_group_volume[i] = v;
        break;
      }
    }
  }
  fclose(f);
}
