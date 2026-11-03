/*
* Audacity: A Digital Audio Editor
*/
#include "pendingframes.h"

using namespace au::au3audio;

void PendingFrames::reset()
{
    m_ranges.clear();
    m_ranges.reserve(capacity);
}

void PendingFrames::push(Range range)
{
    if (range.frames <= 0) {
        return;
    }
    if (!m_ranges.empty()) {
        auto& last = m_ranges.back();
        const bool contiguous = last.valid == range.valid && (!range.valid || last.start + last.frames == range.start);
        // When full, the positions get approximate rather than allocating on the audio thread
        if (contiguous || m_ranges.size() == capacity) {
            last.frames += range.frames;
            return;
        }
    }
    m_ranges.push_back(range);
}
