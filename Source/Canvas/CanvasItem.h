#pragma once

#include <JuceHeader.h>
#include "../UI/MeterBase.h"
#include <memory>
#include <vector>

/// Identifies which type of meter a CanvasItem contains.
enum class MeterType
{
    MultiBandAnalyzer,
    Spectrogram,
    Goniometer,
    LissajousScope,
    LoudnessMeter,
    LevelHistogram,
    CorrelationMeter,
    PeakMeter,
    SkinnedSpectrum,
    SkinnedVUMeter,
    SkinnedOscilloscope,
    WinampSkin,
    WaveformView,
    ImageLayer,
    VideoLayer,
    SkinnedPlayer,
    // Shapes
    ShapeRectangle,
    ShapeEllipse,
    ShapeTriangle,
    ShapeLine,
    ShapeStar,
    TextLabel,
    // Custom Python plugins (GPU-accelerated)
    CustomPlugin,
    // --
    NumTypes
};

/// Human-readable names for meter types (for toolbox / layer panel).
inline juce::String meterTypeName(MeterType t)
{
    switch (t)
    {
        case MeterType::MultiBandAnalyzer:  return "Multi-Band Analyzer";
        case MeterType::Spectrogram:        return "Spectrogram";
        case MeterType::Goniometer:         return "Goniometer";
        case MeterType::LissajousScope:     return "Lissajous Scope";
        case MeterType::LoudnessMeter:      return "Loudness Meter";
        case MeterType::LevelHistogram:     return "Level Histogram";
        case MeterType::CorrelationMeter:   return "Correlation Meter";
        case MeterType::PeakMeter:          return "Peak Meter";
        case MeterType::SkinnedSpectrum:    return "Skinned Spectrum";
        case MeterType::SkinnedVUMeter:     return "Skinned VU Meter";
        case MeterType::SkinnedOscilloscope:return "Skinned Oscilloscope";
        case MeterType::WinampSkin:         return "Winamp Skin";
        case MeterType::WaveformView:       return "Waveform View";
        case MeterType::ImageLayer:        return "Image";
        case MeterType::VideoLayer:        return "Video / GIF";
        case MeterType::SkinnedPlayer:     return "Skinned Player";
        case MeterType::ShapeRectangle:    return "Rectangle";
        case MeterType::ShapeEllipse:      return "Ellipse";
        case MeterType::ShapeTriangle:     return "Triangle";
        case MeterType::ShapeLine:         return "Line";
        case MeterType::ShapeStar:         return "Star";
        case MeterType::TextLabel:         return "Text";
        case MeterType::CustomPlugin:      return "Custom Plugin";
        default: return "Unknown";
    }
}

/// Default size (width×height) when a new meter is first placed.
inline juce::Rectangle<int> meterDefaultSize(MeterType t)
{
    switch (t)
    {
        case MeterType::MultiBandAnalyzer:  return { 0, 0, 400, 250 };
        case MeterType::Spectrogram:        return { 0, 0, 400, 200 };
        case MeterType::Goniometer:         return { 0, 0, 250, 250 };
        case MeterType::LissajousScope:     return { 0, 0, 250, 250 };
        case MeterType::LoudnessMeter:      return { 0, 0, 300, 280 };
        case MeterType::LevelHistogram:     return { 0, 0, 300, 200 };
        case MeterType::CorrelationMeter:   return { 0, 0, 300, 30 };
        case MeterType::PeakMeter:          return { 0, 0, 80, 300 };
        case MeterType::SkinnedSpectrum:    return { 0, 0, 280, 100 };
        case MeterType::SkinnedVUMeter:     return { 0, 0, 120, 120 };
        case MeterType::SkinnedOscilloscope:return { 0, 0, 280, 100 };
        case MeterType::WinampSkin:         return { 0, 0, 550, 232 };
        case MeterType::WaveformView:       return { 0, 0, 500, 80 };
        case MeterType::ImageLayer:        return { 0, 0, 300, 300 };
        case MeterType::VideoLayer:        return { 0, 0, 400, 300 };
        case MeterType::SkinnedPlayer:     return { 0, 0, 550, 232 };
        case MeterType::ShapeRectangle:    return { 0, 0, 200, 150 };
        case MeterType::ShapeEllipse:      return { 0, 0, 200, 200 };
        case MeterType::ShapeTriangle:     return { 0, 0, 200, 180 };
        case MeterType::ShapeLine:         return { 0, 0, 300, 4 };
        case MeterType::ShapeStar:         return { 0, 0, 200, 200 };
        case MeterType::TextLabel:         return { 0, 0, 300, 60 };
        case MeterType::CustomPlugin:      return { 0, 0, 300, 200 };
        default: return { 0, 0, 200, 200 };
    }
}

