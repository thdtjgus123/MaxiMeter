#pragma once

#include <JuceHeader.h>
#include "../Skin/SkinModel.h"
#include "BitmapFontRenderer.h"

//==============================================================================
/// SkinnedPlayerPanel — a fully interactive Winamp-style player control panel.
///
/// Renders the classic 275×116 Winamp main window using skin bitmaps, with
/// interactive transport buttons, position slider, volume & balance sliders,
/// time display, scrolling title, mono/stereo indicator, play state,
/// shuffle/repeat/EQ/PL toggle buttons, and a mini visualization area.
///
/// Native size: 275×116 pixels × scale factor (default 2).
///
/// Coordinates reference (all in native 275×116 space, matching Winamp spec):
///   Title bar:       (0, 0,  275×14)
///   Time display:    (48, 26) — 4 digits + colon via numbers.bmp
///   Play state:      (24, 28) — 9×9 indicator
///   Scrolling title: (111, 27, 154×6)
///   Mono/Stereo:     (212, 41) / (239, 41) — 29×12 / 27×12
///   Vis area:        (24, 43, 76×16)
///   Volume:          (107, 57, 68×15) + thumb 14×11
///   Balance:         (177, 57, 38×15) + thumb 14×11
///   Shuffle:         (164, 89, 47×15)
///   Repeat:          (210, 89, 28×15)
///   EQ toggle:       (219, 58, 23×12)
///   PL toggle:       (242, 58, 23×12)
///   Position bar:    (16, 72, 248×10) + thumb 29×10
///   Transport:       y=88 — prev(16), play(39), pause(62), stop(85), next(108), eject(136)
class SkinnedPlayerPanel : public juce::Component,
                           public juce::Timer
{
public:
    SkinnedPlayerPanel();
    ~SkinnedPlayerPanel() override;

    //--- Configuration ---
    void setSkinModel(const Skin::SkinModel* model);
    bool hasSkin() const { return skin != nullptr && skin->isLoaded(); }

    void setScale(int s);
    int getScale() const { return scale; }

    //--- Playback state setters (call from timer) ---
    void setTime(int minutes, int seconds);
    void setTitleText(const juce::String& text);

    enum class PlayState { Stopped, Playing, Paused };
    void setPlayState(PlayState state);
    void setMonoStereo(bool stereo);
    void setPosition(double normalised);       // 0..1
    void setVolume(double normalised);         // 0..1
    void setBalance(double normalised);        // 0..1  (0=left, 0.5=center, 1=right)

    /// Set spectrum data (20 bands in dB)
    void setSpectrumData(const float* data, int numBands);

    /// Set oscilloscope waveform
    void setOscilloscopeData(const float* data, int numSamples);

    enum class VisMode { Spectrum, Oscilloscope };
    void setVisMode(VisMode m) { visMode = m; }

    //--- Callbacks (set by owner) ---
    std::function<void()> onPrevClicked;
    std::function<void()> onPlayClicked;
    std::function<void()> onPauseClicked;
    std::function<void()> onStopClicked;
    std::function<void()> onNextClicked;
    std::function<void()> onEjectClicked;
    std::function<void(double)> onPositionChanged;     // 0..1
    std::function<void(double)> onVolumeChanged;       // 0..1
    std::function<void(double)> onBalanceChanged;      // 0..1
    std::function<void(bool)>   onShuffleToggled;
    std::function<void(bool)>   onRepeatToggled;

    //--- Toggle states ---
    void setShuffle(bool on) { shuffleOn = on; repaint(); }
    void setRepeat(bool on)  { repeatOn = on; repaint(); }
    bool isShuffle() const { return shuffleOn; }
    bool isRepeat() const { return repeatOn; }

    //--- Component overrides ---
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void timerCallback() override;

private:
    const Skin::SkinModel* skin = nullptr;
    BitmapFontRenderer fontRenderer;
    int scale = 2;

    // State
    int timeMinutes = 0, timeSeconds = 0;
    juce::String titleText { "MaxiMeter" };
    int scrollOffset = 0;
    PlayState playState = PlayState::Stopped;
    bool stereoMode = true;
    double positionValue = 0.0;     // 0..1
    double volumeValue = 0.78;      // 0..1
    double balanceValue = 0.5;      // 0..1  center
    bool shuffleOn = false;
    bool repeatOn = false;

    // Vis data
    VisMode visMode = VisMode::Spectrum;
    std::array<float, 20> specBands {};
    std::array<float, 512> oscSamples {};
    int oscSampleCount = 0;

    //--- Hit areas (in native coords) ---
    enum class HitZone
    {
        None,
        Prev, Play, Pause, Stop, Next, Eject,
        PositionBar, VolumeSlider, BalanceSlider,
        Shuffle, Repeat, EqToggle, PlToggle,
        Minimize, Shade, Close,
        VisArea, TimeDisplay
    };

    HitZone pressedZone = HitZone::None;
    bool isDraggingPosition = false;
    bool isDraggingVolume = false;
    bool isDraggingBalance = false;

    //--- Native-coord rectangles ---
    // Transport buttons (all 23×18 except eject 22×16)
    static constexpr int kBtnY = 88;
    juce::Rectangle<int> prevRect()     const { return { 16, kBtnY, 23, 18 }; }
    juce::Rectangle<int> playRect()     const { return { 39, kBtnY, 23, 18 }; }
    juce::Rectangle<int> pauseRect()    const { return { 62, kBtnY, 23, 18 }; }
    juce::Rectangle<int> stopRect()     const { return { 85, kBtnY, 23, 18 }; }
    juce::Rectangle<int> nextRect()     const { return { 108, kBtnY, 22, 18 }; }
    juce::Rectangle<int> ejectRect()    const { return { 136, kBtnY, 22, 16 }; }
    // Sliders
    juce::Rectangle<int> posBarRect()   const { return { 16, 72, 248, 10 }; }
    juce::Rectangle<int> volRect()      const { return { 107, 57, 68, 15 }; }
    juce::Rectangle<int> balRect()      const { return { 177, 57, 38, 15 }; }
    // Toggles
    juce::Rectangle<int> shuffleRect()  const { return { 164, 89, 47, 15 }; }
    juce::Rectangle<int> repeatRect()   const { return { 210, 89, 28, 15 }; }
    juce::Rectangle<int> eqBtnRect()    const { return { 219, 58, 23, 12 }; }
    juce::Rectangle<int> plBtnRect()    const { return { 242, 58, 23, 12 }; }
    // Title bar buttons
    juce::Rectangle<int> minimizeRect() const { return { 244, 3, 9, 9 }; }
    juce::Rectangle<int> shadeRect()    const { return { 254, 3, 9, 9 }; }
    juce::Rectangle<int> closeRect()    const { return { 264, 3, 9, 9 }; }
    // Vis area
    juce::Rectangle<int> visAreaRect()  const { return { 24, 43, 76, 16 }; }
    // Time area
    juce::Rectangle<int> timeAreaRect() const { return { 36, 26, 63, 13 }; }

    /// Convert mouse position to native coords (uses actual component size)
    juce::Point<int> toNative(juce::Point<int> p) const
    {
        float sx = static_cast<float>(getWidth())  / 275.0f;
        float sy = static_cast<float>(getHeight()) / 116.0f;
        return { static_cast<int>(p.x / sx), static_cast<int>(p.y / sy) };
    }

    /// Hit-test in native coords
    HitZone hitTest(juce::Point<int> nativePos) const;

    //--- Drawing helpers ---
    void drawSprite(juce::Graphics& g, Skin::SpriteID id, int nx, int ny) const;
    void drawBackground(juce::Graphics& g) const;
    void drawTitleBar(juce::Graphics& g) const;
    void drawTimeDisplay(juce::Graphics& g) const;
    void drawScrollingTitle(juce::Graphics& g) const;
    void drawPlayStateIndicator(juce::Graphics& g) const;
    void drawMonoStereo(juce::Graphics& g) const;
    void drawTransportButtons(juce::Graphics& g) const;
    void drawPositionBar(juce::Graphics& g) const;
    void drawVolumeSlider(juce::Graphics& g) const;
    void drawBalanceSlider(juce::Graphics& g) const;
    void drawShuffleRepeat(juce::Graphics& g) const;
    void drawEqPlToggles(juce::Graphics& g) const;
    void drawVisualization(juce::Graphics& g) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkinnedPlayerPanel)
};
