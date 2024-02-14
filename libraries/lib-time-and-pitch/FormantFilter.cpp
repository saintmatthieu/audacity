#include "FormantFilter.h"
#include "MirDsp.h"
#include <algorithm>
#include <iterator>

namespace
{
constexpr auto hopDuration = .1;
constexpr auto bufferDuration = 0.02;

std::vector<float> GetWindow(int sampleRate)
{
   constexpr auto twoPi = 3.14159265359f * 2;
   const auto N =
      1 << static_cast<int>(std::log2(bufferDuration * sampleRate) + .5);
   std::vector<float> window(N);
   for (auto i = 0; i < N; ++i)
      window[i] = .5f * (1.f - std::cos(twoPi * i / (N - 1)));
   return window;
}

// Perform Gaussian elimination with partial pivoting to invert a square matrix
std::vector<float> InvertMatrix(const std::vector<float>& matrix, int N)
{
   std::vector<float> invMatrix(N * N, 0.0f);
   std::vector<float> temp(matrix); // Copy of the original matrix

   // Initialize the inverse matrix as the identity matrix
   for (int i = 0; i < N; ++i)
      invMatrix[i * N + i] = 1.0f;

   // Perform Gaussian elimination
   for (int i = 0; i < N; ++i)
   {
      // Find pivot row and swap
      int pivotRow = i;
      for (int j = i + 1; j < N; ++j)
         if (std::abs(temp[j * N + i]) > std::abs(temp[pivotRow * N + i]))
            pivotRow = j;
      if (pivotRow != i)
         // Swap rows
         for (int j = 0; j < N; ++j)
         {
            std::swap(temp[i * N + j], temp[pivotRow * N + j]);
            std::swap(invMatrix[i * N + j], invMatrix[pivotRow * N + j]);
         }

      // Eliminate elements below the diagonal
      for (int j = i + 1; j < N; ++j)
      {
         float factor = temp[j * N + i] / temp[i * N + i];
         for (int k = 0; k < N; ++k)
         {
            temp[j * N + k] -= factor * temp[i * N + k];
            invMatrix[j * N + k] -= factor * invMatrix[i * N + k];
         }
      }
   }

   // Back substitution
   for (int i = N - 1; i > 0; --i)
      for (int j = i - 1; j >= 0; --j)
      {
         float factor = temp[j * N + i] / temp[i * N + i];
         for (int k = 0; k < N; ++k)
         {
            temp[j * N + k] -= factor * temp[i * N + k];
            invMatrix[j * N + k] -= factor * invMatrix[i * N + k];
         }
      }

   // Normalize rows
   for (int i = 0; i < N; ++i)
   {
      float factor = 1.0f / temp[i * N + i];
      for (int j = 0; j < N; ++j)
      {
         temp[i * N + j] *= factor;
         invMatrix[i * N + j] *= factor;
      }
   }

   return invMatrix;
}
} // namespace

FormantFilter::FormantFilter(int sampleRate, size_t numChannels)
    : mNumChannels { numChannels }
    , mHopSize { static_cast<size_t>(sampleRate * hopDuration + .5) }
    , mWindow { GetWindow(sampleRate) }
    , mBuffers(mNumChannels, std::vector<float>(mWindow.size()))
    , mWhiteningStates(mNumChannels)
    , mColoringStates(mNumChannels)
{
   std::fill(mLpcCoefs.begin(), mLpcCoefs.end(), 0.);
   mOldLpcCoefs = mLpcCoefs;
   std::for_each(
      mWhiteningStates.begin(), mWhiteningStates.end(),
      [](auto& state) { std::fill(state.begin(), state.end(), 0.); });
   std::for_each(
      mColoringStates.begin(), mColoringStates.end(),
      [](auto& state) { std::fill(state.begin(), state.end(), 0.); });
}

void FormantFilter::Whiten(float* const* channels, size_t numSamples)
{
   for (auto s = 0; s < numSamples; ++s)
   {
      for (auto c = 0u; c < mNumChannels; ++c)
      {
         mBuffers[c].push_back(channels[c][s]);
         mBuffers[c].erase(mBuffers[c].begin());
      }

      if (mCountSinceHop >= mHopSize)
         UpdateCoefficients();

      std::array<double, numFormants> coefs;
      // Interpolate between the old and new coefs
      for (auto k = 0u; k < numFormants; ++k)
         coefs[k] = mOldLpcCoefs[k] + (mLpcCoefs[k] - mOldLpcCoefs[k]) *
                                         mCountSinceHop / mHopSize;
      ++mCountSinceHop;

      for (auto c = 0u; c < mNumChannels; ++c)
      {
         auto& state = mWhiteningStates[c];
         auto& sample = channels[c][s];
         auto prediction = 0.;
         for (auto k = 0u; k < numFormants; ++k)
            prediction += state[k] * coefs[k];
         std::rotate(state.rbegin(), state.rbegin() + 1, state.rend());
         state[0] = sample;
         sample -= prediction;
      }
   }
}

void FormantFilter::Color(float* const* channels, size_t numSamples)
{
   for (auto i = 0u; i < mNumChannels; ++i)
   {
      auto& state = mColoringStates[i];
      auto channel = channels[i];
      for (auto j = 0u; j < numSamples; ++j)
      {
         double sample = channel[j];
         for (auto k = 0u; k < numFormants; ++k)
            sample -= state[k] * mLpcCoefs[k];
         std::rotate(state.rbegin(), state.rbegin() + 1, state.rend());
         state[0] = sample;
         channel[j] = sample;
      }
   }
}

void FormantFilter::UpdateCoefficients()
{
   std::vector<float> mix { mBuffers[0].begin(),
                            mBuffers[0].begin() + mWindow.size() };
   std::for_each(mBuffers.begin() + 1, mBuffers.end(), [&](auto& buffer) {
      std::transform(
         buffer.begin(), buffer.begin() + mWindow.size(), mix.begin(),
         mix.begin(), std::plus<>());
   });
   auto i = 0;
   std::transform(mix.begin(), mix.end(), mix.begin(), [&](auto sample) {
      return mWindow[i++] * sample / mNumChannels;
   });

   const auto M = mWindow.size() - numFormants;
   const auto N = numFormants;
   std::vector<std::vector<float>> A(M, std::vector<float>(N));
   const std::vector<float> b(mix.begin(), mix.begin() + M);
   // The coefs are found by solving Ax = b, i.e., x = (A^T A)^-1 A^T b.
   for (auto i = 0; i < M; ++i)
      for (auto j = 0; j < N; ++j)
         A[i][j] = mix[i + j + 1];

   std::vector<float> AtA(N * N);
   std::vector<float> Atb(N);
   for (auto i = 0; i < N; ++i)
   {
      for (auto j = 0; j < N; ++j)
      {
         AtA[i * N + j] = 0;
         for (auto k = 0; k < M; ++k)
            AtA[i * N + j] += A[k][i] * A[k][j];
      }
      Atb[i] = 0;
      for (auto k = 0; k < M; ++k)
         Atb[i] += A[k][i] * b[k];
   }

   std::vector<float> AtAInv(N * N);
   AtAInv = InvertMatrix(AtA, N);

   mOldLpcCoefs = mLpcCoefs;
   std::fill(mLpcCoefs.begin(), mLpcCoefs.end(), 0);
   for (auto i = 0; i < N; ++i)
      for (auto j = 0; j < N; ++j)
         mLpcCoefs[i] += AtAInv[i * N + j] * Atb[j];

   mCountSinceHop = 0;
}
