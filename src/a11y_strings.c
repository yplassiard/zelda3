#include "a11y_strings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Default English strings indexed by kA11y_* enum ---

static const char * const kDefaultStrings[kA11y_StringCount] = {
  [kA11y_On]                = "Accessibility on",
  [kA11y_Off]               = "Accessibility off",
  [kA11y_InMenu]            = "In menu",
  [kA11y_Entrance]          = "Entrance",
  [kA11y_LegendOpen]        = "Sound Legend. Use Up and Down arrows to browse.",
  [kA11y_LegendClosed]      = "Sound Legend closed",
  [kA11y_OptionsOpen]       = "Accessibility Options. Use Up and Down to navigate.",
  [kA11y_OptionsClosed]     = "Options closed",
  [kA11y_OptionsSaved]      = "Options saved",
  [kA11y_VoiceSubmenuOpen]  = "Voice selection. Use Up and Down, Enter to select, Escape to go back.",
  [kA11y_SoundSetupOpen]    = "Sound Setup. Use Up and Down to navigate, Left and Right to adjust.",
  [kA11y_North]             = "North",
  [kA11y_South]             = "South",
  [kA11y_Left]              = "Left",
  [kA11y_Right]             = "Right",
  [kA11y_north]             = "north",
  [kA11y_south]             = "south",
  [kA11y_left]              = "left",
  [kA11y_right]             = "right",
  [kA11y_Nintendo]          = "Nintendo",
  [kA11y_LegendOfZelda]     = "The Legend of Zelda",
  [kA11y_FileSelect]        = "File Select",
  [kA11y_CopyPlayer]        = "Copy Player",
  [kA11y_ErasePlayer]       = "Erase Player",
  [kA11y_RegisterName]      = "Register Your Name",
  [kA11y_Cancel]            = "Cancel",
  [kA11y_Back]              = "Back",
  [kA11y_Next]              = "Next",
  [kA11y_GroundFloor]       = "Ground Floor",
  [kA11y_FmtPercent]        = "%d percent",
  [kA11y_FmtSelected]       = "%s selected",
  [kA11y_FmtDefault]        = "default",
  [kA11y_FmtHearts]         = "%d out of %d hearts",
  [kA11y_FmtHeartsHalf]     = "%d and a half out of %d hearts",
  [kA11y_FmtPlusRupees]     = "Plus %d rupees, %d total",
  [kA11y_FmtArrows]         = "%d arrows",
  [kA11y_FmtBombs]          = "%d bombs",
  [kA11y_FmtGotKey]         = "Got key, %d keys",
  [kA11y_FmtFloor]          = "Floor %d",
  [kA11y_FmtBasement]       = "Basement %d",
  [kA11y_FmtFile]           = "File %d, %s",
  [kA11y_FmtFileEmpty]      = "File %d, empty",
  [kA11y_FmtOption]         = "Option %d",
  [kA11y_FmtFacing]         = "Facing %s. ",
  [kA11y_FmtEntranceDir]    = "%s to the %s. ",
  [kA11y_FmtEntranceNearby] = "%s nearby. ",
  [kA11y_FmtNPCDir]         = "Someone to the %s. ",
  [kA11y_FmtNothingNearby]  = "Nothing nearby",
  [kA11y_FmtSpeechRate]     = "Speech Rate, %d percent",
  [kA11y_FmtSpeechVoice]    = "Speech Voice, %s",
  [kA11y_FmtSpeechVolume]   = "Speech Volume, %d percent",
  [kA11y_FmtDetectionRange] = "Detection Range, %d",
  [kA11y_FmtCapital]        = "Capital %c",
  [kA11y_Unnamed]           = "unnamed",
  [kA11y_Flute]             = "Flute",
  [kA11y_LightWorldMap]     = "Light World map.",
  [kA11y_DarkWorldMap]      = "Dark World map.",
  [kA11y_DungeonMap]        = "Dungeon map.",
  [kA11y_FmtMarkersRemaining] = "%d locations marked.",
  [kA11y_FmtMarkerNav]      = "%s. %d of %d.",
  [kA11y_NoMarkers]         = "No markers.",
};

