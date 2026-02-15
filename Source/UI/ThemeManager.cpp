#include "ThemeManager.h"
#include "../Project/AppSettings.h"

//==============================================================================
ThemeManager::ThemeManager()
{
    buildPalette();
}

ThemeManager& ThemeManager::getInstance()
{
    static ThemeManager instance;
    return instance;
}

//==============================================================================
void ThemeManager::setTheme(AppTheme theme)
{
    if (theme == AppTheme::Skin)
    {
        // Use applySkinTheme() instead
        return;
    }
    if (currentTheme == theme && !hasSkinPalette) return;
    currentTheme = theme;
    baseTheme = theme;
    hasSkinPalette = false;
    buildPalette();
    listeners.call(&Listener::themeChanged, currentTheme);

    // Persist
    int id = (theme == AppTheme::Light) ? 2 : 1;
    AppSettings::getInstance().set(AppSettings::kTheme, id);
}

void ThemeManager::toggleTheme()
{
    if (currentTheme == AppTheme::Skin)
    {
        // Exit skin theme ↁEgo to dark
        clearSkinTheme(AppTheme::Dark);
        return;
    }
    setTheme(currentTheme == AppTheme::Dark ? AppTheme::Light : AppTheme::Dark);
}

//==============================================================================
void ThemeManager::applySkinTheme(const Skin::SkinModel& skin)
{
    skinPalette = buildPaletteFromSkin(skin);
    hasSkinPalette = true;
    currentTheme = AppTheme::Skin;
    palette = skinPalette;
    listeners.call(&Listener::themeChanged, currentTheme);
}

void ThemeManager::clearSkinTheme(AppTheme fallback)
{
    hasSkinPalette = false;
    currentTheme = fallback;
    baseTheme = fallback;
    buildPalette();
    listeners.call(&Listener::themeChanged, currentTheme);
}

void ThemeManager::setCustomAccentColor(std::optional<juce::Colour> color)
{
    if (customAccent == color) return;
    customAccent = color;
    
    // Rebuild palette using new accent (if not using skin)
    if (!hasSkinPalette)
    {
        buildPalette();
        listeners.call(&Listener::themeChanged, currentTheme);
    }
}

//==============================================================================
void ThemeManager::buildPalette()
{
    if (hasSkinPalette && currentTheme == AppTheme::Skin)
        palette = skinPalette;
    else
        palette = (currentTheme == AppTheme::Dark) ? darkPalette() : lightPalette();
}

//==============================================================================
ThemePalette ThemeManager::darkPalette()
{
    ThemePalette p;
    p.windowBg          = juce::Colour(0xFF16213E);
    p.panelBg           = juce::Colour(0xFF0A0E1A);
    p.toolboxBg         = juce::Colour(0xFF12192D);
    p.headerText        = juce::Colours::white.withAlpha(0.6f);
    p.bodyText          = juce::Colours::white.withAlpha(0.85f);
    p.dimText           = juce::Colours::grey;
    
    // Apply custom accent if set
    p.accent            = customAccent.value_or(juce::Colour(0xFF3A7BFF));
    
    p.accentHover       = p.accent.brighter(0.2f);
    p.border            = juce::Colour(0xFF2A3A5E);
    p.meterBg           = juce::Colour(0xFF0D1117);
    p.selection         = p.accent.withAlpha(0.35f);
    p.selectionOutline  = p.accent;
    p.transportBg       = juce::Colour(0xFF1A1A2E);
    p.statusBarBg       = juce::Colour(0xFF0E1226);
    p.canvasBg          = juce::Colour(0xFF16213E);
    p.gridLine          = juce::Colours::white.withAlpha(0.05f);
    p.rulerBg           = juce::Colour(0xFF0E1226);
    p.rulerText         = juce::Colours::white.withAlpha(0.4f);
    p.toolboxItem       = juce::Colour(0xFF1A2442);
    p.toolboxItemHover  = juce::Colour(0xFF2A3A5E);
    p.buttonText        = juce::Colours::white.withAlpha(0.85f);
    p.editorText        = juce::Colours::white.withAlpha(0.85f);
    p.propertyLabel     = juce::Colours::white.withAlpha(0.5f);
    p.propertyValue     = juce::Colours::white.withAlpha(0.9f);
    p.scrollbar         = juce::Colours::white.withAlpha(0.15f);
    return p;
}

