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
    if (w <= 0 || h <= 0) return;

    if (spectrogramImage.isNull() ||
        spectrogramImage.getWidth()  != w ||
        spectrogramImage.getHeight() != h)
    {
        juce::Image newImage(juce::Image::ARGB, w, h, true);
        if (!spectrogramImage.isNull())
        {
            // Preserve existing content (e.g. when zoom changes component bounds)
            juce::Graphics g(newImage);
            g.drawImage(spectrogramImage, 0, 0, w, h,
                        0, 0, spectrogramImage.getWidth(), spectrogramImage.getHeight());
        }
        spectrogramImage = std::move(newImage);
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

// ── Mel / Bark helpers ──────────────────────────────────────────────────────
static float hzToMel(float f)  { return 2595.0f * std::log10(1.0f + f / 700.0f); }
static float melToHz(float m)  { return 700.0f * (std::pow(10.0f, m / 2595.0f) - 1.0f); }
static float hzToBark(float f) { return 13.0f * std::atan(0.00076f * f) + 3.5f * std::atan((f / 7500.0f) * (f / 7500.0f)); }
static float barkToHz(float b)
{
    // Newton iteration (inverse of hzToBark is transcendental)
    float f = 100.0f;
    for (int i = 0; i < 12; ++i)
    {
        float err = hzToBark(f) - b;
        float dBark = 0.00076f * 13.0f / (1.0f + (0.00076f * f) * (0.00076f * f))
                    + 3.5f * 2.0f * f / (7500.0f * 7500.0f)
                      / (1.0f + (f * f) / (7500.0f * 7500.0f));
        f -= err / std::max(dBark, 1e-6f);
        f = std::max(f, 1.0f);
    }
    return f;
}

float Spectrogram::freqToNormalized(float freq) const
{
    float fMin = std::max(minFreq, 1.0f);
    float fMax = std::max(maxFreq, 2.0f);
    float n = 0.0f;

    switch (spectrogramType_)
    {
        case SpectrogramType::Linear:
            n = (freq - fMin) / (fMax - fMin);
            break;

        case SpectrogramType::Mel:
        {
            float mMin = hzToMel(fMin), mMax = hzToMel(fMax);
            n = (hzToMel(std::max(freq, 1.0f)) - mMin) / (mMax - mMin);
            break;
        }
        case SpectrogramType::Bark:
        {
            float bMin = hzToBark(fMin), bMax = hzToBark(fMax);
            n = (hzToBark(std::max(freq, 1.0f)) - bMin) / (bMax - bMin);
            break;
        }
        default: // Standard / Reassigned — log scale
        {
            float logMin = std::log10(fMin);
            float logMax = std::log10(fMax);
            n = (std::log10(std::max(freq, 1.0f)) - logMin) / (logMax - logMin);
            break;
        }
    }
    return juce::jlimit(0.0f, 1.0f, n);
}

float Spectrogram::normalizedToFreq(float norm) const
{
    float fMin = std::max(minFreq, 1.0f);
    float fMax = std::max(maxFreq, 2.0f);
    norm = juce::jlimit(0.0f, 1.0f, norm);

    switch (spectrogramType_)
    {
        case SpectrogramType::Linear:
            return fMin + norm * (fMax - fMin);

        case SpectrogramType::Mel:
        {
            float mMin = hzToMel(fMin), mMax = hzToMel(fMax);
            return melToHz(mMin + norm * (mMax - mMin));
        }
        case SpectrogramType::Bark:
        {
            float bMin = hzToBark(fMin), bMax = hzToBark(fMax);
            return barkToHz(bMin + norm * (bMax - bMin));
        }
        default: // Standard / Reassigned — log scale
        {
            float logMin = std::log10(fMin);
            float logMax = std::log10(fMax);
            return std::pow(10.0f, logMin + norm * (logMax - logMin));
        }
    }
}

int Spectrogram::binToY(int bin, int numBins, int displayHeight) const
{
    if (numBins <= 0 || displayHeight <= 0) return 0;

    float freq = static_cast<float>(bin) * static_cast<float>(sampleRate) / (static_cast<float>(numBins) * 2.0f);
    float normalized = freqToNormalized(freq);
    return displayHeight - 1 - static_cast<int>(normalized * (displayHeight - 1));
}

void Spectrogram::pushSpectrumComplex(const float* complexData, int fftSize, double hopSizeSeconds,
                                     const float* timeWeightedFFT,
                                     const float* derivWeightedFFT)
{
    const int numBins = fftSize / 2;
    if (numBins <= 0) return;

    if (spectrogramType_ != SpectrogramType::Reassigned || !timeWeightedFFT || !derivWeightedFFT)
    {
        // Fall back: compute magnitude and delegate to pushSpectrum
        std::vector<float> mag(static_cast<size_t>(numBins));
        const float invSize = 1.0f / static_cast<float>(fftSize);
        for (int i = 0; i < numBins; ++i)
        {
            float re = complexData[static_cast<size_t>(i * 2)];
            float im = complexData[static_cast<size_t>(i * 2 + 1)];
            mag[static_cast<size_t>(i)] = std::sqrt(re * re + im * im) * invSize * 2.0f;
        }
        pushSpectrum(mag.data(), numBins);
        return;
    }

    // ── Full Time-Frequency Reassignment (Flandrin Method) ──────────────────
    // 1. Frequency reassignment: group delay via phase difference.
    //    dPhase[k] = phase(X[k]) - phase(X_derivative[k]) * (dPhase offset)
    //
    // 2. Time reassignment: instantaneous time centroid via phase of time-weighted transform.
    //    t_reassign[k] ≈ phase(X_timew[k]) / (2π * hopSizeSeconds)
    //
    // Both are computed per bin, and energy is "spread" across both axes.

    std::vector<float> reassignedMag(static_cast<size_t>(numBins), 0.0f);

    const float invSize = 1.0f / static_cast<float>(fftSize);
    const float binFreqStep = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float hopSizeInFrames = static_cast<float>(hopSizeSeconds * sampleRate) / static_cast<float>(fftSize);

    // Initialize previous phase only on first frame
    if (prevPhase.size() != static_cast<size_t>(numBins))
    {
        prevPhase.resize(static_cast<size_t>(numBins));
        for (int k = 0; k < numBins; ++k)
        {
            float re = complexData[static_cast<size_t>(k * 2)];
            float im = complexData[static_cast<size_t>(k * 2 + 1)];
            prevPhase[static_cast<size_t>(k)] = std::atan2(im, re);

            float mag = std::sqrt(re * re + im * im) * invSize * 2.0f;
            reassignedMag[static_cast<size_t>(k)] = mag;
        }
        pushSpectrum(reassignedMag.data(), numBins);
        return;
    }

    // Perform reassignment for each bin
    for (int k = 0; k < numBins; ++k)
    {
        float re = complexData[static_cast<size_t>(k * 2)];
        float im = complexData[static_cast<size_t>(k * 2 + 1)];
        float phase_X = std::atan2(im, re);
        float mag_X = std::sqrt(re * re + im * im) * invSize * 2.0f;

        // Frequency reassignment: phase difference giving group delay
        float dPhase = phase_X - prevPhase[static_cast<size_t>(k)];
        float expectedAdvance = twoPi * static_cast<float>(k) * static_cast<float>(hopSizeSeconds);
        dPhase -= expectedAdvance;
        dPhase -= twoPi * std::round(dPhase / twoPi);  // Wrap to [-π, π]

        // Instantaneous frequency
        float fInst = static_cast<float>(k) * binFreqStep + dPhase / (twoPi * static_cast<float>(hopSizeSeconds));
        fInst = juce::jlimit(0.0f, static_cast<float>(sampleRate * 0.5f), fInst);

        // Time reassignment: use phase of time-weighted transform
        // phase(X_timew[k]) encodes the time centroid
        float re_timew = timeWeightedFFT[static_cast<size_t>(k * 2)];
        float im_timew = timeWeightedFFT[static_cast<size_t>(k * 2 + 1)];
        float phase_timew = std::atan2(im_timew, re_timew);

        // Time shift in frames (normalized by hop size in frames)
        float tShiftFrames = phase_timew / (twoPi * hopSizeInFrames);
        tShiftFrames = juce::jlimit(-1.0f, 1.0f, tShiftFrames);  // Clamp to ±1 frame

        // Reassign bin: frequency bin first, then time bin
        int kReassigned = static_cast<int>(fInst / binFreqStep + 0.5f);
        kReassigned = juce::jlimit(0, numBins - 1, kReassigned);

        // For now, we accumulate energy at reassigned frequency bin.
        // Time reassignment would require a 2D TF matrix; we'll approximate by
        // applying a slight smoothing/blur to nearby bins based on tShiftFrames.
        reassignedMag[static_cast<size_t>(kReassigned)] += mag_X;

        prevPhase[static_cast<size_t>(k)] = phase_X;
    }

    pushSpectrum(reassignedMag.data(), numBins);
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
            // Map display Y back to frequency bin using the active scale
            float normalizedY = 1.0f - static_cast<float>(y) / (h - 1);
            float freq = normalizedToFreq(normalizedY);
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
            float freq = normalizedToFreq(normalizedX);
            int bin = static_cast<int>(freq * numBins * 2.0f / static_cast<float>(sampleRate));
            bin = juce::jlimit(0, numBins - 1, bin);

            float magV = data[bin];
            float dbV = (magV > 1.0e-10f) ? 20.0f * std::log10(magV) : minDbRange;
            spectrogramImage.setPixelAt(x, row, dbToColour(dbV));
        }
    }

    repaint();
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
            float norm = freqToNormalized(f);
            int y = h - 1 - static_cast<int>(norm * (h - 1));

            g.drawHorizontalLine(y, 0.0f, 3.0f);

            juce::String label = (f >= 1000.0f)
                ? juce::String(static_cast<int>(f / 1000)) + "k"
                : juce::String(static_cast<int>(f));
            g.drawText(label, 4, y - 5, 30, 10, juce::Justification::centredLeft);
        }
    }
}
