#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"
#include "../Skin/SkinModel.h"

//==============================================================================
/// SkinnedOscilloscope — waveform display for Winamp-style vis area or standalone.
/// Renders a time-domain waveform with configurable line width and color.
class SkinnedOscilloscope : public juce::Component,
                             public MeterBase
{
public:
    SkinnedOscilloscope();
    ~SkinnedOscilloscope() override = default;

    /// Push a new buffer of samples (mono, -1..1)
    void pushSamples(const float* data, int numSamples);

    /// Set the skin for themed rendering
    void setSkin(const Skin::SkinModel* skin) { currentSkin = skin; repaint(); }

    /// Configuration
    void setLineThickness(float thickness) { lineWidth = thickness; }
    void setWaveformColour(juce::Colour c) { waveColour = c; }
    void setBackgroundColour(juce::Colour c) { bgColour = c; }
    void setDrawStyle(int style) { drawStyle = juce::jlimit(0, 2, style); }
    //  0 = line, 1 = dots, 2 = solid fill

    // Getters for export/serialization
    float getLineThickness() const { return lineWidth; }
    int   getDrawStyle()     const { return drawStyle; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    std::vector<float> displayBuffer;
    int writePos = 0;
    int displaySamples = 576;  // Winamp default oscilloscope width

    const Skin::SkinModel* currentSkin = nullptr;

    float lineWidth = 1.5f;
    juce::Colour waveColour { 0xFF00FF00 };
    juce::Colour bgColour   { 0xFF000000 };
    int drawStyle = 0;

    void drawLineWaveform(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawDotWaveform(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawFilledWaveform(juce::Graphics& g, juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkinnedOscilloscope)
};
