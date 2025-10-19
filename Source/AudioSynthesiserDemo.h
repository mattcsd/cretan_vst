#pragma once

#include "DemoUtilities.h"
#include "AudioLiveScrollingDisplay.h"
#include <juce_dsp/juce_dsp.h>  // Include the DSP module

//==============================================================================
// Proper FFT Analyzer Component using JUCE's DSP module
class FFTAnalyzer : public Component, private Timer
{
public:
    FFTAnalyzer() : forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        setOpaque(true);
        startTimerHz(30); // Update at 30 Hz
    }

    void pushNextSample(float sample) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (!nextFFTBlockReady)
            {
                zeromem(fftData, sizeof(fftData));
                memcpy(fftData, fifo, sizeof(fifo));
                nextFFTBlockReady = true;
            }
            fifoIndex = 0;
        }
        fifo[fifoIndex++] = sample;
    }

    void drawNextFrameOfSpectrum()
    {
        // Apply windowing function
        window.multiplyWithWindowingTable(fftData, fftSize);
        
        // Perform FFT
        forwardFFT.performFrequencyOnlyForwardTransform(fftData);
        
        // Normalize and convert to dB
        auto mindB = -100.0f;
        auto maxdB = 0.0f;
        
        for (int i = 0; i < scopeSize; ++i)
        {
            // Map to logarithmic frequency scale
            auto skewedProportionX = 1.0f - std::exp(std::log(1.0f - (float)i / (float)scopeSize) * 0.2f);
            auto fftDataIndex = jlimit(0, fftSize / 2, (int)(skewedProportionX * (float)fftSize * 0.5f));
            
            // Get magnitude and convert to dB
            auto level = jmap(jlimit(mindB, maxdB,
                                   Decibels::gainToDecibels(fftData[fftDataIndex]) - Decibels::gainToDecibels((float)fftSize)),
                             mindB, maxdB, 0.0f, 1.0f);
            
            scopeData[i] = level;
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colours::black);

        g.setColour(Colours::white);
        g.drawText("FFT Spectrum Analysis", getLocalBounds().removeFromTop(20), Justification::centred);

        auto area = getLocalBounds().withTrimmedTop(25).reduced(2);

        // Draw frequency labels (logarithmic scale)
        g.setFont(10.0f);
        g.setColour(Colours::grey);
        
        String freqLabels[] = {"20Hz", "100Hz", "500Hz", "2kHz", "10kHz", "20kHz"};
        float freqPositions[] = {0.02f, 0.1f, 0.3f, 0.5f, 0.8f, 1.0f};
        
        for (int i = 0; i < 6; ++i)
        {
            auto xPos = area.getX() + area.getWidth() * freqPositions[i];
            g.drawText(freqLabels[i],
                      (int)xPos - 25, area.getBottom() - 15, 50, 15, Justification::centred);
        }

        // Draw the spectrum
        g.setColour(Colours::cyan);
        
        auto prevX = area.getX();
        auto prevY = area.getY() + area.getHeight();
        
        for (int i = 0; i < scopeSize; ++i)
        {
            // Use logarithmic frequency scale for x-axis
            auto normalizedIndex = 1.0f - std::exp(std::log(1.0f - (float)i / (float)scopeSize) * 0.2f);
            auto x = area.getX() + area.getWidth() * normalizedIndex;
            auto y = area.getY() + area.getHeight() * (1.0f - scopeData[i]);
            
            if (i > 0)
            {
                g.drawLine(prevX, prevY, (float)x, (float)y, 2.0f);
            }
            
            prevX = x;
            prevY = y;
        }

        // Draw grid lines and dB labels
        g.setColour(Colours::grey.withAlpha(0.3f));
        String dbLabels[] = {"0dB", "-20dB", "-40dB", "-60dB", "-80dB"};
        for (int i = 0; i < 5; ++i)
        {
            auto y = area.getY() + (area.getHeight() * i) / 4;
            g.drawHorizontalLine(y, float(area.getX()), float(area.getRight()));
            
            g.drawText(dbLabels[i], area.getX() - 35, (int)y - 7, 33, 14, Justification::right);
        }
    }

    void timerCallback() override
    {
        if (nextFFTBlockReady)
        {
            drawNextFrameOfSpectrum();
            nextFFTBlockReady = false;
            repaint();
        }
    }

