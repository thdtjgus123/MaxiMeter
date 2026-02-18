#pragma once

#include <JuceHeader.h>
#include "../Canvas/CanvasItem.h"

//==============================================================================
/// Windows Fluent Design icon paths drawn as juce::Path objects.
/// All icons are designed for a 16x16 unit space (scalable).
namespace FluentIcons
{
    //-- Transport Icons -------------------------------------------------------
    inline juce::Path playIcon()
    {
        juce::Path p;
        p.addTriangle(3.0f, 1.0f, 3.0f, 15.0f, 14.0f, 8.0f);
        return p;
    }

    inline juce::Path pauseIcon()
    {
        juce::Path p;
        p.addRoundedRectangle(3.0f, 2.0f, 3.5f, 12.0f, 0.8f);
        p.addRoundedRectangle(9.5f, 2.0f, 3.5f, 12.0f, 0.8f);
        return p;
    }

    inline juce::Path stopIcon()
    {
        juce::Path p;
        p.addRoundedRectangle(3.0f, 3.0f, 10.0f, 10.0f, 1.2f);
        return p;
    }

    inline juce::Path openFolderIcon()
    {
        juce::Path p;
        // Folder base
        p.startNewSubPath(1.0f, 5.0f);
        p.lineTo(1.0f, 13.0f);
        p.lineTo(13.0f, 13.0f);
        p.lineTo(15.0f, 7.0f);
        p.lineTo(6.0f, 7.0f);
        p.lineTo(5.0f, 5.0f);
        p.closeSubPath();
        // Folder tab
        p.startNewSubPath(1.0f, 5.0f);
        p.lineTo(1.0f, 3.0f);
        p.lineTo(5.0f, 3.0f);
        p.lineTo(6.5f, 5.0f);
        p.closeSubPath();
        return p;
    }

    //-- File / Project Icons --------------------------------------------------
    inline juce::Path saveIcon()
    {
        juce::Path p;
        // Disk body
        p.addRoundedRectangle(2.0f, 2.0f, 12.0f, 12.0f, 1.0f);
        // Slot
        p.addRectangle(5.0f, 2.0f, 6.0f, 4.0f);
        // Label area
        p.addRectangle(4.0f, 9.0f, 8.0f, 5.0f);
        return p;
    }

    inline juce::Path newFileIcon()
    {
        juce::Path p;
        p.startNewSubPath(3.0f, 1.0f);
        p.lineTo(10.0f, 1.0f);
        p.lineTo(13.0f, 4.0f);
        p.lineTo(13.0f, 15.0f);
        p.lineTo(3.0f, 15.0f);
        p.closeSubPath();
        // Plus
        p.startNewSubPath(6.5f, 8.0f);
        p.lineTo(9.5f, 8.0f);
        p.startNewSubPath(8.0f, 6.5f);
        p.lineTo(8.0f, 9.5f);
        return p;
    }

    //-- Alignment Icons -------------------------------------------------------
    inline juce::Path alignLeftIcon()
    {
        juce::Path p;
        p.addRectangle(1.0f, 1.0f, 1.5f, 14.0f);
        p.addRectangle(4.0f, 3.0f, 10.0f, 3.0f);
        p.addRectangle(4.0f, 9.0f, 6.0f, 3.0f);
        return p;
    }

    inline juce::Path alignCenterHIcon()
    {
        juce::Path p;
        p.addRectangle(7.25f, 1.0f, 1.5f, 14.0f);
        p.addRectangle(3.0f, 3.0f, 10.0f, 3.0f);
        p.addRectangle(5.0f, 9.0f, 6.0f, 3.0f);
        return p;
    }

    inline juce::Path alignRightIcon()
    {
        juce::Path p;
        p.addRectangle(13.5f, 1.0f, 1.5f, 14.0f);
        p.addRectangle(2.0f, 3.0f, 10.0f, 3.0f);
        p.addRectangle(6.0f, 9.0f, 6.0f, 3.0f);
        return p;
    }

    inline juce::Path alignTopIcon()
    {
        juce::Path p;
        p.addRectangle(1.0f, 1.0f, 14.0f, 1.5f);
        p.addRectangle(3.0f, 4.0f, 3.0f, 10.0f);
        p.addRectangle(9.0f, 4.0f, 3.0f, 6.0f);
        return p;
    }

    inline juce::Path alignCenterVIcon()
    {
        juce::Path p;
        p.addRectangle(1.0f, 7.25f, 14.0f, 1.5f);
        p.addRectangle(3.0f, 3.0f, 3.0f, 10.0f);
        p.addRectangle(9.0f, 5.0f, 3.0f, 6.0f);
        return p;
    }

