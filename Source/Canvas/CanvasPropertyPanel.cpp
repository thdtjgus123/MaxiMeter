#include "CanvasPropertyPanel.h"
#include "../UI/ThemeManager.h"
#include "../UI/ColourPickerWithEyedropper.h"
#include "../UI/MeterBase.h"

//==============================================================================
CanvasPropertyPanel::CanvasPropertyPanel(CanvasModel& m) : model(m)
{
    model.addListener(this);

    // ── Viewport setup ──
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);

    auto addCtrl = [this](juce::Component& c) { content.addAndMakeVisible(c); };

    titleLabel.setText("PROPERTIES", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    addCtrl(titleLabel);

    setupRow(nameLabel, "Name", nameEditor);
    setupRow(xLabel, "X", xEditor);
    setupRow(yLabel, "Y", yEditor);
    setupRow(wLabel, "W", wEditor);
    setupRow(hLabel, "H", hEditor);

    rotLabel.setText("Rot", juce::dontSendNotification);
    addCtrl(rotLabel);

    rotCombo.addItem("0",   1);
    rotCombo.addItem("90",  2);
    rotCombo.addItem("180", 3);
    rotCombo.addItem("270", 4);
    addCtrl(rotCombo);

    addCtrl(lockToggle);
    addCtrl(visibleToggle);
    addCtrl(aspectToggle);

    // Opacity slider
    opacityLabel.setText("Opacity", juce::dontSendNotification);
    addCtrl(opacityLabel);
    opacitySlider.setRange(0.0, 1.0, 0.01);
    opacitySlider.setValue(1.0, juce::dontSendNotification);
    opacitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    opacitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    addCtrl(opacitySlider);

    // Callbacks
    auto commitEdits = [this] { applyEdits(); };
    opacitySlider.onValueChange = commitEdits;

    // ── Canvas background colour ──
    bgColourLabel.setText("Background", juce::dontSendNotification);
    addCtrl(bgColourLabel);
    addCtrl(bgColourButton);
    bgColourButton.setColour(juce::TextButton::buttonColourId, model.background.colour1);
    bgColourButton.onClick = [this]()
    {
        launchColourPicker(bgColourButton, model.background.colour1,
            [this](juce::Colour c)
            {
                model.background.colour1 = c;
                model.background.colour2 = c.darker(0.4f);
                bgColourButton.setColour(juce::TextButton::buttonColourId, c);
                bgColourButton.repaint();
                model.notifyBackgroundChanged();
            });
    };

    // ── Grid colour ──
    gridColourLabel.setText("Grid Color", juce::dontSendNotification);
    addCtrl(gridColourLabel);
    addCtrl(gridColourButton);
    gridColourButton.setColour(juce::TextButton::buttonColourId, model.grid.gridColour);
    gridColourButton.onClick = [this]()
    {
        launchColourPicker(gridColourButton, model.grid.gridColour,
            [this](juce::Colour c)
            {
                model.grid.gridColour = c;
                gridColourButton.setColour(juce::TextButton::buttonColourId, c);
                gridColourButton.repaint();
                model.notifyItemsChanged();
            });
    };

    // ── Per-item background colour ──
    itemBgColourLabel.setText("Item BG", juce::dontSendNotification);
    addCtrl(itemBgColourLabel);
    addCtrl(itemBgColourButton);
    itemBgColourButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
    itemBgColourButton.onClick = [this]()
    {
        auto sel = model.getSelectedItems();
        if (sel.empty()) return;
        launchColourPicker(itemBgColourButton, sel.front()->itemBackground,
            [this](juce::Colour c)
            {
                for (auto* item : model.getSelectedItems())
                    item->itemBackground = c;
                itemBgColourButton.setColour(juce::TextButton::buttonColourId, c);
                itemBgColourButton.repaint();
                model.notifyItemsChanged();
            });
    };

    // ── Meter background colour ──
    meterBgLabel.setText("Meter BG", juce::dontSendNotification);
    addCtrl(meterBgLabel);
    addCtrl(meterBgButton);
    meterBgButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
    meterBgButton.onClick = [this]()
    {
        auto sel = model.getSelectedItems();
        if (sel.empty()) return;
        launchColourPicker(meterBgButton, sel.front()->meterBgColour,
            [this](juce::Colour c)
            {
                for (auto* item : model.getSelectedItems())
                    item->meterBgColour = c;
                meterBgButton.setColour(juce::TextButton::buttonColourId, c);
                meterBgButton.repaint();
                model.notifyItemsChanged();
            });
    };

    // ── Meter foreground colour ──
    meterFgLabel.setText("Meter FG", juce::dontSendNotification);
    addCtrl(meterFgLabel);
    addCtrl(meterFgButton);
    meterFgButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
    meterFgButton.onClick = [this]()
    {
        auto sel = model.getSelectedItems();
        if (sel.empty()) return;
        launchColourPicker(meterFgButton, sel.front()->meterFgColour,
            [this](juce::Colour c)
            {
                for (auto* item : model.getSelectedItems())
                    item->meterFgColour = c;
                meterFgButton.setColour(juce::TextButton::buttonColourId, c);
                meterFgButton.repaint();
                model.notifyItemsChanged();
            });
    };

    // ── Blend mode ──
    blendLabel.setText("Blend", juce::dontSendNotification);
    addCtrl(blendLabel);
    for (int i = 0; i < blendModeCount(); ++i)
        blendCombo.addItem(blendModeName(static_cast<BlendMode>(i)), i + 1);
    addCtrl(blendCombo);
    blendCombo.onChange = commitEdits;

    // ── Editor callbacks ──
    nameEditor.onReturnKey = commitEdits;
    nameEditor.onFocusLost = commitEdits;
    xEditor.onReturnKey = commitEdits;
    xEditor.onFocusLost = commitEdits;
    yEditor.onReturnKey = commitEdits;
    yEditor.onFocusLost = commitEdits;
    wEditor.onReturnKey = commitEdits;
    wEditor.onFocusLost = commitEdits;
    hEditor.onReturnKey = commitEdits;
    hEditor.onFocusLost = commitEdits;
    rotCombo.onChange = commitEdits;
    lockToggle.onClick   = commitEdits;
    visibleToggle.onClick = commitEdits;
    aspectToggle.onClick  = commitEdits;

    // Initial theme
    applyThemeColours();
}

