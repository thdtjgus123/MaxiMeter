#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioEngine.h"
#include "../Audio/LevelAnalyzer.h"

//==============================================================================
/// StatusBar — bottom bar showing file info, current levels, sample rate,
/// and playback state.
class StatusBar : public juce::Component,
                  public juce::Timer,
                  public AudioEngine::Listener
{
public:
    StatusBar(AudioEngine& engine, LevelAnalyzer& levels);
    ~StatusBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // AudioEngine::Listener
    void fileLoaded(const juce::String& fileName, double lengthSeconds) override;
    void transportStateChanged(bool isPlaying) override;

private:
    AudioEngine&   engine;
    LevelAnalyzer& levels;

    juce::String fileInfo;
    juce::String playbackState { "Stopped" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
