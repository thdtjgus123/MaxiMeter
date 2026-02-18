#include "MeterSettingsPanel.h"
#include "../UI/ColourPickerWithEyedropper.h"
#include "../UI/MultiBandAnalyzer.h"
#include "../UI/Spectrogram.h"
#include "../UI/Goniometer.h"
#include "../UI/LissajousScope.h"
#include "../UI/LoudnessMeter.h"
#include "../UI/LevelHistogram.h"
#include "../UI/CorrelationMeter.h"
#include "../UI/PeakMeter.h"
#include "../UI/SkinnedSpectrumAnalyzer.h"
#include "../UI/SkinnedVUMeter.h"
#include "../UI/SkinnedOscilloscope.h"
#include "../UI/ShapeComponent.h"
#include "../UI/TextLabelComponent.h"
#include "../UI/ImageLayerComponent.h"
#include "../UI/VideoLayerComponent.h"
#include "../UI/ThemeManager.h"
#include "CustomPluginComponent.h"
#include "PythonPluginBridge.h"

//==============================================================================
static void styleLabel(juce::Label& lbl)
{
    // Let global LookAndFeel handle colours if possible, 
    // or just set Font. 
    // If we must set colour, do it via LookAndFeel or ensure it updates.
    // However, labels usually default to proper text colour from L&F.
    // We'll trust L&F for text colour or set it to a generic ID that updates.
    lbl.setFont(juce::Font(11.0f));
}

static void styleSlider(juce::Slider& s, double min, double max, double step, double def)
{
    s.setRange(min, max, step);
    s.setValue(def, juce::dontSendNotification);
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    // Remove hardcoded colours so L&F can update them dynamically
}

static void styleToggle(juce::ToggleButton& btn)
{
    // Remove hardcoded alpha text colour, let L&F handle it
}

static void styleCombo(juce::ComboBox& c)
{
    // Remove hardcoded colours so L&F can update them dynamically
}

