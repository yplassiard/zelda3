#ifndef SPEECHSYNTHESIS_H
#define SPEECHSYNTHESIS_H

void SpeechSynthesis_Init(void);
void SpeechSynthesis_Speak(const char *text);
void SpeechSynthesis_SpeakQueued(const char *text);
void SpeechSynthesis_AdjustRate(int direction);
void SpeechSynthesis_Shutdown(void);

#endif  // SPEECHSYNTHESIS_H
