// Things I'm not ready to throw away just yet, yet whose compilation would require maintenance.

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
