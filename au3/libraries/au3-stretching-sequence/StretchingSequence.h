/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  StretchingSequence.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include "au3-mixer/AudioIOSequences.h"

#include <memory>
#include <vector>

class AudioSegmentFactoryInterface;
class ClipInterface;
struct ClipAudioSegment;
using ClipConstHolders = std::vector<std::shared_ptr<const ClipInterface> >;

//! Reads the clips of a sequence, time-stretched and pitch-shifted, and silence between them
/*!
 * Any range can be read, forward or backward. Time stretching being stateful, reading a clip where its previous read
 * ended is cheaper than elsewhere: see readme.md.
 */
class STRETCHING_SEQUENCE_API StretchingSequence final : public PlayableSequence
{
public:
    //! Must be called on the main thread
    static std::shared_ptr<StretchingSequence>
    Create(const PlayableSequence&, const ClipConstHolders& clips);

    //! Must be called on the main thread
    StretchingSequence(const PlayableSequence&, const AudioSegmentFactoryInterface&);
    ~StretchingSequence() override = default;

    // WideSampleSequence
    size_t NChannels() const override;
    float GetChannelVolume(int channel) const override;
    double GetStartTime() const override;
    double GetEndTime() const override;
    double GetRate() const override;
    sampleFormat WidestEffectiveFormat() const override;
    bool HasTrivialEnvelope() const override;
    void GetEnvelopeValues(
        double* buffer, size_t bufferLen, double t0, bool backwards) const override;
    bool DoGet(
        size_t iChannel, size_t nBuffers, const samplePtr buffers[], sampleFormat format, sampleCount start, size_t len, bool backwards,
        fillFormat fill = FillFormat::fillZero, bool mayThrow = true, sampleCount* pNumWithinClips = nullptr) const override;

    // PlayableSequence
    const ChannelGroup* FindChannelGroup() const override;
    bool GetSolo() const override;
    bool GetMute() const override;

    // AudioGraph::Channel
    AudioGraph::ChannelType GetChannelType() const override;

    // class methods
    bool GetFloats(
        float* buffers[], sampleCount start, size_t len, bool backwards) const;

private:
    const PlayableSequence& mSequence;
    const std::vector<ClipAudioSegment> mSegments;
};
