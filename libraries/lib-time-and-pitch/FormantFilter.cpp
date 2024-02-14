#include "FormantFilter.h"
#include "MirDsp.h"
#include <algorithm>
#include <cassert>
#include <complex>
#include <iterator>

namespace
{
constexpr auto bufferDuration = 0.1;
constexpr auto overlap = 4;

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
    , mWindow { GetWindow(sampleRate) }
    , mHopSize { mWindow.size() / overlap }
    , mInBuffers(mNumChannels)
    , mOutBuffers(mNumChannels, std::vector<float>(2 * mWindow.size()))
    , mSetup { pffft_new_setup(mWindow.size(), PFFFT_REAL) }
    , mWork { reinterpret_cast<float*>(
         pffft_aligned_malloc(mWindow.size() * sizeof(float))) }
    , mTmp { reinterpret_cast<float*>(
         pffft_aligned_malloc(mWindow.size() * sizeof(float))) }
{
}

FormantFilter::~FormantFilter()
{
   pffft_destroy_setup(mSetup);
   pffft_aligned_free(mWork);
   pffft_aligned_free(mTmp);
}

void FormantFilter::Whiten(float* const* channels, size_t numSamples)
{
   for (auto c = 0u; c < mNumChannels; ++c)
      mInBuffers[c].insert(
         mInBuffers[c].end(), channels[c], channels[c] + numSamples);

   while (mInBuffers[0].size() >= mWindow.size())
      ProcessWindow();

   for (auto c = 0u; c < mNumChannels; ++c)
   {
      auto& buff = mOutBuffers[c];
      std::copy(buff.begin(), buff.begin() + numSamples, channels[c]);
      buff.erase(buff.begin(), buff.begin() + numSamples);
   }
}

void FormantFilter::Color(float* const* channels, size_t numSamples)
{
}

void FormantFilter::ProcessWindow()
{
   std::vector<float> mix { mInBuffers[0].begin(),
                            mInBuffers[0].begin() + mWindow.size() };
   std::for_each(mInBuffers.begin() + 1, mInBuffers.end(), [&](auto& buffer) {
      std::transform(
         buffer.begin(), buffer.begin() + mWindow.size(), mix.begin(),
         mix.begin(), std::plus<>());
   });
   std::transform(
      mix.begin(), mix.end(), mWindow.begin(), mix.begin(),
      std::multiplies<>());

   constexpr auto numFormants = 3;
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

   const std::array<float, numFormants> coefs = [&] {
      std::array<float, numFormants> coefs;
      std::fill(coefs.begin(), coefs.end(), 0.f);
      for (auto i = 0; i < N; ++i)
         for (auto j = 0; j < N; ++j)
            coefs[i] += AtAInv[i * N + j] * Atb[j];
      return coefs;
   }();

   for (auto c = 0u; c < mNumChannels; ++c)
   {
      auto& in = mInBuffers[c];
      std::transform(
         in.begin(), in.begin() + mWindow.size(), mWindow.begin(), mTmp,
         [&](auto a, auto b) { return a * b / mWindow.size(); });
      in.erase(in.begin(), in.begin() + mHopSize);
      pffft_transform_ordered(mSetup, mTmp, mTmp, mWork, PFFFT_FORWARD);
      std::vector<std::complex<float>> spectrum(mWindow.size() / 2 + 1);
      auto specPtr = reinterpret_cast<float*>(spectrum.data());
      std::copy(mTmp, mTmp + mWindow.size(), specPtr);
      // PFFFT stores the nyquist real part in the imaginary part of the first
      // element.
      spectrum.back() = { spectrum[0].imag(), 0.f };
      spectrum[0] = { spectrum[0].real(), 0.f };
      for (auto i = 0u; i < spectrum.size(); ++i)
      {
         const auto omega = 2 * 3.14159265359f * i / mWindow.size();
         std::complex<float> H(1, 0);
         for (auto j = 0u; j < numFormants; ++j)
            H -= coefs[j] * std::exp(std::complex<float>(0, -omega * (j + 1)));
         spectrum[i] *= H;
      }
      spectrum[0] = { spectrum[0].real(), spectrum.back().real() };
      std::copy(specPtr, specPtr + mWindow.size(), mTmp);
      pffft_transform_ordered(mSetup, mTmp, mTmp, mWork, PFFFT_BACKWARD);
      std::transform(
         mTmp, mTmp + mWindow.size(), mWindow.begin(), mTmp,
         std::multiplies<>());
      auto& out = mOutBuffers[c];
      std::fill_n(std::back_inserter(out), mHopSize, 0.f);
      assert(out.size() >= mWindow.size());
      const auto offset = out.size() - static_cast<int>(mWindow.size());
      auto begin = out.data() + offset;
      std::transform(mTmp, mTmp + mWindow.size(), begin, begin, std::plus<>());
   }
}