// --- Default indexed arrays ---

#define MAX_AREAS 160
#define MAX_ENTRANCES 99
#define MAX_ITEMS 20
#define MAX_DUNGEONS 17
#define MAX_LEGEND 24
#define MAX_OPTIONS 5
#define MAX_SOUND_SETUP 16
#define MAX_FLUTE 8

static const char * const kDefaultAreaNames[MAX_AREAS] = {
  [0] = "Lost Woods", [1] = "Lost Woods",
  [2] = "Lumberjack Estate",
  [3] = "Tower of Hera", [4] = "Tower of Hera",
  [5] = "Death Mountain", [6] = "Death Mountain",
  [7] = "Turtle Rock",
  [8] = "Lost Woods", [9] = "Lost Woods",
  [10] = "Death Mountain Gateway",
  [11] = "Tower of Hera", [12] = "Tower of Hera",
  [13] = "Death Mountain", [14] = "Death Mountain",
  [15] = "Zora Falls",
  [16] = "Lost Woods Outskirts",
  [17] = "Kakariko Village",
  [18] = "Northern Pond",
  [19] = "Sanctuary",
  [20] = "Graveyard",
  [21] = "South Bend",
  [22] = "Coven of Commerce",
  [23] = "Zora Ridge",
  [24] = "Kakariko Village", [25] = "Kakariko Village",
  [26] = "West Woods",
  [27] = "Hyrule Castle", [28] = "Hyrule Castle",
  [29] = "Castle Bridge",
  [30] = "Eastern Ruins", [31] = "Eastern Ruins",
  [32] = "Kakariko Village", [33] = "Kakariko Village",
  [34] = "Smithy Estate",
  [35] = "Hyrule Castle", [36] = "Hyrule Castle",
  [37] = "Moundlands",
  [38] = "Eastern Ruins", [39] = "Eastern Ruins",
  [40] = "Kakariko Village", [41] = "Kakariko Village",
  [42] = "Haunted Grove",
  [43] = "Link's House", [44] = "Link's House",
  [45] = "Eastern Ruins", [46] = "Eastern Ruins", [47] = "Eastern Ruins",
  [48] = "Desert of Mystery", [49] = "Desert of Mystery",
  [50] = "Haunted Terrace",
  [51] = "Hyrule Wetlands", [52] = "Hyrule Wetlands",
  [53] = "Lake Hylia", [54] = "Lake Hylia",
  [55] = "Frosty Caves",
  [56] = "Desert of Mystery", [57] = "Desert of Mystery",
  [58] = "Via of Mystery",
  [59] = "Watergate",
  [60] = "Hyrule Wetlands",
  [61] = "Lake Hylia", [62] = "Lake Hylia",
  [63] = "Octorock Grounds",
  [64] = "Skull Woods", [65] = "Skull Woods",
  [66] = "Eastern Skull Clearing",
  [67] = "Ganon's Tower", [68] = "Ganon's Tower",
  [69] = "Dark Death Mountain", [70] = "Dark Death Mountain",
  [71] = "Dark Turtle Rock",
  [72] = "Skull Woods", [73] = "Skull Woods",
  [75] = "Ganon's Tower", [76] = "Ganon's Tower",
  [77] = "Dark Death Mountain", [78] = "Dark Death Mountain",
  [79] = "Falls of Ill Omen",
  [80] = "Skull Woods",
  [81] = "Village of Outcasts",
  [82] = "Dark Northern Pond",
  [83] = "Dark Sanctuary",
  [84] = "Garden of Bad Things",
  [85] = "South Bend",
  [86] = "Riverside Commerce",
  [87] = "Zora Ridge",
  [88] = "Village of Outcasts", [89] = "Village of Outcasts",
  [90] = "West Woods",
  [91] = "Pyramid of Power", [92] = "Pyramid of Power",
  [94] = "Maze of Darkness", [95] = "Maze of Darkness",
  [96] = "Village of Outcasts", [97] = "Village of Outcasts",
  [98] = "Gossip Shop",
  [99] = "Pyramid of Power", [100] = "Pyramid of Power",
  [101] = "Moundlands",
  [102] = "Maze of Darkness", [103] = "Maze of Darkness",
  [104] = "Digging Game",
  [105] = "Archery Shop",
  [106] = "Depressing Grove",
  [107] = "Bomb Shop", [108] = "Bomb Shop",
  [109] = "Hammer Bridge",
  [112] = "Swamp of Evil", [113] = "Swamp of Evil",
  [114] = "Haunted Terrace",
  [115] = "Wilted Wetlands", [116] = "Wilted Wetlands",
  [117] = "Lake of Ill Omen", [118] = "Lake of Ill Omen",
  [120] = "Swamp of Evil", [121] = "Swamp of Evil",
  [123] = "Swamp Palace",
  [124] = "Wilted Wetlands",
  [125] = "Lake of Ill Omen", [126] = "Lake of Ill Omen",
  [128] = "Master Sword Grove",
  [129] = "Zora Falls", [130] = "Zora Falls",
  [137] = "Zora Falls", [138] = "Zora Falls",
};

