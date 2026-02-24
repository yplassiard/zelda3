#ifndef ASSET_EXTRACT_H
#define ASSET_EXTRACT_H

#include "types.h"
#include <stdbool.h>

// Extract all assets from a US ROM and write zelda3_assets.dat to output_path.
// Returns true on success.
bool AssetExtract_BuildFromROM(const uint8 *rom, size_t rom_size,
                               const char *output_path);

// Re-extract with an additional language ROM's dialogue.
// us_rom/lang_rom are raw ROM data. output_path is where to write assets.
bool AssetExtract_AddLanguage(const uint8 *us_rom, size_t us_rom_size,
                              const uint8 *lang_rom, size_t lang_rom_size,
                              const char *output_path);

// Identify a ROM by SHA-1 hash. Returns language code ("us", "fr", "de", etc.)
// or NULL if not recognized.
const char *AssetExtract_IdentifyROM(const uint8 *rom, size_t rom_size);

// Get human-readable name for a recognized ROM, or NULL.
const char *AssetExtract_GetROMName(const uint8 *rom, size_t rom_size);

// Get the last error message.
const char *AssetExtract_GetError(void);

#endif  // ASSET_EXTRACT_H
