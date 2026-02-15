#include "StatusBar.h"
#include "ThemeManager.h"

//==============================================================================
StatusBar::StatusBar(AudioEngine& eng, LevelAnalyzer& lvl)
    : engine(eng), levels(lvl)
{
    engine.addListener(this);
    startTimerHz(15);
}

StatusBar::~StatusBar()
{
    engine.removeListener(this);
    stopTimer();
}

//==============================================================================
void StatusBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto& pal = ThemeManager::getInstance().getPalette();

    // Background
    g.fillAll(pal.statusBarBg);

    // Top separator
    g.setColour(pal.border);
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    g.setFont(12.0f);
    auto area = bounds.reduced(6, 0);

    // Left: playback state
    g.setColour(engine.isPlaying() ? juce::Colours::limegreen : pal.dimText);
    g.drawText(playbackState, area.removeFromLeft(80), juce::Justification::centredLeft);

    // File info
    g.setColour(pal.bodyText.withAlpha(0.7f));
    g.drawText(fileInfo, area.removeFromLeft(400), juce::Justification::centredLeft);

    // Right: current levels
    float dbL = LevelAnalyzer::toDecibels(levels.getRMSLeft());
    float dbR = LevelAnalyzer::toDecibels(levels.getRMSRight());

    juce::String levelStr = "L: " + juce::String(dbL, 1) + " dB  R: " + juce::String(dbR, 1) + " dB";

    // Color based on level
    if (levels.hasClippedLeft() || levels.hasClippedRight())
        g.setColour(juce::Colours::red);
    else if (dbL > -6.0f || dbR > -6.0f)
        g.setColour(juce::Colours::yellow);
    else
        g.setColour(juce::Colours::limegreen);

    g.drawText(levelStr, area.removeFromRight(250), juce::Justification::centredRight);
}

void StatusBar::resized()
{
    // Layout handled in paint()
}

//==============================================================================
void StatusBar::timerCallback()
{
    repaint();
}

void StatusBar::fileLoaded(const juce::String& fileName, double lengthSeconds)
{
    int mins = static_cast<int>(lengthSeconds) / 60;
    int secs = static_cast<int>(lengthSeconds) % 60;

    fileInfo = fileName + "  |  "
        + juce::String(engine.getFileSampleRate() / 1000.0, 1) + " kHz  |  "
        + juce::String::formatted("%d:%02d", mins, secs);
}

void StatusBar::transportStateChanged(bool isPlaying)
{
    playbackState = isPlaying ? "Playing" : "Stopped";
}