static const char * const kDefaultEntranceNames[MAX_ENTRANCES] = {
  "Link's House",      // 0
  "Link's House",      // 1
  "Sanctuary",         // 2
  "Hyrule Castle",     // 3
  "Hyrule Castle",     // 4
  "Hyrule Castle",     // 5
  "Cave",              // 6
  "Cave",              // 7
  "Eastern Palace",    // 8
  "Desert Palace",     // 9
  "Desert Palace",     // 10
  "Desert Palace",     // 11
  "Desert Palace",     // 12
  "Elder's House",     // 13
  "Elder's House",     // 14
  "Angry Brothers",    // 15
  "Angry Brothers",    // 16
  "Mad Batter",        // 17
  "Lumberjack Tree",   // 18
  "Cave",              // 19
  "Cave",              // 20
  "Cave",              // 21
  "Cave",              // 22
  "Cave",              // 23
  "Cave",              // 24
  "Cave",              // 25
  "Cave",              // 26
  "Cave",              // 27
  "Cave",              // 28
  "Cave",              // 29
  "Cave",              // 30
  "Cave",              // 31
  "Cave",              // 32
  "Castle Tower",      // 33
  "Swamp Palace",      // 34
  "Palace of Darkness",// 35
  "Misery Mire",       // 36
  "Skull Woods",       // 37
  "Skull Woods",       // 38
  "Skull Woods",       // 39
  "Skull Woods",       // 40
  "Thieves' Lair",     // 41
  "Ice Palace",        // 42
  "Cave",              // 43
  "Cave",              // 44
  "Elder Cave",        // 45
  "Elder Cave",        // 46
  "Castle Cellar",     // 47
  "Tower of Hera",     // 48
  "Thieves' Town",     // 49
  "Turtle Rock",       // 50
  "Pyramid Sanctum",   // 51
  "Ganon's Tower",     // 52
  "Fairy Cave",        // 53
  "Well",              // 54
  "Cave",              // 55
  "Cave",              // 56
  "Treasure Game",     // 57
  "Cave",              // 58
  "House",             // 59
  "House",             // 60
  "Sick Boy's House",  // 61
  "Cave",              // 62
  "Pub",               // 63
  "Pub",               // 64
  "Inn",               // 65
  "Sahasrahla",        // 66
  "Shop",              // 67
  "Chest Game",        // 68
  "Orphanage",         // 69
  "Library",           // 70
  "Storage Shed",      // 71
  "House",             // 72
  "Potion Shop",       // 73
  "Desert Cottage",    // 74
  "Watergate",         // 75
  "Cave",              // 76
  "Fairy Cave",        // 77
  "Cave",              // 78
  "Cave",              // 79
  "Shop",              // 80
  "Cave",              // 81
  "Archery Game",      // 82
  "Cave",              // 83
  "Cape Cave",         // 84
  "Wishing Pond",      // 85
  "Pond of Happiness", // 86
  "Fairy Cave",        // 87
  "Cave",              // 88
  "Shop",              // 89
  "Hideout",           // 90
  "Cave",              // 91
  "Warped Pond",       // 92
  "Smithy",            // 93
  "Fortune Teller",    // 94
  "Fortune Teller",    // 95
  "Chest Game",        // 96
  "Cave",              // 97
  "Cave",              // 98
};

