#include "WavFileIO.h"

#include <cassert>
#include <iostream>
#include <sndfile.h>

namespace fs = std::filesystem;
using namespace std::literals::string_literals;
using Info = WavFileIO::Info;

bool WavFileIO::Read(
   const std::filesystem::path& inputPath,
   std::vector<std::vector<float>>& audio, Info& info)
{
   SF_INFO sfInfo;
   auto sndfile = sf_open(inputPath.string().c_str(), SFM_READ, &sfInfo);
   if (!sndfile)
   {
      std::cerr << "libsndfile could not read "s + inputPath.string()
                << std::endl;
      return false;
   }
   std::vector<float> tmp(sfInfo.frames * sfInfo.channels);
   const auto numReadFrames =
      sf_readf_float(sndfile, tmp.data(), sfInfo.frames);
   sf_close(sndfile);
   assert(numReadFrames == sfInfo.frames);
   audio.resize(sfInfo.channels);
   for (auto i = 0; i < sfInfo.channels; ++i)
   {
      audio[i].resize(sfInfo.frames);
      for (auto ii = 0; ii < sfInfo.frames; ++ii)
      {
         audio[i][ii] = tmp[ii * sfInfo.channels + i];
      }
   }
   info.sampleRate = sfInfo.samplerate;
   info.numChannels = sfInfo.channels;
   info.numFrames = sfInfo.frames;
   return true;
}

bool WavFileIO::Write(
   const std::filesystem::path& outputPath,
   const std::vector<std::vector<float>>& audio, int sampleRate)
{
   const auto numChannels = audio.size();
   const auto numFrames = audio[0].size();
   SF_INFO sfInfo;
   sfInfo.channels = numChannels;
   sfInfo.frames = numFrames;
   sfInfo.samplerate = sampleRate;
   sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
   sfInfo.sections = 1;
   sfInfo.seekable = 1;
   auto sndfile = sf_open(outputPath.string().c_str(), SFM_WRITE, &sfInfo);
   if (!sndfile)
   {
      std::cout << "libsndfile could not open "s + outputPath.string() +
                      " for write."
                << std::endl;
      return false;
   }
   std::vector<float> interleaved(numChannels * numFrames);
   for (auto i = 0u; i < numChannels; ++i)
   {
      for (auto ii = 0u; ii < numFrames; ++ii)
      {
         interleaved[ii * numChannels + i] = audio[i][ii];
      }
   }
   const auto numFramesWritten =
      sf_writef_float(sndfile, interleaved.data(), numFrames);
   assert(numFramesWritten == numFrames);
   sf_close(sndfile);
   return true;
}
