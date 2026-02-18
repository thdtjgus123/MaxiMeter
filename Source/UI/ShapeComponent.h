#pragma once

#include <JuceHeader.h>
#include "StackBlur.h"

//==============================================================================
/// Identifies which geometric shape to draw.
enum class ShapeType { Rectangle, Ellipse, Triangle, Line, Star, SVG };

/// Stroke alignment relative to the shape path.
enum class StrokeAlignment { Center = 0, Inside, Outside };

/// Line end-cap style.
enum class LineCap { Butt = 0, Round, Square };

//==============================================================================
/// A Component that renders a basic geometric shape with optional gradient fill,
/// stroke, and rounded corners.  Used for ShapeRectangle / ShapeEllipse /
/// ShapeTriangle / ShapeLine / ShapeStar canvas items.
class ShapeComponent : public juce::Component
{
public:
    explicit ShapeComponent(ShapeType s = ShapeType::Rectangle) : shape(s) { setOpaque(false); }

    //-- Property setters ------------------------------------------------------
    void setShapeType(ShapeType t)          { shape = t; pathDirty_ = true; repaint(); }
    ShapeType getShapeType() const          { return shape; }

    void setFillColour1(juce::Colour c)     { fill1 = c; pathDirty_ = true; repaint(); }
    void setFillColour2(juce::Colour c)     { fill2 = c; pathDirty_ = true; repaint(); }
    juce::Colour getFillColour1() const     { return fill1; }
    juce::Colour getFillColour2() const     { return fill2; }

    /// 0 = solid (fill1 only), 1 = vertical, 2 = horizontal, 3 = diagonal
    void setGradientDirection(int dir)      { gradientDir = dir; pathDirty_ = true; repaint(); }
    int  getGradientDirection() const       { return gradientDir; }

    void setCornerRadius(float r)           { cornerRadius = r; pathDirty_ = true; repaintWithParent(); }
    float getCornerRadius() const           { return cornerRadius; }

    void setStrokeColour(juce::Colour c)    { strokeCol = c; repaintWithParent(); }
    juce::Colour getStrokeColour() const    { return strokeCol; }

    void setStrokeWidth(float w)            { strokeW = w; repaintWithParent(); }
    float getStrokeWidth() const            { return strokeW; }

    void setStrokeAlignment(StrokeAlignment a) { strokeAlign = a; repaintWithParent(); }
    StrokeAlignment getStrokeAlignment() const { return strokeAlign; }

    void setLineCap(LineCap c)              { lineCap = c; repaintWithParent(); }
    LineCap getLineCap() const              { return lineCap; }

    void setStarPoints(int n)               { starPoints = juce::jlimit(3, 20, n); pathDirty_ = true; repaintWithParent(); }
    int  getStarPoints() const              { return starPoints; }

    void setTriangleRoundness(float r)      { triangleRoundness = juce::jlimit(0.0f, 1.0f, r); pathDirty_ = true; repaintWithParent(); }
    float getTriangleRoundness() const      { return triangleRoundness; }

    /// Set SVG path data string (the 'd' attribute from <path>).
    /// Multiple paths can be concatenated.
    void setSvgPathData(const juce::String& data)
    {
        svgPathData_ = data;
        svgDrawable_.reset();
        svgParsedPath_ = juce::Path();
        if (data.isNotEmpty())
        {
            // Try parsing as an SVG document first
            if (auto xml = juce::XmlDocument::parse(data))
            {
                svgDrawable_ = juce::Drawable::createFromSVG(*xml);
            }
            // If that didn't work, try just as path data
            if (!svgDrawable_)
            {
                juce::Path p;
                p.restoreFromString(data);
                if (!p.isEmpty())
                    svgParsedPath_ = p;
            }
        }
        pathDirty_ = true;
        repaint();
    }
    const juce::String& getSvgPathData() const { return svgPathData_; }

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
    void resized() override { pathDirty_ = true; }