ThemePalette ThemeManager::lightPalette()
{
    ThemePalette p;
    p.windowBg          = juce::Colour(0xFFF0F2F5);
    p.panelBg           = juce::Colour(0xFFFFFFFF);
    p.toolboxBg         = juce::Colour(0xFFE8ECF0);
    p.headerText        = juce::Colour(0xFF333333);
    p.bodyText          = juce::Colour(0xFF1A1A1A);
    p.dimText           = juce::Colour(0xFF888888);
    
    // Apply custom accent if set
    p.accent            = customAccent.value_or(juce::Colour(0xFF2962FF));
    
    p.accentHover       = p.accent.darker(0.1f);
    p.border            = juce::Colour(0xFFCCCCCC);
    p.meterBg           = juce::Colour(0xFFE0E4E8);
    p.selection         = p.accent.withAlpha(0.25f);
    p.selectionOutline  = p.accent;
    p.transportBg       = juce::Colour(0xFFDDE2E8);
    p.statusBarBg       = juce::Colour(0xFFD0D5DC);
    p.canvasBg          = juce::Colour(0xFFE8ECF0);
    p.gridLine          = juce::Colour(0x15000000);
    p.rulerBg           = juce::Colour(0xFFD0D5DC);
    p.rulerText         = juce::Colour(0xFF555555);
    p.toolboxItem       = juce::Colour(0xFFDBE0E6);
    p.toolboxItemHover  = juce::Colour(0xFFC5CCD6);
    p.buttonText        = juce::Colour(0xFF1A1A1A);
    p.editorText        = juce::Colour(0xFF1A1A1A);
    p.propertyLabel     = juce::Colour(0xFF666666);
    p.propertyValue     = juce::Colour(0xFF1A1A1A);
    p.scrollbar         = juce::Colour(0x20000000);
    return p;
}

//==============================================================================
juce::Colour ThemeManager::getColour(int colourId) const
{
    using namespace ThemeColour;
    switch (colourId)
    {
        case windowBackground:    return palette.windowBg;
        case panelBackground:     return palette.panelBg;
        case toolboxBackground:   return palette.toolboxBg;
        case headerText:          return palette.headerText;
        case bodyText:            return palette.bodyText;
        case dimText:             return palette.dimText;
        case accent:              return palette.accent;
        case accentHover:         return palette.accentHover;
        case border:              return palette.border;
        case meterBackground:     return palette.meterBg;
        case selection:           return palette.selection;
        case selectionOutline:    return palette.selectionOutline;
        case transportBackground: return palette.transportBg;
        case statusBarBackground: return palette.statusBarBg;
        case canvasBackground:    return palette.canvasBg;
        case gridLine:            return palette.gridLine;
        case rulerBackground:     return palette.rulerBg;
        case rulerText:           return palette.rulerText;
        case toolboxItem:         return palette.toolboxItem;
        case toolboxItemHover:    return palette.toolboxItemHover;
        case propertyLabel:       return palette.propertyLabel;
        case propertyValue:       return palette.propertyValue;
        case scrollbar:           return palette.scrollbar;
        default:                  return juce::Colours::magenta;
    }
}

