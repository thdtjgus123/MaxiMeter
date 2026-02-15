#pragma once

#include <JuceHeader.h>
#include "StackBlur.h"

//==============================================================================
/// Identifies which geometric shape to draw.
enum class ShapeType { Rectangle, Ellipse, Triangle, Line, Star };

//==============================================================================
/// A Component that renders a basic geometric shape with optional gradient fill,
/// stroke, and rounded corners.  Used for ShapeRectangle / ShapeEllipse /
/// ShapeTriangle / ShapeLine / ShapeStar canvas items.
class ShapeComponent : public juce::Component
{
public:
    explicit ShapeComponent(ShapeType s = ShapeType::Rectangle) : shape(s) { setOpaque(false); }

    //-- Property setters ------------------------------------------------------
    void setShapeType(ShapeType t)          { shape = t; repaint(); }
    ShapeType getShapeType() const          { return shape; }

    void setFillColour1(juce::Colour c)     { fill1 = c; repaint(); }
    void setFillColour2(juce::Colour c)     { fill2 = c; repaint(); }
    juce::Colour getFillColour1() const     { return fill1; }
    juce::Colour getFillColour2() const     { return fill2; }

    /// 0 = solid (fill1 only), 1 = vertical, 2 = horizontal, 3 = diagonal
    void setGradientDirection(int dir)      { gradientDir = dir; repaint(); }
    int  getGradientDirection() const       { return gradientDir; }

    void setCornerRadius(float r)           { cornerRadius = r; repaint(); }
    float getCornerRadius() const           { return cornerRadius; }

    void setStrokeColour(juce::Colour c)    { strokeCol = c; repaint(); }
    juce::Colour getStrokeColour() const    { return strokeCol; }

    void setStrokeWidth(float w)            { strokeW = w; repaint(); }
    float getStrokeWidth() const            { return strokeW; }

    void setItemBackground(juce::Colour c)  { bgColour = c; repaint(); }
    juce::Colour getItemBackground() const  { return bgColour; }

    // ── Frosted Glass (Background Blur) ──
    void setFrostedGlass(bool enabled)      { frostedGlass = enabled; repaint(); }
    bool getFrostedGlass() const            { return frostedGlass; }

    void setBlurRadius(float r)             { blurRadius = juce::jlimit(0.0f, 60.0f, r); repaint(); }
    float getBlurRadius() const             { return blurRadius; }

    void setFrostTint(juce::Colour c)       { frostTint = c; repaint(); }
    juce::Colour getFrostTint() const       { return frostTint; }

    void setFrostOpacity(float o)           { frostOpacity = juce::jlimit(0.0f, 1.0f, o); repaint(); }
    float getFrostOpacity() const           { return frostOpacity; }

    //--------------------------------------------------------------------------
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(strokeW * 0.5f);

        // Per-item background
        if (bgColour.getAlpha() > 0)
        {
            g.setColour(bgColour);
            g.fillRect(getLocalBounds().toFloat());
        }

        // Build shape path
        juce::Path path = buildShapePath(bounds);

        // ── Frosted Glass Effect ──
        if (frostedGlass && blurRadius > 0.0f && shape != ShapeType::Line)
        {
            if (auto* parent = getParentComponent())
            {
                auto parentBounds = getBoundsInParent();
                // Temporarily hide to capture what's behind us
                setVisible(false);
                auto backdrop = parent->createComponentSnapshot(parentBounds, false, 1.0f);
                setVisible(true);

                // Apply blur
                int intRadius = juce::jlimit(1, 60, static_cast<int>(blurRadius));
                StackBlur::applyBoxBlur(backdrop, intRadius);

                // Draw blurred backdrop clipped to shape
                {
                    juce::Graphics::ScopedSaveState saveState(g);
                    g.reduceClipRegion(path);
                    g.drawImageAt(backdrop, 0, 0);

                    // Frost tint overlay
                    g.setColour(frostTint.withAlpha(frostOpacity));
                    g.fillRect(getLocalBounds().toFloat());
                }
            }
        }