//==============================================================================
MeterSettingsPanel::MeterSettingsPanel(CanvasModel& m) : model(m)
{
    model.addListener(this);

    titleLabel.setText("SETTINGS", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId,
                         ThemeManager::getInstance().getPalette().headerText);
    addAndMakeVisible(titleLabel);

    meterTypeLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    meterTypeLabel.setColour(juce::Label::textColourId,
                             ThemeManager::getInstance().getPalette().bodyText.withAlpha(0.9f));
    addChildComponent(meterTypeLabel);

    // ── Common ──
    styleLabel(fontSizeLabel);      addChildComponent(fontSizeLabel);
    styleSlider(fontSizeSlider, 6, 48, 1, 12);  addChildComponent(fontSizeSlider);
    styleLabel(fontFamilyLabel);    addChildComponent(fontFamilyLabel);
    styleCombo(fontFamilyCombo);    addChildComponent(fontFamilyCombo);
    {
        fontFamilyCombo.addItem("Default", 1);
        auto allFonts = juce::Font::findAllTypefaceNames();
        allFonts.sort(true);
        for (int i = 0; i < allFonts.size(); ++i)
            fontFamilyCombo.addItem(allFonts[i], i + 2);   // IDs start at 2
        fontFamilyCombo.setSelectedId(1, juce::dontSendNotification);
    }

    // ── Audio / Analysis ──
    styleLabel(smoothingLabel);     addChildComponent(smoothingLabel);
    styleSlider(smoothingSlider, 1, 100, 1, 20);   addChildComponent(smoothingSlider);
    styleLabel(dynamicRangeLabel);  addChildComponent(dynamicRangeLabel);
    styleSlider(minDbSlider, -120, 0, 1, -60);      addChildComponent(minDbSlider);
    styleSlider(maxDbSlider, -30, 12, 1, 0);         addChildComponent(maxDbSlider);

    // ── Spectrum ──
    styleLabel(numBandsLabel);      addChildComponent(numBandsLabel);
    styleCombo(numBandsCombo);      addChildComponent(numBandsCombo);
    numBandsCombo.addItem("8",  1);
    numBandsCombo.addItem("16", 2);
    numBandsCombo.addItem("31", 3);
    numBandsCombo.addItem("64", 4);
    numBandsCombo.setSelectedId(3, juce::dontSendNotification);

    styleLabel(scaleModeLabel);     addChildComponent(scaleModeLabel);
    styleCombo(scaleModeCombo);     addChildComponent(scaleModeCombo);
    scaleModeCombo.addItem("Logarithmic", 1);
    scaleModeCombo.addItem("Linear", 2);
    scaleModeCombo.addItem("Octave", 3);

    // ── Spectrogram ──
    styleLabel(colourMapLabel);     addChildComponent(colourMapLabel);
    styleCombo(colourMapCombo);     addChildComponent(colourMapCombo);
    colourMapCombo.addItem("Rainbow", 1);
    colourMapCombo.addItem("Heat", 2);
    colourMapCombo.addItem("Greyscale", 3);
    colourMapCombo.addItem("Custom", 4);

    styleLabel(scrollDirLabel);     addChildComponent(scrollDirLabel);
    styleCombo(scrollDirCombo);     addChildComponent(scrollDirCombo);
    scrollDirCombo.addItem("Horizontal", 1);
    scrollDirCombo.addItem("Vertical", 2);

    // ── Goniometer ──
    styleLabel(dotSizeLabel);       addChildComponent(dotSizeLabel);
    styleSlider(dotSizeSlider, 1, 8, 0.5, 2);  addChildComponent(dotSizeSlider);
    styleLabel(trailLabel);         addChildComponent(trailLabel);
    styleSlider(trailSlider, 0, 1, 0.01, 0.5); addChildComponent(trailSlider);
    styleToggle(showGridToggle);    addChildComponent(showGridToggle);

    // ── Loudness ──
    styleLabel(targetLufsLabel);    addChildComponent(targetLufsLabel);
    styleCombo(targetLufsCombo);    addChildComponent(targetLufsCombo);
    targetLufsCombo.addItem("-14 LUFS (Streaming)", 1);
    targetLufsCombo.addItem("-23 LUFS (EBU R128)", 2);
    targetLufsCombo.addItem("-24 LUFS (ATSC A/85)", 3);
    targetLufsCombo.addItem("-16 LUFS (Podcast)", 4);
    styleToggle(showHistoryToggle); addChildComponent(showHistoryToggle);

    // ── Peak Meter ──
    styleLabel(peakModeLabel);      addChildComponent(peakModeLabel);
    styleCombo(peakModeCombo);      addChildComponent(peakModeCombo);
    peakModeCombo.addItem("Sample Peak", 1);
    peakModeCombo.addItem("True Peak", 2);

    styleLabel(peakHoldLabel);      addChildComponent(peakHoldLabel);
    styleSlider(peakHoldSlider, 0, 10000, 100, 1000); addChildComponent(peakHoldSlider);
    styleToggle(clipWarningToggle); addChildComponent(clipWarningToggle);

    // ── Correlation ──
    styleLabel(integrationLabel);   addChildComponent(integrationLabel);
    styleSlider(integrationSlider, 50, 5000, 50, 300); addChildComponent(integrationSlider);
    styleToggle(showNumericToggle); addChildComponent(showNumericToggle);

    // ── Histogram ──
    styleLabel(binResLabel);        addChildComponent(binResLabel);
    styleSlider(binResSlider, 0.1, 3.0, 0.1, 1.0);    addChildComponent(binResSlider);
    styleToggle(cumulativeToggle);  addChildComponent(cumulativeToggle);
    styleToggle(showStereoToggle);  addChildComponent(showStereoToggle);

    // ── Skinned VU ──
    styleLabel(ballisticLabel);     addChildComponent(ballisticLabel);
    styleCombo(ballisticCombo);     addChildComponent(ballisticCombo);
    ballisticCombo.addItem("VU", 1);
    ballisticCombo.addItem("PPM Type I", 2);
    ballisticCombo.addItem("PPM Type II", 3);
    ballisticCombo.addItem("Nordic", 4);

    styleLabel(decayTimeLabel);     addChildComponent(decayTimeLabel);
    styleSlider(decayTimeSlider, 100, 5000, 50, 300);  addChildComponent(decayTimeSlider);

    styleLabel(vuChannelLabel);      addChildComponent(vuChannelLabel);
    styleCombo(vuChannelCombo);      addChildComponent(vuChannelCombo);
    vuChannelCombo.addItem("Left (L)", 1);
    vuChannelCombo.addItem("Right (R)", 2);
    vuChannelCombo.setSelectedId(1, juce::dontSendNotification);

    // ── Skinned Oscilloscope ──
    styleLabel(lineThicknessLabel); addChildComponent(lineThicknessLabel);
    styleSlider(lineThicknessSlider, 0.5, 5.0, 0.25, 1.0); addChildComponent(lineThicknessSlider);
    styleLabel(drawStyleLabel);     addChildComponent(drawStyleLabel);
    styleCombo(drawStyleCombo);     addChildComponent(drawStyleCombo);
    drawStyleCombo.addItem("Line", 1);
    drawStyleCombo.addItem("Dots", 2);
    drawStyleCombo.addItem("Solid Fill", 3);

    // ── Lissajous Mode ──
    styleLabel(lissajousModeLabel); addChildComponent(lissajousModeLabel);
    styleCombo(lissajousModeCombo); addChildComponent(lissajousModeCombo);
    lissajousModeCombo.addItem("Lissajous", 1);
    lissajousModeCombo.addItem("XY", 2);
    lissajousModeCombo.addItem("Polar", 3);

    // ── Shape Controls ──
    styleLabel(fillColour1Label);   addChildComponent(fillColour1Label);
    addChildComponent(fillColour1Button);
    fillColour1Button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A7BFF));

    styleLabel(fillColour2Label);   addChildComponent(fillColour2Label);
    addChildComponent(fillColour2Button);
    fillColour2Button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A4ACA));

    styleLabel(gradientDirLabel);   addChildComponent(gradientDirLabel);
    styleCombo(gradientDirCombo);   addChildComponent(gradientDirCombo);
    gradientDirCombo.addItem("Solid", 1);
    gradientDirCombo.addItem("Vertical", 2);
    gradientDirCombo.addItem("Horizontal", 3);
    gradientDirCombo.addItem("Diagonal", 4);

    styleLabel(cornerRadiusLabel);  addChildComponent(cornerRadiusLabel);
    styleSlider(cornerRadiusSlider, 0, 100, 1, 0); addChildComponent(cornerRadiusSlider);

    styleLabel(strokeColourLabel);  addChildComponent(strokeColourLabel);
    addChildComponent(strokeColourButton);
    strokeColourButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFFFFFF));

    styleLabel(strokeWidthLabel);   addChildComponent(strokeWidthLabel);
    styleSlider(strokeWidthSlider, 0, 20, 0.5, 2); addChildComponent(strokeWidthSlider);

    styleLabel(strokeAlignLabel);   addChildComponent(strokeAlignLabel);
    styleCombo(strokeAlignCombo);   addChildComponent(strokeAlignCombo);
    strokeAlignCombo.addItem("Center",  1);
    strokeAlignCombo.addItem("Inside",  2);
    strokeAlignCombo.addItem("Outside", 3);

    styleLabel(lineCapLabel);       addChildComponent(lineCapLabel);
    styleCombo(lineCapCombo);       addChildComponent(lineCapCombo);
    lineCapCombo.addItem("Butt",   1);
    lineCapCombo.addItem("Round",  2);
    lineCapCombo.addItem("Square", 3);

    styleLabel(starPointsLabel);    addChildComponent(starPointsLabel);
    styleSlider(starPointsSlider, 3, 20, 1, 5); addChildComponent(starPointsSlider);

    styleLabel(triRoundLabel);      addChildComponent(triRoundLabel);
    styleSlider(triRoundSlider, 0, 100, 1, 0);  addChildComponent(triRoundSlider);

    styleLabel(itemBgLabel);        addChildComponent(itemBgLabel);
    addChildComponent(itemBgButton);
    itemBgButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));

    // Shape colour button callbacks
    auto makeColourCallback = [this](juce::TextButton& btn,
                                      std::function<void(juce::Colour)> onColourChange)
    {
        btn.onClick = [this, &btn, onColourChange]()
        {
            auto picker = std::make_unique<ColourPickerWithEyedropper>();
            picker->setCurrentColour(btn.findColour(juce::TextButton::buttonColourId));
            picker->setSize(300, 430);
            picker->onColourChanged = [&btn, onColourChange](juce::Colour c)
            {
                btn.setColour(juce::TextButton::buttonColourId, c);
                btn.repaint();
                onColourChange(c);
            };
            juce::CallOutBox::launchAsynchronously(
                std::move(picker), btn.getScreenBounds(), nullptr);
        };
    };

    makeColourCallback(fillColour1Button, [this](juce::Colour c) {
        auto sel = model.getSelectedItems();
        if (!sel.empty()) { sel.front()->fillColour1 = c; applySettingsToItem(sel.front()); }
    });
    makeColourCallback(fillColour2Button, [this](juce::Colour c) {
        auto sel = model.getSelectedItems();
        if (!sel.empty()) { sel.front()->fillColour2 = c; applySettingsToItem(sel.front()); }
    });
    makeColourCallback(strokeColourButton, [this](juce::Colour c) {
        auto sel = model.getSelectedItems();
        if (!sel.empty()) { sel.front()->strokeColour = c; applySettingsToItem(sel.front()); }
    });
    makeColourCallback(itemBgButton, [this](juce::Colour c) {
        auto sel = model.getSelectedItems();
        if (!sel.empty()) { sel.front()->itemBackground = c; applySettingsToItem(sel.front()); }
    });

    // ── Frosted Glass Controls ──
    styleToggle(frostedGlassToggle); addChildComponent(frostedGlassToggle);
    styleLabel(blurRadiusLabel);     addChildComponent(blurRadiusLabel);
    styleSlider(blurRadiusSlider, 1, 60, 1, 10); addChildComponent(blurRadiusSlider);
    styleLabel(frostOpacityLabel);   addChildComponent(frostOpacityLabel);
    styleSlider(frostOpacitySlider, 0, 100, 1, 15); addChildComponent(frostOpacitySlider);
    styleLabel(frostTintLabel);      addChildComponent(frostTintLabel);
    {
        frostTintButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFFFFFF));
        addChildComponent(frostTintButton);
    }
    makeColourCallback(frostTintButton, [this](juce::Colour c) {
        auto sel = model.getSelectedItems();
        if (!sel.empty()) { sel.front()->frostTint = c; applySettingsToItem(sel.front()); }
    });

    // ── Text Controls ──
    styleLabel(textContentLabel); addChildComponent(textContentLabel);
    {
        auto& pal = ThemeManager::getInstance().getPalette();
        textContentEditor.setColour(juce::TextEditor::backgroundColourId, pal.toolboxItem);
        textContentEditor.setColour(juce::TextEditor::textColourId, pal.bodyText);
        textContentEditor.setColour(juce::TextEditor::outlineColourId, pal.border);
        textContentEditor.setMultiLine(true, true);
        textContentEditor.setReturnKeyStartsNewLine(false);
        addChildComponent(textContentEditor);
    }

    styleLabel(textFontLabel); addChildComponent(textFontLabel);
    styleCombo(textFontCombo); addChildComponent(textFontCombo);
    {
        auto allFonts = juce::Font::findAllTypefaceNames();
        allFonts.sort(true);
        for (int i = 0; i < allFonts.size(); ++i)
            textFontCombo.addItem(allFonts[i], i + 1);
        // Select "Arial" if available, else first
        int sel = 1;
        for (int i = 0; i < allFonts.size(); ++i)
            if (allFonts[i].equalsIgnoreCase("Arial")) { sel = i + 1; break; }
        textFontCombo.setSelectedId(sel, juce::dontSendNotification);
    }

    styleLabel(textSizeLabel); addChildComponent(textSizeLabel);
    styleSlider(textSizeSlider, 6, 200, 1, 24); addChildComponent(textSizeSlider);

    styleToggle(textBoldToggle);   addChildComponent(textBoldToggle);
    styleToggle(textItalicToggle); addChildComponent(textItalicToggle);

    styleLabel(textColourLabel);   addChildComponent(textColourLabel);
    addChildComponent(textColourButton);
    textColourButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFFFFFF));

    makeColourCallback(textColourButton, [this](juce::Colour c) {
        auto sel = model.getSelectedItems();
        if (!sel.empty()) { sel.front()->textColour = c; applySettingsToItem(sel.front()); }
    });

    styleLabel(textAlignLabel); addChildComponent(textAlignLabel);
    styleCombo(textAlignCombo); addChildComponent(textAlignCombo);
    textAlignCombo.addItem("Left", 1);
    textAlignCombo.addItem("Center", 2);
    textAlignCombo.addItem("Right", 3);
    textAlignCombo.setSelectedId(1, juce::dontSendNotification);

    // ── File Selectors ──
    addChildComponent(skinFileButton);
    skinPathLabel.setColour(juce::Label::textColourId, 
        ThemeManager::getInstance().getPalette().bodyText.withAlpha(0.6f));
    skinPathLabel.setFont(juce::Font(10.0f));
    addChildComponent(skinPathLabel);

    // Remove explicit colours so they follow the skin theme
    // audioFileButton.setColour(...);
    // skinFileButton.setColour(...);

    addChildComponent(audioFileButton);
    audioPathLabel.setColour(juce::Label::textColourId, 
        ThemeManager::getInstance().getPalette().bodyText.withAlpha(0.6f));
    audioPathLabel.setFont(juce::Font(10.0f));
    addChildComponent(audioPathLabel);

    // Media file button
    // mediaFileButton.setColour(...);
    mediaFileButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    addChildComponent(mediaFileButton);
    mediaPathLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    mediaPathLabel.setFont(juce::Font(10.0f));
    addChildComponent(mediaPathLabel);

    // SVG file button
    svgFileButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    addChildComponent(svgFileButton);
    svgPathLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    svgPathLabel.setFont(juce::Font(10.0f));
    addChildComponent(svgPathLabel);

    // File button callbacks
    skinFileButton.onClick = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Winamp Skin", juce::File{}, "*.wsz;*.zip");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    skinPathLabel.setText(f.getFileName(), juce::dontSendNotification);
                    if (onSkinFileSelected)
                        onSkinFileSelected(f);
                }
            });
    };

    audioFileButton.onClick = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Audio File", juce::File{},
            "*.wav;*.mp3;*.flac;*.ogg;*.aiff;*.aif");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    audioPathLabel.setText(f.getFileName(), juce::dontSendNotification);
                    if (onAudioFileSelected)
                        onAudioFileSelected(f);
                }
            });
    };

    mediaFileButton.onClick = [this]()
    {
        auto sel = model.getSelectedItems();
        if (sel.empty()) return;
        auto* item = sel.front();

        // Choose filter based on item type
        juce::String filter;
        juce::String title;
        if (item->meterType == MeterType::VideoLayer)
        {
            filter = "*.gif;*.mp4;*.avi;*.webm;*.mov;*.mkv;*.wmv;*.flv;*.m4v;*.png;*.jpg;*.jpeg;*.bmp;*.tiff";
            title  = "Select Video, GIF, or Image File";
        }
        else
        {
            filter = "*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tiff";
            title  = "Select Image File";
        }

        auto chooser = std::make_shared<juce::FileChooser>(title, juce::File{}, filter);
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    mediaPathLabel.setText(f.getFileName(), juce::dontSendNotification);
                    auto sel2 = model.getSelectedItems();
                    if (!sel2.empty())
                    {
                        auto* item2 = sel2.front();
                        item2->mediaFilePath = f.getFullPathName();

                        if (item2->meterType == MeterType::ImageLayer)
                        {
                            if (auto* comp = dynamic_cast<ImageLayerComponent*>(item2->component.get()))
                                comp->loadFromFile(f);
                        }
                        else if (item2->meterType == MeterType::VideoLayer)
                        {
                            VLC_Log::log("MeterSettingsPanel: VideoLayer detected");
                            auto* rawComp = item2->component.get();
                            VLC_Log::log(("MeterSettingsPanel: rawComp ptr = " + juce::String::toHexString((juce::int64)(uintptr_t)rawComp)).toRawUTF8());
                            auto* comp = dynamic_cast<VideoLayerComponent*>(rawComp);
                            VLC_Log::log(("MeterSettingsPanel: VideoLayerComponent* = " + juce::String::toHexString((juce::int64)(uintptr_t)comp)).toRawUTF8());
                            if (comp != nullptr)
                            {
                                VLC_Log::log("MeterSettingsPanel: calling loadFromFile");
                                comp->loadFromFile(f);
                                VLC_Log::log("MeterSettingsPanel: loadFromFile returned");
                            }
                            else
                            {
                                VLC_Log::log("MeterSettingsPanel: dynamic_cast returned nullptr!");
                            }
                        }

                        if (onMediaFileSelected)
                            onMediaFileSelected(item2, f);
                    }
                }
            });
    };

    svgFileButton.onClick = [this]()
    {
        auto sel = model.getSelectedItems();
        if (sel.empty()) return;
        auto* item = sel.front();

        auto chooser = std::make_shared<juce::FileChooser>(
            "Select SVG File", juce::File{}, "*.svg");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    auto svgContent = f.loadFileAsString();
                    if (svgContent.isNotEmpty())
                    {
                        svgPathLabel.setText(f.getFileName(), juce::dontSendNotification);
                        auto sel2 = model.getSelectedItems();
                        if (!sel2.empty())
                        {
                            auto* item2 = sel2.front();
                            item2->svgPathData = svgContent;
                            item2->svgFilePath = f.getFullPathName();

                            if (auto* shape = dynamic_cast<ShapeComponent*>(item2->component.get()))
                                shape->setSvgPathData(svgContent);
                        }
                    }
                }
            });
    };

    // ── Wire slider/combo change callbacks ──
    auto commitChange = [this]() {
        auto sel = model.getSelectedItems();
        if (!sel.empty())
            applySettingsToItem(sel.front());
    };

    smoothingSlider.onValueChange      = commitChange;
    minDbSlider.onValueChange          = commitChange;
    maxDbSlider.onValueChange          = commitChange;
    fontSizeSlider.onValueChange       = commitChange;
    fontFamilyCombo.onChange            = commitChange;
    numBandsCombo.onChange              = commitChange;
    scaleModeCombo.onChange             = commitChange;
    colourMapCombo.onChange             = commitChange;
    scrollDirCombo.onChange             = commitChange;
    dotSizeSlider.onValueChange        = commitChange;
    trailSlider.onValueChange          = commitChange;
    showGridToggle.onClick             = commitChange;
    targetLufsCombo.onChange            = commitChange;
    showHistoryToggle.onClick          = commitChange;
    peakModeCombo.onChange              = commitChange;
    peakHoldSlider.onValueChange       = commitChange;
    clipWarningToggle.onClick          = commitChange;
    integrationSlider.onValueChange    = commitChange;
    showNumericToggle.onClick          = commitChange;
    binResSlider.onValueChange         = commitChange;
    cumulativeToggle.onClick           = commitChange;
    showStereoToggle.onClick           = commitChange;
    ballisticCombo.onChange             = commitChange;
    decayTimeSlider.onValueChange      = commitChange;
    vuChannelCombo.onChange             = commitChange;
    lineThicknessSlider.onValueChange  = commitChange;
    drawStyleCombo.onChange             = commitChange;
    lissajousModeCombo.onChange         = commitChange;
    gradientDirCombo.onChange           = commitChange;
    cornerRadiusSlider.onValueChange   = commitChange;
    strokeWidthSlider.onValueChange    = commitChange;
    strokeAlignCombo.onChange           = commitChange;
    lineCapCombo.onChange               = commitChange;
    starPointsSlider.onValueChange     = commitChange;
    triRoundSlider.onValueChange       = commitChange;
    textContentEditor.onReturnKey      = commitChange;
    textContentEditor.onFocusLost      = commitChange;
    textFontCombo.onChange              = commitChange;
    textSizeSlider.onValueChange       = commitChange;
    textBoldToggle.onClick             = commitChange;
    textItalicToggle.onClick           = commitChange;
    textAlignCombo.onChange             = commitChange;
    frostedGlassToggle.onClick         = commitChange;
    blurRadiusSlider.onValueChange     = commitChange;
    frostOpacitySlider.onValueChange   = commitChange;

    // ── Reparent all controls into scrollable viewport ──
    juce::Array<juce::Component*> toReparent;
    for (int i = 0; i < getNumChildComponents(); ++i)
        toReparent.add(getChildComponent(i));

    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);

    for (auto* c : toReparent)
        content_.addAndMakeVisible(c);
}

