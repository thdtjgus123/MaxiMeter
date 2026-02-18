#pragma once

#include <JuceHeader.h>
#include "../Canvas/CanvasItem.h"

//==============================================================================
/// Font Awesome 6 Free Solid icon glyphs rendered as juce::Path objects.
/// Drop-in replacement for FluentIcons — same API, different icon set.
namespace FontAwesomeIcons
{
    //--------------------------------------------------------------------------
    // Typeface singleton — loaded once from embedded binary data
    //--------------------------------------------------------------------------
    inline juce::Typeface::Ptr getFontAwesomeTypeface()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::fasolid900_ttf, BinaryData::fasolid900_ttfSize);
        return tf;
    }

    //--------------------------------------------------------------------------
    // Helper: create a juce::Path from a Font Awesome Unicode character
    //--------------------------------------------------------------------------
    inline juce::Path glyphToPath(juce::juce_wchar codepoint, float size = 16.0f)
    {
        auto tf = getFontAwesomeTypeface();
        juce::Font font(tf);
        font = font.withHeight(size);

        juce::String text;
        text += codepoint;

        juce::GlyphArrangement ga;
        ga.addLineOfText(font, text, 0.0f, 0.0f);

        juce::Path p;
        ga.createPath(p);

        // Centre the glyph in a 0..size square
        auto bounds = p.getBounds();
        if (!bounds.isEmpty())
        {
            float scale = size / juce::jmax(bounds.getWidth(), bounds.getHeight());
            p.applyTransform(juce::AffineTransform::translation(-bounds.getX(), -bounds.getY())
                                 .scaled(scale)
                                 .translated((size - bounds.getWidth() * scale) * 0.5f,
                                             (size - bounds.getHeight() * scale) * 0.5f));
        }
        return p;
    }

    //--------------------------------------------------------------------------
    // Transport icons
    //--------------------------------------------------------------------------
    inline juce::Path playIcon()        { return glyphToPath(0xf04b); }   // fa-play
    inline juce::Path pauseIcon()       { return glyphToPath(0xf04c); }   // fa-pause
    inline juce::Path stopIcon()        { return glyphToPath(0xf04d); }   // fa-stop
    inline juce::Path openFolderIcon()  { return glyphToPath(0xf07c); }   // fa-folder-open

    //--------------------------------------------------------------------------
    // File / Project icons (currently unused but kept for parity)
    //--------------------------------------------------------------------------
    inline juce::Path saveIcon()        { return glyphToPath(0xf0c7); }   // fa-floppy-disk
    inline juce::Path newFileIcon()     { return glyphToPath(0xf15b); }   // fa-file

    //--------------------------------------------------------------------------
    // Alignment icons
    //--------------------------------------------------------------------------
    inline juce::Path alignLeftIcon()    { return glyphToPath(0xf036); }  // fa-align-left
    inline juce::Path alignCenterHIcon() { return glyphToPath(0xf037); }  // fa-align-center
    inline juce::Path alignRightIcon()   { return glyphToPath(0xf038); }  // fa-align-right
    inline juce::Path alignTopIcon()     { return glyphToPath(0xf062); }  // fa-arrow-up
    inline juce::Path alignCenterVIcon() { return glyphToPath(0xf338); }  // fa-arrows-alt-v
    inline juce::Path alignBottomIcon()  { return glyphToPath(0xf063); }  // fa-arrow-down
    inline juce::Path distributeHIcon()  { return glyphToPath(0xf337); }  // fa-arrows-alt-h
    inline juce::Path distributeVIcon()  { return glyphToPath(0xf338); }  // fa-arrows-alt-v

    //--------------------------------------------------------------------------
    // Grid / View / Zoom
    //--------------------------------------------------------------------------
    inline juce::Path gridIcon()      { return glyphToPath(0xf00a); }     // fa-th / fa-table-cells
    inline juce::Path zoomInIcon()    { return glyphToPath(0xf00e); }     // fa-search-plus

    //--------------------------------------------------------------------------
    // Visibility / Layer
    //--------------------------------------------------------------------------
    inline juce::Path eyeIcon()       { return glyphToPath(0xf06e); }     // fa-eye
    inline juce::Path eyeOffIcon()    { return glyphToPath(0xf070); }     // fa-eye-slash
    inline juce::Path lockIcon()      { return glyphToPath(0xf023); }     // fa-lock
    inline juce::Path unlockIcon()    { return glyphToPath(0xf3c1); }     // fa-lock-open

    //--------------------------------------------------------------------------
    // Arrows
    //--------------------------------------------------------------------------
    inline juce::Path arrowUpIcon()   { return glyphToPath(0xf062); }     // fa-arrow-up
    inline juce::Path arrowDownIcon() { return glyphToPath(0xf063); }     // fa-arrow-down

    //--------------------------------------------------------------------------
    // Meter-type icons
    //--------------------------------------------------------------------------
    inline juce::Path spectrumIcon()     { return glyphToPath(0xf080); }  // fa-chart-bar
    inline juce::Path waveformIcon()     { return glyphToPath(0xf83e); }  // fa-wave-square
    inline juce::Path circleIcon()       { return glyphToPath(0xf111); }  // fa-circle
    inline juce::Path meterIcon()        { return glyphToPath(0xf624); }  // fa-gauge-high
    inline juce::Path spectrogramIcon()  { return glyphToPath(0xf550); }  // fa-grip-lines
    inline juce::Path correlationIcon()  { return glyphToPath(0xf07e); }  // fa-arrows-left-right
    inline juce::Path loudnessIcon()     { return glyphToPath(0xf028); }  // fa-volume-high
    inline juce::Path histogramIcon()    { return glyphToPath(0xf012); }  // fa-signal
    inline juce::Path lissajousIcon()    { return glyphToPath(0xf534); }  // fa-infinity
    inline juce::Path imageIcon()        { return glyphToPath(0xf03e); }  // fa-image
    inline juce::Path videoIcon()        { return glyphToPath(0xf03d); }  // fa-video

    //--------------------------------------------------------------------------
    // Shape icons
    //--------------------------------------------------------------------------
    inline juce::Path shapeRectIcon()    { return glyphToPath(0xf0c8); }  // fa-square
    inline juce::Path shapeEllipseIcon() { return glyphToPath(0xf111); }  // fa-circle
    inline juce::Path shapeTriangleIcon(){ return glyphToPath(0xf0d8); }  // fa-caret-up
    inline juce::Path shapeLineIcon()    { return glyphToPath(0xf068); }  // fa-minus
    inline juce::Path shapeStarIcon()    { return glyphToPath(0xf005); }  // fa-star
    inline juce::Path textIcon()         { return glyphToPath(0xf031); }  // fa-font

    //==========================================================================
    // Draw helper — scales & centres icon path inside area, then fills
    //==========================================================================
    inline void drawIcon(juce::Graphics& g, const juce::Path& icon,
                         juce::Rectangle<float> area, juce::Colour colour,
                         float /*strokeThickness*/ = 0.0f)
    {
        auto pathBounds = icon.getBounds();
        if (pathBounds.isEmpty() || area.isEmpty()) return;

        float scale = juce::jmin(area.getWidth()  / pathBounds.getWidth(),
                                 area.getHeight() / pathBounds.getHeight());

        auto centred = pathBounds * scale;
        float dx = area.getCentreX() - centred.getCentreX();
        float dy = area.getCentreY() - centred.getCentreY();

        juce::Path scaled(icon);
        scaled.applyTransform(juce::AffineTransform::scale(scale).translated(dx, dy));

        g.setColour(colour);
        g.fillPath(scaled);
    }

    //==========================================================================
    // Map MeterType → icon
    //==========================================================================
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