static const char * const kDefaultItemNames[MAX_ITEMS] = {
  "Bow",             // 0
  "Boomerang",       // 1
  "Hookshot",        // 2
  "Bombs",           // 3
  "Mushroom",        // 4
  "Fire Rod",        // 5
  "Ice Rod",         // 6
  "Bombos",          // 7
  "Ether",           // 8
  "Quake",           // 9
  "Lamp",            // 10
  "Hammer",          // 11
  "Shovel",          // 12
  "Bug Net",         // 13
  "Book of Mudora",  // 14
  "Bottle",          // 15
  "Cane of Somaria", // 16
  "Cane of Byrna",   // 17
  "Magic Cape",      // 18
  "Magic Mirror",    // 19
};

static const char * const kDefaultDungeonNames[MAX_DUNGEONS] = {
  "Sanctuary",           // 0
  NULL,                  // 1
  "Hyrule Castle",       // 2
  "Eastern Palace",      // 3
  "Desert Palace",       // 4
  "Agahnim's Tower",     // 5
  NULL,                  // 6
  "Swamp Palace",        // 7
  "Palace of Darkness",  // 8
  "Misery Mire",         // 9
  "Skull Woods",         // 10
  "Turtle Rock",         // 11
  "Ice Palace",          // 12
  "Tower of Hera",       // 13
  "Thieves' Town",       // 14
  "Ganon's Tower",       // 15
  "Agahnim's Tower",     // 16
};

static const char * const kDefaultLegendNames[MAX_LEGEND] = {
  "Wall ahead",
  "Wall behind",
  "Wall right",
  "Wall left",
  "Passage opening",
  "Hole or pit",
  "Ledge, one way drop",
  "Deep water",
  "Hazard, spikes",
  "Conveyor belt",
  "Enemy",
  "Friendly character",
  "Chest",
  "Liftable object",
  "Door or entrance",
  "Stairs going up",
  "Stairs going down",
  "Grass terrain",
  "Shallow water terrain",
  "Ice floor",
  "Sword range",
  "Danger zone",
  "Alignment sonar",
  "Movement blocked",
};

static const char * const kDefaultOptionsNames[MAX_OPTIONS] = {
  "Speech Rate",
  "Speech Voice",
  "Speech Volume",
  "Sound Setup",
  "Save Options",
};

static const char * const kDefaultSoundSetupNames[MAX_SOUND_SETUP] = {
  "Walls volume",
  "Holes and Pits volume",
  "Enemy volume",
  "NPC volume",
  "Chest volume",
  "Liftable volume",
  "Door volume",
  "Stairs volume",
  "Ledge volume",
  "Deep Water volume",
  "Hazard volume",
  "Conveyor volume",
  "Terrain volume",
  "Combat volume",
  "Detection Range",
  "Back",
};

static const char * const kDefaultFluteNames[MAX_FLUTE] = {
  "Death Mountain",      // 0
  "Witch's Hut",         // 1
  "Kakariko Village",    // 2
  "Link's House",        // 3
  "South Shore",         // 4
  "Desert of Mystery",   // 5
  "Lake Hylia",          // 6
  "East Hyrule",         // 7
};

// --- Override storage (loaded from INI) ---

static const char *g_strings[kA11y_StringCount];
static const char *g_area_names[MAX_AREAS];
static const char *g_entrance_names[MAX_ENTRANCES];
static const char *g_item_names[MAX_ITEMS];
static const char *g_dungeon_names[MAX_DUNGEONS];
static const char *g_legend_names[MAX_LEGEND];
static const char *g_options_names[MAX_OPTIONS];
static const char *g_sound_setup_names[MAX_SOUND_SETUP];
static const char *g_flute_names[MAX_FLUTE];

// Arena allocator for INI strings (no individual frees needed)
#define INI_BUFFER_SIZE (32 * 1024)
static char g_ini_buf[INI_BUFFER_SIZE];
static int g_ini_buf_pos;

