#include "MultiBandAnalyzer.h"
#include <cmath>
#include <algorithm>

//==============================================================================
MultiBandAnalyzer::MultiBandAnalyzer()
{
    bandInfos.resize(kMaxBands);
}

float MultiBandAnalyzer::dbToNormalized(float db) const
{
    return juce::jlimit(0.0f, 1.0f, (db - minRange) / (maxRange - minRange));
}

//==============================================================================
void MultiBandAnalyzer::computeBandBoundaries(int numBins, double sampleRate)
{
    bandInfos.resize(static_cast<size_t>(numBands));

    float nyquist = static_cast<float>(sampleRate) * 0.5f;

    switch (scaleMode)
    {
        case ScaleMode::Logarithmic:
        {
            // Logarithmically spaced from 20 Hz to Nyquist
            float logMin = std::log10(20.0f);
            float logMax = std::log10(std::min(nyquist, 20000.0f));

            for (int i = 0; i < numBands; ++i)
            {
                float t0 = static_cast<float>(i) / numBands;
                float t1 = static_cast<float>(i + 1) / numBands;
                float tc = static_cast<float>(i) / numBands + 0.5f / numBands;

                bandInfos[static_cast<size_t>(i)].lowFreq    = std::pow(10.0f, logMin + t0 * (logMax - logMin));
                bandInfos[static_cast<size_t>(i)].highFreq   = std::pow(10.0f, logMin + t1 * (logMax - logMin));
                bandInfos[static_cast<size_t>(i)].centerFreq = std::pow(10.0f, logMin + tc * (logMax - logMin));
            }
            break;
        }
        case ScaleMode::Linear:
        {
            float step = nyquist / numBands;
            for (int i = 0; i < numBands; ++i)
            {
                bandInfos[static_cast<size_t>(i)].lowFreq    = i * step;
                bandInfos[static_cast<size_t>(i)].highFreq   = (i + 1) * step;
                bandInfos[static_cast<size_t>(i)].centerFreq = (i + 0.5f) * step;
            }
            break;
        }
        case ScaleMode::Octave:
        {
            // ISO 1/3 octave or 1/1 octave depending on numBands
            float baseFraction = (numBands <= 12) ? 1.0f : (numBands <= 24 ? 0.5f : (1.0f / 3.0f));
            float baseFreq = 31.25f;

            for (int i = 0; i < numBands; ++i)
            {
                float fc = baseFreq * std::pow(2.0f, i * baseFraction);
                float factor = std::pow(2.0f, baseFraction * 0.5f);
                bandInfos[static_cast<size_t>(i)].centerFreq = fc;
                bandInfos[static_cast<size_t>(i)].lowFreq    = fc / factor;
                bandInfos[static_cast<size_t>(i)].highFreq   = fc * factor;
            }
            break;
        }
    }
}

//==============================================================================
void MultiBandAnalyzer::setSpectrumData(const float* data, int numBins, double sampleRate)
{
    computeBandBoundaries(numBins, sampleRate);

    float binWidth = static_cast<float>(sampleRate) / (numBins * 2.0f);
    float dt = 1.0f / 60.0f;

    for (int b = 0; b < numBands; ++b)
    {
        auto& info = bandInfos[static_cast<size_t>(b)];

        // Average bins in this band's frequency range
        int binLow  = static_cast<int>(info.lowFreq / binWidth);
        int binHigh = static_cast<int>(info.highFreq / binWidth);
        binLow  = juce::jlimit(0, numBins - 1, binLow);
        binHigh = juce::jlimit(binLow, numBins - 1, binHigh);

        float sum = 0.0f;
        int count = 0;
        for (int i = binLow; i <= binHigh; ++i)
        {
            sum += data[i];
            count++;
        }

        // data[] is linear magnitude — convert to dB for display
        float mag = (count > 0) ? sum / count : 0.0f;
        float level = (mag > 1.0e-10f) ? 20.0f * std::log10(mag) : minRange;
        bandLevels[static_cast<size_t>(b)] = level;

        // Smooth
        float target = level;
        float& sm = smoothed[static_cast<size_t>(b)];
        if (target > sm)
            sm = target;  // instant attack
        else
            sm = std::max(target, sm - decayRate * dt);

        // Peak hold
        float& pk = peaks[static_cast<size_t>(b)];
        float& pt = peakTimers[static_cast<size_t>(b)];
        if (sm >= pk)
        {
            pk = sm;
            pt = 2.0f;  // 2 second hold
        }
        else
        {
            pt -= dt;
            if (pt <= 0.0f)
                pk = std::max(minRange, pk - decayRate * 0.5f * dt);
        }
    }

    repaint();
}

