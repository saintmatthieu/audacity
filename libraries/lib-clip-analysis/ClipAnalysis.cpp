#include "ClipAnalysis.h"

#include "ClipInterface.h"
#include "SpectrumTransformer.h"

#include <array>

namespace ClipAnalysis
{
namespace
{
constexpr auto fftSize = 4096;

double GetBpmLogLikelihood(double bpm)
{
   constexpr auto mu = 115.;
   constexpr auto alpha = 25.;
   const auto arg = (bpm - mu) / alpha;
   return -0.5 * arg * arg;
}
} // namespace

std::optional<double> GetBpm(const ClipInterface& clip)
{
   const auto playDur = clip.GetPlayEndTime() - clip.GetPlayStartTime();
   if (playDur <= 0)
      return {};

   class ClipSpectrumTransformer : public SpectrumTransformer
   {
      const ClipInterface& mClip;

   public:
      ClipSpectrumTransformer(const ClipInterface& clip)
          : SpectrumTransformer { false,
                                  eWinFuncHann,
                                  eWinFuncRectangular,
                                  fftSize,
                                  2,
                                  true,
                                  true }
          , mClip(clip)
      {
      }

      void DoOutput(const float* outBuffer, size_t mStepSize) override
      {
         assert(false);
      }

      int Process()
      {
         constexpr auto historyLength = 1u;
         Start(historyLength);
         std::array<float, fftSize / 2> prevPhase;
         std::array<float, fftSize / 2> prevPhase2;
         std::array<float, fftSize / 2> prevMagSpec;
         std::fill(prevPhase.begin(), prevPhase.end(), 0.f);
         std::fill(prevPhase2.begin(), prevPhase2.end(), 0.f);
         std::fill(prevMagSpec.begin(), prevMagSpec.end(), 0.f);

         std::array<float, fftSize / 2> phase;
         std::array<float, fftSize / 2> magSpec;

         std::ofstream odf("C:/Users/saint/Downloads/odf.m");
         odf << "odfVal = [";

         auto separator = "";

         std::vector<double> odfVals;
         const auto playDur = mClip.GetPlayEndTime() - mClip.GetPlayStartTime();

         const auto processor = [&](SpectrumTransformer& transformer) {
            auto sum = 0.; // initialise sum to zero

            const Window& win = transformer.Latest();
            const auto& real = win.mRealFFTs;
            const auto& imag = win.mImagFFTs;

            // compute phase values from fft output and sum deviations
            for (auto i = 0u; i < real.size(); ++i)
            {
               // calculate phase value
               phase[i] = std::atan2(imag[i], real[i]);

               // calculate magnitude value
               magSpec[i] =
                  std::sqrt(std::pow(real[i], 2) + std::pow(imag[i], 2));

               // phase deviation
               const auto phaseDeviation =
                  phase[i] - (2 * prevPhase[i]) + prevPhase2[i];

               // calculate magnitude difference (real part of Euclidean
               // distance between complex frames)
               const auto magnitudeDifference = magSpec[i] - prevMagSpec[i];

               // if we have a positive change in magnitude, then include in
               // sum, otherwise ignore (half-wave rectification)
               if (magnitudeDifference > 0)
               {
                  // calculate complex spectral difference for the current
                  // spectral bin
                  const auto csd = sqrt(
                     pow(magSpec[i], 2) + pow(prevMagSpec[i], 2) -
                     2 * magSpec[i] * prevMagSpec[i] * cos(phaseDeviation));

                  // add to sum
                  sum += csd;
               }

               // store values for next calculation
               prevPhase2[i] = prevPhase[i];
               prevPhase[i] = phase[i];
               prevMagSpec[i] = magSpec[i];
            }

            const auto avg = sum * 2 / fftSize;
            odf << separator << avg;
            separator = ", ";

            odfVals.push_back(avg);

            return true;
         };

         bool bLoopSuccess = true;
         sampleCount samplePos = 0;
         const auto numSamples = mClip.GetVisibleSampleCount();
         std::optional<AudioSegmentSampleView> sampleCacheHolder;
         constexpr auto blockSize = 4096u;
         std::array<float, blockSize> buffer;
         while (bLoopSuccess && samplePos < numSamples)
         {
            constexpr auto mayThrow = false;
            sampleCacheHolder.emplace(mClip.GetSampleView(
               0u, samplePos,
               limitSampleBufferSize(blockSize, numSamples - samplePos),
               mayThrow));
            sampleCacheHolder->Copy(buffer.data(), buffer.size());
            const auto numNewSamples = sampleCacheHolder->GetSampleCount();
            bLoopSuccess =
               ProcessSamples(processor, buffer.data(), numNewSamples);
            samplePos += numNewSamples;
         }

         odf << "];";

         Finish(processor);

         constexpr auto maxNumBars = 8u;
         struct Score
         {
            double xcorr;
            double bpm;
         };
         std::vector<Score> scores;
         for (auto numBars = 1u; numBars <= maxNumBars; ++numBars)
         {
            const auto numBeats = numBars * 4;
            auto xcorr = 0.;
            for (auto beat = 1; beat <= numBeats; ++beat)
            {
               const auto offset = beat * odfVals.size() / numBeats;
               auto rotated = odfVals;
               std::rotate(
                  rotated.begin(), rotated.begin() + offset, rotated.end());
               auto k = 0;
               for (auto v : rotated)
                  xcorr += v * odfVals[k++];
            }
            const auto bpm = 4 * 60 * numBars / playDur;
            Score score { 10 * std::log(xcorr / numBeats),
                          GetBpmLogLikelihood(bpm) };
            scores.push_back(score);
         }
         const auto winner = std::distance(
            scores.begin(), std::max_element(
                               scores.begin(), scores.end(),
                               [](const Score& a, const Score& b) {
                                  return a.xcorr + a.bpm < b.xcorr + b.bpm;
                               }));
         const auto numBars = winner + 1;
         return numBars;
      }
   };

   ClipSpectrumTransformer transformer { clip };
   const auto numBars = transformer.Process();
   return 4 * numBars * 60 / playDur;
}
} // namespace ClipAnalysis
