Rounded boxes are testable milestones, rectangles work items.

```mermaid
flowchart TD
   passthrough_playback_export_and_render([passthrough playback,\nexport & render])
   joint_stereo([joint stereo])
   stretched_playback([stretched playback])
   export_and_render([export & render])
   clip_stretching([clip stretching])
   release([release])
   stretchingtrack[StretchingSequence]
   wavecliptrimandstretchhandle[WaveClipTrimHandle\ngeneralized for stretching]
   joint_stereo_refactoring[joint stereo\nrefactoring]

   %% BIG ONE: resizable_clips entails
   %% * boundary calculations
   %% * sample indexing / time scaling
   %% * clips listen to project_tempo
   %% * envelopes can be stretched
   resizable_clips[resizeable clips]

   caching
   avoid_duplicate_calculations[avoid duplicate calculations]

   joint_stereo_refactoring --> joint_stereo
   caching -- makes work easier -.-> stretchingtrack
   caching -- is dependency to test\nstretching in large projects -.-> stretched_playback

   stretchingtrack -.-> resizable_clips
   resizable_clips --> stretched_playback
   resizable_clips --> export_and_render
   stretchingtrack --> passthrough_playback_export_and_render
   passthrough_playback_export_and_render --> export_and_render
   resizable_clips --> clip_stretching
   wavecliptrimandstretchhandle --> clip_stretching

   joint_stereo --> release
   stretched_playback --> release
   export_and_render --> release
   clip_stretching --> release
```