static char *IniStrdup(const char *s) {
  int len = (int)strlen(s) + 1;
  if (g_ini_buf_pos + len > INI_BUFFER_SIZE) return NULL;
  char *p = g_ini_buf + g_ini_buf_pos;
  memcpy(p, s, len);
  g_ini_buf_pos += len;
  return p;
}

// --- INI key-to-ID mapping ---

typedef struct {
  const char *key;
  int id;
} KeyId;

static const KeyId kSystemKeys[] = {
  {"on", kA11y_On}, {"off", kA11y_Off}, {"in_menu", kA11y_InMenu},
  {"entrance", kA11y_Entrance},
  {"legend_open", kA11y_LegendOpen}, {"legend_closed", kA11y_LegendClosed},
  {"options_open", kA11y_OptionsOpen}, {"options_closed", kA11y_OptionsClosed},
  {"options_saved", kA11y_OptionsSaved},
  {"voice_submenu", kA11y_VoiceSubmenuOpen}, {"sound_setup_open", kA11y_SoundSetupOpen},
  {"back", kA11y_Back}, {"next", kA11y_Next},
  {"unnamed", kA11y_Unnamed}, {"flute", kA11y_Flute},
  {"light_world_map", kA11y_LightWorldMap}, {"dark_world_map", kA11y_DarkWorldMap},
  {"dungeon_map", kA11y_DungeonMap}, {"no_markers", kA11y_NoMarkers},
  {NULL, 0}
};

static const KeyId kDirectionKeys[] = {
  {"north", kA11y_North}, {"south", kA11y_South},
  {"left", kA11y_Left}, {"right", kA11y_Right},
  {"north_lc", kA11y_north}, {"south_lc", kA11y_south},
  {"left_lc", kA11y_left}, {"right_lc", kA11y_right},
  {NULL, 0}
};

static const KeyId kTitleKeys[] = {
  {"nintendo", kA11y_Nintendo}, {"legend_of_zelda", kA11y_LegendOfZelda},
  {"file_select", kA11y_FileSelect}, {"copy_player", kA11y_CopyPlayer},
  {"erase_player", kA11y_ErasePlayer}, {"register_name", kA11y_RegisterName},
  {"cancel", kA11y_Cancel}, {"ground_floor", kA11y_GroundFloor},
  {NULL, 0}
};

static const KeyId kFormatKeys[] = {
  {"hearts", kA11y_FmtHearts}, {"hearts_half", kA11y_FmtHeartsHalf},
  {"plus_rupees", kA11y_FmtPlusRupees}, {"arrows", kA11y_FmtArrows},
  {"bombs", kA11y_FmtBombs}, {"got_key", kA11y_FmtGotKey},
  {"floor", kA11y_FmtFloor}, {"basement", kA11y_FmtBasement},
  {"file", kA11y_FmtFile}, {"file_empty", kA11y_FmtFileEmpty},
  {"option", kA11y_FmtOption}, {"facing", kA11y_FmtFacing},
  {"entrance_dir", kA11y_FmtEntranceDir}, {"entrance_nearby", kA11y_FmtEntranceNearby},
  {"npc_dir", kA11y_FmtNPCDir}, {"nothing_nearby", kA11y_FmtNothingNearby},
  {"percent", kA11y_FmtPercent}, {"selected", kA11y_FmtSelected},
  {"default", kA11y_FmtDefault},
  {"speech_rate", kA11y_FmtSpeechRate}, {"speech_voice", kA11y_FmtSpeechVoice},
  {"speech_volume", kA11y_FmtSpeechVolume}, {"detection_range", kA11y_FmtDetectionRange},
  {"capital", kA11y_FmtCapital},
  {"markers_remaining", kA11y_FmtMarkersRemaining}, {"marker_nav", kA11y_FmtMarkerNav},
  {NULL, 0}
};

// Look up a key in a KeyId table, return -1 if not found
static int LookupKey(const KeyId *table, const char *key) {
  for (int i = 0; table[i].key; i++) {
    if (strcmp(table[i].key, key) == 0)
      return table[i].id;
  }
  return -1;
}

// --- INI parser ---

