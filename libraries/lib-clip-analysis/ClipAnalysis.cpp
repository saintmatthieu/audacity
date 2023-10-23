#include "ClipAnalysis.h"

#include "ClipAnalysisUtils.h"
#include "ClipInterface.h"
#include "GetBpmFromOdfXcorr.h"
#include "OnsetDetector.h"
#include "Resample.h"

#include <array>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>

namespace
{

double GetBpmLogLikelihood(double bpm)
{
   constexpr auto mu = 115.;
   constexpr auto alpha = 25.;
   const auto arg = (bpm - mu) / alpha;
   return -0.5 * arg * arg;
}

template <typename T>
std::string PrintVector(const T& vector, const std::string& name)
{
   std::stringstream ss;
   ss << name << " = [";
   auto separator = "";
   for (auto val : vector)
   {
      ss << separator << val;
      separator = ", ";
   }
   ss << "];" << std::endl;
   return ss.str();
}

// Assuming that odfVals are onsets of a loop. Then there must be a round number
// of beats. Currently assuming a 4/4 time signature, it just tried a range of
// number of measures and picks the most likely one. Which is ?..
//
// Fact number 1: tempi abide by a distribution that might well be approximated
// with a gaussian PDF. Looking up some numbers on the net, this could have an
// average around 115 bpm and a variance of 25 bpm. This is given a weight in
// the comparison of hypotheses.
//
// Second, in most musical genres, and in any case those we expect to see the
// most in a DAW, a piece is a recursive rhythmic construction: the sections of
// a piece may have common time divisors ; the number of bars in a section is
// often a multiple of 2 ; and then a bar, as time signatures suggest, is
// divisible most often in 4 (4/4), sometimes in 3 (3/4, 6/8) and sometimes
// in 2.
//
// Here we begin simple and assume 4/4. Remains just finding the number of bars.
// The rest is currently an improvisation using the auto correlation of the
// onset detection values, which works halfway, but something more sensible and
// discriminant might be at hand ...

std::optional<double> GetBpm(const std::vector<double>& odfVals, double playDur)
{
   const auto xcorr = ClipAnalysis::GetNormalizedAutocorr(odfVals);
   const int M = 2 * (xcorr.size() - 1);
   const auto lagPerSample = playDur / M;
   constexpr auto minBpm = 60;
   constexpr auto maxBpm = 180;
   const auto minSamps = 60. / maxBpm / lagPerSample;
   const auto maxSamps = 60. / minBpm / lagPerSample;

   std::ofstream ofs { "C:/Users/saint/Downloads/test.m" };
   ofs << "clear all, close all" << std::endl;
   ofs << PrintVector(odfVals, "odfVals");
   ofs << PrintVector(xcorr, "xcorr");
   ofs << "plot(odfVals), grid" << std::endl;
   ofs
      << "hold on, plot((0:length(interpOdfVals)-1)*length(odfVals)/length(interpOdfVals), interpOdfVals, 'r--'), hold off, grid"
      << std::endl;
   ofs << "lagPerSample = " << lagPerSample << ";\n";
   ofs << "minLag = " << minSamps * lagPerSample << ";\n";
   ofs << "maxLag = " << maxSamps * lagPerSample << ";\n";
   ofs << "figure, plot((0:length(xcorr)-1)*lagPerSample, xcorr)\n";
   ofs << "hold on, plot([minLag, minLag], [0, max(xcorr)])\n";
   ofs << "plot([maxLag, maxLag], [0, max(xcorr)]), hold off\n";

   const auto groundTruth = 32;

   const auto winner = 0;
   const auto numBars = winner + 1;

   return ClipAnalysis::GetBpmFromOdfXcorr(xcorr, playDur);
}

class BaseClipInterface : public ClipInterface
{
protected:
   BaseClipInterface(const ClipInterface& clip)
       : mClip { clip }
   {
   }

   int GetRate() const override
   {
      return mClip.GetRate();
   }

   double GetPlayStartTime() const override
   {
      return mClip.GetPlayStartTime();
   }

   double GetPlayEndTime() const override
   {
      return mClip.GetPlayEndTime();
   }

   sampleCount TimeToSamples(double time) const override
   {
      return mClip.TimeToSamples(time);
   }

   double GetStretchRatio() const override
   {
      return mClip.GetStretchRatio();
   }

   size_t GetWidth() const override
   {
      return mClip.GetWidth();
   }

   const ClipInterface& mClip;
};

class ResamplingClipInterface : public BaseClipInterface
{
public:
   ResamplingClipInterface(const ClipInterface& clip, double outSampleRate)
       : BaseClipInterface { clip }
       , mResampleFactor { outSampleRate / clip.GetRate() }
       , mNumSamplesAfterResampling { static_cast<sampleCount>(
            clip.GetVisibleSampleCount().as_double() * mResampleFactor) }
       , mResampler { true, mResampleFactor, mResampleFactor }
   {
   }

   sampleCount GetVisibleSampleCount() const override
   {
      return mNumSamplesAfterResampling;
   }

