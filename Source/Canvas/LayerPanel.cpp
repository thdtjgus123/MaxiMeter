#include "LayerPanel.h"
#include "CanvasCommands.h"
#include "../UI/ThemeManager.h"
// Icons not used here — layer buttons use text labels
#include <algorithm>

//==============================================================================
LayerPanel::LayerPanel(CanvasModel& m) : model(m)
{
    model.addListener(this);

    titleLabel.setText("LAYERS", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    addAndMakeVisible(moveUpBtn);
    addAndMakeVisible(moveDownBtn);

    viewport.setViewedComponent(&rowContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    applyThemeColours();

    moveUpBtn.onClick = [this]
    {
        auto sel = model.getSelectedItems();
        if (sel.empty()) return;
        auto* s = sel.front();

        // Find neighbour with the smallest z-order that is still > s->zOrder
        CanvasItem* neighbor = nullptr;
        for (int i = 0; i < model.getNumItems(); ++i)
        {
            auto* item = model.getItem(i);
            if (item->id != s->id && item->zOrder > s->zOrder)
                if (!neighbor || item->zOrder < neighbor->zOrder)
                    neighbor = item;
        }
        if (!neighbor) return; // already at top

        model.undoManager.beginNewTransaction("Move Layer Up");
        model.undoManager.perform(
            new SwapZOrderAction(model, s->id, s->zOrder, neighbor->id, neighbor->zOrder),
            "Swap Z");
    };
    moveDownBtn.onClick = [this]
    {
        auto sel = model.getSelectedItems();
        if (sel.empty()) return;
        auto* s = sel.front();

        // Find neighbour with the largest z-order that is still < s->zOrder
        CanvasItem* neighbor = nullptr;
        for (int i = 0; i < model.getNumItems(); ++i)
        {
            auto* item = model.getItem(i);
            if (item->id != s->id && item->zOrder < s->zOrder)
                if (!neighbor || item->zOrder > neighbor->zOrder)
                    neighbor = item;
        }
        if (!neighbor) return; // already at bottom

        model.undoManager.beginNewTransaction("Move Layer Down");
        model.undoManager.perform(
            new SwapZOrderAction(model, s->id, s->zOrder, neighbor->id, neighbor->zOrder),
            "Swap Z");
    };
}

LayerPanel::~LayerPanel()
{
    model.removeListener(this);
}

void LayerPanel::paint(juce::Graphics& g)
{
    g.fillAll(ThemeManager::getInstance().getPalette().toolboxBg);

    // Draw divider handle at top
    auto& pal = ThemeManager::getInstance().getPalette();
    g.setColour(pal.border);
    g.fillRect(0, 0, getWidth(), kDividerHeight);
    // Small grab indicator
    g.setColour(pal.dimText.withAlpha(0.4f));
    int cx = getWidth() / 2;
    g.fillRect(cx - 15, 1, 30, 2);
}

void LayerPanel::applyThemeColours()
{
    auto& pal = ThemeManager::getInstance().getPalette();
    titleLabel.setColour(juce::Label::textColourId, pal.headerText);
    moveUpBtn.setColour(juce::TextButton::buttonColourId, pal.toolboxItem);
    moveUpBtn.setColour(juce::TextButton::textColourOffId, pal.buttonText.withAlpha(0.8f));
    moveDownBtn.setColour(juce::TextButton::buttonColourId, pal.toolboxItem);
    moveDownBtn.setColour(juce::TextButton::textColourOffId, pal.buttonText.withAlpha(0.8f));
}

void LayerPanel::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(kDividerHeight);   // skip divider zone
    auto topBar = area.removeFromTop(24);
    titleLabel.setBounds(topBar.removeFromLeft(60));
    moveDownBtn.setBounds(topBar.removeFromRight(30));
    topBar.removeFromRight(2);
    moveUpBtn.setBounds(topBar.removeFromRight(30));

    area.removeFromTop(4);
    viewport.setBounds(area);
    layoutRows();
}

void LayerPanel::layoutRows()
{
    int rowH = 26;
    int gap  = 2;
    int totalH = static_cast<int>(rows.size()) * (rowH + gap);
    int w = viewport.getMaximumVisibleWidth();
    rowContainer.setSize(w, juce::jmax(totalH, viewport.getHeight()));

    int y = 0;
    for (auto& row : rows)
    {
        row->setBounds(0, y, w, rowH);
        y += rowH + gap;
    }
}

void LayerPanel::rebuild()
{
    rows.clear();

    // Build rows from items in reverse z-order (top layer first)
    std::vector<CanvasItem*> sorted;
    for (int i = 0; i < model.getNumItems(); ++i)
        sorted.push_back(model.getItem(i));
    std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) { return a->zOrder > b->zOrder; });

    for (auto* item : sorted)
    {
        auto row = std::make_unique<LayerRow>(model, item->id);
        rowContainer.addAndMakeVisible(row.get());
        rows.push_back(std::move(row));
    }

    layoutRows();
    repaint();
}

