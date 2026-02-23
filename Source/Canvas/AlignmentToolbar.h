#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"
#include "../UI/FontAwesomeIcons.h"

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

    /// Callback fired when the user switches between 2D and 3D mode.
    std::function<void(bool is3D)> onModeChanged;

    /// Programmatically set the active mode (does NOT fire onModeChanged).
    void setMode(bool is3D);
    bool isIn3DMode() const { return mode3DBtn_.getToggleState(); }

private:
    CanvasModel& model;

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

    // 2D / 3D workflow mode toggle buttons
    juce::TextButton mode2DBtn_ { "2D" };
    juce::TextButton mode3DBtn_ { "3D" };

    void styleButton(juce::Button& b);
    void buildSnowflakeIcon();

    // CanvasModelListener
    void zoomPanChanged() override { repaint(); }
};
