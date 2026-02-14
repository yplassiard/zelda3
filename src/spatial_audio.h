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

#endif  // SPATIAL_AUDIO_H
