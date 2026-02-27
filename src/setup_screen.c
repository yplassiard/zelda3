#include "setup_screen.h"
#include "embed_font.h"
#include "filepicker.h"
#include "asset_extract.h"
#include "a11y_strings.h"
#include "util.h"
#include "types.h"
#include <SDL.h>
#include <string.h>
#include <stdio.h>

#if defined(__APPLE__)
#include "platform/macos/speechsynthesis.h"
#elif defined(_WIN32)
#include "platform/win32/speechsynthesis.h"
#endif

// ── Screen dimensions (SNES resolution) ──
#define SETUP_W 256
#define SETUP_H 224

// ── Menu items ──
enum {
  kItem_ROM,
  kItem_Language,
  kItem_Accessibility,
  kItem_Start,
  kItem_Count,
};

// ── Supported languages ──
typedef struct {
  const char *code;
  const char *display;
  const char *tts_prefix;   // TTS voice language prefix (e.g. "en", "fr", "de")
  bool rom_loaded;
  char rom_path[512];
} LangEntry;

static LangEntry g_langs[] = {
  {"us", "English", "en", true,  ""},
  {"fr", "Fran\xC3\xA7" "ais", "fr", false, ""},
};
#define NUM_LANGS ((int)(sizeof(g_langs) / sizeof(g_langs[0])))

// ── State ──
static int  g_item;             // main menu cursor
static int  g_submenu;          // 0=main, 1=language, 2=options, 3=voice, 4=legend
static int  g_lang_cursor;      // cursor in language submenu
static int  g_lang_selected;    // which language is active (index into g_langs)
static bool g_a11y;             // accessibility toggle
static bool g_a11y_announced;   // 3-sec announcement fired?
static bool g_reenter;          // re-enter flag
static char g_rom_path[512];    // path to US ROM
static uint32_t g_frames;       // frame counter

// ── Options submenu (g_submenu == 2) ──
enum {
  kOpt_SpeechRate,
  kOpt_Voice,
  kOpt_Volume,
  kOpt_Save,
  kOpt_Back,
  kOpt_Count,
};
static int g_options_index;     // cursor in options menu
static int g_voice_cursor;      // cursor in voice submenu
static int g_legend_index;      // cursor in legend menu
#define LEGEND_COUNT 24

// ── Pixel buffer ──
static uint32_t g_fb[SETUP_W * SETUP_H];

// Forward declarations
static void SaveGameConfig(void);

// ── Localized menu strings ──
typedef struct {
  const char *game_rom;          // "Game ROM"
  const char *language;          // "Language"
  const char *accessibility;     // "Accessibility"
  const char *start;             // "Start"
  const char *on;                // "On"
  const char *off;               // "Off"
  const char *not_selected;      // "[not selected]"
  const char *select_lang;       // "Select Language:"
  const char *hint_nav;          // "Enter: select  Esc: back"
  const char *ctrl_a11y;         // "Ctrl+Space: accessibility"
  const char *a11y_is_on;        // "Accessibility is ON"
  const char *rom_selected;      // "ROM selected."
  const char *lang_nav_hint;     // "Language selection. Use arrow keys."
  const char *selected_suffix;   // ", selected"
  const char *rom_loaded_sfx;    // ", ROM loaded"
  const char *back_to_main;      // "Back to main menu."
  const char *select_rom_first;  // "Please select a ROM first."
  const char *extracting;        // "Extracting assets..."
  const char *please_wait;       // "Please wait."
  const char *extracting_speak;  // "Extracting assets. Please wait."
  const char *err_read_rom;      // "Error: could not read ROM file."
  const char *press_ctrl_a11y;   // "Press Control Space to enable accessibility."
  const char *a11y_on;           // "Accessibility on"
  const char *a11y_off;          // "Accessibility off"
  const char *select_rom_fmt;    // "Select the %s ROM file."
  const char *rom_loaded_sel_fmt; // "%s ROM loaded and selected."
  const char *rom_no_match_fmt;  // "ROM does not match %s."
  const char *rom_not_yet;       // "Start. ROM not yet selected."
  const char *wrong_rom_fmt;     // "This is a %s ROM. Select the US ROM first."
  const char *bad_rom;           // "Unrecognized ROM."
  const char *cant_read;         // "Could not read file."
  const char *options_title;     // "Accessibility Options"
  const char *opt_speech_rate;   // "Speech Rate"
  const char *opt_speech_voice;  // "Voice"
  const char *opt_speech_volume; // "Speech Volume"
  const char *opt_save;          // "Save Settings"
  const char *opt_back;          // "Back"
  const char *options_closed;    // "Options closed"
  const char *settings_saved;    // "Settings saved"
  const char *help_title;        // "Sound Legend"
  const char *help_hint;         // "Sound Legend. Up and Down to browse. Escape to close."
} MenuL10n;

