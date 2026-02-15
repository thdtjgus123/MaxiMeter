#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include <vector>

//==============================================================================
/// LevelHistogram — displays the distribution of audio levels over time.
/// Shows how often each dB level occurs, displayed as a vertical/horizontal histogram.
/// Supports L/R separation, configurable bin resolution, cumulative/reset modes.
class LevelHistogram : public juce::Component,
                       public MeterBase
{
public:
    LevelHistogram();
    ~LevelHistogram() override = default;

    /// Push a new level sample (linear, 0..1+). Call at regular intervals (e.g., 60fps).
    void pushLevel(float leftLinear, float rightLinear);

    /// Configuration
    void setBinResolution(float dbPerBin)  { binRes = juce::jlimit(0.1f, 3.0f, dbPerBin); rebuildBins(); }
    void setDisplayRange(float minDb, float maxDb) { minRange = minDb; maxRange = maxDb; rebuildBins(); }
    void setShowStereo(bool show)          { showStereo = show; }
    void setCumulative(bool on)            { cumulative = on; }
    void setOrientation(bool horizontal)   { isHorizontal = horizontal; }

    // Getters for export/serialization
    float getMinDb()        const { return minRange; }
    float getMaxDb()        const { return maxRange; }
    float getBinResolution() const { return binRes; }
    bool  getCumulative()   const { return cumulative; }
    bool  getShowStereo()   const { return showStereo; }

    /// Reset accumulated histogram data
    void reset();

    void paint(juce::Graphics& g) override;

private:
    float binRes   = 0.5f;   // dB per bin
    float minRange = -60.0f;
    float maxRange = 0.0f;
    bool  showStereo = true;
    bool  cumulative = true;
    bool  isHorizontal = false;

    int numBins = 120;
    std::vector<double> binsL;
    std::vector<double> binsR;
    double totalSamples = 0;

    void rebuildBins();
    int dbToBin(float db) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelHistogram)
};
