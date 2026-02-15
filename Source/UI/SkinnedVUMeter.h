#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include "../Skin/SkinModel.h"

//==============================================================================
/// SkinnedVUMeter — analog-style VU meter with configurable ballistics.
/// Supports VU, PPM Type I/II, and Nordic ballistic modes.
/// Can display as a horizontal or vertical bar with optional needle mode.
class SkinnedVUMeter : public juce::Component,
                       public MeterBase
{
public:
    enum class Ballistic { VU, PPM_I, PPM_II, Nordic };
    enum class Orientation { Horizontal, Vertical };

    SkinnedVUMeter();
    ~SkinnedVUMeter() override = default;

    /// Set the input level (linear, 0..1+)
    void setLevel(float linearLevel);

    /// Set the skin for themed rendering (null for default)
    void setSkin(const Skin::SkinModel* skin) { currentSkin = skin; repaint(); }

    /// Configuration
    void setBallistic(Ballistic mode)     { ballistic = mode; updateCoefficients(); }
    void setOrientation(Orientation o)    { orientation = o; repaint(); }
    void setDecayTimeMs(float ms)         { decayMs = juce::jlimit(100.0f, 3000.0f, ms); updateCoefficients(); }
    void setChannelLabel(const juce::String& label) { channelName = label; }
    void setShowPeakHold(bool show)       { showPeakHold = show; }

    // Getters for export/serialization
    Ballistic getBallistic()  const { return ballistic; }
    float     getDecayTimeMs() const { return decayMs; }

    void paint(juce::Graphics& g) override;

private:
    const Skin::SkinModel* currentSkin = nullptr;

    float currentLevel  = 0.0f;   // smoothed display level (linear)
    float targetLevel   = 0.0f;   // target from setLevel()
    float peakHoldLevel = 0.0f;
    float peakHoldTimer = 0.0f;

    Ballistic   ballistic   = Ballistic::VU;
    Orientation orientation = Orientation::Vertical;
    float       decayMs     = 300.0f;   // VU = 300ms, PPM = 1700ms
    juce::String channelName { "L" };
    bool showPeakHold = true;

    // Smoothing coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    void updateCoefficients();
    void smoothLevel();

    void drawVerticalBar(juce::Graphics& g, juce::Rectangle<int> bounds, float normalized);
    void drawHorizontalBar(juce::Graphics& g, juce::Rectangle<int> bounds, float normalized);
    juce::Colour levelToColour(float normalized) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkinnedVUMeter)
};
