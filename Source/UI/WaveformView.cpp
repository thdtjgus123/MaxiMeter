#include "WaveformView.h"
#include "ThemeManager.h"

//==============================================================================
WaveformView::WaveformView(AudioEngine& eng) : engine(eng)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);
    engine.addListener(this);

    // ── Volume overlay slider ───────────────────────────────────
    volumeSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider_.setRange(0.0, 1.5, 0.01);
    volumeSlider_.setValue(engine.getGain(), juce::dontSendNotification);
    volumeSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider_.setTooltip("Volume  (0 – 150%)");
    volumeSlider_.onValueChange = [this]
    {
        engine.setGain((float)volumeSlider_.getValue());
        volumeLabel_.setText(juce::String(juce::roundToInt(volumeSlider_.getValue() * 100)) + "%",
                             juce::dontSendNotification);
    };
    addAndMakeVisible(volumeSlider_);

    volumeLabel_.setText(juce::String(juce::roundToInt(engine.getGain() * 100)) + "%",
                         juce::dontSendNotification);
    volumeLabel_.setFont(juce::Font(10.0f));
    volumeLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(volumeLabel_);

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

    // ── Volume overlay background + speaker icon ─────────────
    static constexpr int kMargin  = 6;
    static constexpr int kIconW   = 22;
    static constexpr int kSliderW = 90;
    static constexpr int kLabelW  = 36;
    static constexpr int kH       = 22;
    static constexpr int kTotalW  = kIconW + kSliderW + kLabelW;

    auto panelBounds = juce::Rectangle<int>(
        getWidth()  - kTotalW - kMargin,
        getHeight() - kH      - kMargin,
        kTotalW, kH);

    // Panel background
    g.setColour(juce::Colour(0xcc1a1a2a));
    g.fillRoundedRectangle(panelBounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0x44ffffff));
    g.drawRoundedRectangle(panelBounds.toFloat(), 4.0f, 0.5f);

    // Speaker icon
    auto iconR = panelBounds.removeFromLeft(kIconW).toFloat().reduced(3.0f, 4.0f);
    drawVolumeIcon(g, iconR);
}

void WaveformView::resized()
{
    layoutVolumeOverlay();
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

    double currentPos = (offlinePos_ >= 0.0) ? offlinePos_ : engine.getCurrentPosition();
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

//==============================================================================
void WaveformView::seekToMousePosition(const juce::MouseEvent& e)
{
    if (totalLength <= 0.0)
        return;

    // Don't seek if the click is inside the volume overlay
    static constexpr int kMargin  = 6;
    static constexpr int kTotalW  = 22 + 90 + 36;
    static constexpr int kH       = 22;
    juce::Rectangle<int> overlayBounds(
        getWidth()  - kTotalW - kMargin,
        getHeight() - kH      - kMargin,
        kTotalW, kH);
    if (overlayBounds.contains(e.getPosition()))
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

    // Sync slider to current engine gain (in case it changed)
    volumeSlider_.setValue(engine.getGain(), juce::dontSendNotification);
    volumeLabel_.setText(juce::String(juce::roundToInt(engine.getGain() * 100)) + "%",
                         juce::dontSendNotification);

    // Load the waveform thumbnail from the file that AudioEngine just loaded
    auto loadedFile = engine.getLoadedFile();
    if (loadedFile.existsAsFile())
        loadThumbnail(loadedFile);
}

//==============================================================================
void WaveformView::layoutVolumeOverlay()
{
    static constexpr int kMargin  = 6;
    static constexpr int kIconW   = 22;
    static constexpr int kSliderW = 90;
    static constexpr int kLabelW  = 36;
    static constexpr int kH       = 22;
    static constexpr int kTotalW  = kIconW + kSliderW + kLabelW;

    int panelX = getWidth()  - kTotalW - kMargin;
    int panelY = getHeight() - kH      - kMargin;

    // Reserve left kIconW for the speaker icon drawn in paint()
    volumeSlider_.setBounds(panelX + kIconW, panelY, kSliderW, kH);
    volumeLabel_ .setBounds(panelX + kIconW + kSliderW, panelY, kLabelW, kH);

    // Colour the label text to match panel
    volumeLabel_.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));

    // Style the slider thumb/track to blend with the dark panel
    volumeSlider_.setColour(juce::Slider::thumbColourId,      juce::Colour(0xffaabbff));
    volumeSlider_.setColour(juce::Slider::trackColourId,      juce::Colour(0x884466cc));
    volumeSlider_.setColour(juce::Slider::backgroundColourId, juce::Colour(0x33ffffff));
}

void WaveformView::drawVolumeIcon(juce::Graphics& g, juce::Rectangle<float> r)
{
    // Simple speaker icon: body + two arc waves
    g.setColour(juce::Colours::white.withAlpha(0.75f));

    float cx  = r.getX();
    float cy  = r.getCentreY();
    float h   = r.getHeight();
    float bodyW = h * 0.38f;
    float bodyH = h * 0.55f;

    // Speaker body (trapezoid)
    juce::Path body;
    body.addRectangle(cx, cy - bodyH * 0.5f, bodyW, bodyH);
    body.addTriangle(cx + bodyW, cy - bodyH * 0.5f,
                     cx + bodyW,  cy + bodyH * 0.5f,
                     cx + bodyW + h * 0.3f, cy + h * 0.5f);
    body.addTriangle(cx + bodyW, cy - bodyH * 0.5f,
                     cx + bodyW + h * 0.3f, cy - h * 0.5f,
                     cx + bodyW + h * 0.3f, cy + h * 0.5f);
    g.fillPath(body);

    // Volume arc waves (only if gain > small threshold)
    float gain = (float)volumeSlider_.getValue();
    if (gain > 0.05f)
    {
        float arcX  = cx + bodyW + h * 0.35f;
        float arcR1 = h * 0.28f;
        float arcR2 = h * 0.46f;
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawEllipse(arcX - arcR1, cy - arcR1, arcR1 * 2, arcR1 * 2, 0.9f);
        if (gain > 0.35f)
            g.drawEllipse(arcX - arcR2, cy - arcR2, arcR2 * 2, arcR2 * 2, 0.9f);
    }
}
