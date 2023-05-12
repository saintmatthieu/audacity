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

## Proposal

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
   WaveClip --* StretchingWaveClip
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