static const MenuL10n kL10n_en = {
  "Game ROM", "Language", "Accessibility", "Start",
  "On", "Off", "[not selected]", "Select Language:",
  "Enter: select  Esc: back", "Ctrl+Space: accessibility",
  "Accessibility is ON", "ROM selected.",
  "Language selection. Use arrow keys.", ", selected", ", ROM loaded",
  "Back to main menu.", "Please select a ROM first.",
  "Extracting assets...", "Please wait.",
  "Extracting assets. Please wait.",
  "Error: could not read ROM file.",
  "Press Control Space to enable accessibility.",
  "Accessibility on", "Accessibility off",
  "Select the %s ROM file.", "%s ROM loaded and selected.",
  "ROM does not match %s. Please try again.",
  "Start. ROM not yet selected.",
  "This is a %s ROM. Please select the US ROM first.",
  "Unrecognized ROM. Please select a valid Zelda 3 US ROM.",
  "Could not read file.",
  "Accessibility Options", "Speech Rate", "Voice", "Speech Volume",
  "Save Settings", "Back", "Options closed", "Settings saved",
  "Sound Legend", "Sound Legend. Up and Down to browse. Escape to close.",
};

static const MenuL10n kL10n_fr = {
  "ROM du jeu", "Langue", "Accessibilit\xC3\xA9", "Lancer",
  "Oui", "Non", "[non s\xC3\xA9lectionn\xC3\xA9]", "Choisir la langue :",
  "Entr\xC3\xA9" "e: choisir  \xC3\x89" "chap: retour", "Ctrl+Espace : accessibilit\xC3\xA9",
  "Accessibilit\xC3\xA9 activ\xC3\xA9" "e", "ROM s\xC3\xA9lectionn\xC3\xA9" "e.",
  "S\xC3\xA9lection de la langue. Utilisez les fl\xC3\xA8" "ches.",
  ", s\xC3\xA9lectionn\xC3\xA9", ", ROM charg\xC3\xA9" "e",
  "Retour au menu principal.", "Veuillez d'abord choisir une ROM.",
  "Extraction en cours...", "Veuillez patienter.",
  "Extraction en cours. Veuillez patienter.",
  "Erreur : impossible de lire la ROM.",
  "Appuyez sur Contr\xC3\xB4le Espace pour activer l'accessibilit\xC3\xA9.",
  "Accessibilit\xC3\xA9 activ\xC3\xA9" "e", "Accessibilit\xC3\xA9 d\xC3\xA9sactiv\xC3\xA9" "e",
  "S\xC3\xA9lectionnez la ROM %s.", "ROM %s charg\xC3\xA9" "e et s\xC3\xA9lectionn\xC3\xA9" "e.",
  "La ROM ne correspond pas \xC3\xA0 %s. R\xC3\xA9" "essayez.",
  "Lancer. ROM non s\xC3\xA9lectionn\xC3\xA9" "e.",
  "Ceci est une ROM %s. S\xC3\xA9lectionnez d'abord la ROM US.",
  "ROM non reconnue. S\xC3\xA9lectionnez une ROM Zelda 3 US valide.",
  "Impossible de lire le fichier.",
  "Options d'accessibilit\xC3\xA9", "Vitesse de parole", "Voix", "Volume de parole",
  "Enregistrer", "Retour", "Options ferm\xC3\xA9" "es", "Param\xC3\xA8tres enregistr\xC3\xA9s",
  "L\xC3\xA9gende sonore", "L\xC3\xA9gende sonore. Haut et Bas pour parcourir. \xC3\x89" "chap pour fermer.",
};

static const MenuL10n *GetL10n(void) {
  if (strcmp(g_langs[g_lang_selected].tts_prefix, "fr") == 0) return &kL10n_fr;
  return &kL10n_en;
}

// ── Switch TTS voice to match language ──
static void SetTTSLanguage(int lang_index) {
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_SetLanguage(g_langs[lang_index].tts_prefix);
#endif
  (void)lang_index;
}

// ════════════════════════════════════════════════════════════
//  Drawing helpers
// ════════════════════════════════════════════════════════════

static void PutPixel(int x, int y, uint32_t c) {
  if ((unsigned)x < SETUP_W && (unsigned)y < SETUP_H)
    g_fb[y * SETUP_W + x] = c;
}

static void FillRect(int x0, int y0, int w, int h, uint32_t c) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++)
      PutPixel(x, y, c);
}

static void ClearScreen(void) {
  for (int i = 0; i < SETUP_W * SETUP_H; i++)
    g_fb[i] = kBgColor;
}

static void DrawChar8(int x, int y, char ch, uint32_t color) {
  if (ch < 32 || ch > 126) return;
  const uint8_t *g = kFont8x8[ch - 32];
  for (int r = 0; r < 8; r++) {
    uint8_t bits = g[r];
    for (int c = 0; c < 8; c++)
      if (bits & (0x80 >> c))
        PutPixel(x + c, y + r, color);
  }
}

static void DrawStr(int x, int y, const char *s, uint32_t color) {
  while (*s) {
    if ((uint8_t)s[0] == 0xC3 && s[1]) {
      const uint8_t *g = GetAccentedGlyph((uint8_t)s[1]);
      if (g) {
        for (int r = 0; r < 8; r++) {
          uint8_t bits = g[r];
          for (int c = 0; c < 8; c++)
            if (bits & (0x80 >> c))
              PutPixel(x + c, y + r, color);
        }
      }
      s += 2;
    } else {
      DrawChar8(x, y, *s, color);
      s++;
    }
    x += 8;
  }
}

static int StrPixelWidth(const char *s) {
  int w = 0;
  while (*s) {
    if ((uint8_t)s[0] == 0xC3 && s[1]) { s += 2; }
    else { s++; }
    w += 8;
  }
  return w;
}

static void DrawStrCenter(int y, const char *s, uint32_t color) {
  DrawStr((SETUP_W - StrPixelWidth(s)) / 2, y, s, color);
}