    inline juce::Path alignBottomIcon()
    {
        juce::Path p;
        p.addRectangle(1.0f, 13.5f, 14.0f, 1.5f);
        p.addRectangle(3.0f, 2.0f, 3.0f, 10.0f);
        p.addRectangle(9.0f, 6.0f, 3.0f, 6.0f);
        return p;
    }

    inline juce::Path distributeHIcon()
    {
        juce::Path p;
        p.addRectangle(1.0f, 1.0f, 1.5f, 14.0f);
        p.addRectangle(13.5f, 1.0f, 1.5f, 14.0f);
        p.addRectangle(5.0f, 4.0f, 2.5f, 8.0f);
        p.addRectangle(9.0f, 4.0f, 2.5f, 8.0f);
        return p;
    }

    inline juce::Path distributeVIcon()
    {
        juce::Path p;
        p.addRectangle(1.0f, 1.0f, 14.0f, 1.5f);
        p.addRectangle(1.0f, 13.5f, 14.0f, 1.5f);
        p.addRectangle(4.0f, 5.0f, 8.0f, 2.5f);
        p.addRectangle(4.0f, 9.0f, 8.0f, 2.5f);
        return p;
    }

    //-- Grid / View Icons -----------------------------------------------------
    inline juce::Path gridIcon()
    {
        juce::Path p;
        // Vertical lines
        p.addRectangle(5.0f, 1.0f, 1.0f, 14.0f);
        p.addRectangle(10.0f, 1.0f, 1.0f, 14.0f);
        // Horizontal lines
        p.addRectangle(1.0f, 5.0f, 14.0f, 1.0f);
        p.addRectangle(1.0f, 10.0f, 14.0f, 1.0f);
        return p;
    }

    inline juce::Path zoomInIcon()
    {
        juce::Path p;
        // Circle (magnifier)
        p.addEllipse(1.0f, 1.0f, 10.0f, 10.0f);
        // Plus
        p.addRectangle(4.5f, 5.0f, 4.0f, 1.0f);
        p.addRectangle(5.5f, 4.0f, 1.0f, 4.0f);
        // Handle
        p.addRectangle(10.0f, 10.0f, 5.0f, 1.5f);
        p.applyTransform(juce::AffineTransform::rotation(
            0.785f, 10.0f, 10.0f)); // Rotate handle 45 degrees
        return p;
    }

    //-- Layer / Visibility Icons ----------------------------------------------
    inline juce::Path eyeIcon()
    {
        juce::Path p;
        // Eye shape
        p.startNewSubPath(1.0f, 8.0f);
        p.cubicTo(4.0f, 3.0f, 12.0f, 3.0f, 15.0f, 8.0f);
        p.cubicTo(12.0f, 13.0f, 4.0f, 13.0f, 1.0f, 8.0f);
        p.closeSubPath();
        // Pupil
        p.addEllipse(6.0f, 6.0f, 4.0f, 4.0f);
        return p;
    }

    inline juce::Path eyeOffIcon()
    {
        juce::Path p;
        // Eye shape
        p.startNewSubPath(1.0f, 8.0f);
        p.cubicTo(4.0f, 3.0f, 12.0f, 3.0f, 15.0f, 8.0f);
        p.cubicTo(12.0f, 13.0f, 4.0f, 13.0f, 1.0f, 8.0f);
        p.closeSubPath();
        // Strike-through line
        p.startNewSubPath(2.0f, 14.0f);
        p.lineTo(14.0f, 2.0f);
        return p;
    }

    inline juce::Path lockIcon()
    {
        juce::Path p;
        // Lock body
        p.addRoundedRectangle(3.0f, 7.0f, 10.0f, 8.0f, 1.0f);
        // Shackle
        p.startNewSubPath(5.0f, 7.0f);
        p.lineTo(5.0f, 4.5f);
        p.cubicTo(5.0f, 2.0f, 11.0f, 2.0f, 11.0f, 4.5f);
        p.lineTo(11.0f, 7.0f);
        return p;
    }

    inline juce::Path unlockIcon()
    {
        juce::Path p;
        // Lock body
        p.addRoundedRectangle(3.0f, 7.0f, 10.0f, 8.0f, 1.0f);
        // Open shackle
        p.startNewSubPath(5.0f, 7.0f);
        p.lineTo(5.0f, 4.5f);
        p.cubicTo(5.0f, 2.0f, 11.0f, 2.0f, 11.0f, 4.5f);
        return p;
    }

    //-- Arrow Icons -----------------------------------------------------------
    inline juce::Path arrowUpIcon()
    {
        juce::Path p;
        p.addTriangle(8.0f, 2.0f, 3.0f, 10.0f, 13.0f, 10.0f);
        p.addRectangle(6.5f, 10.0f, 3.0f, 4.0f);
        return p;
    }

