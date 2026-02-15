#include "Goniometer.h"
#include <cmath>

//==============================================================================
Goniometer::Goniometer()
{
    points.resize(kMaxPoints);
}

void Goniometer::update(const StereoFieldAnalyzer& analyzer)
{
    numPoints = analyzer.getGonioPoints(points.data(), kMaxPoints);
    correlationValue = analyzer.getCorrelation();
    repaint();
}

//==============================================================================
void Goniometer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(getBgColour(juce::Colour(0xFF0A0A18)));

    // Reserve bottom for correlation bar
    juce::Rectangle<int> corrBarArea;
    if (showCorrelationBar)
        corrBarArea = bounds.removeFromBottom(20);

    auto area = bounds.toFloat();
    float cx = area.getCentreX();
    float cy = area.getCentreY();
    float radius = std::min(area.getWidth(), area.getHeight()) * 0.45f;

    // Grid
    if (showGrid)
        drawGrid(g, area);

    // Draw points with afterglow (older = more transparent)
    if (numPoints > 0)
    {
        for (int i = 0; i < numPoints; ++i)
        {
            float age = static_cast<float>(i) / static_cast<float>(numPoints);
            float alpha = age * trailAlpha;

            float x = cx + points[static_cast<size_t>(i)].x * radius * zoom;
            float y = cy - points[static_cast<size_t>(i)].y * radius * zoom;

            // Clamp to bounds
            x = juce::jlimit(area.getX(), area.getRight(), x);
            y = juce::jlimit(area.getY(), area.getBottom(), y);

            // Color based on position: green (correlated) → red (anti-phase)
            float r = std::fabs(points[static_cast<size_t>(i)].x);
            juce::Colour dotColour = tintFg(juce::Colour(0xFF00DD88).interpolatedWith(
                juce::Colour(0xFFFF4466), juce::jlimit(0.0f, 1.0f, r * 2.0f)));

            g.setColour(dotColour.withAlpha(alpha));
            g.fillEllipse(x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize);
        }
    }

    // Correlation bar
    if (showCorrelationBar && !corrBarArea.isEmpty())
        drawCorrelationBar(g, corrBarArea);
}

//==============================================================================
void Goniometer::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.45f;

    g.setColour(juce::Colours::grey.withAlpha(0.15f));

    // Concentric circles
    for (int i = 1; i <= 3; ++i)
    {
        float r = radius * static_cast<float>(i) / 3.0f;
        g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 0.5f);
    }

    // Axis lines: +M (top), -M (bottom), +S (right), -S (left)
    g.drawLine(cx, bounds.getY(), cx, bounds.getBottom(), 0.5f);
    g.drawLine(bounds.getX(), cy, bounds.getRight(), cy, 0.5f);

    // Diagonal lines: L and R axes
    g.setColour(juce::Colours::grey.withAlpha(0.1f));
    g.drawLine(cx - radius, cy + radius, cx + radius, cy - radius, 0.5f);
    g.drawLine(cx - radius, cy - radius, cx + radius, cy + radius, 0.5f);

    // Labels
    g.setFont(meterFont(10.0f));
    g.setColour(juce::Colours::grey.withAlpha(0.5f));
    g.drawText("M+", static_cast<int>(cx) - 10, static_cast<int>(bounds.getY()), 20, 14,
               juce::Justification::centred);
    g.drawText("L", static_cast<int>(bounds.getX()), static_cast<int>(cy) - 7, 14, 14,
               juce::Justification::centred);
    g.drawText("R", static_cast<int>(bounds.getRight()) - 14, static_cast<int>(cy) - 7, 14, 14,
               juce::Justification::centred);
}

void Goniometer::drawCorrelationBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto bgCol = getBgColour(juce::Colour(0xFF111122));
    g.setColour(bgCol);
    g.fillRect(area);

    auto bar = area.reduced(4, 3);
    float cx = static_cast<float>(bar.getCentreX());
    float halfW = static_cast<float>(bar.getWidth()) * 0.5f;

    // Background bar (slightly brighter than outer bg)
    g.setColour(hasCustomBg() ? bgCol.brighter(0.15f) : tintSecondary(juce::Colour(0xFF222244)));
    g.fillRect(bar);

    // Center tick
    g.setColour(juce::Colours::grey);
    g.drawVerticalLine(static_cast<int>(cx), static_cast<float>(bar.getY()),
                        static_cast<float>(bar.getBottom()));

    // Correlation indicator
    float pos = cx + correlationValue * halfW;
    float indicatorW = 4.0f;

    juce::Colour indColour;
    if (correlationValue > 0.3f)
        indColour = tintFg(juce::Colour(0xFF00DD88));  // good correlation (green)
    else if (correlationValue > -0.3f)
        indColour = tintFg(juce::Colour(0xFFFFDD00));  // neutral (yellow)
    else
        indColour = tintFg(juce::Colour(0xFFFF4444));  // anti-phase (red)

    g.setColour(indColour);
    g.fillRect(pos - indicatorW * 0.5f, static_cast<float>(bar.getY()),
               indicatorW, static_cast<float>(bar.getHeight()));

    // Labels
    g.setFont(meterFont(9.0f));
    g.setColour(juce::Colours::grey);
    g.drawText("-1", bar.getX(), bar.getY(), 16, bar.getHeight(), juce::Justification::centredLeft);
    g.drawText("+1", bar.getRight() - 16, bar.getY(), 16, bar.getHeight(), juce::Justification::centredRight);

    // Numeric value
    g.setColour(indColour);
    g.drawText(juce::String(correlationValue, 2),
               static_cast<int>(cx) - 20, bar.getY() - 2,
               40, bar.getHeight() + 4, juce::Justification::centred);
}
