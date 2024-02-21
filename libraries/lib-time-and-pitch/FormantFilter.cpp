#include "FormantFilter.h"
#include "MirDsp.h"
#include <algorithm>
#include <cassert>
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iterator>

using namespace std;
using namespace std::complex_literals;

namespace
{
constexpr auto bufferDuration = .1;
constexpr auto overlap = 4;
constexpr auto lpfCoef = .5; // To be tuned

std::vector<float> GetWindow(int sampleRate)
{
   constexpr auto twoPi = 3.14159265359f * 2;
   const auto N =
      1 << static_cast<int>(std::log2(bufferDuration * sampleRate) + .5);
   // constexpr std::array<float, 4> coefs { 1.f, -15.f / 10, 6.f / 10,
   //                                        -1.f / 10 };
   // constexpr std::array<float, 3> coefs { 1.f, -4.f / 3, 1.f / 3 };
   constexpr std::array<float, 2> coefs { 1.f, -1.f };
   std::vector<float> w(N, 0.f);
   for (auto k = 0; k < coefs.size(); k++)
      for (auto n = 0; n < N; n++)
         w[n] += coefs[k] * std::cos(twoPi * k * n / N);
   return w;
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
   std::for_each(mLpfStates.begin(), mLpfStates.end(), [](auto& state) {
      state = { 0., 0. };
   });
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

namespace
{
void GetEigenValues(
   const std::vector<float>& matrix, int N, std::vector<float>& eigenvalues)
{
   std::vector<float> temp(matrix); // Copy of the original matrix

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
            std::swap(temp[i * N + j], temp[pivotRow * N + j]);

      // Eliminate elements below the diagonal
      for (int j = i + 1; j < N; ++j)
      {
         float factor = temp[j * N + i] / temp[i * N + i];
         for (int k = 0; k < N; ++k)
            temp[j * N + k] -= factor * temp[i * N + k];
      }
   }

   // Back substitution
   for (int i = N - 1; i > 0; --i)
      for (int j = i - 1; j >= 0; --j)
      {
         float factor = temp[j * N + i] / temp[i * N + i];
         for (int k = 0; k < N; ++k)
            temp[j * N + k] -= factor * temp[i * N + k];
      }

   // Extract the eigenvalues
   for (int i = 0; i < N; ++i)
      eigenvalues[i] = temp[i * N + i];
}

// Function to perform QR decomposition
void qrDecomposition(
   vector<vector<double>>& A, vector<vector<double>>& Q,
   vector<vector<double>>& R)
{
   int n = A.size();
   Q.resize(n, std::vector<double>(n, 0.0));
   R.resize(n, std::vector<double>(n, 0.0));

   // Initialize Q as identity matrix
   for (int i = 0; i < n; ++i)
      Q[i][i] = 1.0;

   for (int k = 0; k < n - 1; ++k)
   {
      // Compute householder reflection
      std::vector<double> x(n - k, 0.0);
      double norm_x = 0.0;
      for (int i = k; i < n; ++i)
      {
         x[i - k] = A[i][k];
         norm_x += x[i - k] * x[i - k];
      }
      norm_x = sqrt(norm_x);
      if (x[k] > 0)
         norm_x = -norm_x;

      std::vector<double> v(n - k, 0.0);
      v[0] = x[0] - norm_x;
      double beta = -1.0 / (norm_x * (x[0] - norm_x));

      // Compute R matrix
      double vvT_factor = 2.0 / (beta * beta);
      for (int i = k; i < n; ++i)
      {
         for (int j = k; j < n; ++j)
         {
            R[i][j] -= vvT_factor * v[i - k] * v[j - k];
         }
      }

      // Update A matrix
      for (int j = k; j < n; ++j)
      {
         double dot_product = 0.0;
         for (int i = k; i < n; ++i)
            dot_product += A[i][j] * v[i - k];
         dot_product *= beta;
         for (int i = k; i < n; ++i)
            A[i][j] -= dot_product * v[i - k];
      }

      // Compute Q matrix
      for (int j = 0; j < n; ++j)
      {
         double dot_product = 0.0;
         for (int i = k; i < n; ++i)
            dot_product += Q[i][j] * v[i - k];
         dot_product *= beta;
         for (int i = k; i < n; ++i)
            Q[i][j] -= dot_product * v[i - k];
      }
   }
}

// Function to multiply two matrices
vector<vector<double>>
matrixMultiply(const vector<vector<double>>& A, const vector<vector<double>>& B)
{
   int n = A.size();
   int m = B[0].size();
   int p = B.size();
   vector<vector<double>> result(n, std::vector<double>(m, 0.0));

   for (int i = 0; i < n; ++i)
   {
      for (int j = 0; j < m; ++j)
      {
         for (int k = 0; k < p; ++k)
         {
            result[i][j] += A[i][k] * B[k][j];
         }
      }
   }

   return result;
}

// Function to perform QR iteration
vector<complex<double>>
qrIteration(const vector<vector<double>>& A, int maxIterations = 100)
{
   vector<vector<double>> A_k = A;
   int n = A.size();
   vector<complex<double>> eigenvalues(n);

   for (int k = 0; k < maxIterations; ++k)
   {
      vector<vector<double>> Q, R;
      qrDecomposition(A_k, Q, R);
      A_k = matrixMultiply(R, Q);

      // Check if off-diagonal elements are small
      double offDiagNorm = 0.0;
      for (int i = 0; i < n; ++i)
      {
         for (int j = 0; j < i; ++j)
         {
            offDiagNorm += abs(A_k[i][j]);
         }
      }
      if (offDiagNorm < 1e-6)
      {
         for (int i = 0; i < n; ++i)
         {
            eigenvalues[i] = A_k[i][i];
         }
         return eigenvalues;
      }
   }

   assert(false); // QR iteration did not converge
   return eigenvalues;
}

// Function to find roots of polynomial using QR algorithm
vector<complex<double>> findRoots(const std::vector<double>& coeffs)
{
   const int n = coeffs.size();
   vector<vector<double>> companionMatrix(n, std::vector<double>(n, 0.0));

   // Fill companion matrix
   for (int i = 0; i < n - 1; ++i)
      companionMatrix[i + 1][i] = 1.0;

   for (int i = 0; i < n; ++i)
      companionMatrix[i][n - 1] = -coeffs[i];

   // Perform QR iteration
   vector<complex<double>> roots = qrIteration(companionMatrix);

   return roots;
}
} // namespace

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
      // Found out that the coefs are negated, don't know why.
      std::transform(
         coefs.begin(), coefs.end(), coefs.begin(), [](auto a) { return -a; });
      return coefs;
   }();

   std::vector<double> coefVec(coefs.begin(), coefs.end());
   coefVec.insert(coefVec.begin(), 1.);
   // const auto roots = findRoots(coefVec);

   {
      // Write `coefVec` to 'C:/Users/saint/Downloads/coefs.txt'
      ofstream file("C:/Users/saint/Downloads/coefsIn.txt");
      file.precision(std::numeric_limits<double>::max_digits10);
      for (auto i = 0; i < coefVec.size(); ++i)
         file << coefVec[i] << endl;
   }
   {
      // Execute the python script 'find-roots.py'
      // This will write the roots to 'C:/Users/saint/Downloads/roots.txt'
      const auto retcode = system(
         (std::string {
             "C:/Users/saint/AppData/Local/Programs/Python/python310/python.exe " } +
          TIME_AND_PITCH_SOURCE_DIR + "/find-roots.py")
            .c_str());
      assert(retcode == 0);
   }
   std::array<std::complex<double>, numFormants> roots;
   {
      // Read the roots from 'C:/Users/saint/Downloads/roots.txt'
      ifstream file("C:/Users/saint/Downloads/rootsIn.txt");
      for (auto i = 0; i < numFormants; ++i)
      {
         double real, imag;
         file >> real >> imag;
         roots[i] = { real, imag };
      }
   }
   for (auto i = 0; i < numFormants; ++i)
   {
      roots[i] = roots[i] * (1 - lpfCoef) + mLpfStates[i] * lpfCoef;
      mLpfStates[i] = roots[i];
   }

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
      // PFFFT stores the nyquist real part in the imaginary part of the
      // first element.
      spectrum.back() = { spectrum[0].imag(), 0.f };
      spectrum[0] = { spectrum[0].real(), 0.f };
      // Fundamental frequency in radians per sample
      const auto omega = 2 * 3.14159265359f / mWindow.size();
      for (auto i = 0u; i < spectrum.size(); ++i)
      {
         const auto theta = omega * i;
         const std::complex<double> z1 { std::cos(theta), std::sin(theta) };
         std::complex<float> H(1, 0);
         for (auto j = 0u; j < numFormants; ++j)
            H *= 1. - roots[j] / z1;
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
