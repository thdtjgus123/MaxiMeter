#pragma once

#include <JuceHeader.h>

//==============================================================================
/// A Component that renders editable text on the canvas with customisable
/// font family, size, style, colour, alignment, and optional background.
class TextLabelComponent : public juce::Component
{
public:
    TextLabelComponent() { setOpaque(false); }

    //-- Property setters ------------------------------------------------------
    void setTextContent(const juce::String& t)  { text = t; repaint(); }
    juce::String getTextContent() const         { return text; }

    void setFontFamily(const juce::String& f)   { fontFamily = f; repaint(); }
    juce::String getFontFamily() const          { return fontFamily; }

    void setFontSize(float s)                   { fontSize = s; repaint(); }
    float getFontSize() const                   { return fontSize; }

    void setBold(bool b)                        { bold = b; repaint(); }
    bool isBold() const                         { return bold; }

    void setItalic(bool i)                      { italic = i; repaint(); }
    bool isItalic() const                       { return italic; }

    void setTextColour(juce::Colour c)          { textCol = c; repaint(); }
    juce::Colour getTextColour() const          { return textCol; }

    /// 0 = left, 1 = centre, 2 = right
    void setTextAlignment(int a)                { alignment = a; repaint(); }
    int  getTextAlignment() const               { return alignment; }

    void setItemBackground(juce::Colour c)      { bgColour = c; repaint(); }
    juce::Colour getItemBackground() const      { return bgColour; }

    void setFillColour1(juce::Colour c)         { fill1 = c; repaint(); }
    juce::Colour getFillColour1() const         { return fill1; }

    void setFillColour2(juce::Colour c)         { fill2 = c; repaint(); }
    juce::Colour getFillColour2() const         { return fill2; }

    void setGradientDirection(int d)            { gradientDir = d; repaint(); }
    int  getGradientDirection() const           { return gradientDir; }

    void setCornerRadius(float r)               { cornerRadius = r; repaint(); }
    float getCornerRadius() const               { return cornerRadius; }

    void setStrokeColour(juce::Colour c)        { strokeCol = c; repaint(); }
    juce::Colour getStrokeColour() const        { return strokeCol; }

    void setStrokeWidth(float w)                { strokeW = w; repaint(); }
    float getStrokeWidth() const                { return strokeW; }

    //--------------------------------------------------------------------------
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        if (bgColour.getAlpha() > 0)
        {
            g.setColour(bgColour);
            if (cornerRadius > 0.0f)
                g.fillRoundedRectangle(bounds, cornerRadius);
            else
                g.fillRect(bounds);
        }

        // Box fill (gradient or solid)
        if (fill1.getAlpha() > 0 || fill2.getAlpha() > 0)
        {
            auto fillBounds = bounds.reduced(strokeW * 0.5f);
            if (gradientDir > 0)
            {
                juce::Point<float> p1, p2;
                switch (gradientDir)
                {
                    case 1: p1 = fillBounds.getTopLeft(); p2 = fillBounds.getBottomLeft(); break;
                    case 2: p1 = fillBounds.getTopLeft(); p2 = fillBounds.getTopRight(); break;
                    default: p1 = fillBounds.getTopLeft(); p2 = fillBounds.getBottomRight(); break;
                }
                g.setGradientFill(juce::ColourGradient(fill1, p1, fill2, p2, false));
            }
            else
            {
                g.setColour(fill1);
            }

            if (cornerRadius > 0.0f)
                g.fillRoundedRectangle(fillBounds, cornerRadius);
            else
                g.fillRect(fillBounds);
        }

        // Stroke
        if (strokeW > 0.0f && strokeCol.getAlpha() > 0)
        {
            g.setColour(strokeCol);
            auto strokeBounds = bounds.reduced(strokeW * 0.5f);
            if (cornerRadius > 0.0f)
                g.drawRoundedRectangle(strokeBounds, cornerRadius, strokeW);
            else
                g.drawRect(strokeBounds, strokeW);
        }

        // Text
        int style = juce::Font::plain;
        if (bold) style |= juce::Font::bold;
        if (italic) style |= juce::Font::italic;

        juce::Font font(fontFamily, fontSize, style);
        g.setFont(font);
        g.setColour(textCol);

        juce::Justification just = juce::Justification::centredLeft;
        if (alignment == 1) just = juce::Justification::centred;
        else if (alignment == 2) just = juce::Justification::centredRight;

        auto textBounds = bounds.reduced(6.0f, 2.0f);
        g.drawFittedText(text, textBounds.toNearestInt(), just, 100);
    }

private:
    juce::String text        { "Text" };
    juce::String fontFamily  { "Arial" };
    float        fontSize    = 24.0f;
    bool         bold        = false;
    bool         italic      = false;
    juce::Colour textCol     { 0xFFFFFFFF };
    int          alignment   = 0;
    juce::Colour bgColour    { 0x00000000 };
    juce::Colour fill1       { 0x00000000 };
    juce::Colour fill2       { 0x00000000 };
    int          gradientDir = 0;
    float        cornerRadius = 0.0f;
    juce::Colour strokeCol   { 0x00000000 };
    float        strokeW     = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextLabelComponent)
};
