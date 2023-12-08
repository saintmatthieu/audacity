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

namespace
{
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

template <int bufferSize = 1024>
void UpdateHash(const MirAudioSource& source, float& hash)
{
   // Sum samples to hash.
   long long start = 0;
   std::array<float, bufferSize> buffer;
   while (true)
   {
      const auto numSamples =
         std::min<long long>(bufferSize, source.GetNumSamples() - start);
      if (numSamples == 0)
         break;
      source.ReadFloats(buffer.data(), start, numSamples);
      hash += std::accumulate(buffer.begin(), buffer.begin() + numSamples, 0.f);
      start += numSamples;
   }
}
} // namespace

TEST_CASE("UpdateHash")
{
   class SquareWaveSource : public MirAudioSource
   {
      const int period = 8;

      int GetSampleRate() const override
      {
         return 10;
      }
      long long GetNumSamples() const override
      {
         return period * 10;
      }
      void ReadFloats(
         float* buffer, long long where, size_t numFrames) const override
      {
         for (size_t i = 0; i < numFrames; ++i)
            buffer[i] = (where + i) % period < period / 2 ? 1.f : -1.f;
      }
   };

   float hash = 0.f;
   constexpr auto bufferSize = 5;
   UpdateHash<bufferSize>(SquareWaveSource {}, hash);
   REQUIRE(hash == 0.);
}

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

namespace
{
template <typename T>
void PrintPythonVector(
   std::ofstream& ofs, const std::vector<T>& v, const char* name)
{
   ofs << name << " = [";
   std::for_each(v.begin(), v.end(), [&](T x) { ofs << x << ","; });
   ofs << "]\n";
}
} // namespace

TEST_CASE("Experiment1")
{
   // const auto wavFile =
   //    "C:/Users/saint/Documents/auto-tempo/iroyinspeech/clips/common_voice_yo_36520711.wav";
   // const auto wavFile =
   // "C:/Users/saint/Downloads/anotherOneBitesTheDust.wav";
   // const auto wavFile =
   //    "C:/Users/saint/Downloads/Muse Hub/Hipness_Guitar_fonkyLickHigh_89bpm_Em.wav";
   const auto wavFile =
      "C:/Users/saint/Documents/auto-tempo/looperman/looperman-l-5383014-0346325-roofa-can-we-pretend-i-tory-lanez 78bpm.wav";
   // const auto wavFile = "C:/Users/saint/Downloads/short_chirp.wav";
   const WavMirAudioSource source { wavFile, timeLimit };
   double odfSr = 0.;
   OdfDebugInfo odfDebugInfo;
   const auto odf = GetOnsetDetectionFunction(
      source, odfSr, smoothingThreshold, &odfDebugInfo);
   Experiment1DebugOutput debugOutput;
   const auto audioFileDuration =
      1. * source.GetNumSamples() / source.GetSampleRate();
   const Experiment1Result result =
      Experiment1(odf, odfSr, audioFileDuration, &debugOutput);

   std::ofstream debug_output_module { std::string(CMAKE_CURRENT_SOURCE_DIR) +
                                       "/debug_output.py" };
   // Write the content of `debugOutput` to `debug_output.py`.
   debug_output_module << "wavFile = \"" << wavFile << "\"\n";
   debug_output_module << "odfSr = " << odfSr << "\n";
   debug_output_module << "audioFileDuration = " << audioFileDuration << "\n";
   debug_output_module << "score = " << result.score << "\n";
   debug_output_module << "tatumRate = " << result.tatumRate << "\n";
   debug_output_module << "bpm = " << result.bpm << "\n";
   debug_output_module << "lag = " << result.lag << "\n";
   debug_output_module << "odf_peak_indices = [";
   std::for_each(
      debugOutput.odfPeakIndices.begin(), debugOutput.odfPeakIndices.end(),
      [&](int i) { debug_output_module << i << ","; });
   debug_output_module << "]\n";
   debug_output_module << "tatum_scores = {";
   std::for_each(
      debugOutput.tatumScores.begin(), debugOutput.tatumScores.end(),
      [&](const auto& entry) {
         const auto& [numTatums, tatumInfo] = entry;
         debug_output_module << numTatums << ": {";
         debug_output_module << "\"score\":" << tatumInfo.score << ", ";
         debug_output_module << "\"lag\":" << tatumInfo.lag << ", ";
         debug_output_module << "\"tpm\":" << tatumInfo.tpm << ", ";
         debug_output_module << "},";
      });
   debug_output_module << "}\n";
   PrintPythonVector(debug_output_module, odf, "odf");
   PrintPythonVector(debug_output_module, odfDebugInfo.rawOdf, "rawOdf");
   PrintPythonVector(
      debug_output_module, odfDebugInfo.movingAverage, "movingAverage");
   PrintPythonVector(
      debug_output_module, debugOutput.odfAutoCorr, "odf_auto_corr");

   std::ofstream stft_log_module { std::string { CMAKE_CURRENT_SOURCE_DIR } +
                                   "/stft_log.py" };
   stft_log_module << "wavFile = \"" << wavFile << "\"\n";
   stft_log_module << "sampleRate = " << source.GetSampleRate() << "\n";
   stft_log_module << "frameRate = " << odfSr << "\n";
   stft_log_module << "stft = [";
   std::for_each(
      odfDebugInfo.postProcessedStft.begin(),
      odfDebugInfo.postProcessedStft.end(), [&](const auto& row) {
         stft_log_module << "[";
         std::for_each(row.begin(), row.end(), [&](float x) {
            stft_log_module << x << ",";
         });
         stft_log_module << "],";
      });
   stft_log_module << "]\n";
}
} // namespace MIR
