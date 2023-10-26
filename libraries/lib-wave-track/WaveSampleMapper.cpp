#include "WaveSampleMapper.h"

WaveSampleMapper::WaveSampleMapper(
   double clipStretchRatio, std::optional<double> rawAudioTempo,
   std::optional<double> projectTempo)
    : mClipStretchRatio { clipStretchRatio }
    , mRawAudioTempo { std::move(rawAudioTempo) }
    , mProjectTempo { std::move(projectTempo) }
{
}
