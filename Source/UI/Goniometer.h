#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include "../Audio/StereoFieldAnalyzer.h"

//==============================================================================
/// Goniometer — stereo phase scope (vectorscope) display.
/// Shows L/R distribution as polar scatter plot with afterglow trail.
/// Supports M/S visualization and phase correlation indicator.
class Goniometer : public juce::Component,
                   public MeterBase
{
public:
    Goniometer();
    ~Goniometer() override = default;

    /// Update with latest goniometer data from StereoFieldAnalyzer
    void update(const StereoFieldAnalyzer& analyzer);

    /// Configuration
    void setTrailOpacity(float opacity) { trailAlpha = juce::jlimit(0.0f, 1.0f, opacity); }
    void setDotSize(float size)         { dotSize = juce::jlimit(0.5f, 4.0f, size); }
    void setShowGrid(bool show)         { showGrid = show; }
    void setShowCorrelation(bool show)  { showCorrelationBar = show; }
    void setZoom(float z)               { zoom = juce::jlimit(0.5f, 4.0f, z); }

    // Getters for export/serialization
    float getDotSize()      const { return dotSize; }
    float getTrailOpacity() const { return trailAlpha; }
    bool  getShowGrid()     const { return showGrid; }

    void paint(juce::Graphics& g) override;

private:
    static constexpr int kMaxPoints = 4096;
    std::vector<StereoFieldAnalyzer::GonioPoint> points;
    int numPoints = 0;
    float correlationValue = 0.0f;

    float trailAlpha = 0.6f;
    float dotSize = 1.5f;
    bool showGrid = true;
    bool showCorrelationBar = true;
    float zoom = 1.0f;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawCorrelationBar(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Goniometer)
};
