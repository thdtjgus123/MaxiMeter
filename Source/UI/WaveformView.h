#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include "../Audio/AudioEngine.h"

//==============================================================================
/// WaveformView — displays the full audio waveform as an overview with a
/// playback cursor. Supports click-to-seek and zoom/pan.
class WaveformView : public juce::Component,
                     public MeterBase,
                     public juce::ChangeListener,
                     public juce::Timer,
                     public AudioEngine::Listener
{
public:
    explicit WaveformView(AudioEngine& engine);
    ~WaveformView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    /// Rebuild the thumbnail from the currently loaded file
    void loadThumbnail(const juce::File& file);
    void clearThumbnail();

    /// Set an offline playback position (seconds). When >= 0, this overrides
    /// the live engine position — used during video export.
    void setOfflinePosition(double seconds) { offlinePos_ = seconds; }

    // Timer (repaint cursor)
    void timerCallback() override;

    // ChangeListener (thumbnail finished)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // AudioEngine::Listener
    void fileLoaded(const juce::String& fileName, double lengthSeconds) override;

private:
    AudioEngine& engine;

    juce::AudioFormatManager     formatManager;
    juce::AudioThumbnailCache    thumbnailCache { 5 };
    juce::AudioThumbnail         thumbnail { 512, formatManager, thumbnailCache };

    double totalLength = 0.0;  // seconds
    double offlinePos_  = -1.0; // < 0 means use live engine position

    void seekToMousePosition(const juce::MouseEvent& e);
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCursor(juce::Graphics& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
