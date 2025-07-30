/*
  ==============================================================================

   This file is part of the JUCE framework examples.
   Copyright (c) Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   to use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
   FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
   INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
   OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
   PERFORMANCE OF THIS SOFTWARE.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             AudioSynthesiserDemo
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Simple synthesiser application.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2022, linux_make, androidstudio, xcode_iphone

 moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:             Component
 mainClass:        AudioSynthesiserDemo

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "DemoUtilities.h"
#include "AudioLiveScrollingDisplay.h"

//==============================================================================
/** Our demo synth sound is just a basic sine wave.. */
struct SineWaveSound final : public SynthesiserSound
{
    bool appliesToNote (int /*midiNoteNumber*/) override    { return true; }
    bool appliesToChannel (int /*midiChannel*/) override    { return true; }
};

//==============================================================================
/** Our demo synth voice just plays a sine wave.. */
struct SineWaveVoice final : public SynthesiserVoice
{
    bool canPlaySound (SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        auto cyclesPerSecond = MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * MathConstants<double>::twoPi;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (approximatelyEqual (tailOff, 0.0))
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved (int /*newValue*/) override                              {}
    void controllerMoved (int /*controllerNumber*/, int /*newValue*/) override    {}

    void renderNextBlock (AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (! approximatelyEqual (angleDelta, 0.0))
        {
            if (tailOff > 0.0)
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level * tailOff);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;

                    tailOff *= 0.99;

                    if (tailOff <= 0.005)
                    {
                        clearCurrentNote();

                        angleDelta = 0.0;
                        break;
                    }
                }
            }
            else
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;
                }
            }
        }
    }

    using SynthesiserVoice::renderNextBlock;

private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};

//==============================================================================
// This is an audio source that streams the output of our demo synth.
struct SynthAudioSource final : public AudioSource
{
    SynthAudioSource (MidiKeyboardState& keyState)  : keyboardState (keyState)
    {
        for (auto i = 0; i < 4; ++i)
        {
            synth.addVoice (new SineWaveVoice());
            synth.addVoice (new SamplerVoice());
        }

        setUsingSineWaveSound();
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
        synth.addSound (new SineWaveSound());
    }

    void setUsingSampledSound()
    {
        const void* data = BinaryData::sample_wav;
        int dataSize = BinaryData::sample_wavSize;
        
        // Create a new MemoryInputStream each time
        auto* stream = new juce::MemoryInputStream(data, static_cast<size_t>(dataSize), false);
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> audioReader(wavFormat.createReaderFor(stream, true)); // true = reader will delete the stream
        
        BigInteger allNotes;
        allNotes.setRange(0, 128, true);

        synth.clearSounds();
        synth.addSound(new SamplerSound("demo sound",
                                       *audioReader,
                                       allNotes,
                                       74,   // root midi note
                                       0.1,  // attack time
                                       0.1,  // release time
                                       10.0  // maximum sample length
                                       ));
    }        //
        /*WavAudioFormat wavFormat;

        std::unique_ptr<AudioFormatReader> audioReader (wavFormat.createReaderFor (createAssetInputStream ("cello.wav").release(), true));

        BigInteger allNotes;
        allNotes.setRange (0, 128, true);

        synth.clearSounds();
        synth.addSound (new SamplerSound ("demo sound",
                                          *audioReader,
                                          allNotes,
                                          74,   // root midi note
                                          0.1,  // attack time
                                          0.1,  // release time
                                          10.0  // maximum sample length
                                          ));
    */

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        midiCollector.reset (sampleRate);

        synth.setCurrentPlaybackSampleRate (sampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        MidiBuffer incomingMidi;
        midiCollector.removeNextBlockOfMessages (incomingMidi, bufferToFill.numSamples);

        keyboardState.processNextMidiBuffer (incomingMidi, 0, bufferToFill.numSamples, true);

        synth.renderNextBlock (*bufferToFill.buffer, incomingMidi, 0, bufferToFill.numSamples);
    }

    MidiMessageCollector midiCollector;
    MidiKeyboardState& keyboardState;
    Synthesiser synth;

    private:
        std::unique_ptr<juce::MemoryInputStream> inputStream;

};

//==============================================================================
class Callback final : public AudioIODeviceCallback
{
public:
    Callback (AudioSourcePlayer& playerIn, LiveScrollingAudioDisplay& displayIn)
        : player (playerIn), display (displayIn) {}

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const AudioIODeviceCallbackContext& context) override
    {
        player.audioDeviceIOCallbackWithContext (inputChannelData,
                                                 numInputChannels,
                                                 outputChannelData,
                                                 numOutputChannels,
                                                 numSamples,
                                                 context);
        display.audioDeviceIOCallbackWithContext (outputChannelData,
                                                  numOutputChannels,
                                                  nullptr,
                                                  0,
                                                  numSamples,
                                                  context);
    }

    void audioDeviceAboutToStart (AudioIODevice* device) override
    {
        player.audioDeviceAboutToStart (device);
        display.audioDeviceAboutToStart (device);
    }

    void audioDeviceStopped() override
    {
        player.audioDeviceStopped();
        display.audioDeviceStopped();
    }

