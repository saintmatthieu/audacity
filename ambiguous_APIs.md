This document lists the `WaveClip` and `WaveTrack` APIs that now have a potential for ambiguity, i.e., raw sample index/count vs stretched sample index/count.

# WaveClip

`GetPlaySamplesCount()`
Number of raw play samples.
Output must be stretched in `SharesBoundaryWithNextClip()`
LGTM

<br/>
<br/>

`GetPlayStartSample()`
Start sample within rendered track. Used extensively by `WaveTrack`.
Needed usage changes:
* `WithinPlayRegion()`: Find another solution (`GetPlayStartTime` instead?)
* `BeforePlayStartTime()`: Same.
LGTM

<br/>
<br/>

`GetPlayEndSample()`
Also in relation to a rendered track. (Careful, `GetPlayStartSample() + GetPlaySamplesCount() != GetPlayEndSample()`.).
Needed usage changes:
* `WithinPlayRegion()`
* `AfterPlayEndTime()`
LGTM

<br/>
<br/>

`GetSequenceSamplesCount()`
LGTM

<br/>
<br/>

`GetSequenceStartTime()`
`mSequenceOffset`, or `GetPlayStartTime() - mTrimLeft`
No change needed if `mSequenceOffset` and `mTrimLeft` are stretched.
LGTM

<br/>
<br/>

`GetSequenceStartSample()`
Can be removed, since in the only remaining use of it,
```cpp
size_t WaveTrack::GetBestBlockSize(sampleCount s) const
{
   auto bestBlockSize = GetMaxBlockSize();

   for (const auto &clip : mClips)
   {
      auto startSample = clip->GetPlayStartSample();
      auto endSample = clip->GetPlayEndSample();
      if (s >= startSample && s < endSample)
      {
         // ignore extra channels (this function will soon be removed)
         bestBlockSize = clip->GetSequence(0)
            ->GetBestBlockSize(s - clip->GetSequenceStartSample());
         break;
      }
   }

   return bestBlockSize;
}
```
the argument to `->GetBestBlockSize` could be replaced by `s - startSample + clip->GetHiddenLeftSampleCount()`.

<br/>
<br/>

`CountSamples(double t0, double t1)` : used by `WaveTrack::Copy`. I think copying can be done with unstretched data, creating clips copying stretch ratio. In that case `CountSamples` should return raw num. samples.

<br/>
<br/>

`TimeToSequenceSamples(t)`: could be called `AbsoluteTimeToSequenceSamples`. Number of samples from `GetSequenceStartTime()` to `t`.

<br/>
<br/>

`TimeToSamples(double time)`, `SamplesToTime(sampleCount)` : map stretched time to num. samples and back. Uses:
* in `WaveTrack::Disjoin`: Maps time to raw audio sample index.
* in `WaveClip::WithinPlayRegion`, `BeforePlayStartTime`, `AfterPlayStartTime`: Can't be used there anymore anyway
* in `WaveClip::CountSamples(t0, t1)`: should give raw num. of samples from play start time.
* in `TimeToSequenceSamples`: raw num. samples
* ...

<br/>
<br/>

`GetNumSamples()`: total num. sequence samples
* in `UpdateEnvelopeTrackLen()`: output must be stretched
* in `SetRate()`: output must be stretched
* in `GetPlayEndTime()`: output must be stretched

## Can be removed:

`ToSequenceSamples`

<br/>
<br/>

`GetSequenceEndSample()`

<br/>
<br/>

`GetSequenceStartSample()`
Can be removed, since in the only remaining use of it,
```cpp
size_t WaveTrack::GetBestBlockSize(sampleCount s) const
{
   auto bestBlockSize = GetMaxBlockSize();

   for (const auto &clip : mClips)
   {
      auto startSample = clip->GetPlayStartSample();
      auto endSample = clip->GetPlayEndSample();
      if (s >= startSample && s < endSample)
      {
         // ignore extra channels (this function will soon be removed)
         bestBlockSize = clip->GetSequence(0)
            ->GetBestBlockSize(s - clip->GetSequenceStartSample());
         break;
      }
   }

   return bestBlockSize;
}
```
the argument to `->GetBestBlockSize` could be replaced by `s - startSample + clip->GetHiddenLeftSampleCount()`.

# WaveTrack

`GetSequenceSamplesCount`
LGTM

<br/>
<br/>

`Get` and `Set`
Still works out-of-the-box if no stretching whatsoever occurred, which applies at best only in some cases (e.g. benchmarking).
If stretching is applied, it makes little sense. Unstretching clip A would mean modifying `clipB->GetPlayStartTime()`. Should this unwrapping really be done?

<br/>
<br/>

`GetBestBlockSize`
`GetMaxBlockSize`
`GetIdealBlockSize`
These give users of `Get` and `Set` limits for queries. These limits are based on the SQL block size limits, as well as the clip limits themselves.
I think these should now be simplified to ignore SQL block size boundaries, since this part is now managed by caching.
If we cannot get rid of `Get` and `Set` so fast, then I suppose these can be used to index raw clip data through `Get` and `Set`. Clients respect those boundaries, we could leverage this.
I still am unsure, though, if this will work where stretched clips begin overlapping when unstretched. Couldn't then a given sample index possibly map to several clips?

