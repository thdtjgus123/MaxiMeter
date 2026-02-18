#pragma once

#include <JuceHeader.h>
#include "../Canvas/CanvasModel.h"
#include "../Canvas/CanvasItem.h"

//==============================================================================
/// Right-side panel showing meter-type-specific settings for the selected item.
/// Dynamically builds controls based on MeterType:
///   - Skin meters: skin file path selector
///   - Audio meters: audio source, smoothing/decay
///   - All: font size, colours, display options, etc.
class MeterSettingsPanel : public juce::Component,
                           public CanvasModelListener
{
public:
    explicit MeterSettingsPanel(CanvasModel& model);
    ~MeterSettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    /// Callback fired when the user drags the top-edge divider.
    /// Parameter: delta Y (positive = boundary moves down = settings shrinks)
    std::function<void(int deltaY)> onDividerDragged;

    // ── Scrollable viewport ──
    juce::Viewport viewport_;
    class ContentComp : public juce::Component {
    public: void paint(juce::Graphics&) override {} };
    ContentComp content_;

    // CanvasModelListener
    void selectionChanged() override { refresh(); }
    void itemsChanged() override    { refresh(); }

    /// Refresh displayed settings for the current selection.
    void refresh();

    //-- Callbacks set by CanvasEditor to load skin/audio --
    std::function<void(const juce::File&)> onSkinFileSelected;
    std::function<void(const juce::File&)> onAudioFileSelected;
    std::function<void(CanvasItem*, const juce::File&)> onMediaFileSelected;

private:
    CanvasModel& model;

    // ── Header ──
    juce::Label titleLabel;
    juce::Label meterTypeLabel;

    // ── Common Settings ──
    juce::Label         fontSizeLabel      { {}, "Font Size" };
    juce::Slider        fontSizeSlider;
    juce::Label         fontFamilyLabel    { {}, "Font" };
    juce::ComboBox      fontFamilyCombo;

    // ── Audio / Analysis Settings ──
    juce::Label         smoothingLabel     { {}, "Smoothing" };
    juce::Slider        smoothingSlider;     // decay rate dB/s
    juce::Label         dynamicRangeLabel  { {}, "Range (dB)" };
    juce::Slider        minDbSlider;
    juce::Slider        maxDbSlider;

    // ── Spectrum Settings ──
    juce::Label         numBandsLabel      { {}, "Bands" };
    juce::ComboBox      numBandsCombo;
    juce::Label         scaleModeLabel     { {}, "Scale" };
    juce::ComboBox      scaleModeCombo;

    // ── Spectrogram Settings ──
    juce::Label         colourMapLabel     { {}, "Colour Map" };
    juce::ComboBox      colourMapCombo;
    juce::Label         scrollDirLabel     { {}, "Scroll" };
    juce::ComboBox      scrollDirCombo;

    // ── Goniometer / Lissajous Settings ──
    juce::Label         dotSizeLabel       { {}, "Dot Size" };
    juce::Slider        dotSizeSlider;
    juce::Label         trailLabel         { {}, "Trail" };
    juce::Slider        trailSlider;
    juce::ToggleButton  showGridToggle     { "Show Grid" };

    // ── Loudness Settings ──
    juce::Label         targetLufsLabel    { {}, "Target LUFS" };
    juce::ComboBox      targetLufsCombo;
    juce::ToggleButton  showHistoryToggle  { "Show History" };

    // ── Peak Meter Settings ──
    juce::Label         peakModeLabel      { {}, "Peak Mode" };
    juce::ComboBox      peakModeCombo;
    juce::Label         peakHoldLabel      { {}, "Peak Hold (ms)" };
    juce::Slider        peakHoldSlider;
    juce::ToggleButton  clipWarningToggle  { "Clip Warning" };

    // ── Correlation Settings ──
    juce::Label         integrationLabel   { {}, "Integration (ms)" };
    juce::Slider        integrationSlider;
    juce::ToggleButton  showNumericToggle  { "Show Value" };

    // ── Histogram Settings ──
    juce::Label         binResLabel        { {}, "Bin Res (dB)" };
    juce::Slider        binResSlider;
    juce::ToggleButton  cumulativeToggle   { "Cumulative" };
    juce::ToggleButton  showStereoToggle   { "Stereo" };

    // ── Skinned VU Settings ──
    juce::Label         ballisticLabel     { {}, "Ballistic" };
    juce::ComboBox      ballisticCombo;
    juce::Label         decayTimeLabel     { {}, "Decay (ms)" };
    juce::Slider        decayTimeSlider;
    juce::Label         vuChannelLabel     { {}, "Channel" };
    juce::ComboBox      vuChannelCombo;

    // ── Skinned Oscilloscope ──
    juce::Label         lineThicknessLabel { {}, "Line Width" };
    juce::Slider        lineThicknessSlider;
    juce::Label         drawStyleLabel     { {}, "Draw Style" };
    juce::ComboBox      drawStyleCombo;

    // ── Lissajous Scope Mode ──
    juce::Label         lissajousModeLabel { {}, "Mode" };
    juce::ComboBox      lissajousModeCombo;

    // ── Shape Controls ──
    juce::Label         fillColour1Label   { {}, "Fill 1" };
    juce::TextButton    fillColour1Button  { "" };
    juce::Label         fillColour2Label   { {}, "Fill 2" };
    juce::TextButton    fillColour2Button  { "" };
    juce::Label         gradientDirLabel   { {}, "Gradient" };
    juce::ComboBox      gradientDirCombo;
    juce::Label         cornerRadiusLabel  { {}, "Corner R" };
    juce::Slider        cornerRadiusSlider;
    juce::Label         strokeColourLabel  { {}, "Stroke" };
    juce::TextButton    strokeColourButton { "" };
    juce::Label         strokeWidthLabel   { {}, "Stroke W" };
    juce::Slider        strokeWidthSlider;
    juce::Label         strokeAlignLabel   { {}, "Stroke Align" };
    juce::ComboBox      strokeAlignCombo;
    juce::Label         lineCapLabel       { {}, "Line Cap" };
    juce::ComboBox      lineCapCombo;
    juce::Label         starPointsLabel    { {}, "Points" };
    juce::Slider        starPointsSlider;
    juce::Label         triRoundLabel      { {}, "Roundness" };
    juce::Slider        triRoundSlider;
    juce::Label         itemBgLabel        { {}, "Item BG" };
    juce::TextButton    itemBgButton       { "" };

    // ── Frosted Glass Controls ──
    juce::ToggleButton  frostedGlassToggle { "Frosted Glass" };
    juce::Label         blurRadiusLabel    { {}, "Blur" };
    juce::Slider        blurRadiusSlider;
    juce::Label         frostTintLabel     { {}, "Tint" };
    juce::TextButton    frostTintButton    { "" };
    juce::Label         frostOpacityLabel  { {}, "Opacity" };
    juce::Slider        frostOpacitySlider;

    // ── Text Controls ──
    juce::Label         textContentLabel   { {}, "Text" };
    juce::TextEditor    textContentEditor;
    juce::Label         textFontLabel      { {}, "Font" };
    juce::ComboBox      textFontCombo;
    juce::Label         textSizeLabel      { {}, "Size" };
    juce::Slider        textSizeSlider;
    juce::ToggleButton  textBoldToggle     { "Bold" };
    juce::ToggleButton  textItalicToggle   { "Italic" };
    juce::Label         textColourLabel    { {}, "Color" };
    juce::TextButton    textColourButton   { "" };
    juce::Label         textAlignLabel     { {}, "Align" };
    juce::ComboBox      textAlignCombo;

    // ── File Selectors ──
    juce::TextButton    skinFileButton     { "Select Skin (.wsz)..." };
    juce::Label         skinPathLabel;
    juce::TextButton    audioFileButton    { "Select Audio File..." };
    juce::Label         audioPathLabel;
    juce::TextButton    mediaFileButton    { "Import Media File..." };
    juce::Label         mediaPathLabel;
    juce::TextButton    svgFileButton      { "Import SVG..." };
    juce::Label         svgPathLabel;

    // ── Dynamic Custom Plugin Controls ──
    struct DynControl
    {
        juce::String key;     ///< Property key for IPC
        std::unique_ptr<juce::Label>        label;
        std::unique_ptr<juce::Slider>       slider;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::ComboBox>     combo;
        std::unique_ptr<juce::TextButton>   colorBtn;
    };
    juce::OwnedArray<DynControl> dynControls_;
    void buildDynamicControls(const std::vector<struct CustomPluginProperty>& props,
                              const juce::String& instanceId);
    void clearDynamicControls();

    // ── Internal ──
    MeterType currentType = MeterType::NumTypes;

    void hideAllControls();
    void showCommonControls();
    void showControlsForType(MeterType type);
    void applySettingsToItem(CanvasItem* item);
    void layoutContent();

    // Helper to add a labelled row in layout
    struct RowLayout
    {
        juce::Rectangle<int> area;
        int rowH = 24;
        int gap  = 2;
        int labelW = 80;

        juce::Rectangle<int> nextRow()
        {
            auto r = area.removeFromTop(rowH);
            area.removeFromTop(gap);
            return r;
        }

        void labelAndControl(juce::Label& lbl, juce::Component& ctrl)
        {
            auto r = nextRow();
            lbl.setBounds(r.removeFromLeft(labelW));
            ctrl.setBounds(r);
        }

        void singleControl(juce::Component& ctrl, int h = 24)
        {
            auto r = area.removeFromTop(h);
            area.removeFromTop(gap);
            ctrl.setBounds(r);
        }
    };

    void setupLabel(juce::Label& lbl);
    void setupSlider(juce::Slider& s, double min, double max, double step, double def);
    void setupToggle(juce::ToggleButton& btn);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterSettingsPanel)

    static constexpr int kDividerHeight = 5;
    int  dividerDragStartY_ = 0;
    bool draggingDivider_   = false;
};
