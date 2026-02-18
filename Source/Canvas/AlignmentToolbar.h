#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"

//==============================================================================
/// Toolbar with alignment and distribution buttons.
class AlignmentToolbar : public juce::Component,
                         private CanvasModelListener
{
public:
    explicit AlignmentToolbar(CanvasModel& model);
    ~AlignmentToolbar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void applyThemeColours();

    /// Callback fired when the freeze (render-preview) button is clicked.
    std::function<void()> onFreezeClicked;

private:
    CanvasModel& model;

    juce::TextButton alignLeft   { "Left" };
    juce::TextButton alignCenterH{ "CenterH" };
    juce::TextButton alignRight  { "Right" };
    juce::TextButton alignTop    { "Top" };
    juce::TextButton alignCenterV{ "CenterV" };
    juce::TextButton alignBottom { "Bottom" };
    juce::TextButton distH       { "DistH" };
    juce::TextButton distV       { "DistV" };

    // Grid controls
    juce::ToggleButton gridToggle { "Grid" };
    juce::ComboBox gridSizeCombo;
    juce::Label zoomLabel;

    // Freeze (render preview) button — snowflake icon
    juce::DrawableButton freezeButton { "Freeze", juce::DrawableButton::ImageFitted };
    std::unique_ptr<juce::Drawable> snowflakeIcon;

    void styleButton(juce::TextButton& b);
    void buildSnowflakeIcon();

    // CanvasModelListener
    void zoomPanChanged() override { repaint(); }
};