        // Build fill brush
        juce::ColourGradient gradient;
        bool useGradient = (gradientDir > 0);
        if (useGradient)
        {
            juce::Point<float> p1, p2;
            switch (gradientDir)
            {
                case 1: p1 = bounds.getTopLeft(); p2 = bounds.getBottomLeft(); break;
                case 2: p1 = bounds.getTopLeft(); p2 = bounds.getTopRight(); break;
                case 3: default: p1 = bounds.getTopLeft(); p2 = bounds.getBottomRight(); break;
            }
            gradient = juce::ColourGradient(fill1, p1, fill2, p2, false);
        }

        // Fill (skip if frosted glass provides the fill instead)
        if (shape != ShapeType::Line)
        {
            if (frostedGlass && blurRadius > 0.0f)
            {
                // When frosted glass is active, draw fill with reduced alpha
                if (useGradient)
                {
                    auto g1 = fill1.withMultipliedAlpha(0.3f);
                    auto g2 = fill2.withMultipliedAlpha(0.3f);
                    juce::Point<float> p1, p2;
                    switch (gradientDir)
                    {
                        case 1: p1 = bounds.getTopLeft(); p2 = bounds.getBottomLeft(); break;
                        case 2: p1 = bounds.getTopLeft(); p2 = bounds.getTopRight(); break;
                        case 3: default: p1 = bounds.getTopLeft(); p2 = bounds.getBottomRight(); break;
                    }
                    g.setGradientFill(juce::ColourGradient(g1, p1, g2, p2, false));
                }
                else
                {
                    g.setColour(fill1.withMultipliedAlpha(0.3f));
                }
                g.fillPath(path);
            }
            else
            {
                if (useGradient)
                    g.setGradientFill(gradient);
                else
                    g.setColour(fill1);
                g.fillPath(path);
            }
        }

        // Stroke
        if (strokeW > 0.0f)
        {
            g.setColour(strokeCol);
            g.strokePath(path, juce::PathStrokeType(strokeW));
        }
    }

private:
    ShapeType    shape        = ShapeType::Rectangle;
    juce::Colour fill1        { 0xFF3A7BFF };
    juce::Colour fill2        { 0xFF1A4ACA };
    int          gradientDir  = 0;
    float        cornerRadius = 0.0f;
    juce::Colour strokeCol    { 0xFFFFFFFF };
    float        strokeW      = 2.0f;
    juce::Colour bgColour     { 0x00000000 };

    // Frosted glass
    bool         frostedGlass = false;
    float        blurRadius   = 10.0f;
    juce::Colour frostTint    { 0xFFFFFFFF };
    float        frostOpacity = 0.15f;

    /// Build the shape path for the given bounds.
    juce::Path buildShapePath(juce::Rectangle<float> bounds) const
    {
        juce::Path path;
        switch (shape)
        {
            case ShapeType::Rectangle:
                if (cornerRadius > 0.0f)
                    path.addRoundedRectangle(bounds, cornerRadius);
                else
                    path.addRectangle(bounds);
                break;
            case ShapeType::Ellipse:
                path.addEllipse(bounds);
                break;
            case ShapeType::Triangle:
                path.startNewSubPath(bounds.getCentreX(), bounds.getY());
                path.lineTo(bounds.getRight(), bounds.getBottom());
                path.lineTo(bounds.getX(), bounds.getBottom());
                path.closeSubPath();
                break;
            case ShapeType::Line:
                path.startNewSubPath(bounds.getX(), bounds.getCentreY());
                path.lineTo(bounds.getRight(), bounds.getCentreY());
                break;
            case ShapeType::Star:
            {
                float cx = bounds.getCentreX();
                float cy = bounds.getCentreY();
                float outerR = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
                float innerR = outerR * 0.4f;
                int points = 5;
                path.startNewSubPath(cx, cy - outerR);
                for (int i = 1; i < points * 2; ++i)
                {
                    float angle = static_cast<float>(i) * juce::MathConstants<float>::pi / static_cast<float>(points)
                                  - juce::MathConstants<float>::halfPi;
                    float r = (i % 2 == 0) ? outerR : innerR;
                    path.lineTo(cx + r * std::cos(angle), cy + r * std::sin(angle));
                }
                path.closeSubPath();
                break;
            }
        }
        return path;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShapeComponent)
};
