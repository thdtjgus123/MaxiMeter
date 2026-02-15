#include "LissajousScope.h"
#include <cmath>

//==============================================================================
LissajousScope::LissajousScope()
{
    points.resize(kMaxPoints);
}

void LissajousScope::update(const StereoFieldAnalyzer& analyzer)
{
    switch (mode)
    {
        case Mode::Lissajous:
            numPoints = analyzer.getLissajousPoints(points.data(), std::min(trailLength, kMaxPoints));
            break;
        case Mode::XY:
        case Mode::Polar:
            numPoints = analyzer.getGonioPoints(points.data(), std::min(trailLength, kMaxPoints));
            break;
    }
    repaint();
}

//==============================================================================
void LissajousScope::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(getBgColour(juce::Colour(0xFF0A0A18)));

    // Label
    g.setFont(10.0f);
    g.setColour(juce::Colours::grey.withAlpha(0.5f));
    juce::String modeLabel;
    switch (mode)
    {
        case Mode::Lissajous: modeLabel = "Lissajous (L/R)"; break;
        case Mode::XY:        modeLabel = "XY (M/S)"; break;
        case Mode::Polar:     modeLabel = "Polar"; break;
    }
    g.drawText(modeLabel, bounds.removeFromTop(14).toNearestInt(), juce::Justification::centredLeft);

    auto area = bounds.reduced(4);
    float cx = area.getCentreX();
    float cy = area.getCentreY();
    float radius = std::min(area.getWidth(), area.getHeight()) * 0.45f;

    if (showGrid)
        drawGrid(g, area);

    if (numPoints < 2) return;

    // Draw with afterglow trail
    if (mode == Mode::Polar)
    {
        // Polar: map to angle/magnitude
        for (int i = 0; i < numPoints; ++i)
        {
            auto& p = points[static_cast<size_t>(i)];
            float age = static_cast<float>(i) / numPoints;
            float alpha = age * 0.8f;

            float mag = std::sqrt(p.x * p.x + p.y * p.y);
            float r = mag * radius * zoom;
            float angle = std::atan2(p.y, p.x);

            float px = cx + r * std::cos(angle);
            float py = cy - r * std::sin(angle);

            g.setColour(tintFg(waveColour).withAlpha(alpha));
            g.fillEllipse(px - 0.75f, py - 0.75f, 1.5f, 1.5f);
        }
    }
    else
    {
        // Line path
        juce::Path path;
        bool started = false;

        for (int i = 0; i < numPoints; ++i)
        {
            auto& p = points[static_cast<size_t>(i)];
            float x = cx + p.x * radius * zoom;
            float y = cy - p.y * radius * zoom;

            x = juce::jlimit(area.getX(), area.getRight(), x);
            y = juce::jlimit(area.getY(), area.getBottom(), y);

            if (!started) { path.startNewSubPath(x, y); started = true; }
            else path.lineTo(x, y);
        }

        // Draw with gradient: older segments more transparent
        g.setColour(tintFg(waveColour).withAlpha(0.6f));
        g.strokePath(path, juce::PathStrokeType(lineWidth));

        // Draw last few points brighter (head of the trail)
        int headLen = std::min(numPoints, 256);
        for (int i = numPoints - headLen; i < numPoints; ++i)
        {
            auto& p = points[static_cast<size_t>(i)];
            float age = static_cast<float>(i - (numPoints - headLen)) / headLen;
            float x = cx + p.x * radius * zoom;
            float y = cy - p.y * radius * zoom;
            x = juce::jlimit(area.getX(), area.getRight(), x);
            y = juce::jlimit(area.getY(), area.getBottom(), y);

            g.setColour(tintFg(waveColour).withAlpha(0.3f + age * 0.7f));
            g.fillEllipse(x - 1.0f, y - 1.0f, 2.0f, 2.0f);
        }
    }
}

//==============================================================================
void LissajousScope::drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    float cx = area.getCentreX();
    float cy = area.getCentreY();
    float radius = std::min(area.getWidth(), area.getHeight()) * 0.45f;

    g.setColour(juce::Colours::grey.withAlpha(0.12f));

    // Concentric circles / boxes
    for (int i = 1; i <= 4; ++i)
    {
        float r = radius * static_cast<float>(i) / 4.0f;
        g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 0.5f);
    }

    // Cross-hairs
    g.drawLine(cx, area.getY(), cx, area.getBottom(), 0.5f);
    g.drawLine(area.getX(), cy, area.getRight(), cy, 0.5f);

    // Diagonal axes
    g.setColour(juce::Colours::grey.withAlpha(0.08f));
    g.drawLine(cx - radius, cy + radius, cx + radius, cy - radius, 0.5f);
    g.drawLine(cx - radius, cy - radius, cx + radius, cy + radius, 0.5f);

    // Axis labels
    g.setFont(9.0f);
    g.setColour(juce::Colours::grey.withAlpha(0.4f));

    if (mode == Mode::Lissajous)
    {
        g.drawText("L", static_cast<int>(area.getX()), static_cast<int>(cy) - 7, 12, 14,
                   juce::Justification::centred);
        g.drawText("R", static_cast<int>(area.getRight()) - 12, static_cast<int>(cy) - 7, 12, 14,
                   juce::Justification::centred);
    }
    else
    {
        g.drawText("S-", static_cast<int>(area.getX()), static_cast<int>(cy) - 7, 14, 14,
                   juce::Justification::centred);
        g.drawText("S+", static_cast<int>(area.getRight()) - 14, static_cast<int>(cy) - 7, 14, 14,
                   juce::Justification::centred);
        g.drawText("M+", static_cast<int>(cx) - 10, static_cast<int>(area.getY()), 20, 14,
                   juce::Justification::centred);
    }
}
