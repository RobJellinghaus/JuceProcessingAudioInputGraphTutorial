/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2017 - ROLI Ltd.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             ProcessingAudioInputTutorial
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Performs processing on an input signal.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2017, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once

//==============================================================================
class MainContentComponent   : public AudioAppComponent
{
public:
    //==============================================================================
    MainContentComponent()
    {
        levelSlider.setRange (0.0, 0.25);
        levelSlider.setTextBoxStyle (Slider::TextBoxRight, false, 50, 20);
        levelLabel.setText ("Noise Level (.........................)", dontSendNotification);

        addAndMakeVisible (levelSlider);
        addAndMakeVisible (levelLabel);

        setSize (800, 100);

        // we deliberately don't call this
        //setAudioChannels (2, 2);

        // instead we call prepareToPlay directly
        // the parameters are actually ignored
        prepareToPlay(0, 0);
    }

    ~MainContentComponent()
    {
        //shutdownAudio();
    }

    // Set the buffer size of the current device to the minimum supported size
    void setBufferSizeToMinimum()
    {
        // Set buffer size to minimum available on current device
        auto* device = deviceManager.getCurrentAudioDevice();

        auto bufferRate = device->getCurrentSampleRate();
        auto bufferSize = device->getCurrentBufferSizeSamples();

        const bool setMinBufferSize = true;
        if (setMinBufferSize)
        {
            auto bufferSizes = device->getAvailableBufferSizes();
            AudioDeviceManager::AudioDeviceSetup setup;
            deviceManager.getAudioDeviceSetup(setup);
            setup.bufferSize = bufferSizes[0];
            deviceManager.setAudioDeviceSetup(setup, false);

            if (bufferSizes[0] != device->getCurrentBufferSizeSamples())
            {
                // die horribly
                throw "Can't set buffer size to minimum";
            }
        }
    }

    // Append the source string to the target
    static void AppendToString(String& target, const wchar_t* source)
    {
        String sourceString{ source };
        AppendToString(target, sourceString);
    }

    // Append the source string to the target
    static void AppendToString(String& target, const String& sourceString)
    {
        target.append(sourceString, sourceString.length());
    }

    // Prepare to play
    // In the original sample code, this method is called (re-entrantly, it turns out) from
    // the MainContentComponent() constructor via the setChannels(2, 2) call.
    void prepareToPlay(int, double) override
    {
        player.setProcessor(&graph);
        deviceManager.addAudioCallback(&player);

        deviceManager.initialiseWithDefaultDevices(2, 2);

        setBufferSizeToMinimum();

        AudioIODevice* device = deviceManager.getCurrentAudioDevice();

        BigInteger activeInputChannels = device->getActiveInputChannels();
        BigInteger activeOutputChannels = device->getActiveOutputChannels();

        int maxInputChannels = activeInputChannels.getHighestBit() + 1;
        int maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        double bufferRate = device->getCurrentSampleRate();
        int bufferSize = device->getCurrentBufferSizeSamples();

        String label;
        AppendToString(label, L"Noise level: (buffer rate ");
        AppendToString(label, String(bufferRate));
        AppendToString(label, L", buffer size ");
        AppendToString(label, String(bufferSize));
        levelLabel.setText(label, NotificationType::dontSendNotification);

        graph.setPlayConfigDetails(
            maxInputChannels,
            maxOutputChannels,
            device->getCurrentSampleRate(),
            device->getCurrentBufferSizeSamples());

        if (activeInputChannels != activeOutputChannels)
        {
            throw "Don't yet support different numbers of input vs output channels";
        }

        // TBD: is double better?  Single (e.g. float32) definitely best for starters though
        graph.setProcessingPrecision(AudioProcessor::singlePrecision);

        graph.prepareToPlay(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());

        AudioProcessorGraph::AudioGraphIOProcessor* input =
            new AudioProcessorGraph::AudioGraphIOProcessor(
                AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);

        AudioProcessorGraph::AudioGraphIOProcessor* output =
            new AudioProcessorGraph::AudioGraphIOProcessor(
                AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);

        //mOsc1Node = new OscillatorNode();
        //mOsc1Node->setPlayConfigDetails(getNumInputChannels(), getNumOutputChannels(), sampleRate, samplesPerBlock);

        AudioProcessorGraph::Node::Ptr inputNodePtr = graph.addNode(input);
        AudioProcessorGraph::Node::Ptr outputNodePtr = graph.addNode(output);

        for (int i = 0; i < maxInputChannels; i++)
        {
            graph.addConnection({ { inputNodePtr->nodeID, i }, { outputNodePtr->nodeID, i } });
        }
    }

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {

#if USE_DIRECT_AUDIO_COPY // original tutorial code that doesn't use AudioProcessorGraph

        auto* device = deviceManager.getCurrentAudioDevice();

        auto bufferRate = device->getCurrentSampleRate();
        auto bufferSize = device->getCurrentBufferSizeSamples();

        auto activeInputChannels  = device->getActiveInputChannels();
        auto activeOutputChannels = device->getActiveOutputChannels();
        auto maxInputChannels  = activeInputChannels .getHighestBit() + 1;
        auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;

        auto level = (float) levelSlider.getValue();

        for (auto channel = 0; channel < maxOutputChannels; ++channel)
        {
            if ((! activeOutputChannels[channel]) || maxInputChannels == 0)
            {
                bufferToFill.buffer->clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
            }
            else
            {
                auto actualInputChannel = channel % maxInputChannels; // [1]

                if (! activeInputChannels[channel]) // [2]
                {
                    bufferToFill.buffer->clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
                }
                else // [3]
                {
                    auto* inBuffer = bufferToFill.buffer->getReadPointer (actualInputChannel,
                                                                          bufferToFill.startSample);
                    auto* outBuffer = bufferToFill.buffer->getWritePointer (channel, bufferToFill.startSample);

                    for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                        outBuffer[sample] = inBuffer[sample]; // * random.nextFloat() * level;
                }
            }
        }

#else

        // we expect AudioProcessorPlayer to be handling the device callback
        // and in fact we don't expect this method to be called at all...?

#endif

    }

    void releaseResources() override {}

    void resized() override
    {
        const int width = 300;
        levelLabel .setBounds (10, 10, width - 10, 20);
        levelSlider.setBounds (100, 10, getWidth() - (width + 10), 20);
    }

private:
    Random random;
    Slider levelSlider;
    Label levelLabel;

    AudioProcessorGraph graph;
    AudioProcessorPlayer player;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};
