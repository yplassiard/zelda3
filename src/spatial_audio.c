#include "spatial_audio.h"
#include "variables.h"
#include "tile_detect.h"
#include "zelda_rtl.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#ifdef __APPLE__
#include "platform/macos/speechsynthesis.h"
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

// Dialog choice tracking
static uint8 g_prev_choice;
static uint8 g_prev_gameover_choice;

// Inventory tracking
static uint8 g_prev_hud_item;
static bool g_prev_menu_open;

// Dungeon name tracking
static uint16 g_prev_palace_index;
static bool g_prev_indoors;

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

// Hole ding envelope (for continuous tone)
static uint32 g_hole_envelope;
static int g_hole_timer;
static uint32 g_hole_decay;

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

// Base frequencies per category
static const uint16 kBaseFreq[kSpatialCue_Count] = {
  150,  // Wall
  220,  // HoleL (A3)
  220,  // HoleR (A3) — same sound, split by side
  440,  // Enemy (A4)
  330,  // NPC (E4)
  523,  // Chest (C5)
  262,  // Liftable (C4)
  392,  // Door (G4)
  500,  // StairUp — rising sweep from 500Hz
  500,  // StairDown — falling sweep from 500Hz
};

#define SCAN_RANGE 96

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

#ifdef __APPLE__
static void Speak(const char *fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  SpeechSynthesis_Speak(buf);
}
#endif

void SpatialAudio_Init(int sample_rate) {
  g_sample_rate = sample_rate;
  g_enabled = false;
  memset(g_phase, 0, sizeof(g_phase));
  memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));

  for (int i = 0; i < 256; i++)
    g_sine_table[i] = (int16)(sinf(2.0f * 3.14159265f * i / 256.0f) * 16383.0f);

  for (int i = 0; i < kSpatialCue_Count; i++)
    g_phase_inc[i] = (uint32)((uint64)kBaseFreq[i] * 256 * 65536 / sample_rate);

  for (int i = 0; i < 9; i++) {
    float semitones = (float)(i - 4);
    g_pitch_mult[i] = (uint16)(powf(2.0f, semitones / 12.0f) * 32768.0f);
  }

  // NPC ding: 150ms decay, 1s repeat
  int ds = sample_rate * 150 / 1000;
  g_ding_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_ding_envelope = 0;
  g_ding_timer = 0;

  // Enemy ding: 100ms decay, 200ms repeat
  ds = sample_rate * 100 / 1000;
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

  // Hole ding: 200ms decay, 500ms repeat
  ds = sample_rate * 200 / 1000;
  g_hole_decay = (uint32)(powf(0.01f, 1.0f / ds) * 65536.0f);
  g_hole_envelope = 0;
  g_hole_timer = 0;

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
  g_prev_choice = 0xFF;
  g_prev_gameover_choice = 0xFF;
  g_prev_hud_item = 0xFF;
  g_prev_menu_open = false;
  g_prev_palace_index = 0xFFFF;
  g_prev_indoors = false;
  g_tracking_initialized = false;
  g_prev_name_col = 0xFF;
  g_prev_name_row = 0xFF;
}

void SpatialAudio_Shutdown(void) {
  g_enabled = false;
}

void SpatialAudio_Toggle(void) {
  g_enabled = !g_enabled;
  if (!g_enabled)
    memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));
#ifdef __APPLE__
  SpeechSynthesis_Speak(g_enabled ? "Spatial audio on" : "Spatial audio off");
#endif
}

void SpatialAudio_SpeakHealth(void) {
#ifdef __APPLE__
  uint8 cur = link_health_current;
  uint8 cap = link_health_capacity;
  int hearts = cur / 8;
  int half = (cur % 8) >= 4 ? 1 : 0;
  int max_hearts = cap / 8;
  if (half)
    Speak("%d and a half out of %d hearts", hearts, max_hearts);
  else
    Speak("%d out of %d hearts", hearts, max_hearts);
#endif
}

static int ClassifyTile(uint8 tile) {
  if (tile >= 0x01 && tile <= 0x03) return kSpatialCue_Wall;
  if (tile == 0x26 || tile == 0x43)  return kSpatialCue_Wall;
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
  return -1;
}

