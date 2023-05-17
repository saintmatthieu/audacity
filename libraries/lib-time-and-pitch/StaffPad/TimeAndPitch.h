//
// Arbitrary time and pitch stretching, based on the Phase-locked Vocoder.
//
#pragma once

#include <memory>
#include <vector>

namespace staffpad
{

class TIME_AND_PITCH_API TimeAndPitch
{
public:
   TimeAndPitch() = default;
   ~TimeAndPitch();

   /**
     Setup at least once before processing.
     \param numChannels  Must be 1 or 2
     \param maxBlockSize The caller's maximum block size, e.g. 1024 samples
   */
   void setup(int numChannels, int maxBlockSize);

   /**
     Set independent time stretch and pitch factors (synchronously to processing
     thread). The factor change resolution follows the processing block size,
     with some approximation. As a general rule, smaller block sizes result in
     finer changes, although there's a lower bound due to internal windowed
     segmentation and OLA resynthesis.
   */
   void setTimeStretchAndPitchFactor(double timeStretch, double pitchFactor);

   /**
     Return the ~number of samples until the next hop gets processed (might be 1
     or 2 too high) If this number of samples is added using feedAudio(), more
     output samples are produced.
     */
   int getSamplesToNextHop() const;

   /**
     Tells the number of already processed output samples (after
     time-stretching) that are ready for consumption.
   */
   int getNumAvailableOutputSamples() const;

   /**
     Feed more input samples. \see `getSamplesToNextHop()` to know how much to
     feed to produce more output samples.
   */
   void feedAudio(const float* const* in_smp, int numSamples);

   /**
     Retrieved shifted/stretched samples in output. \see
     `getNumAvailableOutputSamples()` to ask a valid amount of `numSamples`.
   */
   void retrieveAudio(float* const* out_smp, int numSamples);

   /**
     Pitch-shift only in-place processing.
    */
   void
   processPitchShift(float* const* smp, int numSamples, double pitchFactor);

   /**
     Latency in input samples, output_latency = input_latency * time_scale
   */
   int getLatencySamples() const;

   /**
     Resets the internal state, discards any buffered input
   */
   void reset();

private:
   static constexpr int fftSize = 4096;
   static constexpr int overlap = 4;
   static constexpr bool normalize_window =
      true; // compensate for ola window overlaps
   static constexpr bool modulate_synthesis_hop = true;

   void _process_hop(int hop_a, int hop_s);
   template <int num_channels> void _time_stretch(float hop_a, float hop_s);

   struct impl;
   std::shared_ptr<impl> d;

   int _numChannels = 1;
   int _maxBlockSize = 1024;
   double _resampleReadPos = 0.0;
   int _availableOutputSamples = 0;
   int _numBins = fftSize / 2 + 1;
   double _overlap_a = overlap;

   int _analysis_hop_counter = 0;

   double _expectedPhaseChangePerBinPerSample = 0.01;
   double _timeStretch = 1.0;
   double _pitchFactor = 1.0;

   int _outBufferWriteOffset = 0;
};

} // namespace staffpad
