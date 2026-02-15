#include "LoudnessMeter.h"
#include <cmath>

//==============================================================================
LoudnessMeter::LoudnessMeter()
{
    shortTermHistory.resize(kHistorySize, -100.0f);
}

void LoudnessMeter::resized()
{
    // Nothing specific
}

float LoudnessMeter::lufsToNormalized(float lufs) const
{
    return juce::jlimit(0.0f, 1.0f, (lufs - minRange) / (maxRange - minRange));
}

juce::Colour LoudnessMeter::lufsToColour(float lufs) const
{
    float diff = lufs - targetLUFS;
    juce::Colour result;
    if (diff > 3.0f)
        result = juce::Colour(0xFFFF2200);
    else if (diff > 1.0f)
        result = juce::Colour(0xFFFF8800);
    else if (diff > -1.0f)
        result = juce::Colour(0xFF00DD88);
    else if (diff > -3.0f)
        result = juce::Colour(0xFF44BBFF);
    else
        result = juce::Colour(0xFF6666AA);
    return tintFg(result);
}

//==============================================================================
void LoudnessMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(getBgColour(juce::Colour(0xFF0D0D1A)));

    // Update history (once per repaint from timer ≈ 60fps, we only write ~1/60th)
    // For proper timing, the caller should only repaint at the history rate
    // Here we push shortTerm to history every ~60 frames
    static int historyCounter = 0;
    if (++historyCounter >= 60)  // ~1 second
    {
        historyCounter = 0;
        shortTermHistory[static_cast<size_t>(historyWritePos)] = shortTerm;
        historyWritePos = (historyWritePos + 1) % kHistorySize;
    }

    // Layout
    auto infoArea = bounds.removeFromBottom(70);
    auto histArea = showHistory ? bounds.removeFromBottom(80) : juce::Rectangle<int>();
    auto meterArea = bounds;

    // Split meter area into 3 bars: Momentary, Short-term, Integrated
    int barWidth = meterArea.getWidth() / 3;
    auto momArea = meterArea.removeFromLeft(barWidth);
    auto stArea  = meterArea.removeFromLeft(barWidth);
    auto intArea = meterArea;

    drawMeterBar(g, momArea.reduced(4, 2), momentary, "M", false);
    drawMeterBar(g, stArea.reduced(4, 2), shortTerm, "S", true);
    drawMeterBar(g, intArea.reduced(4, 2), integrated, "I", true);

    // History graph
    if (showHistory && !histArea.isEmpty())
        drawHistoryGraph(g, histArea.reduced(4, 2));

    // Info panel
    drawInfoPanel(g, infoArea.reduced(4, 2));
}

//==============================================================================
void LoudnessMeter::drawMeterBar(juce::Graphics& g, juce::Rectangle<int> area,
                                  float value, const juce::String& label, bool showTarget)
{
    // Label
    auto labelArea = area.removeFromBottom(16);
    g.setFont(meterFont(10.0f));
    g.setColour(juce::Colours::grey);

    juce::String fullLabel = label + ": ";
    if (value > -90.0f)
        fullLabel += juce::String(value, 1) + " LUFS";
    else
        fullLabel += "---";
    g.drawText(fullLabel, labelArea, juce::Justification::centred);

    // Bar background
    g.setColour(getBgColour(juce::Colour(0xFF111122)));
    g.fillRect(area);

    // Segmented bar (48 segments)
    int totalSegs = 48;
    float segH = static_cast<float>(area.getHeight()) / totalSegs;
    float norm = lufsToNormalized(value);
    int litSegs = static_cast<int>(norm * totalSegs);

    for (int i = 0; i < totalSegs; ++i)
    {
        float segLUFS = minRange + (maxRange - minRange) * static_cast<float>(i) / totalSegs;
        float y = area.getBottom() - (i + 1) * segH;

        if (i < litSegs)
            g.setColour(lufsToColour(segLUFS));
        else
            g.setColour(lufsToColour(segLUFS).withAlpha(0.06f));

        g.fillRect(static_cast<float>(area.getX()), y,
                    static_cast<float>(area.getWidth()), segH - 0.5f);
    }

    // Target line
    if (showTarget)
    {
        float targetNorm = lufsToNormalized(targetLUFS);
        float targetY = area.getBottom() - targetNorm * area.getHeight();
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillRect(static_cast<float>(area.getX()), targetY - 0.5f,
                    static_cast<float>(area.getWidth()), 1.5f);
    }

    // Scale ticks on the left
    g.setFont(meterFont(8.0f));
    g.setColour(juce::Colours::grey.withAlpha(0.5f));
    for (int db = static_cast<int>(maxRange); db >= static_cast<int>(minRange); db -= 6)
    {
        float n = lufsToNormalized(static_cast<float>(db));
        float y = area.getBottom() - n * area.getHeight();
        g.drawHorizontalLine(static_cast<int>(y),
                              static_cast<float>(area.getX()),
                              static_cast<float>(area.getX() + 3));
    }

    g.setColour(tintSecondary(juce::Colour(0xFF333344)));
    g.drawRect(area, 1);
}

