#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include "../Skin/SkinModel.h"
#include <array>
#include <vector>

//==============================================================================
/// MultiBandAnalyzer — multi-band frequency analyzer with configurable band count
/// and scale modes (Log/Linear/Octave).
/// Supports 8/16/20/31/64 bands, peak hold, dB grid, frequency labels.
class MultiBandAnalyzer : public juce::Component,
                         public MeterBase
{
public:
    enum class ScaleMode { Logarithmic, Linear, Octave };
    enum class BarStyle  { Filled, LED, Outline };

    MultiBandAnalyzer();
    ~MultiBandAnalyzer() override = default;

    /// Set spectrum data (dB values, -60..0 range)
    void setSpectrumData(const float* data, int numBins, double sampleRate);

    /// Configuration
    void setNumBands(int bands)          { numBands = juce::jlimit(8, 64, bands); }
    int  getNumBands() const             { return numBands; }
    void setScaleMode(ScaleMode mode)    { scaleMode = mode; }
    void setBarStyle(BarStyle style)     { barStyle = style; }
    void setPeakHoldEnabled(bool on)     { peakHoldEnabled = on; }
    void setShowGrid(bool show)          { showGrid = show; }
    void setShowFreqLabels(bool show)    { showFreqLabels = show; }
    void setDecayRate(float dbPerSec)    { decayRate = juce::jlimit(5.0f, 60.0f, dbPerSec); }
    void setDynamicRange(float minDb, float maxDb) { minRange = minDb; maxRange = maxDb; }
    void setSkin(const Skin::SkinModel* skin) { currentSkin = skin; repaint(); }

    // Getters for export/serialization
    float     getDecayRate() const { return decayRate; }
    float     getMinDb()     const { return minRange; }
    float     getMaxDb()     const { return maxRange; }
    ScaleMode getScaleMode() const { return scaleMode; }

    void paint(juce::Graphics& g) override;

private:
    const Skin::SkinModel* currentSkin = nullptr;

    int numBands = 31;
    ScaleMode scaleMode  = ScaleMode::Logarithmic;
    BarStyle  barStyle   = BarStyle::Filled;
    bool peakHoldEnabled = true;
    bool showGrid        = true;
    bool showFreqLabels  = true;
    float decayRate      = 25.0f;  // dB/sec
    float minRange       = -60.0f;
    float maxRange       = 0.0f;

    static constexpr int kMaxBands = 64;
    std::array<float, kMaxBands> bandLevels {};
    std::array<float, kMaxBands> smoothed {};
    std::array<float, kMaxBands> peaks {};
    std::array<float, kMaxBands> peakTimers {};

    struct BandInfo { float centerFreq; float lowFreq; float highFreq; };
    std::vector<BandInfo> bandInfos;

    void computeBandBoundaries(int numBins, double sampleRate);
    float dbToNormalized(float db) const;
    juce::Colour getBarColour(float normalized, int band) const;
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiBandAnalyzer)
};