CanvasPropertyPanel::~CanvasPropertyPanel()
{
    model.removeListener(this);
}

//==============================================================================
void CanvasPropertyPanel::launchColourPicker(
    juce::TextButton& btn,
    juce::Colour initial,
    std::function<void(juce::Colour)> onChange)
{
    auto picker = std::make_unique<ColourPickerWithEyedropper>();
    picker->setCurrentColour(initial);
    picker->setSize(300, 430);
    picker->onColourChanged = [onChange](juce::Colour c) { if (onChange) onChange(c); };
    juce::CallOutBox::launchAsynchronously(
        std::move(picker),
        btn.getScreenBounds(),
        nullptr);
}

//==============================================================================
void CanvasPropertyPanel::applyThemeColours()
{
    auto& pal = ThemeManager::getInstance().getPalette();

    titleLabel.setColour(juce::Label::textColourId, pal.headerText);
    nameLabel.setColour(juce::Label::textColourId, pal.dimText);
    xLabel.setColour(juce::Label::textColourId, pal.dimText);
    yLabel.setColour(juce::Label::textColourId, pal.dimText);
    wLabel.setColour(juce::Label::textColourId, pal.dimText);
    hLabel.setColour(juce::Label::textColourId, pal.dimText);
    rotLabel.setColour(juce::Label::textColourId, pal.dimText);

    auto styleTextEditor = [&](juce::TextEditor& te) {
        te.setColour(juce::TextEditor::backgroundColourId, pal.toolboxItem);
        te.setColour(juce::TextEditor::textColourId, pal.buttonText);
        te.setColour(juce::TextEditor::outlineColourId, pal.border);
    };
    styleTextEditor(nameEditor);
    styleTextEditor(xEditor);
    styleTextEditor(yEditor);
    styleTextEditor(wEditor);
    styleTextEditor(hEditor);

    auto styleCombo = [&](juce::ComboBox& cb) {
        cb.setColour(juce::ComboBox::backgroundColourId, pal.toolboxItem);
        cb.setColour(juce::ComboBox::textColourId, pal.buttonText);
        cb.setColour(juce::ComboBox::outlineColourId, pal.border);
    };
    styleCombo(rotCombo);
    styleCombo(blendCombo);

    lockToggle.setColour(juce::ToggleButton::textColourId, pal.bodyText.withAlpha(0.7f));
    visibleToggle.setColour(juce::ToggleButton::textColourId, pal.bodyText.withAlpha(0.7f));
    aspectToggle.setColour(juce::ToggleButton::textColourId, pal.bodyText.withAlpha(0.7f));

    opacityLabel.setColour(juce::Label::textColourId, pal.dimText);
    opacitySlider.setColour(juce::Slider::backgroundColourId, pal.toolboxItem);
    opacitySlider.setColour(juce::Slider::trackColourId, pal.accent);
    opacitySlider.setColour(juce::Slider::textBoxTextColourId, pal.buttonText);
    opacitySlider.setColour(juce::Slider::textBoxBackgroundColourId, pal.toolboxItem);
    opacitySlider.setColour(juce::Slider::textBoxOutlineColourId, pal.border);

    bgColourLabel.setColour(juce::Label::textColourId, pal.dimText);
    gridColourLabel.setColour(juce::Label::textColourId, pal.dimText);
    itemBgColourLabel.setColour(juce::Label::textColourId, pal.dimText);
    meterBgLabel.setColour(juce::Label::textColourId, pal.dimText);
    meterFgLabel.setColour(juce::Label::textColourId, pal.dimText);
    blendLabel.setColour(juce::Label::textColourId, pal.dimText);
}

