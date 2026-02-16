#ifndef A11Y_STRINGS_H
#define A11Y_STRINGS_H

// Localization system for accessibility strings.
// Call A11yStrings_Init() once at startup with the game language code.
// Compiled-in English defaults are used if no INI file is found.

void A11yStrings_Init(const char *language);

// Lookup a UI/system string by ID (never returns NULL)
const char *A11y(int id);

// Category-specific lookups by runtime index (return NULL if out of range or unset)
const char *A11yAreaName(int area_index);        // overworld area (0-159)
const char *A11yEntranceName(int entrance_id);   // entrance (0-98)
const char *A11yItemName(int y_item_index);      // Y-item index (0-19, link_item_bow offset)
const char *A11yDungeonName(int palace_div2);    // palace index / 2 (0-16)
const char *A11yLegendName(int legend_index);    // sound legend (0-23)
const char *A11yTopMenuName(int index);          // options top menu (0-4)
const char *A11ySoundSetupName(int index);       // sound setup submenu (0-15)
const char *A11yFluteName(int index);            // flute destination (0-7)

// String IDs for A11y()
enum {
  // System
  kA11y_On,
  kA11y_Off,
  kA11y_InMenu,
  kA11y_Entrance,

  // Sound Legend
  kA11y_LegendOpen,
  kA11y_LegendClosed,

  // Options menu
  kA11y_OptionsOpen,
  kA11y_OptionsClosed,
  kA11y_OptionsSaved,
  kA11y_VoiceSubmenuOpen,
  kA11y_SoundSetupOpen,

  // Directions (capitalized for standalone use)
  kA11y_North,
  kA11y_South,
  kA11y_Left,
  kA11y_Right,

  // Lowercase directions (for "to the north" etc.)
  kA11y_north,
  kA11y_south,
  kA11y_left,
  kA11y_right,

  // Title/menu screens
  kA11y_Nintendo,
  kA11y_LegendOfZelda,
  kA11y_FileSelect,
  kA11y_CopyPlayer,
  kA11y_ErasePlayer,
  kA11y_RegisterName,
  kA11y_Cancel,

  // Name registration
  kA11y_Back,
  kA11y_Next,

  // Floor
  kA11y_GroundFloor,

  // Format strings
  kA11y_FmtPercent,           // "%d percent"
  kA11y_FmtSelected,          // "%s selected"
  kA11y_FmtDefault,           // "default"
  kA11y_FmtHearts,            // "%d out of %d hearts"
  kA11y_FmtHeartsHalf,        // "%d and a half out of %d hearts"
  kA11y_FmtPlusRupees,        // "Plus %d rupees, %d total"
  kA11y_FmtArrows,            // "%d arrows"
  kA11y_FmtBombs,             // "%d bombs"
  kA11y_FmtGotKey,            // "Got key, %d keys"
  kA11y_FmtFloor,             // "Floor %d"
  kA11y_FmtBasement,          // "Basement %d"
  kA11y_FmtFile,              // "File %d, %s"
  kA11y_FmtFileEmpty,         // "File %d, empty"
  kA11y_FmtOption,            // "Option %d"
  kA11y_FmtFacing,            // "Facing %s. "
  kA11y_FmtEntranceDir,       // "%s to the %s. "
  kA11y_FmtEntranceNearby,    // "%s nearby. "
  kA11y_FmtNPCDir,            // "Someone to the %s. "
  kA11y_FmtNothingNearby,     // "Nothing nearby"
  kA11y_FmtSpeechRate,        // "Speech Rate, %d percent"
  kA11y_FmtSpeechVoice,       // "Speech Voice, %s"
  kA11y_FmtSpeechVolume,      // "Speech Volume, %d percent"
  kA11y_FmtDetectionRange,    // "Detection Range, %d"
  kA11y_FmtCapital,           // "Capital %c"
  kA11y_Unnamed,              // "unnamed"
  kA11y_Flute,                // "Flute"

  // Map / dungeon map
  kA11y_LightWorldMap,        // "Light World map."
  kA11y_DarkWorldMap,         // "Dark World map."
  kA11y_DungeonMap,           // "Dungeon map."
  kA11y_FmtMarkersRemaining,  // "%d locations marked."
  kA11y_FmtMarkerNav,         // "%s. %d of %d."
  kA11y_NoMarkers,            // "No markers."

  kA11y_StringCount,
};

#endif  // A11Y_STRINGS_H
