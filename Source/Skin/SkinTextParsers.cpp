#include "SkinTextParsers.h"

namespace SkinTextParsers
{

//==============================================================================
// viscolor.txt parser
//==============================================================================
Skin::VisColors parseVisColors(const juce::String& text)
{
    Skin::VisColors result;  // starts with defaults

    juce::StringArray lines;
    lines.addLines(text);

    int colorIndex = 0;

    for (const auto& line : lines)
    {
        if (colorIndex >= Skin::VisColors::kNumColors)
            break;

        auto trimmed = line.trim();

        // Skip empty lines and comments
        if (trimmed.isEmpty() || trimmed.startsWith("//"))
            continue;

        // Parse "R,G,B" or "R G B" (mixed separators common in wild skins)
        // Regex: capture up to 3 integers
        int r = 0, g = 0, b = 0;
        juce::StringArray tokens;

        // Replace commas with spaces, then tokenize
        auto normalized = trimmed.replaceCharacter(',', ' ');
        tokens.addTokens(normalized, " \t", "");
        tokens.removeEmptyStrings();

        if (tokens.size() >= 3)
        {
            r = tokens[0].getIntValue();
            g = tokens[1].getIntValue();
            b = tokens[2].getIntValue();

            result.colors[static_cast<size_t>(colorIndex)] =
                juce::Colour(static_cast<uint8_t>(juce::jlimit(0, 255, r)),
                             static_cast<uint8_t>(juce::jlimit(0, 255, g)),
                             static_cast<uint8_t>(juce::jlimit(0, 255, b)));
            ++colorIndex;
        }
    }

    DBG("viscolor.txt: parsed " + juce::String(colorIndex) + " colors");
    return result;
}

//==============================================================================
// region.txt parser
//==============================================================================
static Skin::Region parseRegionSection(const juce::StringPairArray& props)
{
    Skin::Region region;

    if (props.containsKey("numpoints"))
    {
        region.numPoints = props.getValue("numpoints", "0").getIntValue();
    }

    if (props.containsKey("pointlist"))
    {
        auto pointStr = props.getValue("pointlist", "");
        juce::StringArray tokens;
        tokens.addTokens(pointStr, ",", "");
        tokens.removeEmptyStrings();

        for (int i = 0; i + 1 < tokens.size(); i += 2)
        {
            int x = tokens[i].trim().getIntValue();
            int y = tokens[i + 1].trim().getIntValue();
            region.points.push_back({ x, y });
        }
    }

    return region;
}

Skin::RegionData parseRegion(const juce::String& text)
{
    Skin::RegionData result;

    juce::StringArray lines;
    lines.addLines(text);

    juce::String currentSection;
    juce::StringPairArray currentProps;

    auto flushSection = [&]()
    {
        if (currentSection.equalsIgnoreCase("normal"))
            result.normal = parseRegionSection(currentProps);
        else if (currentSection.equalsIgnoreCase("windowshade"))
            result.windowShade = parseRegionSection(currentProps);
        else if (currentSection.equalsIgnoreCase("equalizer"))
            result.equalizer = parseRegionSection(currentProps);

        currentProps.clear();
    };

    for (const auto& line : lines)
    {
        auto trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWith(";") || trimmed.startsWith("//"))
            continue;

        // Section header: [SectionName]
        if (trimmed.startsWith("[") && trimmed.endsWith("]"))
        {
            flushSection();
            currentSection = trimmed.substring(1, trimmed.length() - 1).trim();
            continue;
        }

        // Key=Value pair
        int eqPos = trimmed.indexOf("=");
        if (eqPos > 0)
        {
            auto key   = trimmed.substring(0, eqPos).trim().toLowerCase();
            auto value = trimmed.substring(eqPos + 1).trim();
            currentProps.set(key, value);
        }
    }

    flushSection();

    DBG("region.txt: normal=" + juce::String(result.normal.numPoints)
        + " shade=" + juce::String(result.windowShade.numPoints)
        + " eq=" + juce::String(result.equalizer.numPoints));

    return result;
}

//==============================================================================
// pledit.txt parser
//==============================================================================
static juce::Colour parseColorString(const juce::String& str)
{
    auto s = str.trim();
    if (s.startsWith("#"))
        s = s.substring(1);

    // Parse hex RRGGBB
    if (s.length() == 6)
    {
        uint32_t hex = static_cast<uint32_t>(s.getHexValue32());
        return juce::Colour(static_cast<uint8_t>((hex >> 16) & 0xFF),
                            static_cast<uint8_t>((hex >> 8) & 0xFF),
                            static_cast<uint8_t>(hex & 0xFF));
    }

    return juce::Colours::black;
}

Skin::PleditConfig parsePledit(const juce::String& text)
{
    Skin::PleditConfig result;

    juce::StringArray lines;
    lines.addLines(text);

    bool inTextSection = false;

    for (const auto& line : lines)
    {
        auto trimmed = line.trim();

        if (trimmed.equalsIgnoreCase("[Text]") || trimmed.equalsIgnoreCase("[text]"))
        {
            inTextSection = true;
            continue;
        }

        if (trimmed.startsWith("["))
        {
            inTextSection = false;
            continue;
        }

        if (!inTextSection)
            continue;

        int eqPos = trimmed.indexOf("=");
        if (eqPos <= 0) continue;

        auto key   = trimmed.substring(0, eqPos).trim().toLowerCase();
        auto value = trimmed.substring(eqPos + 1).trim();

        if (key == "normal")          result.normalText  = parseColorString(value);
        else if (key == "current")    result.currentText = parseColorString(value);
        else if (key == "normalbg")   result.normalBg    = parseColorString(value);
        else if (key == "selectedbg") result.selectedBg  = parseColorString(value);
        else if (key == "font")       result.fontName    = value;
    }

    DBG("pledit.txt: font=" + result.fontName
        + " normal=" + result.normalText.toDisplayString(true));

    return result;
}

} // namespace SkinTextParsers
