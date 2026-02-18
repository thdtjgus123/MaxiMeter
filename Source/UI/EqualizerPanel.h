#pragma once

#include <JuceHeader.h>
#include "../Skin/SkinModel.h"

//==============================================================================
/// EqualizerPanel -- Interactive 10-band Winamp-style graphic equalizer.
///
/// Supports two rendering modes:
///   1. **Skinned** -- draws using eqmain.bmp sprites from a Winamp .wsz skin.
///   2. **Original** -- draws a clean, modern EQ UI when no skin is loaded.
///
/// The EQ window is 275x116 native pixels (same as Winamp EQ panel).
/// Scaled by `scale` factor (default 2 = 550x232).
///
/// Native layout (matching Winamp spec):
///   Title bar:    (0, 0, 275x14)
///   ON button:    (14, 18, 26x12)
///   AUTO button:  (39, 18, 32x12)
///   Presets btn:  (217, 18, 44x12)
///   EQ graph:     (86, 17, 113x19)   -- spline response curve
///   Preamp:       (21, 38, 11x64)    -- vertical slider
///   Bands 1-10:   (78+i*18, 38, 11x64)  i=0..9  -- vertical sliders
///                 Band frequencies: 60 70 80 100 200 400 1K 3K 6K 12K (Hz approx)
///   Close btn:    (264, 3, 9x9)
class EqualizerPanel : public juce::Component
{
public:
    EqualizerPanel();
    ~EqualizerPanel() override = default;

    //--- Configuration ---
    void setSkinModel(const Skin::SkinModel* model);
    bool hasSkin() const { return skin != nullptr && skin->isLoaded() && skin->hasBitmap(Skin::SkinBitmap::EQMain); }
    const Skin::SkinModel* getSkinModel() const { return skin; }

    void setScale(int s);
    int getScale() const { return scale; }

    //--- EQ state ---
    static constexpr int kNumBands = 10;

    /// Get/set preamp value (-12..+12 dB, 0 = center)
    float getPreamp() const { return preampDb; }
    void  setPreamp(float db);

    /// Get/set band gain (-12..+12 dB, 0 = center)
    float getBandGain(int band) const;
    void  setBandGain(int band, float db);

    /// Get all band gains
    const std::array<float, kNumBands>& getBandGains() const { return bandGains; }

    /// EQ on/off
    bool isEqOn() const { return eqOn; }
    void setEqOn(bool on) { eqOn = on; repaint(); }

    /// Auto EQ on/off
    bool isAutoOn() const { return autoOn; }
    void setAutoOn(bool on) { autoOn = on; repaint(); }

    //--- Callbacks ---
    std::function<void(float)>       onPreampChanged;       ///< preamp dB
    std::function<void(int, float)>  onBandChanged;         ///< band index, dB
    std::function<void(bool)>        onEqToggled;           ///< on/off
    std::function<void(bool)>        onAutoToggled;         ///< auto on/off

    //--- Component overrides ---
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    /// Reset all bands to 0 dB (flat)
    void resetToFlat();

private:
    const Skin::SkinModel* skin = nullptr;
    int scale = 2;

    // State
    bool eqOn  = true;
    bool autoOn = false;
    float preampDb = 0.0f;                        ///< -12..+12
    std::array<float, kNumBands> bandGains {};     ///< -12..+12 each

    //--- Hit zones ---
    enum class HitZone
    {
        None,
        Preamp,
        Band0, Band1, Band2, Band3, Band4,
        Band5, Band6, Band7, Band8, Band9,
        OnButton, AutoButton, PresetsButton, Close
    };

    HitZone pressedZone = HitZone::None;
    bool    isDragging  = false;

    //--- Geometry (native 275x116 coords) ---
    static constexpr int kThumbW  = 11;    ///< thumb width
    static constexpr int kThumbH  = 11;    ///< thumb height
    static constexpr int kTrackW  = 14;    ///< slider track width
    static constexpr int kTrackH  = 63;    ///< slider track height
    static constexpr int kTrackTop = 38;   ///< slider track top Y
    static constexpr int kTravel  = kTrackH - kThumbH;  ///< 52px thumb travel

    juce::Rectangle<int> preampSliderRect() const { return { 21, kTrackTop, kTrackW, kTrackH }; }
    juce::Rectangle<int> bandSliderRect(int i) const { return { 78 + i * 18, kTrackTop, kTrackW, kTrackH }; }
    juce::Rectangle<int> onBtnRect()    const { return { 14, 18, 26, 12 }; }
    juce::Rectangle<int> autoBtnRect()  const { return { 39, 18, 32, 12 }; }
    juce::Rectangle<int> presetBtnRect() const { return { 217, 18, 44, 12 }; }
    juce::Rectangle<int> closeBtnRect() const { return { 264, 3, 9, 9 }; }
    juce::Rectangle<int> graphRect()    const { return { 86, 17, 113, 19 }; }

    /// Convert mouse position to native coords
    juce::Point<int> toNative(juce::Point<int> p) const
    {
        float sx = static_cast<float>(getWidth())  / 275.0f;
        float sy = static_cast<float>(getHeight()) / 116.0f;
        return { static_cast<int>(p.x / sx), static_cast<int>(p.y / sy) };
    }

    /// Convert dB value (-12..+12) to thumb offset within travel (0 = top, kTravel = bottom)
    int dbToThumbOffset(float db) const
    {
        // +12 dB = top (offset=0), -12 dB = bottom (offset=kTravel)
        float norm = (12.0f - db) / 24.0f;
        return juce::jlimit(0, kTravel, static_cast<int>(norm * kTravel));
    }

    /// Convert thumb offset to dB value
    float thumbOffsetToDb(int offset) const
    {
        float norm = static_cast<float>(offset) / static_cast<float>(kTravel);
        return juce::jlimit(-12.0f, 12.0f, 12.0f - norm * 24.0f);
    }

    HitZone hitTest(juce::Point<int> nativePos) const;
    void handleSliderDrag(juce::Point<int> nativePos);

    //--- Drawing helpers ---
    void drawSkinned(juce::Graphics& g) const;
    void drawOriginal(juce::Graphics& g) const;
    void drawSprite(juce::Graphics& g, Skin::SpriteID id, int nx, int ny) const;
    void drawEqGraph(juce::Graphics& g, juce::Rectangle<float> area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqualizerPanel)
};
