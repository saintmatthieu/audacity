#include "FormantFilter.h"
#include "WavFileIO.h"
#include <catch2/catch.hpp>

TEST_CASE("FormantFilter")
{
   const auto inputPath =
      "C:/Users/saint/Downloads/ah.wav";
   std::vector<std::vector<float>> input;
   AudioFileInfo info;
   REQUIRE(WavFileIO::Read(inputPath, input, info));

   SECTION("only whitening")
   {
      FormantFilter sut(info.sampleRate, info.numChannels);
      std::vector<float*> channels;
      for (auto& channel : input)
         channels.push_back(channel.data());
      constexpr auto blockSize = 1234u;
      auto read = 0u;
      while (read < info.numFrames)
      {
         const auto toRead = std::min(blockSize, info.numFrames - read);
         sut.Whiten(channels.data(), toRead);
         read += toRead;
         for (auto& channel : channels)
            channel += toRead;
      }
      WavFileIO::Write(
         "C:/Users/saint/Downloads/whitened.wav", input, info.sampleRate);
   }

   // SECTION("is transparent")
   // {
   //    FormantFilter sut(info.sampleRate, info.numChannels);
   //    auto output = input;
   //    std::vector<float*> channels;
   //    for (auto& channel : output)
   //       channels.push_back(channel.data());
   //    constexpr auto blockSize = 1234u;
   //    auto read = 0u;
   //    while (read < info.numFrames)
   //    {
   //       const auto toRead = std::min(blockSize, info.numFrames - read);
   //       auto data = channels.data();
   //       sut.Whiten(data, toRead);
   //       sut.Color(data, toRead);
   //       read += toRead;
   //       for (auto& channel : channels)
   //          channel += toRead;
   //    }
   //    // RMS error is less than 0.1%.
   //    auto sum = 0.f;
   //    for (auto i = 0u; i < info.numChannels; ++i)
   //       for (auto j = 0u; j < info.numFrames; ++j)
   //       {
   //          const auto diff = output[i][j] - input[i][j];
   //          sum += diff * diff;
   //       }
   //    const auto rmsError =
   //       std::sqrt(sum / (info.numChannels * info.numFrames));
   //    REQUIRE(rmsError < 0.001);
   // }
}
