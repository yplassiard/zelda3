#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <limits.h>

const char *FilePicker_OpenROM(void) {
  static char path[PATH_MAX];
  @autoreleasepool {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setTitle:@"Select ROM File"];
    [panel setMessage:@"Choose a Zelda 3 SNES ROM file (.sfc or .smc)"];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];

    UTType *sfcType = [UTType typeWithFilenameExtension:@"sfc"];
    UTType *smcType = [UTType typeWithFilenameExtension:@"smc"];
    [panel setAllowedContentTypes:@[sfcType, smcType]];

    if ([panel runModal] == NSModalResponseOK) {
      NSURL *url = [[panel URLs] firstObject];
      if (url) {
        strlcpy(path, [[url path] UTF8String], sizeof(path));
        return path;
      }
    }
  }
  return NULL;
}

#endif  // __APPLE__