private:
    enum
    {
        fftOrder = 11,
        fftSize = 1 << fftOrder,
        scopeSize = 512
    };

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fifo[fftSize];
    float fftData[2 * fftSize];
    float scopeData[scopeSize];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFTAnalyzer)
};

//==============================================================================
/** Our demo synth sound is just a basic sine wave.. */
struct SineWaveSound final : public SynthesiserSound
{
    bool appliesToNote(int /*midiNoteNumber*/) override    { return true; }
    bool appliesToChannel(int /*midiChannel*/) override    { return true; }
};

//==============================================================================
/** Our demo synth voice just plays a sine wave.. */
struct SineWaveVoice final : public SynthesiserVoice
{
    bool canPlaySound(SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity,
                    SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        auto cyclesPerSecond = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * MathConstants<double>::twoPi;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (approximatelyEqual(tailOff, 0.0))
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved(int /*newValue*/) override                              {}
    void controllerMoved(int /*controllerNumber*/, int /*newValue*/) override    {}

    void renderNextBlock(AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (!approximatelyEqual(angleDelta, 0.0))
        {
            if (tailOff > 0.0)
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float)(std::sin(currentAngle) * level * tailOff);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample(i, startSample, currentSample);

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
                    auto currentSample = (float)(std::sin(currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample(i, startSample, currentSample);

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
    SynthAudioSource(MidiKeyboardState& keyState, FFTAnalyzer& fftAnalyzerIn)
        : keyboardState(keyState), fftAnalyzer(fftAnalyzerIn)
    {
        for (auto i = 0; i < 4; ++i)
        {
            synth.addVoice(new SineWaveVoice());
            synth.addVoice(new SamplerVoice());
        }

        setUsingSineWaveSound();
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
        synth.addSound(new SineWaveSound());
    }

    void setUsingSampledSound()
    {
        const void* data = BinaryData::sample_wav;
        int dataSize = BinaryData::sample_wavSize;
        
        // Create a new MemoryInputStream each time
        auto* stream = new juce::MemoryInputStream(data, static_cast<size_t>(dataSize), false);
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> audioReader(wavFormat.createReaderFor(stream, true));
        
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
    }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        midiCollector.reset(sampleRate);
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        MidiBuffer incomingMidi;
        midiCollector.removeNextBlockOfMessages(incomingMidi, bufferToFill.numSamples);

        keyboardState.processNextMidiBuffer(incomingMidi, 0, bufferToFill.numSamples, true);

        synth.renderNextBlock(*bufferToFill.buffer, incomingMidi, 0, bufferToFill.numSamples);

        // Feed audio to FFT analyzer (use first channel)
        auto* channelData = bufferToFill.buffer->getReadPointer(0);
        for (int i = 0; i < bufferToFill.numSamples; ++i)
            fftAnalyzer.pushNextSample(channelData[i]);
    }

    MidiMessageCollector midiCollector;
    MidiKeyboardState& keyboardState;
    Synthesiser synth;
    FFTAnalyzer& fftAnalyzer;

private:
    std::unique_ptr<juce::MemoryInputStream> inputStream;
};

//==============================================================================
// Modified Callback to fix display scaling
class Callback final : public AudioIODeviceCallback
{
public:
    Callback(AudioSourcePlayer& playerIn, LiveScrollingAudioDisplay& displayIn)
        : player(playerIn), display(displayIn) {}

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const AudioIODeviceCallbackContext& context) override
    {
        player.audioDeviceIOCallbackWithContext(inputChannelData,
                                                 numInputChannels,
                                                 outputChannelData,
                                                 numOutputChannels,
                                                 numSamples,
                                                 context);
        
        // Create a scaled copy for the display to prevent visual clipping
        AudioBuffer<float> displayBuffer(numOutputChannels, numSamples);
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            auto* source = outputChannelData[channel];
            auto* dest = displayBuffer.getWritePointer(channel);
            for (int i = 0; i < numSamples; ++i)
                dest[i] = source[i] * 0.45f; // Scale down for display
        }
        
        display.audioDeviceIOCallbackWithContext(displayBuffer.getArrayOfReadPointers(),
                                                  numOutputChannels,
                                                  nullptr,
                                                  0,
                                                  numSamples,
                                                  context);
    }

    void audioDeviceAboutToStart(AudioIODevice* device) override
    {
        player.audioDeviceAboutToStart(device);
        display.audioDeviceAboutToStart(device);
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
        addAndMakeVisible(keyboardComponent);

        addAndMakeVisible(sineButton);
        sineButton.setRadioGroupId(321);
        sineButton.setToggleState(true, dontSendNotification);
        sineButton.onClick = [this] { synthAudioSource.setUsingSineWaveSound(); };

        addAndMakeVisible(sampledButton);
        sampledButton.setRadioGroupId(321);
        sampledButton.onClick = [this] { synthAudioSource.setUsingSampledSound(); };

        addAndMakeVisible(midiInputListLabel);
        midiInputListLabel.setText("MIDI Input:", dontSendNotification);
        midiInputListLabel.attachToComponent(&midiInputList, true);

        addAndMakeVisible(midiInputList);
        midiInputList.onChange = [this] { setMidiInput(midiInputList.getSelectedId() - 1); };

        auto midiInputs = MidiInput::getAvailableDevices();
        for (auto i = 0; i < midiInputs.size(); ++i)
            midiInputList.addItem(midiInputs[i].name, i + 1);

        midiInputList.setSelectedId(1);

        // Add both displays
        addAndMakeVisible(liveAudioDisplayComp);
        addAndMakeVisible(fftAnalyzer);

        audioSourcePlayer.setSource(&synthAudioSource);

       #ifndef JUCE_DEMO_RUNNER
        audioDeviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
       #endif

        audioDeviceManager.addAudioCallback(&callback);
        audioDeviceManager.addMidiInputDeviceCallback({}, &(synthAudioSource.midiCollector));

        setOpaque(true);
        setSize(640, 600); // Increased height to accommodate both displays
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

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        
        // Give equal vertical space to both displays (150 pixels each)
        liveAudioDisplayComp.setBounds(area.removeFromTop(150));
        fftAnalyzer.setBounds(area.removeFromTop(150));
        
        // Adjust the remaining space distribution
        auto bottomArea = area.removeFromBottom(96); // Keyboard area
        keyboardComponent.setBounds(bottomArea);
        
        // Adjust control panel area
        auto controlArea = area.removeFromLeft(180);
        sineButton.setBounds(controlArea.removeFromTop(24).reduced(2));
        sampledButton.setBounds(controlArea.removeFromTop(24).reduced(2));
        midiInputList.setBounds(controlArea.removeFromTop(24).reduced(2));
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
            if (!audioDeviceManager.isMidiInputDeviceEnabled(newInput.identifier))
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
    FFTAnalyzer fftAnalyzer;
    SynthAudioSource synthAudioSource { keyboardState, fftAnalyzer };
    MidiKeyboardComponent keyboardComponent { keyboardState, MidiKeyboardComponent::horizontalKeyboard };

    ToggleButton sineButton { "Use sine wave" };
    ToggleButton sampledButton { "Use sampled sound" };

    LiveScrollingAudioDisplay liveAudioDisplayComp;

    Callback callback { audioSourcePlayer, liveAudioDisplayComp };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSynthesiserDemo)
};
