#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"

//==============================================================================
/// Small minimap overview of the entire canvas, shown in a corner.
class MiniMap : public juce::Component,
                public CanvasModelListener
{
public:
    explicit MiniMap(CanvasModel& model);
    ~MiniMap() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    void itemsChanged() override   { repaint(); }
    void zoomPanChanged() override { repaint(); }

private:
    CanvasModel& model;

    /// Compute bounding box encompassing all items.
    juce::Rectangle<float> getContentBounds() const;

    /// Map a screen point on the minimap to a canvas centre.
    void navigateTo(juce::Point<float> minimapPt);
};
