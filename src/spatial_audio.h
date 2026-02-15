#ifndef SPATIAL_AUDIO_H
#define SPATIAL_AUDIO_H

#include "types.h"

enum {
  kSpatialCue_WallN,     // wall ahead (screen up)
  kSpatialCue_WallS,     // wall behind (screen down)
  kSpatialCue_WallE,     // wall to right
  kSpatialCue_WallW,     // wall to left
  kSpatialCue_HoleL,
  kSpatialCue_HoleR,
  kSpatialCue_Enemy,
  kSpatialCue_NPC,
  kSpatialCue_Chest,
  kSpatialCue_Liftable,
  kSpatialCue_Door,
  kSpatialCue_StairUp,
  kSpatialCue_StairDown,
  kSpatialCue_Ledge,
  kSpatialCue_DeepWater,
  kSpatialCue_Hazard,
  kSpatialCue_Conveyor,
  kSpatialCue_Count,
};

// Cue groups for per-category volume control
enum {
  kCueGroup_Walls,      // N/S/E/W walls
  kCueGroup_Holes,      // Holes L/R
  kCueGroup_Enemy,
  kCueGroup_NPC,
  kCueGroup_Chest,
  kCueGroup_Liftable,
  kCueGroup_Door,
  kCueGroup_Stairs,     // StairUp/StairDown
  kCueGroup_Ledge,
  kCueGroup_DeepWater,
  kCueGroup_Hazard,
  kCueGroup_Conveyor,
  kCueGroup_Terrain,    // Grass/Water/Ice underfoot
  kCueGroup_Combat,     // Sword/Danger/Sonar/Blocked
  kCueGroup_Count,
};

void SpatialAudio_Init(int sample_rate);
void SpatialAudio_Shutdown(void);
void SpatialAudio_Toggle(void);
void SpatialAudio_Enable(void);
bool SpatialAudio_IsEnabled(void);
void SpatialAudio_ScanFrame(void);
void SpatialAudio_MixAudio(int16 *buf, int samples, int channels);
void SpatialAudio_SpeakHealth(void);
void SpatialAudio_SpeakLocation(void);

// Sound legend menu
void SpatialAudio_ToggleLegend(void);
void SpatialAudio_LegendNavigate(int dir);
bool SpatialAudio_IsLegendActive(void);

// Accessibility options menu
void SpatialAudio_ToggleOptions(void);
void SpatialAudio_OptionsNavigate(int dir);
void SpatialAudio_OptionsAdjust(int dir);
void SpatialAudio_OptionsSelect(void);
bool SpatialAudio_IsOptionsActive(void);

// Settings getters/setters for persistence
int SpatialAudio_GetCueGroupVolume(int group);
void SpatialAudio_SetCueGroupVolume(int group, int vol);
int SpatialAudio_GetScanRange(void);
void SpatialAudio_SetScanRange(int range);

// Settings I/O
void SpatialAudio_SaveSettings(void);
void SpatialAudio_LoadSettings(void);

#endif  // SPATIAL_AUDIO_H
