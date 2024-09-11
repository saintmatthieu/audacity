## Background

We are considering modifying the playback behavior to improve the user experience. In short, we would like to give a chance for effects with a tail (echo, reverb, etc.) to continue output audio after the playhead stops.

There are several playback-related states and actions users can take that bring from one state to another. This document presents the current and proposed state diagrams to help understand the changes.

There are five events

-  PAUSE
-  PLAY
-  STOP
-  RECORD
-  EDIT

The EDIT event broadly denotes all events that are currently prohibited during playback, such as edits of the track panel, applying effects, creating a new project, etc.

We would have the current six states, plus a new one, Sustained.

-  Stopped
-  Playing
-  Recording
-  Stopped Paused
-  Recording Paused
-  Playing Paused
-  Sustained (new)

In Sustained state, the playhead isn't moving, but the audio continues to play, giving a chance to effects with a tail to continue output audio.

Here's a state diagram of what the playback would be like:

```mermaid
stateDiagram-v2
    [*] --> Stopped

   RecordingPaused: Recording Paused
   StopPaused: Stopped Paused
   PlayingPaused: Playing Paused

   StopPaused --> Stopped: PAUSE
   StopPaused --> Playing: PLAY
   StopPaused --> RecordingPaused: RECORD
   note right of StopPaused
       STOP disabled
       EDIT doesn't change state
   end note

   RecordingPaused --> Recording: PAUSE
   RecordingPaused --> Stopped: STOP<br/>EDIT
   note right of RecordingPaused
       PLAY disabled
       RECORD ineffective
   end note

   PlayingPaused --> Playing: PAUSE<br/>PLAY
   PlayingPaused --> Stopped: STOP<br/>EDIT
   PlayingPaused --> Recording: RECORD

    Stopped --> Playing: PLAY
    Stopped --> Recording: RECORD
    Stopped --> StopPaused: PAUSE
    note right of Stopped
        STOP disabled
        EDIT doesn't change state
    end note

    Playing --> PlayingPaused: PAUSE
    Playing --> Sustained: STOP
   Playing --> Stopped: STOP
    note right of Playing
        PLAY resets playhead
        RECORD & EDIT disabled
    end note

   Recording --> RecordingPaused: PAUSE
   Recording --> Stopped: STOP
   note right of Recording
       PLAY & EDIT disabled
       RECORD ineffective
   end note

    Sustained --> Playing: PLAY
    Sustained --> Stopped: STOP<br/>EDIT
   Sustained --> Recording: RECORD
   Sustained --> StopPaused: PAUSE

```

The only difference to the current state diagram is that the Playing state now transitions to the Sustained state when the user stops the playback. Then, in Sustained state,
* PLAY brings back to the Playing state,
* STOP and EDIT bring to the Stopped state,
* RECORD brings to the Recording state,
* PAUSE is ineffective.
