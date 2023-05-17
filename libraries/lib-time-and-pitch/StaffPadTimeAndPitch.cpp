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
   size_t mNumChannels, AudioSource audioSource, Parameters parameters)
    : mAudioSource(audioSource)
    , mNumChannels(mNumChannels)
{
   SetParameters(
      parameters.timeRatio.value_or(1.0), parameters.pitchRatio.value_or(1.0));
}

void StaffPadTimeAndPitch::SetParameters(double timeRatio, double pitchRatio)
{
   const auto timeSet = _SetRatio(timeRatio, mTimeRatio, false);
   const auto pitchSet = _SetRatio(pitchRatio, mPitchRatio, false);
   if (timeSet || pitchSet)
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
   if (!mTimeAndPitch)
   {
      mAudioSource(output, outputLen);
      return;
   }
   auto numOutputSamples = 0u;
   AudioContainer readBuffer(maxBlockSize, mNumChannels);
   while (numOutputSamples < outputLen)
   {
      // One would expect that feeding `getSamplesToNextHop()` samples would
      // always bring mTimeAndPitch to return `getNumAvailableOutputSamples() >
      // 0`, right ? With pitch shifting I saw the opposite happening. Brief
      // debugging hinted that `getSamplesToNextHop` might indeed be yielding an
      // underestimation. In that case repeating the operation once should
      // produce the expected result, but let's not write and infinite loop for
      // that until we know for sure what the problem is.
      // todo(mhodgkinson) figure this out
      auto firstCall = true;
      auto numOutputSamplesAvailable = 0;
      while (numOutputSamplesAvailable == 0)
      {
         auto numRequired = mTimeAndPitch->getSamplesToNextHop();
         while (numRequired > 0)
         {
            const auto numSamplesToFeed = std::min(numRequired, maxBlockSize);
            mAudioSource(readBuffer.Get(), numSamplesToFeed);
            mTimeAndPitch->feedAudio(readBuffer.Get(), numSamplesToFeed);
            numRequired -= numSamplesToFeed;
         }
         numOutputSamplesAvailable =
            mTimeAndPitch->getNumAvailableOutputSamples();
         if (!firstCall)
         {
            break;
         }
         firstCall = false;
      }
      while (numOutputSamples < outputLen && numOutputSamplesAvailable > 0)
      {
         const auto numSamplesToGet =
            std::min({ maxBlockSize, numOutputSamplesAvailable,
                       static_cast<int>(outputLen - numOutputSamples) });
         const auto buffer =
            GetOffsetBuffer(output, mNumChannels, numOutputSamples);
         mTimeAndPitch->retrieveAudio(buffer.data(), numSamplesToGet);
         numOutputSamplesAvailable -= numSamplesToGet;
         numOutputSamples += numSamplesToGet;
      }
   }
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
   if (mTimeRatio == 1.0 && mPitchRatio == 1.0)
   {
      // Bypass
      mTimeAndPitch.reset();
      return;
   }
   mTimeAndPitch = std::make_unique<staffpad::TimeAndPitch>();
   mTimeAndPitch->setup(static_cast<int>(mNumChannels), maxBlockSize);
   mTimeAndPitch->setTimeStretchAndPitchFactor(mTimeRatio, mPitchRatio);
   const auto latencySamples = mTimeAndPitch->getLatencySamples();
   auto numOutputSamplesToDiscard =
      static_cast<int>(static_cast<double>(latencySamples) * mTimeRatio + 0.5);
   AudioContainer container(maxBlockSize, mNumChannels);
   while (numOutputSamplesToDiscard > 0)
   {
      auto numRequired = mTimeAndPitch->getSamplesToNextHop();
      while (numRequired > 0)
      {
         const auto numSamplesToFeed = std::min(maxBlockSize, numRequired);
         mAudioSource(container.Get(), numSamplesToFeed);
         mTimeAndPitch->feedAudio(container.Get(), numSamplesToFeed);
         numRequired -= numSamplesToFeed;
      }
      const auto totalNumSamplesToRetrieve = std::min(
         mTimeAndPitch->getNumAvailableOutputSamples(),
         numOutputSamplesToDiscard);
      auto totalNumRetrievedSamples = 0;
      while (totalNumRetrievedSamples < totalNumSamplesToRetrieve)
      {
         const auto numSamplesToRetrieve = std::min(
            maxBlockSize, totalNumSamplesToRetrieve - totalNumRetrievedSamples);
         mTimeAndPitch->retrieveAudio(container.Get(), numSamplesToRetrieve);
         totalNumRetrievedSamples += numSamplesToRetrieve;
      }
      numOutputSamplesToDiscard -= totalNumSamplesToRetrieve;
   }
}
