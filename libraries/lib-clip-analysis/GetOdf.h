#pragma once

#include "ODF.h"

#include <vector>

class ClipInterface;

namespace ClipAnalysis
{
CLIP_ANALYSIS_API ODF GetOdf(const ClipInterface& clip);
} // namespace ClipAnalysis
