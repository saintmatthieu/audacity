#include "GetBeatIndices.h"
#include "ClipAnalysisUtils.h"
#include "ODF.h"

#include <fstream>
#include <numeric>
#include <sstream>

namespace ClipAnalysis
{
namespace
{

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

} // namespace

std::optional<std::vector<size_t>> GetBeatIndices(const ODF& odf)
{
   const auto xcorr = GetNormalizedAutocorr(odf.values);
   constexpr auto logValues = false;
   const auto lagPerSample = odf.duration / xcorr.size();
   constexpr auto minBpm = 60;
   constexpr auto maxBpm = 180;
   const auto minSamps = 60. / maxBpm / lagPerSample;
   const auto maxSamps = 60. / minBpm / lagPerSample;

   if (logValues)
   {
      std::ofstream ofs { "C:/Users/saint/Downloads/test.m" };
      ofs << "clear all, close all" << std::endl;
      ofs << PrintVector(odf.values, "odfVals");
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
   }

   // Get first peak after minLag whose value is larger than the xcorr average:
   const auto avg = std::accumulate(xcorr.begin(), xcorr.end(), 0.) /
                    static_cast<double>(xcorr.size());
   const auto end = xcorr.begin() + static_cast<int>(maxSamps);

   // Guess beat period
   auto P = std::max<size_t>(minSamps, 1);
   for (; P < maxSamps && P < xcorr.size(); ++P)
      if (xcorr[P] > avg && xcorr[P - 1] < xcorr[P] && xcorr[P] > xcorr[P + 1])
         break;
   if (P == maxSamps || P == xcorr.size())
      return {};

   std::vector<size_t> beatIndices { 0u };
   auto prevP = 0;
   auto i = 0;
   while (true)
   {
      // peak index
      auto j =
         i == 0 ?
            P :
            static_cast<size_t>(static_cast<double>(prevP) * (i + 1) / i + .5);
      if (j >= xcorr.size())
         break;
      while (true)
      {
         const auto a = (j - 1) % xcorr.size();
         const auto b = (j + 1) % xcorr.size();
         if (xcorr[a] <= xcorr[j] && xcorr[j] >= xcorr[b])
            break;
         j = xcorr[a] > xcorr[j] ? a : b;
      }
      if (j < prevP)
         break;
      beatIndices.push_back(j);
      prevP = j;
      ++i;
   }
   return beatIndices;
}
} // namespace ClipAnalysis
