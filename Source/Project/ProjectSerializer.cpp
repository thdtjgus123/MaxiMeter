#include "ProjectSerializer.h"

//==============================================================================
// Helpers
//==============================================================================
juce::String ProjectSerializer::meterTypeToString(MeterType t)
{
    switch (t)
    {
        case MeterType::MultiBandAnalyzer:   return "MultiBandAnalyzer";
        case MeterType::Spectrogram:         return "Spectrogram";
        case MeterType::Goniometer:          return "Goniometer";
        case MeterType::LissajousScope:      return "LissajousScope";
        case MeterType::LoudnessMeter:       return "LoudnessMeter";
        case MeterType::LevelHistogram:      return "LevelHistogram";
        case MeterType::CorrelationMeter:    return "CorrelationMeter";
        case MeterType::PeakMeter:           return "PeakMeter";
        case MeterType::SkinnedSpectrum:     return "SkinnedSpectrum";
        case MeterType::SkinnedVUMeter:      return "SkinnedVUMeter";
        case MeterType::SkinnedOscilloscope: return "SkinnedOscilloscope";
        case MeterType::WinampSkin:          return "WinampSkin";
        case MeterType::WaveformView:        return "WaveformView";        case MeterType::ImageLayer:          return "ImageLayer";
        case MeterType::VideoLayer:          return "VideoLayer";
        case MeterType::SkinnedPlayer:       return "SkinnedPlayer";
        case MeterType::ShapeRectangle:      return "ShapeRectangle";
        case MeterType::ShapeEllipse:        return "ShapeEllipse";
        case MeterType::ShapeTriangle:       return "ShapeTriangle";
        case MeterType::ShapeLine:           return "ShapeLine";
        case MeterType::ShapeStar:           return "ShapeStar";
        case MeterType::TextLabel:           return "TextLabel";
        default: return "Unknown";
    }
}

MeterType ProjectSerializer::meterTypeFromString(const juce::String& s)
{
    if (s == "MultiBandAnalyzer")   return MeterType::MultiBandAnalyzer;
    if (s == "Spectrogram")         return MeterType::Spectrogram;
    if (s == "Goniometer")          return MeterType::Goniometer;
    if (s == "LissajousScope")      return MeterType::LissajousScope;
    if (s == "LoudnessMeter")       return MeterType::LoudnessMeter;
    if (s == "LevelHistogram")      return MeterType::LevelHistogram;
    if (s == "CorrelationMeter")    return MeterType::CorrelationMeter;
    if (s == "PeakMeter")           return MeterType::PeakMeter;
    if (s == "SkinnedSpectrum")     return MeterType::SkinnedSpectrum;
    if (s == "SkinnedVUMeter")      return MeterType::SkinnedVUMeter;
    if (s == "SkinnedOscilloscope") return MeterType::SkinnedOscilloscope;
    if (s == "WinampSkin")          return MeterType::WinampSkin;
    if (s == "WaveformView")        return MeterType::WaveformView;
    if (s == "ImageLayer")          return MeterType::ImageLayer;
    if (s == "VideoLayer")          return MeterType::VideoLayer;
    if (s == "SkinnedPlayer")       return MeterType::SkinnedPlayer;
    if (s == "ShapeRectangle")      return MeterType::ShapeRectangle;
    if (s == "ShapeEllipse")        return MeterType::ShapeEllipse;
    if (s == "ShapeTriangle")       return MeterType::ShapeTriangle;
    if (s == "ShapeLine")           return MeterType::ShapeLine;
    if (s == "ShapeStar")           return MeterType::ShapeStar;
    if (s == "TextLabel")           return MeterType::TextLabel;
    return MeterType::MultiBandAnalyzer;
}

juce::String ProjectSerializer::bgModeToString(BackgroundMode m)
{
    switch (m)
    {
        case BackgroundMode::SolidColour:     return "SolidColour";
        case BackgroundMode::LinearGradient:  return "LinearGradient";
        case BackgroundMode::RadialGradient:  return "RadialGradient";
        case BackgroundMode::Image:           return "Image";
        default: return "SolidColour";
    }
}

BackgroundMode ProjectSerializer::bgModeFromString(const juce::String& s)
{
    if (s == "LinearGradient")  return BackgroundMode::LinearGradient;
    if (s == "RadialGradient")  return BackgroundMode::RadialGradient;
    if (s == "Image")           return BackgroundMode::Image;
    return BackgroundMode::SolidColour;
}

