#include "SkinParser.h"
#include "BitmapLoader.h"
#include "SkinTextParsers.h"

//==============================================================================
Skin::SkinModel SkinParser::loadFromFile(const juce::File& wszFile)
{
    Skin::SkinModel model;
    lastError.clear();

    if (!wszFile.existsAsFile())
    {
        lastError = "File not found: " + wszFile.getFullPathName();
        return model;
    }

    model.skinName = wszFile.getFileNameWithoutExtension();
    model.skinFile = wszFile;

    juce::ZipFile zip(wszFile);

    if (zip.getNumEntries() == 0)
    {
        lastError = "Failed to open or empty ZIP: " + wszFile.getFileName();
        return model;
    }

    DBG("SkinParser: Loading skin '" + model.skinName + "' ("
        + juce::String(zip.getNumEntries()) + " entries)");

    loadBitmaps(zip, model);
    loadTextConfigs(zip, model);

    // Check minimum requirement: main.bmp must be present
    if (!model.hasBitmap(Skin::SkinBitmap::Main))
    {
        lastError = "Not a valid Winamp classic skin: main.bmp not found";
        DBG("SkinParser: " + lastError);
    }

    return model;
}

Skin::SkinModel SkinParser::loadFromStream(juce::InputStream& stream)
{
    Skin::SkinModel model;
    lastError.clear();

    juce::ZipFile zip(&stream, false);

    if (zip.getNumEntries() == 0)
    {
        lastError = "Failed to open ZIP from stream";
        return model;
    }

    loadBitmaps(zip, model);
    loadTextConfigs(zip, model);

    return model;
}

//==============================================================================
void SkinParser::loadBitmaps(juce::ZipFile& zip, Skin::SkinModel& model)
{
    for (int i = 0; i < static_cast<int>(Skin::SkinBitmap::Count); ++i)
    {
        auto bitmapEnum = static_cast<Skin::SkinBitmap>(i);
        auto filename = juce::String(Skin::bitmapFilename(bitmapEnum));

        if (filename.isEmpty())
            continue;

        int entryIdx = findEntry(zip, filename);

        if (entryIdx < 0)
        {
            // Try .png alternative (some modern skins use PNG)
            auto pngName = filename.replace(".bmp", ".png", true);
            entryIdx = findEntry(zip, pngName);
        }

        if (entryIdx < 0)
        {
            // Optional files: nums_ex.bmp, eq_ex.bmp, pledit.bmp
            bool isOptional = (bitmapEnum == Skin::SkinBitmap::NumbersEx ||
                               bitmapEnum == Skin::SkinBitmap::EQEx ||
                               bitmapEnum == Skin::SkinBitmap::PleditBmp);

            if (!isOptional)
                DBG("SkinParser: Missing bitmap: " + filename);

            continue;
        }

        // Read the entry data
        auto data = readEntry(zip, entryIdx);
        if (data.getSize() == 0)
            continue;

        // Load via stb_image with magenta transparency key
        auto image = BitmapLoader::loadFromMemory(
            data.getData(), static_cast<int>(data.getSize()), true);

        if (image.isValid())
        {
            model.bitmaps[static_cast<size_t>(i)] = image;
            model.bitmapLoaded[static_cast<size_t>(i)] = true;
            DBG("SkinParser: Loaded " + filename + " ("
                + juce::String(image.getWidth()) + "x"
                + juce::String(image.getHeight()) + ")");
        }
        else
        {
            DBG("SkinParser: Failed to decode " + filename);
        }
    }
}

//==============================================================================
void SkinParser::loadTextConfigs(juce::ZipFile& zip, Skin::SkinModel& model)
{
    // viscolor.txt
    int idx = findEntry(zip, "viscolor.txt");
    if (idx >= 0)
    {
        auto text = readEntryAsText(zip, idx);
        if (text.isNotEmpty())
            model.visColors = SkinTextParsers::parseVisColors(text);
    }

    // region.txt
    idx = findEntry(zip, "region.txt");
    if (idx >= 0)
    {
        auto text = readEntryAsText(zip, idx);
        if (text.isNotEmpty())
            model.regions = SkinTextParsers::parseRegion(text);
    }

    // pledit.txt
    idx = findEntry(zip, "pledit.txt");
    if (idx >= 0)
    {
        auto text = readEntryAsText(zip, idx);
        if (text.isNotEmpty())
            model.pleditConfig = SkinTextParsers::parsePledit(text);
    }

    // readme.txt
    idx = findEntry(zip, "readme.txt");
    if (idx < 0)
        idx = findEntry(zip, "read me.txt");  // occasionally seen
    if (idx >= 0)
    {
        model.readmeText = readEntryAsText(zip, idx);
    }
}

//==============================================================================
int SkinParser::findEntry(juce::ZipFile& zip, const juce::String& filename)
{
    // Case-insensitive search — skins in the wild have inconsistent casing
    // (e.g., MAIN.BMP, main.bmp, Main.Bmp)
    // Also handle entries with path prefixes (some skins nest files in a subfolder)
    auto lowerTarget = filename.toLowerCase();

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        auto* entry = zip.getEntry(i);
        if (entry == nullptr)
            continue;

        auto entryName = entry->filename.toLowerCase();

        // Exact match (case-insensitive)
        if (entryName == lowerTarget)
            return i;

        // Match filename only (strip any directory prefix)
        if (entryName.endsWith("/" + lowerTarget) ||
            entryName.endsWith("\\" + lowerTarget))
            return i;

        // Strip any leading path to just the filename
        auto entryFilename = juce::File::createFileWithoutCheckingPath(entryName)
                                .getFileName().toLowerCase();
        if (entryFilename == lowerTarget)
            return i;
    }

    return -1;
}

//==============================================================================
juce::MemoryBlock SkinParser::readEntry(juce::ZipFile& zip, int entryIndex)
{
    juce::MemoryBlock block;

    auto stream = std::unique_ptr<juce::InputStream>(zip.createStreamForEntry(entryIndex));
    if (stream != nullptr)
        stream->readIntoMemoryBlock(block);

    return block;
}

juce::String SkinParser::readEntryAsText(juce::ZipFile& zip, int entryIndex)
{
    auto block = readEntry(zip, entryIndex);
    if (block.getSize() == 0)
        return {};

    return block.toString();
}