   AudioSegmentSampleView GetSampleView(
      size_t iChannel, sampleCount start, size_t length,
      bool mayThrow = true) const override
   {
      return const_cast<ResamplingClipInterface*>(this)->MutableGetSampleView(
         iChannel, start, length, mayThrow);
   }

private:
   AudioSegmentSampleView MutableGetSampleView(
      size_t iChannel, sampleCount start, size_t length, bool mayThrow)
   {
      if (!mReadStart.has_value())
         mReadStart = sampleCount { start.as_double() / mResampleFactor };
      constexpr auto blockSize = 1024;
      auto numOutSamples = 0;
      std::array<float, blockSize> buffer;
      while (mOut.size() < length)
      {
         const auto sampleView =
            mClip.GetSampleView(iChannel, *mReadStart, blockSize, mayThrow);
         sampleView.Copy(buffer.data(), blockSize);
         constexpr auto isLast = false; // We're reading a loop - never last.
         const auto [consumed, produced] = mResampler.Process(
            mResampleFactor, buffer.data(), blockSize, isLast, buffer.data(),
            blockSize);
         std::copy(
            buffer.data(), buffer.data() + produced, std::back_inserter(mOut));
         mReadStart = *mReadStart + consumed;
      }
      auto block = std::make_shared<std::vector<float>>(
         mOut.begin(), mOut.begin() + length);
      mOut.erase(mOut.begin(), mOut.begin() + length);
      return { { block }, 0, length };
   }

   const double mResampleFactor;
   const sampleCount mNumSamplesAfterResampling;
   Resample mResampler;
   std::optional<sampleCount> mReadStart;
   std::vector<float> mOut;
};

class CircularClipInterface : public BaseClipInterface
{
public:
   CircularClipInterface(const ClipInterface& clip, int fftSize, int overlap)
       : BaseClipInterface { clip }
       , mFftSize { fftSize }
       , mOverlap { overlap }
   {
   }

   sampleCount GetVisibleSampleCount() const override
   {
      // We assume the clip to be a loop. We have to begin by giving the
      // SpectrumTransformer the last fftSize/2 samples of the clip, and at the
      // end again the first fftSize*(overlap - 1)/overlap samples.
      return mClip.GetVisibleSampleCount() + mFftSize / 2 +
             mFftSize * (mOverlap - 1);
   }

   AudioSegmentSampleView GetSampleView(
      size_t iChannel, sampleCount start, size_t length,
      bool mayThrow = true) const override
   {
      const auto numSamples = mClip.GetVisibleSampleCount();
      auto clipStart = start - mFftSize / 2;
      if (clipStart < 0)
         clipStart += numSamples;
      else if (clipStart >= numSamples)
         clipStart -= numSamples;
      auto clipEnd = std::min(numSamples, clipStart + length);
      auto firstView = mClip.GetSampleView(
         iChannel, clipStart, (clipEnd - clipStart).as_size_t(), mayThrow);
      if (clipEnd - clipStart == length)
         return firstView;
      auto secondView = mClip.GetSampleView(
         iChannel, 0, (length - (clipEnd - clipStart)).as_size_t(), mayThrow);
      // This is not very efficient, but it's just a prototype.
      auto newSamples = std::make_shared<std::vector<float>>();
      newSamples->resize(length);
      firstView.Copy(newSamples->data(), firstView.GetSampleCount());
      secondView.Copy(
         newSamples->data() + firstView.GetSampleCount(),
         secondView.GetSampleCount());
      return AudioSegmentSampleView { { newSamples }, 0, length };
   }

private:
   const int mFftSize;
   const int mOverlap;
};
} // namespace

namespace ClipAnalysis
{
std::optional<double> GetBpm(const ClipInterface& clip)
{
   const auto playDur = clip.GetPlayEndTime() - clip.GetPlayStartTime();
   if (playDur <= 0)
      return {};

   // If `clip` is a loop, we can improve the analysis by treating it as such.
   // We also would like a power-of-two number of analyses, because we'll need
   // to take the auto-correlation of it, which can also be done efficiently
   // with an FFT.
   // 1. The number of analyses is a power of two such that our hop size
   // approximately 10ms.
   const int k = 1 << static_cast<int>(std::round(std::log2(playDur / 0.01)));
   constexpr auto overlap = 2;
   const double fftDur = overlap * playDur / k;
   // To satisfy this `fftDur` as well as the power-of-two constraint, we will
   // need to resample. We don't need much more than 16kHz in the end.
   const int fftSize =
      1 << static_cast<int>(std::ceil(std::log2(fftDur * 16000)));
   const auto resampleRate = static_cast<double>(fftSize) / fftDur;
   const auto resampleFactor = resampleRate / clip.GetRate();
   CircularClipInterface circularClip {
      clip, static_cast<int>(fftSize / resampleFactor + .5), overlap
   };
   ResamplingClipInterface stftClip { circularClip, resampleRate };

   OnsetDetector detector { fftSize };

   constexpr auto historyLength = 1u;
   detector.Start(historyLength);

   bool bLoopSuccess = true;
   sampleCount samplePos = 0;
   const auto numSamples = stftClip.GetVisibleSampleCount();
   std::optional<AudioSegmentSampleView> sampleCacheHolder;
   std::vector<float> buffer(fftSize / overlap);
   const auto processor = std::bind(
      &OnsetDetector::WindowProcessor, &detector, std::placeholders::_1);
   while (bLoopSuccess && samplePos < numSamples)
   {
      constexpr auto mayThrow = false;
      const auto numSamplesToGet =
         limitSampleBufferSize(fftSize / overlap, numSamples - samplePos);
      sampleCacheHolder.emplace(
         stftClip.GetSampleView(0u, samplePos, numSamplesToGet, mayThrow));
      sampleCacheHolder->Copy(buffer.data(), buffer.size());
      const auto numNewSamples = sampleCacheHolder->GetSampleCount();
      bLoopSuccess =
         detector.ProcessSamples(processor, buffer.data(), numNewSamples);
      samplePos += numNewSamples;
   }

   detector.Finish(processor);

   return ::GetBpm(detector.GetOnsetDetectionResults(), playDur);
}
} // namespace ClipAnalysis