    inline juce::Path arrowDownIcon()
    {
        juce::Path p;
        p.addTriangle(8.0f, 14.0f, 3.0f, 6.0f, 13.0f, 6.0f);
        p.addRectangle(6.5f, 2.0f, 3.0f, 4.0f);
        return p;
    }

    //-- Meter Type Icons (conceptual/abstract shapes) -------------------------
    inline juce::Path spectrumIcon()
    {
        juce::Path p;
        // Bars
        p.addRectangle(2.0f, 8.0f, 2.0f, 6.0f);
        p.addRectangle(5.0f, 5.0f, 2.0f, 9.0f);
        p.addRectangle(8.0f, 2.0f, 2.0f, 12.0f);
        p.addRectangle(11.0f, 6.0f, 2.0f, 8.0f);
        return p;
    }

    inline juce::Path waveformIcon()
    {
        juce::Path p;
        p.startNewSubPath(1.0f, 8.0f);
        p.cubicTo(3.0f, 2.0f, 5.0f, 14.0f, 8.0f, 8.0f);
        p.cubicTo(10.0f, 4.0f, 12.0f, 12.0f, 15.0f, 8.0f);
        return p;
    }

    inline juce::Path circleIcon()
    {
        juce::Path p;
        p.addEllipse(2.0f, 2.0f, 12.0f, 12.0f);
        return p;
    }

    inline juce::Path meterIcon()
    {
        juce::Path p;
        // VU meter arc
        p.addArc(2.0f, 2.0f, 12.0f, 12.0f, -2.4f, -0.74f, true);
        // Needle
        p.startNewSubPath(8.0f, 12.0f);
        p.lineTo(5.0f, 4.0f);
        return p;
    }

    inline juce::Path spectrogramIcon()
    {
        juce::Path p;
        // Horizontal bands with gaps
        p.addRectangle(2.0f, 2.0f, 12.0f, 2.0f);
        p.addRectangle(2.0f, 5.5f, 12.0f, 2.0f);
        p.addRectangle(2.0f, 9.0f, 12.0f, 2.0f);
        p.addRectangle(2.0f, 12.5f, 12.0f, 2.0f);
        return p;
    }

    inline juce::Path correlationIcon()
    {
        juce::Path p;
        // Horizontal bar with center dot
        p.addRectangle(1.0f, 7.0f, 14.0f, 2.0f);
        p.addEllipse(6.5f, 5.5f, 3.0f, 5.0f);
        return p;
    }

    inline juce::Path loudnessIcon()
    {
        juce::Path p;
        // LUFS meter representation
        p.addRectangle(3.0f, 2.0f, 3.0f, 12.0f);
        p.addRectangle(8.0f, 5.0f, 3.0f, 9.0f);
        // Target line
        p.addRectangle(1.0f, 6.0f, 14.0f, 0.5f);
        return p;
    }

    inline juce::Path histogramIcon()
    {
        juce::Path p;
        p.addRectangle(2.0f, 10.0f, 2.0f, 4.0f);
        p.addRectangle(5.0f, 6.0f, 2.0f, 8.0f);
        p.addRectangle(8.0f, 3.0f, 2.0f, 11.0f);
        p.addRectangle(11.0f, 7.0f, 2.0f, 7.0f);
        return p;
    }

    inline juce::Path lissajousIcon()
    {
        juce::Path p;
        // Figure-8 / infinity shape
        p.addEllipse(1.0f, 4.0f, 7.0f, 8.0f);
        p.addEllipse(8.0f, 4.0f, 7.0f, 8.0f);
        return p;
    }

    /// Image icon — a rectangle with a mountain/sun scene.
    inline juce::Path imageIcon()
    {
        juce::Path p;
        p.addRoundedRectangle(1.0f, 1.0f, 14.0f, 14.0f, 1.5f);
        // Sun
        p.addEllipse(3.5f, 3.5f, 3.0f, 3.0f);
        // Mountain
        p.startNewSubPath(1.0f, 13.0f);
        p.lineTo(5.5f, 7.0f);
        p.lineTo(8.0f, 10.0f);
        p.lineTo(10.0f, 8.0f);
        p.lineTo(15.0f, 13.0f);
        p.closeSubPath();
        return p;
    }

    /// Video/film icon — a clapperboard / play rectangle.
    inline juce::Path videoIcon()
    {
        juce::Path p;
        p.addRoundedRectangle(1.0f, 3.0f, 14.0f, 11.0f, 1.5f);
        // Play triangle
        p.startNewSubPath(6.0f, 6.0f);
        p.lineTo(12.0f, 8.5f);
        p.lineTo(6.0f, 11.0f);
        p.closeSubPath();
        return p;
    }

    //-- Shape icons -----------------------------------------------------------