MeterSettingsPanel::~MeterSettingsPanel()
{
    model.removeListener(this);
}

//==============================================================================
void MeterSettingsPanel::paint(juce::Graphics& g)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    g.fillAll(pal.toolboxBg);

    // Top-edge divider handle (drag to resize settings panel height)
    g.setColour(pal.border);
    g.fillRect(0, 0, getWidth(), kDividerHeight);
    g.setColour(pal.dimText.withAlpha(0.4f));
    int cx = getWidth() / 2;
    g.fillRect(cx - 15, 1, 30, 2);
}

void MeterSettingsPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(kDividerHeight);  // skip top divider zone
    viewport_.setBounds(area);
    layoutContent();
    layoutContent();  // double-call to handle scrollbar appearance
}

void MeterSettingsPanel::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(e.y < kDividerHeight ? juce::MouseCursor::UpDownResizeCursor
                                        : juce::MouseCursor::NormalCursor);
}

void MeterSettingsPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y < kDividerHeight)
    {
        draggingDivider_   = true;
        dividerDragStartY_ = e.getScreenY();
    }
}

void MeterSettingsPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingDivider_ && onDividerDragged)
    {
        int delta = e.getScreenY() - dividerDragStartY_;
        dividerDragStartY_ = e.getScreenY();
        onDividerDragged(delta);
    }
}

