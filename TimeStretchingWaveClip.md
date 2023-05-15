# Refactoring of PoC

## Current

### Note
`WaveTrack::GetFloats` is a const function, made possible by having the client pass the `from` and `to` arguments and the rest just being readout of const audio. Time stretching is a stateful operation and must be done at clip level. To keep constness of `GetFloats`, either the time stretching state is passed down from `SampleTrackCache::GetFloats` (still mutable) or, like now, a `const_cast` is made on `WaveClip` is made so as to access a mutable reference to its attached `WaveClipProcessor`.

Since it is guaranteed (?) that access of the members involved in `WaveTrack::GetFloats` and `WaveTrack::Reposition` are not concurrently accessed by `MainThread` and `AudioThread`, const-casting for the purpose of time stretching during playback should be fine. This alleviates the duty on the client to maintain the time-stretching state.

```mermaid
classDiagram
   class WaveClip
   class TimeStretcherInterface
   class SampleTrackCache {
      GetFloats() (mutable)
   }
   class SampleTrack {
      GetFloats(long from, long to) (const, AudioThread)
   }
   class WaveTrack {
      Reposition() (MainThread)
   }
   class WaveClipProcessor {
      Process() (mutable)
   }

   SampleTrackCache *-- SampleTrack: Owns
   SampleTrack <|-- WaveTrack
   WaveTrack *-- WaveClip: 1-M
   WaveClip *.. WaveClipProcessor: attached to get\nmutable WaveClipProcessor
   WaveClipProcessor *-- TimeStretcherInterface

   class libsampletrack["lib-sample-track"] {
      SampleTrackCache
      SampleTrack
   }
   class libtimeandpitch["lib-time-and-pitch"] {
      TimeStretcherInterface
   }
   class libwavetrack["lib-wave-track"] {
      WaveClip
      WaveClipProcessor
      WaveTrack
   }

   libsampletrack <.. libwavetrack
   libtimeandpitch <.. libwavetrack
```

---

## Proposal A

Simplest, but may suffer quality problems : unstretched clip A may transition without a *click* into unstretched clip B, but time-stretching either or both could introduce clicking.

```mermaid
classDiagram
   class SampleTrackCache
   class SampleTrack
   class WaveClipInterface
   class WaveClip
   class StretchingWaveClip
   class WaveTrack
   class WaveClipFactoryInterface
   class StretchingWaveClipFactory

   SampleTrackCache *-- SampleTrack: Owns
   SampleTrack <|-- WaveTrack
   WaveTrack *-- WaveClipInterface: 1-M
   WaveClipInterface <|-- StretchingWaveClip
   WaveClipInterface <|-- WaveClip
   StretchingWaveClip *-- WaveClip
   WaveClipFactoryInterface <|-- StretchingWaveClipFactory
   WaveTrack *-- WaveClipFactoryInterface

   class libsampletrack["lib-sample-track"] {
      SampleTrackCache
      SampleTrack
   }
   class libwavetrack["lib-wave-track"] {
      WaveClip
      WaveClipFactoryInterface
      WaveClipInterface
      WaveTrack
   }
   class libtimestretching["lib-time-stretching"] {
      StretchingWaveClip
      StretchingWaveClipFactory
   }

   libsampletrack <.. libwavetrack
   libwavetrack <.. libtimestretching
```

---

## Proposal B

The `StretchingSampleTrack` proposal, wrapping `WaveTrack` and replacing the `playbackTracks` member of `TransportTracks`. (What are the `otherPlayableTracks`?).

```mermaid
classDiagram
class TransportTracks
class SampleTrack
class StretchingSampleTrack
class WaveTrack {
   GetAllClips()
}

TransportTracks *-- SampleTrack: 1-M
SampleTrack <|-- WaveTrack: implements
SampleTrack <|-- StretchingSampleTrack
StretchingSampleTrack *-- WaveTrack: owns
```