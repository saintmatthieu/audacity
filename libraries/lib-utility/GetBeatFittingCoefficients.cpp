#include "GetBeatFittingCoefficients.h"

#include <algorithm> // transform
#include <cassert>
#include <numeric> // iota

std::pair<double, double> GetBeatFittingCoefficients(
   const std::vector<double>& beatTimes,
   const std::optional<int>& indexOfFirstBeat)
{
   assert(beatTimes.size() > 1);
   // Fit a model which assumes constant tempo, and hence a beat time `t_k`
   // at `alpha*(k0+k) + beta`, in least-square sense, where `k0` is the
   // index of the first beat, which we know, and `k \in [0, N)`. That is,
   // find `beta` and `alpha` such that `sum_k (t_k - alpha*(k0+k) - beta)^2`
   // is minimized. In matrix form, this is | k0     1 | | alpha | = | t_0 |
   // | k0+1   1 | | beta  |   | t_1     |
   // | k0+2   1 |             | t_2     |
   // |  ...     |             | ...     |
   // | k0+N-1 1 |             | t_(N-1) |
   // i.e, Ax = b
   // x = (A^T A)^-1 A^T b
   // A^T A = | X Y |
   //         | Y Z |
   // where X = N, Y = N*k0 + N*(N-1)/2, Z = N*(N-1)*(2N-1)/6
   // where X = N, Y = N*(N-1)/2, Z = N*(N-1)*(2N-1)/6
   // and the inverse - call it M - is
   // (A^T A)^-1 = | Z -Y | / d = M / d
   //              | -Y X |
   // where d = XZ - Y^2
   // Now M A^T = | Z  Z-Y  Z-2Y ... Z-(N-1)Y | = W
   //             | -Y X-Y  2X-Y ... (N-1)X-Y |

   // Get index of first beat, which is readily available from the beat
   // tracking algorithm:
   const auto k0 = indexOfFirstBeat.value_or(0);
   const auto N = beatTimes.size();
   const auto X =
      N * k0 * k0 + k0 * N * (N - 1) + N * (N - 1) * (2 * N - 1) / 6.;
   const auto Y = N * k0 + N * (N - 1) / 2.;
   const auto Z = static_cast<double>(N);
   const auto d = X * Z - Y * Y;

   static_assert(std::is_same_v<decltype(Y), const double>);
   static_assert(std::is_same_v<decltype(X), const double>);

   std::vector<int> n(N);
   std::iota(n.begin(), n.end(), 0);
   std::vector<double> W0(N);
   std::vector<double> W1(N);
   std::transform(n.begin(), n.end(), W0.begin(), [Z, Y, k0](int k) {
      return Z * (k0 + k) - Y;
   });
   std::transform(n.begin(), n.end(), W1.begin(), [X, Y, k0](int k) {
      return X - Y * (k0 + k);
   });
   const auto alpha =
      std::inner_product(W0.begin(), W0.end(), beatTimes.begin(), 0.) / d;
   const auto beta =
      std::inner_product(W1.begin(), W1.end(), beatTimes.begin(), 0.) / d;

   return { alpha, beta };
}