void MeterSettingsPanel::mouseUp(const juce::MouseEvent&)
{
    draggingDivider_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void MeterSettingsPanel::layoutContent()
{
    const int panelW = juce::jmax(50, viewport_.getMaximumVisibleWidth());
    auto area = juce::Rectangle<int>(0, 0, panelW, 4000).reduced(6);
    titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(2);
    meterTypeLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);

    RowLayout layout { area };

    // Show controls dynamically based on currentType
    // We just position everything; visibility was set in showControlsForType
    auto positionIfVisible = [&](juce::Label& lbl, juce::Component& ctrl) {
        if (ctrl.isVisible())
            layout.labelAndControl(lbl, ctrl);
    };

    auto positionToggle = [&](juce::ToggleButton& t) {
        if (t.isVisible())
            layout.singleControl(t);
    };

    auto positionButton = [&](juce::TextButton& btn, juce::Label& path) {
        if (btn.isVisible())
        {
            layout.singleControl(btn, 26);
            layout.singleControl(path, 16);
        }
    };

    // File selectors (top priority if visible)
    positionButton(skinFileButton, skinPathLabel);
    positionButton(audioFileButton, audioPathLabel);
    positionButton(mediaFileButton, mediaPathLabel);
    positionButton(svgFileButton, svgPathLabel);

    // Common
    positionIfVisible(fontSizeLabel, fontSizeSlider);
    positionIfVisible(fontFamilyLabel, fontFamilyCombo);
    positionIfVisible(smoothingLabel, smoothingSlider);
    positionIfVisible(dynamicRangeLabel, minDbSlider);
    if (maxDbSlider.isVisible())
    {
        auto r = layout.nextRow();
        r.removeFromLeft(layout.labelW);
        maxDbSlider.setBounds(r);
    }

    // Spectrum
    positionIfVisible(numBandsLabel, numBandsCombo);
    positionIfVisible(scaleModeLabel, scaleModeCombo);

    // Spectrogram
    positionIfVisible(colourMapLabel, colourMapCombo);
    positionIfVisible(scrollDirLabel, scrollDirCombo);

    // Goniometer
    positionIfVisible(dotSizeLabel, dotSizeSlider);
    positionIfVisible(trailLabel, trailSlider);
    positionToggle(showGridToggle);

    // Lissajous
    positionIfVisible(lissajousModeLabel, lissajousModeCombo);

    // Loudness
    positionIfVisible(targetLufsLabel, targetLufsCombo);
    positionToggle(showHistoryToggle);

    // Peak
    positionIfVisible(peakModeLabel, peakModeCombo);
    positionIfVisible(peakHoldLabel, peakHoldSlider);
    positionToggle(clipWarningToggle);

    // Correlation
    positionIfVisible(integrationLabel, integrationSlider);
    positionToggle(showNumericToggle);

    // Histogram
    positionIfVisible(binResLabel, binResSlider);
    positionToggle(cumulativeToggle);
    positionToggle(showStereoToggle);

    // Skinned VU
    positionIfVisible(ballisticLabel, ballisticCombo);
    positionIfVisible(decayTimeLabel, decayTimeSlider);
    positionIfVisible(vuChannelLabel, vuChannelCombo);

    // Skinned Oscilloscope
    positionIfVisible(lineThicknessLabel, lineThicknessSlider);
    positionIfVisible(drawStyleLabel, drawStyleCombo);

    // Shape controls
    positionIfVisible(fillColour1Label, fillColour1Button);
    positionIfVisible(fillColour2Label, fillColour2Button);
    positionIfVisible(gradientDirLabel, gradientDirCombo);
    positionIfVisible(cornerRadiusLabel, cornerRadiusSlider);
    positionIfVisible(strokeColourLabel, strokeColourButton);
    positionIfVisible(strokeWidthLabel, strokeWidthSlider);
    positionIfVisible(strokeAlignLabel, strokeAlignCombo);
    positionIfVisible(lineCapLabel, lineCapCombo);
    positionIfVisible(starPointsLabel, starPointsSlider);
    positionIfVisible(triRoundLabel, triRoundSlider);
    positionIfVisible(itemBgLabel, itemBgButton);

    // Frosted glass controls
    positionToggle(frostedGlassToggle);
    positionIfVisible(blurRadiusLabel, blurRadiusSlider);
    positionIfVisible(frostTintLabel, frostTintButton);
    positionIfVisible(frostOpacityLabel, frostOpacitySlider);

    // Text controls
    if (textContentEditor.isVisible())
    {
        layout.labelAndControl(textContentLabel, textContentEditor);
        auto edBounds = textContentEditor.getBounds();
        edBounds.setHeight(48);
        textContentEditor.setBounds(edBounds);
        layout.area.removeFromTop(26);
    }
    positionIfVisible(textFontLabel, textFontCombo);
    positionIfVisible(textSizeLabel, textSizeSlider);
    positionToggle(textBoldToggle);
    positionToggle(textItalicToggle);
    positionIfVisible(textColourLabel, textColourButton);
    positionIfVisible(textAlignLabel, textAlignCombo);

    // Dynamic custom-plugin controls
    for (auto* dc : dynControls_)
    {
        if (dc->slider && dc->slider->isVisible())
            layout.labelAndControl(*dc->label, *dc->slider);
        else if (dc->toggle && dc->toggle->isVisible())
            layout.singleControl(*dc->toggle);
        else if (dc->combo && dc->combo->isVisible())
            layout.labelAndControl(*dc->label, *dc->combo);
        else if (dc->colorBtn && dc->colorBtn->isVisible())
            layout.labelAndControl(*dc->label, *dc->colorBtn);
    }

    // Size content to actual used height
    int usedH = 4000 - layout.area.getHeight() + 12;
    content_.setSize(panelW, usedH);
}

//==============================================================================
void MeterSettingsPanel::hideAllControls()
{
    // Hide everything except the title (controls are children of content_)
    for (int i = 0; i < content_.getNumChildComponents(); ++i)
    {
        auto* child = content_.getChildComponent(i);
        if (child != &titleLabel)
            child->setVisible(false);
    }
    clearDynamicControls();
}

void MeterSettingsPanel::showCommonControls()
{
    meterTypeLabel.setVisible(true);
    fontSizeLabel.setVisible(true);
    fontSizeSlider.setVisible(true);
    fontFamilyLabel.setVisible(true);
    fontFamilyCombo.setVisible(true);
}

