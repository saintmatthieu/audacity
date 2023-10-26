#pragma once

#include "SampleCount.h"

#include <memory>
#include <vector>

class BlockArray;
class SampleBlock;

// This is an internal data structure!  For advanced use only.
class SeqBlock
{
public:
   using SampleBlockPtr = std::shared_ptr<SampleBlock>;
   SampleBlockPtr sb;
   /// the sample in the global wavetrack that this block starts at.
   sampleCount start;

   SeqBlock()
       : sb {}
       , start(0)
   {
   }

   SeqBlock(const SampleBlockPtr& sb_, sampleCount start_)
       : sb(sb_)
       , start(start_)
   {
   }

   // Construct a SeqBlock with changed start, same file
   SeqBlock Plus(sampleCount delta) const
   {
      return SeqBlock(sb, start + delta);
   }
};

class BlockArray : public std::vector<SeqBlock>
{
};

using BlockPtrArray = std::vector<SeqBlock*>; // non-owning pointers
