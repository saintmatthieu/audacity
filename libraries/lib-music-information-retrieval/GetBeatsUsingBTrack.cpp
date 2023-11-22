#include "GetBeatsUsingBTrack.h"
#include "MirAudioSource.h"

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
} // namespace

std::optional<BeatInfo> GetBeats(const MirAudioSource& source)
{
   const auto [hopSize, frameSize] =
      GetHopAndFrameSizes(source.GetSampleRate());
   BTrack btrack(hopSize, frameSize);
   long long start = 0;
   std::vector<float> fBuff(frameSize);
   std::vector<double> dBuff(frameSize);
   BeatInfo info;
   while (source.ReadFloats(fBuff.data(), start, frameSize) == frameSize)
   {
      std::copy(fBuff.begin(), fBuff.end(), dBuff.begin());
      btrack.processAudioFrame(dBuff.data());
      if (btrack.beatDueInCurrentFrame())
         info.beatTimes.push_back(
            (start + frameSize / 2) / source.GetSampleRate());
      start += hopSize;
   }
   return info;
}
} // namespace GetBeatsUsingBTrack
} // namespace MIR
