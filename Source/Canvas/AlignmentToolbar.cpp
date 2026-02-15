#include "AlignmentToolbar.h"
#include "../UI/ThemeManager.h"
// Icons not used here — alignment buttons use text labels

//==============================================================================
AlignmentToolbar::AlignmentToolbar(CanvasModel& m) : model(m)
{
    styleButton(alignLeft);
    styleButton(alignCenterH);
    styleButton(alignRight);
    styleButton(alignTop);
    styleButton(alignCenterV);
    styleButton(alignBottom);
    styleButton(distH);
    styleButton(distV);

    alignLeft.setTooltip("Align Left");
    alignCenterH.setTooltip("Align Center Horizontally");
    alignRight.setTooltip("Align Right");
    alignTop.setTooltip("Align Top");
    alignCenterV.setTooltip("Align Center Vertically");
    alignBottom.setTooltip("Align Bottom");
    distH.setTooltip("Distribute Horizontally");
    distV.setTooltip("Distribute Vertically");

    alignLeft.onClick   = [this] { model.alignSelection(CanvasModel::AlignEdge::Left); };
    alignCenterH.onClick = [this] { model.alignSelection(CanvasModel::AlignEdge::CenterH); };
    alignRight.onClick  = [this] { model.alignSelection(CanvasModel::AlignEdge::Right); };
    alignTop.onClick    = [this] { model.alignSelection(CanvasModel::AlignEdge::Top); };
    alignCenterV.onClick = [this] { model.alignSelection(CanvasModel::AlignEdge::CenterV); };
    alignBottom.onClick = [this] { model.alignSelection(CanvasModel::AlignEdge::Bottom); };
    distH.onClick       = [this] { model.distributeSelectionH(); };
    distV.onClick       = [this] { model.distributeSelectionV(); };

    // Grid toggle
    gridToggle.setToggleState(model.grid.enabled, juce::dontSendNotification);
    gridToggle.onClick = [this]
    {
        model.grid.enabled = gridToggle.getToggleState();
        model.grid.showGrid = gridToggle.getToggleState();
        model.notifyItemsChanged();
    };
    addAndMakeVisible(gridToggle);

    // Grid size combo
    gridSizeCombo.addItem("1px", 1);
    gridSizeCombo.addItem("5px", 5);
    gridSizeCombo.addItem("10px", 10);
    gridSizeCombo.addItem("25px", 25);
    gridSizeCombo.addItem("50px", 50);
    gridSizeCombo.setSelectedId(model.grid.spacing, juce::dontSendNotification);
    gridSizeCombo.onChange = [this]
    {
        model.grid.spacing = gridSizeCombo.getSelectedId();
        model.notifyItemsChanged();
    };
    addAndMakeVisible(gridSizeCombo);

    // Zoom label
    zoomLabel.setFont(11.0f);
    addAndMakeVisible(zoomLabel);

    applyThemeColours();
}

void AlignmentToolbar::applyThemeColours()
{
    auto& pal = ThemeManager::getInstance().getPalette();

    for (auto* b : { &alignLeft, &alignCenterH, &alignRight,
                     &alignTop, &alignCenterV, &alignBottom, &distH, &distV })
    {
        b->setColour(juce::TextButton::buttonColourId, pal.toolboxItem);
        b->setColour(juce::TextButton::textColourOffId, pal.buttonText.withAlpha(0.8f));
    }

    gridToggle.setColour(juce::ToggleButton::textColourId, pal.bodyText.withAlpha(0.6f));
    gridSizeCombo.setColour(juce::ComboBox::backgroundColourId, pal.toolboxItem);
    gridSizeCombo.setColour(juce::ComboBox::textColourId, pal.buttonText.withAlpha(0.8f));
    gridSizeCombo.setColour(juce::ComboBox::outlineColourId, pal.border);
    zoomLabel.setColour(juce::Label::textColourId, pal.bodyText.withAlpha(0.6f));
}

void AlignmentToolbar::styleButton(juce::TextButton& b)
{
    addAndMakeVisible(b);
}

void AlignmentToolbar::paint(juce::Graphics& g)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    g.fillAll(pal.statusBarBg);
    zoomLabel.setText(juce::String(static_cast<int>(model.zoom * 100)) + "%",
                      juce::dontSendNotification);
}

void AlignmentToolbar::resized()
{
    auto area = getLocalBounds().reduced(4, 2);

    auto btn = [&](juce::Component& c, int w = 52)
    {
        c.setBounds(area.removeFromLeft(w));
        area.removeFromLeft(3);
    };

    btn(alignLeft);
    btn(alignCenterH, 62);
    btn(alignRight, 52);
    area.removeFromLeft(6);
    btn(alignTop, 46);
    btn(alignCenterV, 62);
    btn(alignBottom, 56);
    area.removeFromLeft(10);
    btn(distH, 52);
    btn(distV, 52);
    area.removeFromLeft(14);
    btn(gridToggle, 55);
    btn(gridSizeCombo, 65);

    // Zoom label at right
    zoomLabel.setBounds(area.removeFromRight(50));
}
