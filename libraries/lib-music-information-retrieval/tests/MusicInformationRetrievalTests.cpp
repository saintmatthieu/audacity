#include "WavMirAudioSource.h"

#include <GetBeats.h>
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
std::vector<std::string> GetWavFilesUnderDir(const char* dir)
{
   // Only compile this if std::filesystem is available.
   std::vector<std::string> files;
#if __has_include(<filesystem>)
   namespace fs = std::filesystem;
   for (const auto& entry : fs::directory_iterator(dir))
      // Ignore directories starting with underscore.
      if (entry.is_directory() && entry.path().filename().string()[0] != '_')
      {
         // Look for a file `audacity-maxNumFiles.txt` in this directory. If it
         // exists and contains a number, only process that many files.
         const auto maxNumFilesFilename =
            entry.path().string() + "/audacity-maxNumFiles.txt";
         std::ifstream maxNumFilesFile { maxNumFilesFilename };
         size_t maxNumFiles = std::numeric_limits<size_t>::max();
         if (maxNumFilesFile.good())
         {
            std::string line;
            std::getline(maxNumFilesFile, line);
            maxNumFiles = std::stoi(line);
         }
         // Collect up to `maxNumFiles` wav files in this directory.
         size_t numFiles = 0;
         for (const auto& subEntry : fs::recursive_directory_iterator(entry))
            if (
               subEntry.is_regular_file() &&
               subEntry.path().extension() == ".wav")
            {
               files.push_back(subEntry.path().string());
               if (++numFiles >= maxNumFiles)
                  break;
            }
      }
#endif
   return files;
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

void ProgressBar(int width, int percent)
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
      { "my/path\\Hipness_808_fonky3_89.wav", 89 },
      { "Fantasy Op. 49 in F minor.wav", std::nullopt },
      { "Cymatics - Cyclone Top Drum Loop 3 - 174 BPM", 174 },
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
   std::optional<const char*> dir;
   dir = "C:/Users/saint/Documents/auto-tempo";
   if (!dir)
      return;

   const auto wavFiles = GetWavFilesUnderDir(*dir);
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
         const auto stem = GetFilenameStem(wavFile);
         const auto bpmFromFilename = GetBpmFromFilename(wavFile);
         const WavMirAudioSource source { wavFile };
         std::optional<double> bpm;
         std::optional<double> offset;
         const auto beatInfo =
            GetBeats(BeatTrackingAlgorithm::QueenMaryBarBeatTrack, source);
         if (beatInfo.has_value())
            GetBpmAndOffset(source, *beatInfo, bpm, offset);
         ProgressBar(progressBarWidth, 100 * i++ / wavFiles.size());
         // For convenience, add the octave-2 error here, too, not leaving
         // it for computation in the python evaluation script.
         return EvalSample { stem, bpmFromFilename, bpm,
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
   constexpr int progressBarWidth = 50;
   const auto wavFiles =
      GetWavFilesUnderDir("C:/Users/saint/Documents/auto-tempo");
   std::vector<std::optional<BeatInfo>> beatInfos;
   const auto numFiles = wavFiles.size();
   auto count = 0;
   std::cout << "Computing beats...\n";
   std::transform(
      wavFiles.begin(), wavFiles.begin() + numFiles,
      std::back_inserter(beatInfos), [&](const std::string& wavFile) {
         const WavMirAudioSource source { wavFile };
         const auto beatInfo =
            GetBeats(BeatTrackingAlgorithm::QueenMaryBarBeatTrack, source);
         ProgressBar(progressBarWidth, 100 * count++ / numFiles);
         return beatInfo;
      });
   struct TuningStep
   {
      const double isRhythmicThreshold;
      const double lineFittingThreshold;
      const double truePositiveRate;
      const double falsePositiveRate;
   };

   const std::vector<double> isRhythmicThresholds {
      0., 6e+3, 6e+6, 6e+9, std::numeric_limits<double>::max(),
   };

   const std::vector<double> lineFittingThresholds {
      0.,
      0.0005,
      // 0.0025,
      // 0.005,
      // 0.05,
      // 0.25,
      // 0.5,
      // 1,
      // 2,
      // 3,
      // 4,
      // 5,
      std::numeric_limits<double>::max(),
   };

   const auto numEvals = lineFittingThresholds.size() *
                         isRhythmicThresholds.size() * beatInfos.size();
   count = 0;
   std::cout << "Evaluating...\n";
   std::vector<TuningStep> steps;
   std::for_each(
      isRhythmicThresholds.begin(), isRhythmicThresholds.end(),
      [&](double isRhythmicThreshold) {
         std::transform(
            lineFittingThresholds.begin(), lineFittingThresholds.end(),
            std::back_inserter(steps), [&](double lineFittingThreshold) {
               std::vector<std::string> truePositives;
               std::vector<std::string> falsePositives;
               std::vector<std::string> falseNegatives;
               std::vector<std::string> trueNegatives;
               auto i = 0;
               assert(beatInfos.size() == wavFiles.size());
               std::for_each(
                  beatInfos.begin(), beatInfos.end(),
                  [&](const std::optional<BeatInfo>& beatInfo) {
                     auto actual = false;
                     const auto& wavFile = wavFiles[i++];
                     if (beatInfo.has_value())
                     {
                        std::optional<double> bpm;
                        std::optional<double> offset;
                        const WavMirAudioSource source { wavFile };
                        GetBpmAndOffset(
                           source, *beatInfo, bpm, offset,
                           { { lineFittingThreshold, isRhythmicThreshold } });
                        actual = bpm.has_value();
                     }
                     const auto expected =
                        GetBpmFromFilename(wavFile).has_value();
                     if (expected && actual)
                        truePositives.push_back(wavFile);
                     else if (expected && !actual)
                        falseNegatives.push_back(wavFile);
                     else if (!expected && actual)
                        falsePositives.push_back(wavFile);
                     else if (!expected && !actual)
                        trueNegatives.push_back(wavFile);
                     ProgressBar(progressBarWidth, 100 * count++ / numEvals);
                  });
               const double tp = truePositives.size();
               const double fp = falsePositives.size();
               const double fn = falseNegatives.size();
               const double tn = trueNegatives.size();
               return TuningStep { lineFittingThreshold, isRhythmicThreshold,
                                   tp / (tp + fp), fp / (fp + tn) };
            });
      });
   std::ofstream csvFile { std::string(CMAKE_CURRENT_SOURCE_DIR) + "/ROC_" +
                           GIT_COMMIT_HASH + ".csv" };
   csvFile << "isRhythmicThreshold,lineFittingThreshold,TPR,FPR\n";
   std::for_each(
      steps.begin(), steps.end(), [&csvFile](const TuningStep& step) {
         csvFile << step.isRhythmicThreshold << "," << step.lineFittingThreshold
                 << "," << step.truePositiveRate << ","
                 << step.falsePositiveRate << "\n";
      });
}

TEST_CASE("GetBpmAndOffsetOnce")
{
   // const auto filename =
   //    "C:/Users/saint/Documents/auto-tempo/Big_Band_Drums_-_210BPM_Sticks_-_Krupa_Toms_-_Quiet_with_Crash.wav";
   const auto filename =
      "C:/Users/saint/Documents/auto-tempo\\iroyinspeech\\clips\\common_voice_yo_36525942.wav";
   std::optional<double> bpm;
   std::optional<double> offset;
   const WavMirAudioSource source { filename };
   const auto beatInfo =
      GetBeats(BeatTrackingAlgorithm::QueenMaryBarBeatTrack, source);
   if (!beatInfo.has_value())
      return;
   GetBpmAndOffset(source, *beatInfo, bpm, offset);
}

TEST_CASE("GetKey")
{
   const auto filename =
      "C:/Users/saint/Documents/auto-tempo/Muse Hub/Club_Baritone_136_Gm.wav";
   const WavMirAudioSource source { filename };
   GetKey(source);
}

} // namespace MIR
