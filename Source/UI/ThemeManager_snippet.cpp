    // Explicitly derive button colors from base face, but strongly tinted by selection color
    juce::Colour btnBase = isDark ? baseBg.brighter(0.2f) : baseBg.darker(0.1f);
    
    // Mix 60% of the selection color into the button base.
    // Ensure the selection color has some brightness so it doesn't turn buttons black.
    juce::Colour effectiveSelect = selectCol;
    if (effectiveSelect.getBrightness() < 0.2f)
        effectiveSelect = effectiveSelect.brighter(0.3f);

    if (selectCol.withAlpha(1.0f) != juce::Colours::black)
    {
        btnBase = btnBase.interpolatedWith(effectiveSelect, 0.6f);
    }
    // If still too dark (e.g. skin was pitch black), lift it up
    if (btnBase.getBrightness() < 0.15f) 
        btnBase = btnBase.brighter(0.15f);

    p.toolboxItem       = btnBase;
    p.toolboxItemHover  = btnBase.brighter(0.2f);

    // --- Text Editor / Input Fields ---
    // Users want to see the box. If background is black, box should be slightly lighter.
    p.panelBg = pl.normalBg.interpolatedWith(faceColor, 0.3f);
    
    // Ensure contrast against the main window background (so the box shape is visible)
    if (std::abs(p.panelBg.getBrightness() - p.windowBg.getBrightness()) < 0.08f)
    {
        p.panelBg = p.panelBg.brighter(0.15f);
    }

    // Ensure text is readable on this new background
    // If text is light and bg became light, or text dark and bg dark -> flip bg
    if (pl.normalText.getBrightness() > 0.5f && p.panelBg.getBrightness() > 0.6f)
        p.panelBg = p.panelBg.darker(0.6f);
    else if (pl.normalText.getBrightness() < 0.5f && p.panelBg.getBrightness() < 0.3f)
        p.panelBg = p.panelBg.brighter(0.5f);

    p.bodyText          = pl.normalText;