    /// Rectangle shape icon
    inline juce::Path shapeRectIcon()
    {
        juce::Path p;
        p.addRoundedRectangle(2.0f, 3.0f, 12.0f, 10.0f, 1.5f);
        return p;
    }

    /// Ellipse shape icon
    inline juce::Path shapeEllipseIcon()
    {
        juce::Path p;
        p.addEllipse(2.0f, 3.0f, 12.0f, 10.0f);
        return p;
    }

    /// Triangle shape icon
    inline juce::Path shapeTriangleIcon()
    {
        juce::Path p;
        p.startNewSubPath(8.0f, 2.0f);
        p.lineTo(15.0f, 14.0f);
        p.lineTo(1.0f, 14.0f);
        p.closeSubPath();
        return p;
    }

    /// Line shape icon
    inline juce::Path shapeLineIcon()
    {
        juce::Path p;
        p.startNewSubPath(2.0f, 13.0f);
        p.lineTo(14.0f, 3.0f);
        return p;
    }

    /// Star shape icon
    inline juce::Path shapeStarIcon()
    {
        juce::Path p;
        float cx = 8.0f, cy = 8.0f, outerR = 6.0f, innerR = 2.5f;
        int pts = 5;
        p.startNewSubPath(cx, cy - outerR);
        for (int i = 1; i < pts * 2; ++i)
        {
            float angle = static_cast<float>(i) * juce::MathConstants<float>::pi / static_cast<float>(pts)
                          - juce::MathConstants<float>::halfPi;
            float r = (i % 2 == 0) ? outerR : innerR;
            p.lineTo(cx + r * std::cos(angle), cy + r * std::sin(angle));
        }
        p.closeSubPath();
        return p;
    }

    /// Text / font icon — a stylised "A"
    inline juce::Path textIcon()
    {
        juce::Path p;
        // Letter A shape
        p.startNewSubPath(8.0f, 2.0f);
        p.lineTo(3.0f, 14.0f);
        p.startNewSubPath(8.0f, 2.0f);
        p.lineTo(13.0f, 14.0f);
        // crossbar
        p.startNewSubPath(5.0f, 9.5f);
        p.lineTo(11.0f, 9.5f);
        return p;
    }

    //==========================================================================
    /// Draw an icon path scaled to fit a given rectangle.
    inline void drawIcon(juce::Graphics& g, const juce::Path& icon,
                         juce::Rectangle<float> area, juce::Colour colour,
                         float strokeThickness = 0.0f)
    {
        auto bounds = icon.getBounds();
        if (bounds.isEmpty()) return;

        float scaleX = area.getWidth() / bounds.getWidth();
        float scaleY = area.getHeight() / bounds.getHeight();
        float scale = std::min(scaleX, scaleY);

        juce::Path scaled(icon);
        scaled.applyTransform(juce::AffineTransform::scale(scale, scale)
            .translated(area.getCentreX() - bounds.getCentreX() * scale,
                        area.getCentreY() - bounds.getCentreY() * scale));

        g.setColour(colour);
        if (strokeThickness > 0.0f)
            g.strokePath(scaled, juce::PathStrokeType(strokeThickness));
        else
            g.fillPath(scaled);
    }

    /// Get the icon for a given MeterType.
    inline juce::Path iconForMeterType(MeterType type)
    {
        switch (type)
        {
            case MeterType::SkinnedSpectrum:
            case MeterType::MultiBandAnalyzer:    return spectrumIcon();
            case MeterType::SkinnedVUMeter:       return meterIcon();
            case MeterType::SkinnedOscilloscope:  return waveformIcon();
            case MeterType::Spectrogram:          return spectrogramIcon();
            case MeterType::Goniometer:           return circleIcon();
            case MeterType::CorrelationMeter:     return correlationIcon();
            case MeterType::LoudnessMeter:        return loudnessIcon();
            case MeterType::LevelHistogram:       return histogramIcon();
            case MeterType::LissajousScope:       return lissajousIcon();
            case MeterType::PeakMeter:            return spectrumIcon();
            case MeterType::ImageLayer:           return imageIcon();
            case MeterType::VideoLayer:           return videoIcon();
            case MeterType::SkinnedPlayer:        return meterIcon();
            case MeterType::Equalizer:            return spectrumIcon();
            case MeterType::ShapeRectangle:       return shapeRectIcon();
            case MeterType::ShapeEllipse:         return shapeEllipseIcon();
            case MeterType::ShapeTriangle:        return shapeTriangleIcon();
            case MeterType::ShapeLine:            return shapeLineIcon();
            case MeterType::ShapeStar:            return shapeStarIcon();
            case MeterType::ShapeSVG:             return shapeRectIcon();
            case MeterType::TextLabel:            return textIcon();
            default: return spectrumIcon();
        }
    }
}

