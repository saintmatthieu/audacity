#include "ClipAnalysisUtils.h"
#include "GetBpmFromOdf.h"

#include <catch2/catch.hpp>

#include <numeric>

namespace
{
// TODO will have to be something more elaborate. Keeping it here for the
// record.
std::vector<size_t> GetBeatIndices(const std::vector<double>& odf)
{
   const auto xcorr = ClipAnalysis::GetNormalizedAutocorr(odf);
   const auto mean =
      std::accumulate(xcorr.begin(), xcorr.end(), 0.) / xcorr.size();
   std::vector<size_t> indices;
   for (auto i = 0u; i < xcorr.size() - 1; ++i)
   {
      const auto h = (i - 1) % xcorr.size();
      const auto j = (i + 1) % xcorr.size();
      if (xcorr[h] <= xcorr[i] && xcorr[i] >= xcorr[j] && xcorr[i] > mean)
         indices.push_back(i);
   }
   std::transform(
      indices.begin(), indices.end(), indices.begin(), [&](size_t i) {
         // TODO we may need to search for largest peak within interval around i
         // ; but what interval should that be ? In the meantime let's try that.
         while (true)
         {
            const auto h = (i - 1) % odf.size();
            const auto j = (i + 1) % odf.size();
            if (odf[h] <= odf[i] && odf[i] >= odf[j])
               return i;
            i = odf[h] > odf[j] ? h : j;
         }
      });
   return indices;
}

const ClipAnalysis::ODF nothingElse {
   { 0.0592116,  0.0368057,  0.0118619,  0.00713629, 0.00688765, 0.00749884,
     0.00594557, 0.00364093, 0.00609268, 0.0030691,  0.00471843, 0.00963024,
     0.0110642,  0.00843918, 0.00483536, 0.00489409, 0.00491847, 0.00446826,
     0.00525965, 0.00384075, 0.00397104, 0.0199283,  0.0257986,  0.0109135,
     0.0161345,  0.0136978,  0.0112196,  0.0130777,  0.00574548, 0.00976361,
     0.008504,   0.0140315,  0.0438434,  0.01883,    0.0149668,  0.015639,
     0.0121092,  0.015423,   0.01394,    0.0125771,  0.011362,   0.00618227,
     0.00963938, 0.0338254,  0.0232921,  0.0165231,  0.0141583,  0.0112063,
     0.00876422, 0.0128253,  0.0106267,  0.0134456,  0.0141509,  0.0165383,
     0.0528113,  0.0168823,  0.0168154,  0.011857,   0.0138141,  0.0106778,
     0.0123727,  0.0108503,  0.00917571, 0.0284215,  0.0795517,  0.0408803,
     0.0184968,  0.0148115,  0.0122303,  0.0107282,  0.00981307, 0.00794695,
     0.00773232, 0.00786283, 0.010179,   0.0185274,  0.0118851,  0.0123312,
     0.00888367, 0.0103451,  0.00876566, 0.00808055, 0.00649639, 0.00576739,
     0.00505951, 0.0233998,  0.027658,   0.0122533,  0.00988819, 0.00686025,
     0.00961066, 0.00918073, 0.0068535,  0.00674812, 0.00902702, 0.00558276,
     0.0337403,  0.0266758,  0.0217005,  0.0201223,  0.0177159,  0.0135496,
     0.0133415,  0.014189,   0.0124369,  0.0119417,  0.0103729,  0.0531878,
     0.0288933,  0.0183242,  0.0181038,  0.0173096,  0.0145352,  0.00973059,
     0.0164872,  0.0147812,  0.0159889,  0.0375471,  0.0440894,  0.0164112,
     0.018136,   0.0172117,  0.011541,   0.0112245,  0.0105222,  0.0116389,
     0.00875201, 0.0107586 },
   5.075,
   {} // TODO can't remember the indices
};

const ClipAnalysis::ODF drums {
   { 1.9842,     0.348138,    0.0317856,  0.142962,    0.0267322,  0.0230012,
     0.0186575,  0.00170355,  1.87967,    0.167126,    0.0180388,  0.0939828,
     0.0558142,  0.00835295,  0.0218427,  0.000860311, 0.339384,   0.190026,
     0.00687679, 0.00686465,  0.0135761,  0.00329505,  0.0035818,  0.000777877,
     0.179587,   0.116665,    0.00367479, 0.00451342,  0.0090899,  0.00252957,
     0.00251503, 0.000497886, 0.306189,   0.2304,      0.00749276, 0.00677238,
     0.0141207,  0.00334203,  0.00365365, 0.00078284,  1.89586,    0.216867,
     0.0260953,  0.102996,    0.0641059,  0.0128345,   0.012916,   0.00176934,
     0.277117,   0.20467,     0.00566965, 0.00687126,  0.0140997,  0.00400404,
     0.00344126, 0.000653325, 1.78737,    0.378403,    0.0346937,  0.111101,
     0.0337092,  1.10146,     0.178378,   0.0174122 },
   3.329,
   { 0, 8, 16, 24, 32, 40, 48, 56 }
};

} // namespace

TEST_CASE("GetBpmFromOdf")
{
   const auto result = ClipAnalysis::GetBpmFromOdf(drums);
}

TEST_CASE("GetBeatIndexPairs2")
{
   SECTION("2 bars of 6/8")
   {
      const auto pairs = ClipAnalysis::GetBeatIndexPairs2(12, 2);
      REQUIRE(pairs.size() == 5);
      REQUIRE(pairs[0] == std::pair<size_t, size_t>(1, 7));
      REQUIRE(pairs[1] == std::pair<size_t, size_t>(2, 8));
      REQUIRE(pairs[2] == std::pair<size_t, size_t>(3, 9));
      REQUIRE(pairs[3] == std::pair<size_t, size_t>(4, 10));
      REQUIRE(pairs[4] == std::pair<size_t, size_t>(5, 11));
   }
}
