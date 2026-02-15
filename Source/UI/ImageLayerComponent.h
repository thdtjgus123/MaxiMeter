#pragma once

#include <JuceHeader.h>

//==============================================================================
/// Displays a static image (PNG, JPG, BMP, GIF frame) on the canvas.
/// Supports loading from file, stretch-to-fill with optional aspect ratio.
class ImageLayerComponent : public juce::Component
{
public:
    ImageLayerComponent() = default;
    ~ImageLayerComponent() override = default;

    /// Load an image from file. Returns true on success.
    bool loadFromFile(const juce::File& file)
    {
        auto img = juce::ImageFileFormat::loadFrom(file);
        if (img.isValid())
        {
            image = img;
            filePath = file.getFullPathName();
            repaint();
            return true;
        }
        return false;
    }

    /// Set image directly.
    void setImage(const juce::Image& img)
    {
        image = img;
        repaint();
    }

    juce::String getFilePath() const { return filePath; }
    bool hasImage() const { return image.isValid(); }

    void paint(juce::Graphics& g) override
    {
        if (image.isValid())
        {
            g.drawImage(image, getLocalBounds().toFloat(),
                        juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            g.fillAll(juce::Colour(0xFF1A1A2E));
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("Drop image here", getLocalBounds(),
                       juce::Justification::centred);
        }
    }

private:
    juce::Image image;
    juce::String filePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImageLayerComponent)
};