private:
    AudioSourcePlayer& player;
    LiveScrollingAudioDisplay& display;
};

//==============================================================================
class AudioSynthesiserDemo final : public Component
{
public:
    AudioSynthesiserDemo()
    {
        #ifndef JUCE_DEMO_RUNNER
        audioDeviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
        #endif
        addAndMakeVisible (keyboardComponent);

        addAndMakeVisible (sineButton);
        sineButton.setRadioGroupId (321);
        sineButton.setToggleState (true, dontSendNotification);
        sineButton.onClick = [this] { synthAudioSource.setUsingSineWaveSound(); };

        addAndMakeVisible (sampledButton);
        sampledButton.setRadioGroupId (321);
        sampledButton.onClick = [this] { synthAudioSource.setUsingSampledSound(); };

        addAndMakeVisible (midiInputListLabel);
        midiInputListLabel.setText ("MIDI Input:", dontSendNotification);
        midiInputListLabel.attachToComponent (&midiInputList, true);

        addAndMakeVisible (midiInputList);
        midiInputList.onChange = [this] { setMidiInput (midiInputList.getSelectedId() - 1); };

        auto midiInputs = MidiInput::getAvailableDevices();
        for (auto i = 0; i < midiInputs.size(); ++i)
            midiInputList.addItem (midiInputs[i].name, i + 1);

        midiInputList.setSelectedId (1);

        addAndMakeVisible (liveAudioDisplayComp);
        audioSourcePlayer.setSource (&synthAudioSource);

       #ifndef JUCE_DEMO_RUNNER
        audioDeviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
       #endif

        audioDeviceManager.addAudioCallback (&callback);
        audioDeviceManager.addMidiInputDeviceCallback ({}, &(synthAudioSource.midiCollector));

        setOpaque (true);
        setSize (640, 480);
    }

    ~AudioSynthesiserDemo() override
    {
        // Stop audio processing first
        audioDeviceManager.removeAudioCallback(&callback);
        audioDeviceManager.removeMidiInputDeviceCallback({}, &(synthAudioSource.midiCollector));
        
        // Then release the audio source
        audioSourcePlayer.setSource(nullptr);
        
        // Clear any remaining sounds and voices
        synthAudioSource.synth.clearSounds();
        synthAudioSource.synth.clearVoices();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        liveAudioDisplayComp.setBounds (area.removeFromTop (64));
        keyboardComponent.setBounds (area.removeFromBottom (96));

        auto buttonArea = area.removeFromLeft (150);
        sineButton.setBounds (buttonArea.removeFromTop (24));
        sampledButton.setBounds (buttonArea.removeFromTop (24));

        midiInputList.setBounds (area.removeFromTop (24).removeFromLeft (getWidth() / 2));
    }

private:
    // Select your midi device
    void setMidiInput(int index)
    {
        auto list = MidiInput::getAvailableDevices();
        if (list.isEmpty())
            return;

        // Disable all current MIDI inputs
        for (auto& input : MidiInput::getAvailableDevices())
            audioDeviceManager.setMidiInputDeviceEnabled(input.identifier, false);

        audioDeviceManager.removeMidiInputDeviceCallback({}, &(synthAudioSource.midiCollector));

        if (index >= 0 && index < list.size())
        {
            auto newInput = list[index];
            if (! audioDeviceManager.isMidiInputDeviceEnabled(newInput.identifier))
            {
                audioDeviceManager.setMidiInputDeviceEnabled(newInput.identifier, true);
                audioDeviceManager.addMidiInputDeviceCallback(newInput.identifier, &(synthAudioSource.midiCollector));
            }
        }
    }



    AudioDeviceManager audioDeviceManager;

    Label midiInputListLabel { {}, "MIDI Input:" };
    ComboBox midiInputList;

    MidiKeyboardState keyboardState;
    AudioSourcePlayer audioSourcePlayer;
    SynthAudioSource synthAudioSource { keyboardState };
    MidiKeyboardComponent keyboardComponent { keyboardState, MidiKeyboardComponent::horizontalKeyboard };

    ToggleButton sineButton { "Use sine wave" };
    ToggleButton sampledButton { "Use sampled sound" };

    LiveScrollingAudioDisplay liveAudioDisplayComp;

    Callback callback { audioSourcePlayer, liveAudioDisplayComp };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSynthesiserDemo)
};