// Draw a character at scale with vertical golden gradient + 1px shadow
static void DrawCharTitle(int x, int y, char ch, int scale) {
  if (ch < 32 || ch > 126) return;
  const uint8_t *glyph = kFont8x8[ch - 32];
  int th = 8 * scale;
  for (int r = 0; r < 8; r++) {
    uint8_t bits = glyph[r];
    int grad = 1 + (r * 6) / 7;  // palette index 1..7
    uint32_t fg = kGoldPalette[grad];
    for (int c = 0; c < 8; c++) {
      if (!(bits & (0x80 >> c))) continue;
      for (int sy = 0; sy < scale; sy++)
        for (int sx = 0; sx < scale; sx++) {
          // shadow
          PutPixel(x + c * scale + sx + 1, y + r * scale + sy + 1,
                   kGoldPalette[0]);
          // foreground
          PutPixel(x + c * scale + sx, y + r * scale + sy, fg);
        }
    }
  }
}

static void DrawTitle(void) {
  const char *title = "ZELDA 3";
  int scale = 3;
  int cw = 8 * scale;
  int tw = (int)strlen(title) * cw;
  int x0 = (SETUP_W - tw) / 2;
  int y0 = 12;
  for (const char *p = title; *p; p++, x0 += cw)
    DrawCharTitle(x0, y0, *p, scale);
}

// ════════════════════════════════════════════════════════════
//  TTS helpers
// ════════════════════════════════════════════════════════════

static void Speak(const char *text) {
#if defined(__APPLE__) || defined(_WIN32)
  SpeechSynthesis_Speak(text);
#endif
  (void)text;
}

static void SpeakIfA11y(const char *text) {
  if (g_a11y) Speak(text);
}

static void AnnounceOptionsItem(void) {
  const MenuL10n *l = GetL10n();
  char buf[256];
  switch (g_options_index) {
  case kOpt_SpeechRate: {
#if defined(__APPLE__) || defined(_WIN32)
    int pct = (int)(SpeechSynthesis_GetRate() * 100 + 0.5f);
#else
    int pct = 50;
#endif
    snprintf(buf, sizeof(buf), "%s: %d%%", l->opt_speech_rate, pct);
    break;
  }
  case kOpt_Voice: {
#if defined(__APPLE__) || defined(_WIN32)
    int vi = SpeechSynthesis_GetCurrentVoiceIndex();
    const char *vn = (vi >= 0) ? SpeechSynthesis_GetVoiceName(vi) : "?";
#else
    const char *vn = "default";
#endif
    snprintf(buf, sizeof(buf), "%s: %s", l->opt_speech_voice, vn);
    break;
  }
  case kOpt_Volume: {
#if defined(__APPLE__) || defined(_WIN32)
    int pct = (int)(SpeechSynthesis_GetVolume() * 100 + 0.5f);
#else
    int pct = 100;
#endif
    snprintf(buf, sizeof(buf), "%s: %d%%", l->opt_speech_volume, pct);
    break;
  }
  case kOpt_Save:
    snprintf(buf, sizeof(buf), "%s", l->opt_save);
    break;
  case kOpt_Back:
    snprintf(buf, sizeof(buf), "%s", l->opt_back);
    break;
  default: return;
  }
  SpeakIfA11y(buf);
}

static void AnnounceItem(void) {
  const MenuL10n *l = GetL10n();
  char buf[256];
  if (g_submenu == 0) {
    switch (g_item) {
    case kItem_ROM:
      snprintf(buf, sizeof(buf), "%s: %s", l->game_rom,
               g_rom_path[0] ? g_rom_path : l->not_selected);
      break;
    case kItem_Language:
      snprintf(buf, sizeof(buf), "%s: %s", l->language,
               g_langs[g_lang_selected].display);
      break;
    case kItem_Accessibility:
      snprintf(buf, sizeof(buf), "%s: %s", l->accessibility,
               g_a11y ? l->on : l->off);
      break;
    case kItem_Start:
      snprintf(buf, sizeof(buf), "%s",
               g_rom_path[0] ? l->start : l->rom_not_yet);
      break;
    default: return;
    }
    SpeakIfA11y(buf);
  } else if (g_submenu == 1) {
    snprintf(buf, sizeof(buf), "%s%s",
             g_langs[g_lang_cursor].display,
             g_lang_cursor == g_lang_selected ? l->selected_suffix
             : g_langs[g_lang_cursor].rom_loaded ? l->rom_loaded_sfx : "");
    SpeakIfA11y(buf);
  } else if (g_submenu == 2) {
    AnnounceOptionsItem();
  } else if (g_submenu == 3) {
#if defined(__APPLE__) || defined(_WIN32)
    int vc = SpeechSynthesis_GetVoiceCount();
    if (g_voice_cursor >= 0 && g_voice_cursor < vc) {
      const char *vn = SpeechSynthesis_GetVoiceName(g_voice_cursor);
      int cur = SpeechSynthesis_GetCurrentVoiceIndex();
      snprintf(buf, sizeof(buf), "%s%s", vn,
               g_voice_cursor == cur ? " *" : "");
      SpeakIfA11y(buf);
    }
#endif
  } else if (g_submenu == 4) {
    const char *name = A11yLegendName(g_legend_index);
    if (name) SpeakIfA11y(name);
  }
}

// ════════════════════════════════════════════════════════════
//  Rendering
// ════════════════════════════════════════════════════════════

