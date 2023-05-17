#pragma once

#include <filesystem>
#include <optional>
#include <vector>

struct WavFileIO
{
   struct Info
   {
      int sampleRate = 0;
      int numChannels = 0;
      int numFrames = 0;
   };

   static bool
   Read(const std::filesystem::path&, std::vector<std::vector<float>>&, Info&);

   static bool Write(
      const std::filesystem::path&, const std::vector<std::vector<float>>&,
      int sampleRate);
};