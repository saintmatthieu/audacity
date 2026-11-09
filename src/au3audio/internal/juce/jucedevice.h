/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <memory>

#include "../au4/idevice.h"
#include "juceincludes.h"

namespace au::au4 {
class JuceAudioDevices;

//! A JUCE device
class JuceDevice final : public IDevice, private juce::AudioIODeviceCallback
{
public:
    explicit JuceDevice(std::unique_ptr<juce::AudioIODevice> device);
    ~JuceDevice() override;

    double sampleRate() const override;
    int blockSize() const override;
    int numInputChannels() const override;
    int numOutputChannels() const override;
    double inputLatency() const override;
    double outputLatency() const override;

    void start(IDeviceCallback& callback) override;
    void stop() override;
    int xrunCount() const override;

private:
    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                                          int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
    void audioDeviceError(const juce::String& errorMessage) override;

    const std::unique_ptr<juce::AudioIODevice> m_device;
    IDeviceCallback* m_callback = nullptr;
};

//! Opens JUCE devices
class JuceDeviceOpener final : public IDeviceOpener
{
public:
    explicit JuceDeviceOpener(std::shared_ptr<JuceAudioDevices> devices);

    std::string name() const override { return "JUCE"; }
    std::unique_ptr<IDevice> open(const audio::AudioConfiguration& configuration, int numInputChannels, int numOutputChannels,
                                  double sampleRate, std::string& error) override;

private:
    const std::shared_ptr<JuceAudioDevices> m_devices;
};
}
