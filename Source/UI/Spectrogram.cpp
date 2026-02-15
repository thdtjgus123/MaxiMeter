#include "Spectrogram.h"
#include <cmath>

//==============================================================================
Spectrogram::Spectrogram()
{
    updatePalette();
}

void Spectrogram::resized()
{
    int w = getWidth();
    int h = getHeight();
    if (w > 0 && h > 0)
    {
        spectrogramImage = juce::Image(juce::Image::ARGB, w, h, true);
        writeColumn = 0;
    }
}

//==============================================================================
void Spectrogram::updatePalette()
{
    palette.resize(256);

    for (int i = 0; i < 256; ++i)
    {
        float t = static_cast<float>(i) / 255.0f;

        switch (colourMap)
        {
            case ColourMap::Rainbow:
            {
                // Blue → Cyan → Green → Yellow → Red
                float hue = (1.0f - t) * 0.7f;  // 0.7 (blue) → 0.0 (red)
                palette[static_cast<size_t>(i)] = juce::Colour::fromHSV(hue, 0.9f, 0.1f + t * 0.9f, 1.0f);
                break;
            }
            case ColourMap::Heat:
            {
                // Black → Dark Red → Red → Orange → Yellow → White
                if (t < 0.2f)
                {
                    float s = t / 0.2f;
                    palette[static_cast<size_t>(i)] = juce::Colour::fromFloatRGBA(s * 0.5f, 0.0f, 0.0f, 1.0f);
                }
                else if (t < 0.5f)
                {
                    float s = (t - 0.2f) / 0.3f;
                    palette[static_cast<size_t>(i)] = juce::Colour::fromFloatRGBA(0.5f + s * 0.5f, s * 0.3f, 0.0f, 1.0f);
                }
                else if (t < 0.8f)
                {
                    float s = (t - 0.5f) / 0.3f;
                    palette[static_cast<size_t>(i)] = juce::Colour::fromFloatRGBA(1.0f, 0.3f + s * 0.5f, s * 0.2f, 1.0f);
                }
                else
                {
                    float s = (t - 0.8f) / 0.2f;
                    palette[static_cast<size_t>(i)] = juce::Colour::fromFloatRGBA(1.0f, 0.8f + s * 0.2f, 0.2f + s * 0.8f, 1.0f);
                }
                break;
            }
            case ColourMap::Greyscale:
            {
                uint8_t v = static_cast<uint8_t>(t * 255.0f);
                palette[static_cast<size_t>(i)] = juce::Colour(v, v, v);
                break;
            }
            case ColourMap::Custom:
            {
                // Default to a cool blue→hot pink gradient
                float hue = 0.7f - t * 0.4f;  // blue → magenta
                palette[static_cast<size_t>(i)] = juce::Colour::fromHSV(hue, 0.8f, 0.1f + t * 0.9f, 1.0f);
                break;
            }
        }
    }
}

juce::Colour Spectrogram::dbToColour(float db) const
{
    float normalized = (db - minDbRange) / (maxDbRange - minDbRange);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    int idx = static_cast<int>(normalized * 255.0f);
    return tintFg(palette[static_cast<size_t>(juce::jlimit(0, 255, idx))]);
}

int Spectrogram::binToY(int bin, int numBins, int displayHeight) const
{
    if (numBins <= 0 || displayHeight <= 0) return 0;

    // Map bin to frequency
    float freq = static_cast<float>(bin) * static_cast<float>(sampleRate) / (static_cast<float>(numBins) * 2.0f);

    // Log-scale mapping
    float logMin = std::log10(std::max(minFreq, 1.0f));
    float logMax = std::log10(std::max(maxFreq, 2.0f));
    float logF   = std::log10(std::max(freq, 1.0f));

    float normalized = (logF - logMin) / (logMax - logMin);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    // Invert so low frequencies are at bottom
    return displayHeight - 1 - static_cast<int>(normalized * (displayHeight - 1));
}

//==============================================================================
void Spectrogram::pushSpectrum(const float* data, int numBins)
{
    if (spectrogramImage.isNull() || numBins <= 0) return;

    int w = spectrogramImage.getWidth();
    int h = spectrogramImage.getHeight();

    if (scrollDir == ScrollDirection::Horizontal)
    {
        // Shift existing image left by 1 pixel
        spectrogramImage.moveImageSection(0, 0, 1, 0, w - 1, h);

        // Draw new column on the right edge
        int col = w - 1;
        for (int y = 0; y < h; ++y)
        {
            // Map display Y back to frequency bin
            float normalizedY = 1.0f - static_cast<float>(y) / (h - 1);
            float logMin = std::log10(std::max(minFreq, 1.0f));
            float logMax = std::log10(std::max(maxFreq, 2.0f));
            float freq = std::pow(10.0f, logMin + normalizedY * (logMax - logMin));
            int bin = static_cast<int>(freq * numBins * 2.0f / static_cast<float>(sampleRate));
            bin = juce::jlimit(0, numBins - 1, bin);

            float mag = data[bin];
            float db = (mag > 1.0e-10f) ? 20.0f * std::log10(mag) : minDbRange;
            spectrogramImage.setPixelAt(col, y, dbToColour(db));
        }
    }
    else // Vertical scroll
    {
        spectrogramImage.moveImageSection(0, 1, 0, 0, w, h - 1);

        int row = 0;
        for (int x = 0; x < w; ++x)
        {
            float normalizedX = static_cast<float>(x) / (w - 1);
            float logMin = std::log10(std::max(minFreq, 1.0f));
            float logMax = std::log10(std::max(maxFreq, 2.0f));
            float freq = std::pow(10.0f, logMin + normalizedX * (logMax - logMin));
            int bin = static_cast<int>(freq * numBins * 2.0f / static_cast<float>(sampleRate));
            bin = juce::jlimit(0, numBins - 1, bin);

            float magV = data[bin];
            float dbV = (magV > 1.0e-10f) ? 20.0f * std::log10(magV) : minDbRange;
            spectrogramImage.setPixelAt(x, row, dbToColour(dbV));
        }
    }
}

//==============================================================================
void Spectrogram::paint(juce::Graphics& g)
{
    g.fillAll(getBgColour(juce::Colour(0xFF0A0A1A)));

    if (!spectrogramImage.isNull())
        g.drawImageAt(spectrogramImage, 0, 0);

    // Draw frequency axis labels
    g.setFont(meterFont(9.0f));
    g.setColour(juce::Colours::grey.withAlpha(0.6f));

    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f,
                              2000.0f, 5000.0f, 10000.0f, 20000.0f };
    int h = getHeight();

    if (scrollDir == ScrollDirection::Horizontal)
    {
        for (float f : freqs)
        {
            if (f < minFreq || f > maxFreq) continue;
            float logMin = std::log10(std::max(minFreq, 1.0f));
            float logMax = std::log10(std::max(maxFreq, 2.0f));
            float norm = (std::log10(f) - logMin) / (logMax - logMin);
            int y = h - 1 - static_cast<int>(norm * (h - 1));

            g.drawHorizontalLine(y, 0.0f, 3.0f);

            juce::String label = (f >= 1000.0f)
                ? juce::String(static_cast<int>(f / 1000)) + "k"
                : juce::String(static_cast<int>(f));
            g.drawText(label, 4, y - 5, 30, 10, juce::Justification::centredLeft);
        }
    }
}
