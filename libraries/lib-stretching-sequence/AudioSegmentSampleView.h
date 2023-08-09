/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  @file AudioSegmentSampleView.h
  @brief An audio segment is either a whole clip or the silence between clips.
Views allow shared references to the same memory yet over instance-specific
ranges.

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include "SampleCount.h"

#include <chrono>
#include <fstream>
#include <memory>
#include <vector>

using BlockSampleView = std::shared_ptr<std::vector<float>>;

class STRETCHING_SEQUENCE_API AudioSegmentSampleView final
{
public:
   /**
    * @brief Constructs an `AudioSegmentSampleView` from `BlockSampleView`s.
    * @param blockViews A continuous sequence of samples, segmented in blocks.
    * @param start Index of the first sample in `blockViews` to look at.
    * @param length The number of samples from `start` to look at.
    * @pre `!blockViews.empty()`
    * @pre `start < blockViews[0].size()`
    * @pre length < sum(sizes(blockViews)) - start
    */
   AudioSegmentSampleView(
      std::vector<BlockSampleView> blockViews, size_t start,
      sampleCount length);

   /**
    * @brief Constructs a silent `AudioSegmentSampleView`.
    * @param length The number of "silence samples".
    */
   AudioSegmentSampleView(sampleCount length);

   /**
    * @brief Copies up to `GetSampleCount()` or `bufferSize` samples, whichever
    * is less, into `buffer`.
    * @return The number of samples copied. Samples in `buffer` beyond that are
    * not zeroed.
    */
   size_t Copy(float* buffer, size_t bufferSize) const;

   /**
    * @brief The number of samples in this view.
    */
   sampleCount GetSampleCount() const;

private:
   size_t DoCopy(float* buffer, size_t bufferSize) const;

   const std::vector<BlockSampleView> mBlockViews;
   const size_t mStart = 0;
   const sampleCount mLength;
   const bool mIsSilent;
};

using ChannelSampleView = std::vector<AudioSegmentSampleView>;

void STRETCHING_SEQUENCE_API FillBufferFromTrackBlockSequence(
   const ChannelSampleView& track, float* buffer, size_t bufferSize);
