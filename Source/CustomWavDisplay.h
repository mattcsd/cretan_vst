//
//  CustomWavDisplay.h
//  AudioSynthesiserDemo - App
//
//  Created by kez542 on 30/7/25.
//  Copyright © 2025 JUCE. All rights reserved.
//

#pragma once

#include "AudioLiveScrollingDisplay.h"  // Or <path/to/AudioLiveScrollingDisplay.h>, depending on where it lives
using namespace juce;

class CustomAudioDisplay : public LiveScrollingAudioDisplay
{
public:
    CustomAudioDisplay(float gainMultiplier = 5.0f);

    void setGain(float newGain);

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numberOfSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

private:
    float gain = 5.0f;
};
