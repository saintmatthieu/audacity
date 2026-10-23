/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  LookaheadSmoother.cpp

**********************************************************************/
#include "LookaheadSmoother.h"

#include <algorithm>
#include <cmath>

StreamProcessor::~StreamProcessor() = default;

LookaheadSmoother::LookaheadSmoother(StreamProcessor& processor, Source source, size_t numChannels, size_t lookahead)
    : mProcessor{processor}
    , mSource{std::move(source)}
    , mLookahead{std::max<size_t>(lookahead, 1)}
    , mRaw(numChannels, std::vector<float>(mLookahead))
    , mOutput(numChannels, std::vector<float>(mLookahead))
    , mReprocessed(numChannels, std::vector<float>(mLookahead))
    , mPointers(numChannels)
{
}

float* const* LookaheadSmoother::Pointers(ChannelBuffers& channelBuffers, size_t offset)
{
    for (size_t c = 0; c < channelBuffers.size(); ++c) {
        mPointers[c] = channelBuffers[c].data() + offset;
    }
    return mPointers.data();
}

void LookaheadSmoother::GetSamples(float* const* buffers, size_t len)
{
    // Allocates only when reads get longer than ever
    for (auto& channel : mRaw) {
        channel.resize(std::max(channel.size(), mLookahead + len));
    }
    for (auto& channel : mOutput) {
        channel.resize(std::max(channel.size(), mLookahead + len));
    }

    const auto changed = mProcessor.UpdateParameters();

    if (!mReadAhead) {
        mSource(Pointers(mRaw, 0), mLookahead);
        for (size_t c = 0; c < mRaw.size(); ++c) {
            std::copy_n(mRaw[c].begin(), mLookahead, mOutput[c].begin());
        }
        mProcessor.Process(Pointers(mOutput, 0), mLookahead, mPosition);
        mReadAhead = true;
    } else if (changed) {
        for (size_t c = 0; c < mRaw.size(); ++c) {
            std::copy_n(mRaw[c].begin(), mLookahead, mReprocessed[c].begin());
        }
        mProcessor.Process(Pointers(mReprocessed, 0), mLookahead, mPosition);
        for (size_t c = 0; c < mOutput.size(); ++c) {
            for (size_t i = 0; i < mLookahead; ++i) {
                const auto weight = static_cast<float>(i + 1) / mLookahead;
                mOutput[c][i] = std::lerp(mOutput[c][i], mReprocessed[c][i], weight);
            }
        }
    }

    // Read further ahead, already with the latest parameters
    mSource(Pointers(mRaw, mLookahead), len);
    for (size_t c = 0; c < mRaw.size(); ++c) {
        std::copy_n(mRaw[c].begin() + mLookahead, len, mOutput[c].begin() + mLookahead);
    }
    mProcessor.Process(Pointers(mOutput, mLookahead), len, mPosition + mLookahead);

    for (size_t c = 0; c < mOutput.size(); ++c) {
        std::copy_n(mOutput[c].begin(), len, buffers[c]);
        // Keep what is now read ahead at the front
        std::copy_n(mRaw[c].begin() + len, mLookahead, mRaw[c].begin());
        std::copy_n(mOutput[c].begin() + len, mLookahead, mOutput[c].begin());
    }
    mPosition += len;
}