enum {
  kSection_None,
  kSection_System,
  kSection_Directions,
  kSection_Title,
  kSection_Format,
  kSection_Items,
  kSection_Dungeons,
  kSection_Areas,
  kSection_Entrances,
  kSection_Legend,
  kSection_Options,
  kSection_SoundSetup,
  kSection_Flute,
};

static int ParseSectionName(const char *name) {
  if (strcmp(name, "system") == 0) return kSection_System;
  if (strcmp(name, "directions") == 0) return kSection_Directions;
  if (strcmp(name, "title") == 0) return kSection_Title;
  if (strcmp(name, "format") == 0) return kSection_Format;
  if (strcmp(name, "items") == 0) return kSection_Items;
  if (strcmp(name, "dungeons") == 0) return kSection_Dungeons;
  if (strcmp(name, "areas") == 0) return kSection_Areas;
  if (strcmp(name, "entrances") == 0) return kSection_Entrances;
  if (strcmp(name, "legend") == 0) return kSection_Legend;
  if (strcmp(name, "options") == 0) return kSection_Options;
  if (strcmp(name, "sound_setup") == 0) return kSection_SoundSetup;
  if (strcmp(name, "flute") == 0) return kSection_Flute;
  return kSection_None;
}

static void SetIndexedString(int section, int idx, const char *value) {
  char *s = IniStrdup(value);
  if (!s) return;
  switch (section) {
  case kSection_Items:      if (idx >= 0 && idx < MAX_ITEMS)       g_item_names[idx] = s; break;
  case kSection_Dungeons:   if (idx >= 0 && idx < MAX_DUNGEONS)    g_dungeon_names[idx] = s; break;
  case kSection_Areas:      if (idx >= 0 && idx < MAX_AREAS)       g_area_names[idx] = s; break;
  case kSection_Entrances:  if (idx >= 0 && idx < MAX_ENTRANCES)   g_entrance_names[idx] = s; break;
  case kSection_Legend:     if (idx >= 0 && idx < MAX_LEGEND)      g_legend_names[idx] = s; break;
  case kSection_Options:    if (idx >= 0 && idx < MAX_OPTIONS)     g_options_names[idx] = s; break;
  case kSection_SoundSetup: if (idx >= 0 && idx < MAX_SOUND_SETUP) g_sound_setup_names[idx] = s; break;
  case kSection_Flute:      if (idx >= 0 && idx < MAX_FLUTE)       g_flute_names[idx] = s; break;
  }
}

static void LoadIniFile(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) return;

  char line[512];
  int section = kSection_None;

  while (fgets(line, sizeof(line), f)) {
    // Strip newline
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    nl = strchr(line, '\r');
    if (nl) *nl = '\0';

    // Skip empty lines and comments
    if (line[0] == '\0' || line[0] == '#' || line[0] == ';')
      continue;

    // Section header
    if (line[0] == '[') {
      char *end = strchr(line, ']');
      if (end) {
        *end = '\0';
        section = ParseSectionName(line + 1);
      }
      continue;
    }

    // Key=value
    char *eq = strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';
    const char *key = line;
    const char *value = eq + 1;

    // Keyed sections → enum ID lookup
    const KeyId *table = NULL;
    switch (section) {
    case kSection_System:     table = kSystemKeys; break;
    case kSection_Directions: table = kDirectionKeys; break;
    case kSection_Title:      table = kTitleKeys; break;
    case kSection_Format:     table = kFormatKeys; break;
    default: break;
    }

    if (table) {
      int id = LookupKey(table, key);
      if (id >= 0) {
        char *s = IniStrdup(value);
        if (s) g_strings[id] = s;
      }
    } else if (section >= kSection_Items) {
      // Indexed sections
      int idx = atoi(key);
      SetIndexedString(section, idx, value);
    }
  }
  fclose(f);
}

// --- Public API ---

