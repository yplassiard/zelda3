#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <string.h>

const char *FilePicker_OpenROM(void) {
  static char filename[MAX_PATH];
  OPENFILENAMEA ofn;
  memset(&ofn, 0, sizeof(ofn));
  memset(filename, 0, sizeof(filename));

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFilter = "SNES ROM Files (*.sfc, *.smc)\0*.sfc;*.smc\0All Files\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrTitle = "Select ROM File";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  if (GetOpenFileNameA(&ofn))
    return filename;
  return NULL;
}

#endif  // _WIN32
