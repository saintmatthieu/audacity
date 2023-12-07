#include "WavMirAudioSource.h"

#include <BeatFinder.h>
#include <GetBeatFittingCoefficients.h>
#include <GetBeats.h>
#include <MusicInformationRetrieval.h>
#include <array>
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
TEST_CASE("LinearFit")
{
   struct Foo
   {
      int x = 1;
   };
   const auto foo = std::make_shared<Foo>();
   const auto bar = std::move(foo);
   REQUIRE(foo->x == 1);

   SECTION("A")
   {
      const std::vector<double> x { 1.0, 2.0, 3.0, 4.0, 5.0 };
      const std::vector<double> y { 2.0, 3.0, 4.0, 5.0, 6.0 };
      const std::vector<double> weights { 1.0, 2.0, 1.0, 3.0, 1.0 };
      const auto result = LinearFit(x, y, weights);
      REQUIRE(result.first == 1.0);
      REQUIRE(result.second == 1.0);
   }
   SECTION("B")
   {
      const std::vector<int> x { 1, 2, 3, 4, 5 };
      const std::vector<int> y { 2, 3, 4, 5, 6 };
      const std::vector<double> weights { 1.0, 1.0, 1.0, 1.0, 1.0 };
      const auto result = LinearFit(x, y, weights);
      REQUIRE(result.first == 1.0);
      REQUIRE(result.second == 1.0);
   }
   SECTION("C")
   {
      const std::vector<double> x { 1.0, 2.0, 3.0, 4.0, 5.0 };
      const std::vector<double> y { 2.0, 3.0, 4.0, 5.0, 7.0 };
      const std::vector<double> weights { 1.0, 1.0, 1.0, 1.0, 0.0 };
      const auto result = LinearFit(x, y, weights);
      REQUIRE(result.first == 1.0);
      REQUIRE(result.second == 1.0);
   }
   SECTION("D")
   {
      const std::vector<double> x { 1.0, 2.0, 3.0, 4.0, 5.0 };
      const std::vector<double> y { 2.0, 4.0, 6.0, 8.0, 10.0 };
      const std::vector<double> weights { 1.0, 1.0, 1.0, 1.0, 0.0 };
      const auto result = LinearFit(x, y, weights);
      REQUIRE(result.first == 2.0);
      REQUIRE(result.second == 0.0);
   }
}

struct RocInfo
{
   const double areaUnderCurve;
   const double threshold;
};

template <typename Result>
RocInfo
GetRocInfo(std::vector<Result> results, double allowedFalsePositiveRate = 0.)
{
   std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
      return a.score < b.score;
   });
   const auto numPositives = std::count_if(
      results.begin(), results.end(), [](const auto& s) { return s.truth; });
   const auto numNegatives = results.size() - numPositives;
   std::vector<double> truePositiveRates(results.size() + 1);
   std::vector<double> falsePositiveRates(results.size() + 1);
   for (size_t i = 0; i < results.size(); ++i)
   {
      const auto numTruePositives =
         std::count_if(results.begin(), results.begin() + i, [](const auto& s) {
            return s.truth;
         });
      const auto numFalsePositives =
         std::count_if(results.begin(), results.begin() + i, [](const auto& s) {
            return !s.truth;
         });
      truePositiveRates[i] =
         static_cast<double>(numTruePositives) / numPositives;
      falsePositiveRates[i] =
         static_cast<double>(numFalsePositives) / numNegatives;
   }
   truePositiveRates.back() = 1.;
   falsePositiveRates.back() = 1.;

   // Now compute the area under the curve.
   double auc = 0.;
   for (size_t i = 0; i < results.size(); ++i)
      auc += (truePositiveRates[i + 1] + truePositiveRates[i]) *
             (falsePositiveRates[i + 1] - falsePositiveRates[i]) / 2.;

   // Find the score corresponding to the desired false positive rate.
   const auto it = std::lower_bound(
      falsePositiveRates.begin(), falsePositiveRates.end(),
      allowedFalsePositiveRate);
   const auto index = std::distance(falsePositiveRates.begin(), it);
   const auto threshold = results[index].score;

   return { auc, threshold };
}

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
         const auto beatFinder = BeatFinder::CreateInstance(
            source, BeatTrackingAlgorithm::QueenMaryBarBeatTrack);
         std::optional<double> bpm;
         std::optional<double> offset;
         const auto beatInfo = beatFinder->GetBeats();
         if (beatInfo.has_value())
            GetBpmAndOffset(
               GetNormalizedAutocorrCurvatureRms(
                  beatFinder->GetOnsetDetectionFunction()),
               *beatInfo, bpm, offset);
         ProgressBar(progressBarWidth, 100 * i++ / wavFiles.size());
         // For convenience, add the octave-2 error here, too, not leaving
         // it for computation in the python evaluation script.
         return EvalSample { stem, bpmFromFilename, bpm,
                             GetOctaveError(bpmFromFilename, bpm) };
      });

   // Write CSV file with results.
   std::ofstream csvFile { std::string(CMAKE_CURRENT_SOURCE_DIR) +
                           "/GetBpmAndOffset_eval.csv" };
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

