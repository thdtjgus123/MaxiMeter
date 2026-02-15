#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioEngine.h"
#include "FontAwesomeIcons.h"
#include "ThemeManager.h"

//==============================================================================
/// TransportBar — play/pause/stop buttons, position slider, time display,
/// and file-open button. Uses Fluent Design icons and theme colors.
class TransportBar : public juce::Component,
                     public juce::Button::Listener,
                     public juce::Slider::Listener,
                     public juce::Timer,
                     public AudioEngine::Listener,
                     public ThemeManager::Listener
{
public:
    explicit TransportBar(AudioEngine& engine);
    ~TransportBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Button callbacks
    void buttonClicked(juce::Button* button) override;

    // Slider callback (seek)
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted(juce::Slider* slider) override;
    void sliderDragEnded(juce::Slider* slider) override;

    // Timer for position update
    void timerCallback() override;

    // AudioEngine::Listener
    void transportStateChanged(bool isPlaying) override;
    void fileLoaded(const juce::String& fileName, double lengthSeconds) override;

    // ThemeManager::Listener
    void themeChanged(AppTheme newTheme) override;

private:
    AudioEngine& engine;

    //-- Icon button helper - draws a Fluent icon as a button --
    class IconButton : public juce::Button
    {
    public:
        IconButton(const juce::String& name, juce::Path iconPath)
            : juce::Button(name), icon(std::move(iconPath)) {}

        void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            auto area = getLocalBounds().toFloat().reduced(2.0f);

            if (isDown)
                g.setColour(pal.accent.darker(0.3f));
            else if (isHighlighted)
                g.setColour(pal.accentHover.withAlpha(0.3f));
            else
                g.setColour(pal.border.withAlpha(0.2f));

            g.fillRoundedRectangle(area, 4.0f);

            auto iconArea = area.reduced(6.0f);
            auto colour = isDown ? pal.bodyText.darker(0.2f)
                                 : (isHighlighted ? pal.accentHover : pal.bodyText);
            FontAwesomeIcons::drawIcon(g, icon, iconArea, colour);
        }

    private:
        juce::Path icon;
    };

    IconButton openButton   { "Open",  FontAwesomeIcons::openFolderIcon() };
    IconButton playButton   { "Play",  FontAwesomeIcons::playIcon() };
    IconButton pauseButton  { "Pause", FontAwesomeIcons::pauseIcon() };
    IconButton stopButton   { "Stop",  FontAwesomeIcons::stopIcon() };
    juce::Slider     positionSlider;

    juce::Label      timeLabel;
    juce::Label      fileNameLabel;

    bool isDraggingPosition = false;

    juce::String formatTime(double seconds) const;
    void updateTimeDisplay();
    void applyThemeColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
