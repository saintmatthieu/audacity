#pragma once

#include "ClientData.h"

class AudioSegment;

class AudioSegmentProcessor
{
public:
   virtual ~AudioSegmentProcessor() = default;
   virtual void Process() = 0;
};

class AudioSegment :
    public ClientData::Site<AudioSegment, AudioSegmentProcessor>
{
public:
   using Caches = ClientData::Site<AudioSegment, AudioSegmentProcessor>;
   virtual AudioSegmentProcessor& GetProcessor() const = 0;
};
