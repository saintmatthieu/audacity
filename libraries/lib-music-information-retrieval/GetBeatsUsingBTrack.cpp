#include "GetBeatsUsingBTrack.h"
#include "GetVampFeatures.h"
#include "MirAudioSource.h"
#include "MirUtils.h"

#include <BTrack.h>
#include <cassert>
#include <cmath>

namespace MIR
{
namespace GetBeatsUsingBTrack
{
namespace
{
std::pair<int, int> GetHopAndFrameSizes(int sampleRate)
{
   // 512 for sample rate 44.1kHz
   const auto hopSize =
      1 << (9 + (int)std::round(std::log2(sampleRate / 44100.)));
   return { hopSize, 2 * hopSize };
}

class OffsetAudioSource : public MirAudioSource
{
public:
   OffsetAudioSource(const MirAudioSource& source, double offsetTime)
       : mSource { source }
       , offset { static_cast<long long>(
            offsetTime * source.GetSampleRate() + .5) }
   {
   }

   int GetSampleRate() const override
   {
      return mSource.GetSampleRate();
   }

   size_t
   ReadFloats(float* buffer, long long where, size_t numFrames) const override
   {
      return mSource.ReadFloats(buffer, where + offset, numFrames);
   }

private:
   const MirAudioSource& mSource;
   const long long offset;
};
} // namespace

std::optional<BeatInfo> GetBeats(const MirAudioSource& source)
{
   const auto [hopSize, frameSize] =
      GetHopAndFrameSizes(source.GetSampleRate());
   BTrack btrack(hopSize, frameSize);
   long long start = 0;
   std::vector<float> fBuff(frameSize);
   std::vector<double> dBuff(frameSize);
   BeatInfo result;

   while (source.ReadFloats(fBuff.data(), start, frameSize) == frameSize)
   {
      std::copy(fBuff.begin(), fBuff.end(), dBuff.begin());
      btrack.processAudioFrame(dBuff.data());
      if (btrack.beatDueInCurrentFrame())
         result.beatTimes.push_back(
            (start + frameSize / 2) / source.GetSampleRate());
      start += hopSize;
   }

   // BTrack doesn't label beat times with beat numbers out of the box. We must
   // try to do this ourselves.

   // Now that we have precise beat times, we can do a time-synchronized
   // analysis. Let's get a linear fit of the beat times first.
   const auto coefs = GetBeatFittingCoefficients({ result.beatTimes });
   if (!coefs.has_value())
      return result;

   const auto beatOffset = coefs->second;
   const OffsetAudioSource offsetSource { source, beatOffset };
   VampPluginConfig config;

   // Use the power of two below the beat duration, meaning we'll be analyzing
   // at least the first half of each beat. Should be fine, shouldn't it?
   const int beatDurationSmp = coefs->first * source.GetSampleRate() + .5;
   config.blockSize =
      1 << static_cast<int>(std::floor(std::log2(beatDurationSmp)));
   config.stepSize = beatDurationSmp;
   const auto chromaFeatures =
      GetVampFeatures("qm-vamp-plugins:qm-chromagram", offsetSource, config);

   return result;
}
} // namespace GetBeatsUsingBTrack
} // namespace MIR