static void Render(void) {
  ClearScreen();

  // ── Title ──
  DrawTitle();
  DrawStrCenter(40, "A LINK TO THE PAST", kGoldPalette[4]);

  // ── Separator ──
  for (int x = 32; x < SETUP_W - 32; x++)
    PutPixel(x, 54, kGoldPalette[2]);

  // ── Menu ──
  int mx = 24;
  int my = 66;

  const MenuL10n *l = GetL10n();

  if (g_submenu == 0) {
    for (int i = 0; i < kItem_Count; i++) {
      bool sel = (i == g_item);
      uint32_t color = sel ? kColorWhite : kColorGray;
      char line[64];

      switch (i) {
      case kItem_ROM: {
        const char *rn = g_rom_path[0] ? g_rom_path : l->not_selected;
        // Show only filename, not full path
        const char *slash = strrchr(rn, '/');
        if (!slash) slash = strrchr(rn, '\\');
        if (slash) rn = slash + 1;
        snprintf(line, sizeof(line), "%s: %s", l->game_rom, rn);
        if (strlen(line) > 27) { line[24] = '.'; line[25] = '.'; line[26] = 0; }
        break;
      }
      case kItem_Language:
        snprintf(line, sizeof(line), "%s: %s", l->language,
                 g_langs[g_lang_selected].display);
        break;
      case kItem_Accessibility:
        snprintf(line, sizeof(line), "%s: %s", l->accessibility,
                 g_a11y ? l->on : l->off);
        break;
      case kItem_Start:
        snprintf(line, sizeof(line), "%s", l->start);
        if (!g_rom_path[0]) color = kColorDimGray;
        break;
      }

      DrawStr(mx, my + i * 16, sel ? ">" : " ", kGoldPalette[6]);
      DrawStr(mx + 12, my + i * 16, line, color);
    }
  } else if (g_submenu == 1) {
    // Language submenu
    DrawStr(mx, my - 4, l->select_lang, kColorWhite);
    int vis_start = 0;
    int vis_count = NUM_LANGS;
    // Scroll if needed (show 10 items max)
    if (vis_count > 10) {
      vis_start = g_lang_cursor - 4;
      if (vis_start < 0) vis_start = 0;
      if (vis_start > vis_count - 10) vis_start = vis_count - 10;
      vis_count = 10;
    }
    for (int vi = 0; vi < vis_count; vi++) {
      int i = vis_start + vi;
      bool sel = (i == g_lang_cursor);
      uint32_t color = sel ? kColorWhite : kColorGray;
      char line[64];
      const char *tag = (i == g_lang_selected) ? " *"
                        : g_langs[i].rom_loaded ? " [ROM]" : "";
      snprintf(line, sizeof(line), "%s%s", g_langs[i].display, tag);
      DrawStr(mx, my + 10 + vi * 12, sel ? ">" : " ", kGoldPalette[6]);
      DrawStr(mx + 12, my + 10 + vi * 12, line, color);
    }
    DrawStr(mx + 12, my + 10 + vis_count * 12 + 4,
            l->hint_nav, kColorDimGray);
  } else if (g_submenu == 2) {
    // Options submenu
    DrawStr(mx, my - 4, l->options_title, kColorWhite);
    const char *labels[kOpt_Count];
    char val_buf[kOpt_Count][32];
    labels[kOpt_SpeechRate] = l->opt_speech_rate;
    labels[kOpt_Voice] = l->opt_speech_voice;
    labels[kOpt_Volume] = l->opt_speech_volume;
    labels[kOpt_Save] = l->opt_save;
    labels[kOpt_Back] = l->opt_back;
    memset(val_buf, 0, sizeof(val_buf));
#if defined(__APPLE__) || defined(_WIN32)
    snprintf(val_buf[kOpt_SpeechRate], sizeof(val_buf[0]), ": %d%%",
             (int)(SpeechSynthesis_GetRate() * 100 + 0.5f));
    {
      int vi = SpeechSynthesis_GetCurrentVoiceIndex();
      snprintf(val_buf[kOpt_Voice], sizeof(val_buf[0]), ": %s",
               vi >= 0 ? SpeechSynthesis_GetVoiceName(vi) : "?");
    }
    snprintf(val_buf[kOpt_Volume], sizeof(val_buf[0]), ": %d%%",
             (int)(SpeechSynthesis_GetVolume() * 100 + 0.5f));
#endif
    for (int i = 0; i < kOpt_Count; i++) {
      bool sel = (i == g_options_index);
      uint32_t color = sel ? kColorWhite : kColorGray;
      char line[64];
      snprintf(line, sizeof(line), "%s%s", labels[i], val_buf[i]);
      DrawStr(mx, my + 10 + i * 12, sel ? ">" : " ", kGoldPalette[6]);
      DrawStr(mx + 12, my + 10 + i * 12, line, color);
    }
  } else if (g_submenu == 3) {
    // Voice selection submenu
    DrawStr(mx, my - 4, l->opt_speech_voice, kColorWhite);
#if defined(__APPLE__) || defined(_WIN32)
    int vc = SpeechSynthesis_GetVoiceCount();
    int cur = SpeechSynthesis_GetCurrentVoiceIndex();
    int vis_start = 0;
    int vis_count = vc;
    if (vis_count > 10) {
      vis_start = g_voice_cursor - 4;
      if (vis_start < 0) vis_start = 0;
      if (vis_start > vis_count - 10) vis_start = vis_count - 10;
      vis_count = 10;
    }
    for (int vi = 0; vi < vis_count; vi++) {
      int i = vis_start + vi;
      bool sel = (i == g_voice_cursor);
      uint32_t color = sel ? kColorWhite : kColorGray;
      char line[64];
      snprintf(line, sizeof(line), "%s%s",
               SpeechSynthesis_GetVoiceName(i),
               i == cur ? " *" : "");
      DrawStr(mx, my + 10 + vi * 12, sel ? ">" : " ", kGoldPalette[6]);
      DrawStr(mx + 12, my + 10 + vi * 12, line, color);
    }
#endif
  } else if (g_submenu == 4) {
    // Sound legend submenu
    DrawStr(mx, my - 4, l->help_title, kColorWhite);
    int vis_start = 0;
    int vis_count = LEGEND_COUNT;
    if (vis_count > 10) {
      vis_start = g_legend_index - 4;
      if (vis_start < 0) vis_start = 0;
      if (vis_start > vis_count - 10) vis_start = vis_count - 10;
      vis_count = 10;
    }
    for (int vi = 0; vi < vis_count; vi++) {
      int i = vis_start + vi;
      bool sel = (i == g_legend_index);
      uint32_t color = sel ? kColorWhite : kColorGray;
      const char *name = A11yLegendName(i);
      DrawStr(mx, my + 10 + vi * 12, sel ? ">" : " ", kGoldPalette[6]);
      DrawStr(mx + 12, my + 10 + vi * 12, name ? name : "?", color);
    }
  }

  // ── Status line ──
  if (g_frames > 180 && !g_a11y) {
    DrawStrCenter(200, l->ctrl_a11y, kColorDimGray);
  }
  if (g_frames > 180 && g_a11y) {
    DrawStrCenter(200, l->a11y_is_on, kGoldPalette[3]);
  }
}

