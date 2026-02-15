#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include "../Audio/StereoFieldAnalyzer.h"

//==============================================================================
/// LissajousScope — Lissajous/XY display for stereo audio analysis.
/// Modes: Lissajous (L vs R raw), XY (rotated 45°), Polar.
/// Supports trigger modes and zoom.
class LissajousScope : public juce::Component,
                       public MeterBase
{
public:
    enum class Mode { Lissajous, XY, Polar };
    enum class TriggerMode { Auto, Normal, Single, Free };

    LissajousScope();
    ~LissajousScope() override = default;

    /// Update with data from StereoFieldAnalyzer
    void update(const StereoFieldAnalyzer& analyzer);

    /// Configuration
    void setMode(Mode m)          { mode = m; repaint(); }
    void setTriggerMode(TriggerMode t) { trigger = t; }
    void setZoom(float z)         { zoom = juce::jlimit(0.25f, 8.0f, z); }
    void setTrailLength(int len)  { trailLength = juce::jlimit(256, 8192, len); }
    void setLineThickness(float t){ lineWidth = juce::jlimit(0.5f, 3.0f, t); }
    void setColour(juce::Colour c){ waveColour = c; }
    void setShowGrid(bool show)   { showGrid = show; }

    // Getters for export/serialization
    Mode getMode()        const { return mode; }
    int  getTrailLength() const { return trailLength; }
    bool getShowGrid()    const { return showGrid; }

    void paint(juce::Graphics& g) override;

private:
    Mode mode = Mode::Lissajous;
    TriggerMode trigger = TriggerMode::Free;
    float zoom = 1.0f;
    int trailLength = 2048;
    float lineWidth = 1.0f;
    juce::Colour waveColour { 0xFF00DD88 };
    bool showGrid = true;

    static constexpr int kMaxPoints = 4096;
    std::vector<StereoFieldAnalyzer::GonioPoint> points;
    int numPoints = 0;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LissajousScope)
};
