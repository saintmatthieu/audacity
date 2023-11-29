#include "WavMirAudioSource.h"

#include <MusicInformationRetrieval.h>
#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <numeric>

#if __has_include(<filesystem>)
#   include <filesystem>
#endif

namespace MIR
{
namespace
{
std::vector<std::string> GetWavFilesFromDir(const std::string& dir)
{
// Only compile this if std::filesystem is available.
#if __has_include(<filesystem>)
   std::vector<std::string> files;
   for (const auto& entry : std::filesystem::directory_iterator(dir))
      if (entry.path().extension() == ".wav")
         files.push_back(entry.path().string());
   return files;
#else
#endif
}

std::string GetFilenameStem(const std::string& filename)
{
   const auto lastSlash = filename.find_last_of("/\\");
   const auto lastDot = filename.find_last_of(".");
   if (lastSlash == std::string::npos)
      return filename.substr(0, lastDot);
   else
      return filename.substr(lastSlash + 1, lastDot - lastSlash - 1);
}

void progressBar(int width, int percent)
{
   int progress = (width * percent) / 100;
   std::cout << "[";
   for (int i = 0; i < width; ++i)
      if (i < progress)
         std::cout << "=";
      else
         std::cout << " ";
   std::cout << "] " << std::setw(3) << percent << "%\r";
   std::cout.flush();
}

std::optional<double> GetOctaveError(
   const std::optional<double>& expected, const std::optional<double>& actual)
{
   // This is the the octave-2 error, as introduced in
   // Schreiber, H., et al. (2020). Music Tempo Estimation: Are We Done Yet?
   // Transactions of the International Society for Music Information Retrieval,
   // 3(1), pp. 111–125. DOI: https://doi.org/10.5334/tismir.43
   if (!expected || !actual)
      return {};
   const auto middle = std::log2(*expected / *actual);
   const std::vector<double> candidates { middle - 1, middle, middle + 1 };
   return *std::min_element(
      candidates.begin(), candidates.end(),
      [](const auto& a, const auto& b) { return std::abs(a) < std::abs(b); });
}
} // namespace

TEST_CASE("GetBpmFromFilename")
{
   const std::vector<std::pair<std::string, std::optional<double>>> testCases {
      { "my/path\\foo_-_120BPM_Sticks_-_foo.wav", 120 },
      { "my/path\\foo_-_120bpm_Sticks_-_foo.wav", 120 },
      { "my/path\\foo_-_120xyz_Sticks_-_foo.wav", std::nullopt },
      { "my/path\\foo-120bpm-Sticks_-_foo.wav", 120 },
      { "my/path\\foo-120.wav", 120 },
      { "my/path\\foo_120.wav", 120 },
      { "my/path\\foo_120_.wav", 120 },
      { "my/path\\foo_-_21BPM_Sticks_-_foo.wav", std::nullopt },  // Too low
      { "my/path\\foo_-_500BPM_Sticks_-_foo.wav", std::nullopt }, // Too high
      { "my/path\\Hipness_808_fonky3_89.wav", 89 }
   };
   std::vector<bool> success(testCases.size());
   std::transform(
      testCases.begin(), testCases.end(), success.begin(),
      [](const auto& testCase) {
         return GetBpmFromFilename(testCase.first) == testCase.second;
      });
   REQUIRE(
      std::all_of(success.begin(), success.end(), [](bool b) { return b; }));
}

TEST_CASE("GetBpmAndOffset")
{
   std::optional<std::string> dir;
   dir = "C:/Users/saint/Documents/auto-tempo";
   if (!dir)
      return;

   const auto wavFiles = GetWavFilesFromDir(*dir);
   if (wavFiles.empty())
      // Probably <filesystem> is not available.
      return;

   // Iterate through all files in the directory.
   struct EvalSample
   {
      const std::string filename;
      const std::optional<double> expected;
      const std::optional<double> actual;
      const std::optional<double> o2;
   };
   std::vector<EvalSample> evalSamples;
   constexpr int progressBarWidth = 50;
   auto i = 0;
   auto progress = std::transform(
      wavFiles.begin(), wavFiles.end(), std::back_inserter(evalSamples),
      [&](const auto& wavFile) {
         const auto bpmFromFilename = GetBpmFromFilename(wavFile);
         const WavMirAudioSource source { wavFile };
         std::optional<double> bpm;
         std::optional<double> offset;
         GetBpmAndOffset(source, bpm, offset);
         progressBar(progressBarWidth, 100 * i++ / wavFiles.size());
         // For convenience, add the octave-2 error here, too, not leaving it
         // for computation in the python evaluation script.
         return EvalSample { GetFilenameStem(wavFile), bpmFromFilename, bpm,
                             GetOctaveError(bpmFromFilename, bpm) };
      });

   // Write CSV file with results.
   std::ofstream csvFile { std::string(CMAKE_CURRENT_SOURCE_DIR) +
                           "/GetBpmAndOffset_eval_" + GIT_COMMIT_HASH +
                           ".csv" };
   csvFile << "filename,expected,actual,o2\n";
   std::for_each(
      evalSamples.begin(), evalSamples.end(),
      [&csvFile](const auto& evalSample) {
         csvFile << evalSample.filename << ","
                 << evalSample.expected.value_or(0.) << ","
                 << evalSample.actual.value_or(0.) << ",";
         if (evalSample.o2.has_value())
            csvFile << *evalSample.o2;
         csvFile << "\n";
      });
}

TEST_CASE("Tuning")
{
   const auto wavFiles =
      GetWavFilesFromDir("C:/Users/saint/Documents/auto-tempo");
   struct TuningStep
   {
      const double meanSquareThreshold;
      const double truePositiveRate;
      const double falsePositiveRate;
   };
   std::vector<TuningStep> steps;
   const std::vector<double> thresholds { 0.,  0.0005, 0.005, 0.05,
                                          0.5, 5,      50,    500 };
   constexpr int progressBarWidth = 50;
   const auto numEvals = thresholds.size() * wavFiles.size();
   auto i = 0;
   std::transform(
      thresholds.begin(), thresholds.end(), std::back_inserter(steps),
      [&](double threshold) {
         auto truePositives = 0;
         auto falsePositives = 0;
         auto falseNegatives = 0;
         auto trueNegatives = 0;
         for (const auto& wavFile : wavFiles)
         {
            const WavMirAudioSource source { wavFile };
            std::optional<double> bpm;
            std::optional<double> offset;
            const auto expected = GetBpmFromFilename(wavFile).has_value();
            GetBpmAndOffset(source, bpm, offset, threshold);
            const auto actual = bpm.has_value();
            if (expected && actual)
               ++truePositives;
            else if (expected && !actual)
               ++falseNegatives;
            else if (!expected && actual)
               ++falsePositives;
            else if (!expected && !actual)
               ++trueNegatives;
            progressBar(progressBarWidth, 100 * i++ / numEvals);
         }
         return TuningStep { threshold,
                             static_cast<double>(truePositives) /
                                (truePositives + falseNegatives),
                             static_cast<double>(falsePositives) /
                                (falsePositives + trueNegatives) };
      });
   std::ofstream csvFile { std::string(CMAKE_CURRENT_SOURCE_DIR) + "/ROC_" +
                           GIT_COMMIT_HASH + ".csv" };
   csvFile << "threshold,TPR,FPR\n";
   std::for_each(steps.begin(), steps.end(), [&csvFile](const auto& step) {
      csvFile << step.meanSquareThreshold << "," << step.truePositiveRate << ","
              << step.falsePositiveRate << "\n";
   });
}

TEST_CASE("GetBpmAndOffsetOnce")
{
   // const auto filename =
   //    "C:/Users/saint/Documents/auto-tempo/Big_Band_Drums_-_210BPM_Sticks_-_Krupa_Toms_-_Quiet_with_Crash.wav";
   const auto filename =
      "C:/Users/saint/Documents/auto-tempo/Big_Band_Drums_-_120BPM_-_Brushes_-_Short_Fill_4.wav";
   std::optional<double> bpm;
   std::optional<double> offset;
   const WavMirAudioSource source { filename };
   GetBpmAndOffset(source, bpm, offset);
}

TEST_CASE("GetKey")
{
   const auto filename =
      "C:/Users/saint/Documents/auto-tempo/Club_Baritone_136_Gm.wav";
   const WavMirAudioSource source { filename };
   GetKey(source);
}

} // namespace MIR
