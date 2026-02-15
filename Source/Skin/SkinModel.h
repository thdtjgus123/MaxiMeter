#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
#include <vector>

//==============================================================================
// Data structures for a fully parsed Winamp Classic Skin (.wsz)
//==============================================================================

namespace Skin
{

//----------------------------------------------------------------------
// Sprite definition: a named rectangular region within a source BMP
//----------------------------------------------------------------------
struct SpriteRect
{
    int x = 0, y = 0, w = 0, h = 0;

    juce::Rectangle<int> toRect() const { return { x, y, w, h }; }
    bool isValid() const { return w > 0 && h > 0; }
};

//----------------------------------------------------------------------
// All known source BMP filenames in a .wsz skin
//----------------------------------------------------------------------
enum class SkinBitmap
{
    Main,           // main.bmp
    Titlebar,       // titlebar.bmp
    CButtons,       // cbuttons.bmp
    Volume,         // volume.bmp
    Balance,        // balance.bmp
    PosBar,         // posbar.bmp
    Numbers,        // numbers.bmp
    NumbersEx,      // nums_ex.bmp (optional)
    Text,           // text.bmp
    MonoStereo,     // monoster.bmp
    PlayPause,      // playpaus.bmp
    ShufRep,        // shufrep.bmp
    EQMain,         // eqmain.bmp
    EQEx,           // eq_ex.bmp (optional)
    PleditBmp,      // pledit.bmp
    Count
};

inline const char* bitmapFilename(SkinBitmap bmp)
{
    switch (bmp)
    {
        case SkinBitmap::Main:       return "main.bmp";
        case SkinBitmap::Titlebar:   return "titlebar.bmp";
        case SkinBitmap::CButtons:   return "cbuttons.bmp";
        case SkinBitmap::Volume:     return "volume.bmp";
        case SkinBitmap::Balance:    return "balance.bmp";
        case SkinBitmap::PosBar:     return "posbar.bmp";
        case SkinBitmap::Numbers:    return "numbers.bmp";
        case SkinBitmap::NumbersEx:  return "nums_ex.bmp";
        case SkinBitmap::Text:       return "text.bmp";
        case SkinBitmap::MonoStereo: return "monoster.bmp";
        case SkinBitmap::PlayPause:  return "playpaus.bmp";
        case SkinBitmap::ShufRep:    return "shufrep.bmp";
        case SkinBitmap::EQMain:     return "eqmain.bmp";
        case SkinBitmap::EQEx:       return "eq_ex.bmp";
        case SkinBitmap::PleditBmp:  return "pledit.bmp";
        default:                     return "";
    }
}

//----------------------------------------------------------------------
// Named sprite IDs (comprehensive list from Webamp skinSprites.ts)
//----------------------------------------------------------------------
enum class SpriteID
{
    // main.bmp
    MainBackground,

    // titlebar.bmp
    TitleBar, TitleBarSelected,
    TitleBarEaster, TitleBarEasterSelected,
    OptionsButton, OptionsButtonPressed,
    MinimizeButton, MinimizeButtonPressed,
    ShadeButton, ShadeButtonPressed,
    CloseButton, CloseButtonPressed,
    ClutterBarBg, ClutterBarBgDisabled,
    ShadeBackground, ShadeBackgroundSelected,
    ShadeButtonSelected, ShadeButtonSelectedPressed,
    ShadePositionBg, ShadePositionThumb, ShadePositionThumbLeft, ShadePositionThumbRight,

    // cbuttons.bmp
    PreviousButton, PreviousButtonPressed,
    PlayButton, PlayButtonPressed,
    PauseButton, PauseButtonPressed,
    StopButton, StopButtonPressed,
    NextButton, NextButtonPressed,
    EjectButton, EjectButtonPressed,

    // volume.bmp
    VolumeBackground,            // full 68x420 strip (28 frames of 68x15)
    VolumeThumb, VolumeThumbSelected,

    // balance.bmp
    BalanceBackground,           // 38x420 strip @ x=9
    BalanceThumb, BalanceThumbSelected,

    // posbar.bmp
    PositionBackground,
    PositionThumb, PositionThumbSelected,

    // numbers.bmp
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,
    DigitBlank, DigitMinus,

    // monoster.bmp
    StereoActive, StereoInactive,
    MonoActive, MonoInactive,

