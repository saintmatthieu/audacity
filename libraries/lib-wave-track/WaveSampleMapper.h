#pragma once

class SequenceInterface;

#include <memory>
#include <optional>
#include <vector>

class WAVE_TRACK_API WaveSampleMapper /* not final */
{
public:
protected:
   WaveSampleMapper(
      double clipStretchRatio, std::optional<double> rawAudioTempo,
      std::optional<double> projectTempo);

   //! Real-time durations, i.e., stretching the clip modifies these.
   //! @{
   double mSequenceOffset { 0 };
   double mTrimLeft { 0 };
   double mTrimRight { 0 };
   //! @}

   // Used in GetStretchRatio which computes the factor, by which the sample
   // interval is multiplied, to get a realtime duration.
   double mClipStretchRatio = 1.;
   std::optional<double> mRawAudioTempo;
   std::optional<double> mProjectTempo;

   //! Sample rate of the raw audio, i.e., before stretching.
   int mRate;
};
