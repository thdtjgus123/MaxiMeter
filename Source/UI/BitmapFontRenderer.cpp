#include "BitmapFontRenderer.h"

//==============================================================================
void BitmapFontRenderer::drawText(juce::Graphics& g, const juce::String& text,
                                   int x, int y, int scale) const
{
    if (currentSkin == nullptr || !currentSkin->hasBitmap(Skin::SkinBitmap::Text))
        return;

    int cx = x;
    for (int i = 0; i < text.length(); ++i)
    {
        char c = static_cast<char>(text[i]);
        drawChar(g, c, cx, y, scale);
        cx += Skin::BitmapFont::kCharWidth * scale;
    }
}

void BitmapFontRenderer::drawChar(juce::Graphics& g, char c, int x, int y, int scale) const
{
    auto rect = Skin::BitmapFont::getCharRect(c);
    if (!rect.isValid())
        return;

    const auto& textBmp = currentSkin->bitmaps[static_cast<size_t>(Skin::SkinBitmap::Text)];
    if (!textBmp.isValid())
        return;

    auto srcRect = rect.toRect();

    // Bounds-check
    if (srcRect.getRight() > textBmp.getWidth() || srcRect.getBottom() > textBmp.getHeight())
        return;

    auto charImage = textBmp.getClippedImage(srcRect);

    if (scale == 1)
        g.drawImageAt(charImage, x, y);
    else
        g.drawImage(charImage,
                     juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(rect.w * scale),
                                            static_cast<float>(rect.h * scale)));
}

//==============================================================================
void BitmapFontRenderer::drawTime(juce::Graphics& g, int minutes, int seconds,
                                   int x, int y, int scale, bool showMinus) const
{
    if (currentSkin == nullptr || !currentSkin->hasBitmap(Skin::SkinBitmap::Numbers))
        return;

    const int digitW = 9 * scale;
    int cx = x;

    // Optional minus sign
    if (showMinus)
    {
        drawDigit(g, -1, cx, y, scale);  // -1 = minus sign
        cx += digitW;
    }

    // Minutes (1 or 2 digits)
    if (minutes >= 10)
    {
        drawDigit(g, minutes / 10, cx, y, scale);
        cx += digitW;
    }
    else
    {
        drawDigit(g, -2, cx, y, scale);  // -2 = blank
        cx += digitW;
    }
    drawDigit(g, minutes % 10, cx, y, scale);
    cx += digitW;

    // Colon — just draw a small separator
    // (not in standard numbers.bmp; draw manually)
    g.setColour(juce::Colours::white);
    int colonX = cx + 1 * scale;
    int colonY1 = y + 3 * scale;
    int colonY2 = y + 8 * scale;
    int dotSize = 2 * scale;
    g.fillRect(colonX, colonY1, dotSize, dotSize);
    g.fillRect(colonX, colonY2, dotSize, dotSize);
    cx += 5 * scale;

    // Seconds (always 2 digits)
    drawDigit(g, seconds / 10, cx, y, scale);
    cx += digitW;
    drawDigit(g, seconds % 10, cx, y, scale);
}

void BitmapFontRenderer::drawDigit(juce::Graphics& g, int digit, int x, int y, int scale) const
{
    if (currentSkin == nullptr || !currentSkin->hasBitmap(Skin::SkinBitmap::Numbers))
        return;

    Skin::SpriteID spriteId;

    if (digit >= 0 && digit <= 9)
        spriteId = static_cast<Skin::SpriteID>(static_cast<int>(Skin::SpriteID::Digit0) + digit);
    else if (digit == -1)
        spriteId = Skin::SpriteID::DigitMinus;
    else
        spriteId = Skin::SpriteID::DigitBlank;

    auto img = currentSkin->getSprite(spriteId);
    if (!img.isValid())
        return;

    if (scale == 1)
        g.drawImageAt(img, x, y);
    else
        g.drawImage(img,
                     juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(img.getWidth() * scale),
                                            static_cast<float>(img.getHeight() * scale)));
}

//==============================================================================
int BitmapFontRenderer::drawScrollingText(juce::Graphics& g, const juce::String& text,
                                           juce::Rectangle<int> clipArea,
                                           int scrollOffset, int scale) const
{
    int totalWidth = getTextWidth(text, scale);

    g.saveState();
    g.reduceClipRegion(clipArea);

    int drawX = clipArea.getX() - scrollOffset;
    int drawY = clipArea.getY() + (clipArea.getHeight() - Skin::BitmapFont::kCharHeight * scale) / 2;

    // Draw the text, and if scrolling wrap-around, draw it again
    drawText(g, text, drawX, drawY, scale);

    // Add gap and second copy for seamless scrolling
    int gap = 30 * scale;
    if (scrollOffset > 0)
        drawText(g, text, drawX + totalWidth + gap, drawY, scale);

    g.restoreState();

    return totalWidth;
}

int BitmapFontRenderer::getTextWidth(const juce::String& text, int scale) const
{
    return text.length() * Skin::BitmapFont::kCharWidth * scale;
}
