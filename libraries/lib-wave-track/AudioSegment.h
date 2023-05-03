#pragma once

#include "ClientData.h"

class AudioSegment;

class AudioSegmentProcessor
{
public:
   virtual ~AudioSegmentProcessor() = default;
   virtual size_t Process(
      float* const* buffers, size_t numChannels, size_t samplesPerChannel) = 0;
};

class AudioSegment :
    public ClientData::Site<AudioSegment, AudioSegmentProcessor>
{
public:
   using Processor = ClientData::Site<AudioSegment, AudioSegmentProcessor>;
   virtual AudioSegmentProcessor& GetProcessor() const = 0;
};