//==============================================================================
ThemePalette ThemeManager::buildPaletteFromSkin(const Skin::SkinModel& skin)
{
    // Extract key colors from the Winamp skin
    // pleditConfig: normalBg, normalText, currentText, selectedBg
    // visColors: 24-color palette (0=bg, 1-17=spectrum bars, 18=osc line, 23=peak)

    const auto& pl = skin.pleditConfig;
    const auto& vc = skin.visColors;

    // Helper: compute perceived brightness
    auto brightness = [](juce::Colour c) -> float {
        return c.getRed() * 0.299f + c.getGreen() * 0.587f + c.getBlue() * 0.114f;
    };

    // 1. Determine "Face" color from Main.bmp (fallback to pledit bg if missing)
    juce::Colour faceColor = pl.normalBg;
    if (skin.hasBitmap(Skin::SkinBitmap::Main))
    {
        // Sample a pixel from the "body" of the main window.
        // Avoid (0,0) as it might be transparent/mask.
        // (25, 25) is usually safely inside the main texture.
        auto mainImg = skin.bitmaps[static_cast<int>(Skin::SkinBitmap::Main)];
        if (mainImg.isValid())
            faceColor = mainImg.getPixelAt(25, 25);
    }
    
    // If face color is suspicious (alpha 0 or magenta mask), try center
    if (faceColor.getAlpha() == 0 || (faceColor.getRed() == 255 && faceColor.getGreen() == 0 && faceColor.getBlue() == 255))
    {
         if (skin.hasBitmap(Skin::SkinBitmap::Main)) {
             auto mainImg = skin.bitmaps[static_cast<int>(Skin::SkinBitmap::Main)];
             if (mainImg.isValid())
                faceColor = mainImg.getPixelAt(mainImg.getWidth() / 2, 20);
         }
    }

    // Still suspicious? Fallback
    if (faceColor.withAlpha(1.0f) == juce::Colours::black || faceColor.getAlpha() == 0)
    {
         // Check if pledit bg is interesting
         if (pl.normalBg.getBrightness() > 0.1f || pl.normalBg.getSaturation() > 0.1f)
             faceColor = pl.normalBg;
         else
             faceColor = juce::Colour(0xFFBAC0C5); // Reliable Winamp-ish grey
    }
    
    // Force opacity
    faceColor = faceColor.withAlpha(1.0f);

    juce::Colour baseText   = pl.normalText;     // primary text (informational only)
    juce::Colour accentCol  = pl.currentText;    // highlight / accent
    juce::Colour selectCol  = pl.selectedBg;     // selection

    // Determine if the skin is dark or light based on face brightness
    bool isDark = brightness(faceColor) < 128.0f;

    // Use simple black or white text based on background brightness for readability
    juce::Colour autoText = isDark ? juce::Colours::white.withAlpha(0.88f)
                                   : juce::Colour(0xFF1A1A1A);
    juce::Colour autoDim  = isDark ? juce::Colours::white.withAlpha(0.45f)
                                   : juce::Colour(0xFF1A1A1A).withAlpha(0.50f);
    juce::Colour autoHead = isDark ? juce::Colours::white.withAlpha(0.65f)
                                   : juce::Colour(0xFF333333);

    // Derive lighter/darker variants
    juce::Colour baseBg    = faceColor;
    juce::Colour panelBg   = isDark ? baseBg.brighter(0.08f) : baseBg.darker(0.04f);
    juce::Colour toolboxBg = isDark ? baseBg.brighter(0.04f) : baseBg.darker(0.06f);
    juce::Colour borderCol = isDark ? baseBg.brighter(0.2f)  : baseBg.darker(0.25f);
    juce::Colour dimCol    = autoDim;
    juce::Colour headerCol = autoHead;

    // Use vis background color (viscolor[0]) for meter backgrounds
    juce::Colour visBg = vc.colors[0];

    // Use oscilloscope line color (viscolor[18]) for grid, it's usually bright
    juce::Colour gridCol = vc.colors[18].withAlpha(0.12f);

    // Build palette
    ThemePalette p;
    p.windowBg          = baseBg;
    p.panelBg           = panelBg;
    p.toolboxBg         = toolboxBg;
    p.headerText        = headerCol;
    p.bodyText          = autoText;
    p.dimText           = dimCol;
    p.accent            = accentCol;
    p.accentHover       = accentCol.brighter(0.3f);
    p.border            = borderCol;
    p.meterBg           = visBg;
    p.selection         = selectCol.withAlpha(0.35f);
    p.selectionOutline  = selectCol;
    p.transportBg       = isDark ? baseBg.darker(0.1f) : baseBg.brighter(0.05f);
    p.statusBarBg       = isDark ? baseBg.darker(0.15f) : baseBg.brighter(0.08f);
    p.canvasBg          = baseBg; // Canvas matches overall skin face
    p.gridLine          = gridCol;
    p.rulerBg           = isDark ? baseBg.darker(0.15f) : baseBg.brighter(0.08f);
    p.rulerText         = autoText.withAlpha(0.5f);
    
    // --- Buttons / ComboBoxes ---
    // Make buttons aggressively vibrant using the skin's Selection Color.
    juce::Colour btnBase = isDark ? baseBg.brighter(0.2f) : baseBg.darker(0.1f);
    
    // Always try to mix selection color if it's not black/transparent
    if (selectCol.withAlpha(1.0f) != juce::Colours::black)
    {
        // Use 65% of selection color. 
        // We don't check saturation/brightness too strictly, assuming skin designer knew what they were doing.
        btnBase = btnBase.interpolatedWith(selectCol, 0.65f);
    }
    // If selection was black, fallback to accent
    else if (accentCol.withAlpha(1.0f) != juce::Colours::black)
    {
        btnBase = btnBase.interpolatedWith(accentCol, 0.65f);
    }

    // Ensure button isn't too dark to see
    if (btnBase.getBrightness() < 0.18f) btnBase = btnBase.brighter(0.2f); // increased brightness floor

    p.toolboxItem       = btnBase;
    p.toolboxItemHover  = btnBase.brighter(0.2f);
    p.buttonText        = ThemePalette::contrastTextFor(btnBase);

    // --- Text Editor / Input Fields ---
    // Use Playlist BG primarily, but lighten/darken it to stand out from Window BG
    p.panelBg = pl.normalBg;
    
    // If Text Box BG is too close to Window BG, shift it
    if (std::abs(p.panelBg.getBrightness() - p.windowBg.getBrightness()) < 0.05f)
    {
        p.panelBg = isDark ? p.panelBg.brighter(0.1f) : p.panelBg.darker(0.1f);
    }

    // Safety: ensure text is readable
    // If text is bright (e.g. green) and bg is bright (e.g. skin face is white), darken BG.
    // If text is dark (e.g. black) and bg is dark, lighten BG.
    float textBright = autoText.getBrightness();
    float bgBright   = p.panelBg.getBrightness();

    if (textBright > 0.6f && bgBright > 0.6f) p.panelBg = p.panelBg.darker(0.5f);
    if (textBright < 0.4f && bgBright < 0.4f) p.panelBg = p.panelBg.brighter(0.5f);

    p.bodyText          = autoText;
    p.editorText        = ThemePalette::contrastTextFor(p.panelBg);
    
    // If the valid text color is too close to panel bg, adjust it
    // (Simple check: if both are dark or both are light) (omitted for brevity, trusting skin)
    
    // ComboBox Arrow and Outline
    // Use accent color (Playlist Current Text) for borders/focus if available and distinct
    if (pl.currentText != pl.normalText)
        p.accent = pl.currentText;
    p.propertyLabel     = autoText.withAlpha(0.55f);
    p.propertyValue     = autoText.withAlpha(0.9f);
    p.scrollbar         = autoText.withAlpha(0.15f);
    return p;
}
