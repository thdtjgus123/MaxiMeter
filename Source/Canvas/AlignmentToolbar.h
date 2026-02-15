#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"

//==============================================================================
/// Toolbar with alignment and distribution buttons.
class AlignmentToolbar : public juce::Component
{
public:
    explicit AlignmentToolbar(CanvasModel& model);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void applyThemeColours();

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

    void styleButton(juce::TextButton& b);
};
