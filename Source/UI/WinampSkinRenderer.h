#pragma once

#include <JuceHeader.h>
#include "../Skin/SkinModel.h"
#include "../Skin/SkinParser.h"
#include "BitmapFontRenderer.h"

//==============================================================================
/// WinampSkinRenderer — renders the full Winamp main window using skin data.
/// Composites the main.bmp background with titlebar, transport buttons,
/// time display, scrolling title text, mono/stereo indicator, and
/// the visualization area (spectrum/oscilloscope).
///
/// This component is sized at 275×116 (native) × scale factor.
class WinampSkinRenderer : public juce::Component,
                           public juce::Timer
{
public:
    WinampSkinRenderer();
    ~WinampSkinRenderer() override;

    /// Load a .wsz skin file
    bool loadSkin(const juce::File& wszFile);

    /// Apply a pre-loaded skin model (avoids re-parsing the .wsz file)
    void setSkinModel(const Skin::SkinModel* model);

    /// Get the loaded skin model (const ref)
    const Skin::SkinModel& getSkinModel() const { return skinModel; }
    bool hasSkin() const { return skinModel.isLoaded(); }

    /// Set the display scale (1 = native 275×116, 2 = double, etc.)
    void setScale(int newScale);
    int getScale() const { return scale; }

    /// Set the title text (scrolls if longer than display area)
    void setTitleText(const juce::String& text);

    /// Set the current playback time (for digit display)
    void setTime(int minutes, int seconds);

    /// Set playback state (for playpaus indicator)
    enum class PlayState { Stopped, Playing, Paused };
    void setPlayState(PlayState state);

    /// Set mono/stereo indicator
    void setMonoStereo(bool isStereo);

    /// Set spectrum data for the built-in visualization area
    /// `data` should be 20 float values in dB range (-60..0)
    void setSpectrumData(const float* data, int numBands);

    /// Set oscilloscope waveform data (raw audio samples, mono)
    void setOscilloscopeData(const float* data, int numSamples);

    /// Set visualization mode (spectrum bars or oscilloscope)
    enum class VisMode { Spectrum, Oscilloscope };
    void setVisMode(VisMode mode) { visMode = mode; repaint(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    Skin::SkinModel  skinModel;
    SkinParser       parser;
    BitmapFontRenderer fontRenderer;

    int scale = 2;

    // Display state
    juce::String titleText { "MaxiMeter" };
    int scrollOffset = 0;
    int timeMinutes = 0;
    int timeSeconds = 0;
    PlayState playState = PlayState::Stopped;
    bool stereoMode = true;

    // Visualization data
    VisMode visMode = VisMode::Spectrum;
    std::array<float, 20> spectrumBands {};  // 20-band Winamp spectrum
    std::array<float, 512> oscData {};
    int oscDataSize = 0;

    // Window focus state
    bool isWindowActive = true;

    // Painting helpers
    void drawBackground(juce::Graphics& g);
    void drawTitleBar(juce::Graphics& g);
    void drawTransportButtons(juce::Graphics& g);
    void drawTimeDisplay(juce::Graphics& g);
    void drawScrollingTitle(juce::Graphics& g);
    void drawMonoStereoIndicator(juce::Graphics& g);
    void drawPlayStateIndicator(juce::Graphics& g);
    void drawVisualization(juce::Graphics& g);
    void drawSpectrumVis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawOscilloscopeVis(juce::Graphics& g, juce::Rectangle<int> area);

    /// Draw a sprite at scaled coordinates
    void drawSprite(juce::Graphics& g, Skin::SpriteID id, int x, int y);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WinampSkinRenderer)
};
