#pragma once

#include <JuceHeader.h>
#include "../Canvas/CanvasModel.h"
#include "../Canvas/CanvasItem.h"
#include <map>

//==============================================================================
/// Serialises / deserialises a CanvasModel to/from JSON using juce::var / JSON.
/// The project file (.mmproj) stores all item layout, background, grid, and
/// an optional skin reference.
class ProjectSerializer
{
public:
    /// Current file format version.
    static constexpr int kFormatVersion = 1;

    //-- Serialisation -------------------------------------------------------

    /// Serialise the full project to a JSON string.
    static juce::String serialise(const CanvasModel& model,
                                  const juce::File& skinFile = {},
                                  const juce::File& audioFile = {});

    /// Serialise and write to file.  Returns true on success.
    static bool saveToFile(const juce::File& file,
                           const CanvasModel& model,
                           const juce::File& skinFile = {},
                           const juce::File& audioFile = {});

    //-- Deserialisation -----------------------------------------------------

    /// Result of loading a project file.
    struct LoadResult
    {
        bool                    success = false;
        juce::String            errorMessage;

        // Data extracted from file:
        struct ItemDesc
        {
            MeterType      type;
            float          x, y, w, h;
            int            rotation;
            int            zOrder;
            bool           locked;
            bool           visible;
            juce::String   name;
            bool           aspectLock;
            float          opacity = 1.0f;
            juce::String   mediaFilePath;
            int            vuChannel = 0;
            juce::Colour   itemBackground { 0x00000000 };

            // Meter colour overrides
            juce::Colour   meterBgColour  { 0x00000000 };
            juce::Colour   meterFgColour  { 0x00000000 };
            BlendMode      blendMode      = BlendMode::Normal;

            // Custom plugin
            juce::String   customPluginId;
            juce::String   customInstanceId;
            std::map<juce::String, juce::var> customPluginPropertyValues;

            // Shape properties
            juce::Colour   fillColour1  { 0xFF3A7BFF };
            juce::Colour   fillColour2  { 0xFF1A4ACA };
            int            gradientDirection = 0;
            float          cornerRadius = 0.0f;
            juce::Colour   strokeColour { 0xFFFFFFFF };
            float          strokeWidth  = 2.0f;
            int            strokeAlignment = 0; ///< 0=center, 1=inside, 2=outside
            int            lineCap      = 0;    ///< 0=butt, 1=round, 2=square
            int            starPoints   = 5;
            float          triangleRoundness = 0.0f;

            // Loudness meter
            float          targetLUFS = -14.0f;
            bool           loudnessShowHistory = true;

            // Frosted glass properties
            bool           frostedGlass = false;
            float          blurRadius   = 10.0f;
            juce::Colour   frostTint    { 0xFFFFFFFF };
            float          frostOpacity = 0.15f;

            // Text properties
            juce::String   textContent  { "Text" };
            juce::String   fontFamily   { "Arial" };
            float          fontSize     = 24.0f;
            bool           fontBold     = false;
            bool           fontItalic   = false;
            juce::Colour   textColour   { 0xFFFFFFFF };
            int            textAlignment = 0;
        };
        std::vector<ItemDesc>   items;

        // Background
        BackgroundMode          bgMode    = BackgroundMode::SolidColour;
        juce::Colour            bgColour1 = juce::Colour(0xFF16213E);
        juce::Colour            bgColour2 = juce::Colour(0xFF0A0A1A);
        float                   bgAngle   = 0.0f;
        juce::String            bgImagePath;

        // Grid
        bool                    gridEnabled  = true;
        int                     gridSpacing  = 10;
        bool                    showGrid     = true;
        bool                    showRuler    = true;
        bool                    smartGuides  = true;

        // External references
        juce::String            skinFilePath;
        juce::String            audioFilePath;
    };

    /// Load from file.
    static LoadResult loadFromFile(const juce::File& file);

    /// Parse from JSON string.
    static LoadResult parse(const juce::String& json);

private:
    static juce::var itemToVar(const CanvasItem& item);
    static juce::var backgroundToVar(const CanvasBackground& bg);
    static juce::var gridToVar(const GridSettings& grid);

    static MeterType meterTypeFromString(const juce::String& s);
    static juce::String meterTypeToString(MeterType t);
    static BackgroundMode bgModeFromString(const juce::String& s);
    static juce::String bgModeToString(BackgroundMode m);
};
