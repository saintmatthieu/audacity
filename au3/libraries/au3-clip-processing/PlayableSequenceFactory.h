/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <memory>
#include <vector>

class ClipInterface;
class PlayableSequence;
using ClipConstHolders = std::vector<std::shared_ptr<const ClipInterface> >;

//! Makes a sequence playable: its clips time-stretched and pitch-shifted, at their places in time, with silence
//! between them
/*!
 * Any range can be read, forward. Time stretching being stateful, reading a clip where its previous read ended is
 * cheaper than elsewhere: see readme.md.
 */
class PlayableSequenceFactory
{
public:
    //! Must be called on the main thread
    static std::shared_ptr<PlayableSequence> Create(const PlayableSequence&, const ClipConstHolders& clips);
};
