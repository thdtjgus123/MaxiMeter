#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"

//==============================================================================
/// Layer panel showing all items sorted by z-order, with visibility/lock toggles.
class LayerPanel : public juce::Component,
                   public CanvasModelListener
{
public:
    explicit LayerPanel(CanvasModel& model);
    ~LayerPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    void itemsChanged() override    { rebuild(); }
    void selectionChanged() override { repaint(); }
    void applyThemeColours();

    /// Callback fired when the user drags the top-edge divider.
    /// Parameter: delta Y (negative = panel grows, positive = shrinks)
    std::function<void(int deltaY)> onDividerDragged;

private:
    CanvasModel& model;

    //==========================================================================
    class LayerRow : public juce::Component
    {
    public:
        LayerRow(CanvasModel& m, juce::Uuid itemId);
        void paint(juce::Graphics& g) override;
        void mouseUp(const juce::MouseEvent& e) override;

    private:
        CanvasModel& model;
        juce::Uuid id;
        juce::TextButton visBtn  { "V" };
        juce::TextButton lockBtn { "L" };

        void setupButtons();
    };

    std::vector<std::unique_ptr<LayerRow>> rows;
    juce::Label titleLabel;
    juce::TextButton moveUpBtn   { "Up" };
    juce::TextButton moveDownBtn { "Dn" };

    /// Viewport + inner container for scrollable row list.
    juce::Viewport viewport;
    juce::Component rowContainer;

    void rebuild();
    void layoutRows();          ///< Position rows inside rowContainer

    static constexpr int kDividerHeight = 5;  ///< height of the drag zone at top
    int  dividerDragStartY_ = 0;
    bool draggingDivider_   = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerPanel)
};
