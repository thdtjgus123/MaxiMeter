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

    // Winamp time display layout — digits are spaced 12px apart (9px digit + 3px gap),
    // with an 18px gap between minute and second groups (for the colon).
    // Standard absolute positions: 48, 60, (colon ~69), 78, 90
    // Here we compute relative to x (the minute-tens origin).
    const int s = scale;

    // Optional minus sign — positioned one digit-slot before the first digit
    if (showMinus)
        drawDigit(g, -1, x - 12 * s, y, scale);

    // Minute tens digit
    if (minutes >= 10)
        drawDigit(g, minutes / 10, x, y, scale);
    else
        drawDigit(g, -2, x, y, scale);  // blank

    // Minute ones digit (12px from tens)
    drawDigit(g, minutes % 10, x + 12 * s, y, scale);

    // Colon — two dots between minute-ones and second-tens
    {
        int colonX = x + 21 * s;
        int dotSize = 2 * s;
        g.setColour(juce::Colours::white);
        g.fillRect(colonX, y + 3 * s, dotSize, dotSize);
        g.fillRect(colonX, y + 8 * s, dotSize, dotSize);
    }

    // Second tens digit (30px from origin)
    drawDigit(g, seconds / 10, x + 30 * s, y, scale);

    // Second ones digit (42px from origin)
    drawDigit(g, seconds % 10, x + 42 * s, y, scale);
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