namespace
{
// {
// "beatTimes": [5.05034],
// "indexOfFirstBeat": 0
// }
auto ToString(const std::optional<BeatInfo>& beatInfo)
{
   if (!beatInfo)
      return std::string { "null" };
   auto separator = "";
   std::string beatTimes = "[";
   std::for_each(
      beatInfo->beatTimes.begin(), beatInfo->beatTimes.end(), [&](double time) {
         beatTimes += separator;
         beatTimes += std::to_string(time);
      });
   beatTimes += "]";
   std::string indexOfFirstBeat = "null";
   if (beatInfo->indexOfFirstBeat.has_value())
      indexOfFirstBeat = std::to_string(*beatInfo->indexOfFirstBeat);
   return std::string { "{\n\t\"beatTimes\": " } + beatTimes +
          ",\n\t\"indexOfFirstBeat\": " + indexOfFirstBeat + "\n}";
}

constexpr auto timeLimit = 60;
} // namespace

TEST_CASE("GetRocInfo")
{
   // Yes, we are testing a test function, but an important one.
   struct Sample
   {
      bool truth;
      double score;
   };

   REQUIRE(
      GetRocInfo(std::vector<Sample> { { true, 0. }, { false, 1. } })
         .areaUnderCurve == 1.);
   REQUIRE(
      GetRocInfo(std::vector<Sample> { { false, 1. }, { true, 0. } })
         .areaUnderCurve == 1.);
   REQUIRE(
      GetRocInfo(std::vector<Sample> { { false, 0. }, { true, 1. } })
         .areaUnderCurve == 0.);
}

namespace
{
struct OctaveError
{
   double factor;
   double remainder;
};

OctaveError GetOctaveError(double expected, double actual)
{
   constexpr std::array<double, 5> factors { 1., 2., .5, 3., 1. / 3 };
   std::vector<OctaveError> octaveErrors;
   std::transform(
      factors.begin(), factors.end(), std::back_inserter(octaveErrors),
      [&](double factor) {
         const auto remainder = std::log2(factor * actual / expected);
         return OctaveError { factor, remainder };
      });
   return *std::min_element(
      octaveErrors.begin(), octaveErrors.end(),
      [](const auto& a, const auto& b) {
         return std::abs(a.remainder) < std::abs(b.remainder);
      });
}

void UpdateHash(const MirAudioSource& source, float& hash)
{
   // Sum samples to hash.
   long long start = 0;
   constexpr auto bufferSize = 1024;
   std::vector<float> buffer(bufferSize);
   while (source.ReadFloats(buffer.data(), start, bufferSize) > 0)
   {
      hash += std::accumulate(buffer.begin(), buffer.end(), 0.f);
      start += bufferSize;
   }
}
} // namespace

