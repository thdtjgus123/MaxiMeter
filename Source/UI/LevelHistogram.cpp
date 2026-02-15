#include "LevelHistogram.h"
#include <cmath>
#include <algorithm>

//==============================================================================
LevelHistogram::LevelHistogram()
{
    rebuildBins();
}

void LevelHistogram::rebuildBins()
{
    numBins = static_cast<int>((maxRange - minRange) / binRes) + 1;
    numBins = std::max(numBins, 1);
    binsL.assign(static_cast<size_t>(numBins), 0.0);
    binsR.assign(static_cast<size_t>(numBins), 0.0);
    totalSamples = 0;
}

int LevelHistogram::dbToBin(float db) const
{
    int bin = static_cast<int>((db - minRange) / binRes);
    return juce::jlimit(0, numBins - 1, bin);
}

void LevelHistogram::pushLevel(float leftLinear, float rightLinear)
{
    float dbL = (leftLinear > 0.0f) ? 20.0f * std::log10(leftLinear) : -100.0f;
    float dbR = (rightLinear > 0.0f) ? 20.0f * std::log10(rightLinear) : -100.0f;

    if (dbL >= minRange)
        binsL[static_cast<size_t>(dbToBin(dbL))] += 1.0;

    if (dbR >= minRange)
        binsR[static_cast<size_t>(dbToBin(dbR))] += 1.0;

    totalSamples += 1.0;

    if (!cumulative && totalSamples > 18000)  // ~5 minutes at 60fps
    {
        // Decay old data
        for (auto& b : binsL) b *= 0.999;
        for (auto& b : binsR) b *= 0.999;
        totalSamples *= 0.999;
    }

    repaint();
}

void LevelHistogram::reset()
{
    for (auto& b : binsL) b = 0.0;
    for (auto& b : binsR) b = 0.0;
    totalSamples = 0;
}

//==============================================================================
void LevelHistogram::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(getBgColour(juce::Colour(0xFF0D0D1A)));

    if (totalSamples < 1.0) return;

    // Find max bin value for normalization
    double maxVal = 1.0;
    for (int i = 0; i < numBins; ++i)
    {
        maxVal = std::max(maxVal, binsL[static_cast<size_t>(i)]);
        if (showStereo)
            maxVal = std::max(maxVal, binsR[static_cast<size_t>(i)]);
    }

    auto area = bounds.reduced(4);

    // Title
    auto titleArea = area.removeFromTop(14);
    g.setFont(meterFont(10.0f));
    g.setColour(juce::Colours::grey);
    g.drawText("Level Distribution", titleArea, juce::Justification::centredLeft);

    // Draw histogram
    if (isHorizontal)
    {
        // Horizontal: bins along X axis, height = count
        float binW = static_cast<float>(area.getWidth()) / numBins;

        for (int i = 0; i < numBins; ++i)
        {
            float x = area.getX() + i * binW;
            float normalizedL = static_cast<float>(binsL[static_cast<size_t>(i)] / maxVal);
            float barH = normalizedL * area.getHeight();

            // dB color
            float db = minRange + i * binRes;
            float hue = juce::jlimit(0.0f, 0.33f, (1.0f - (db - minRange) / (maxRange - minRange)) * 0.33f);

            if (showStereo)
            {
                float halfW = binW * 0.45f;
                // Left channel
                g.setColour(tintFg(juce::Colour::fromHSV(0.33f, 0.7f, 0.8f, 0.8f)));
                g.fillRect(x, area.getBottom() - barH, halfW, barH);

                // Right channel
                float normalizedR = static_cast<float>(binsR[static_cast<size_t>(i)] / maxVal);
                float barHR = normalizedR * area.getHeight();
                g.setColour(tintFg(juce::Colour::fromHSV(0.55f, 0.7f, 0.8f, 0.8f)));
                g.fillRect(x + halfW + 1, area.getBottom() - barHR, halfW, barHR);
            }
            else
            {
                g.setColour(tintFg(juce::Colour::fromHSV(hue, 0.7f, 0.8f, 0.8f)));
                g.fillRect(x, area.getBottom() - barH, binW - 1, barH);
            }
        }

        // Scale labels
        g.setFont(meterFont(8.0f));
        g.setColour(juce::Colours::grey.withAlpha(0.6f));
        for (int db = static_cast<int>(minRange); db <= static_cast<int>(maxRange); db += 6)
        {
            int bin = dbToBin(static_cast<float>(db));
            float x = area.getX() + bin * binW;
            g.drawText(juce::String(db), static_cast<int>(x) - 10,
                       area.getBottom(), 20, 12, juce::Justification::centred);
        }
    }
    else
    {
        // Vertical: bins along Y axis, width = count
        float binH = static_cast<float>(area.getHeight()) / numBins;

        for (int i = 0; i < numBins; ++i)
        {
            float y = area.getBottom() - (i + 1) * binH;
            float normalizedL = static_cast<float>(binsL[static_cast<size_t>(i)] / maxVal);
            float barW = normalizedL * area.getWidth();

            float db = minRange + i * binRes;
            float hue = juce::jlimit(0.0f, 0.33f, (1.0f - (db - minRange) / (maxRange - minRange)) * 0.33f);

            if (showStereo)
            {
                float halfH = binH * 0.45f;
                g.setColour(tintFg(juce::Colour::fromHSV(0.33f, 0.7f, 0.8f, 0.8f)));
                g.fillRect(static_cast<float>(area.getX()), y, barW, halfH);

                float normalizedR = static_cast<float>(binsR[static_cast<size_t>(i)] / maxVal);
                float barWR = normalizedR * area.getWidth();
                g.setColour(tintFg(juce::Colour::fromHSV(0.55f, 0.7f, 0.8f, 0.8f)));
                g.fillRect(static_cast<float>(area.getX()), y + halfH + 1, barWR, halfH);
            }
            else
            {
                g.setColour(tintFg(juce::Colour::fromHSV(hue, 0.7f, 0.8f, 0.8f)));
                g.fillRect(static_cast<float>(area.getX()), y, barW, binH - 1);
            }
        }
    }

    // Border
    g.setColour(juce::Colour(0xFF333344));
    g.drawRect(area, 1);

    // Legend
    if (showStereo)
    {
        g.setFont(meterFont(9.0f));
        g.setColour(tintFg(juce::Colour::fromHSV(0.33f, 0.7f, 0.8f, 0.8f)));
        g.fillRect(bounds.getRight() - 50, bounds.getY() + 2, 8, 8);
        g.setColour(juce::Colours::grey);
        g.drawText("L", bounds.getRight() - 40, bounds.getY(), 12, 12, juce::Justification::centredLeft);

        g.setColour(tintFg(juce::Colour::fromHSV(0.55f, 0.7f, 0.8f, 0.8f)));
        g.fillRect(bounds.getRight() - 26, bounds.getY() + 2, 8, 8);
        g.setColour(juce::Colours::grey);
        g.drawText("R", bounds.getRight() - 16, bounds.getY(), 12, 12, juce::Justification::centredLeft);
    }
}