    // playpaus.bmp
    PlayingIndicator, PausedIndicator, StoppedIndicator,
    WorkingIndicator, NotWorkingIndicator,

    // shufrep.bmp
    ShuffleButton, ShuffleButtonPressed,
    ShuffleButtonSelected, ShuffleButtonSelectedPressed,
    RepeatButton, RepeatButtonPressed,
    RepeatButtonSelected, RepeatButtonSelectedPressed,
    EqButton, EqButtonSelected,
    EqButtonPressed, EqButtonPressedSelected,
    PlButton, PlButtonSelected,
    PlButtonPressed, PlButtonPressedSelected,

    // eqmain.bmp
    EqBackground,
    EqTitleBar, EqTitleBarSelected,
    EqSliderBg, EqSliderThumb, EqSliderThumbSelected,
    EqCloseButton, EqCloseButtonActive,
    EqOnButton, EqOnButtonPressed, EqOnButtonSelected, EqOnButtonSelectedPressed,
    EqAutoButton, EqAutoButtonPressed, EqAutoButtonSelected, EqAutoButtonSelectedPressed,
    EqGraphBg, EqGraphLineColors,
    EqPresetsButton, EqPresetsButtonSelected,

    Count
};

//----------------------------------------------------------------------
// Mapping from SpriteID → source bitmap + coordinates
//----------------------------------------------------------------------
struct SpriteDef
{
    SkinBitmap bitmap;
    SpriteRect rect;
};

/// Returns the full sprite definition table (populated in SkinSpriteTable.cpp)
const std::array<SpriteDef, static_cast<size_t>(SpriteID::Count)>& getSpriteTable();

//----------------------------------------------------------------------
// viscolor.txt — 24-color palette for spectrum/oscilloscope
//----------------------------------------------------------------------
struct VisColors
{
    static constexpr int kNumColors = 24;
    std::array<juce::Colour, kNumColors> colors;

    VisColors();   // fills with default Winamp palette
};

//----------------------------------------------------------------------
// region.txt — polygon regions for non-rectangular windows
//----------------------------------------------------------------------
struct Region
{
    int numPoints = 0;
    std::vector<juce::Point<int>> points;
};

struct RegionData
{
    Region normal;
    Region windowShade;
    Region equalizer;
};

//----------------------------------------------------------------------
// pledit.txt — playlist editor font and color config
//----------------------------------------------------------------------
struct PleditConfig
{
    juce::Colour normalText    { 0xFF00FF00 };   // green
    juce::Colour currentText   { 0xFFFFFFFF };
    juce::Colour normalBg      { 0xFF000000 };
    juce::Colour selectedBg    { 0xFF0000FF };
    juce::String fontName      { "Arial" };
};

//----------------------------------------------------------------------
// Bitmap font character map (from text.bmp)
//----------------------------------------------------------------------
struct BitmapFont
{
    static constexpr int kCharWidth  = 5;
    static constexpr int kCharHeight = 6;

    /// Get the sprite rect for a given ASCII character
    static SpriteRect getCharRect(char c);
};

//----------------------------------------------------------------------
// SkinModel — the complete parsed skin data
//----------------------------------------------------------------------
struct SkinModel
{
    /// Source name / file path
    juce::String skinName;
    juce::File   skinFile;

    /// All loaded BMP images, keyed by SkinBitmap enum
    std::array<juce::Image, static_cast<size_t>(SkinBitmap::Count)> bitmaps;

    /// Whether each bitmap was successfully loaded
    std::array<bool, static_cast<size_t>(SkinBitmap::Count)> bitmapLoaded {};

    /// Visualization color palette
    VisColors visColors;

    /// Region definitions
    RegionData regions;

    /// Playlist config
    PleditConfig pleditConfig;

    /// Skin readme text
    juce::String readmeText;

    //--- Helpers ---

    /// Get a specific sprite image (clipped from source bitmap)
    juce::Image getSprite(SpriteID id) const;

    /// Check if the skin has a specific bitmap
    bool hasBitmap(SkinBitmap bmp) const
    {
        return bitmapLoaded[static_cast<size_t>(bmp)];
    }

    /// Check if the skin is loaded at all
    bool isLoaded() const
    {
        return hasBitmap(SkinBitmap::Main);
    }
};

} // namespace Skin