void MeterSettingsPanel::showControlsForType(MeterType type)
{
    hideAllControls();
    currentType = type;   // update BEFORE early returns so refresh() re-shows controls

    if (type == MeterType::NumTypes)
    {
        meterTypeLabel.setText("No Selection", juce::dontSendNotification);
        meterTypeLabel.setVisible(true);
        return;
    }

    showCommonControls();
    meterTypeLabel.setText(meterTypeName(type), juce::dontSendNotification);

    // Audio file selector — always show
    audioFileButton.setVisible(true);
    audioPathLabel.setVisible(true);

    switch (type)
    {
        case MeterType::MultiBandAnalyzer:
            smoothingLabel.setVisible(true);   smoothingSlider.setVisible(true);
            dynamicRangeLabel.setVisible(true); minDbSlider.setVisible(true); maxDbSlider.setVisible(true);
            numBandsLabel.setVisible(true);    numBandsCombo.setVisible(true);
            scaleModeLabel.setVisible(true);   scaleModeCombo.setVisible(true);
            break;

        case MeterType::Spectrogram:
            dynamicRangeLabel.setVisible(true); minDbSlider.setVisible(true); maxDbSlider.setVisible(true);
            colourMapLabel.setVisible(true);   colourMapCombo.setVisible(true);
            scrollDirLabel.setVisible(true);   scrollDirCombo.setVisible(true);
            break;

        case MeterType::Goniometer:
            dotSizeLabel.setVisible(true);     dotSizeSlider.setVisible(true);
            trailLabel.setVisible(true);       trailSlider.setVisible(true);
            showGridToggle.setVisible(true);
            break;

        case MeterType::LissajousScope:
            lissajousModeLabel.setVisible(true); lissajousModeCombo.setVisible(true);
            trailLabel.setVisible(true);         trailSlider.setVisible(true);
            showGridToggle.setVisible(true);
            break;

        case MeterType::LoudnessMeter:
            targetLufsLabel.setVisible(true);    targetLufsCombo.setVisible(true);
            showHistoryToggle.setVisible(true);
            break;

        case MeterType::LevelHistogram:
            dynamicRangeLabel.setVisible(true); minDbSlider.setVisible(true); maxDbSlider.setVisible(true);
            binResLabel.setVisible(true);       binResSlider.setVisible(true);
            cumulativeToggle.setVisible(true);
            showStereoToggle.setVisible(true);
            break;

        case MeterType::CorrelationMeter:
            integrationLabel.setVisible(true);  integrationSlider.setVisible(true);
            showNumericToggle.setVisible(true);
            break;

        case MeterType::PeakMeter:
            peakModeLabel.setVisible(true);     peakModeCombo.setVisible(true);
            peakHoldLabel.setVisible(true);     peakHoldSlider.setVisible(true);
            smoothingLabel.setVisible(true);    smoothingSlider.setVisible(true);
            clipWarningToggle.setVisible(true);
            break;

        case MeterType::SkinnedSpectrum:
            skinFileButton.setVisible(true);    skinPathLabel.setVisible(true);
            smoothingLabel.setVisible(true);    smoothingSlider.setVisible(true);
            numBandsLabel.setVisible(true);     numBandsCombo.setVisible(true);
            break;

        case MeterType::SkinnedVUMeter:
            skinFileButton.setVisible(true);    skinPathLabel.setVisible(true);
            ballisticLabel.setVisible(true);    ballisticCombo.setVisible(true);
            decayTimeLabel.setVisible(true);    decayTimeSlider.setVisible(true);
            vuChannelLabel.setVisible(true);    vuChannelCombo.setVisible(true);
            break;

        case MeterType::SkinnedOscilloscope:
            skinFileButton.setVisible(true);    skinPathLabel.setVisible(true);
            lineThicknessLabel.setVisible(true); lineThicknessSlider.setVisible(true);
            drawStyleLabel.setVisible(true);     drawStyleCombo.setVisible(true);
            break;

        case MeterType::WinampSkin:
            skinFileButton.setVisible(true);    skinPathLabel.setVisible(true);
            break;

        case MeterType::WaveformView:
            // Only common + audio file
            break;

        case MeterType::ImageLayer:
        case MeterType::VideoLayer:
            // Show media import, hide audio/font controls
            mediaFileButton.setVisible(true);   mediaPathLabel.setVisible(true);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            break;

        case MeterType::SkinnedPlayer:
            skinFileButton.setVisible(true);    skinPathLabel.setVisible(true);
            break;

        case MeterType::Equalizer:
            skinFileButton.setVisible(true);    skinPathLabel.setVisible(true);
            break;

        case MeterType::ShapeRectangle:
            fillColour1Label.setVisible(true);   fillColour1Button.setVisible(true);
            fillColour2Label.setVisible(true);   fillColour2Button.setVisible(true);
            gradientDirLabel.setVisible(true);   gradientDirCombo.setVisible(true);
            cornerRadiusLabel.setVisible(true);  cornerRadiusSlider.setVisible(true);
            strokeColourLabel.setVisible(true);  strokeColourButton.setVisible(true);
            strokeWidthLabel.setVisible(true);   strokeWidthSlider.setVisible(true);
            strokeAlignLabel.setVisible(true);   strokeAlignCombo.setVisible(true);
            lineCapLabel.setVisible(true);       lineCapCombo.setVisible(true);
            itemBgLabel.setVisible(true);        itemBgButton.setVisible(true);
            frostedGlassToggle.setVisible(true);
            blurRadiusLabel.setVisible(true);    blurRadiusSlider.setVisible(true);
            frostTintLabel.setVisible(true);     frostTintButton.setVisible(true);
            frostOpacityLabel.setVisible(true);  frostOpacitySlider.setVisible(true);
            // Hide common controls that don't apply to shapes
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            break;

        case MeterType::ShapeEllipse:
            fillColour1Label.setVisible(true);   fillColour1Button.setVisible(true);
            fillColour2Label.setVisible(true);   fillColour2Button.setVisible(true);
            gradientDirLabel.setVisible(true);   gradientDirCombo.setVisible(true);
            strokeColourLabel.setVisible(true);  strokeColourButton.setVisible(true);
            strokeWidthLabel.setVisible(true);   strokeWidthSlider.setVisible(true);
            strokeAlignLabel.setVisible(true);   strokeAlignCombo.setVisible(true);
            lineCapLabel.setVisible(true);       lineCapCombo.setVisible(true);
            itemBgLabel.setVisible(true);        itemBgButton.setVisible(true);
            frostedGlassToggle.setVisible(true);
            blurRadiusLabel.setVisible(true);    blurRadiusSlider.setVisible(true);
            frostTintLabel.setVisible(true);     frostTintButton.setVisible(true);
            frostOpacityLabel.setVisible(true);  frostOpacitySlider.setVisible(true);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            break;

        case MeterType::ShapeTriangle:
            fillColour1Label.setVisible(true);   fillColour1Button.setVisible(true);
            fillColour2Label.setVisible(true);   fillColour2Button.setVisible(true);
            gradientDirLabel.setVisible(true);   gradientDirCombo.setVisible(true);
            strokeColourLabel.setVisible(true);  strokeColourButton.setVisible(true);
            strokeWidthLabel.setVisible(true);   strokeWidthSlider.setVisible(true);
            strokeAlignLabel.setVisible(true);   strokeAlignCombo.setVisible(true);
            lineCapLabel.setVisible(true);       lineCapCombo.setVisible(true);
            triRoundLabel.setVisible(true);      triRoundSlider.setVisible(true);
            itemBgLabel.setVisible(true);        itemBgButton.setVisible(true);
            frostedGlassToggle.setVisible(true);
            blurRadiusLabel.setVisible(true);    blurRadiusSlider.setVisible(true);
            frostTintLabel.setVisible(true);     frostTintButton.setVisible(true);
            frostOpacityLabel.setVisible(true);  frostOpacitySlider.setVisible(true);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            break;

        case MeterType::ShapeStar:
            fillColour1Label.setVisible(true);   fillColour1Button.setVisible(true);
            fillColour2Label.setVisible(true);   fillColour2Button.setVisible(true);
            gradientDirLabel.setVisible(true);   gradientDirCombo.setVisible(true);
            strokeColourLabel.setVisible(true);  strokeColourButton.setVisible(true);
            strokeWidthLabel.setVisible(true);   strokeWidthSlider.setVisible(true);
            strokeAlignLabel.setVisible(true);   strokeAlignCombo.setVisible(true);
            lineCapLabel.setVisible(true);       lineCapCombo.setVisible(true);
            starPointsLabel.setVisible(true);    starPointsSlider.setVisible(true);
            itemBgLabel.setVisible(true);        itemBgButton.setVisible(true);
            frostedGlassToggle.setVisible(true);
            blurRadiusLabel.setVisible(true);    blurRadiusSlider.setVisible(true);
            frostTintLabel.setVisible(true);     frostTintButton.setVisible(true);
            frostOpacityLabel.setVisible(true);  frostOpacitySlider.setVisible(true);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            break;

        case MeterType::ShapeLine:
            strokeColourLabel.setVisible(true);  strokeColourButton.setVisible(true);
            strokeWidthLabel.setVisible(true);   strokeWidthSlider.setVisible(true);
            strokeAlignLabel.setVisible(true);   strokeAlignCombo.setVisible(true);
            lineCapLabel.setVisible(true);       lineCapCombo.setVisible(true);
            itemBgLabel.setVisible(true);        itemBgButton.setVisible(true);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            break;

        case MeterType::ShapeSVG:
            svgFileButton.setVisible(true);      svgPathLabel.setVisible(true);
            fillColour1Label.setVisible(true);   fillColour1Button.setVisible(true);
            fillColour2Label.setVisible(true);   fillColour2Button.setVisible(true);
            gradientDirLabel.setVisible(true);   gradientDirCombo.setVisible(true);
            strokeColourLabel.setVisible(true);  strokeColourButton.setVisible(true);
            strokeWidthLabel.setVisible(true);   strokeWidthSlider.setVisible(true);
            strokeAlignLabel.setVisible(true);   strokeAlignCombo.setVisible(true);
            lineCapLabel.setVisible(true);       lineCapCombo.setVisible(true);
            itemBgLabel.setVisible(true);        itemBgButton.setVisible(true);
            frostedGlassToggle.setVisible(true);
            blurRadiusLabel.setVisible(true);    blurRadiusSlider.setVisible(true);
            frostTintLabel.setVisible(true);     frostTintButton.setVisible(true);
            frostOpacityLabel.setVisible(true);  frostOpacitySlider.setVisible(true);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            break;

        case MeterType::TextLabel:
            textContentLabel.setVisible(true);   textContentEditor.setVisible(true);
            textFontLabel.setVisible(true);      textFontCombo.setVisible(true);
            textSizeLabel.setVisible(true);      textSizeSlider.setVisible(true);
            textBoldToggle.setVisible(true);
            textItalicToggle.setVisible(true);
            textColourLabel.setVisible(true);    textColourButton.setVisible(true);
            textAlignLabel.setVisible(true);     textAlignCombo.setVisible(true);
            fillColour1Label.setVisible(true);   fillColour1Button.setVisible(true);
            fillColour2Label.setVisible(true);   fillColour2Button.setVisible(true);
            gradientDirLabel.setVisible(true);   gradientDirCombo.setVisible(true);
            cornerRadiusLabel.setVisible(true);  cornerRadiusSlider.setVisible(true);
            strokeColourLabel.setVisible(true);  strokeColourButton.setVisible(true);
            strokeWidthLabel.setVisible(true);   strokeWidthSlider.setVisible(true);
            itemBgLabel.setVisible(true);        itemBgButton.setVisible(true);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            break;

        case MeterType::CustomPlugin:
        {
            // Build dynamic controls from the plugin's property descriptors
            audioFileButton.setVisible(false);   audioPathLabel.setVisible(false);
            fontSizeLabel.setVisible(false);     fontSizeSlider.setVisible(false);
            fontFamilyLabel.setVisible(false);   fontFamilyCombo.setVisible(false);

            auto sel = model.getSelectedItems();
            if (!sel.empty() && sel.front()->component)
            {
                auto* cpc = dynamic_cast<CustomPluginComponent*>(sel.front()->component.get());
                if (cpc)
                    buildDynamicControls(cpc->getPluginProperties(), cpc->getInstanceId());
            }
            break;
        }

        default:
            break;
    }

    resized();
}

