#include "SkinnedVUMeter.h"
#include "../Audio/LevelAnalyzer.h"
#include <cmath>

//==============================================================================
SkinnedVUMeter::SkinnedVUMeter()
{
    updateCoefficients();
}

void SkinnedVUMeter::updateCoefficients()
{
    // Coefficients for exponential smoothing at 60fps
    float riseMs, fallMs;

    switch (ballistic)
    {
        case Ballistic::VU:
            riseMs  = 300.0f;
            fallMs  = decayMs;
            break;
        case Ballistic::PPM_I:
            riseMs  = 5.0f;     // Near-instant attack
            fallMs  = 1700.0f;  // PPM Type I: 1.7s to -20dB
            break;
        case Ballistic::PPM_II:
            riseMs  = 10.0f;
            fallMs  = 2800.0f;  // PPM Type II: 2.8s to -24dB
            break;
        case Ballistic::Nordic:
            riseMs  = 5.0f;
            fallMs  = 1700.0f;
            break;
    }

    // Convert to per-frame coefficients (60fps)
    float frameDuration = 1000.0f / 60.0f;  // ~16.67ms
    attackCoeff  = 1.0f - std::exp(-frameDuration / riseMs);
    releaseCoeff = 1.0f - std::exp(-frameDuration / fallMs);
}

//==============================================================================
void SkinnedVUMeter::setLevel(float linearLevel)
{
    targetLevel = juce::jlimit(0.0f, 2.0f, linearLevel);  // allow >1.0 for clipping display
    smoothLevel();
}

void SkinnedVUMeter::smoothLevel()
{
    if (targetLevel > currentLevel)
        currentLevel += (targetLevel - currentLevel) * attackCoeff;
    else
        currentLevel += (targetLevel - currentLevel) * releaseCoeff;

    // Peak hold
    if (currentLevel >= peakHoldLevel)
    {
        peakHoldLevel = currentLevel;
        peakHoldTimer = 2.0f;  // 2 seconds
    }
    else
    {
        peakHoldTimer -= 1.0f / 60.0f;
        if (peakHoldTimer <= 0.0f)
        {
            peakHoldLevel -= releaseCoeff * 0.5f;
            if (peakHoldLevel < 0.0f)
                peakHoldLevel = 0.0f;
        }
    }
}

//==============================================================================
void SkinnedVUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(getBgColour(juce::Colour(0xFF0A0A1A)));

    // Convert to dB for display (-60..0 range)
    float db = LevelAnalyzer::toDecibels(currentLevel);
    float normalized = (db + 60.0f) / 60.0f;
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    // Reserve space for label
    auto labelArea = (orientation == Orientation::Vertical)
                         ? bounds.removeFromBottom(16)
                         : bounds.removeFromLeft(16);

    // Draw meter
    if (orientation == Orientation::Vertical)
        drawVerticalBar(g, bounds.reduced(2), normalized);
    else
        drawHorizontalBar(g, bounds.reduced(2), normalized);

    // Channel label
    g.setColour(juce::Colours::lightgrey);
    g.setFont(meterFont(11.0f));
    g.drawText(channelName, labelArea, juce::Justification::centred);
}

//==============================================================================
void SkinnedVUMeter::drawVerticalBar(juce::Graphics& g, juce::Rectangle<int> bounds, float normalized)
{
    float h = bounds.getHeight() * normalized;

    // Segmented LED style
    int totalSegments = 30;
    float segH = static_cast<float>(bounds.getHeight()) / totalSegments;
    int litSegments = static_cast<int>(normalized * totalSegments);

    for (int i = 0; i < totalSegments; ++i)
    {
        float segY = bounds.getBottom() - (i + 1) * segH;
        float segNorm = static_cast<float>(i) / totalSegments;

        if (i < litSegments)
            g.setColour(tintFg(levelToColour(segNorm)));
        else
            g.setColour(tintFg(levelToColour(segNorm)).withAlpha(0.08f));

        g.fillRect(static_cast<float>(bounds.getX()), segY,
                    static_cast<float>(bounds.getWidth()), segH - 1.0f);
    }

    // Peak hold indicator
    if (showPeakHold && peakHoldLevel > 0.001f)
    {
        float peakDb = LevelAnalyzer::toDecibels(peakHoldLevel);
        float peakNorm = juce::jlimit(0.0f, 1.0f, (peakDb + 60.0f) / 60.0f);
        float peakY = bounds.getBottom() - peakNorm * bounds.getHeight();

        g.setColour(juce::Colours::white);
        g.fillRect(static_cast<float>(bounds.getX()), peakY,
                    static_cast<float>(bounds.getWidth()), 2.0f);
    }

    // Border
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(bounds, 1);

    // dB scale markers
    g.setFont(meterFont(9.0f));
    g.setColour(juce::Colours::grey);
    const int markers[] = { 0, -6, -12, -18, -24, -36, -48, -60 };
    for (int db : markers)
    {
        float norm = (static_cast<float>(db) + 60.0f) / 60.0f;
        float y = bounds.getBottom() - norm * bounds.getHeight();
        g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(bounds.getRight() + 1),
                              static_cast<float>(bounds.getRight() + 4));
    }
}

void SkinnedVUMeter::drawHorizontalBar(juce::Graphics& g, juce::Rectangle<int> bounds, float normalized)
{
    int totalSegments = 30;
    float segW = static_cast<float>(bounds.getWidth()) / totalSegments;
    int litSegments = static_cast<int>(normalized * totalSegments);

    for (int i = 0; i < totalSegments; ++i)
    {
        float segX = bounds.getX() + i * segW;
        float segNorm = static_cast<float>(i) / totalSegments;

        if (i < litSegments)
            g.setColour(tintFg(levelToColour(segNorm)));
        else
            g.setColour(tintFg(levelToColour(segNorm)).withAlpha(0.08f));

        g.fillRect(segX, static_cast<float>(bounds.getY()),
                    segW - 1.0f, static_cast<float>(bounds.getHeight()));
    }

    // Peak hold
    if (showPeakHold && peakHoldLevel > 0.001f)
    {
        float peakDb = LevelAnalyzer::toDecibels(peakHoldLevel);
        float peakNorm = juce::jlimit(0.0f, 1.0f, (peakDb + 60.0f) / 60.0f);
        float peakX = bounds.getX() + peakNorm * bounds.getWidth();

        g.setColour(juce::Colours::white);
        g.fillRect(peakX, static_cast<float>(bounds.getY()),
                    2.0f, static_cast<float>(bounds.getHeight()));
    }

    g.setColour(juce::Colours::darkgrey);
    g.drawRect(bounds, 1);
}

//==============================================================================
juce::Colour SkinnedVUMeter::levelToColour(float normalized) const
{
    if (currentSkin != nullptr)
    {
        // Map to viscolor palette
        int idx = static_cast<int>(normalized * 23.0f);
        idx = juce::jlimit(1, 23, idx);
        return currentSkin->visColors.colors[static_cast<size_t>(idx)];
    }

    // Default: green → yellow → red
    if (normalized < 0.7f)
        return juce::Colour(0xFF00D4AA);  // teal-green
    else if (normalized < 0.9f)
        return juce::Colour(0xFFFFD700);  // yellow
    else
        return juce::Colour(0xFFFF4444);  // red
}