//==============================================================================
/// A single item on the canvas – wraps a Component (meter) with canvas-space
/// transform (position, size, rotation), z-order, visibility, lock, and group.
struct CanvasItem
{
    juce::Uuid                          id;              ///< Globally unique
    MeterType                           meterType;
    std::unique_ptr<juce::Component>    component;       ///< The actual meter

    // Canvas-space geometry
    float                               x       = 0.0f;
    float                               y       = 0.0f;
    float                               width   = 200.0f;
    float                               height  = 200.0f;
    int                                 rotation = 0;     ///< 0 / 90 / 180 / 270

    // Layer
    int                                 zOrder  = 0;
    bool                                locked  = false;
    bool                                visible = true;
    juce::String                        name;
    juce::Uuid                          groupId;          ///< null Uuid = not grouped

    bool                                aspectLock = false;
    float                               opacity = 1.0f;   ///< 0.0 (invisible) to 1.0 (fully opaque)

    /// File path for ImageLayer / VideoLayer items.
    juce::String                        mediaFilePath;

    /// Custom plugin manifest ID and instance ID for CustomPlugin items.
    juce::String                        customPluginId;     ///< manifest id
    juce::String                        customInstanceId;   ///< bridge instance UUID

    /// VU meter channel: 0 = Left, 1 = Right
    int                                 vuChannel = 0;

    // ── Per-item background ──
    juce::Colour                        itemBackground { 0x00000000 }; ///< transparent by default

    // ── Meter colour overrides ──
    juce::Colour                        meterBgColour  { 0x00000000 }; ///< transparent = use default
    juce::Colour                        meterFgColour  { 0x00000000 }; ///< transparent = use default
    BlendMode                           blendMode      = BlendMode::Normal;

    // ── Shape properties ──
    juce::Colour                        fillColour1  { 0xFF3A7BFF };
    juce::Colour                        fillColour2  { 0xFF1A4ACA };
    int                                 gradientDirection = 0; ///< 0=solid, 1=vertical, 2=horizontal, 3=diagonal
    float                               cornerRadius = 0.0f;
    juce::Colour                        strokeColour { 0xFFFFFFFF };
    float                               strokeWidth  = 2.0f;
    int                                 strokeAlignment = 0; ///< 0=center, 1=inside, 2=outside
    int                                 lineCap      = 0;    ///< 0=butt, 1=round, 2=square
    int                                 starPoints   = 5;    ///< number of star spike points (3–20)
    float                               triangleRoundness = 0.0f; ///< 0..1 roundness for triangle corners

    // ── Loudness Meter ──
    float                               targetLUFS      = -14.0f;
    bool                                loudnessShowHistory = true;

    // ── Frosted Glass ──
    bool                                frostedGlass    = false;
    float                               blurRadius      = 10.0f;
    juce::Colour                        frostTint       { 0xFFFFFFFF };
    float                               frostOpacity    = 0.15f;

    // ── Text properties ──
    juce::String                        textContent  { "Text" };
    juce::String                        fontFamily   { "Arial" };
    float                               fontSize     = 24.0f;
    bool                                fontBold     = false;
    bool                                fontItalic   = false;
    juce::Colour                        textColour   { 0xFFFFFFFF };
    int                                 textAlignment = 0; ///< 0=left, 1=center, 2=right

    // ── Interactive mode (for SkinnedPlayer etc.) ──
    /// When true, mouse events go to the component instead of CanvasView.
    /// Toggle via double-click; exit via Escape or clicking outside.
    bool                                interactiveMode = false;

    /// Bounding rectangle in canvas space.
    juce::Rectangle<float> getBounds() const { return { x, y, width, height }; }
    void setBounds(juce::Rectangle<float> r) { x = r.getX(); y = r.getY(); width = r.getWidth(); height = r.getHeight(); }

    /// Centre point in canvas space.
    juce::Point<float> getCentre() const { return { x + width * 0.5f, y + height * 0.5f }; }
};
