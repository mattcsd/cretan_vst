//
//  CustomWavDisplay.cpp
//  AudioSynthesiserDemo - App
//
//  Created by kez542 on 30/7/25.
//  Copyright © 2025 JUCE. All rights reserved.
//

#include "CustomWavDisplay.h"
using namespace juce;

CustomAudioDisplay::CustomAudioDisplay(float gainMultiplier)
    : gain(gainMultiplier)
{
}

void CustomAudioDisplay::setGain(float newGain)
{
    gain = newGain;
}

void CustomAudioDisplay::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                          int numInputChannels,
                                                          float* const* outputChannelData,
                                                          int numOutputChannels,
                                                          int numberOfSamples,
                                                          const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    for (int i = 0; i < numberOfSamples; ++i)
    {
        float inputSample = 0.0f;

        for (int chan = 0; chan < numInputChannels; ++chan)
            if (const float* inputChannel = inputChannelData[chan])
                inputSample += inputChannel[i];

        inputSample *= gain;

        pushSample(&inputSample, 1);
    }

    for (int j = 0; j < numOutputChannels; ++j)
        if (float* outputChannel = outputChannelData[j])
            zeromem(outputChannel, (size_t) numberOfSamples * sizeof(float));
}
