#pragma once

#include <JuceHeader.h>
#include "ThemeManager.h"

//==============================================================================
/// A skeuomorphic LookAndFeel with realistic 3D depth: raised buttons,
/// sunken text fields, glossy slider knobs, bevelled combo boxes.
class SkeuomorphicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkeuomorphicLookAndFeel()
    {
        setDefaultSansSerifTypefaceName("Segoe UI");
        updateFromTheme();
    }

    /// Re-read colours from ThemeManager and set JUCE colour IDs accordingly.
    void updateFromTheme()
    {
        auto& pal = ThemeManager::getInstance().getPalette();

        setColour(juce::ResizableWindow::backgroundColourId, pal.windowBg);
        setColour(juce::Label::textColourId, pal.bodyText);
        setColour(juce::TextButton::buttonColourId, pal.toolboxItem);
        setColour(juce::TextButton::textColourOffId, pal.buttonText);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::TextEditor::backgroundColourId, pal.panelBg);
        setColour(juce::TextEditor::textColourId, pal.editorText);
        setColour(juce::TextEditor::outlineColourId, pal.border);
        setColour(juce::TextEditor::focusedOutlineColourId, pal.accent);
        setColour(juce::ComboBox::backgroundColourId, pal.toolboxItem);
        setColour(juce::ComboBox::textColourId, pal.buttonText);
        setColour(juce::ComboBox::outlineColourId, pal.border);
        setColour(juce::ComboBox::arrowColourId, pal.buttonText);
        setColour(juce::Slider::backgroundColourId, pal.panelBg.darker(0.2f));
        setColour(juce::Slider::trackColourId, pal.accent);
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxTextColourId, pal.editorText);
        setColour(juce::Slider::textBoxBackgroundColourId, pal.panelBg);
        setColour(juce::Slider::textBoxOutlineColourId, pal.border);
        setColour(juce::ScrollBar::thumbColourId, pal.scrollbar);
        setColour(juce::PopupMenu::backgroundColourId, pal.panelBg);
        setColour(juce::PopupMenu::textColourId, pal.bodyText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, pal.accent);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::ToggleButton::textColourId, pal.bodyText);
        setColour(juce::ToggleButton::tickColourId, pal.accent);
    }

    //==========================================================================
    //  B U T T O N S  — raised 3D bevel
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        float cornerSize = 4.0f;

        auto baseColour = backgroundColour;
        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.25f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.08f);

        // Main body gradient — lighter top, darker bottom
        juce::ColourGradient bodyGrad(
            baseColour.brighter(0.15f), bounds.getX(), bounds.getY(),
            baseColour.darker(0.12f),   bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(bounds, cornerSize);

        if (shouldDrawButtonAsDown)
        {
            // Pressed: inner shadow (dark top edge, lighter bottom)
            juce::ColourGradient innerShadow(
                juce::Colours::black.withAlpha(0.25f), bounds.getX(), bounds.getY(),
                juce::Colours::transparentBlack,       bounds.getX(), bounds.getY() + 6.0f, false);
            g.setGradientFill(innerShadow);
            g.fillRoundedRectangle(bounds, cornerSize);
        }
        else
        {
            // Raised: top highlight line
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawHorizontalLine(static_cast<int>(bounds.getY() + 1),
                                 bounds.getX() + cornerSize, bounds.getRight() - cornerSize);
        }

        // Outer border
        auto& pal = ThemeManager::getInstance().getPalette();
        g.setColour(pal.border.darker(0.15f));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }

    //==========================================================================
    //  T O G G L E   B U T T O N  — rounded checkbox with 3D
    //==========================================================================
    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
    {
        auto& pal = ThemeManager::getInstance().getPalette();
        float fontSize = juce::jmin(15.0f, static_cast<float>(button.getHeight()) * 0.75f);
        float tickW = fontSize * 1.1f;

        auto tickBounds = juce::Rectangle<float>(4.0f, (static_cast<float>(button.getHeight()) - tickW) * 0.5f, tickW, tickW);

        // Sunken checkbox background
        juce::ColourGradient boxGrad(
            pal.panelBg.darker(0.3f), tickBounds.getX(), tickBounds.getY(),
            pal.panelBg.darker(0.1f), tickBounds.getX(), tickBounds.getBottom(), false);
        g.setGradientFill(boxGrad);
        g.fillRoundedRectangle(tickBounds, 3.0f);

        // Inset border
        g.setColour(pal.border.darker(0.2f));
        g.drawRoundedRectangle(tickBounds.reduced(0.5f), 3.0f, 1.0f);

        if (button.getToggleState())
        {
            // Fat check mark
            auto tick = tickBounds.reduced(tickW * 0.22f);
            juce::Path checkPath;
            checkPath.startNewSubPath(tick.getX(), tick.getCentreY());
            checkPath.lineTo(tick.getX() + tick.getWidth() * 0.35f, tick.getBottom());
            checkPath.lineTo(tick.getRight(), tick.getY());
            g.setColour(pal.accent);
            g.strokePath(checkPath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(fontSize);

        if (!button.isEnabled())
            g.setOpacity(0.5f);

        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds()
                             .withTrimmedLeft(juce::roundToInt(tickW) + 10)
                             .withTrimmedRight(2),
                         juce::Justification::centredLeft,
                         10);
    }

    //==========================================================================
    //  C O M B O   B O X  — raised box with 3D arrow
    //==========================================================================
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        float corner = 4.0f;
        auto baseCol = box.findColour(juce::ComboBox::backgroundColourId);
        auto& pal = ThemeManager::getInstance().getPalette();

        if (isButtonDown)
            baseCol = baseCol.darker(0.15f);

        // Raised gradient
        juce::ColourGradient bodyGrad(
            baseCol.brighter(0.12f), 0, 0,
            baseCol.darker(0.10f),   0, static_cast<float>(height), false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(bounds, corner);

        // Top highlight
        if (!isButtonDown)
        {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawHorizontalLine(1, corner, static_cast<float>(width) - corner);
        }

        // Border
        g.setColour(pal.border.darker(0.1f));
        g.drawRoundedRectangle(bounds, corner, 1.0f);

        // Arrow area separator
        float arrowX = static_cast<float>(width) - 25.0f;
        g.setColour(pal.border);
        g.drawVerticalLine(static_cast<int>(arrowX), 4.0f, static_cast<float>(height) - 4.0f);

        // Arrow triangle
        juce::Path arrow;
        float cx = arrowX + 12.5f;
        float cy = static_cast<float>(height) * 0.5f;
        arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour(pal.bodyText.withAlpha(0.7f));
        g.fillPath(arrow);
    }

    //==========================================================================
    //  T E X T   E D I T O R  — sunken / inset field
    //==========================================================================
    void fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                  juce::TextEditor& te) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
        float corner = 3.0f;
        auto bgCol = te.findColour(juce::TextEditor::backgroundColourId);

        // Sunken gradient: darker top → lighter bottom
        juce::ColourGradient sunkenGrad(
            bgCol.darker(0.15f), 0, 0,
            bgCol.brighter(0.05f), 0, static_cast<float>(height), false);
        g.setGradientFill(sunkenGrad);
        g.fillRoundedRectangle(bounds.reduced(0.5f), corner);

        // Inner shadow at top
        juce::ColourGradient innerShadow(
            juce::Colours::black.withAlpha(0.12f), 0, 0,
            juce::Colours::transparentBlack, 0, 5.0f, false);
        g.setGradientFill(innerShadow);
        g.fillRoundedRectangle(bounds.reduced(0.5f), corner);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& te) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
        float corner = 3.0f;

        if (te.hasKeyboardFocus(true))
        {
            g.setColour(te.findColour(juce::TextEditor::focusedOutlineColourId));
            g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.5f);
        }
        else
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            g.setColour(pal.border.darker(0.2f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

            // Bottom highlight for inset effect
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.drawHorizontalLine(height - 1, corner, static_cast<float>(width) - corner);
        }
    }

    //==========================================================================
    //  S L I D E R  — grooved track + glossy thumb
    //==========================================================================
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
            return;
        }

        auto& pal = ThemeManager::getInstance().getPalette();
        bool isHorizontal = (style == juce::Slider::LinearHorizontal);

        auto trackBounds = juce::Rectangle<float>(
            static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(width), static_cast<float>(height));

        if (isHorizontal)
        {
            float trackY = trackBounds.getCentreY() - 3.0f;
            float trackH = 6.0f;
            auto trackRect = juce::Rectangle<float>(trackBounds.getX(), trackY, trackBounds.getWidth(), trackH);

            // Grooved track — sunken
            juce::ColourGradient trackGrad(
                pal.panelBg.darker(0.35f), 0, trackY,
                pal.panelBg.darker(0.15f), 0, trackY + trackH, false);
            g.setGradientFill(trackGrad);
            g.fillRoundedRectangle(trackRect, 3.0f);

            // Inset border for track groove
            g.setColour(pal.border.darker(0.25f));
            g.drawRoundedRectangle(trackRect, 3.0f, 0.8f);

            // Filled portion
            auto filledRect = trackRect.withRight(sliderPos);
            if (filledRect.getWidth() > 0.0f)
            {
                juce::ColourGradient fillGrad(
                    pal.accent.brighter(0.15f), 0, trackY,
                    pal.accent.darker(0.1f),    0, trackY + trackH, false);
                g.setGradientFill(fillGrad);
                g.fillRoundedRectangle(filledRect, 3.0f);
            }

            // Thumb — glossy round knob
            float thumbRadius = 8.0f;
            float thumbX = sliderPos;
            float thumbY = trackBounds.getCentreY();
            drawGlossyKnob(g, thumbX, thumbY, thumbRadius, pal);
        }
        else
        {
            float trackX = trackBounds.getCentreX() - 3.0f;
            float trackW = 6.0f;
            auto trackRect = juce::Rectangle<float>(trackX, trackBounds.getY(), trackW, trackBounds.getHeight());

            // Grooved track
            juce::ColourGradient trackGrad(
                pal.panelBg.darker(0.35f), trackX, 0,
                pal.panelBg.darker(0.15f), trackX + trackW, 0, false);
            g.setGradientFill(trackGrad);
            g.fillRoundedRectangle(trackRect, 3.0f);
            g.setColour(pal.border.darker(0.25f));
            g.drawRoundedRectangle(trackRect, 3.0f, 0.8f);

            // Filled from bottom
            auto filledRect = trackRect.withTop(sliderPos);
            if (filledRect.getHeight() > 0.0f)
            {
                juce::ColourGradient fillGrad(
                    pal.accent.brighter(0.15f), trackX, 0,
                    pal.accent.darker(0.1f),    trackX + trackW, 0, false);
                g.setGradientFill(fillGrad);
                g.fillRoundedRectangle(filledRect, 3.0f);
            }

            float thumbRadius = 8.0f;
            drawGlossyKnob(g, trackBounds.getCentreX(), sliderPos, thumbRadius, pal);
        }
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto& pal = ThemeManager::getInstance().getPalette();
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Background ring (groove)
        g.setColour(pal.panelBg.darker(0.3f));
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Arc for the track
        juce::Path arcBg;
        arcBg.addArc(cx - radius + 2, cy - radius + 2, (radius - 2) * 2, (radius - 2) * 2,
                      rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(pal.border.darker(0.2f));
        g.strokePath(arcBg, juce::PathStrokeType(3.0f));

        // Filled arc
        juce::Path arcFill;
        arcFill.addArc(cx - radius + 2, cy - radius + 2, (radius - 2) * 2, (radius - 2) * 2,
                        rotaryStartAngle, angle, true);
        g.setColour(pal.accent);
        g.strokePath(arcFill, juce::PathStrokeType(3.0f));

        // Glossy knob centre
        float knobR = radius * 0.65f;
        drawGlossyKnob(g, cx, cy, knobR, pal);

        // Pointer line
        juce::Path pointer;
        float pLen = knobR * 0.7f;
        pointer.addRectangle(-1.5f, -pLen, 3.0f, pLen * 0.6f);
        g.setColour(pal.accent);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(cx, cy));
    }

    //==========================================================================
    //  S C R O L L B A R  — raised thumb on grooved track
    //==========================================================================
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override
    {
        juce::ignoreUnused(scrollbar);
        auto& pal = ThemeManager::getInstance().getPalette();

        // Track background
        g.setColour(pal.panelBg.darker(0.15f));
        g.fillRect(x, y, width, height);

        // Thumb
        juce::Rectangle<int> thumbRect;
        if (isScrollbarVertical)
            thumbRect = juce::Rectangle<int>(x + 1, thumbStartPosition, width - 2, thumbSize);
        else
            thumbRect = juce::Rectangle<int>(thumbStartPosition, y + 1, thumbSize, height - 2);

        auto thumbBounds = thumbRect.toFloat();
        auto thumbCol = pal.scrollbar;
        if (isMouseDown)
            thumbCol = thumbCol.brighter(0.2f);
        else if (isMouseOver)
            thumbCol = thumbCol.brighter(0.1f);

        // Raised thumb
        juce::ColourGradient thumbGrad(
            thumbCol.brighter(0.15f), thumbBounds.getX(), thumbBounds.getY(),
            thumbCol.darker(0.1f), thumbBounds.getX(),
            isScrollbarVertical ? thumbBounds.getBottom() : thumbBounds.getY(),
            false);
        if (!isScrollbarVertical)
        {
            thumbGrad = juce::ColourGradient(
                thumbCol.brighter(0.15f), thumbBounds.getX(), thumbBounds.getY(),
                thumbCol.darker(0.1f), thumbBounds.getX(), thumbBounds.getBottom(),
                false);
        }
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(thumbBounds, 3.0f);
    }

    //==========================================================================
    //  P O P U P   M E N U
    //==========================================================================
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto& pal = ThemeManager::getInstance().getPalette();
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        // Slightly raised panel
        g.setColour(pal.panelBg.brighter(0.05f));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Outer shadow line
        g.setColour(pal.border.darker(0.1f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    }

    //==========================================================================
    //  L A B E L  — optional subtle embossed text
    //==========================================================================
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll(label.findColour(juce::Label::backgroundColourId));

        if (!label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            auto font = getLabelFont(label);
            g.setFont(font);

            auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

            // Subtle emboss: dark shadow 1px below
            g.setColour(juce::Colours::black.withAlpha(0.15f * alpha));
            g.drawFittedText(label.getText(), textArea.translated(0, 1),
                             label.getJustificationType(),
                             juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                             label.getMinimumHorizontalScale());

            g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.drawFittedText(label.getText(), textArea,
                             label.getJustificationType(),
                             juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                             label.getMinimumHorizontalScale());
        }
        else if (label.isEnabled())
        {
            g.setColour(label.findColour(juce::Label::outlineColourId));
        }
    }

private:
    //==========================================================================
    /// Draw a glossy circular knob with highlight and shadow.
    void drawGlossyKnob(juce::Graphics& g, float cx, float cy, float radius,
                         const ThemePalette& pal)
    {
        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillEllipse(cx - radius + 1.0f, cy - radius + 1.5f, radius * 2.0f, radius * 2.0f);

        // Main knob body — gradient
        auto knobCol = pal.toolboxItem.brighter(0.3f);
        juce::ColourGradient knobGrad(
            knobCol.brighter(0.25f), cx, cy - radius,
            knobCol.darker(0.2f),    cx, cy + radius, false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Glossy highlight — upper half
        juce::ColourGradient glossGrad(
            juce::Colours::white.withAlpha(0.22f), cx, cy - radius,
            juce::Colours::transparentWhite,       cx, cy, false);
        g.setGradientFill(glossGrad);
        g.fillEllipse(cx - radius * 0.8f, cy - radius * 0.9f, radius * 1.6f, radius * 1.2f);

        // Border ring
        g.setColour(pal.border.darker(0.15f));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkeuomorphicLookAndFeel)
};
