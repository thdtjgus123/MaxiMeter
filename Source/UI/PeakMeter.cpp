#include "PeakMeter.h"
#include <cmath>

//==============================================================================
PeakMeter::PeakMeter()
{
    channelStates.resize(2);
    startTimerHz(60);
}

PeakMeter::~PeakMeter()
{
    stopTimer();
}

void PeakMeter::setNumChannels(int numChannels)
{
    channels = juce::jlimit(1, 8, numChannels);
    channelStates.resize(static_cast<size_t>(channels));
    resized();
}

void PeakMeter::setLevel(int channel, float linearLevel)
{
    if (channel < 0 || channel >= channels) return;

    auto& state = channelStates[static_cast<size_t>(channel)];

    float db = (linearLevel > 0.0f)
        ? 20.0f * std::log10(linearLevel)
        : -100.0f;

    // Sample peak: just use the value directly
    // True peak: would need oversampling (simplified here)
    if (peakMode == PeakMode::TruePeak)
    {
        // In real implementation, oversample 4x with FIR and find true peak
        // For now, apply a +0.3dB correction factor as approximation
        db += 0.3f;
    }

    // Update display level (fast attack)
    if (db > state.level)
        state.level = db;

    // Update peak hold
    if (db >= state.peakHold)
    {
        state.peakHold = db;
        state.peakHoldAge = 0.0f;
    }

    // Clip detection: >= 0dBFS (or 3 consecutive samples at max)
    if (linearLevel >= 1.0f)
        state.clipping = true;
}

void PeakMeter::resetPeaks()
{
    for (auto& state : channelStates)
    {
        state.peakHold = -100.0f;
        state.peakHoldAge = 0.0f;
        state.clipping = false;
    }
}

//==============================================================================
void PeakMeter::timerCallback()
{
    float dt = 1.0f / 60.0f;

    for (auto& state : channelStates)
    {
        // Decay the display level
        state.level -= decayRate * dt;
        if (state.level < minDb)
            state.level = minDb;

        // Age peak hold
        if (!infiniteHold)
        {
            state.peakHoldAge += dt;
            if (state.peakHoldAge > peakHoldMs / 1000.0f)
            {
                state.peakHold -= decayRate * dt;
                if (state.peakHold < minDb)
                    state.peakHold = minDb;
            }
        }
    }

    repaint();
}

//==============================================================================
void PeakMeter::paint(juce::Graphics& g)
{
    g.fillAll(getBgColour(juce::Colour(0xFF0D0D1A)));

    auto bounds = getLocalBounds();

    // Scale area
    juce::Rectangle<int> scaleArea;
    if (showScale)
    {
        if (orientation == Orientation::Vertical)
        {
            scaleArea = bounds.removeFromRight(28);
        }
        else
        {
            scaleArea = bounds.removeFromBottom(16);
        }
    }

    // Clip indicator area
    juce::Rectangle<int> clipArea;
    if (showClip && orientation == Orientation::Vertical)
    {
        clipArea = bounds.removeFromTop(12);
    }

    // Draw clip indicators
    if (showClip && !clipArea.isEmpty())
    {
        int chWidth = clipArea.getWidth() / channels;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto area = clipArea.withTrimmedLeft(ch * chWidth).withWidth(chWidth).reduced(1, 0);
            auto& state = channelStates[static_cast<size_t>(ch)];

            if (state.clipping)
                g.setColour(juce::Colour(0xFFFF0000));
            else
                g.setColour(juce::Colour(0xFF330000));

            g.fillRect(area);
        }
    }

    // Draw meters per channel
    if (orientation == Orientation::Vertical)
    {
        int chWidth = bounds.getWidth() / channels;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto area = bounds.withTrimmedLeft(ch * chWidth).withWidth(chWidth).reduced(1, 1);
            drawVerticalMeter(g, area, ch);
        }
    }
    else
    {
        int chHeight = bounds.getHeight() / channels;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto area = bounds.withTrimmedTop(ch * chHeight).withHeight(chHeight).reduced(1, 1);
            drawHorizontalMeter(g, area, ch);
        }
    }

    // Draw scale
    if (showScale && !scaleArea.isEmpty())
        drawScale(g, scaleArea);
}

void PeakMeter::resized()
{
    // Nothing specific needed, paint handles layout
}

