/**********************************************************************

Audacity: A Digital Audio Editor

WideSampleSequence.cpp

Paul Licameli split from SampleFrame.cpp

**********************************************************************/
#include "WideSampleSequence.h"
#include <cmath>

WideSampleSequence::~WideSampleSequence() = default;

const WideSampleSequence& WideSampleSequence::GetDecorated() const
{
   const WideSampleSequence* innermost = this;
   while (const auto newP = innermost->DoGetDecorated())
      innermost = newP;
   return *innermost;
}

const WideSampleSequence* WideSampleSequence::DoGetDecorated() const
{
   return nullptr;
}