void LoudnessMeter::drawHistoryGraph(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(getBgColour(juce::Colour(0xFF111122)));
    g.fillRect(area);

    // Draw target line
    float targetN = lufsToNormalized(targetLUFS);
    float targetY = area.getBottom() - targetN * area.getHeight();
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawHorizontalLine(static_cast<int>(targetY),
                          static_cast<float>(area.getX()),
                          static_cast<float>(area.getRight()));

    // Draw history
    juce::Path path;
    bool started = false;
    float stepX = static_cast<float>(area.getWidth()) / kHistorySize;

    for (int i = 0; i < kHistorySize; ++i)
    {
        int idx = (historyWritePos + i) % kHistorySize;
        float val = shortTermHistory[static_cast<size_t>(idx)];

        if (val < -90.0f) continue;

        float x = area.getX() + i * stepX;
        float n = lufsToNormalized(val);
        float y = area.getBottom() - n * area.getHeight();

        if (!started) { path.startNewSubPath(x, y); started = true; }
        else path.lineTo(x, y);
    }

    g.setColour(tintFg(juce::Colour(0xFF44BBFF)).withAlpha(0.7f));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    g.setColour(tintSecondary(juce::Colour(0xFF333344)));
    g.drawRect(area, 1);

    g.setFont(meterFont(8.0f));
    g.setColour(juce::Colours::grey);
    g.drawText("Short-term history", area.removeFromTop(12),
               juce::Justification::centredLeft);
}

void LoudnessMeter::drawInfoPanel(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(getBgColour(juce::Colour(0xFF111122)));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);

    auto left = area.removeFromLeft(area.getWidth() / 2);
    auto right = area;

    g.setFont(meterFont(10.0f));

    // Left column
    auto drawInfo = [&](juce::Rectangle<int>& col, const juce::String& label,
                        const juce::String& value, juce::Colour col2)
    {
        auto row = col.removeFromTop(16);
        g.setColour(juce::Colours::grey);
        g.drawText(label, row.removeFromLeft(row.getWidth() / 2),
                   juce::Justification::centredLeft);
        g.setColour(col2);
        g.drawText(value, row, juce::Justification::centredLeft);
    };

    juce::String intStr = (integrated > -90.0f)
        ? juce::String(integrated, 1) + " LUFS" : "---";
    drawInfo(left, "Integrated:", intStr, lufsToColour(integrated));

    juce::String lraStr = juce::String(lra, 1) + " LU";
    drawInfo(left, "LRA:", lraStr, juce::Colours::lightgrey);

    juce::String targetStr = juce::String(targetLUFS, 0) + " LUFS";
    drawInfo(left, "Target:", targetStr, juce::Colours::white);

    // Right column
    float tpL = (truePeakL > 0.0f) ? 20.0f * std::log10(truePeakL) : -100.0f;
    float tpR = (truePeakR > 0.0f) ? 20.0f * std::log10(truePeakR) : -100.0f;
    juce::Colour tpColL = (tpL > -1.0f) ? juce::Colour(0xFFFF4444) : juce::Colours::lightgrey;
    juce::Colour tpColR = (tpR > -1.0f) ? juce::Colour(0xFFFF4444) : juce::Colours::lightgrey;

    juce::String tpLStr = (tpL > -90.0f) ? juce::String(tpL, 1) + " dBTP" : "---";
    juce::String tpRStr = (tpR > -90.0f) ? juce::String(tpR, 1) + " dBTP" : "---";

    drawInfo(right, "TP Left:", tpLStr, tpColL);
    drawInfo(right, "TP Right:", tpRStr, tpColR);

    float diffFromTarget = integrated - targetLUFS;
    juce::String diffStr = (integrated > -90.0f)
        ? ((diffFromTarget >= 0 ? "+" : "") + juce::String(diffFromTarget, 1) + " LU")
        : "---";
    drawInfo(right, "Offset:", diffStr, lufsToColour(integrated));
}
