#ifndef SETUP_SCREEN_H
#define SETUP_SCREEN_H

#include <stdbool.h>

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;

// Check if first-time setup is needed (no assets in app support dir).
bool SetupScreen_IsNeeded(const char *app_support_dir);

// Run the setup screen event loop. Blocks until user completes setup or quits.
// Returns true if setup completed (assets extracted), false if user quit.
bool SetupScreen_Run(SDL_Window *window, SDL_Renderer *renderer,
                     const char *app_support_dir);

// Re-enter setup from in-game (Alt+Ctrl+S). Sets flag for main loop.
void SetupScreen_RequestReenter(void);

// Check and clear the re-enter flag.
bool SetupScreen_ShouldReenter(void);

// Returns true if the user enabled accessibility in the setup screen.
bool SetupScreen_GetAccessibility(void);

// Returns the selected language code (e.g. "us", "fr").
const char *SetupScreen_GetLanguage(void);

// Loads persisted settings from zelda3_a11y.ini.
void SetupScreen_LoadPersistedSettings(void);

#endif  // SETUP_SCREEN_H