void CanvasPropertyPanel::setupRow(juce::Label& lbl, const juce::String& text, juce::Component& editor)
{
    lbl.setText(text, juce::dontSendNotification);
    content.addAndMakeVisible(lbl);
    content.addAndMakeVisible(editor);
}

//==============================================================================
void CanvasPropertyPanel::paint(juce::Graphics& g)
{
    g.fillAll(ThemeManager::getInstance().getPalette().toolboxBg);
}

void CanvasPropertyPanel::resized()
{
    viewport.setBounds(getLocalBounds());
    layoutContent();
    // Re-layout after scrollbar appeared (width may have changed)
    layoutContent();
}

void CanvasPropertyPanel::layoutContent()
{
    const int panelW = juce::jmax(50, viewport.getMaximumVisibleWidth());
    auto area = juce::Rectangle<int>(0, 0, panelW, 2000).reduced(6);

    titleLabel.setBounds(area.removeFromTop(22));

    auto row = [&](juce::Label& lbl, juce::Component& editor, int h = 22)
    {
        auto r = area.removeFromTop(h);
        lbl.setBounds(r.removeFromLeft(30));
        editor.setBounds(r.reduced(2, 1));
        area.removeFromTop(2);
    };

    row(nameLabel, nameEditor, 24);
    row(xLabel, xEditor);
    row(yLabel, yEditor);
    row(wLabel, wEditor);
    row(hLabel, hEditor);
    row(rotLabel, rotCombo, 24);

    lockToggle.setBounds(area.removeFromTop(22));
    area.removeFromTop(2);
    visibleToggle.setBounds(area.removeFromTop(22));
    area.removeFromTop(2);
    aspectToggle.setBounds(area.removeFromTop(22));

    // Opacity
    area.removeFromTop(4);
    auto opRow = area.removeFromTop(22);
    opacityLabel.setBounds(opRow.removeFromLeft(50));
    opacitySlider.setBounds(opRow.reduced(2, 0));

    auto colourRow = [&](juce::Label& lbl, juce::TextButton& btn)
    {
        area.removeFromTop(4);
        auto r = area.removeFromTop(24);
        lbl.setBounds(r.removeFromLeft(70));
        btn.setBounds(r.reduced(2, 2));
    };

    colourRow(bgColourLabel, bgColourButton);
    colourRow(gridColourLabel, gridColourButton);
    colourRow(itemBgColourLabel, itemBgColourButton);
    colourRow(meterBgLabel, meterBgButton);
    colourRow(meterFgLabel, meterFgButton);

    // Blend mode
    area.removeFromTop(4);
    auto blendRow = area.removeFromTop(24);
    blendLabel.setBounds(blendRow.removeFromLeft(70));
    blendCombo.setBounds(blendRow.reduced(2, 1));

    // Set content height to actual usage
    content.setSize(panelW, area.getY() + 12);
}

