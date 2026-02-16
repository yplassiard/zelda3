#include "accessibility.h"
#include "a11y_strings.h"
#include "spatial_audio.h"
#include "types.h"
#include "variables.h"
#include "zelda_rtl.h"
#include <string.h>

#if defined(__APPLE__)
#include "platform/macos/speechsynthesis.h"
#elif defined(_WIN32)
#include "platform/win32/speechsynthesis.h"
#endif

// Character lookup table matching kTextAlphabet_US from text_compression.py.
// Index 0x00-0x66 maps to printable characters. 0 means skip (glyph/icon).
static const char kAccessibility_CharTable_US[0x67] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',  // 0x00-0x0F
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',  // 0x10-0x1F
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',  // 0x20-0x2F
  'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '!', '?',  // 0x30-0x3F
  '-', '.', ',',  '.', '>', '(', ')',                                                   // 0x40-0x46
  0,   0,   0,   0,   0,                                                                // 0x47-0x4B: Ankh,Waves,Snake,LinkL,LinkR
  '"',                                                                                   // 0x4C
  0,   0,   0,                                                                           // 0x4D-0x4F: Up,Down,Left
  0,                                                                                     // 0x50: Right
  '\'',                                                                                  // 0x51
  0,   0,   0,   0,   0,   0,   0,                                                      // 0x52-0x58: heart glyphs
  ' ',                                                                                   // 0x59
  '<',                                                                                   // 0x5A
  0,   0,   0,   0,                                                                      // 0x5B-0x5E: A,B,X,Y button glyphs
  0,   0,   0,   0,   0,   0,   0,   0,                                                 // 0x5F-0x66: padding to 0x67
};

