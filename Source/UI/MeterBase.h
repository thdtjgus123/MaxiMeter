#pragma once

#include <JuceHeader.h>

//==============================================================================
/// Blend modes for canvas item compositing.
enum class BlendMode
{
    Normal = 0,     ///< Standard rendering with opaque background
    Add,            ///< Transparent background, bright foreground (additive feel)
    Multiply,       ///< Transparent background, darkened foreground
    Screen,         ///< Transparent background, brightened foreground
    Overlay         ///< Transparent background, high-contrast foreground
};

inline juce::String blendModeName(BlendMode m)
{
    switch (m)
    {
        case BlendMode::Normal:   return "Normal";
        case BlendMode::Add:      return "Add";
        case BlendMode::Multiply: return "Multiply";
        case BlendMode::Screen:   return "Screen";
        case BlendMode::Overlay:  return "Overlay";
        default: return "Normal";
    }
}

inline int blendModeCount() { return 5; }

//==============================================================================
/// Mixin providing background/foreground colour overrides and blend mode
/// support for all meter/visualizer components.
///
/// Usage:
///   class MyMeter : public juce::Component, public MeterBase { ... };
///
/// In paint():
///   g.fillAll(getBgColour(juce::Colour(0xFF0A0A1A)));
///   auto col = tintFg(someDefaultColour);
class MeterBase
{
public:
    virtual ~MeterBase() = default;

    void setMeterBgColour(juce::Colour c) { meterBg_ = c; }
    void setMeterFgColour(juce::Colour c) { meterFg_ = c; }
    void setBlendMode(BlendMode m)        { blendMode_ = m; }

    juce::Colour getMeterBgColour() const { return meterBg_; }
    juce::Colour getMeterFgColour() const { return meterFg_; }
    BlendMode    getBlendMode()     const { return blendMode_; }

    bool hasCustomBg() const { return meterBg_.getAlpha() > 0; }
    bool hasCustomFg() const { return meterFg_.getAlpha() > 0; }

    // ── Font overrides ──────────────────────────────────────────────────
    void setMeterFontSize(float size)                     { meterFontSize_ = size; }
    float getMeterFontSize() const                        { return meterFontSize_; }
    void setMeterFontFamily(const juce::String& family)   { meterFontFamily_ = family; }
    juce::String getMeterFontFamily() const               { return meterFontFamily_; }

protected:
    juce::Colour meterBg_ { 0x00000000 };   ///< transparent = use built-in default
    juce::Colour meterFg_ { 0x00000000 };   ///< transparent = use built-in default
    BlendMode    blendMode_ = BlendMode::Normal;
    float        meterFontSize_   = 12.0f;          ///< reference font size (slider default = 12)
    juce::String meterFontFamily_ { "Default" };    ///< "Default" = use built-in sans-serif

    /// Build a juce::Font scaled proportionally from the user's font-size setting.
    /// @param defaultSize  the original hardcoded size for this particular text element.
    juce::Font meterFont(float defaultSize) const
    {
        const float scale = meterFontSize_ / 12.0f;
        const float size  = defaultSize * scale;
        if (meterFontFamily_ == "Default" || meterFontFamily_.isEmpty())
            return juce::Font(size);
        return juce::Font(meterFontFamily_, size, juce::Font::plain);
    }

    /// Return the background colour to use in paint().
    /// Non-Normal blend modes yield a transparent background.
    juce::Colour getBgColour(juce::Colour defaultBg) const
    {
        if (blendMode_ != BlendMode::Normal)
            return juce::Colours::transparentBlack;
        return hasCustomBg() ? meterBg_ : defaultBg;
    }

    /// Tint a foreground colour: applies custom FG blend + blend mode adjustment.
    juce::Colour tintFg(juce::Colour c) const
    {
        if (hasCustomFg())
            c = c.interpolatedWith(meterFg_, 0.7f);

        switch (blendMode_)
        {
            case BlendMode::Add:      return c.brighter(0.3f);
            case BlendMode::Multiply: return c.darker(0.3f);
            case BlendMode::Screen:   return c.brighter(0.5f);
            case BlendMode::Overlay:  return c.withMultipliedSaturation(1.4f).brighter(0.15f);
            default: return c;
        }
    }

    /// Tint a secondary/panel colour (bar backgrounds, borders, etc.)
    juce::Colour tintSecondary(juce::Colour c) const
    {
        if (blendMode_ != BlendMode::Normal)
            return c.withAlpha(c.getFloatAlpha() * 0.5f);
        return c;
    }
};
