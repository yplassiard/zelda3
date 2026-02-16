// Windows TTS via NVDA Controller Client (runtime-loaded).
// If NVDA is not running or the DLL is not found, all calls silently do nothing.

#ifdef _WIN32

#include "speechsynthesis.h"
#include <windows.h>
#include <stdlib.h>

typedef unsigned long error_status_t;

typedef error_status_t (__stdcall *pfn_speakText)(const wchar_t *text);
typedef error_status_t (__stdcall *pfn_cancelSpeech)(void);
typedef error_status_t (__stdcall *pfn_testIfRunning)(void);

static HMODULE g_nvda_dll;
static pfn_speakText g_speakText;
static pfn_cancelSpeech g_cancelSpeech;
static pfn_testIfRunning g_testIfRunning;

static int g_nvda_available;

static wchar_t *Utf8ToWide(const char *utf8) {
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (len <= 0) return NULL;
  wchar_t *buf = (wchar_t *)malloc(len * sizeof(wchar_t));
  if (buf)
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, len);
  return buf;
}

void SpeechSynthesis_Init(void) {
  g_nvda_dll = LoadLibraryA("nvdaControllerClient.dll");
  if (!g_nvda_dll) return;

  g_speakText = (pfn_speakText)GetProcAddress(g_nvda_dll, "nvdaController_speakText");
  g_cancelSpeech = (pfn_cancelSpeech)GetProcAddress(g_nvda_dll, "nvdaController_cancelSpeech");
  g_testIfRunning = (pfn_testIfRunning)GetProcAddress(g_nvda_dll, "nvdaController_testIfRunning");

  if (g_speakText && g_cancelSpeech && g_testIfRunning) {
    g_nvda_available = (g_testIfRunning() == 0);
  }
}

void SpeechSynthesis_Speak(const char *text) {
  if (!g_nvda_available || !text) return;
  g_cancelSpeech();
  wchar_t *wide = Utf8ToWide(text);
  if (wide) {
    g_speakText(wide);
    free(wide);
  }
}

void SpeechSynthesis_SpeakQueued(const char *text) {
  if (!g_nvda_available || !text) return;
  wchar_t *wide = Utf8ToWide(text);
  if (wide) {
    g_speakText(wide);
    free(wide);
  }
}

void SpeechSynthesis_AdjustRate(int direction) {
  // NVDA speech rate is controlled through NVDA's own settings.
  (void)direction;
}

void SpeechSynthesis_Shutdown(void) {
  if (g_nvda_dll) {
    FreeLibrary(g_nvda_dll);
    g_nvda_dll = NULL;
  }
  g_speakText = NULL;
  g_cancelSpeech = NULL;
  g_testIfRunning = NULL;
  g_nvda_available = 0;
}

void SpeechSynthesis_SetLanguage(const char *lang_prefix) { (void)lang_prefix; }

// Voice/volume API stubs — NVDA controls its own settings
void SpeechSynthesis_SetVolume(float volume) { (void)volume; }
float SpeechSynthesis_GetVolume(void) { return 1.0f; }
int SpeechSynthesis_GetVoiceCount(void) { return 0; }
const char *SpeechSynthesis_GetVoiceName(int index) { (void)index; return NULL; }
const char *SpeechSynthesis_GetVoiceId(int index) { (void)index; return NULL; }
void SpeechSynthesis_SetVoice(int index) { (void)index; }
void SpeechSynthesis_SetVoiceById(const char *identifier) { (void)identifier; }
int SpeechSynthesis_GetCurrentVoiceIndex(void) { return 0; }
float SpeechSynthesis_GetRate(void) { return 0.5f; }
void SpeechSynthesis_SetRate(float rate) { (void)rate; }

#endif  // _WIN32
