#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"
#include "../UI/FontAwesomeIcons.h"
#include "../ThreeD/WorkflowMode.h"

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

    /// Callback fired when the user switches workflow mode.
    std::function<void(WorkflowMode)> onModeChanged;

    /// Programmatically set the active mode (does NOT fire onModeChanged).
    void setMode(WorkflowMode mode);
    WorkflowMode getMode() const { return currentMode_; }

private:
    CanvasModel& model;
    WorkflowMode currentMode_ = WorkflowMode::Mode2D;

    FontAwesomeIcons::FAIconButton alignLeft    { "alignLeft",    FontAwesomeIcons::alignLeftIcon()    };
    FontAwesomeIcons::FAIconButton alignCenterH { "alignCenterH", FontAwesomeIcons::alignCenterHIcon() };
    FontAwesomeIcons::FAIconButton alignRight   { "alignRight",   FontAwesomeIcons::alignRightIcon()   };
    FontAwesomeIcons::FAIconButton alignTop     { "alignTop",     FontAwesomeIcons::alignTopIcon()     };
    FontAwesomeIcons::FAIconButton alignCenterV { "alignCenterV", FontAwesomeIcons::alignCenterVIcon() };
    FontAwesomeIcons::FAIconButton alignBottom  { "alignBottom",  FontAwesomeIcons::alignBottomIcon()  };
    FontAwesomeIcons::FAIconButton distH        { "distH",        FontAwesomeIcons::distributeHIcon()  };
    FontAwesomeIcons::FAIconButton distV        { "distV",        FontAwesomeIcons::distributeVIcon()  };

    // Grid controls
    juce::ToggleButton gridToggle { "Grid" };
    juce::ComboBox gridSizeCombo;
    juce::Label zoomLabel;

    // Freeze (render preview) button — snowflake icon
    juce::DrawableButton freezeButton { "Freeze", juce::DrawableButton::ImageFitted };
    std::unique_ptr<juce::Drawable> snowflakeIcon;

    // 2D / 3D / VJ workflow mode toggle buttons
    juce::TextButton mode2DBtn_ { "2D" };
    juce::TextButton mode3DBtn_ { "3D" };
    juce::TextButton modeVJBtn_ { "VJ" };

    void styleButton(juce::Button& b);
    void buildSnowflakeIcon();

    // CanvasModelListener
    void zoomPanChanged() override { repaint(); }
};
