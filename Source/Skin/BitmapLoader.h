#pragma once

#include <JuceHeader.h>

//==============================================================================
/// BitmapLoader — loads BMP images using stb_image and converts to juce::Image.
/// Handles the Winamp convention of magenta (#FF00FF) as the transparency key.
namespace BitmapLoader
{
    /// Load a BMP (or PNG/JPG) image from raw memory data.
    /// Returns a juce::Image in ARGB format.
    /// If applyMagentaKey is true, all #FF00FF pixels become fully transparent.
    juce::Image loadFromMemory(const void* data, int dataSize, bool applyMagentaKey = true);

    /// Load from a JUCE InputStream (reads entire stream into memory first).
    juce::Image loadFromStream(juce::InputStream& stream, bool applyMagentaKey = true);

    /// Apply magenta transparency key to an existing image.
    void applyMagentaTransparency(juce::Image& image);
}
