#include "SkinnedSpectrumAnalyzer.h"

//==============================================================================
SkinnedSpectrumAnalyzer::SkinnedSpectrumAnalyzer()
{
    bandLevels.fill(-60.0f);
    smoothedLevels.fill(-60.0f);
    peakLevels.fill(-60.0f);
    peakTimers.fill(0.0f);
}

void SkinnedSpectrumAnalyzer::setSpectrumData(const float* data, int numBands)
{
    int count = juce::jmin(numBands, kMaxBands);
    float smoothingCoeff = 0.7f;

    for (int i = 0; i < count; ++i)
    {
        auto idx = static_cast<size_t>(i);
        float newLevel = juce::jlimit(-60.0f, 0.0f, data[i]);
        bandLevels[idx] = newLevel;

        // Smooth: rise fast, fall slow
        if (newLevel > smoothedLevels[idx])
            smoothedLevels[idx] = newLevel;
        else
            smoothedLevels[idx] = smoothedLevels[idx] * smoothingCoeff + newLevel * (1.0f - smoothingCoeff);

        // Peak hold
        if (newLevel >= peakLevels[idx])
        {
            peakLevels[idx] = newLevel;
            peakTimers[idx] = 1.5f;  // hold for 1.5 seconds
        }
        else
        {
            peakTimers[idx] -= 1.0f / 60.0f;  // assume 60fps
            if (peakTimers[idx] <= 0.0f)
            {
                peakLevels[idx] -= decayRate / 60.0f;
                if (peakLevels[idx] < -60.0f)
                    peakLevels[idx] = -60.0f;
            }
        }
    }
}

//==============================================================================
void SkinnedSpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    juce::Colour bg(0xFF1A1A2E);
    if (currentSkin != nullptr)
        bg = currentSkin->visColors.colors[0];
    g.fillAll(getBgColour(bg));

    float barWidth = static_cast<float>(bounds.getWidth()) / numDisplayBands;
    float maxHeight = static_cast<float>(bounds.getHeight());

    for (int i = 0; i < numDisplayBands; ++i)
    {
        auto idx = static_cast<size_t>(i);
        float normalized = (smoothedLevels[idx] + 60.0f) / 60.0f;
        normalized = juce::jlimit(0.0f, 1.0f, normalized);

        float barH = normalized * maxHeight;
        float x = bounds.getX() + i * barWidth;
        float barX = x + static_cast<float>(barGap);
        float barW = barWidth - static_cast<float>(barGap) * 2.0f;

        if (currentSkin != nullptr)
        {
            // Use viscolor palette — draw as discrete LED cells
            int numCells = static_cast<int>(std::ceil(normalized * 16.0f));
            float cellH = maxHeight / 16.0f;

            for (int cell = 0; cell < numCells; ++cell)
            {
                int colorIdx = static_cast<int>(static_cast<float>(cell) / 16.0f * 23.0f);
                colorIdx = juce::jlimit(1, 23, colorIdx);

                float cellY = bounds.getBottom() - (cell + 1) * cellH;
                g.setColour(currentSkin->visColors.colors[static_cast<size_t>(colorIdx)]);
                g.fillRect(barX, cellY, barW, cellH - 1.0f);
            }
        }
        else
        {
            // Default color scheme
            g.setColour(tintFg(getBarColour(normalized)));
            g.fillRect(barX, bounds.getBottom() - barH, barW, barH);
        }

        // Peak hold dot
        if (peakHoldEnabled)
        {
            float peakNorm = (peakLevels[idx] + 60.0f) / 60.0f;
            peakNorm = juce::jlimit(0.0f, 1.0f, peakNorm);

            if (peakNorm > 0.01f)
            {
                float peakY = bounds.getBottom() - peakNorm * maxHeight;
                g.setColour(juce::Colours::white);
                g.fillRect(barX, peakY, barW, 2.0f);
            }
        }
    }
}

//==============================================================================
juce::Colour SkinnedSpectrumAnalyzer::getBarColour(float normalizedLevel) const
{
    if (normalizedLevel < 0.5f)
        return juce::Colour::fromHSV(0.33f, 0.8f, 0.4f + normalizedLevel * 0.6f, 1.0f);
    else if (normalizedLevel < 0.8f)
        return juce::Colour::fromHSV(0.16f, 0.8f, 0.6f + normalizedLevel * 0.4f, 1.0f);
    else
        return juce::Colour::fromHSV(0.0f, 0.9f, 0.8f + normalizedLevel * 0.2f, 1.0f);
}