//==============================================================================
void LayerPanel::mouseMove(const juce::MouseEvent& e)
{
    if (e.y < kDividerHeight)
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void LayerPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y < kDividerHeight)
    {
        draggingDivider_ = true;
        dividerDragStartY_ = e.getScreenY();
    }
}

void LayerPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingDivider_ && onDividerDragged)
    {
        int delta = e.getScreenY() - dividerDragStartY_;
        dividerDragStartY_ = e.getScreenY();
        onDividerDragged(delta);
    }
}

void LayerPanel::mouseUp(const juce::MouseEvent&)
{
    draggingDivider_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}
//==============================================================================
// LayerRow
//==============================================================================
LayerPanel::LayerRow::LayerRow(CanvasModel& m, juce::Uuid itemId)
    : model(m), id(itemId)
{
    setupButtons();
}

void LayerPanel::LayerRow::setupButtons()
{
    addAndMakeVisible(visBtn);
    addAndMakeVisible(lockBtn);

    auto& pal = ThemeManager::getInstance().getPalette();
    visBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    visBtn.setColour(juce::TextButton::textColourOffId, pal.bodyText.withAlpha(0.5f));
    lockBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    lockBtn.setColour(juce::TextButton::textColourOffId, pal.bodyText.withAlpha(0.5f));

    visBtn.onClick = [this]
    {
        if (auto* item = model.findItem(id))
        {
            item->visible = !item->visible;
            model.notifyItemsChanged();
        }
    };
    lockBtn.onClick = [this]
    {
        if (auto* item = model.findItem(id))
        {
            item->locked = !item->locked;
            model.notifyItemsChanged();
        }
    };
}

void LayerPanel::LayerRow::paint(juce::Graphics& g)
{
    auto* item = model.findItem(id);
    if (!item) return;

    auto& pal = ThemeManager::getInstance().getPalette();
    bool selected = model.isSelected(id);
    g.setColour(selected ? pal.toolboxItemHover : pal.toolboxItem);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);

    g.setColour(pal.bodyText.withAlpha(item->visible ? 0.8f : 0.3f));
    g.setFont(11.0f);
    juce::String label = item->name.isEmpty() ? meterTypeName(item->meterType) : item->name;
    g.drawText(label, 4, 0, getWidth() - 60, getHeight(), juce::Justification::centredLeft);

    // Position buttons
    auto area = getLocalBounds();
    lockBtn.setBounds(area.removeFromRight(22).reduced(1));
    visBtn.setBounds(area.removeFromRight(22).reduced(1));

    // Update button text
    visBtn.setButtonText(item->visible ? "V" : "-");
    lockBtn.setButtonText(item->locked ? "L" : "-");
}

void LayerPanel::LayerRow::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
        model.selectItem(id, e.mods.isShiftDown());
}