// Command extra-byte lengths for commands 0x67+, indexed by (byte - 0x67).
// Matches kText_CommandLengths_US in messaging.c.
// 0 = single-byte command, 1 = two-byte command (skip next byte too).
static const uint8 kAccessibility_CmdLengths_US[25] = {
  0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

// Command indices (byte - 0x67) for US encoding
enum {
  kAccessibility_US_CmdScroll = 12,    // 0x73
  kAccessibility_US_CmdLine1 = 13,
  kAccessibility_US_CmdLine2 = 14,
  kAccessibility_US_CmdLine3 = 15,
  kAccessibility_US_CmdWaitkey = 23,   // 0x7E
  kAccessibility_US_CmdEndMsg = 24,    // 0x7F
};

// EU (PAL) encoding: characters are 0x00-0x7E, commands start at 0x7F.
// Matching kTextAlphabet_FR / kTextAlphabet_DE from text_compression.py.
// We use a 2-byte UTF-8 encoding for accented characters.
// Index 0x00-0x7E maps to printable characters (or multi-byte UTF-8).
typedef struct {
  char c[3];  // up to 2-byte UTF-8 + null
} EuChar;

static const EuChar kAccessibility_CharTable_EU[0x70] = {
  {"A"}, {"B"}, {"C"}, {"D"}, {"E"}, {"F"}, {"G"}, {"H"}, {"I"}, {"J"}, {"K"}, {"L"}, {"M"}, {"N"}, {"O"}, {"P"},  // 0x00-0x0F
  {"Q"}, {"R"}, {"S"}, {"T"}, {"U"}, {"V"}, {"W"}, {"X"}, {"Y"}, {"Z"}, {"a"}, {"b"}, {"c"}, {"d"}, {"e"}, {"f"},  // 0x10-0x1F
  {"g"}, {"h"}, {"i"}, {"j"}, {"k"}, {"l"}, {"m"}, {"n"}, {"o"}, {"p"}, {"q"}, {"r"}, {"s"}, {"t"}, {"u"}, {"v"},  // 0x20-0x2F
  {"w"}, {"x"}, {"y"}, {"z"}, {"0"}, {"1"}, {"2"}, {"3"}, {"4"}, {"5"}, {"6"}, {"7"}, {"8"}, {"9"}, {"!"}, {"?"},  // 0x30-0x3F
  {"-"}, {"."}, {","}, {"."}, {">"}, {"("}, {")"},                                                                   // 0x40-0x46
  {""}, {""}, {""}, {""}, {""},                                                                                       // 0x47-0x4B: icons
  {"\""},{""}, {""}, {""},                                                                                            // 0x4C-0x4F
  {""}, {"'"},                                                                                                        // 0x50-0x51
  {""}, {""}, {""}, {""}, {""}, {""}, {""},                                                                           // 0x52-0x58: hearts
  {" "},                                                                                                              // 0x59
  {"\xC3\xB6"}, // 0x5A: o-umlaut
  {""}, {""}, {""}, {""}, // 0x5B-0x5E: A,B,X,Y
  {"\xC3\xBC"}, // 0x5F: u-umlaut
  {"\xC3\xB4"}, // 0x60: o-circumflex
  {":"},         // 0x61
  {""}, {""},    // 0x62-0x63: arrow icons
  {""}, {""},    // 0x64-0x65: arrow icons
  {"\xC3\xA8"}, // 0x66: e-grave
  {"\xC3\xA9"}, // 0x67: e-acute
  {"\xC3\xAA"}, // 0x68: e-circumflex
  {"\xC3\xA0"}, // 0x69: a-grave
  {"\xC3\xB9"}, // 0x6A: u-grave
  {"\xC3\xA7"}, // 0x6B: c-cedilla
  {"\xC3\xA2"}, // 0x6C: a-circumflex
  {"\xC3\xBB"}, // 0x6D: u-circumflex
  {"\xC3\xAE"}, // 0x6E: i-circumflex
  {"\xC3\xA4"}, // 0x6F: a-umlaut
};

// EU command bytes:
// 0x7F = EndMessage
// 0x80 = Scroll
// 0x81 = Waitkey
// 0x82-0x86 = simple commands (line1, line2, line3, Name, unused)
// 0x87 = multi-byte command prefix (next byte encodes the specific command)

#define MAX_CHUNKS 32
#define MAX_CHUNK_LEN 256

static char g_chunks[MAX_CHUNKS][MAX_CHUNK_LEN];
static int g_chunk_count;
static int g_chunk_current;

// Parse using US encoding (NTSC)
static void ParseChunks_US(void) {
  const uint8 *buf = messaging_text_buffer;
  int pos = 0;

  for (int i = 0; i < 1024; i++) {
    uint8 c = buf[i];

    if (c < 0x67) {
      char ch = kAccessibility_CharTable_US[c];
      if (ch && pos < MAX_CHUNK_LEN - 2)
        g_chunks[g_chunk_count][pos++] = ch;
    } else {
      int cmd_idx = c - 0x67;
      if (cmd_idx >= 25) break;

      if (cmd_idx == kAccessibility_US_CmdScroll ||
          cmd_idx == kAccessibility_US_CmdLine1 ||
          cmd_idx == kAccessibility_US_CmdLine2 ||
          cmd_idx == kAccessibility_US_CmdLine3) {
        if (pos < MAX_CHUNK_LEN - 2)
          g_chunks[g_chunk_count][pos++] = ' ';
      }

      if (cmd_idx == kAccessibility_US_CmdWaitkey ||
          cmd_idx == kAccessibility_US_CmdEndMsg) {
        g_chunks[g_chunk_count][pos] = '\0';
        if (pos > 0 && g_chunk_count < MAX_CHUNKS - 1)
          g_chunk_count++;
        pos = 0;
        if (cmd_idx == kAccessibility_US_CmdEndMsg) break;
      }

      i += kAccessibility_CmdLengths_US[cmd_idx];
    }
  }

  if (pos > 0 && g_chunk_count < MAX_CHUNKS) {
    g_chunks[g_chunk_count][pos] = '\0';
    g_chunk_count++;
  }
}

// Parse using EU encoding (PAL — French, German, Spanish, etc.)
static void ParseChunks_EU(void) {
  const uint8 *buf = messaging_text_buffer;
  int pos = 0;

  for (int i = 0; i < 1024; i++) {
    uint8 c = buf[i];

    if (c < 0x7F) {
      // Character byte — look up in EU table
      if (c < 0x70) {
        const char *s = kAccessibility_CharTable_EU[c].c;
        while (*s && pos < MAX_CHUNK_LEN - 2)
          g_chunks[g_chunk_count][pos++] = *s++;
      }
      // 0x70-0x7E are unused character slots, skip
    } else if (c == 0x7F) {
      // EndMessage
      g_chunks[g_chunk_count][pos] = '\0';
      if (pos > 0 && g_chunk_count < MAX_CHUNKS - 1)
        g_chunk_count++;
      break;
    } else if (c == 0x80) {
      // Scroll — insert space, not a chunk boundary
      if (pos < MAX_CHUNK_LEN - 2)
        g_chunks[g_chunk_count][pos++] = ' ';
    } else if (c == 0x81) {
      // Waitkey — end current chunk
      g_chunks[g_chunk_count][pos] = '\0';
      if (pos > 0 && g_chunk_count < MAX_CHUNKS - 1)
        g_chunk_count++;
      pos = 0;
    } else if (c >= 0x82 && c <= 0x86) {
      // Simple commands (line breaks, Name) — insert space
      if (pos < MAX_CHUNK_LEN - 2)
        g_chunks[g_chunk_count][pos++] = ' ';
    } else if (c == 0x87) {
      // Multi-byte command prefix — skip next byte
      i++;
    }
  }

  if (pos > 0 && g_chunk_count < MAX_CHUNKS) {
    g_chunks[g_chunk_count][pos] = '\0';
    g_chunk_count++;
  }
}

static void Accessibility_ParseChunks(void) {
  g_chunk_count = 0;
  g_chunk_current = 0;

  if (g_zenv.dialogue_flags & 1)
    ParseChunks_EU();
  else
    ParseChunks_US();
}

void Accessibility_Init(void) {
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_Init();
#endif
}

void Accessibility_SetLanguage(const char *lang_prefix) {
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_SetLanguage(lang_prefix);
#endif
}

void Accessibility_AnnounceDialog(void) {
  if (!SpatialAudio_IsEnabled()) return;
#if defined(__APPLE__) || defined(_WIN32)
  Accessibility_ParseChunks();
  g_chunk_current = 0;
  if (g_chunk_count > 0)
    SpeechSynthesis_Speak(g_chunks[0]);
#endif
}

void Accessibility_AnnounceNextChunk(void) {
  if (!SpatialAudio_IsEnabled()) return;
#if defined(__APPLE__) || defined(_WIN32)
  g_chunk_current++;
  if (g_chunk_current < g_chunk_count)
    SpeechSynthesis_SpeakQueued(g_chunks[g_chunk_current]);
#endif
}

void Accessibility_AdjustSpeechRate(int direction) {
  if (!SpatialAudio_IsEnabled()) return;
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_AdjustRate(direction);
#endif
}

void Accessibility_AnnounceChooseItem(int y_item_index) {
  if (!SpatialAudio_IsEnabled()) return;
#if defined(__APPLE__) || defined(_WIN32)
  const char *name;
  if (y_item_index == 12 && link_item_flute >= 2)
    name = A11y(kA11y_Flute);
  else
    name = A11yItemName(y_item_index);
  if (name) SpeechSynthesis_Speak(name);
#endif
}

void Accessibility_Shutdown(void) {
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_Shutdown();
#endif
}
