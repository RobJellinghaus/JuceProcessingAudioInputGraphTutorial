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
        levelLabel.setText ("Noise Level", dontSendNotification);

        addAndMakeVisible (levelSlider);
        addAndMakeVisible(levelLabel);
        addAndMakeVisible(infoLabel);

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
            int minBufferSize = bufferSizes[0];

            AudioDeviceManager::AudioDeviceSetup setup;
            deviceManager.getAudioDeviceSetup(setup);

            if (setup.bufferSize > minBufferSize)
            {
                setup.bufferSize = minBufferSize;
                String result = deviceManager.setAudioDeviceSetup(setup, false);
                if (result.length() > 0)
                {
                    throw std::exception(result.getCharPointer());
                }
            }

            if (minBufferSize != device->getCurrentBufferSizeSamples())
            {
                // die horribly
                throw std::exception("Can't set buffer size to minimum");
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
        const OwnedArray<AudioIODeviceType>& deviceTypes = deviceManager.getAvailableDeviceTypes();
        // if true, matchString will be considered a substring; if false, an exact match
        String desiredTypeName{L""};
        // "Exclusive" substring for WASAPI exclusive mode
        // "ASIO" substring for Asio
        // "Windows Audio" non-substring) for WASAPI shared mode
        String matchString = L"ASIO";
        bool isSubstring = true;
        for (int i = 0; i < deviceTypes.size(); i++)
        {
            AudioIODeviceType* type = deviceTypes[i];
            String typeName = type->getTypeName();

            bool isMatch = isSubstring ? typeName.contains(matchString) : typeName == matchString;
            if (isMatch)
            {
                desiredTypeName = typeName;
            }
        }

        if (desiredTypeName.length() > 0)
        {
            deviceManager.setCurrentAudioDeviceType(desiredTypeName, /*treatAsChosenDevice*/ false);
        }
        else
        {
            throw std::exception("Could not set audio device type to desired type name");
        }

        String result = deviceManager.initialiseWithDefaultDevices(2, 2);
        if (result.length() > 0)
        {
            throw std::exception(result.getCharPointer());
        }
        
        player.setProcessor(&graph);
        deviceManager.addAudioCallback(&player);

        setBufferSizeToMinimum();

        AudioIODevice* device = deviceManager.getCurrentAudioDevice();

        if (device->getTypeName() != desiredTypeName)
        {
            throw std::exception("Device type names don't match");
        }

        BigInteger activeInputChannels = device->getActiveInputChannels();
        BigInteger activeOutputChannels = device->getActiveOutputChannels();

        int maxInputChannels = activeInputChannels.getHighestBit() + 1;
        int maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        double bufferRate = device->getCurrentSampleRate();
        int bufferSize = device->getCurrentBufferSizeSamples();

        if (maxInputChannels != maxOutputChannels)
        {
            throw std::exception("Don't yet support different numbers of input vs output channels");
        }

        String label;
        AppendToString(label, L"Buffer rate ");
        AppendToString(label, String(bufferRate));
        AppendToString(label, L", buffer size ");
        AppendToString(label, String(bufferSize));
        AppendToString(label, L", device type ");
        AppendToString(label, device->getTypeName());
        AppendToString(label, L", maxin ");
        AppendToString(label, String(maxInputChannels));
        AppendToString(label, L", maxout ");
        AppendToString(label, String(maxOutputChannels));
        infoLabel.setText(label, NotificationType::dontSendNotification);

        graph.setPlayConfigDetails(
            maxInputChannels,
            maxOutputChannels,
            device->getCurrentSampleRate(),
            device->getCurrentBufferSizeSamples());

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

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &) override
    {
        throw std::exception("This method should never be called since the AudioProcessorPlayer should be the callback");
    }

    void releaseResources() override
    {
        player.setProcessor(nullptr);
        graph.clear();
    }

    void resized() override
    {
        const int width = 100;

        levelLabel.setBounds(10, 10, width - 10, 20);
        levelSlider.setBounds (100, 10, getWidth() - (width + 10), 20);

        infoLabel.setBounds(10, 30, getWidth(), 20);
    }

private:
    Random random;
    Slider levelSlider;
    Label levelLabel;
    Label infoLabel;

    AudioProcessorGraph graph;
    AudioProcessorPlayer player;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};