//==============================================================================
juce::Colour MultiBandAnalyzer::getBarColour(float normalized, int /*band*/) const
{
    if (currentSkin != nullptr)
    {
        int idx = static_cast<int>(normalized * 23.0f);
        idx = juce::jlimit(1, 23, idx);
        return currentSkin->visColors.colors[static_cast<size_t>(idx)];
    }

    // Default gradient: teal → yellow → red
    juce::Colour result;
    if (normalized < 0.6f)
        result = juce::Colour(0xFF00CC88).interpolatedWith(juce::Colour(0xFF44DDAA), normalized / 0.6f);
    else if (normalized < 0.85f)
        result = juce::Colour(0xFF44DDAA).interpolatedWith(juce::Colour(0xFFFFDD00), (normalized - 0.6f) / 0.25f);
    else
        result = juce::Colour(0xFFFFDD00).interpolatedWith(juce::Colour(0xFFFF3333), (normalized - 0.85f) / 0.15f);
    return tintFg(result);
}

//==============================================================================
void MultiBandAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(getBgColour(juce::Colour(0xFF0A0A1A)));

    auto area = bounds;

    // Freq labels at bottom
    juce::Rectangle<int> freqLabelArea;
    if (showFreqLabels)
        freqLabelArea = area.removeFromBottom(14);

    area = area.reduced(2, 2);

    // Grid
    if (showGrid)
        drawGrid(g, area);

    // Draw bars
    float barW = static_cast<float>(area.getWidth()) / numBands;

    for (int b = 0; b < numBands; ++b)
    {
        float x = area.getX() + b * barW;
        float norm = dbToNormalized(smoothed[static_cast<size_t>(b)]);
        float barH = norm * area.getHeight();

        switch (barStyle)
        {
            case BarStyle::Filled:
            {
                // Gradient bar
                for (int py = 0; py < static_cast<int>(barH); ++py)
                {
                    float segNorm = static_cast<float>(py) / area.getHeight();
                    g.setColour(getBarColour(segNorm, b));
                    g.fillRect(x + 1.0f, area.getBottom() - py - 1.0f, barW - 2.0f, 1.0f);
                }
                break;
            }
            case BarStyle::LED:
            {
                int totalSegs = 24;
                float segH = static_cast<float>(area.getHeight()) / totalSegs;
                int litSegs = static_cast<int>(norm * totalSegs);

                for (int s = 0; s < totalSegs; ++s)
                {
                    float sy = area.getBottom() - (s + 1) * segH;
                    float segNorm = static_cast<float>(s) / totalSegs;

                    if (s < litSegs)
                        g.setColour(getBarColour(segNorm, b));
                    else
                        g.setColour(getBarColour(segNorm, b).withAlpha(0.05f));

                    g.fillRect(x + 1.0f, sy, barW - 2.0f, segH - 1.0f);
                }
                break;
            }
            case BarStyle::Outline:
            {
                g.setColour(getBarColour(norm, b));
                g.drawRect(x + 1.0f, area.getBottom() - barH, barW - 2.0f, barH, 1.0f);
                break;
            }
        }

        // Peak hold dot
        if (peakHoldEnabled)
        {
            float peakNorm = dbToNormalized(peaks[static_cast<size_t>(b)]);
            if (peakNorm > 0.0f)
            {
                float peakY = area.getBottom() - peakNorm * area.getHeight();
                g.setColour(juce::Colours::white);
                g.fillRect(x + 1.0f, peakY - 1.0f, barW - 2.0f, 2.0f);
            }
        }
    }

    // Frequency labels
    if (showFreqLabels && !freqLabelArea.isEmpty() && !bandInfos.empty())
    {
        g.setFont(meterFont(8.0f));
        g.setColour(juce::Colours::grey.withAlpha(0.6f));

        // Show labels for a subset of bands
        int step = std::max(1, numBands / 10);
        for (int b = 0; b < numBands; b += step)
        {
            float x = static_cast<float>(area.getX()) + b * barW + barW * 0.5f;
            float freq = bandInfos[static_cast<size_t>(b)].centerFreq;

            juce::String label;
            if (freq >= 1000.0f)
                label = juce::String(freq / 1000.0f, 1) + "k";
            else
                label = juce::String(static_cast<int>(freq));

            g.drawText(label, static_cast<int>(x) - 15, freqLabelArea.getY(),
                       30, 14, juce::Justification::centred);
        }
    }
}

void MultiBandAnalyzer::drawGrid(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colours::grey.withAlpha(0.1f));

    // dB horizontal lines
    for (int db = static_cast<int>(maxRange); db >= static_cast<int>(minRange); db -= 6)
    {
        float norm = dbToNormalized(static_cast<float>(db));
        int y = area.getBottom() - static_cast<int>(norm * area.getHeight());
        g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

        // Label
        g.setFont(meterFont(8.0f));
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawText(juce::String(db), area.getX(), y - 5, 20, 10, juce::Justification::centredLeft);
        g.setColour(juce::Colours::grey.withAlpha(0.1f));
    }
}
