#include "ClipAnalysisUtils.h"

#include "CircularSampleBuffer.h"
#include "FFT.h"

namespace ClipAnalysis
{
namespace
{
inline double lagrange6(const double (&smp)[6], double t)
{
   auto* y = &smp[2];

   auto ym1_y1 = y[-1] + y[1];
   auto twentyfourth_ym2_y2 = 1.f / 24.f * (y[-2] + y[2]);
   auto c0 = y[0];
   auto c1 = 0.05f * y[-2] - 0.5f * y[-1] - 1.f / 3.f * y[0] + y[1] -
             0.25f * y[2] + 1.f / 30.f * y[3];
   auto c2 = 2.f / 3.f * ym1_y1 - 1.25f * y[0] - twentyfourth_ym2_y2;
   auto c3 = 5.f / 12.f * y[0] - 7.f / 12.f * y[1] + 7.f / 24.f * y[2] -
             1.f / 24.f * (y[-2] + y[-1] + y[3]);
   auto c4 = 0.25f * y[0] - 1.f / 6.f * ym1_y1 + twentyfourth_ym2_y2;
   auto c5 = 1.f / 120.f * (y[3] - y[-2]) + 1.f / 24.f * (y[-1] - y[2]) +
             1.f / 12.f * (y[1] - y[0]);

   // Estrin's scheme
   auto t2 = t * t;
   return (c0 + c1 * t) + (c2 + c3 * t) * t2 + (c4 + c5 * t) * t2 * t2;
}
} // namespace

std::vector<float> GetNormalizedAutocorr(const std::vector<double>& x)
{
   const auto N = x.size();
   const auto M = 1 << static_cast<int>(std::ceil(std::log2(x.size())));
   const double step = 1. * N / M;
   auto samp = 0.;
   std::vector<float> interpOdfVals(M);
   staffpad::audio::CircularSampleBuffer<double> ringBuff;
   ringBuff.setSize(N);
   ringBuff.writeBlock(0, N, x.data());

   for (auto m = 0; m < M; ++m)
   {
      int n = samp;
      int start = n - 6;
      const auto frac = samp - n;
      samp += step;
      double smp[6];
      ringBuff.readBlock(n - 6, 6, smp);
      // interpOdfVals is floats because that's what RealFFT expects.
      interpOdfVals[m] = std::max<float>(0, lagrange6(smp, frac));
   }

   std::vector<float> powSpec(M);
   PowerSpectrum(M, interpOdfVals.data(), powSpec.data());
   // We need the entire power spectrum for the cross-correlation
   std::copy(
      powSpec.begin() + 1, powSpec.begin() + M / 2 - 1, powSpec.rbegin());
   std::vector<float> xcorr(M);
   PowerSpectrum(M, powSpec.data(), xcorr.data());
   const auto max = xcorr[0];
   for (auto i = 0; i < M / 2 + 1; ++i)
      xcorr[i] /= max;
   std::copy(xcorr.begin() + 1, xcorr.begin() + M / 2, xcorr.rbegin());
   return xcorr;
}
} // namespace ClipAnalysis