TEST_CASE("Tuning")
{
   constexpr int progressBarWidth = 50;
   const auto wavFiles =
      GetWavFilesUnderDir("C:/Users/saint/Documents/auto-tempo");
   std::ofstream sampleValueCsv { std::string(CMAKE_CURRENT_SOURCE_DIR) +
                                  "/sampleValues.csv" };
   sampleValueCsv
      << "truth,score,tatumRate,bpm,octaveFactor,octaveError,filename\n";

   // We will want to compute a hash of the audio files used.
   float hash = 0.f;

   struct Sample
   {
      bool truth;
      double score;
      std::optional<OctaveError> octaveError;
   };
   std::vector<Sample> samples;
   const auto numFiles = wavFiles.size();
   auto count = 0;
   std::transform(
      wavFiles.begin(), wavFiles.begin() + numFiles,
      std::back_inserter(samples), [&](const std::string& wavFile) {
         const WavMirAudioSource source { wavFile, timeLimit };
         UpdateHash(source, hash);
         auto odfSr = 0.;
         const auto odf =
            GetOnsetDetectionFunction(source, odfSr, smoothingThreshold);
         const auto audioFileDuration =
            1. * source.GetNumSamples() / source.GetSampleRate();
         // const auto [bpm, confidence] = GetApproximateGcd(odf, odfSr);
         const auto result = Experiment1(odf, odfSr, audioFileDuration);
         ProgressBar(progressBarWidth, 100 * count++ / numFiles);
         const auto expected = GetBpmFromFilename(wavFile);
         const auto truth = expected.has_value();
         const std::optional<OctaveError> error =
            truth ? std::make_optional(GetOctaveError(*expected, result.bpm)) :
                    std::nullopt;
         sampleValueCsv << (truth ? "true" : "false") << "," << result.score
                        << "," << result.tatumRate << "," << result.bpm << ","
                        << (error.has_value() ? error->factor : 0.) << ","
                        << (error.has_value() ? error->remainder : 0.) << ","
                        << wavFile << "\n";
         return Sample { truth, result.score, error };
      });

   // Log ROC curve:
   std::ofstream rocFile { std::string(CMAKE_CURRENT_SOURCE_DIR) + "/ROC.csv" };
   rocFile << "truth,score\n";
   std::for_each(samples.begin(), samples.end(), [&](const auto& sample) {
      rocFile << sample.truth << "," << sample.score << "\n";
   });

   // AUC of ROC curve. Tells how good our loop/not-loop clasifier is.
   const auto auc = GetRocInfo(samples);

   // Get RMS of octave errors. Tells how good the BPM estimation is.
   const auto octaveErrors = std::accumulate(
      samples.begin(), samples.end(), std::vector<double> {},
      [&](std::vector<double> octaveErrors, const Sample& sample) {
         if (sample.octaveError.has_value())
            octaveErrors.push_back(sample.octaveError->remainder);
         return octaveErrors;
      });
   const auto octaveErrorStd = std::sqrt(
      std::accumulate(
         octaveErrors.begin(), octaveErrors.end(), 0.,
         [&](double sum, double octaveError) {
            return sum + octaveError * octaveError;
         }) /
      octaveErrors.size());

   std::ofstream summaryFile { std::string(CMAKE_CURRENT_SOURCE_DIR) +
                               "/summary.txt" };
   summaryFile << "AUC: " << auc.areaUnderCurve << "\n"
               << "Threshold: " << auc.threshold << "\n"
               << "Octave error RMS: " << octaveErrorStd << "\n"
               << "Git commit: " << GIT_COMMIT_HASH << "\n"
               << "Audio file hash: " << hash << "\n"
               << "Audio files:\n";
   // Add list of wav file paths to summary file:
   std::for_each(
      wavFiles.begin(), wavFiles.end(),
      [&](const std::string& wavFile) { summaryFile << wavFile << "\n"; });

   // If this changed, then some non-refactoring code change happened and either
   // they must be rectified or the threshold must be adjusted.
   REQUIRE(auc.threshold == rhythmicClassifierScoreThreshold);
}

