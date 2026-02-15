#pragma once

#include <JuceHeader.h>
#include "../Skin/SkinModel.h"

//==============================================================================
/// BitmapFontRenderer — draws text using Winamp's text.bmp and numbers.bmp
/// bitmap fonts extracted from a skin.
class BitmapFontRenderer
{
public:
    BitmapFontRenderer() = default;

    /// Set the skin to use for font rendering
    void setSkin(const Skin::SkinModel* skin) { currentSkin = skin; }

    /// Draw a string using text.bmp (5×6 character cells)
    /// Renders at native size or scaled to fit within maxWidth
    void drawText(juce::Graphics& g, const juce::String& text,
                  int x, int y, int scale = 1) const;

    /// Draw a time string using numbers.bmp (9×13 digit cells)
    /// Format: "MM:SS" or "-M:SS" — uses large digit sprites
    void drawTime(juce::Graphics& g, int minutes, int seconds,
                  int x, int y, int scale = 1, bool showMinus = false) const;

    /// Draw a single digit (0-9) using numbers.bmp
    void drawDigit(juce::Graphics& g, int digit, int x, int y, int scale = 1) const;

    /// Draw scrolling text (call repeatedly with advancing offset)
    /// Returns the total pixel width of the rendered text
    int drawScrollingText(juce::Graphics& g, const juce::String& text,
                          juce::Rectangle<int> clipArea,
                          int scrollOffset, int scale = 1) const;

    /// Get the pixel width of a string at given scale
    int getTextWidth(const juce::String& text, int scale = 1) const;

private:
    const Skin::SkinModel* currentSkin = nullptr;

    /// Draw a single character from text.bmp
    void drawChar(juce::Graphics& g, char c, int x, int y, int scale) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BitmapFontRenderer)
};