// ════════════════════════════════════════════════════════════
//  Settings persistence (zelda3_a11y.ini)
// ════════════════════════════════════════════════════════════

static void SaveSetupSettings(void) {
  // Read existing a11y settings to preserve them
  char existing[4096] = "";
  FILE *f = fopen("zelda3_a11y.ini", "r");
  if (f) {
    size_t n = fread(existing, 1, sizeof(existing) - 1, f);
    existing[n] = 0;
    fclose(f);
  }

  f = fopen("zelda3_a11y.ini", "w");
  if (!f) return;

  bool wrote_section = false;
  // Write existing lines, skipping our keys
  char *line = existing;
  while (*line) {
    char *nl = strchr(line, '\n');
    int len = nl ? (int)(nl - line) : (int)strlen(line);

    if (strncmp(line, "[Accessibility]", 15) == 0)
      wrote_section = true;

    // Skip lines we'll rewrite
    if (strncmp(line, "accessibility_enabled=", 22) != 0 &&
        strncmp(line, "setup_language=", 15) != 0) {
      fwrite(line, 1, len, f);
      fputc('\n', f);
    }

    line = nl ? nl + 1 : line + len;
  }

  // Add section header if not present
  if (!wrote_section)
    fprintf(f, "[Accessibility]\n");

  // Append our settings
  fprintf(f, "accessibility_enabled=%d\n", g_a11y ? 1 : 0);
  fprintf(f, "setup_language=%s\n", g_langs[g_lang_selected].code);
  fclose(f);

  // Also update zelda3.ini: Language and Controls (AZERTY for French)
  SaveGameConfig();
}

static void SaveGameConfig(void) {
  char ini_buf[8192] = "";
  FILE *f = fopen("zelda3.ini", "r");
  if (f) {
    size_t n = fread(ini_buf, 1, sizeof(ini_buf) - 1, f);
    ini_buf[n] = 0;
    fclose(f);
  }
  f = fopen("zelda3.ini", "w");
  if (!f) return;

  bool wrote_lang = false, wrote_ctrl = false;
  const char *lang_code = g_langs[g_lang_selected].code;
  bool is_fr = (strcmp(lang_code, "fr") == 0);
  const char *controls_val = is_fr
    ? "Up, Down, Left, Right, Right Shift, Return, x, w, s, q, c, v"
    : "Up, Down, Left, Right, Right Shift, Return, x, z, s, a, c, v";

  char *p = ini_buf;
  while (*p) {
    char *nl = strchr(p, '\n');
    int len = nl ? (int)(nl - p) : (int)strlen(p);

    // Replace Language line
    if (strncmp(p, "Language", 8) == 0 && !wrote_lang) {
      char *eq = memchr(p, '=', len);
      if (eq) {
        fprintf(f, "Language = %s\n", lang_code);
        wrote_lang = true;
        p = nl ? nl + 1 : p + len;
        continue;
      }
    }
    // Replace Controls line (skip commented ones)
    if (p[0] != '#' && strncmp(p, "Controls", 8) == 0 && !wrote_ctrl) {
      char *eq = memchr(p, '=', len);
      if (eq) {
        fprintf(f, "Controls = %s\n", controls_val);
        wrote_ctrl = true;
        p = nl ? nl + 1 : p + len;
        continue;
      }
    }

    fwrite(p, 1, len, f);
    fputc('\n', f);
    p = nl ? nl + 1 : p + len;
  }
  fclose(f);
}

