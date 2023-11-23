#include "GetBeatsUsingBTrack.h"
#include "AudacityVampUtils.h"
#include "GetVampFeatures.h"
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
   const auto info = AudacityVamp::GetBeatInfo(
      std::bind(
         &MirAudioSource::ReadFloats, &source, std::placeholders::_1,
         std::placeholders::_2, std::placeholders::_3),
      source.GetSampleRate());
   if (!info)
      return {};
   return { { info->beatTimes, info->indexOfFirstBeat } };
}
} // namespace GetBeatsUsingBTrack
} // namespace MIR
