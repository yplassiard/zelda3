#if !defined(__APPLE__) && !defined(_WIN32)
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

const char *FilePicker_OpenROM(void) {
  static char path[PATH_MAX];
  // Try zenity first, fall back to kdialog
  const char *cmds[] = {
    "zenity --file-selection --title='Select ROM File' "
      "--file-filter='SNES ROM|*.sfc *.smc' 2>/dev/null",
    "kdialog --getopenfilename . 'SNES ROM (*.sfc *.smc)' 2>/dev/null",
    NULL
  };

  for (int i = 0; cmds[i]; i++) {
    FILE *f = popen(cmds[i], "r");
    if (f) {
      if (fgets(path, sizeof(path), f)) {
        int status = pclose(f);
        if (status == 0) {
          // Strip trailing newline
          char *nl = strchr(path, '\n');
          if (nl) *nl = 0;
          if (path[0])
            return path;
        }
      } else {
        pclose(f);
      }
    }
  }
  return NULL;
}

#endif  // !__APPLE__ && !_WIN32
