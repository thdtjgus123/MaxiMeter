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

    // Freeze (render preview) button with snowflake icon
    buildSnowflakeIcon();
    freezeButton.setTooltip("Render Preview (Freeze Frame)");
    freezeButton.onClick = [this] { if (onFreezeClicked) onFreezeClicked(); };
    addAndMakeVisible(freezeButton);

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

    // Rebuild snowflake icon with current theme colours
    buildSnowflakeIcon();
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

void AlignmentToolbar::buildSnowflakeIcon()
{
    auto colour = juce::Colours::white.withAlpha(0.85f);

    // Font Awesome 6 Free — snowflake (solid), viewBox 0 0 448 512
    juce::String svgText =
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 448 512'>"
        "<path fill='#ffffff' opacity='0.85' d='"
        "M224 0c17.7 0 32 14.3 32 32V62.1l15-15c9.4-9.4 24.6-9.4 33.9 "
        "0s9.4 24.6 0 33.9l-49 49v70.3l61.4-35.8 17.7-66.1c3.4-12.8 "
        "16.6-20.4 29.4-17s20.4 16.6 17 29.4l-5.2 19.3 23.6-13.8c15.3-8.9 "
        "34.9-3.7 43.8 11.5s3.8 34.9-11.5 43.8l-25.3 14.8 21.7 5.8c12.8 "
        "3.4 20.4 16.6 17 29.4s-16.6 20.4-29.4 17l-67.7-18.1L287.5 256l61.4 "
        "35.8 67.7-18.1c12.8-3.4 26 4.2 29.4 17s-4.2 26-17 29.4l-21.7 "
        "5.8 25.3 14.8c15.3 8.9 20.4 28.5 11.5 43.8s-28.5 20.4-43.8 "
        "11.5l-23.6-13.8 5.2 19.3c3.4 12.8-4.2 26-17 29.4s-26-4.2-29.4-17"
        "l-17.7-66.1L256 311.7v70.3l49 49c9.4 9.4 9.4 24.6 0 33.9s-24.6 "
        "9.4-33.9 0l-15-15V480c0 17.7-14.3 32-32 32s-32-14.3-32-32V449.9"
        "l-15 15c-9.4 9.4-24.6 9.4-33.9 0s-9.4-24.6 0-33.9l49-49V311.7"
        "l-61.4 35.8-17.7 66.1c-3.4 12.8-16.6 20.4-29.4 17s-20.4-16.6-17"
        "-29.4l5.2-19.3L48.1 395.6c-15.3 8.9-34.9 3.7-43.8-11.5s-3.7-34.9 "
        "11.5-43.8l25.3-14.8-21.7-5.8c-12.8-3.4-20.4-16.6-17-29.4s16.6-20.4 "
        "29.4-17l67.7 18.1L160.5 256 99.1 220.2 31.4 238.3c-12.8 3.4-26-4.2"
        "-29.4-17s4.2-26 17-29.4l21.7-5.8L15.4 171.3c-15.3-8.9-20.4-28.5"
        "-11.5-43.8s28.5-20.4 43.8-11.5l23.6 13.8-5.2-19.3c-3.4-12.8 "
        "4.2-26 17-29.4s26 4.2 29.4 17l17.7 66.1L192 200.3V130L143 81c-9.4"
        "-9.4-9.4-24.6 0-33.9s24.6-9.4 33.9 0l15 15V32c0-17.7 14.3-32 "
        "32-32z'/></svg>";

    auto xml = juce::XmlDocument::parse(svgText);
    if (xml)
        snowflakeIcon = juce::Drawable::createFromSVG(*xml);

    if (snowflakeIcon)
        freezeButton.setImages(snowflakeIcon.get());
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

    // Freeze button — rightmost before zoom label
    zoomLabel.setBounds(area.removeFromRight(50));
    area.removeFromRight(4);
    freezeButton.setBounds(area.removeFromRight(28));
}
