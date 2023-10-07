#include "ClipAnalysis.h"

#include "ClipInterface.h"
#include "ClipTimingEstimator.h"

namespace ClipAnalysis
{
std::optional<double> GetBpm(const ClipInterface& clip)
{
   const auto playDur = clip.GetPlayEndTime() - clip.GetPlayStartTime();
   if (playDur <= 0)
      return {};

   ClipTimingEstimator transformer { clip };
   const auto numBars = transformer.Process();
   return 4 * numBars * 60 / playDur;
}
} // namespace ClipAnalysis
