#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"

//==============================================================================
/// PeakMeter — professional-grade peak level meter with True Peak and Sample Peak modes.
/// Features: color zones (Green/Yellow/Red), peak hold with configurable time,
/// clip warning, stereo or multi-channel, dB scale markings.
class PeakMeter : public juce::Component,
                  public MeterBase,
                  private juce::Timer
{
public:
    enum class PeakMode { SamplePeak, TruePeak };
    enum class Orientation { Vertical, Horizontal };

    PeakMeter();
    ~PeakMeter() override;

    /// Set number of channels (1=mono, 2=stereo, up to 8)
    void setNumChannels(int numChannels);
    int  getNumChannels() const { return channels; }

    /// Set instantaneous level for a channel (linear 0..1+)
    void setLevel(int channel, float linearLevel);

    /// Configuration
    void setPeakMode(PeakMode mode)           { peakMode = mode; }
    void setOrientation(Orientation o)        { orientation = o; resized(); }
    void setPeakHoldTimeMs(float ms)          { peakHoldMs = juce::jlimit(500.0f, 30000.0f, ms); }
    void setPeakHoldInfinite(bool inf)        { infiniteHold = inf; }
    void setDecayRateDbPerSec(float rate)     { decayRate = juce::jlimit(3.0f, 60.0f, rate); }
    void setShowClipWarning(bool show)        { showClip = show; }
    void setShowScale(bool show)              { showScale = show; resized(); }
    void setMinDb(float db)                   { minDb = db; }
    void setMaxDb(float db)                   { maxDb = db; }
    void resetPeaks();

    // Getters for export/serialization
    PeakMode getPeakMode()           const { return peakMode; }
    float    getPeakHoldTimeMs()     const { return peakHoldMs; }
    float    getDecayRateDbPerSec()  const { return decayRate; }
    bool     getShowClipWarning()    const { return showClip; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct ChannelState
    {
        float level        = 0.0f;  // current display level (dB)
        float peakHold     = -100.0f;
        float peakHoldAge  = 0.0f;  // seconds since peak was set
        bool  clipping     = false;
    };

    int channels = 2;
    std::vector<ChannelState> channelStates;

    PeakMode    peakMode    = PeakMode::SamplePeak;
    Orientation orientation = Orientation::Vertical;
    float       peakHoldMs  = 2000.0f;
    bool        infiniteHold = false;
    float       decayRate    = 20.0f;   // dB/sec
    bool        showClip     = true;
    bool        showScale    = true;
    float       minDb        = -60.0f;
    float       maxDb        = 3.0f;

    void timerCallback() override;

    void drawVerticalMeter(juce::Graphics& g, juce::Rectangle<int> area, int ch);
    void drawHorizontalMeter(juce::Graphics& g, juce::Rectangle<int> area, int ch);
    void drawScale(juce::Graphics& g, juce::Rectangle<int> area);
    juce::Colour dbToColour(float db) const;
    float dbToNormalized(float db) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PeakMeter)
};