//==============================================================================
void MeterSettingsPanel::refresh()
{
    auto sel = model.getSelectedItems();
    if (sel.empty())
    {
        showControlsForType(MeterType::NumTypes);
        setEnabled(false);
        return;
    }

    setEnabled(true);
    auto* item = sel.front();

    if (item->meterType != currentType)
        showControlsForType(item->meterType);

    // Read current values from the component where possible
    if (!item->component) return;

    switch (item->meterType)
    {
        case MeterType::MultiBandAnalyzer:
            // Settings are live; sliders keep their last-set values
            break;

        case MeterType::Spectrogram:
            break;

        case MeterType::Goniometer:
            break;

        case MeterType::LoudnessMeter:
        {
            auto* m = dynamic_cast<LoudnessMeter*>(item->component.get());
            if (m)
            {
                float target = m->getTargetLUFS();
                if      (target > -15.0f) targetLufsCombo.setSelectedId(1, juce::dontSendNotification); // -14
                else if (target > -23.5f) targetLufsCombo.setSelectedId(2, juce::dontSendNotification); // -23
                else if (target > -24.5f) targetLufsCombo.setSelectedId(3, juce::dontSendNotification); // -24
                else                      targetLufsCombo.setSelectedId(4, juce::dontSendNotification); // -16
                showHistoryToggle.setToggleState(m->getShowHistory(), juce::dontSendNotification);
            }
            break;
        }

        case MeterType::PeakMeter:
            break;

        case MeterType::ImageLayer:
        case MeterType::VideoLayer:
        {
            if (item->mediaFilePath.isNotEmpty())
                mediaPathLabel.setText(juce::File(item->mediaFilePath).getFileName(), juce::dontSendNotification);
            else
                mediaPathLabel.setText("(no file)", juce::dontSendNotification);
            break;
        }

        case MeterType::ShapeRectangle:
        case MeterType::ShapeEllipse:
        case MeterType::ShapeTriangle:
        case MeterType::ShapeLine:
        case MeterType::ShapeStar:
        case MeterType::ShapeSVG:
        {
            // Populate controls from item properties
            fillColour1Button.setColour(juce::TextButton::buttonColourId, item->fillColour1);
            fillColour2Button.setColour(juce::TextButton::buttonColourId, item->fillColour2);
            gradientDirCombo.setSelectedId(item->gradientDirection + 1, juce::dontSendNotification);
            cornerRadiusSlider.setValue(item->cornerRadius, juce::dontSendNotification);
            strokeColourButton.setColour(juce::TextButton::buttonColourId, item->strokeColour);
            strokeWidthSlider.setValue(item->strokeWidth, juce::dontSendNotification);
            strokeAlignCombo.setSelectedId(item->strokeAlignment + 1, juce::dontSendNotification);
            lineCapCombo.setSelectedId(item->lineCap + 1, juce::dontSendNotification);
            starPointsSlider.setValue(item->starPoints, juce::dontSendNotification);
            triRoundSlider.setValue(item->triangleRoundness * 100.0f, juce::dontSendNotification);
            itemBgButton.setColour(juce::TextButton::buttonColourId, item->itemBackground);

            // SVG file path
            if (item->meterType == MeterType::ShapeSVG)
            {
                if (item->svgFilePath.isNotEmpty())
                    svgPathLabel.setText(juce::File(item->svgFilePath).getFileName(), juce::dontSendNotification);
                else
                    svgPathLabel.setText("(no file)", juce::dontSendNotification);
            }

            // Frosted glass
            frostedGlassToggle.setToggleState(item->frostedGlass, juce::dontSendNotification);
            blurRadiusSlider.setValue(item->blurRadius, juce::dontSendNotification);
            frostTintButton.setColour(juce::TextButton::buttonColourId, item->frostTint);
            frostOpacitySlider.setValue(item->frostOpacity * 100.0f, juce::dontSendNotification);
            break;
        }

        case MeterType::TextLabel:
        {
            textContentEditor.setText(item->textContent, false);
            // Find font combo id
            for (int i = 0; i < textFontCombo.getNumItems(); ++i)
                if (textFontCombo.getItemText(i) == item->fontFamily)
                { textFontCombo.setSelectedItemIndex(i, juce::dontSendNotification); break; }
            textSizeSlider.setValue(item->fontSize, juce::dontSendNotification);
            textBoldToggle.setToggleState(item->fontBold, juce::dontSendNotification);
            textItalicToggle.setToggleState(item->fontItalic, juce::dontSendNotification);
            textColourButton.setColour(juce::TextButton::buttonColourId, item->textColour);
            textAlignCombo.setSelectedId(item->textAlignment + 1, juce::dontSendNotification);
            fillColour1Button.setColour(juce::TextButton::buttonColourId, item->fillColour1);
            fillColour2Button.setColour(juce::TextButton::buttonColourId, item->fillColour2);
            gradientDirCombo.setSelectedId(item->gradientDirection + 1, juce::dontSendNotification);
            cornerRadiusSlider.setValue(item->cornerRadius, juce::dontSendNotification);
            strokeColourButton.setColour(juce::TextButton::buttonColourId, item->strokeColour);
            strokeWidthSlider.setValue(item->strokeWidth, juce::dontSendNotification);
            strokeAlignCombo.setSelectedId(item->strokeAlignment + 1, juce::dontSendNotification);
            lineCapCombo.setSelectedId(item->lineCap + 1, juce::dontSendNotification);
            itemBgButton.setColour(juce::TextButton::buttonColourId, item->itemBackground);
            break;
        }

        default:
            break;
    }

    // ── Populate font controls for any item ──────────────────────────────
    fontSizeSlider.setValue(item->fontSize, juce::dontSendNotification);
    {
        bool found = false;
        for (int i = 0; i < fontFamilyCombo.getNumItems(); ++i)
            if (fontFamilyCombo.getItemText(i) == item->fontFamily)
            { fontFamilyCombo.setSelectedItemIndex(i, juce::dontSendNotification); found = true; break; }
        if (!found)
            fontFamilyCombo.setSelectedId(1, juce::dontSendNotification);  // "Default"
    }
}

//==============================================================================
// Dynamic custom-plugin property controls
//==============================================================================

void MeterSettingsPanel::clearDynamicControls()
{
    for (auto* dc : dynControls_)
    {
        if (dc->label)    content_.removeChildComponent(dc->label.get());
        if (dc->slider)   content_.removeChildComponent(dc->slider.get());
        if (dc->toggle)   content_.removeChildComponent(dc->toggle.get());
        if (dc->combo)    content_.removeChildComponent(dc->combo.get());
        if (dc->colorBtn) content_.removeChildComponent(dc->colorBtn.get());
    }
    dynControls_.clear();
}

