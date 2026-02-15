#include "CorrelationMeter.h"
#include <cmath>

//==============================================================================
CorrelationMeter::CorrelationMeter()
{
}

void CorrelationMeter::setCorrelation(float value)
{
    targetCorrelation = juce::jlimit(-1.0f, 1.0f, value);

    // Smooth toward target
    float frameDuration = 1000.0f / 60.0f;
    smoothCoeff = 1.0f - std::exp(-frameDuration / integrationMs);
    displayCorrelation += (targetCorrelation - displayCorrelation) * smoothCoeff;
    repaint();
}

//==============================================================================
void CorrelationMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(getBgColour(juce::Colour(0xFF0D0D1A)));

    juce::Rectangle<int> barArea;
    juce::Rectangle<int> labelArea;

    if (isHorizontal)
    {
        labelArea = bounds.removeFromTop(14);
        barArea = bounds.reduced(4, 2);
    }
    else
    {
        labelArea = bounds.removeFromBottom(14);
        barArea = bounds.reduced(2, 4);
    }

    // Label
    g.setFont(meterFont(10.0f));
    g.setColour(juce::Colours::grey);
    g.drawText("Correlation", labelArea, juce::Justification::centred);

    // Bar background
    g.setColour(juce::Colour(0xFF111122));
    g.fillRect(barArea);

    if (isHorizontal)
    {
        float cx = static_cast<float>(barArea.getCentreX());
        float halfW = static_cast<float>(barArea.getWidth()) * 0.5f;

        // Center line
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawVerticalLine(static_cast<int>(cx), static_cast<float>(barArea.getY()),
                            static_cast<float>(barArea.getBottom()));

        // Scale ticks at -1, -0.5, 0, +0.5, +1
        g.setFont(meterFont(8.0f));
        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        const float ticks[] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
        for (float t : ticks)
        {
            float x = cx + t * halfW;
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(barArea.getBottom() - 3),
                                static_cast<float>(barArea.getBottom()));
        }

        // Fill bar from center
        float barPos = cx + displayCorrelation * halfW;
        float x1 = std::min(cx, barPos);
        float x2 = std::max(cx, barPos);

        juce::Colour barCol = getCorrelationColour(displayCorrelation);
        g.setColour(barCol);
        g.fillRect(x1, static_cast<float>(barArea.getY()),
                   x2 - x1, static_cast<float>(barArea.getHeight()));

        // Indicator line
        g.setColour(juce::Colours::white);
        g.fillRect(barPos - 1.0f, static_cast<float>(barArea.getY()),
                   2.0f, static_cast<float>(barArea.getHeight()));

        // Numeric value
        if (showNumeric)
        {
            g.setFont(meterFont(11.0f));
            g.setColour(barCol);
            juce::String val = juce::String(displayCorrelation, 2);
            g.drawText(val, barArea.withTrimmedLeft(barArea.getWidth() - 40),
                       juce::Justification::centredRight);
        }
    }
    else
    {
        float cy = static_cast<float>(barArea.getCentreY());
        float halfH = static_cast<float>(barArea.getHeight()) * 0.5f;

        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawHorizontalLine(static_cast<int>(cy), static_cast<float>(barArea.getX()),
                              static_cast<float>(barArea.getRight()));

        float barPos = cy - displayCorrelation * halfH;
        float y1 = std::min(cy, barPos);
        float y2 = std::max(cy, barPos);

        juce::Colour barCol = getCorrelationColour(displayCorrelation);
        g.setColour(barCol);
        g.fillRect(static_cast<float>(barArea.getX()), y1,
                   static_cast<float>(barArea.getWidth()), y2 - y1);

        g.setColour(juce::Colours::white);
        g.fillRect(static_cast<float>(barArea.getX()), barPos - 1.0f,
                   static_cast<float>(barArea.getWidth()), 2.0f);

        if (showNumeric)
        {
            g.setFont(meterFont(11.0f));
            g.setColour(barCol);
            g.drawText(juce::String(displayCorrelation, 2),
                       barArea.removeFromTop(16), juce::Justification::centred);
        }
    }

    // Border
    g.setColour(juce::Colour(0xFF333344));
    g.drawRect(barArea, 1);
}

//==============================================================================
juce::Colour CorrelationMeter::getCorrelationColour(float value) const
{
    juce::Colour result;
    if (value > 0.5f)
        result = juce::Colour(0xFF00DD88);
    else if (value > 0.0f)
        result = juce::Colour(0xFF88DD44);
    else if (value > -0.5f)
        result = juce::Colour(0xFFFFDD00);
    else
        result = juce::Colour(0xFFFF4444);
    return tintFg(result);
}