static void LoadSetupSettings(void) {
  FILE *f = fopen("zelda3_a11y.ini", "r");
  if (!f) return;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "accessibility_enabled=", 22) == 0) {
      g_a11y = (line[22] == '1');
    } else if (strncmp(line, "setup_language=", 15) == 0) {
      // Trim newline
      char val[32];
      int i = 0;
      const char *p = line + 15;
      while (*p && *p != '\n' && *p != '\r' && i < (int)sizeof(val) - 1)
        val[i++] = *p++;
      val[i] = 0;
      for (int li = 0; li < NUM_LANGS; li++) {
        if (strcmp(g_langs[li].code, val) == 0) {
          g_lang_selected = li;
          break;
        }
      }
    }
  }
  fclose(f);
}

// ════════════════════════════════════════════════════════════
//  Extraction
// ════════════════════════════════════════════════════════════

static bool CopyFile(const char *src, const char *dst) {
  FILE *fin = fopen(src, "rb");
  if (!fin) return false;
  FILE *fout = fopen(dst, "wb");
  if (!fout) { fclose(fin); return false; }
  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), fin)) > 0)
    fwrite(buf, 1, n, fout);
  fclose(fin);
  fclose(fout);
  return true;
}

static bool DoExtraction(const char *app_dir,
                         SDL_Renderer *renderer, SDL_Texture *tex) {
  const MenuL10n *l = GetL10n();
  // Show "Please wait..."
  ClearScreen();
  DrawTitle();
  DrawStrCenter(100, l->extracting, kColorWhite);
  DrawStrCenter(116, l->please_wait, kColorGray);

  // Present the "please wait" frame
  void *px;
  int pitch;
  SDL_LockTexture(tex, NULL, &px, &pitch);
  for (int y = 0; y < SETUP_H; y++)
    memcpy((uint8_t *)px + y * pitch, g_fb + y * SETUP_W, SETUP_W * 4);
  SDL_UnlockTexture(tex);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, tex, NULL, NULL);
  SDL_RenderPresent(renderer);

  SpeakIfA11y(l->extracting_speak);

  // Read ROM
  size_t rom_size = 0;
  uint8 *rom = ReadWholeFile(g_rom_path, &rom_size);
  if (!rom) {
    SpeakIfA11y(l->err_read_rom);
    return false;
  }

  // Extract assets
  char assets_path[512];
  snprintf(assets_path, sizeof(assets_path), "%s/zelda3_assets.dat", app_dir);
  bool ok = AssetExtract_BuildFromROM(rom, rom_size, assets_path);

  if (ok) {
    // If a non-US language was selected and has a ROM, add its dialogue
    if (g_lang_selected > 0 && g_langs[g_lang_selected].rom_loaded) {
      size_t lang_size = 0;
      uint8 *lang_rom = ReadWholeFile(g_langs[g_lang_selected].rom_path,
                                      &lang_size);
      if (lang_rom) {
        AssetExtract_AddLanguage(rom, rom_size, lang_rom, lang_size,
                                 assets_path);
        free(lang_rom);
      }
    }

    // Copy ROM to App Support for future language additions
    char rom_dst[512];
    snprintf(rom_dst, sizeof(rom_dst), "%s/zelda3.sfc", app_dir);
    CopyFile(g_rom_path, rom_dst);
  } else {
    const char *err = AssetExtract_GetError();
    char msg[256];
    snprintf(msg, sizeof(msg), "%s: %s",
             l->extracting, err ? err : "unknown error");
    SpeakIfA11y(msg);
  }

  free(rom);
  return ok;
}

// ════════════════════════════════════════════════════════════
//  Input handling
// ════════════════════════════════════════════════════════════