void A11yStrings_Init(const char *language) {
  memset(g_strings, 0, sizeof(g_strings));
  memset(g_area_names, 0, sizeof(g_area_names));
  memset(g_entrance_names, 0, sizeof(g_entrance_names));
  memset(g_item_names, 0, sizeof(g_item_names));
  memset(g_dungeon_names, 0, sizeof(g_dungeon_names));
  memset(g_legend_names, 0, sizeof(g_legend_names));
  memset(g_options_names, 0, sizeof(g_options_names));
  memset(g_sound_setup_names, 0, sizeof(g_sound_setup_names));
  memset(g_flute_names, 0, sizeof(g_flute_names));
  g_ini_buf_pos = 0;

  // Map game language codes to INI language codes
  const char *lang = "en";
  if (language && language[0]) {
    if (strcmp(language, "us") == 0)
      lang = "en";
    else
      lang = language;
  }

  // Try language-specific file, fall back to English
  char path[256];
  snprintf(path, sizeof(path), "a11y_%s.ini", lang);
  LoadIniFile(path);
  if (strcmp(lang, "en") != 0) {
    // Also load English as base (language-specific overrides take precedence
    // because they were loaded first into non-NULL slots; but for missing keys
    // we want English). Actually, we should load English first, then overlay.
    // Reset and reload in the right order.
    memset(g_strings, 0, sizeof(g_strings));
    memset(g_area_names, 0, sizeof(g_area_names));
    memset(g_entrance_names, 0, sizeof(g_entrance_names));
    memset(g_item_names, 0, sizeof(g_item_names));
    memset(g_dungeon_names, 0, sizeof(g_dungeon_names));
    memset(g_legend_names, 0, sizeof(g_legend_names));
    memset(g_options_names, 0, sizeof(g_options_names));
    memset(g_sound_setup_names, 0, sizeof(g_sound_setup_names));
    memset(g_flute_names, 0, sizeof(g_flute_names));
    g_ini_buf_pos = 0;
    LoadIniFile("a11y_en.ini");
    LoadIniFile(path);
  }
}

const char *A11y(int id) {
  if (id < 0 || id >= kA11y_StringCount) return "";
  if (g_strings[id]) return g_strings[id];
  return kDefaultStrings[id] ? kDefaultStrings[id] : "";
}

const char *A11yAreaName(int area_index) {
  if (area_index < 0 || area_index >= MAX_AREAS) return NULL;
  if (g_area_names[area_index]) return g_area_names[area_index];
  return kDefaultAreaNames[area_index];
}

const char *A11yEntranceName(int entrance_id) {
  if (entrance_id < 0 || entrance_id >= MAX_ENTRANCES)
    return A11y(kA11y_Entrance);
  const char *name = g_entrance_names[entrance_id];
  if (name) return name;
  return kDefaultEntranceNames[entrance_id] ? kDefaultEntranceNames[entrance_id] : A11y(kA11y_Entrance);
}

const char *A11yItemName(int y_item_index) {
  if (y_item_index < 0 || y_item_index >= MAX_ITEMS) return NULL;
  if (g_item_names[y_item_index]) return g_item_names[y_item_index];
  return kDefaultItemNames[y_item_index];
}

const char *A11yDungeonName(int palace_div2) {
  if (palace_div2 < 0 || palace_div2 >= MAX_DUNGEONS) return NULL;
  if (g_dungeon_names[palace_div2]) return g_dungeon_names[palace_div2];
  return kDefaultDungeonNames[palace_div2];
}

const char *A11yLegendName(int legend_index) {
  if (legend_index < 0 || legend_index >= MAX_LEGEND) return NULL;
  if (g_legend_names[legend_index]) return g_legend_names[legend_index];
  return kDefaultLegendNames[legend_index];
}

const char *A11yTopMenuName(int index) {
  if (index < 0 || index >= MAX_OPTIONS) return NULL;
  if (g_options_names[index]) return g_options_names[index];
  return kDefaultOptionsNames[index];
}

const char *A11ySoundSetupName(int index) {
  if (index < 0 || index >= MAX_SOUND_SETUP) return NULL;
  if (g_sound_setup_names[index]) return g_sound_setup_names[index];
  return kDefaultSoundSetupNames[index];
}

const char *A11yFluteName(int index) {
  if (index < 0 || index >= MAX_FLUTE) return NULL;
  if (g_flute_names[index]) return g_flute_names[index];
  return kDefaultFluteNames[index];
}
