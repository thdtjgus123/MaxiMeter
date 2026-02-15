#include "TransportBar.h"

//==============================================================================
TransportBar::TransportBar(AudioEngine& eng) : engine(eng)
{
    ThemeManager::getInstance().addListener(this);

    // Buttons
    addAndMakeVisible(openButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(pauseButton);
    addAndMakeVisible(stopButton);

    openButton.addListener(this);
    playButton.addListener(this);
    pauseButton.addListener(this);
    stopButton.addListener(this);

    // Position slider
    positionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider.setRange(0.0, 1.0);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    positionSlider.addListener(this);
    addAndMakeVisible(positionSlider);

    // Time label
    timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
    timeLabel.setJustificationType(juce::Justification::centredLeft);
    timeLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    addAndMakeVisible(timeLabel);

    // File name label
    fileNameLabel.setText("No file loaded", juce::dontSendNotification);
    fileNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fileNameLabel);

    // Listen to engine
    engine.addListener(this);

    // Apply initial theme colours
    applyThemeColours();

    // Timer for position updates (30 fps)
    startTimerHz(30);
}

TransportBar::~TransportBar()
{
    ThemeManager::getInstance().removeListener(this);
    engine.removeListener(this);
    stopTimer();
}

//==============================================================================
void TransportBar::applyThemeColours()
{
    auto& pal = ThemeManager::getInstance().getPalette();

    timeLabel.setColour(juce::Label::textColourId, pal.bodyText);
    fileNameLabel.setColour(juce::Label::textColourId, pal.dimText);

    positionSlider.setColour(juce::Slider::backgroundColourId, pal.border.withAlpha(0.3f));
    positionSlider.setColour(juce::Slider::trackColourId, pal.accent);
    positionSlider.setColour(juce::Slider::thumbColourId, pal.accent.brighter(0.3f));
}

//==============================================================================
void TransportBar::paint(juce::Graphics& g)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    g.fillAll(pal.transportBg);

    // Bottom separator line
    g.setColour(pal.border);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced(4);

    // Left: transport buttons
    auto btnArea = area.removeFromLeft(200);
    openButton.setBounds(btnArea.removeFromLeft(44).reduced(2));
    playButton.setBounds(btnArea.removeFromLeft(40).reduced(2));
    pauseButton.setBounds(btnArea.removeFromLeft(40).reduced(2));
    stopButton.setBounds(btnArea.removeFromLeft(40).reduced(2));

    // Right: time display
    auto timeArea = area.removeFromRight(140);
    timeLabel.setBounds(timeArea.reduced(2));

    // Right of time: filename
    auto nameArea = area.removeFromRight(200);
    fileNameLabel.setBounds(nameArea.reduced(2));

    // Center: position slider
    positionSlider.setBounds(area.reduced(4, 2));
}

//==============================================================================
void TransportBar::buttonClicked(juce::Button* button)
{
    if (button == &openButton)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Open Audio File",
            juce::File{},
            "*.wav;*.mp3;*.flac;*.ogg;*.aiff;*.aif;*.wma;*.m4a;*.aac"
        );

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                    engine.loadFile(file);
            });
    }
    else if (button == &playButton)
    {
        engine.play();
    }
    else if (button == &pauseButton)
    {
        engine.pause();
    }
    else if (button == &stopButton)
    {
        engine.stop();
    }
}

//==============================================================================
void TransportBar::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &positionSlider && isDraggingPosition)
    {
        double pos = positionSlider.getValue() * engine.getLengthInSeconds();
        engine.setPosition(pos);
    }
}

void TransportBar::sliderDragStarted(juce::Slider* slider)
{
    if (slider == &positionSlider)
        isDraggingPosition = true;
}

void TransportBar::sliderDragEnded(juce::Slider* slider)
{
    if (slider == &positionSlider)
    {
        isDraggingPosition = false;
        double pos = positionSlider.getValue() * engine.getLengthInSeconds();
        engine.setPosition(pos);
    }
}

//==============================================================================
void TransportBar::timerCallback()
{
    updateTimeDisplay();
}

void TransportBar::updateTimeDisplay()
{
    if (!engine.isFileLoaded())
        return;

    double current = engine.getCurrentPosition();
    double total   = engine.getLengthInSeconds();

    timeLabel.setText(formatTime(current) + " / " + formatTime(total),
                      juce::dontSendNotification);

    if (!isDraggingPosition && total > 0.0)
        positionSlider.setValue(current / total, juce::dontSendNotification);
}

juce::String TransportBar::formatTime(double seconds) const
{
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    return juce::String::formatted("%02d:%02d", mins, secs);
}

//==============================================================================
void TransportBar::transportStateChanged(bool isPlaying)
{
    juce::ignoreUnused(isPlaying);
    repaint();
}

void TransportBar::fileLoaded(const juce::String& fileName, double lengthSeconds)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    fileNameLabel.setText(fileName, juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, pal.bodyText);
    positionSlider.setValue(0.0, juce::dontSendNotification);
    juce::ignoreUnused(lengthSeconds);
}

void TransportBar::themeChanged(AppTheme /*newTheme*/)
{
    applyThemeColours();
    repaint();
}
