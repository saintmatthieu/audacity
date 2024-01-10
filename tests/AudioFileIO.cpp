#include "AudioFileIO.h"
#include "Mp3FileReader.h"
#include "WavFileIO.h"

bool AudioFileIO::Read(
   const std::string& path, std::vector<std::vector<float>>& audio,
   AudioFileInfo& info, const std::optional<std::chrono::seconds>& upTo)
{
   return WavFileIO::Read(path, audio, info, upTo) ||
          Mp3FileReader::Read(path, audio, info);
}
