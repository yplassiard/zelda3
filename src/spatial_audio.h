#ifndef SPATIAL_AUDIO_H
#define SPATIAL_AUDIO_H

#include "types.h"

enum {
  kSpatialCue_Wall,
  kSpatialCue_HoleL,
  kSpatialCue_HoleR,
  kSpatialCue_Enemy,
  kSpatialCue_NPC,
  kSpatialCue_Chest,
  kSpatialCue_Liftable,
  kSpatialCue_Door,
  kSpatialCue_StairUp,
  kSpatialCue_StairDown,
  kSpatialCue_Count,
};

void SpatialAudio_Init(int sample_rate);
void SpatialAudio_Shutdown(void);
void SpatialAudio_Toggle(void);
void SpatialAudio_ScanFrame(void);
void SpatialAudio_MixAudio(int16 *buf, int samples, int channels);
void SpatialAudio_SpeakHealth(void);

#endif  // SPATIAL_AUDIO_H
