#include "StaffPadTimeAndPitch.h"
#include "AudioContainer.h"

#include <cassert>
#include <memory>

namespace
{
constexpr auto maxBlockSize = 512;

std::vector<float*>
GetOffsetBuffer(float* const* buffer, size_t numChannels, size_t offset)
{
   std::vector<float*> offsetBuffer(numChannels);
   for (auto i = 0u; i < numChannels; ++i)
   {
      offsetBuffer[i] = buffer[i] + offset;
   }
   return offsetBuffer;
}
} // namespace

StaffPadTimeAndPitch::StaffPadTimeAndPitch(
   size_t mNumChannels, InputGetter inputGetter, Parameters parameters)
    : mInputGetter(inputGetter)
    , mNumChannels(mNumChannels)
{
   SetParameters(
      parameters.timeRatio.value_or(1.0), parameters.pitchRatio.value_or(1.0));
}

void StaffPadTimeAndPitch::SetParameters(double timeRatio, double pitchRatio)
{
   if (
      _SetRatio(timeRatio, mTimeRatio, false) ||
      _SetRatio(pitchRatio, mPitchRatio, false))
   {
      _BootStretcher();
   }
}

void StaffPadTimeAndPitch::SetTimeRatio(double r)
{
   _SetRatio(r, mTimeRatio, true);
}

void StaffPadTimeAndPitch::SetPitchRatio(double r)
{
   _SetRatio(r, mPitchRatio, true);
}

void StaffPadTimeAndPitch::GetSamples(float* const* output, size_t outputLen)
{
   auto numOutputSamples = 0u;
   while (numOutputSamples < outputLen)
   {
      // Probably missing out on some tail samples in the final call, but
      // nevermind for now.
      auto numRequired = mStretcher->getSamplesToNextHop();
      while (numRequired > 0)
      {
         const auto numSamplesToFeed = std::min(numRequired, maxBlockSize);
         AudioContainer readContainer(numSamplesToFeed, mNumChannels);
         mInputGetter(readContainer.get(), numSamplesToFeed);
         mStretcher->feedAudio(readContainer.get(), numSamplesToFeed);
         numRequired -= numSamplesToFeed;
      }
      auto numOutputSamplesAvailable =
         mStretcher->getNumAvailableOutputSamples();
      assert(numOutputSamplesAvailable > 0);
      while (numOutputSamples < outputLen && numOutputSamplesAvailable > 0)
      {
         const auto numSamplesToGet =
            std::min({ maxBlockSize, numOutputSamplesAvailable,
                       static_cast<int>(outputLen - numOutputSamples) });
         const auto buffer =
            GetOffsetBuffer(output, mNumChannels, numOutputSamples);
         mStretcher->retrieveAudio(buffer.data(), numSamplesToGet);
         numOutputSamplesAvailable -= numSamplesToGet;
         numOutputSamples += numSamplesToGet;
      }
   }
}

bool StaffPadTimeAndPitch::SamplesRemaining() const
{
   return mStretcher != nullptr &&
          mStretcher->getNumAvailableOutputSamples() > 0;
}

bool StaffPadTimeAndPitch::_SetRatio(
   double r, double& member, bool rebootOnSuccess)
{
   assert(r > 0.0);
   if (r <= 0.0)
   {
      return false;
   }
   member = r;
   if (rebootOnSuccess)
   {
      _BootStretcher();
   }
   return true;
}

void StaffPadTimeAndPitch::_BootStretcher()
{
   mStretcher = std::make_unique<staffpad::TimeAndPitch>();
   mStretcher->setup(static_cast<int>(mNumChannels), maxBlockSize);
   mStretcher->setTimeStretchAndPitchFactor(mTimeRatio, mPitchRatio);
   const auto latencySamples = mStretcher->getLatencySamples();
   auto numLeadingSamplesToDiscard =
      static_cast<int>(static_cast<double>(latencySamples) * mTimeRatio + 0.5);
   while (numLeadingSamplesToDiscard > 0)
   {
      auto numRequired = mStretcher->getSamplesToNextHop();
      while (numRequired > 0)
      {
         const auto numSamplesToFeed = std::min(maxBlockSize, numRequired);
         AudioContainer feeder(numSamplesToFeed, mNumChannels);
         mInputGetter(feeder.get(), numSamplesToFeed);
         mStretcher->feedAudio(feeder.get(), numSamplesToFeed);
         numRequired -= numSamplesToFeed;
      }
      const auto totalNumSamplesToRetrieve = std::min(
         mStretcher->getNumAvailableOutputSamples(),
         numLeadingSamplesToDiscard);
      auto totalNumRetrievedSamples = 0;
      while (totalNumRetrievedSamples < totalNumSamplesToRetrieve)
      {
         const auto numSamplesToRetrieve =
            std::min(maxBlockSize, totalNumSamplesToRetrieve);
         AudioContainer retriever(numSamplesToRetrieve, mNumChannels);
         mStretcher->retrieveAudio(retriever.get(), numSamplesToRetrieve);
         totalNumRetrievedSamples += numSamplesToRetrieve;
      }
      numLeadingSamplesToDiscard -= static_cast<int>(
         static_cast<double>(totalNumSamplesToRetrieve) / mTimeRatio + 0.5);
   }
}
