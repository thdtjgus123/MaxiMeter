#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include "../Skin/SkinModel.h"
#include <array>

//==============================================================================
/// SkinnedSpectrumAnalyzer — standalone spectrum analyzer component
/// that can use either Winamp skin viscolor.txt palette or a custom color scheme.
/// Supports 8/16/20/31/64 bands, linear/log scale, peak hold dots.
class SkinnedSpectrumAnalyzer : public juce::Component,
                                public MeterBase
{
public:
    SkinnedSpectrumAnalyzer();
    ~SkinnedSpectrumAnalyzer() override = default;

    /// Set the skin for viscolor palette (null for default colors)
    void setSkin(const Skin::SkinModel* skin) { currentSkin = skin; repaint(); }

    /// Set spectrum data (dB values, -60..0 range)
    void setSpectrumData(const float* data, int numBands);

    /// Configuration
    void setNumBands(int bands)         { numDisplayBands = juce::jlimit(8, 64, bands); }
    int  getNumBands() const            { return numDisplayBands; }
    void setPeakHoldEnabled(bool on)    { peakHoldEnabled = on; }
    void setBarGap(int pixels)          { barGap = pixels; }
    void setDecayRate(float dbPerSec)   { decayRate = dbPerSec; }

    // Getters for export/serialization
    float getDecayRate() const { return decayRate; }

    void paint(juce::Graphics& g) override;

private:
    const Skin::SkinModel* currentSkin = nullptr;

    static constexpr int kMaxBands = 64;
    int numDisplayBands = 20;
    bool peakHoldEnabled = true;
    int barGap = 1;
    float decayRate = 25.0f;  // dB per second

    std::array<float, kMaxBands> bandLevels {};     // current dB levels
    std::array<float, kMaxBands> smoothedLevels {}; // smoothed for display
    std::array<float, kMaxBands> peakLevels {};     // peak hold
    std::array<float, kMaxBands> peakTimers {};     // peak hold countdown

    juce::Colour getBarColour(float normalizedLevel) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkinnedSpectrumAnalyzer)
};
