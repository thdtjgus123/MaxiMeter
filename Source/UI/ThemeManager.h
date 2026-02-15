#pragma once

#include <JuceHeader.h>
#include <optional>
#include "../Skin/SkinModel.h"

//==============================================================================
/// Available application themes.
enum class AppTheme { Dark, Light, Skin };

//==============================================================================
/// Colour IDs used throughout the application.
namespace ThemeColour
{
    enum Ids
    {
        windowBackground = 0x2000001,
        panelBackground,
        toolboxBackground,
        headerText,
        bodyText,
        dimText,
        accent,
        accentHover,
        border,
        meterBackground,
        selection,
        selectionOutline,
        transportBackground,
        statusBarBackground,
        canvasBackground,
        gridLine,
        rulerBackground,
        rulerText,
        toolboxItem,
        toolboxItemHover,
        propertyLabel,
        propertyValue,
        scrollbar
    };
}

//==============================================================================
/// Struct holding all themed colours for quick lookup.
struct ThemePalette
{
    juce::Colour windowBg;
    juce::Colour panelBg;
    juce::Colour toolboxBg;
    juce::Colour headerText;
    juce::Colour bodyText;
    juce::Colour dimText;
    juce::Colour accent;
    juce::Colour accentHover;
    juce::Colour border;
    juce::Colour meterBg;
    juce::Colour selection;
    juce::Colour selectionOutline;
    juce::Colour transportBg;
    juce::Colour statusBarBg;
    juce::Colour canvasBg;
    juce::Colour gridLine;
    juce::Colour rulerBg;
    juce::Colour rulerText;
    juce::Colour toolboxItem;
    juce::Colour toolboxItemHover;
    juce::Colour buttonText;        // auto-contrasted text for buttons/combos
    juce::Colour editorText;        // auto-contrasted text for text editors
    juce::Colour propertyLabel;
    juce::Colour propertyValue;
    juce::Colour scrollbar;

    /// Return white or black (with optional tint) to contrast against the given background.
    static juce::Colour contrastTextFor(juce::Colour bg, juce::Colour hint = juce::Colours::white)
    {
        float lum = bg.getRed() * 0.299f + bg.getGreen() * 0.587f + bg.getBlue() * 0.114f;
        if (lum < 140.0f)
            return hint.getBrightness() > 0.5f ? hint : juce::Colours::white;
        else
            return hint.getBrightness() < 0.5f ? hint : juce::Colour(0xFF1A1A1A);
    }
};

//==============================================================================
/// Manages dark / light theme, provides palettes, notifies listeners.
class ThemeManager
{
public:
    ThemeManager();

    /// Get singleton instance.
    static ThemeManager& getInstance();

    /// Get / set current theme.
    AppTheme getCurrentTheme() const { return currentTheme; }
    void setTheme(AppTheme theme);
    void toggleTheme();

    /// Apply a Winamp skin as a full theme override.
    /// Derives a complete palette from the skin's colors and switches to AppTheme::Skin.
    void applySkinTheme(const Skin::SkinModel& skin);

    /// Clear skin override and revert to the given base theme (or Dark if not specified).
    void clearSkinTheme(AppTheme fallback = AppTheme::Dark);

    /// Set a custom accent colour to override the theme default.
    /// Pass std::nullopt (or default) to use the theme's default accent.
    void setCustomAccentColor(std::optional<juce::Colour> color);

    /// Current palette.
    const ThemePalette& getPalette() const { return palette; }

    /// Convenience colour getter by ID.
    juce::Colour getColour(int colourId) const;

    //-- Listener ------------------------------------------------------------
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void themeChanged(AppTheme newTheme) = 0;
    };

    void addListener(Listener* l)    { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    AppTheme currentTheme = AppTheme::Dark;
    AppTheme baseTheme    = AppTheme::Dark;   // remembered when skin overrides
    std::optional<juce::Colour> customAccent;
    ThemePalette palette;
    ThemePalette skinPalette;
    bool hasSkinPalette = false;
    juce::ListenerList<Listener> listeners;

    void buildPalette();

    ThemePalette darkPalette();
    ThemePalette lightPalette();
    static ThemePalette buildPaletteFromSkin(const Skin::SkinModel& skin);
};
