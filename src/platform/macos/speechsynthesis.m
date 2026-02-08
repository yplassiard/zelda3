#import <AVFoundation/AVFoundation.h>
#include "speechsynthesis.h"

static AVSpeechSynthesizer *g_synth;
static float g_speech_rate;

void SpeechSynthesis_Init(void) {
  @autoreleasepool {
    g_synth = [[AVSpeechSynthesizer alloc] init];
    g_speech_rate = AVSpeechUtteranceDefaultSpeechRate;
  }
}

static AVSpeechUtterance *MakeUtterance(const char *text) {
  NSString *str = [NSString stringWithUTF8String:text];
  if (!str)
    return nil;
  AVSpeechUtterance *utterance = [AVSpeechUtterance speechUtteranceWithString:str];
  utterance.rate = g_speech_rate;
  AVSpeechSynthesisVoice *voice = [AVSpeechSynthesisVoice voiceWithIdentifier:@"com.apple.voice.compact.en-US.Samantha"];
  if (voice)
    utterance.voice = voice;
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
    }
  }
}