//==============================================================================
void PeakMeter::drawVerticalMeter(juce::Graphics& g, juce::Rectangle<int> area, int ch)
{
    auto& state = channelStates[static_cast<size_t>(ch)];

    // Background
    g.setColour(getBgColour(juce::Colour(0xFF111122)));
    g.fillRect(area);

    // Segmented bars
    int totalSegs = 48;
    float segHeight = static_cast<float>(area.getHeight()) / totalSegs;
    float levelNorm = dbToNormalized(state.level);
    int litSegs = static_cast<int>(levelNorm * totalSegs);

    for (int i = 0; i < totalSegs; ++i)
    {
        float segDb = minDb + (maxDb - minDb) * static_cast<float>(i) / totalSegs;
        float y = area.getBottom() - (i + 1) * segHeight;

        if (i < litSegs)
            g.setColour(dbToColour(segDb));
        else
            g.setColour(dbToColour(segDb).withAlpha(0.06f));

        g.fillRect(static_cast<float>(area.getX()), y,
                    static_cast<float>(area.getWidth()), segHeight - 0.5f);
    }

    // Peak hold line
    float peakNorm = dbToNormalized(state.peakHold);
    if (peakNorm > 0.0f)
    {
        float peakY = area.getBottom() - peakNorm * area.getHeight();

        juce::Colour peakCol = (state.peakHold >= 0.0f)
            ? juce::Colour(0xFFFF4444)
            : juce::Colour(0xFFFFFFFF);

        g.setColour(peakCol);
        g.fillRect(static_cast<float>(area.getX()), peakY,
                    static_cast<float>(area.getWidth()), 2.0f);
    }

    // Border
    g.setColour(tintSecondary(juce::Colour(0xFF333344)));
    g.drawRect(area, 1);
}

void PeakMeter::drawHorizontalMeter(juce::Graphics& g, juce::Rectangle<int> area, int ch)
{
    auto& state = channelStates[static_cast<size_t>(ch)];

    g.setColour(getBgColour(juce::Colour(0xFF111122)));
    g.fillRect(area);

    int totalSegs = 60;
    float segWidth = static_cast<float>(area.getWidth()) / totalSegs;
    float levelNorm = dbToNormalized(state.level);
    int litSegs = static_cast<int>(levelNorm * totalSegs);

    for (int i = 0; i < totalSegs; ++i)
    {
        float segDb = minDb + (maxDb - minDb) * static_cast<float>(i) / totalSegs;
        float x = area.getX() + i * segWidth;

        if (i < litSegs)
            g.setColour(dbToColour(segDb));
        else
            g.setColour(dbToColour(segDb).withAlpha(0.06f));

        g.fillRect(x, static_cast<float>(area.getY()),
                    segWidth - 0.5f, static_cast<float>(area.getHeight()));
    }

    // Peak hold
    float peakNorm = dbToNormalized(state.peakHold);
    if (peakNorm > 0.0f)
    {
        float peakX = area.getX() + peakNorm * area.getWidth();
        juce::Colour peakCol = (state.peakHold >= 0.0f)
            ? juce::Colour(0xFFFF4444)
            : juce::Colour(0xFFFFFFFF);
        g.setColour(peakCol);
        g.fillRect(peakX, static_cast<float>(area.getY()),
                    2.0f, static_cast<float>(area.getHeight()));
    }

    g.setColour(tintSecondary(juce::Colour(0xFF333344)));
    g.drawRect(area, 1);
}

//==============================================================================
void PeakMeter::drawScale(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(meterFont(9.0f));
    g.setColour(juce::Colours::grey);

    const float scaleMarks[] = { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f,
                                 -18.0f, -24.0f, -30.0f, -36.0f,
                                 -42.0f, -48.0f, -54.0f, -60.0f };

    for (float db : scaleMarks)
    {
        if (db < minDb || db > maxDb) continue;

        float norm = dbToNormalized(db);

        if (orientation == Orientation::Vertical)
        {
            int y = area.getBottom() - static_cast<int>(norm * area.getHeight());
            juce::String label = (db == 0.0f) ? "0" : juce::String(static_cast<int>(db));
            g.drawText(label, area.getX() + 2, y - 5, area.getWidth() - 4, 10,
                        juce::Justification::centredLeft);
        }
        else
        {
            int x = area.getX() + static_cast<int>(norm * area.getWidth());
            juce::String label = (db == 0.0f) ? "0" : juce::String(static_cast<int>(db));
            g.drawText(label, x - 10, area.getY(), 20, area.getHeight(),
                        juce::Justification::centred);
        }
    }
}

//==============================================================================
juce::Colour PeakMeter::dbToColour(float db) const
{
    juce::Colour result;
    if (db < -18.0f)
        result = juce::Colour(0xFF00CC77);
    else if (db < -6.0f)
    {
        float t = (db + 18.0f) / 12.0f;
        result = juce::Colour(0xFF00CC77).interpolatedWith(juce::Colour(0xFFFFDD00), t);
    }
    else if (db < -3.0f)
    {
        float t = (db + 6.0f) / 3.0f;
        result = juce::Colour(0xFFFFDD00).interpolatedWith(juce::Colour(0xFFFF8800), t);
    }
    else
        result = juce::Colour(0xFFFF2200);
    return tintFg(result);
}

float PeakMeter::dbToNormalized(float db) const
{
    return juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
}
