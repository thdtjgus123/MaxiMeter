#pragma once

#include <JuceHeader.h>
#include "ThemeManager.h"
#include "SkeuomorphicLookAndFeel.h"

//==============================================================================
/// Custom LookAndFeel that extends SkeuomorphicLookAndFeel and additionally
/// skins the window title bar buttons (-□X).
class SkinnedTitleBarLookAndFeel : public SkeuomorphicLookAndFeel
{
public:
    SkinnedTitleBarLookAndFeel() { updateColours(); }

    void updateColours()
    {
        // First update all skeuomorphic colours from the theme
        updateFromTheme();

        auto& pal = ThemeManager::getInstance().getPalette();

        // Title bar specific
        setColour(juce::DocumentWindow::textColourId, pal.bodyText);
    }

    void drawDocumentWindowTitleBar(juce::DocumentWindow& window,
                                    juce::Graphics& g,
                                    int w, int h,
                                    int titleSpaceX, int titleSpaceW,
                                    const juce::Image* /*icon*/,
                                    bool /*drawTitleTextOnLeft*/) override
    {
        auto& pal = ThemeManager::getInstance().getPalette();
        float fw = static_cast<float>(w);
        float fh = static_cast<float>(h);

        // --- Brushed-metal gradient background ---
        auto baseBg = pal.transportBg;
        juce::ColourGradient metalGrad(
            baseBg.brighter(0.22f), 0, 0,
            baseBg.darker(0.15f),   0, fh, false);
        metalGrad.addColour(0.45, baseBg.brighter(0.10f));
        metalGrad.addColour(0.55, baseBg.darker(0.05f));
        g.setGradientFill(metalGrad);
        g.fillRect(0, 0, w, h);

        // Top highlight line
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawHorizontalLine(0, 0.0f, fw);

        // Bottom shadow
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.drawHorizontalLine(h - 1, 0.0f, fw);

        // --- Embossed grooves (Winamp-style ridged lines) ---
        // Compute title text width so grooves fill all remaining space
        juce::Font titleFont(13.0f, juce::Font::bold);
        g.setFont(titleFont);
        float titleTextW = static_cast<float>(
            juce::GlyphArrangement::getStringWidthInt(titleFont, window.getName()));
        float titleCentre = fw * 0.5f;
        float titleLeft  = titleCentre - titleTextW * 0.5f;
        float titleRight = titleCentre + titleTextW * 0.5f;

        float grooveL     = static_cast<float>(titleSpaceX) + 4.0f;
        float grooveR     = titleLeft - 8.0f;   // left grooves: up to title text
        float grooveR2Start = titleRight + 8.0f; // right grooves: after title text
        float grooveR2End   = static_cast<float>(titleSpaceX + titleSpaceW) - 4.0f; // up to buttons

        auto drawGrooves = [&](float left, float right) {
            for (int i = 0; i < 5; ++i) {
                float y = fh * 0.3f + static_cast<float>(i) * 3.0f;
                g.setColour(juce::Colours::black.withAlpha(0.18f));
                g.drawHorizontalLine(static_cast<int>(y), left, right);
                g.setColour(juce::Colours::white.withAlpha(0.07f));
                g.drawHorizontalLine(static_cast<int>(y) + 1, left, right);
            }
        };
        if (grooveR > grooveL + 10.0f)
            drawGrooves(grooveL, grooveR);
        if (grooveR2End > grooveR2Start + 10.0f)
            drawGrooves(grooveR2Start, grooveR2End);

        // --- Title text (centred, embossed) ---
        auto textArea = juce::Rectangle<int>(0, 0, w, h);

        // Dark shadow under text
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawText(window.getName(), textArea.translated(1, 1),
                   juce::Justification::centred, true);
        // Bright text on top
        g.setColour(pal.bodyText);
        g.drawText(window.getName(), textArea,
                   juce::Justification::centred, true);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        auto* btn = new WindowButton(buttonType);
        return btn;
    }

private:
    /// Custom button that draws themed -□X icons.
    class WindowButton : public juce::Button
    {
    public:
        WindowButton(int type) : juce::Button(""), buttonType(type) {}

        void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            auto bounds = getLocalBounds().toFloat().reduced(2.0f, 4.0f);
            bool isClose = (buttonType == juce::DocumentWindow::closeButton);

            // --- Small raised square button (Winamp style) ---
            auto btnCol = pal.transportBg.brighter(0.15f);
            if (isButtonDown)
                btnCol = btnCol.darker(0.2f);
            else if (isMouseOver)
                btnCol = isClose ? juce::Colour(0xFFCC2222) : btnCol.brighter(0.12f);

            // Raised gradient
            juce::ColourGradient btnGrad(
                btnCol.brighter(0.18f), bounds.getX(), bounds.getY(),
                btnCol.darker(0.12f),   bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(btnGrad);
            g.fillRoundedRectangle(bounds, 1.5f);

            // Bevel: top-left bright, bottom-right dark
            if (!isButtonDown) {
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.drawHorizontalLine(static_cast<int>(bounds.getY()), bounds.getX() + 1, bounds.getRight() - 1);
                g.drawVerticalLine(static_cast<int>(bounds.getX()), bounds.getY() + 1, bounds.getBottom() - 1);
                g.setColour(juce::Colours::black.withAlpha(0.20f));
                g.drawHorizontalLine(static_cast<int>(bounds.getBottom() - 1), bounds.getX() + 1, bounds.getRight() - 1);
                g.drawVerticalLine(static_cast<int>(bounds.getRight() - 1), bounds.getY() + 1, bounds.getBottom() - 1);
            } else {
                g.setColour(juce::Colours::black.withAlpha(0.15f));
                g.drawHorizontalLine(static_cast<int>(bounds.getY()), bounds.getX() + 1, bounds.getRight() - 1);
                g.drawVerticalLine(static_cast<int>(bounds.getX()), bounds.getY() + 1, bounds.getBottom() - 1);
            }

            // Outer border
            g.setColour(pal.border.darker(0.2f));
            g.drawRoundedRectangle(bounds, 1.5f, 0.8f);

            // --- Icon ---
            g.setColour((isMouseOver && isClose) ? juce::Colours::white : pal.bodyText);
            auto centre = bounds.getCentre();
            float sz = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.22f;

            if (buttonType == juce::DocumentWindow::minimiseButton)
            {
                g.fillRect(centre.x - sz, centre.y + sz * 0.4f, sz * 2.0f, 1.5f);
            }
            else if (buttonType == juce::DocumentWindow::maximiseButton)
            {
                auto r = juce::Rectangle<float>(centre.x - sz, centre.y - sz, sz * 2.0f, sz * 2.0f);
                g.drawRect(r, 1.2f);
                g.fillRect(r.getX(), r.getY(), r.getWidth(), 2.0f);
            }
            else // Close
            {
                g.drawLine(centre.x - sz, centre.y - sz, centre.x + sz, centre.y + sz, 1.4f);
                g.drawLine(centre.x + sz, centre.y - sz, centre.x - sz, centre.y + sz, 1.4f);
            }
        }

    private:
        int buttonType;
    };
};