#ifdef __APPLE__
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

  if (mod != g_prev_module || (mod == 1 && sub != g_prev_submodule)) {
    g_prev_module = mod;
    g_prev_submodule = sub;
    g_prev_cursor = 0xFF;

    if (mod == 1 && sub == 5)
      SpeechSynthesis_Speak("File Select");
    else if (mod == 2)
      SpeechSynthesis_Speak("Copy Player");
    else if (mod == 3)
      SpeechSynthesis_Speak("Erase Player");
    else if (mod == 4)
      SpeechSynthesis_Speak("Register Your Name");
    else
      return;
  }

  if (mod == 1 && sub == 5 && cursor != g_prev_cursor) {
    g_prev_cursor = cursor;
    char buf[64];
    if (cursor < 3) {
      uint16 valid = *(uint16 *)(g_zenv.sram + cursor * 0x500 + 0x3E5);
      if (valid == 0x55AA) {
        char name[16];
        DecodeSramName(cursor, name, sizeof(name));
        snprintf(buf, sizeof(buf), "File %d, %s", cursor + 1, name[0] ? name : "unnamed");
      } else {
        snprintf(buf, sizeof(buf), "File %d, empty", cursor + 1);
      }
    } else if (cursor == 3) {
      snprintf(buf, sizeof(buf), "Copy Player");
    } else {
      snprintf(buf, sizeof(buf), "Erase Player");
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
        snprintf(buf, sizeof(buf), "File %d, %s", cursor + 1, name[0] ? name : "unnamed");
      } else {
        snprintf(buf, sizeof(buf), "File %d, empty", cursor + 1);
      }
    } else {
      snprintf(buf, sizeof(buf), "Cancel");
    }
    SpeechSynthesis_Speak(buf);
  }
}

// --- Game Over choice tracking ---

static const char *kGameOverChoices[3] = {
  "Save and Continue",
  "Save and Quit",
  "Continue Without Saving",
};

static void AnnounceGameOverChoice(void) {
  uint8 mod = main_module_index;
  if (mod != 12) {
    g_prev_gameover_choice = 0xFF;
    return;
  }
  // submodule 9 = choice selection phase
  if (submodule_index != 9) return;

  uint8 choice = subsubmodule_index;
  if (choice != g_prev_gameover_choice && choice < 3) {
    g_prev_gameover_choice = choice;
    SpeechSynthesis_Speak(kGameOverChoices[choice]);
  }
}

// --- In-game dialog choice tracking ---

static const char *kSaveMenuChoices[2] = { "Continue", "Save and Quit" };

