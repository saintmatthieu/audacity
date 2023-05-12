#include "WaveTrack.h"
#include "MockedAudio.h"
#include "MockedPrefs.h"
#include "Project.h"
#include "ProjectFileIO.h"
#include "SampleBlock.h"
#include "WaveClip.h"

#include <catch2/catch.hpp>
#include <sndfile.h>

#include <cmath>

namespace
{
std::vector<float> makeSquareWave()
{
   constexpr auto numSamples = 10u;
   std::vector<float> wave(numSamples);
   std::fill(wave.begin(), wave.begin() + numSamples / 2, 0.5f);
   std::fill(wave.begin() + numSamples / 2, wave.end(), -0.5f);
   return wave;
}

std::vector<std::vector<float>> readWavFile(SF_INFO& info)
{
   auto sndfile = sf_open(
      "C:\\Users\\saint\\Downloads\\test\\guitarAndDrums.wav", SFM_READ, &info);
   assert(sndfile);
   if (!sndfile)
   {
      return {};
   }
   std::vector<float> tmp(info.frames * info.channels);
   const auto numReadFrames = sf_readf_float(sndfile, tmp.data(), info.frames);
   sf_close(sndfile);
   assert(numReadFrames == info.frames);
   std::vector<std::vector<float>> audio(info.channels);
   for (auto i = 0; i < info.channels; ++i)
   {
      audio[i].resize(info.frames);
      for (auto ii = 0; ii < info.frames; ++ii)
      {
         audio[i][ii] = tmp[ii * info.channels + i];
      }
   }
   return audio;
}

void writeWavFile(const std::vector<std::vector<float>>& audio, SF_INFO& info)
{
   const auto fname =
      "C:\\Users\\saint\\Downloads\\test\\guitarAndDrums-out.wav";
   auto sndfile = sf_open(fname, SFM_WRITE, &info);
   assert(sndfile);
   if (!sndfile)
   {
      return;
   }
   const auto numFrames = audio[0].size();
   std::vector<float> interleaved(audio.size() * numFrames);
   for (auto i = 0u; i < audio.size(); ++i)
   {
      for (auto ii = 0u; ii < numFrames; ++ii)
      {
         interleaved[ii * audio.size() + i] = audio[i][ii];
      }
   }
   const auto numFramesWritten =
      sf_writef_float(sndfile, interleaved.data(), numFrames);
   assert(numFramesWritten == numFrames);
   sf_close(sndfile);
}

constexpr auto floatFormat = sampleFormat::floatSample;
} // namespace

TEST_CASE("WaveTrack")
{
   MockedAudio mockedAudio;
   MockedPrefs mockedPrefs;
   ProjectFileIO::InitializeSQL();
   SECTION("GetStretched")
   {
      const auto project = AudacityProject::Create();
      auto& projectFileIO = ProjectFileIO::Get(*project);
      projectFileIO.OpenProject();
      const auto factory = SampleBlockFactory::New(*project);
      const auto clip = std::make_shared<WaveClip>(
         factory, floatFormat, 0, 0, std::weak_ptr<TrackList>());
      SF_INFO sfInfo;
      const auto audio = readWavFile(sfInfo);
      const auto numFrames = audio[0].size();
      clip->SetRate(sfInfo.samplerate);
      auto totalNumAppendedSamples = 0LL;
      while (totalNumAppendedSamples < numFrames)
      {
         constexpr auto blockSize = (1 << 20) / sizeof(float);
         const auto numSamplesToAppend =
            std::min(blockSize, numFrames - totalNumAppendedSamples);
         clip->Append(
            reinterpret_cast<const char*>(
               audio[0].data() + totalNumAppendedSamples),
            floatFormat, numSamplesToAppend, 1, floatFormat);
         totalNumAppendedSamples += numSamplesToAppend;
      }
      std::vector<float> output(numFrames);
      auto& processor = clip->GetProcessor();
      auto outputData = output.data();
      processor.Reposition(0.0);
      processor.Process(&outputData, 1u, output.size());
      sfInfo.channels = 1; // Until WaveTracks can be stereo
      writeWavFile({ output }, sfInfo);
      projectFileIO.CloseProject();
   }
}