//==============================================================================
juce::var ProjectSerializer::itemToVar(const CanvasItem& item)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("type",       meterTypeToString(item.meterType));
    obj->setProperty("name",       item.name);
    obj->setProperty("x",          item.x);
    obj->setProperty("y",          item.y);
    obj->setProperty("width",      item.width);
    obj->setProperty("height",     item.height);
    obj->setProperty("rotation",   item.rotation);
    obj->setProperty("zOrder",     item.zOrder);
    obj->setProperty("locked",     item.locked);
    obj->setProperty("visible",    item.visible);
    obj->setProperty("aspectLock", item.aspectLock);
    obj->setProperty("opacity",    item.opacity);
    obj->setProperty("vuChannel",  item.vuChannel);
    obj->setProperty("itemBackground", item.itemBackground.toString());

    if (item.mediaFilePath.isNotEmpty())
        obj->setProperty("mediaFilePath", item.mediaFilePath);

    // Shape properties
    obj->setProperty("fillColour1",       item.fillColour1.toString());
    obj->setProperty("fillColour2",       item.fillColour2.toString());
    obj->setProperty("gradientDirection", item.gradientDirection);
    obj->setProperty("cornerRadius",      item.cornerRadius);
    obj->setProperty("strokeColour",      item.strokeColour.toString());
    obj->setProperty("strokeWidth",       item.strokeWidth);

    // Frosted glass
    obj->setProperty("frostedGlass",      item.frostedGlass);
    obj->setProperty("blurRadius",        item.blurRadius);
    obj->setProperty("frostTint",         item.frostTint.toString());
    obj->setProperty("frostOpacity",      item.frostOpacity);

    // Font properties (for all items, not just TextLabel)
    obj->setProperty("fontFamily",    item.fontFamily);
    obj->setProperty("fontSize",      item.fontSize);

    // Text properties
    if (item.meterType == MeterType::TextLabel)
    {
        obj->setProperty("textContent",   item.textContent);
        obj->setProperty("fontBold",      item.fontBold);
        obj->setProperty("fontItalic",    item.fontItalic);
        obj->setProperty("textColour",    item.textColour.toString());
        obj->setProperty("textAlignment", item.textAlignment);
    }

    return juce::var(obj);
}

juce::var ProjectSerializer::backgroundToVar(const CanvasBackground& bg)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("mode",    bgModeToString(bg.mode));
    obj->setProperty("colour1", bg.colour1.toString());
    obj->setProperty("colour2", bg.colour2.toString());
    obj->setProperty("angle",   bg.angle);
    if (bg.imageFile.existsAsFile())
        obj->setProperty("imagePath", bg.imageFile.getFullPathName());
    return juce::var(obj);
}

juce::var ProjectSerializer::gridToVar(const GridSettings& grid)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("enabled",     grid.enabled);
    obj->setProperty("spacing",     grid.spacing);
    obj->setProperty("showGrid",    grid.showGrid);
    obj->setProperty("showRuler",   grid.showRuler);
    obj->setProperty("smartGuides", grid.smartGuides);
    return juce::var(obj);
}

//==============================================================================
// Serialisation
//==============================================================================

juce::String ProjectSerializer::serialise(const CanvasModel& model,
                                          const juce::File& skinFile,
                                          const juce::File& audioFile)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("formatVersion", kFormatVersion);
    root->setProperty("app",           "MaxiMeter");

    // Items
    juce::Array<juce::var> itemArray;
    for (int i = 0; i < model.getNumItems(); ++i)
        itemArray.add(itemToVar(*model.getItem(i)));
    root->setProperty("items", itemArray);

    // Background
    root->setProperty("background", backgroundToVar(model.background));

    // Grid
    root->setProperty("grid", gridToVar(model.grid));

    // Zoom / Pan
    root->setProperty("zoom", model.zoom);
    root->setProperty("panX", model.panX);
    root->setProperty("panY", model.panY);

    // External references
    if (skinFile.existsAsFile())
        root->setProperty("skinFile", skinFile.getFullPathName());
    if (audioFile.existsAsFile())
        root->setProperty("audioFile", audioFile.getFullPathName());

    return juce::JSON::toString(juce::var(root), true);
}

bool ProjectSerializer::saveToFile(const juce::File& file,
                                   const CanvasModel& model,
                                   const juce::File& skinFile,
                                   const juce::File& audioFile)
{
    auto json = serialise(model, skinFile, audioFile);
    return file.replaceWithText(json);
}

//==============================================================================
// Deserialisation
//==============================================================================

ProjectSerializer::LoadResult ProjectSerializer::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return { false, "File not found: " + file.getFullPathName() };

    auto json = file.loadFileAsString();
    return parse(json);
}

