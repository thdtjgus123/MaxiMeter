#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include <vector>

//==============================================================================
/// Spectrogram — waterfall spectrogram display with configurable colormaps.
/// Renders a scrolling time-frequency heat map from FFT spectrum data.
/// Supports an optional time-frequency reassignment mode for sharper resolution.
class Spectrogram : public juce::Component,
                    public MeterBase
{
public:
    enum class ColourMap { Rainbow, Heat, Greyscale, Custom };
    enum class ScrollDirection { Horizontal, Vertical };

    Spectrogram();
    ~Spectrogram() override = default;

    /// Push a new column of FFT data (magnitude in dB, -60..0 range).
    /// `numBins` should match fftSize/2.
    void pushSpectrum(const float* data, int numBins);

    /// Push a new frame using raw complex FFT output for reassignment.
    /// `complexData` contains interleaved (re, im) pairs; `fftSize` = total complex size (numBins*2).
    /// `hopSizeSeconds` is the time between consecutive frames (frame advance / sampleRate).
    /// When reassigned mode is disabled this falls back to magnitude-based rendering.
    void pushSpectrumComplex(const float* complexData, int fftSize, double hopSizeSeconds);

    /// Configuration
    void setColourMap(ColourMap map)           { colourMap = map; updatePalette(); }
    void setScrollDirection(ScrollDirection d) { scrollDir = d; }
    void setDynamicRange(float minDb, float maxDb) { minDbRange = minDb; maxDbRange = maxDb; }
    void setFrequencyRange(float minHz, float maxHz) { minFreq = minHz; maxFreq = maxHz; }
    void setSampleRate(double sr) { sampleRate = sr; }
    void setReassignedMode(bool r) { reassignedMode = r; prevPhase.clear(); }

    // Getters for export/serialization
    ColourMap       getColourMap()      const { return colourMap; }
    ScrollDirection getScrollDirection() const { return scrollDir; }
    float           getMinDb()          const { return minDbRange; }
    float           getMaxDb()          const { return maxDbRange; }
    bool            isReassignedMode()  const { return reassignedMode; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ColourMap colourMap = ColourMap::Heat;
    ScrollDirection scrollDir = ScrollDirection::Horizontal;
    float minDbRange = -60.0f;
    float maxDbRange = 0.0f;
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    double sampleRate = 44100.0;
    bool reassignedMode = false;

    // Image buffer for waterfall
    juce::Image spectrogramImage;
    int writeColumn = 0;

    // Palette (256 entries)
    std::vector<juce::Colour> palette;
    void updatePalette();
    juce::Colour dbToColour(float db) const;

    // Map frequency bin to display Y position (log scale)
    int binToY(int bin, int numBins, int displayHeight) const;

    // Previous-frame phases for reassignment (one entry per FFT bin)
    std::vector<float> prevPhase;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spectrogram)
};