//==============================================================================
void CanvasPropertyPanel::refresh()
{
    auto sel = model.getSelectedItems();
    if (sel.empty())
    {
        nameEditor.setText("", false);
        xEditor.setText("", false);
        yEditor.setText("", false);
        wEditor.setText("", false);
        hEditor.setText("", false);
        rotCombo.setSelectedId(0, juce::dontSendNotification);
        lockToggle.setToggleState(false, juce::dontSendNotification);
        visibleToggle.setToggleState(true, juce::dontSendNotification);
        aspectToggle.setToggleState(false, juce::dontSendNotification);
        opacitySlider.setValue(1.0, juce::dontSendNotification);
        blendCombo.setSelectedId(1, juce::dontSendNotification);
        setEnabled(false);
        return;
    }

    setEnabled(true);
    auto* item = sel.front();
    nameEditor.setText(item->name, false);
    xEditor.setText(juce::String(item->x, 1), false);
    yEditor.setText(juce::String(item->y, 1), false);
    wEditor.setText(juce::String(item->width, 1), false);
    hEditor.setText(juce::String(item->height, 1), false);

    int rotIdx = (item->rotation / 90) + 1;
    rotCombo.setSelectedId(juce::jlimit(1, 4, rotIdx), juce::dontSendNotification);
    lockToggle.setToggleState(item->locked, juce::dontSendNotification);
    visibleToggle.setToggleState(item->visible, juce::dontSendNotification);
    aspectToggle.setToggleState(item->aspectLock, juce::dontSendNotification);
    opacitySlider.setValue(item->opacity, juce::dontSendNotification);
    itemBgColourButton.setColour(juce::TextButton::buttonColourId, item->itemBackground);
    meterBgButton.setColour(juce::TextButton::buttonColourId, item->meterBgColour);
    meterFgButton.setColour(juce::TextButton::buttonColourId, item->meterFgColour);
    blendCombo.setSelectedId(static_cast<int>(item->blendMode) + 1, juce::dontSendNotification);
}

void CanvasPropertyPanel::applyEdits()
{
    auto sel = model.getSelectedItems();
    if (sel.empty()) return;

    for (auto* item : sel)
    {
        item->name = nameEditor.getText();
        item->x = xEditor.getText().getFloatValue();
        item->y = yEditor.getText().getFloatValue();
        item->width = std::max(10.0f, wEditor.getText().getFloatValue());
        item->height = std::max(10.0f, hEditor.getText().getFloatValue());
        item->rotation = (rotCombo.getSelectedId() - 1) * 90;
        item->locked = lockToggle.getToggleState();
        item->visible = visibleToggle.getToggleState();
        item->aspectLock = aspectToggle.getToggleState();
        item->opacity = static_cast<float>(opacitySlider.getValue());
        item->blendMode = static_cast<BlendMode>(blendCombo.getSelectedId() - 1);
    }
    model.notifyItemsChanged();
}