ProjectSerializer::LoadResult ProjectSerializer::parse(const juce::String& json)
{
    LoadResult result;

    auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject())
    {
        result.errorMessage = "Invalid JSON format.";
        return result;
    }

    auto* root = parsed.getDynamicObject();
    if (!root)
    {
        result.errorMessage = "Root is not a JSON object.";
        return result;
    }

    // Version check
    int version = root->getProperty("formatVersion");
    if (version < 1)
    {
        result.errorMessage = "Unknown format version.";
        return result;
    }

    // Items
    auto itemsVar = root->getProperty("items");
    if (auto* itemArray = itemsVar.getArray())
    {
        for (const auto& iv : *itemArray)
        {
            if (auto* obj = iv.getDynamicObject())
            {
                LoadResult::ItemDesc desc;
                desc.type       = meterTypeFromString(obj->getProperty("type").toString());
                desc.name       = obj->getProperty("name").toString();
                desc.x          = static_cast<float>((double)obj->getProperty("x"));
                desc.y          = static_cast<float>((double)obj->getProperty("y"));
                desc.w          = static_cast<float>((double)obj->getProperty("width"));
                desc.h          = static_cast<float>((double)obj->getProperty("height"));
                desc.rotation   = static_cast<int>((int)obj->getProperty("rotation"));
                desc.zOrder     = static_cast<int>((int)obj->getProperty("zOrder"));
                desc.locked     = (bool)obj->getProperty("locked");
                desc.visible    = (bool)obj->getProperty("visible");
                desc.aspectLock = (bool)obj->getProperty("aspectLock");
                if (obj->hasProperty("opacity"))
                    desc.opacity = static_cast<float>((double)obj->getProperty("opacity"));
                if (obj->hasProperty("mediaFilePath"))
                    desc.mediaFilePath = obj->getProperty("mediaFilePath").toString();
                if (obj->hasProperty("vuChannel"))
                    desc.vuChannel = static_cast<int>((int)obj->getProperty("vuChannel"));
                if (obj->hasProperty("itemBackground"))
                    desc.itemBackground = juce::Colour::fromString(obj->getProperty("itemBackground").toString());

                // Shape properties
                if (obj->hasProperty("fillColour1"))
                    desc.fillColour1 = juce::Colour::fromString(obj->getProperty("fillColour1").toString());
                if (obj->hasProperty("fillColour2"))
                    desc.fillColour2 = juce::Colour::fromString(obj->getProperty("fillColour2").toString());
                if (obj->hasProperty("gradientDirection"))
                    desc.gradientDirection = static_cast<int>((int)obj->getProperty("gradientDirection"));
                if (obj->hasProperty("cornerRadius"))
                    desc.cornerRadius = static_cast<float>((double)obj->getProperty("cornerRadius"));
                if (obj->hasProperty("strokeColour"))
                    desc.strokeColour = juce::Colour::fromString(obj->getProperty("strokeColour").toString());
                if (obj->hasProperty("strokeWidth"))
                    desc.strokeWidth = static_cast<float>((double)obj->getProperty("strokeWidth"));

                // Frosted glass
                if (obj->hasProperty("frostedGlass"))
                    desc.frostedGlass = (bool)obj->getProperty("frostedGlass");
                if (obj->hasProperty("blurRadius"))
                    desc.blurRadius = static_cast<float>((double)obj->getProperty("blurRadius"));
                if (obj->hasProperty("frostTint"))
                    desc.frostTint = juce::Colour::fromString(obj->getProperty("frostTint").toString());
                if (obj->hasProperty("frostOpacity"))
                    desc.frostOpacity = static_cast<float>((double)obj->getProperty("frostOpacity"));

                // Text properties
                if (obj->hasProperty("textContent"))
                    desc.textContent = obj->getProperty("textContent").toString();
                if (obj->hasProperty("fontFamily"))
                    desc.fontFamily = obj->getProperty("fontFamily").toString();
                if (obj->hasProperty("fontSize"))
                    desc.fontSize = static_cast<float>((double)obj->getProperty("fontSize"));
                if (obj->hasProperty("fontBold"))
                    desc.fontBold = (bool)obj->getProperty("fontBold");
                if (obj->hasProperty("fontItalic"))
                    desc.fontItalic = (bool)obj->getProperty("fontItalic");
                if (obj->hasProperty("textColour"))
                    desc.textColour = juce::Colour::fromString(obj->getProperty("textColour").toString());
                if (obj->hasProperty("textAlignment"))
                    desc.textAlignment = static_cast<int>((int)obj->getProperty("textAlignment"));

                result.items.push_back(desc);
            }
        }
    }

    // Background
    auto bgVar = root->getProperty("background");
    if (auto* bg = bgVar.getDynamicObject())
    {
        result.bgMode    = bgModeFromString(bg->getProperty("mode").toString());
        result.bgColour1 = juce::Colour::fromString(bg->getProperty("colour1").toString());
        result.bgColour2 = juce::Colour::fromString(bg->getProperty("colour2").toString());
        result.bgAngle   = static_cast<float>((double)bg->getProperty("angle"));
        result.bgImagePath = bg->getProperty("imagePath").toString();
    }

    // Grid
    auto gridVar = root->getProperty("grid");
    if (auto* grd = gridVar.getDynamicObject())
    {
        result.gridEnabled = (bool)grd->getProperty("enabled");
        result.gridSpacing = static_cast<int>((int)grd->getProperty("spacing"));
        result.showGrid    = (bool)grd->getProperty("showGrid");
        result.showRuler   = (bool)grd->getProperty("showRuler");
        result.smartGuides = (bool)grd->getProperty("smartGuides");
    }

    // External refs
    result.skinFilePath  = root->getProperty("skinFile").toString();
    result.audioFilePath = root->getProperty("audioFile").toString();

    result.success = true;
    return result;
}
