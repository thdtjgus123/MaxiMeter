#include "SkinModel.h"

namespace Skin
{

//==============================================================================
// Complete Winamp Classic skin sprite coordinate table
// Source: Webamp skinSprites.ts (https://github.com/captbaritone/webamp)
//==============================================================================

static const std::array<SpriteDef, static_cast<size_t>(SpriteID::Count)> kSpriteTable = []()
{
    std::array<SpriteDef, static_cast<size_t>(SpriteID::Count)> t {};

    auto set = [&](SpriteID id, SkinBitmap bmp, int x, int y, int w, int h) {
        auto i = static_cast<size_t>(id);
        t[i] = { bmp, { x, y, w, h } };
    };

    // ---- main.bmp ----
    set(SpriteID::MainBackground,    SkinBitmap::Main, 0, 0, 275, 116);

    // ---- titlebar.bmp ----
    set(SpriteID::TitleBar,                    SkinBitmap::Titlebar, 27, 15, 275, 14);
    set(SpriteID::TitleBarSelected,            SkinBitmap::Titlebar, 27,  0, 275, 14);
    set(SpriteID::TitleBarEaster,              SkinBitmap::Titlebar, 27, 72, 275, 14);
    set(SpriteID::TitleBarEasterSelected,      SkinBitmap::Titlebar, 27, 57, 275, 14);
    set(SpriteID::OptionsButton,               SkinBitmap::Titlebar,  0,  0,   9,  9);
    set(SpriteID::OptionsButtonPressed,        SkinBitmap::Titlebar,  0,  9,   9,  9);
    set(SpriteID::MinimizeButton,              SkinBitmap::Titlebar,  9,  0,   9,  9);
    set(SpriteID::MinimizeButtonPressed,       SkinBitmap::Titlebar,  9,  9,   9,  9);
    set(SpriteID::ShadeButton,                 SkinBitmap::Titlebar,  0, 18,   9,  9);
    set(SpriteID::ShadeButtonPressed,          SkinBitmap::Titlebar,  9, 18,   9,  9);
    set(SpriteID::CloseButton,                 SkinBitmap::Titlebar, 18,  0,   9,  9);
    set(SpriteID::CloseButtonPressed,          SkinBitmap::Titlebar, 18,  9,   9,  9);
    set(SpriteID::ClutterBarBg,                SkinBitmap::Titlebar, 304, 0,   8, 43);
    set(SpriteID::ClutterBarBgDisabled,        SkinBitmap::Titlebar, 312, 0,   8, 43);
    set(SpriteID::ShadeBackground,             SkinBitmap::Titlebar, 27, 42, 275, 14);
    set(SpriteID::ShadeBackgroundSelected,     SkinBitmap::Titlebar, 27, 29, 275, 14);
    set(SpriteID::ShadeButtonSelected,         SkinBitmap::Titlebar,  0, 27,   9,  9);
    set(SpriteID::ShadeButtonSelectedPressed,  SkinBitmap::Titlebar,  9, 27,   9,  9);
    set(SpriteID::ShadePositionBg,             SkinBitmap::Titlebar,  0, 36,  17,  7);
    set(SpriteID::ShadePositionThumb,          SkinBitmap::Titlebar, 20, 36,   3,  7);
    set(SpriteID::ShadePositionThumbLeft,      SkinBitmap::Titlebar, 17, 36,   3,  7);
    set(SpriteID::ShadePositionThumbRight,     SkinBitmap::Titlebar, 23, 36,   3,  7);

    // ---- cbuttons.bmp (136×36) ----
    set(SpriteID::PreviousButton,        SkinBitmap::CButtons,   0,  0, 23, 18);
    set(SpriteID::PreviousButtonPressed, SkinBitmap::CButtons,   0, 18, 23, 18);
    set(SpriteID::PlayButton,            SkinBitmap::CButtons,  23,  0, 23, 18);
    set(SpriteID::PlayButtonPressed,     SkinBitmap::CButtons,  23, 18, 23, 18);
    set(SpriteID::PauseButton,           SkinBitmap::CButtons,  46,  0, 23, 18);
    set(SpriteID::PauseButtonPressed,    SkinBitmap::CButtons,  46, 18, 23, 18);
    set(SpriteID::StopButton,            SkinBitmap::CButtons,  69,  0, 23, 18);
    set(SpriteID::StopButtonPressed,     SkinBitmap::CButtons,  69, 18, 23, 18);
    set(SpriteID::NextButton,            SkinBitmap::CButtons,  92,  0, 23, 18);
    set(SpriteID::NextButtonPressed,     SkinBitmap::CButtons,  92, 18, 22, 18);
    set(SpriteID::EjectButton,           SkinBitmap::CButtons, 114,  0, 22, 16);
    set(SpriteID::EjectButtonPressed,    SkinBitmap::CButtons, 114, 16, 22, 16);

    // ---- volume.bmp (68×420 strip + thumb) ----
    set(SpriteID::VolumeBackground,      SkinBitmap::Volume,  0,   0, 68, 420);
    set(SpriteID::VolumeThumb,           SkinBitmap::Volume, 15, 422, 14,  11);
    set(SpriteID::VolumeThumbSelected,   SkinBitmap::Volume,  0, 422, 14,  11);

    // ---- balance.bmp ----
    set(SpriteID::BalanceBackground,     SkinBitmap::Balance,  9,   0, 38, 420);
    set(SpriteID::BalanceThumb,          SkinBitmap::Balance, 15, 422, 14,  11);
    set(SpriteID::BalanceThumbSelected,  SkinBitmap::Balance,  0, 422, 14,  11);

    // ---- posbar.bmp ----
    set(SpriteID::PositionBackground,     SkinBitmap::PosBar,   0, 0, 248, 10);
    set(SpriteID::PositionThumb,          SkinBitmap::PosBar, 248, 0,  29, 10);
    set(SpriteID::PositionThumbSelected,  SkinBitmap::PosBar, 278, 0,  29, 10);

    // ---- numbers.bmp (9×13 each digit) ----
    set(SpriteID::Digit0,      SkinBitmap::Numbers,  0, 0, 9, 13);
    set(SpriteID::Digit1,      SkinBitmap::Numbers,  9, 0, 9, 13);
    set(SpriteID::Digit2,      SkinBitmap::Numbers, 18, 0, 9, 13);
    set(SpriteID::Digit3,      SkinBitmap::Numbers, 27, 0, 9, 13);
    set(SpriteID::Digit4,      SkinBitmap::Numbers, 36, 0, 9, 13);
    set(SpriteID::Digit5,      SkinBitmap::Numbers, 45, 0, 9, 13);
    set(SpriteID::Digit6,      SkinBitmap::Numbers, 54, 0, 9, 13);
    set(SpriteID::Digit7,      SkinBitmap::Numbers, 63, 0, 9, 13);
    set(SpriteID::Digit8,      SkinBitmap::Numbers, 72, 0, 9, 13);
    set(SpriteID::Digit9,      SkinBitmap::Numbers, 81, 0, 9, 13);
    set(SpriteID::DigitBlank,  SkinBitmap::Numbers, 90, 0, 9, 13);
    set(SpriteID::DigitMinus,  SkinBitmap::Numbers, 99, 0, 9, 13);

    // ---- monoster.bmp (56×24) ----
    set(SpriteID::StereoActive,    SkinBitmap::MonoStereo,  0,  0, 29, 12);
    set(SpriteID::StereoInactive,  SkinBitmap::MonoStereo,  0, 12, 29, 12);
    set(SpriteID::MonoActive,      SkinBitmap::MonoStereo, 29,  0, 27, 12);
    set(SpriteID::MonoInactive,    SkinBitmap::MonoStereo, 29, 12, 27, 12);

    // ---- playpaus.bmp ----
    set(SpriteID::PlayingIndicator,     SkinBitmap::PlayPause,  0, 0, 9, 9);
    set(SpriteID::PausedIndicator,      SkinBitmap::PlayPause,  9, 0, 9, 9);
    set(SpriteID::StoppedIndicator,     SkinBitmap::PlayPause, 18, 0, 9, 9);
    set(SpriteID::WorkingIndicator,     SkinBitmap::PlayPause, 39, 0, 9, 9);
    set(SpriteID::NotWorkingIndicator,  SkinBitmap::PlayPause, 36, 0, 9, 9);

    // ---- shufrep.bmp ----
    set(SpriteID::ShuffleButton,                SkinBitmap::ShufRep, 28,  0, 47, 15);
    set(SpriteID::ShuffleButtonPressed,         SkinBitmap::ShufRep, 28, 15, 47, 15);
    set(SpriteID::ShuffleButtonSelected,        SkinBitmap::ShufRep, 28, 30, 47, 15);
    set(SpriteID::ShuffleButtonSelectedPressed, SkinBitmap::ShufRep, 28, 45, 47, 15);
    set(SpriteID::RepeatButton,                 SkinBitmap::ShufRep,  0,  0, 28, 15);
    set(SpriteID::RepeatButtonPressed,          SkinBitmap::ShufRep,  0, 15, 28, 15);
    set(SpriteID::RepeatButtonSelected,         SkinBitmap::ShufRep,  0, 30, 28, 15);
    set(SpriteID::RepeatButtonSelectedPressed,  SkinBitmap::ShufRep,  0, 45, 28, 15);
    set(SpriteID::EqButton,                     SkinBitmap::ShufRep,  0, 61, 23, 12);
    set(SpriteID::EqButtonSelected,             SkinBitmap::ShufRep,  0, 73, 23, 12);
    set(SpriteID::EqButtonPressed,              SkinBitmap::ShufRep, 46, 61, 23, 12);
    set(SpriteID::EqButtonPressedSelected,      SkinBitmap::ShufRep, 46, 73, 23, 12);
    set(SpriteID::PlButton,                     SkinBitmap::ShufRep, 23, 61, 23, 12);
    set(SpriteID::PlButtonSelected,             SkinBitmap::ShufRep, 23, 73, 23, 12);
    set(SpriteID::PlButtonPressed,              SkinBitmap::ShufRep, 69, 61, 23, 12);
    set(SpriteID::PlButtonPressedSelected,      SkinBitmap::ShufRep, 69, 73, 23, 12);

    // ---- eqmain.bmp ----
    set(SpriteID::EqBackground,                 SkinBitmap::EQMain,   0,   0, 275, 116);
    set(SpriteID::EqTitleBar,                   SkinBitmap::EQMain,   0, 149, 275,  14);
    set(SpriteID::EqTitleBarSelected,           SkinBitmap::EQMain,   0, 134, 275,  14);
    set(SpriteID::EqSliderBg,                   SkinBitmap::EQMain,  13, 164, 209, 129);
    set(SpriteID::EqSliderThumb,                SkinBitmap::EQMain,   0, 164,  11,  11);
    set(SpriteID::EqSliderThumbSelected,        SkinBitmap::EQMain,   0, 176,  11,  11);
    set(SpriteID::EqCloseButton,                SkinBitmap::EQMain,   0, 116,   9,   9);
    set(SpriteID::EqCloseButtonActive,          SkinBitmap::EQMain,   0, 125,   9,   9);
    set(SpriteID::EqOnButton,                   SkinBitmap::EQMain,  10, 119,  26,  12);
    set(SpriteID::EqOnButtonPressed,            SkinBitmap::EQMain, 128, 119,  26,  12);
    set(SpriteID::EqOnButtonSelected,           SkinBitmap::EQMain,  69, 119,  26,  12);
    set(SpriteID::EqOnButtonSelectedPressed,    SkinBitmap::EQMain, 187, 119,  26,  12);
    set(SpriteID::EqAutoButton,                 SkinBitmap::EQMain,  36, 119,  32,  12);
    set(SpriteID::EqAutoButtonPressed,          SkinBitmap::EQMain, 154, 119,  32,  12);
    set(SpriteID::EqAutoButtonSelected,         SkinBitmap::EQMain,  95, 119,  32,  12);
    set(SpriteID::EqAutoButtonSelectedPressed,  SkinBitmap::EQMain, 213, 119,  32,  12);
    set(SpriteID::EqGraphBg,                    SkinBitmap::EQMain,   0, 294, 113,  19);
    set(SpriteID::EqGraphLineColors,            SkinBitmap::EQMain, 115, 294,   1,  19);
    set(SpriteID::EqPresetsButton,              SkinBitmap::EQMain, 224, 164,  44,  12);
    set(SpriteID::EqPresetsButtonSelected,      SkinBitmap::EQMain, 224, 176,  44,  12);

    return t;
}();

const std::array<SpriteDef, static_cast<size_t>(SpriteID::Count)>& getSpriteTable()
{
    return kSpriteTable;
}

//==============================================================================
// VisColors default palette (classic Winamp green spectrum)
//==============================================================================
VisColors::VisColors()
{
    // Default Winamp visualization palette (spectrum analyzer)
    // 24 colors from bright peak → dark base
    const uint32_t defaults[kNumColors] = {
        0xFF000000,  //  0 - black (background)
        0xFF181818,  //  1
        0xFF282828,  //  2
        0xFF3A3A3A,  //  3
        0xFF4B6D18,  //  4
        0xFF376E18,  //  5
        0xFF247018,  //  6
        0xFF107218,  //  7
        0xFF007418,  //  8
        0xFF007618,  //  9 - green mid
        0xFF007818,  // 10
        0xFF007A18,  // 11
        0xFF007C18,  // 12
        0xFF007E18,  // 13
        0xFF008018,  // 14
        0xFF008218,  // 15
        0xFF228418,  // 16
        0xFF448618,  // 17
        0xFF668818,  // 18
        0xFF888A18,  // 19 - yellow zone
        0xFFAA8C18,  // 20
        0xFFCC8E18,  // 21
        0xFFEE9018,  // 22
        0xFFFF0000,  // 23 - red peak
    };

    for (int i = 0; i < kNumColors; ++i)
        colors[static_cast<size_t>(i)] = juce::Colour(defaults[i]);
}

//==============================================================================
// BitmapFont character lookup
//==============================================================================
SpriteRect BitmapFont::getCharRect(char c)
{
    int col = -1;
    int row = 0;

    // Row 0: A-Z + special
    if (c >= 'A' && c <= 'Z')      { row = 0; col = c - 'A'; }
    else if (c >= 'a' && c <= 'z') { row = 0; col = c - 'a'; }  // case-insensitive
    else if (c == '"')              { row = 0; col = 26; }
    else if (c == '@')              { row = 0; col = 27; }
    else if (c == ' ')              { row = 0; col = 30; }
    // Row 1: 0-9 + punctuation
    else if (c >= '0' && c <= '9') { row = 1; col = c - '0'; }
    else if (c == '.')              { row = 1; col = 11; }
    else if (c == ':')              { row = 1; col = 12; }
    else if (c == '(')              { row = 1; col = 13; }
    else if (c == ')')              { row = 1; col = 14; }
    else if (c == '-')              { row = 1; col = 15; }
    else if (c == '\'')             { row = 1; col = 16; }
    else if (c == '!')              { row = 1; col = 17; }
    else if (c == '_')              { row = 1; col = 18; }
    else if (c == '+')              { row = 1; col = 19; }
    else if (c == '\\')             { row = 1; col = 20; }
    else if (c == '/')              { row = 1; col = 21; }
    else if (c == '[')              { row = 1; col = 22; }
    else if (c == ']')              { row = 1; col = 23; }
    else if (c == '^')              { row = 1; col = 24; }
    else if (c == '&')              { row = 1; col = 25; }
    else if (c == '%')              { row = 1; col = 26; }
    else if (c == ',')              { row = 1; col = 27; }
    else if (c == '=')              { row = 1; col = 28; }
    else if (c == '$')              { row = 1; col = 29; }
    else if (c == '#')              { row = 1; col = 30; }
    // Row 2: extended
    else if (c == '?')              { row = 2; col = 3; }
    else if (c == '*')              { row = 2; col = 4; }
    else
    {
        // Fallback: space
        row = 0; col = 30;
    }

    return { col * kCharWidth, row * kCharHeight, kCharWidth, kCharHeight };
}

//==============================================================================
// SkinModel::getSprite
//==============================================================================
juce::Image SkinModel::getSprite(SpriteID id) const
{
    const auto& table = getSpriteTable();
    const auto& def = table[static_cast<size_t>(id)];
    auto bmpIdx = static_cast<size_t>(def.bitmap);

    if (!bitmapLoaded[bmpIdx] || !def.rect.isValid())
        return {};

    const auto& img = bitmaps[bmpIdx];
    auto r = def.rect.toRect();

    // Bounds-check against actual image size
    if (r.getRight() > img.getWidth() || r.getBottom() > img.getHeight())
        return {};

    return img.getClippedImage(r);
}

} // namespace Skin