void MeterSettingsPanel::buildDynamicControls(
    const std::vector<CustomPluginProperty>& props,
    const juce::String& instanceId)
{
    clearDynamicControls();

    // Helper: also persist the value into CustomPluginComponent so export picks it up
    auto syncPropToComponent = [this](const juce::String& key, const juce::var& val)
    {
        auto sel = model.getSelectedItems();
        if (!sel.empty() && sel.front()->meterType == MeterType::CustomPlugin)
            if (auto* cpc = dynamic_cast<CustomPluginComponent*>(sel.front()->component.get()))
                cpc->updatePropertyValue(key, val);
    };

    for (auto& p : props)
    {
        auto* dc = dynControls_.add(new DynControl());
        dc->key = p.key;

        // Label
        dc->label = std::make_unique<juce::Label>(juce::String(), p.label);
        styleLabel(*dc->label);
        content_.addAndMakeVisible(*dc->label);

        if (p.type.equalsIgnoreCase("FLOAT") || p.type.equalsIgnoreCase("INT"))
        {
            dc->slider = std::make_unique<juce::Slider>();
            double step = (double)p.step;
            if (step <= 0.0)
                step = p.type.equalsIgnoreCase("INT") ? 1.0 : 0.01;
            styleSlider(*dc->slider, (double)p.minVal, (double)p.maxVal, step, (double)p.defaultVal);
            if (p.type.equalsIgnoreCase("INT"))
                dc->slider->setNumDecimalPlacesToDisplay(0);

            auto capturedKey = p.key;
            auto capturedInstanceId = instanceId;
            dc->slider->onValueChange = [capturedKey, capturedInstanceId, s = dc->slider.get(), syncPropToComponent]()
            {
                auto val = juce::var(s->getValue());
                auto& bridge = PythonPluginBridge::getInstance();
                if (bridge.isRunning())
                    bridge.setProperty(capturedInstanceId, capturedKey, val);
                syncPropToComponent(capturedKey, val);
            };
            content_.addAndMakeVisible(*dc->slider);
        }
        else if (p.type.equalsIgnoreCase("BOOL"))
        {
            dc->toggle = std::make_unique<juce::ToggleButton>(p.label);
            styleToggle(*dc->toggle);
            dc->toggle->setToggleState((bool)p.defaultVal, juce::dontSendNotification);

            auto capturedKey = p.key;
            auto capturedInstanceId = instanceId;
            dc->toggle->onClick = [capturedKey, capturedInstanceId, t = dc->toggle.get(), syncPropToComponent]()
            {
                auto val = juce::var(t->getToggleState());
                auto& bridge = PythonPluginBridge::getInstance();
                if (bridge.isRunning())
                    bridge.setProperty(capturedInstanceId, capturedKey, val);
                syncPropToComponent(capturedKey, val);
            };
            content_.addAndMakeVisible(*dc->toggle);
        }
        else if (p.type.equalsIgnoreCase("ENUM"))
        {
            dc->combo = std::make_unique<juce::ComboBox>();
            styleCombo(*dc->combo);
            int idx = 1;
            for (auto& [val, name] : p.choices)
                dc->combo->addItem(name, idx++);
            dc->combo->setSelectedId(1, juce::dontSendNotification);

            auto capturedKey = p.key;
            auto capturedInstanceId = instanceId;
            auto choices = p.choices;
            dc->combo->onChange = [capturedKey, capturedInstanceId, choices, c = dc->combo.get(), syncPropToComponent]()
            {
                int sel = c->getSelectedId() - 1;
                if (sel >= 0 && sel < (int)choices.size())
                {
                    auto& bridge = PythonPluginBridge::getInstance();
                    if (bridge.isRunning())
                        bridge.setProperty(capturedInstanceId, capturedKey, choices[sel].first);
                    syncPropToComponent(capturedKey, choices[sel].first);
                }
            };
            content_.addAndMakeVisible(*dc->combo);
        }
        else if (p.type.equalsIgnoreCase("COLOR"))
        {
            dc->colorBtn = std::make_unique<juce::TextButton>();
            dc->colorBtn->setButtonText("");

            // Parse default colour — Python sends ARGB integer via _serialise_value
            juce::Colour defCol = juce::Colour(0xFF3A7BFF);
            if (p.defaultVal.isInt() || p.defaultVal.isInt64())
                defCol = juce::Colour((juce::uint32)(juce::int64)p.defaultVal);
            else if (p.defaultVal.isString())
            {
                auto hexStr = p.defaultVal.toString().trimCharactersAtStart("#");
                defCol = juce::Colour((juce::uint32)hexStr.getHexValue64());
            }
            dc->colorBtn->setColour(juce::TextButton::buttonColourId, defCol);

            auto capturedKey = p.key;
            auto capturedInstanceId = instanceId;
            dc->colorBtn->onClick = [this, capturedKey, capturedInstanceId, btn = dc->colorBtn.get(), syncPropToComponent]()
            {
                auto picker = std::make_unique<ColourPickerWithEyedropper>();
                picker->setCurrentColour(btn->findColour(juce::TextButton::buttonColourId));
                picker->setSize(300, 430);
                picker->onColourChanged = [btn, capturedKey, capturedInstanceId, syncPropToComponent](juce::Colour c)
                {
                    btn->setColour(juce::TextButton::buttonColourId, c);
                    btn->repaint();

                    // Send ARGB integer to Python — Property.validate() handles int → Color
                    auto val = juce::var((juce::int64)c.getARGB());
                    auto& bridge = PythonPluginBridge::getInstance();
                    if (bridge.isRunning())
                        bridge.setProperty(capturedInstanceId, capturedKey, val);
                    syncPropToComponent(capturedKey, val);
                };
                juce::CallOutBox::launchAsynchronously(
                    std::move(picker), btn->getScreenBounds(), nullptr);
            };
            content_.addAndMakeVisible(*dc->colorBtn);
        }
    }

    resized();
}
//==============================================================================
void MeterSettingsPanel::applySettingsToItem(CanvasItem* item)
{
    if (!item || !item->component) return;

    auto* comp = item->component.get();

    switch (item->meterType)
    {
        case MeterType::MultiBandAnalyzer:
        {
            auto* m = dynamic_cast<MultiBandAnalyzer*>(comp);
            if (!m) break;
            m->setDecayRate(static_cast<float>(smoothingSlider.getValue()));
            m->setDynamicRange(static_cast<float>(minDbSlider.getValue()),
                               static_cast<float>(maxDbSlider.getValue()));

            // Bands
            int bandsId = numBandsCombo.getSelectedId();
            int bands[] = { 8, 16, 31, 64 };
            if (bandsId >= 1 && bandsId <= 4)
                m->setNumBands(bands[bandsId - 1]);

            // Scale mode
            int scaleId = scaleModeCombo.getSelectedId();
            if (scaleId == 1) m->setScaleMode(MultiBandAnalyzer::ScaleMode::Logarithmic);
            else if (scaleId == 2) m->setScaleMode(MultiBandAnalyzer::ScaleMode::Linear);
            else if (scaleId == 3) m->setScaleMode(MultiBandAnalyzer::ScaleMode::Octave);
            break;
        }

        case MeterType::Spectrogram:
        {
            auto* m = dynamic_cast<Spectrogram*>(comp);
            if (!m) break;
            m->setDynamicRange(static_cast<float>(minDbSlider.getValue()),
                               static_cast<float>(maxDbSlider.getValue()));

            int cmId = colourMapCombo.getSelectedId();
            if (cmId == 1) m->setColourMap(Spectrogram::ColourMap::Rainbow);
            else if (cmId == 2) m->setColourMap(Spectrogram::ColourMap::Heat);
            else if (cmId == 3) m->setColourMap(Spectrogram::ColourMap::Greyscale);
            else if (cmId == 4) m->setColourMap(Spectrogram::ColourMap::Custom);

            int sdId = scrollDirCombo.getSelectedId();
            if (sdId == 1) m->setScrollDirection(Spectrogram::ScrollDirection::Horizontal);
            else if (sdId == 2) m->setScrollDirection(Spectrogram::ScrollDirection::Vertical);
            break;
        }

        case MeterType::Goniometer:
        {
            auto* m = dynamic_cast<Goniometer*>(comp);
            if (!m) break;
            m->setDotSize(static_cast<float>(dotSizeSlider.getValue()));
            m->setTrailOpacity(static_cast<float>(trailSlider.getValue()));
            m->setShowGrid(showGridToggle.getToggleState());
            break;
        }

        case MeterType::LissajousScope:
        {
            auto* m = dynamic_cast<LissajousScope*>(comp);
            if (!m) break;
            int modeId = lissajousModeCombo.getSelectedId();
            if (modeId == 1) m->setMode(LissajousScope::Mode::Lissajous);
            else if (modeId == 2) m->setMode(LissajousScope::Mode::XY);
            else if (modeId == 3) m->setMode(LissajousScope::Mode::Polar);
            m->setTrailLength(static_cast<int>(trailSlider.getValue() * 1000.0));
            m->setShowGrid(showGridToggle.getToggleState());
            break;
        }

        case MeterType::LoudnessMeter:
        {
            auto* m = dynamic_cast<LoudnessMeter*>(comp);
            if (!m) break;
            float targets[] = { -14.0f, -23.0f, -24.0f, -16.0f };
            int tId = targetLufsCombo.getSelectedId();
            if (tId >= 1 && tId <= 4)
            {
                m->setTargetLUFS(targets[tId - 1]);
                item->targetLUFS = targets[tId - 1];
            }
            m->setShowHistory(showHistoryToggle.getToggleState());
            item->loudnessShowHistory = showHistoryToggle.getToggleState();
            break;
        }

        case MeterType::LevelHistogram:
        {
            auto* m = dynamic_cast<LevelHistogram*>(comp);
            if (!m) break;
            m->setDisplayRange(static_cast<float>(minDbSlider.getValue()),
                               static_cast<float>(maxDbSlider.getValue()));
            m->setBinResolution(static_cast<float>(binResSlider.getValue()));
            m->setCumulative(cumulativeToggle.getToggleState());
            m->setShowStereo(showStereoToggle.getToggleState());
            break;
        }

        case MeterType::CorrelationMeter:
        {
            auto* m = dynamic_cast<CorrelationMeter*>(comp);
            if (!m) break;
            m->setIntegrationTimeMs(static_cast<float>(integrationSlider.getValue()));
            m->setShowNumeric(showNumericToggle.getToggleState());
            break;
        }

        case MeterType::PeakMeter:
        {
            auto* m = dynamic_cast<PeakMeter*>(comp);
            if (!m) break;
            int pmId = peakModeCombo.getSelectedId();
            if (pmId == 1) m->setPeakMode(PeakMeter::PeakMode::SamplePeak);
            else if (pmId == 2) m->setPeakMode(PeakMeter::PeakMode::TruePeak);
            m->setPeakHoldTimeMs(static_cast<float>(peakHoldSlider.getValue()));
            m->setDecayRateDbPerSec(static_cast<float>(smoothingSlider.getValue()));
            m->setShowClipWarning(clipWarningToggle.getToggleState());
            break;
        }

        case MeterType::SkinnedVUMeter:
        {
            auto* m = dynamic_cast<SkinnedVUMeter*>(comp);
            if (!m) break;
            int bId = ballisticCombo.getSelectedId();
            if (bId == 1) m->setBallistic(SkinnedVUMeter::Ballistic::VU);
            else if (bId == 2) m->setBallistic(SkinnedVUMeter::Ballistic::PPM_I);
            else if (bId == 3) m->setBallistic(SkinnedVUMeter::Ballistic::PPM_II);
            else if (bId == 4) m->setBallistic(SkinnedVUMeter::Ballistic::Nordic);
            m->setDecayTimeMs(static_cast<float>(decayTimeSlider.getValue()));

            // Channel selection (L=0, R=1)
            int chId = vuChannelCombo.getSelectedId();
            item->vuChannel = (chId == 2) ? 1 : 0;
            m->setChannelLabel((chId == 2) ? "R" : "L");
            break;
        }

        case MeterType::SkinnedSpectrum:
        {
            auto* m = dynamic_cast<SkinnedSpectrumAnalyzer*>(comp);
            if (!m) break;
            m->setDecayRate(static_cast<float>(smoothingSlider.getValue()));
            int bandsId = numBandsCombo.getSelectedId();
            int bands[] = { 8, 16, 31, 64 };
            if (bandsId >= 1 && bandsId <= 4)
                m->setNumBands(bands[bandsId - 1]);
            break;
        }

        case MeterType::SkinnedOscilloscope:
        {
            auto* m = dynamic_cast<SkinnedOscilloscope*>(comp);
            if (!m) break;
            m->setLineThickness(static_cast<float>(lineThicknessSlider.getValue()));
            m->setDrawStyle(drawStyleCombo.getSelectedId() - 1);
            break;
        }

        case MeterType::ShapeRectangle:
        case MeterType::ShapeEllipse:
        case MeterType::ShapeTriangle:
        case MeterType::ShapeLine:
        case MeterType::ShapeStar:
        case MeterType::ShapeSVG:
        {
            auto* m = dynamic_cast<ShapeComponent*>(comp);
            if (!m) break;

            // Sync CanvasItem -> ShapeComponent
            item->gradientDirection = gradientDirCombo.getSelectedId() - 1; // 0=solid,1=vert,2=horiz,3=diag
            item->cornerRadius = static_cast<float>(cornerRadiusSlider.getValue());
            item->strokeWidth = static_cast<float>(strokeWidthSlider.getValue());
            item->strokeAlignment = strokeAlignCombo.getSelectedId() - 1;   // 0=center,1=inside,2=outside
            item->lineCap = lineCapCombo.getSelectedId() - 1;               // 0=butt,1=round,2=square
            item->starPoints = static_cast<int>(starPointsSlider.getValue());
            item->triangleRoundness = static_cast<float>(triRoundSlider.getValue()) / 100.0f;

            m->setFillColour1(item->fillColour1);
            m->setFillColour2(item->fillColour2);
            m->setGradientDirection(item->gradientDirection);
            m->setCornerRadius(item->cornerRadius);
            m->setStrokeColour(item->strokeColour);
            m->setStrokeWidth(item->strokeWidth);
            m->setStrokeAlignment(static_cast<StrokeAlignment>(item->strokeAlignment));
            m->setLineCap(static_cast<LineCap>(item->lineCap));
            m->setStarPoints(item->starPoints);
            m->setTriangleRoundness(item->triangleRoundness);
            m->setItemBackground(item->itemBackground);

            // Frosted glass
            item->frostedGlass = frostedGlassToggle.getToggleState();
            item->blurRadius   = static_cast<float>(blurRadiusSlider.getValue());
            item->frostOpacity = static_cast<float>(frostOpacitySlider.getValue()) / 100.0f;

            m->setFrostedGlass(item->frostedGlass);
            m->setBlurRadius(item->blurRadius);
            m->setFrostTint(item->frostTint);
            m->setFrostOpacity(item->frostOpacity);
            break;
        }

        case MeterType::TextLabel:
        {
            auto* m = dynamic_cast<TextLabelComponent*>(comp);
            if (!m) break;

            // Text properties
            item->textContent = textContentEditor.getText();
            int fontId = textFontCombo.getSelectedId();
            if (fontId > 0)
                item->fontFamily = textFontCombo.getItemText(textFontCombo.indexOfItemId(fontId));
            item->fontSize = static_cast<float>(textSizeSlider.getValue());
            item->fontBold = textBoldToggle.getToggleState();
            item->fontItalic = textItalicToggle.getToggleState();
            item->textAlignment = textAlignCombo.getSelectedId() - 1;

            // Shape/box properties
            item->gradientDirection = gradientDirCombo.getSelectedId() - 1;
            item->cornerRadius = static_cast<float>(cornerRadiusSlider.getValue());
            item->strokeWidth = static_cast<float>(strokeWidthSlider.getValue());

            m->setTextContent(item->textContent);
            m->setFontFamily(item->fontFamily);
            m->setFontSize(item->fontSize);
            m->setBold(item->fontBold);
            m->setItalic(item->fontItalic);
            m->setTextColour(item->textColour);
            m->setTextAlignment(item->textAlignment);
            m->setFillColour1(item->fillColour1);
            m->setFillColour2(item->fillColour2);
            m->setGradientDirection(item->gradientDirection);
            m->setCornerRadius(item->cornerRadius);
            m->setStrokeColour(item->strokeColour);
            m->setStrokeWidth(item->strokeWidth);
            m->setItemBackground(item->itemBackground);
            break;
        }

        default:
            break;
    }

    // ── Apply font settings to any MeterBase-derived component ──────────
    if (auto* mb = dynamic_cast<MeterBase*>(comp))
    {
        float fs = static_cast<float>(fontSizeSlider.getValue());
        item->fontSize = fs;
        mb->setMeterFontSize(fs);

        int fontId = fontFamilyCombo.getSelectedId();
        if (fontId > 0)
        {
            juce::String family = fontFamilyCombo.getItemText(fontFamilyCombo.indexOfItemId(fontId));
            item->fontFamily = family;
            mb->setMeterFontFamily(family);
        }

        if (auto* c = dynamic_cast<juce::Component*>(comp))
            c->repaint();
    }
}
