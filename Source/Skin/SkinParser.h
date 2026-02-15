#pragma once

#include "SkinModel.h"

//==============================================================================
/// SkinParser — loads and parses a Winamp Classic Skin (.wsz) file.
///
/// A .wsz file is a renamed ZIP archive containing BMP sprite sheets
/// and text configuration files. This parser handles:
///   - ZIP extraction via juce::ZipFile
///   - Case-insensitive filename matching (skins in the wild are inconsistent)
///   - BMP loading via stb_image with magenta transparency key
///   - Text config parsing (viscolor.txt, region.txt, pledit.txt)
///   - Graceful fallback for missing optional files
class SkinParser
{
public:
    SkinParser() = default;
    ~SkinParser() = default;

    /// Parse a .wsz skin file and return a populated SkinModel.
    /// Returns a valid SkinModel even if some optional files are missing.
    Skin::SkinModel loadFromFile(const juce::File& wszFile);

    /// Parse from a ZIP input stream (e.g., embedded resource).
    Skin::SkinModel loadFromStream(juce::InputStream& stream);

    /// Get the last error message (empty if successful).
    juce::String getLastError() const { return lastError; }

private:
    juce::String lastError;

    /// Extract and load all BMP images from the ZIP archive.
    void loadBitmaps(juce::ZipFile& zip, Skin::SkinModel& model);

    /// Extract and parse text configuration files.
    void loadTextConfigs(juce::ZipFile& zip, Skin::SkinModel& model);

    /// Find a ZIP entry by filename (case-insensitive).
    /// Returns -1 if not found.
    int findEntry(juce::ZipFile& zip, const juce::String& filename);

    /// Read a ZIP entry as a MemoryBlock.
    juce::MemoryBlock readEntry(juce::ZipFile& zip, int entryIndex);

    /// Read a ZIP entry as a String (UTF-8 text).
    juce::String readEntryAsText(juce::ZipFile& zip, int entryIndex);
};