TEST_CASE("GetBpmAndOffset_once")
{
   const auto wavFile =
      "C:/Users/saint/Documents/auto-tempo/Muse Hub/Club_BigBassHits_136bpm_Gm.wav";
   const WavMirAudioSource source { wavFile, timeLimit };
   const auto beatFinder = BeatFinder::CreateInstance(
      source, BeatTrackingAlgorithm::QueenMaryBarBeatTrack);
   const auto beatInfo = beatFinder->GetBeats();
   if (beatInfo.has_value() && beatInfo->beatTimes.size() > 1)
   {
      const auto coefs = GetBeatFittingCoefficients(
         beatInfo->beatTimes, beatInfo->indexOfFirstBeat);
      const auto rms = GetBeatFittingErrorRms(coefs, *beatInfo);
      const auto odf = beatFinder->GetOnsetDetectionFunction();
      const auto odfSr = beatFinder->GetOnsetDetectionFunctionSampleRate();
      const auto& beatTimes = beatInfo->beatTimes;
      const auto snr = GetBeatSnr(odf, odfSr, beatTimes);
   }
   const auto odf = beatFinder->GetOnsetDetectionFunction();
   GetNormalizedAutocorrCurvatureRms(odf);

   const auto odfSr = beatFinder->GetOnsetDetectionFunctionSampleRate();
   const ODF odfStruct { odf, odf.size() / odfSr };
   const auto autoCorr = GetNormalizedAutocorrelation(odf);
   const auto beatIndices = GetBeatIndices(odfStruct, autoCorr);
   const auto isLoop = beatIndices.has_value() ?
                          IsLoop(odfStruct, autoCorr, *beatIndices) :
                          false;
}

TEST_CASE("NewStuff")
{
   // const auto wavFile =
   //    "C:/Users/saint/Documents/auto-tempo/iroyinspeech/clips/common_voice_yo_36518280.wav";
   // const auto wavFile =
   // "C:/Users/saint/Downloads/anotherOneBitesTheDust.wav";
   const auto wavFile =
      "C:/Users/saint/Documents/auto-tempo/Muse Hub/Club_BWAAHM_136bpm_Gm.wav";
   const WavMirAudioSource source { wavFile, timeLimit };
   auto odfSr = 0.;
   const auto odf =
      GetOnsetDetectionFunction(source, odfSr, smoothingThreshold);
   std::ofstream ofs("C:/Users/saint/Downloads/log_odf.txt");
   std::for_each(odf.begin(), odf.end(), [&](float x) { ofs << x << ","; });
   ofs << std::endl;
   const auto [gcd, confidence] = GetApproximateGcd(odf, odfSr);
}

TEST_CASE("MoreNewStuff")
{
   // const auto wavFile =
   //    "C:/Users/saint/Documents/auto-tempo/iroyinspeech/clips/common_voice_yo_36518280.wav";
   // const auto wavFile =
   // "C:/Users/saint/Downloads/anotherOneBitesTheDust.wav";
   const auto wavFile =
      "C:/Users/saint/Documents/auto-tempo/Muse Hub/Club_MoogBass_136bpm_Gm.wav";
   const WavMirAudioSource source { wavFile, timeLimit };
   NewStuff(source);
}

TEST_CASE("Experiment1")
{
   // const auto wavFile =
   //    "C:/Users/saint/Documents/auto-tempo/iroyinspeech/clips/common_voice_yo_36520588.wav";
   // const auto wavFile =
   // "C:/Users/saint/Downloads/anotherOneBitesTheDust.wav";
   // const auto wavFile =
   //    "C:/Users/saint/Downloads/Muse Hub/Tambourine_-_130_-_01.wav";
   const auto wavFile =
      "C:/Users/saint/Documents/auto-tempo/looperman/looperman funky drums - 96bpm.wav";
   const WavMirAudioSource source { wavFile, timeLimit };
   double odfSr = 0.;
   const auto odf =
      GetOnsetDetectionFunction(source, odfSr, smoothingThreshold);
   const auto audioFileDuration =
      1. * source.GetNumSamples() / source.GetSampleRate();
   Experiment1(odf, odfSr, audioFileDuration);
   std::ofstream ofs("C:/Users/saint/Downloads/log_odf.txt");
   std::for_each(odf.begin(), odf.end(), [&](float x) { ofs << x << ","; });
   ofs << std::endl;
}

TEST_CASE("GetKey")
{
   const auto filename =
      "C:/Users/saint/Documents/auto-tempo/Muse Hub/Club_Baritone_136_Gm.wav";
   const WavMirAudioSource source { filename };
   GetKey(source);
}
} // namespace MIR
