#ifndef FILEPICKER_H
#define FILEPICKER_H

// Opens a native file picker dialog to select a ROM file (.sfc/.smc).
// Returns a path to the selected file, or NULL if cancelled.
// The returned string is valid until the next call.
const char *FilePicker_OpenROM(void);

#endif  // FILEPICKER_H
