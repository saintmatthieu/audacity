/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>

#include "iocallback.h"

namespace au::au4 {
//! Plays fast: the tracks' sequences are read ahead on workers, but the chains render in the device callback
/*!
 * Changes are heard within a block, and the input can be monitored through the effects. What isn't rendered in time is
 * skipped: a worker that can't keep up resumes ahead of the device.
 */
class FastIoCallback final : public IoCallback
{
public:
    //! Mixes the tracks in parallel if `parallelMix`, where tracktion_graph is built; else one after the other
    FastIoCallback(AudacityProject& project, int token, std::unique_ptr<IDevice> device, bool followsPlayRegion,
                   LockFreeQueue<audio::AudioCallbackInfo>& callbackInfos, LockFreeQueue<PlayedRange>& playedRanges,
                   const std::atomic<float>& playbackVolume, bool parallelMix);
    ~FastIoCallback() override;

private:
    const char* name() const override { return "fast"; }
    std::optional<TrackReadAhead> trackReadAhead() const override;
    std::shared_ptr<ISequenceProcessingNode> makeMix(std::vector<std::shared_ptr<ISequenceProcessingNode> > tracks,
                                                     size_t maxFrames) override;
    void onBuilt(std::shared_ptr<ISequenceProcessingNode> top) override { m_top = std::move(top); }
    void startReadingAhead() override;
    void invalidateReadAhead() override;
    int64_t readAheadEnd() const override;
    size_t render(float* const* stereo, int64_t index, size_t frames) override;
    void releaseRendering() override { m_top.reset(); }

    const bool m_parallelMix;
    std::shared_ptr<ISequenceProcessingNode> m_top;
};
}