    void paint(juce::Graphics& g) override
    {
        auto localBounds = getLocalBounds().toFloat();
        if (localBounds.getWidth() <= 0.0f || localBounds.getHeight() <= 0.0f)
            return;

        // Rebuild cached path only when properties or bounds change
        if (pathDirty_)
        {
            cachedPath_ = buildShapePath(localBounds);
            pathDirty_ = false;
        }

        juce::Path shapePath = cachedPath_;

        // Per-item background
        if (bgColour.getAlpha() > 0)
        {
            g.setColour(bgColour);
            g.fillRect(localBounds);
        }

        // ── Frosted Glass Effect ──
        if (frostedGlass && blurRadius > 0.0f && shape != ShapeType::Line)
        {
            juce::Image backdrop;

            if (externalBackdrop_.isValid())
            {
                // Offline rendering: use externally-provided backdrop
                backdrop = externalBackdrop_;
            }
            else if (auto* parent = getParentComponent())
            {
                // Live rendering: capture parent snapshot
                auto parentBounds = getBoundsInParent();
                setVisible(false);
                backdrop = parent->createComponentSnapshot(parentBounds, false, 1.0f);
                setVisible(true);
            }

            if (backdrop.isValid())
            {
                int intRadius = juce::jlimit(1, 60, static_cast<int>(blurRadius));
                StackBlur::applyBoxBlur(backdrop, intRadius);

                {
                    juce::Graphics::ScopedSaveState saveState(g);
                    g.reduceClipRegion(shapePath);
                    g.drawImageAt(backdrop, 0, 0);

                    g.setColour(frostTint.withAlpha(frostOpacity));
                    g.fillRect(localBounds);
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
                case 1: p1 = localBounds.getTopLeft(); p2 = localBounds.getBottomLeft(); break;
                case 2: p1 = localBounds.getTopLeft(); p2 = localBounds.getTopRight(); break;
                case 3: default: p1 = localBounds.getTopLeft(); p2 = localBounds.getBottomRight(); break;
            }
            gradient = juce::ColourGradient(fill1, p1, fill2, p2, false);
        }

        // Fill
        if (shape == ShapeType::SVG && svgDrawable_)
        {
            // Render the SVG drawable directly (preserves all SVG elements)
            auto svgBounds = svgDrawable_->getDrawableBounds();
            if (!svgBounds.isEmpty())
            {
                float scaleX = localBounds.getWidth() / svgBounds.getWidth();
                float scaleY = localBounds.getHeight() / svgBounds.getHeight();
                float scale = juce::jmin(scaleX, scaleY);
                float dx = localBounds.getX() + (localBounds.getWidth() - svgBounds.getWidth() * scale) * 0.5f;
                float dy = localBounds.getY() + (localBounds.getHeight() - svgBounds.getHeight() * scale) * 0.5f;

                auto transform = juce::AffineTransform::translation(-svgBounds.getX(), -svgBounds.getY())
                                     .scaled(scale, scale)
                                     .translated(dx, dy);
                svgDrawable_->draw(g, 1.0f, transform);
            }
        }
        else if (shape != ShapeType::Line)
        {
            if (frostedGlass && blurRadius > 0.0f)
            {
                if (useGradient)
                {
                    auto g1 = fill1.withMultipliedAlpha(0.3f);
                    auto g2 = fill2.withMultipliedAlpha(0.3f);
                    juce::Point<float> p1, p2;
                    switch (gradientDir)
                    {
                        case 1: p1 = localBounds.getTopLeft(); p2 = localBounds.getBottomLeft(); break;
                        case 2: p1 = localBounds.getTopLeft(); p2 = localBounds.getTopRight(); break;
                        case 3: default: p1 = localBounds.getTopLeft(); p2 = localBounds.getBottomRight(); break;
                    }
                    g.setGradientFill(juce::ColourGradient(g1, p1, g2, p2, false));
                }
                else
                {
                    g.setColour(fill1.withMultipliedAlpha(0.3f));
                }
                g.fillPath(shapePath);
            }
            else
            {
                if (useGradient)
                    g.setGradientFill(gradient);
                else
                    g.setColour(fill1);
                g.fillPath(shapePath);
            }
        }

        // ── Stroke (Inside and Center alignments drawn here to respect z-order) ──
        // Outside strokes are rendered by CanvasView::paintOverChildren() because
        // they extend beyond the component boundary and need clipping there.
        if (strokeW > 0.0f && (strokeAlign == StrokeAlignment::Inside
                             || strokeAlign == StrokeAlignment::Center))
        {
            juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::butt;
            switch (lineCap)
            {
                case LineCap::Butt:   cap = juce::PathStrokeType::butt;    break;
                case LineCap::Round:  cap = juce::PathStrokeType::rounded; break;
                case LineCap::Square: cap = juce::PathStrokeType::square;  break;
            }

            g.setColour(strokeCol);

            if (strokeAlign == StrokeAlignment::Inside)
            {
                // Clip to inside the shape, draw 2x so inner half = strokeW
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(shapePath);
                g.strokePath(shapePath, juce::PathStrokeType(strokeW * 2.0f,
                                                              juce::PathStrokeType::mitered, cap));
            }
            else // Center — normal stroke, half inside / half outside
            {
                g.strokePath(shapePath, juce::PathStrokeType(strokeW,
                                                             juce::PathStrokeType::mitered, cap));
            }
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
    StrokeAlignment strokeAlign = StrokeAlignment::Center;
    LineCap      lineCap      = LineCap::Butt;
    int          starPoints   = 5;
    float        triangleRoundness = 0.0f;
    juce::Colour bgColour     { 0x00000000 };
    juce::String svgPathData_;                         ///< raw SVG/path data
    std::unique_ptr<juce::Drawable> svgDrawable_;      ///< parsed SVG drawable
    juce::Path   svgParsedPath_;                       ///< fallback parsed path

    // Frosted glass
    bool         frostedGlass = false;
    float        blurRadius   = 10.0f;
    juce::Colour frostTint    { 0xFFFFFFFF };
    float        frostOpacity = 0.15f;

    // External backdrop for offline / export rendering (no parent available)
    juce::Image  externalBackdrop_;

    // Path caching
    mutable juce::Path cachedPath_;
    mutable bool       pathDirty_ = true;

    /// Repaint this component AND the parent (CanvasView) so that the
    /// stroke overlay rendered in paintOverChildren() is also refreshed.
    void repaintWithParent()
    {
        repaint();
        if (auto* p = getParentComponent())
            p->repaint();
    }

public:
    /// Set an external backdrop image for offline frosted-glass rendering.
    /// When set, paint() uses this instead of capturing a parent snapshot.
    void setExternalBackdrop(const juce::Image& img) { externalBackdrop_ = img; }

    /// Clear the external backdrop after rendering.
    void clearExternalBackdrop() { externalBackdrop_ = juce::Image(); }

    /// Return the cached path (in local-component coordinates).
    /// Guaranteed up-to-date after a paint() call.
    const juce::Path& getCachedPath() const
    {
        if (pathDirty_)
        {
            cachedPath_ = buildShapePath(getLocalBounds().toFloat());
            pathDirty_ = false;
        }
        return cachedPath_;
    }

    /// Build the shape path for the given bounds.  Public so the overlay
    /// renderer in CanvasView can construct stroke paths in screen space.
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
            {
                float cx = bounds.getCentreX();
                juce::Point<float> p0(cx, bounds.getY());
                juce::Point<float> p1(bounds.getRight(), bounds.getBottom());
                juce::Point<float> p2(bounds.getX(), bounds.getBottom());
                if (triangleRoundness > 0.0f)
                {
                    // Maximum rounding radius limited by shortest side * 0.5
                    float side01 = p0.getDistanceFrom(p1);
                    float side12 = p1.getDistanceFrom(p2);
                    float side20 = p2.getDistanceFrom(p0);
                    float minSide = juce::jmin(side01, side12, side20);
                    float maxR = minSide * 0.45f;
                    float r = triangleRoundness * maxR;
                    // Helper: offset a corner vertex along adjacent edges by r
                    auto offsetPt = [](juce::Point<float> corner, juce::Point<float> towards, float dist) {
                        auto dir = towards - corner;
                        float len = dir.getDistanceFromOrigin();
                        if (len < 0.001f) return corner;
                        return corner + dir * (dist / len);
                    };
                    auto a0 = offsetPt(p0, p2, r); auto a1 = offsetPt(p0, p1, r);
                    auto b0 = offsetPt(p1, p0, r); auto b1 = offsetPt(p1, p2, r);
                    auto c0 = offsetPt(p2, p1, r); auto c1 = offsetPt(p2, p0, r);
                    path.startNewSubPath(a1);
                    path.lineTo(b0);
                    path.quadraticTo(p1, b1);
                    path.lineTo(c0);
                    path.quadraticTo(p2, c1);
                    path.lineTo(a0);
                    path.quadraticTo(p0, a1);
                    path.closeSubPath();
                }
                else
                {
                    path.startNewSubPath(p0);
                    path.lineTo(p1);
                    path.lineTo(p2);
                    path.closeSubPath();
                }
                break;
            }
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
                int pts = starPoints;
                path.startNewSubPath(cx, cy - outerR);
                for (int i = 1; i < pts * 2; ++i)
                {
                    float angle = static_cast<float>(i) * juce::MathConstants<float>::pi / static_cast<float>(pts)
                                  - juce::MathConstants<float>::halfPi;
                    float r = (i % 2 == 0) ? outerR : innerR;
                    path.lineTo(cx + r * std::cos(angle), cy + r * std::sin(angle));
                }
                path.closeSubPath();
                break;
            }
            case ShapeType::SVG:
            {
                // Use the drawable or parsed path, scaled to fit bounds
                if (svgDrawable_)
                {
                    auto svgBounds = svgDrawable_->getDrawableBounds();
                    if (!svgBounds.isEmpty())
                    {
                        // Extract path from drawable by drawing to a path
                        // We'll use the drawable's outline path
                        path.addRectangle(bounds); // fallback bounding
                    }
                }
                else if (!svgParsedPath_.isEmpty())
                {
                    path = svgParsedPath_;
                    auto pathBounds = path.getBounds();
                    if (!pathBounds.isEmpty())
                    {
                        // Scale and translate path to fit within bounds
                        float scaleX = bounds.getWidth() / pathBounds.getWidth();
                        float scaleY = bounds.getHeight() / pathBounds.getHeight();
                        float scale = juce::jmin(scaleX, scaleY);
                        path.applyTransform(
                            juce::AffineTransform::translation(-pathBounds.getX(), -pathBounds.getY())
                                .scaled(scale, scale)
                                .translated(bounds.getX() + (bounds.getWidth() - pathBounds.getWidth() * scale) * 0.5f,
                                            bounds.getY() + (bounds.getHeight() - pathBounds.getHeight() * scale) * 0.5f));
                    }
                }
                else
                {
                    // No SVG loaded – draw placeholder rectangle
                    path.addRectangle(bounds);
                }
                break;
            }
        }
        return path;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShapeComponent)
};
