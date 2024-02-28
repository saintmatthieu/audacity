#include "FormantFilter.h"
#include "SampleCount.h"
#include "WavFileIO.h"
#include <catch2/catch.hpp>
#include <map>

class CoefficientUpdater
{
public:
   using OnUpdate = std::function<void(
      sampleCount,
      std::array<std::complex<double>, FormantFilter::numFormants>)>;

   virtual void
   Analyze(float const* const* channels, size_t numSamples, OnUpdate) = 0;

   virtual ~CoefficientUpdater() = default;
};

/*!
 * \brief A 1st order filter, either FIR or IIR.
 */
class DynamicFilter
{
public:
   DynamicFilter(size_t numChannels)
       : mChannelStates(numChannels, std::complex<double> { 0 })
       , mBreakpoints { { sampleCount { 0 }, std::complex<double> { 0 } } }
   {
   }

   virtual ~DynamicFilter() = default;

   void Append(sampleCount index, const std::complex<double>& root)
   {
      mBreakpoints.emplace(index, root);
   }

   void Process(std::vector<std::vector<std::complex<double>>>& data)
   {
      const auto numSamples = data[0].size();
      std::vector<std::complex<double>> roots(numSamples);
      for (size_t i = 0; i < numSamples; ++i)
      {
         // Interpolate between breakpoints, or use the last one if we're past
         // the last one.
         const auto next = mBreakpoints.upper_bound(mIndex);
         const auto prev = std::prev(next);
         const auto t = mIndex.as_double();
         const auto t0 = prev->first.as_double();
         const auto t1 = next->first.as_double();
         const auto fraction = (t - t0) / (t1 - t0);
         roots[i] = prev->second * (1 - fraction) + next->second * fraction;
         ++mIndex;
      }
      DoProcess(data, roots);
   }

protected:
   std::vector<std::complex<double>> mChannelStates;

private:
   virtual void DoProcess(
      std::vector<std::vector<std::complex<double>>>& data,
      const std::vector<std::complex<double>>& coefCurve) = 0;

   std::map<sampleCount, std::complex<double>> mBreakpoints;
   sampleCount mIndex = 0;
};

class ForwardDynamicFilter : public DynamicFilter
{
public:
   ForwardDynamicFilter(size_t numChannels)
       : DynamicFilter { numChannels }
   {
   }

private:
   void DoProcess(
      std::vector<std::vector<std::complex<double>>>& data,
      const std::vector<std::complex<double>>& coefCurve) override
   {
      const auto numChannels = mChannelStates.size();
      const auto numSamples = coefCurve.size();
      for (size_t c = 0; c < numChannels; ++c)
      {
         auto& state = mChannelStates[c];
         for (size_t n = 0; n < numSamples; ++n)
         {
            auto& x = data[c][n];
            const auto b1 = coefCurve[n];
            x += b1 * state;
            state = x;
         }
      }
   }
};

TEST_CASE("FormantFilter")
{
   const auto inputPath = "C:/Users/saint/Downloads/ah.wav";
   std::vector<std::vector<float>> input;
   AudioFileInfo info;
   REQUIRE(WavFileIO::Read(inputPath, input, info));

   SECTION("only whitening")
   {
      FormantFilter sut(info.sampleRate, info.numChannels);
      std::vector<float*> channels;
      for (auto& channel : input)
         channels.push_back(channel.data());
      constexpr auto blockSize = 1234u;
      auto read = 0u;
      while (read < info.numFrames)
      {
         const auto toRead = std::min(blockSize, info.numFrames - read);
         sut.Whiten(channels.data(), toRead);
         read += toRead;
         for (auto& channel : channels)
            channel += toRead;
      }
      WavFileIO::Write(
         "C:/Users/saint/Downloads/whitened.wav", input, info.sampleRate);
   }
}
