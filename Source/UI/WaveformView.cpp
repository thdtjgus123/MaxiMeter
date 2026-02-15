#include "WaveformView.h"
#include "ThemeManager.h"

//==============================================================================
WaveformView::WaveformView(AudioEngine& eng) : engine(eng)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);
    engine.addListener(this);

    startTimerHz(30);
}

WaveformView::~WaveformView()
{
    engine.removeListener(this);
    thumbnail.removeChangeListener(this);
    stopTimer();
}

//==============================================================================
void WaveformView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto& pal = ThemeManager::getInstance().getPalette();

    // Background
    g.fillAll(hasCustomBg() ? meterBg_ : pal.transportBg);

    if (thumbnail.getTotalLength() > 0.0)
    {
        drawWaveform(g, bounds);
        drawCursor(g, bounds);
    }
    else
    {
        // Empty state
        g.setColour(pal.dimText);
        g.setFont(14.0f);
        g.drawText("Drag & drop an audio file or use File > Open",
                    bounds, juce::Justification::centred);
    }
}

void WaveformView::resized()
{
    // Nothing special needed
}

//==============================================================================
void WaveformView::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    g.setColour(pal.accent.brighter(0.2f));

    thumbnail.drawChannels(g,
                            bounds.reduced(2),
                            0.0,
                            thumbnail.getTotalLength(),
                            1.0f);
}

void WaveformView::drawCursor(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (totalLength <= 0.0)
        return;

    double currentPos = engine.getCurrentPosition();
    float relativePos = static_cast<float>(currentPos / totalLength);
    float xPos = bounds.getX() + relativePos * bounds.getWidth();

    // Playback cursor
    g.setColour(juce::Colours::white);
    g.drawVerticalLine(static_cast<int>(xPos), static_cast<float>(bounds.getY()),
                        static_cast<float>(bounds.getBottom()));

    // Slight glow effect
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawVerticalLine(static_cast<int>(xPos) - 1, static_cast<float>(bounds.getY()),
                        static_cast<float>(bounds.getBottom()));
    g.drawVerticalLine(static_cast<int>(xPos) + 1, static_cast<float>(bounds.getY()),
                        static_cast<float>(bounds.getBottom()));
}

//==============================================================================
void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    seekToMousePosition(e);
}

void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    seekToMousePosition(e);
}

void WaveformView::seekToMousePosition(const juce::MouseEvent& e)
{
    if (totalLength <= 0.0)
        return;

    float relativeX = static_cast<float>(e.x) / static_cast<float>(getWidth());
    relativeX = juce::jlimit(0.0f, 1.0f, relativeX);
    double seekPos = relativeX * totalLength;
    engine.setPosition(seekPos);
}

//==============================================================================
void WaveformView::loadThumbnail(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        thumbnail.setSource(new juce::FileInputSource(file));
        totalLength = reader->lengthInSamples / reader->sampleRate;
    }
}

void WaveformView::clearThumbnail()
{
    thumbnail.setSource(nullptr);
    totalLength = 0.0;
    repaint();
}

//==============================================================================
void WaveformView::timerCallback()
{
    if (engine.isPlaying())
        repaint();
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    repaint();
}

void WaveformView::fileLoaded(const juce::String& /*fileName*/, double lengthSeconds)
{
    totalLength = lengthSeconds;

    // Load the waveform thumbnail from the file that AudioEngine just loaded
    auto loadedFile = engine.getLoadedFile();
    if (loadedFile.existsAsFile())
        loadThumbnail(loadedFile);
}
