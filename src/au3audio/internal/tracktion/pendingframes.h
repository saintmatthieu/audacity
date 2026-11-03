/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace au::au3audio {
//! The timeline positions of the frames still in the graph's latency, oldest first
class PendingFrames
{
public:
    //! `frames` frames from `start`, or past the end of the play range if not `valid`
    struct Range {
        int64_t start = 0;
        int64_t frames = 0;
        bool valid = true;
    };

    void reset();
    void push(Range range);

    //! Calls `visitor(range, offset)` for the oldest `frames` frames
    template<typename Visitor>
    void pop(int64_t frames, const Visitor& visitor)
    {
        int64_t offset = 0;
        while (frames > 0 && !m_ranges.empty()) {
            auto& first = m_ranges.front();
            const auto taken = std::min(frames, first.frames);
            visitor(Range { first.start, taken, first.valid }, offset);
            first.start += taken;
            first.frames -= taken;
            if (first.frames == 0) {
                m_ranges.erase(m_ranges.begin());
            }
            frames -= taken;
            offset += taken;
        }
    }

private:
    static constexpr size_t capacity = 256;
    std::vector<Range> m_ranges;
};
}
