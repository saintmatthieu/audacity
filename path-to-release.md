
What to do in which order:
1. `stretchingtrack`: then `passthrough_EXPORT_AND_RENDER` can be tested
2. `resizeable_clips`: clips are resized IF SYNC_CLIPS_TO_PROJECT_TEMPO toggle is defined (must NOT be accessible to users at that stage)
3. `caching`: stretched playback doesn't suffer hick-ups anymore

```mermaid
classDiagram
note "uppercase are FEATURES,\nlower case are work items"

%% features
class passthrough_PLAYBACK
class passthrough_EXPORT_AND_RENDER
class JOINT_STEREO
class STRETCHED_PLAYBACK
class EXPORT_AND_RENDER
class CLIP_STRETCHING

class resizeable_clips {
   boundary calculations
   sample indexing / time scaling
   clips listen to project_tempo
   envelopes can be stretched
}

class caching {
   replacement for SampleTrackCache
   Is MixerSource queue for resampling a problem ?
}

joint_stereo_refactoring --> JOINT_STEREO
trackless_audioio --> passthrough_PLAYBACK
trackless_audioio --> caching
caching --> STRETCHED_PLAYBACK
passthrough_PLAYBACK --> STRETCHED_PLAYBACK
trackless_mixer --> trackless_audioio
class avoid_duplicate_calculations

trackless_mixer --> stretchingtrack
resizeable_clips --> STRETCHED_PLAYBACK
resizeable_clips --> EXPORT_AND_RENDER
stretchingtrack --> passthrough_EXPORT_AND_RENDER
passthrough_EXPORT_AND_RENDER --> EXPORT_AND_RENDER
resizeable_clips --> CLIP_STRETCHING
WaveClipTrimAndStretchHandle --> CLIP_STRETCHING
stretchingtrack --> passthrough_PLAYBACK

JOINT_STEREO --> RELEASE
STRETCHED_PLAYBACK --> RELEASE
EXPORT_AND_RENDER --> RELEASE
CLIP_STRETCHING --> RELEASE

note for WaveClipTrimAndStretchHandle "generalization of Vitaly's\nWaveClipTrimHandle"

note for passthrough_EXPORT_AND_RENDER "both use a Mixer instance and could inject stretchers"

note for trackless_mixer "merge pending"
```
