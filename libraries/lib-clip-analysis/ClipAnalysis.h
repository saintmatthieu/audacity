/**********************************************************************

  Audacity: A Digital Audio Editor

  ClipAnalysis.h

  Matthieu Hodgkinson

*******************************************************************/

#pragma once

#include <optional>

class ClipInterface;

namespace ClipAnalysis
{
CLIP_ANALYSIS_API std::optional<double> GetBpm(const ClipInterface& clip);
}
