#ifndef SPEECHSYNTHESIS_H
#define SPEECHSYNTHESIS_H

void SpeechSynthesis_Init(void);
void SpeechSynthesis_Speak(const char *text);
void SpeechSynthesis_SpeakQueued(const char *text);
void SpeechSynthesis_AdjustRate(int direction);
void SpeechSynthesis_Shutdown(void);

// Voice/volume API stubs — NVDA controls its own settings
void SpeechSynthesis_SetVolume(float volume);
float SpeechSynthesis_GetVolume(void);
int SpeechSynthesis_GetVoiceCount(void);
const char *SpeechSynthesis_GetVoiceName(int index);
const char *SpeechSynthesis_GetVoiceId(int index);
void SpeechSynthesis_SetVoice(int index);
void SpeechSynthesis_SetVoiceById(const char *identifier);
int SpeechSynthesis_GetCurrentVoiceIndex(void);
float SpeechSynthesis_GetRate(void);
void SpeechSynthesis_SetRate(float rate);

#endif  // SPEECHSYNTHESIS_H
