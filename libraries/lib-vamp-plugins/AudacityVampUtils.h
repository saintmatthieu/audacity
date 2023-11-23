#pragma once

#include <functional>
#include <optional>
#include <vector>

namespace AudacityVamp
{
struct BeatInfo
{
   std::vector<double> beatTimes;
   std::optional<int> indexOfFirstBeat;
};

using RandomAccessReaderFn =
   std::function<size_t(float* dst, long long where, size_t numFrames)>;

VAMP_PLUGINS_API std::optional<BeatInfo>
GetBeatInfo(const RandomAccessReaderFn&, int sampleRate);
} // namespace AudacityVamp
