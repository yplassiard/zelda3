#include "accessibility.h"
#include "types.h"
#include "variables.h"
#include <string.h>

#ifdef __APPLE__
#include "platform/macos/speechsynthesis.h"
#endif

// Character lookup table matching kTextAlphabet_US from text_compression.py.
// Index 0x00-0x66 maps to printable characters. 0 means skip (glyph/icon).
static const char kAccessibility_CharTable[0x67] = {
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
static const uint8 kAccessibility_CmdLengths[25] = {
  0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

// Command indices (byte - 0x67)
enum {
  kAccessibility_CmdScroll = 12,    // 0x73
  kAccessibility_CmdLine1 = 13,
  kAccessibility_CmdLine2 = 14,
  kAccessibility_CmdLine3 = 15,
  kAccessibility_CmdWaitkey = 23,   // 0x7E
  kAccessibility_CmdEndMsg = 24,    // 0x7F
};

#define MAX_CHUNKS 32
#define MAX_CHUNK_LEN 256

static char g_chunks[MAX_CHUNKS][MAX_CHUNK_LEN];
static int g_chunk_count;
static int g_chunk_current;

// Decode the messaging_text_buffer into chunks split at Scroll/Waitkey/EndMessage.
static void Accessibility_ParseChunks(void) {
  const uint8 *buf = messaging_text_buffer;
  g_chunk_count = 0;
  g_chunk_current = 0;

  int pos = 0;  // position within current chunk string

  for (int i = 0; i < 1024; i++) {
    uint8 c = buf[i];

    if (c < 0x67) {
      // Character byte
      char ch = kAccessibility_CharTable[c];
      if (ch && pos < MAX_CHUNK_LEN - 2)
        g_chunks[g_chunk_count][pos++] = ch;
    } else {
      // Command byte
      int cmd_idx = c - 0x67;
      if (cmd_idx >= 25)
        break;

      // Line/Scroll commands insert a space (Scroll is automatic, not a chunk boundary)
      if (cmd_idx == kAccessibility_CmdScroll ||
          cmd_idx == kAccessibility_CmdLine1 ||
          cmd_idx == kAccessibility_CmdLine2 ||
          cmd_idx == kAccessibility_CmdLine3) {
        if (pos < MAX_CHUNK_LEN - 2)
          g_chunks[g_chunk_count][pos++] = ' ';
      }

      // Waitkey, EndMessage — end the current chunk (player must press button)
      if (cmd_idx == kAccessibility_CmdWaitkey ||
          cmd_idx == kAccessibility_CmdEndMsg) {
        g_chunks[g_chunk_count][pos] = '\0';
        if (pos > 0 && g_chunk_count < MAX_CHUNKS - 1)
          g_chunk_count++;
        pos = 0;
        if (cmd_idx == kAccessibility_CmdEndMsg)
          break;
      }

      // Skip parameter bytes for multi-byte commands
      i += kAccessibility_CmdLengths[cmd_idx];
    }
  }

  // Finalize any trailing text (shouldn't happen normally, but be safe)
  if (pos > 0 && g_chunk_count < MAX_CHUNKS) {
    g_chunks[g_chunk_count][pos] = '\0';
    g_chunk_count++;
  }
}

void Accessibility_Init(void) {
#ifdef __APPLE__
  SpeechSynthesis_Init();
#endif
}

void Accessibility_AnnounceDialog(void) {
#ifdef __APPLE__
  // Suppress dialog TTS during game over (module 12) and save menu (module 14, submodule 11)
  // — these screens use dedicated choice tracking instead
  uint8 mod = main_module_index;
  if (mod == 12) return;
  if (mod == 14 && submodule_index == 11) return;

  Accessibility_ParseChunks();
  g_chunk_current = 0;
  if (g_chunk_count > 0)
    SpeechSynthesis_Speak(g_chunks[0]);
#endif
}

void Accessibility_AnnounceNextChunk(void) {
#ifdef __APPLE__
  g_chunk_current++;
  if (g_chunk_current < g_chunk_count)
    SpeechSynthesis_SpeakQueued(g_chunks[g_chunk_current]);
#endif
}

void Accessibility_AdjustSpeechRate(int direction) {
#ifdef __APPLE__
  SpeechSynthesis_AdjustRate(direction);
#endif
}

void Accessibility_Shutdown(void) {
#ifdef __APPLE__
  SpeechSynthesis_Shutdown();
#endif
}