// Returns: 0=continue, 1=start game, -1=quit
static int HandleKey(SDL_Keycode key, SDL_Keymod mod,
                     const char *app_dir,
                     SDL_Renderer *renderer, SDL_Texture *tex) {
  bool ctrl = (mod & KMOD_CTRL) != 0;

  const MenuL10n *l = GetL10n();

  // Ctrl+Space: toggle accessibility (always available)
  if (ctrl && key == SDLK_SPACE) {
    g_a11y = !g_a11y;
    Speak(g_a11y ? l->a11y_on : l->a11y_off);
    return 0;
  }

  if (g_submenu == 0) {
    // Ctrl+O: open options (only when a11y is on)
    if (ctrl && key == SDLK_o && g_a11y) {
      g_submenu = 2;
      g_options_index = 0;
      SpeakIfA11y(l->options_title);
      AnnounceOptionsItem();
      return 0;
    }
    // Ctrl+H: open sound legend (only when a11y is on)
    if (ctrl && key == SDLK_h && g_a11y) {
      g_submenu = 4;
      g_legend_index = 0;
      SpeakIfA11y(l->help_hint);
      return 0;
    }
    // Main menu navigation
    switch (key) {
    case SDLK_UP:
      g_item = (g_item + kItem_Count - 1) % kItem_Count;
      AnnounceItem();
      break;
    case SDLK_DOWN:
      g_item = (g_item + 1) % kItem_Count;
      AnnounceItem();
      break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      switch (g_item) {
      case kItem_ROM: {
        const char *path = FilePicker_OpenROM();
        if (path) {
          // Validate ROM
          size_t sz = 0;
          uint8 *data = ReadWholeFile(path, &sz);
          if (data) {
            const char *lang = AssetExtract_IdentifyROM(data, sz);
            free(data);
            if (lang && strcmp(lang, "us") == 0) {
              snprintf(g_rom_path, sizeof(g_rom_path), "%s", path);
              SpeakIfA11y(l->rom_selected);
            } else if (lang) {
              char msg[128];
              const char *name = AssetExtract_GetROMName(NULL, 0);
              snprintf(msg, sizeof(msg), l->wrong_rom_fmt,
                       name ? name : lang);
              SpeakIfA11y(msg);
            } else {
              SpeakIfA11y(l->bad_rom);
            }
          } else {
            SpeakIfA11y(l->cant_read);
          }
        }
        break;
      }
      case kItem_Language:
        g_submenu = 1;
        g_lang_cursor = g_lang_selected;
        SpeakIfA11y(l->lang_nav_hint);
        AnnounceItem();
        break;
      case kItem_Accessibility:
        g_a11y = !g_a11y;
        Speak(g_a11y ? l->a11y_on : l->a11y_off);
        break;
      case kItem_Start:
        if (g_rom_path[0]) {
          SaveSetupSettings();
          if (DoExtraction(app_dir, renderer, tex))
            return 1;  // success - start game
          // On failure, stay in setup
        } else {
          SpeakIfA11y(l->select_rom_first);
        }
        break;
      }
      break;
    case SDLK_ESCAPE:
      return -1;  // quit
    }
  } else if (g_submenu == 1) {
    // Language submenu
    switch (key) {
    case SDLK_UP:
      g_lang_cursor = (g_lang_cursor + NUM_LANGS - 1) % NUM_LANGS;
      AnnounceItem();
      break;
    case SDLK_DOWN:
      g_lang_cursor = (g_lang_cursor + 1) % NUM_LANGS;
      AnnounceItem();
      break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      if (g_lang_cursor == 0) {
        // US - always available, no extra ROM needed
        g_lang_selected = 0;
        SetTTSLanguage(0);
        {
          char msg[128];
          snprintf(msg, sizeof(msg), "%s.", g_langs[0].display);
          SpeakIfA11y(msg);
        }
        g_submenu = 0;
        g_item = kItem_Language;
      } else if (g_langs[g_lang_cursor].rom_loaded) {
        // Already has ROM
        g_lang_selected = g_lang_cursor;
        SetTTSLanguage(g_lang_cursor);
        {
          char msg[128];
          snprintf(msg, sizeof(msg), "%s.", g_langs[g_lang_cursor].display);
          SpeakIfA11y(msg);
        }
        g_submenu = 0;
        g_item = kItem_Language;
      } else {
        // Need to provide ROM for this language
        char msg[128];
        snprintf(msg, sizeof(msg), l->select_rom_fmt,
                 g_langs[g_lang_cursor].display);
        SpeakIfA11y(msg);
        const char *path = FilePicker_OpenROM();
        if (path) {
          size_t sz = 0;
          uint8 *data = ReadWholeFile(path, &sz);
          if (data) {
            const char *lang_id = AssetExtract_IdentifyROM(data, sz);
            free(data);
            if (lang_id && strcmp(lang_id, g_langs[g_lang_cursor].code) == 0) {
              snprintf(g_langs[g_lang_cursor].rom_path,
                       sizeof(g_langs[g_lang_cursor].rom_path), "%s", path);
              g_langs[g_lang_cursor].rom_loaded = true;
              g_lang_selected = g_lang_cursor;
              SetTTSLanguage(g_lang_cursor);
              // Speak in new language after switch
              const MenuL10n *nl = GetL10n();
              snprintf(msg, sizeof(msg), nl->rom_loaded_sel_fmt,
                       g_langs[g_lang_cursor].display);
              SpeakIfA11y(msg);
              g_submenu = 0;
              g_item = kItem_Language;
            } else {
              snprintf(msg, sizeof(msg), l->rom_no_match_fmt,
                       g_langs[g_lang_cursor].display);
              SpeakIfA11y(msg);
            }
          }
        }
      }
      break;
    case SDLK_ESCAPE:
      g_submenu = 0;
      g_item = kItem_Language;
      SpeakIfA11y(l->back_to_main);
      AnnounceItem();
      break;
    }
  } else if (g_submenu == 2) {
    // Options menu
    switch (key) {
    case SDLK_UP:
      g_options_index = (g_options_index + kOpt_Count - 1) % kOpt_Count;
      AnnounceOptionsItem();
      break;
    case SDLK_DOWN:
      g_options_index = (g_options_index + 1) % kOpt_Count;
      AnnounceOptionsItem();
      break;
    case SDLK_LEFT:
    case SDLK_RIGHT: {
      int dir = (key == SDLK_RIGHT) ? 1 : -1;
      if (g_options_index == kOpt_SpeechRate) {
#if defined(__APPLE__) || defined(_WIN32)
        float r = SpeechSynthesis_GetRate() + dir * 0.05f;
        if (r < 0.0f) r = 0.0f;
        if (r > 1.0f) r = 1.0f;
        SpeechSynthesis_SetRate(r);
#endif
        AnnounceOptionsItem();
      } else if (g_options_index == kOpt_Volume) {
#if defined(__APPLE__) || defined(_WIN32)
        float v = SpeechSynthesis_GetVolume() + dir * 0.1f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        SpeechSynthesis_SetVolume(v);
#endif
        AnnounceOptionsItem();
      }
      break;
    }
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      if (g_options_index == kOpt_Voice) {
#if defined(__APPLE__) || defined(_WIN32)
        g_voice_cursor = SpeechSynthesis_GetCurrentVoiceIndex();
        if (g_voice_cursor < 0) g_voice_cursor = 0;
#else
        g_voice_cursor = 0;
#endif
        g_submenu = 3;
        AnnounceItem();
      } else if (g_options_index == kOpt_Save) {
        SaveSetupSettings();
        SpeakIfA11y(l->settings_saved);
      } else if (g_options_index == kOpt_Back) {
        g_submenu = 0;
        SpeakIfA11y(l->options_closed);
        AnnounceItem();
      }
      break;
    case SDLK_ESCAPE:
      g_submenu = 0;
      SpeakIfA11y(l->options_closed);
      AnnounceItem();
      break;
    }
  } else if (g_submenu == 3) {
    // Voice selection submenu
    switch (key) {
    case SDLK_UP:
    case SDLK_DOWN: {
#if defined(__APPLE__) || defined(_WIN32)
      int vc = SpeechSynthesis_GetVoiceCount();
      if (vc > 0) {
        g_voice_cursor += (key == SDLK_DOWN) ? 1 : -1;
        if (g_voice_cursor < 0) g_voice_cursor = vc - 1;
        if (g_voice_cursor >= vc) g_voice_cursor = 0;
        AnnounceItem();
      }
#endif
      break;
    }
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
#if defined(__APPLE__) || defined(_WIN32)
      SpeechSynthesis_SetVoice(g_voice_cursor);
#endif
      g_submenu = 2;
      AnnounceOptionsItem();
      break;
    case SDLK_ESCAPE:
      g_submenu = 2;
      AnnounceOptionsItem();
      break;
    }
  } else if (g_submenu == 4) {
    // Sound legend
    switch (key) {
    case SDLK_UP:
      g_legend_index--;
      if (g_legend_index < 0) g_legend_index = LEGEND_COUNT - 1;
      {
        const char *name = A11yLegendName(g_legend_index);
        if (name) SpeakIfA11y(name);
      }
      break;
    case SDLK_DOWN:
      g_legend_index++;
      if (g_legend_index >= LEGEND_COUNT) g_legend_index = 0;
      {
        const char *name = A11yLegendName(g_legend_index);
        if (name) SpeakIfA11y(name);
      }
      break;
    case SDLK_ESCAPE:
      g_submenu = 0;
      SpeakIfA11y(l->back_to_main);
      AnnounceItem();
      break;
    }
  }
  return 0;
}

