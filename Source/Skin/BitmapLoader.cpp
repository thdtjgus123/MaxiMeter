#include "BitmapLoader.h"
#include "stb_image.h"

namespace BitmapLoader
{

juce::Image loadFromMemory(const void* data, int dataSize, bool applyMagentaKey)
{
    if (data == nullptr || dataSize <= 0)
        return {};

    int width = 0, height = 0, channels = 0;

    // Force 4-channel RGBA output
    unsigned char* pixels = stbi_load_from_memory(
        static_cast<const unsigned char*>(data),
        dataSize,
        &width, &height, &channels, 4);

    if (pixels == nullptr)
    {
        DBG("BitmapLoader: stb_image failed: " + juce::String(stbi_failure_reason()));
        return {};
    }

    // Create a JUCE image and copy pixel data
    juce::Image image(juce::Image::ARGB, width, height, true);

    {
        juce::Image::BitmapData bitmapData(image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int srcIndex = (y * width + x) * 4;
                uint8_t r = pixels[srcIndex + 0];
                uint8_t g = pixels[srcIndex + 1];
                uint8_t b = pixels[srcIndex + 2];
                uint8_t a = pixels[srcIndex + 3];

                // Apply magenta (#FF00FF) transparency key
                if (applyMagentaKey && r == 255 && g == 0 && b == 255)
                    a = 0;

                bitmapData.setPixelColour(x, y, juce::Colour(r, g, b, a));
            }
        }
    }

    stbi_image_free(pixels);

    DBG("BitmapLoader: loaded " + juce::String(width) + "x" + juce::String(height)
        + " (" + juce::String(channels) + "ch)");

    return image;
}

juce::Image loadFromStream(juce::InputStream& stream, bool applyMagentaKey)
{
    juce::MemoryBlock block;
    stream.readIntoMemoryBlock(block);

    if (block.getSize() == 0)
        return {};

    return loadFromMemory(block.getData(), static_cast<int>(block.getSize()), applyMagentaKey);
}

void applyMagentaTransparency(juce::Image& image)
{
    if (!image.isValid())
        return;

    juce::Image::BitmapData bitmapData(image, juce::Image::BitmapData::readWrite);

    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            auto c = bitmapData.getPixelColour(x, y);
            if (c.getRed() == 255 && c.getGreen() == 0 && c.getBlue() == 255)
                bitmapData.setPixelColour(x, y, juce::Colours::transparentBlack);
        }
    }
}

} // namespace BitmapLoader
