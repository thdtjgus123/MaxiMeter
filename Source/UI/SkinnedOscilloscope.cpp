#include "SkinnedOscilloscope.h"

//==============================================================================
SkinnedOscilloscope::SkinnedOscilloscope()
{
    displayBuffer.resize(static_cast<size_t>(displaySamples), 0.0f);
}

void SkinnedOscilloscope::resized()
{
    // Adjust buffer to match component width or keep at 576
    int w = getWidth();
    if (w > 0 && w != displaySamples)
    {
        displaySamples = w;
        displayBuffer.resize(static_cast<size_t>(displaySamples), 0.0f);
        writePos = 0;
    }
}

//==============================================================================
void SkinnedOscilloscope::pushSamples(const float* data, int numSamples)
{
    if (data == nullptr || numSamples <= 0) return;

    // Resample input to fit our display buffer size (simple decimation/interpolation)
    if (numSamples == displaySamples)
    {
        for (int i = 0; i < displaySamples; ++i)
            displayBuffer[static_cast<size_t>(i)] = data[i];
    }
    else
    {
        float ratio = static_cast<float>(numSamples) / static_cast<float>(displaySamples);
        for (int i = 0; i < displaySamples; ++i)
        {
            float fIdx = i * ratio;
            int idx = juce::jlimit(0, numSamples - 1, static_cast<int>(fIdx));
            displayBuffer[static_cast<size_t>(i)] = data[idx];
        }
    }

    repaint();
}

//==============================================================================
void SkinnedOscilloscope::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    auto bg = (currentSkin != nullptr) ? currentSkin->visColors.colors[0] : bgColour;
    g.fillAll(getBgColour(bg));

    switch (drawStyle)
    {
        case 0:  drawLineWaveform(g, bounds);   break;
        case 1:  drawDotWaveform(g, bounds);    break;
        case 2:  drawFilledWaveform(g, bounds); break;
    }
}

//==============================================================================
void SkinnedOscilloscope::drawLineWaveform(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (displayBuffer.empty()) return;

    float midY = bounds.getCentreY();
    float ampH = bounds.getHeight() * 0.45f;
    float stepX = bounds.getWidth() / static_cast<float>(displaySamples - 1);

    // Determine color
    juce::Colour color = waveColour;
    if (currentSkin != nullptr)
        color = currentSkin->visColors.colors[18];  // viscolor index 18 = oscilloscope line
    color = tintFg(color);

    juce::Path path;
    bool started = false;

    for (int i = 0; i < displaySamples; ++i)
    {
        float x = bounds.getX() + i * stepX;
        float y = midY - displayBuffer[static_cast<size_t>(i)] * ampH;
        y = juce::jlimit(bounds.getY(), bounds.getBottom(), y);

        if (!started)
        {
            path.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    g.setColour(color);
    g.strokePath(path, juce::PathStrokeType(lineWidth));
}

void SkinnedOscilloscope::drawDotWaveform(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (displayBuffer.empty()) return;

    float midY = bounds.getCentreY();
    float ampH = bounds.getHeight() * 0.45f;
    float stepX = bounds.getWidth() / static_cast<float>(displaySamples - 1);

    juce::Colour color = waveColour;
    if (currentSkin != nullptr)
        color = currentSkin->visColors.colors[18];
    color = tintFg(color);

    g.setColour(color);

    for (int i = 0; i < displaySamples; ++i)
    {
        float x = bounds.getX() + i * stepX;
        float y = midY - displayBuffer[static_cast<size_t>(i)] * ampH;
        y = juce::jlimit(bounds.getY(), bounds.getBottom(), y);

        g.fillEllipse(x - 1.0f, y - 1.0f, 2.0f, 2.0f);
    }
}

void SkinnedOscilloscope::drawFilledWaveform(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (displayBuffer.empty()) return;

    float midY = bounds.getCentreY();
    float ampH = bounds.getHeight() * 0.45f;
    float stepX = bounds.getWidth() / static_cast<float>(displaySamples - 1);

    juce::Colour color = waveColour;
    if (currentSkin != nullptr)
        color = currentSkin->visColors.colors[18];
    color = tintFg(color);

    juce::Path path;
    path.startNewSubPath(bounds.getX(), midY);

    for (int i = 0; i < displaySamples; ++i)
    {
        float x = bounds.getX() + i * stepX;
        float y = midY - displayBuffer[static_cast<size_t>(i)] * ampH;
        y = juce::jlimit(bounds.getY(), bounds.getBottom(), y);
        path.lineTo(x, y);
    }

    path.lineTo(bounds.getRight(), midY);
    path.closeSubPath();

    g.setColour(color.withAlpha(0.5f));
    g.fillPath(path);

    // Outline
    g.setColour(color);
    juce::Path outline;
    bool started = false;
    for (int i = 0; i < displaySamples; ++i)
    {
        float x = bounds.getX() + i * stepX;
        float y = midY - displayBuffer[static_cast<size_t>(i)] * ampH;
        y = juce::jlimit(bounds.getY(), bounds.getBottom(), y);
        if (!started) { outline.startNewSubPath(x, y); started = true; }
        else outline.lineTo(x, y);
    }
    g.strokePath(outline, juce::PathStrokeType(lineWidth));
}
