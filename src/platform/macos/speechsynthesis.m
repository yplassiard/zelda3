#import <AVFoundation/AVFoundation.h>
#include "speechsynthesis.h"
#include <string.h>
#include <stdlib.h>

static AVSpeechSynthesizer *g_synth;
static float g_speech_rate;
static float g_speech_volume = 1.0f;

// Voice management
#define MAX_VOICES 64
static int g_voice_count;
static char *g_voice_names[MAX_VOICES];
static char *g_voice_ids[MAX_VOICES];
static int g_current_voice_index;
static NSString *g_current_voice_id;

static void EnumerateVoices(void) {
  g_voice_count = 0;
  NSArray<AVSpeechSynthesisVoice *> *voices = [AVSpeechSynthesisVoice speechVoices];
  for (AVSpeechSynthesisVoice *v in voices) {
    if (g_voice_count >= MAX_VOICES) break;
    NSString *lang = [v language];
    if (![lang hasPrefix:@"en"]) continue;
    const char *name = [[v name] UTF8String];
    const char *vid = [[v identifier] UTF8String];
    if (!name || !vid) continue;
    g_voice_names[g_voice_count] = strdup(name);
    g_voice_ids[g_voice_count] = strdup(vid);
    // Check if this is the current voice
    if (g_current_voice_id && [g_current_voice_id isEqualToString:[v identifier]])
      g_current_voice_index = g_voice_count;
    g_voice_count++;
  }
}

void SpeechSynthesis_Init(void) {
  @autoreleasepool {
    g_synth = [[AVSpeechSynthesizer alloc] init];
    g_speech_rate = AVSpeechUtteranceDefaultSpeechRate;
    g_current_voice_id = @"com.apple.voice.compact.en-US.Samantha";
    g_current_voice_index = 0;
    EnumerateVoices();
  }
}

static AVSpeechUtterance *MakeUtterance(const char *text) {
  NSString *str = [NSString stringWithUTF8String:text];
  if (!str)
    return nil;
  AVSpeechUtterance *utterance = [AVSpeechUtterance speechUtteranceWithString:str];
  utterance.rate = g_speech_rate;
  utterance.volume = g_speech_volume;
  if (g_current_voice_id) {
    AVSpeechSynthesisVoice *voice = [AVSpeechSynthesisVoice voiceWithIdentifier:g_current_voice_id];
    if (voice)
      utterance.voice = voice;
  }
  return utterance;
}

void SpeechSynthesis_Speak(const char *text) {
  if (!g_synth || !text || !text[0])
    return;
  @autoreleasepool {
    if ([g_synth isSpeaking])
      [g_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    AVSpeechUtterance *utterance = MakeUtterance(text);
    if (utterance)
      [g_synth speakUtterance:utterance];
  }
}

void SpeechSynthesis_SpeakQueued(const char *text) {
  if (!g_synth || !text || !text[0])
    return;
  @autoreleasepool {
    AVSpeechUtterance *utterance = MakeUtterance(text);
    if (utterance)
      [g_synth speakUtterance:utterance];
  }
}

void SpeechSynthesis_AdjustRate(int direction) {
  g_speech_rate += direction * 0.05f;
  if (g_speech_rate < AVSpeechUtteranceMinimumSpeechRate)
    g_speech_rate = AVSpeechUtteranceMinimumSpeechRate;
  if (g_speech_rate > AVSpeechUtteranceMaximumSpeechRate)
    g_speech_rate = AVSpeechUtteranceMaximumSpeechRate;
}

void SpeechSynthesis_Shutdown(void) {
  if (g_synth) {
    @autoreleasepool {
      if ([g_synth isSpeaking])
        [g_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
      g_synth = nil;
      g_current_voice_id = nil;
    }
  }
  for (int i = 0; i < g_voice_count; i++) {
    free(g_voice_names[i]);
    free(g_voice_ids[i]);
  }
  g_voice_count = 0;
}

void SpeechSynthesis_SetVolume(float volume) {
  if (volume < 0.0f) volume = 0.0f;
  if (volume > 1.0f) volume = 1.0f;
  g_speech_volume = volume;
}

float SpeechSynthesis_GetVolume(void) {
  return g_speech_volume;
}

int SpeechSynthesis_GetVoiceCount(void) {
  return g_voice_count;
}

const char *SpeechSynthesis_GetVoiceName(int index) {
  if (index < 0 || index >= g_voice_count) return NULL;
  return g_voice_names[index];
}

const char *SpeechSynthesis_GetVoiceId(int index) {
  if (index < 0 || index >= g_voice_count) return NULL;
  return g_voice_ids[index];
}

void SpeechSynthesis_SetVoice(int index) {
  if (index < 0 || index >= g_voice_count) return;
  @autoreleasepool {
    g_current_voice_index = index;
    g_current_voice_id = [NSString stringWithUTF8String:g_voice_ids[index]];
  }
}

void SpeechSynthesis_SetVoiceById(const char *identifier) {
  if (!identifier) return;
  @autoreleasepool {
    g_current_voice_id = [NSString stringWithUTF8String:identifier];
    // Find matching index
    for (int i = 0; i < g_voice_count; i++) {
      if (strcmp(g_voice_ids[i], identifier) == 0) {
        g_current_voice_index = i;
        return;
      }
    }
  }
}

int SpeechSynthesis_GetCurrentVoiceIndex(void) {
  return g_current_voice_index;
}

float SpeechSynthesis_GetRate(void) {
  return g_speech_rate;
}

void SpeechSynthesis_SetRate(float rate) {
  if (rate < AVSpeechUtteranceMinimumSpeechRate)
    rate = AVSpeechUtteranceMinimumSpeechRate;
  if (rate > AVSpeechUtteranceMaximumSpeechRate)
    rate = AVSpeechUtteranceMaximumSpeechRate;
  g_speech_rate = rate;
}