// ════════════════════════════════════════════════════════════
//  Public API
// ════════════════════════════════════════════════════════════

bool SetupScreen_IsNeeded(const char *app_support_dir) {
  // Check app support directory
  char path[512];
  snprintf(path, sizeof(path), "%s/zelda3_assets.dat", app_support_dir);
  FILE *f = fopen(path, "rb");
  if (f) { fclose(f); return false; }

  // Check current directory (dev builds)
  f = fopen("zelda3_assets.dat", "rb");
  if (f) { fclose(f); return false; }

  // Check for BPS fallback
  f = fopen("zelda3_assets.bps", "rb");
  if (f) { fclose(f); return false; }

  return true;
}

bool SetupScreen_Run(SDL_Window *window, SDL_Renderer *renderer,
                     const char *app_support_dir) {
  // Reset state
  g_item = 0;
  g_submenu = 0;
  g_lang_cursor = 0;
  g_lang_selected = 0;
  g_a11y = false;
  g_a11y_announced = false;
  g_rom_path[0] = 0;
  g_frames = 0;
  g_options_index = 0;
  g_voice_cursor = 0;
  g_legend_index = 0;

  // Load persisted settings (may set g_a11y, g_lang_selected)
  LoadSetupSettings();

  // Create texture for rendering
  SDL_Texture *tex = SDL_CreateTexture(renderer,
    SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SETUP_W, SETUP_H);
  if (!tex) return false;

  bool result = false;
  bool running = true;

  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_KEYDOWN:
        if (!ev.key.repeat) {
          int r = HandleKey(ev.key.keysym.sym, (SDL_Keymod)ev.key.keysym.mod,
                            app_support_dir, renderer, tex);
          if (r == 1) { result = true; running = false; }
          if (r == -1) { result = false; running = false; }
        }
        break;
      }
    }

    // 3-second accessibility announcement (fires once, always speaks)
    g_frames++;
    if (g_frames == 180 && !g_a11y_announced) {
      g_a11y_announced = true;
      Speak(GetL10n()->press_ctrl_a11y);
    }

    // Render
    Render();

    void *px;
    int pitch;
    SDL_LockTexture(tex, NULL, &px, &pitch);
    for (int y = 0; y < SETUP_H; y++)
      memcpy((uint8_t *)px + y * pitch, g_fb + y * SETUP_W, SETUP_W * 4);
    SDL_UnlockTexture(tex);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_Delay(16);  // ~60 fps
  }

  SDL_DestroyTexture(tex);
  return result;
}

void SetupScreen_RequestReenter(void) {
  g_reenter = true;
}

bool SetupScreen_ShouldReenter(void) {
  bool r = g_reenter;
  g_reenter = false;
  return r;
}

bool SetupScreen_GetAccessibility(void) {
  return g_a11y;
}

const char *SetupScreen_GetLanguage(void) {
  return g_langs[g_lang_selected].code;
}

void SetupScreen_LoadPersistedSettings(void) {
  LoadSetupSettings();
}