static void AnnounceDialogChoice(void) {
  uint8 mod = main_module_index;
  // Only track during messaging module (14) — choice_in_multiselect_box can get
  // written to during normal overworld/dungeon gameplay without an active dialog
  if (mod != 14) {
    g_prev_choice = 0xFF;
    return;
  }
  uint8 choice = choice_in_multiselect_box;
  if (choice != g_prev_choice && choice < 3) {
    g_prev_choice = choice;
    // Save menu (module 14, submodule 11): use specific names
    if (mod == 14 && submodule_index == 11 && choice < 2)
      SpeechSynthesis_Speak(kSaveMenuChoices[choice]);
    else
      Speak("Option %d", choice + 1);
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
    SpeechSynthesis_Speak("Back");
  else if (grid_val == 0x44)
    SpeechSynthesis_Speak("Next");
  else if (grid_val == 0x59 || grid_val == 0x6f)
    return;  // blank, don't announce
  else {
    uint8 idx = (uint8)grid_val;
    char ch = (idx < 128) ? kGridToChar[idx] : 0;
    if (ch) {
      char buf[16];
      if (ch >= 'A' && ch <= 'Z')
        Speak("Capital %c", ch);
      else
        Speak("%c", ch);
    }
  }
}

// --- Inventory tracking ---

static const char *kGameItemNames[22] = {
  "Empty",           // 0
  "Bow",             // 1
  "Boomerang",       // 2
  "Hookshot",        // 3
  "Bombs",           // 4
  "Mushroom",        // 5
  "Fire Rod",        // 6
  "Ice Rod",         // 7
  "Bombos",          // 8
  "Ether",           // 9
  "Quake",           // 10
  "Lamp",            // 11
  "Hammer",          // 12
  "Shovel",          // 13
  "Bug Net",         // 14
  "Book of Mudora",  // 15
  "Bottle",          // 16
  "Cane of Somaria", // 17
  "Cane of Byrna",   // 18
  "Magic Cape",      // 19
  "Magic Mirror",    // 20
  "Bottle",          // 21
};

static const uint8 kHudSlotToGameItem[25] = {
  0,
  3,  2, 14, 1,  10,  5,
  6, 15, 16, 17,  9,  4,
  8,  7, 12, 21, 18, 13,
  19, 20, 11, 11, 11, 11,
};

static void AnnounceInventory(void) {
  uint8 mod = main_module_index;
  // Inventory is active during module 14 with HUD open
  bool menu_open = (mod == 14 && submodule_index == 0);

  if (menu_open && !g_prev_menu_open) {
    // Menu just opened
    g_prev_hud_item = 0xFF;
  }
  g_prev_menu_open = menu_open;

  if (!menu_open) return;

  uint8 item = hud_cur_item;
  if (item != g_prev_hud_item && item < 25) {
    g_prev_hud_item = item;
    uint8 game_item = kHudSlotToGameItem[item];
    if (game_item < 22)
      SpeechSynthesis_Speak(kGameItemNames[game_item]);
  }
}

// --- Dungeon name announcement ---

// Indexed by cur_palace_index_x2 / 2.  The game stores (kPalaceNames_index - 1) * 2,
// so palace/2 maps as: 0=Church, 2=Castle, 3=East, 4=Desert, 5=Agahnim, 7=Water,
// 8=Dark, 9=Mud, 10=Wood, 12=Ice, 13=Tower, 14=Town, 15=Mountain, 16=Agahnim2.
static const char *kDungeonNames[17] = {
  "Sanctuary",           // 0  ← Church
  NULL,                  // 1
  "Hyrule Castle",       // 2  ← Castle
  "Eastern Palace",      // 3  ← East
  "Desert Palace",       // 4  ← Desert
  "Agahnims Tower",      // 5  ← Agahnim
  NULL,                  // 6
  "Swamp Palace",        // 7  ← Water
  "Palace of Darkness",  // 8  ← Dark
  "Misery Mire",         // 9  ← Mud
  "Skull Woods",         // 10 ← Wood
  NULL,                  // 11
  "Ice Palace",          // 12 ← Ice
  "Tower of Hera",       // 13 ← Tower
  "Thieves Town",        // 14 ← Town
  "Ganons Tower",        // 15 ← Mountain
  "Agahnims Tower",      // 16 ← Agahnim2
};

static void AnnounceDungeon(void) {
  bool indoors = player_is_indoors;
  // Read only the low byte — dungeon.c writes BYTE(cur_palace_index_x2), high byte may be stale
  uint8 palace = BYTE(cur_palace_index_x2);

  // Entering a dungeon: was outdoors, now indoors with a valid palace
  if (indoors && !g_prev_indoors) {
    uint8 idx = palace / 2;
    if (idx < 17 && kDungeonNames[idx])
      SpeechSynthesis_Speak(kDungeonNames[idx]);
  }
  // Palace change while indoors (moving between dungeon sections)
  else if (indoors && palace != (uint8)g_prev_palace_index && g_prev_palace_index != 0xFFFF) {
    uint8 idx = palace / 2;
    if (idx < 17 && kDungeonNames[idx])
      SpeechSynthesis_Speak(kDungeonNames[idx]);
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
    Speak("Plus %d rupees, %d total", gained, rupees);
  }

  // Health gained
  if (health > g_prev_health) {
    int hearts = health / 8;
    int half = (health % 8) >= 4;
    int max = link_health_capacity / 8;
    if (half)
      Speak("%d and a half out of %d hearts", hearts, max);
    else
      Speak("%d out of %d hearts", hearts, max);
  }

  // Arrows gained
  if (arrows > g_prev_arrows)
    Speak("%d arrows", arrows);

  // Bombs gained
  if (bombs > g_prev_bombs)
    Speak("%d bombs", bombs);

  // Keys gained
  if (keys > g_prev_keys)
    Speak("Got key, %d keys", keys);

  g_prev_rupees = rupees;
  g_prev_health = health;
  g_prev_arrows = arrows;
  g_prev_bombs = bombs;
  g_prev_keys = keys;
}
#endif  // __APPLE__

void SpatialAudio_ScanFrame(void) {
  SpatialCue cues[kSpatialCue_Count];
  uint32 best_dist2[kSpatialCue_Count];

#ifdef __APPLE__
  AnnounceMenuState();
  AnnounceGameOverChoice();
  AnnounceDialogChoice();
  AnnounceInventory();
  AnnounceDungeon();
  AnnounceCollections();
  AnnounceNameRegistration();
#endif

  if (!g_enabled) return;

  memset(cues, 0, sizeof(cues));
  for (int i = 0; i < kSpatialCue_Count; i++)
    best_dist2[i] = 0xFFFFFFFF;

  uint8 mod = main_module_index;
  if (mod != 7 && mod != 9 && mod != 14) {
    memset(g_cue_snapshot, 0, sizeof(g_cue_snapshot));
    return;
  }

  uint16 lx = link_x_coord;
  uint16 ly = link_y_coord;
  bool indoors = player_is_indoors;

  // Tile scan: 96px radius, 8px step for full diagonal coverage
  int scan_steps = SCAN_RANGE / 8;
  for (int tdy = -scan_steps; tdy <= scan_steps; tdy++) {
    for (int tdx = -scan_steps; tdx <= scan_steps; tdx++) {
      int px = (int)lx + 8 + tdx * 8;
      int py = (int)ly + 12 + tdy * 8;
      if (px < 0 || py < 0) continue;

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
      if (cat < 0) continue;

      int dx = tdx * 8;
      int dy = tdy * 8;

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
  }

  memcpy(g_cue_snapshot, cues, sizeof(g_cue_snapshot));

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

    // Advance ding envelopes
    if (g_ding_timer <= 0) { g_ding_envelope = 65536; g_ding_timer = g_sample_rate; }
    g_ding_timer--;
    g_ding_envelope = (uint32)((uint64)g_ding_envelope * g_ding_decay >> 16);

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

    if (g_hole_timer <= 0) { g_hole_envelope = 65536; g_hole_timer = g_sample_rate / 2; }
    g_hole_timer--;
    g_hole_envelope = (uint32)((uint64)g_hole_envelope * g_hole_decay >> 16);

    for (int c = 0; c < kSpatialCue_Count; c++) {
      if (!g_cue_snapshot[c].active) continue;

      int dx = g_cue_snapshot[c].dx;
      int dy = g_cue_snapshot[c].dy;

      uint32 d2 = (uint32)(dx * dx + dy * dy);
      int dist = (int)isqrt32(d2);
      if (dist >= SCAN_RANGE) continue;
      int volume = (SCAN_RANGE - dist) * 256 / SCAN_RANGE;

      // Per-category envelope
      if (c == kSpatialCue_NPC)
        volume = (int)((uint32)volume * g_ding_envelope >> 16);
      else if (c == kSpatialCue_Enemy) {
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
      }

      int clamped_dx = dx < -64 ? -64 : (dx > 64 ? 64 : dx);
      int gain_L = (64 - clamped_dx) * 2;
      int gain_R = (64 + clamped_dx) * 2;

      int semi = dy / 16;
      if (semi < -4) semi = -4;
      if (semi > 4) semi = 4;
      uint32 adj_inc = (uint32)((uint64)g_phase_inc[c] * g_pitch_mult[semi + 4] >> 15);

      // Stairs: continuous pitch sweep (up=rising, down=falling); others use sine
      int16 raw;
      if (c == kSpatialCue_StairUp || c == kSpatialCue_StairDown) {
        // Sweep multiplier: 1.0x to 2.0x over the sweep period (0.75s)
        // frac goes 0..65535 over the sweep period
        uint32 sweep_period = (uint32)(g_sample_rate * 3 / 4);
        uint32 frac;
        if (c == kSpatialCue_StairUp)
          frac = (g_stair_sweep * 65536) / sweep_period;          // 0→65535 rising
        else
          frac = ((sweep_period - g_stair_sweep) * 65536) / sweep_period;  // 65535→0 falling
        // Multiply phase increment by (1.0 + frac/65536), i.e. sweep 1x to 2x frequency
        uint32 inc = adj_inc + (uint32)((uint64)adj_inc * frac >> 16);
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
      // Pan based on hole's horizontal direction
      int cdx = g_hole_warn_dx < -16 ? -16 : (g_hole_warn_dx > 16 ? 16 : g_hole_warn_dx);
      int wL = (16 - cdx) * 8;  // 0-256 range
      int wR = (16 + cdx) * 8;
      mix_L += scaled * wL >> 8;
      mix_R += scaled * wR >> 8;
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

    buf[s * 2 + 0] += (int16)(mix_L >> 1);
    buf[s * 2 + 1] += (int16)(mix_R >> 1);
  }
}
