#ifndef ACCESSIBILITY_H
#define ACCESSIBILITY_H

void Accessibility_Init(void);
void Accessibility_SetLanguage(const char *lang_prefix);
void Accessibility_AnnounceDialog(void);
void Accessibility_AnnounceNextChunk(void);
void Accessibility_AdjustSpeechRate(int direction);
void Accessibility_AnnounceChooseItem(int y_item_index);
void Accessibility_Shutdown(void);

#endif  // ACCESSIBILITY_H
